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

static 
struct console_mode *cm_list = 0;
static long        con_modes = 0;
static 
struct graphic_mode *gm_list = 0;
static long        grf_modes = 0;
static u32t         vmem_ptr = 0,
                   vmem_size = 0;

static void out_close(void) {
   cm_list = 0;  con_modes = 0;
   gm_list = 0;  grf_modes = 0;
   vmem_ptr  = 0;
   vmem_size = 0;
}

static void _std out_init(void) {
   u32t  ii;
   con_modes = call64(EFN_VIOENUMMODES, 0, 1, &cm_list);
   grf_modes = call64(EFN_GRFENUMMODES, 0, 3, &gm_list, &vmem_ptr, &vmem_size);

   if (con_modes<0) con_modes = 0; else
      if (con_modes>MAXVESA_MODES+PREDEFINED_MODES)
         con_modes = MAXVESA_MODES+PREDEFINED_MODES;
   if (grf_modes<0) grf_modes = 0;
   mode_limit  = con_modes;
   if (!textonly) mode_limit += grf_modes;
   // total number of available modes
   mode_limit += MAXEMU_MODES + 1;
   modes       = (con_modeinfo*)malloc(sizeof(con_modeinfo)*mode_limit);
   memZero(modes);
   // common text modes
   for (ii=0; ii<con_modes; ii++) {
      modes[ii].width  = cm_list[ii].conmode_x;
      modes[ii].height = cm_list[ii].conmode_y;
      // decorative values for native modes
      modes[ii].font_x = 8;
      modes[ii].font_y = modes[ii].height==50?8:16;
   }
   mode_cnt = con_modes;

   if (!textonly && mode_cnt<MAXVESA_MODES+PREDEFINED_MODES)
      for (ii = 0; ii<grf_modes; ii++) {
         static u32t     fbits[] = {1, 4, 8, 15, 16, 24, 32, 0};
         con_modeinfo        *mi = modes + mode_cnt;
         struct graphic_mode *gm = gm_list + ii;
         u32t                *fp;

         mref[mode_cnt].vesaref  = ii;
         mref[mode_cnt].realmode = -1;

         mi->width    = gm->grfmode_x;
         mi->height   = gm->grfmode_y;
         mi->flags    = CON_GRAPHMODE;
         mi->memsize  = vmem_size;
         mi->physaddr = vmem_ptr;
         mi->mempitch = gm->grfmode_pitch;

         /* if no direct memory access - force 32-bit color format, because
            EFI blit wants it, else try to report/use bit mask */
         if (!fbaddr_enabled || !vmem_ptr) {
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

         if (++mode_cnt>=MAXVESA_MODES+PREDEFINED_MODES) break;
      }
}

static void out_addfonts(void) {
}

static u32t _std out_setmode(u32t x, u32t y, u32t flags) {
   int idx = search_mode(x,y,flags);
   if (idx<0) return 0;

   if ((flags&(CON_EMULATED|CON_GRAPHMODE))==0) {
      int rc = call64(EFN_VIOSETMODEEX, 0, 2, x, y);
      if (rc) {
         con_unsetmode();
         current_mode = idx;
         real_mode    = idx;
         mode_changed = 1;
         return 1;
      }
   } else {
      int  actual = idx;
      // force "shadow buffer" flag for any graphic function in EFI
      flags |= CON_SHADOW;
      // is this an emulated text mode? select real graphic mode for it
      if (idx>=0 && (modes[idx].flags&CON_EMULATED)!=0) {
         actual = mref[idx].realmode;
         // internal error?
         if (actual<0) idx=-1;
      }

      if (idx>=0) {
         con_modeinfo *m_pub = modes + idx,    // public mode info
                      *m_act = modes + actual; // actual mode info
         struct graphic_mode *gm = gm_list + mref[actual].vesaref;
         // shadow buffer is required on EFI
         if (!alloc_shadow(m_act,flags&CON_NOSCREENCLEAR)) return 0;
         // set mode
         if ((int)call64(EFN_GRFSETMODE, 0, 1, mref[actual].vesaref)<=0) return 0;
         // free previous mode data
         con_unsetmode();
         // 
         log_it(3,"%d %d %d\n", gm->grfmode_pitch, gm->grfmode_x, gm->grfmode_y);
         // direct memory address
         m_pub->physmap = 0;
         m_act->physmap = 0;
         // trying to use EFI reported memory addr
         if (fbaddr_enabled && (m_act->flags&CON_GRAPHMODE)!=0) 
            m_act->physmap = (u8t*)vmem_ptr;

         current_mode   = idx;
         real_mode      = actual;
         current_flags  = flags;
         mode_changed   = 1;
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

static void _std out_leavemode(void) {
   con_modeinfo *mi = 0;
   do {
      mi = modes + (mi?current_mode:real_mode);
      mi->physmap = 0;
   } while (mi!=modes+current_mode);
}

static u32t _std out_copy(u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch, int write) {
   con_modeinfo *mi = modes+mode;
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
         if (write) evio_writebuf(x,y,dx,dy,buf,pitch);
            else evio_readbuf(x,y,dx,dy,buf,pitch);
      }
      return 1;
   } else {
      u32t  width = dx*mi->bits>>3,
             xpos = x*mi->bits>>3,
               rc = common_copy(mode, x, y, dx, dy, buf, pitch, write);

      if (write && !mi->physmap) {
         u32t   vbpps = BytesBP(mi->bits);
         u8t *membase = mi->shadow + mi->shadowpitch * y + x*vbpps;
         // copy to screen
         rc = call64(EFN_GRFBLIT,0,6,x,y,dx,dy,membase,mi->shadowpitch)>0? 1 : 0;
      }
      return rc;
   }
   return 0;
}

static u32t out_flush(u32t mode, u32t x, u32t y, u32t dx, u32t dy) {
   con_modeinfo  *mi = modes+mode;
   if (mode>=mode_cnt) return 0;
   if (!mi->shadow) return 0;
   if (x>=mi->width || y>=mi->height) return 0;
   if (x+dx>mi->width)  dx=mi->width -x;
   if (y+dy>mi->height) dy=mi->height-y;
   if (!dx||!dy) return 0;
   /* common_flush() returns 1 only if both shadow & frame exists, so we
      finished here, else - trying to blit shadow buffer by EFI call */
   if (common_flush(mode, x, y, dx, dy)) return 1; else {
      u32t  vbpps = BytesBP(mi->bits);
      // copy to screen
      if (call64(EFN_GRFBLIT, 0, 6, x, y, dx, dy, mi->shadow +
         y*mi->shadowpitch + x*vbpps, mi->shadowpitch)>0) return 1;
   }
   return 0;
}

static u32t out_scroll(u32t mode, u32t ys, u32t yd, u32t lines) {
   u32t  rc = 0;
   if (common_scroll(mode, ys, yd, lines)) {
      con_modeinfo *mi = modes+mode;

      if (mi->physmap) rc = 1; else 
         out_flush(mode, 0, yd, mi->width, lines);
   }
   return rc;
}

/// fill number of screen lines by color value
static u32t out_clear(u32t mode, u32t ypos, u32t lines, u32t color) {
   u32t  rc = 0;
   if (common_clear(mode, ypos, lines, color)) {
      con_modeinfo *mi = modes+mode;

      if (mi->physmap) rc = 1; else
      if (call64(EFN_GRFCLEAR, 0, 5, 0, ypos, mi->width, lines, color)>0) rc = 1;
   }
   return rc;
}

int plinit_efi(void) {
   pl_setmode   = out_setmode;
   pl_leavemode = out_leavemode;
   pl_copy      = out_copy;
   pl_flush     = out_flush;
   pl_scroll    = out_scroll;
   pl_clear     = out_clear;
   pl_addfont   = out_addfonts;
   pl_setup     = out_init;
   pl_close     = out_close;
   return 1;
}
