//
// QSINIT
// screen support dll: EFI output
//

#include "conint.h"
#include "efnlist.h"
#include "qecall.h"
#include "stdlib.h"
#include "qslog.h"
#include "vio.h"

static struct console_mode *cm_list = 0;
static struct graphic_mode *gm_list = 0;

static void out_close(void) {
   cm_list   = 0;
   gm_list   = 0;
   vmem_addr = 0;
   vmem_size = 0;
}

static void _std out_init(void) {
   u32t      ii;
   s32t  nm_con = call64(EFN_VIOENUMMODES, 0, 1, &cm_list),
         nm_grf = call64(EFN_GRFENUMMODES, 0, 3, &gm_list, &vmem_addr, &vmem_size);
   // use forced values
   if (fbaddr_forced) {
      vmem_addr = fbaddr_forced;
      if (!vmem_size) vmem_size = fbaddr_size;
   }
   if (nm_con<0) nm_con = 0; else
      if (nm_con>MAX_MODES) nm_con = MAX_MODES;
   if (nm_grf<0) nm_grf = 0;
   // common text modes
   for (ii=0; ii<nm_con; ii++) {
      // module owned memory block (will be auto-released during unload)
      modes[ii] = malloc(sizeof(modeinfo));
      memset(modes[ii], 0, sizeof(modeinfo));

      modes[ii]->width  = cm_list[ii].conmode_x;
      modes[ii]->height = cm_list[ii].conmode_y;
      // decorative values for native modes
      modes[ii]->font_x = 8;
      modes[ii]->font_y = modes[ii]->height==50?8:16;
   }
   mode_cnt = nm_con;

   if (!textonly && mode_cnt<MAX_MODES)
      for (ii = 0; ii<nm_grf; ii++) {
         static u32t     fbits[] = {1, 4, 8, 15, 16, 24, 32, 0};
         modeinfo            *mi = (modeinfo*)malloc(sizeof(modeinfo));
         struct graphic_mode *gm = gm_list + ii;
         u32t                *fp;

         memset(mi, 0, sizeof(modeinfo));
         modes[mode_cnt] = mi;

         mi->modenum  = ii;
         mi->realmode = -1;
         mi->width    = gm->grfmode_x;
         mi->height   = gm->grfmode_y;
         mi->flags    = CON_GRAPHMODE;
         mi->physaddr = vmem_addr;
         mi->mempitch = gm->grfmode_pitch;

         /* if no direct memory access - force 32-bit color format, because
            EFI blit wants it, else try to report/use bit mask */
         if (!fbaddr_enabled || !vmem_addr) {
            mi->bits  = 32;
            mi->rmask = 0xFF0000;  
            mi->gmask = 0xFF00;    
            mi->bmask = 0xFF;      
            mi->amask = 0xFF000000;
         } else {
            mi->bits     = gm->grfmode_bpp;
            if (mi->bits>8) {
               mi->rmask   = gm->grfmode_mred;
               mi->gmask   = gm->grfmode_mgreen;
               mi->bmask   = gm->grfmode_mblue;
               mi->amask   = gm->grfmode_mres;
            }
         }
         // CON_COLOR_XXXX flags
         fp = memchrd(fbits, mi->bits, 8);
         if (fp) mi->flags|= (fp-fbits)<<8;

         if (++mode_cnt>=MAX_MODES) break;
      }
}

static void out_addfonts(void) {}

static u32t _std out_setmodeid(u32t mode_id) {
   if (mode_id>=mode_cnt) return 0;
   // native text mode?
   if ((modes[mode_id]->flags&(CON_EMULATED|CON_GRAPHMODE))==0) {
      int rc = call64(EFN_VIOSETMODEEX, 0, 2, modes[mode_id]->width, modes[mode_id]->height);
      if (rc) {
         con_unsetmode();
         current_mode = mode_id;
         real_mode    = mode_id;
         mode_changed = 1;
         in_native    = 1;
         return 1;
      }
   } else {
      int     idx = mode_id,
           actual = idx;
      // force "shadow buffer" flag for any graphic function in EFI
      u32t  flags = modes[idx]->flags|CON_SHADOW;

      // is this an emulated text mode? select real graphic mode for it
      if (idx>=0 && (flags&CON_EMULATED)!=0) {
         actual = modes[idx]->realmode;
         // internal error?
         if (actual<0) idx=-1;
      }

      if (idx>=0) {
         modeinfo *m_pub = modes[idx],    // public mode info
                  *m_act = modes[actual]; // actual mode info
         struct graphic_mode *gm = gm_list + m_act->modenum;
         /* set mode, but only if we it does not match to current real mode */
         if (real_mode==actual)
            pl_clear(real_mode, 0, 0, m_act->width, m_act->height, 0);
         else
            if ((int)call64(EFN_GRFSETMODE, 0, 1, m_act->modenum)<=0) return 0;
         // free previous mode data
         con_unsetmode();
         /* shadow buffer is required on EFI,
            call should be below con_unsetmode(), because may release the
            same pointer */
         alloc_shadow(m_act, flags&CON_NOSCREENCLEAR);

         log_it(3,"%d %d %d\n", gm->grfmode_pitch, gm->grfmode_x, gm->grfmode_y);
         // direct memory address
         m_pub->physmap = 0;
         m_act->physmap = 0;
         // trying to use EFI reported memory addr
         if (fbaddr_enabled && (m_act->flags&CON_GRAPHMODE)!=0) 
            m_act->physmap = (u8t*)vmem_addr;

         current_mode   = idx;
         real_mode      = actual;
         current_flags  = flags;
         mode_changed   = 1;
         in_native      = 0;
         // setup text emulation
         if ((m_pub->flags&CON_EMULATED)!=0) {
            alloc_shadow(m_pub,0);
            evio_newmode();
         }
         return 1;
      }
   }
   return 0;
}

static u32t _std out_setmode(u32t x, u32t y, u32t flags) {
   int idx = search_mode(x,y,flags);
   if (idx<0) return 0;
   return out_setmodeid(idx);
}

static void _std out_leavemode(void) {
   modeinfo *mi = 0;
   do {
      mi = modes[mi?current_mode:real_mode];
      mi->physmap = 0;
   } while (mi!=modes[current_mode]);
}

static u32t _std out_dirclear(u32t x, u32t y, u32t dx, u32t dy, u32t color) {
   return call64(EFN_GRFCLEAR, 0, 5, x, y, dx, dy, color)>0 ? 1: 0;
}

static u32t _std out_dirblit(u32t x, u32t y, u32t dx, u32t dy, void *src, u32t srcpitch) {
   return call64(EFN_GRFBLIT, 0, 6, x, y, dx, dy, src, srcpitch)>0 ? 1: 0;
}

static u32t _std out_copy(u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch, int write) {
   modeinfo *mi = modes[mode];
   if (mode>=mode_cnt) return 0;
   // copying memory
   if ((mi->flags&CON_GRAPHMODE)==0) {
      // normal or emulated text mode? { i.e. EFI emulation or own emulation? ;) }
      if ((mi->flags&CON_EMULATED)==0) {
         if (write) {
            vio_writebuf(x,y,dx,dy,buf,pitch);
            // update shadow buffer
            if (mi->shadow) MoveBlock(buf, mi->shadow + y*mi->shadowpitch + x*2,
               dx<<1, dy, pitch, mi->shadowpitch);
         } else 
            vio_readbuf(x,y,dx,dy,buf,pitch);
      } else {
#if 0
         if (write) evio_writebuf(x,y,dx,dy,buf,pitch);
            else evio_readbuf(x,y,dx,dy,buf,pitch);
#endif
      }
      return 1;
   } else {
      u32t  width = dx*mi->bits>>3,
             xpos = x*mi->bits>>3,
               rc = common_copy(mode, x, y, dx, dy, buf, pitch, write);

      if (write && !mi->physmap) {
         u32t   vbpps = BytesBP(mi->bits);
         u8t *membase = mi->shadow + mi->shadowpitch * y + x*vbpps;
         // copy to screen without FB pointer
         rc = out_dirblit(x,y,dx,dy,membase,mi->shadowpitch);
      }
      return rc;
   }
   return 0;
}

int plinit_efi(void) {
   pl_setmode   = out_setmode;
   pl_setmodeid = out_setmodeid;
   pl_leavemode = out_leavemode;
   pl_copy      = out_copy;
   pl_flush     = common_flush_nofb;
   pl_scroll    = common_scroll_nofb;
   pl_clear     = common_clear_nofb;
   pl_addfont   = out_addfonts;
   pl_setup     = out_init;
   pl_close     = out_close;
   pl_dirclear  = out_dirclear;
   pl_dirblit   = out_dirblit;
   return 1;
}
