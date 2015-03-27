//
// QSINIT
// screen support dll: vesa output
//
#include "stdlib.h"
#include "qsutil.h"
#include "conint.h"
#include "qspage.h"
#include "qsint.h"
#include "vio.h"
#include "vbedata.h"
#include "dpmi.h"

#define TEXTMEM_SEG    0xB800

u32t int10h(struct rmcallregs_s *regs);
#ifdef __WATCOMC__
#pragma aux int10h = \
   "mov   ax,300h"   \
   "mov   bx,10h"    \
   "xor   ecx,ecx"   \
   "int   31h"       \
   "setnc al"        \
   "movzx eax,al"    \
   parm  [edi]       \
   value [eax]       \
   modify [ebx ecx];
#endif // __WATCOMC__

VBEModeInfo         *vinfo = 0; // vesa mode info
u32t              vesa_cnt = 0; // vesa mode info array size
u32t          vesa_memsize = 0; // vesa memory size
// disk r/w buffer, used for vesa calls
extern boot_data boot_info;

/// call mode info for single mode
u32t  vesamodeinfo(VBEModeInfo *vinfo, u16t mode);

/// set mode
u32t  vesasetmode(VBEModeInfo *vinfo, u16t mode, u32t flags);

/** query all available vesa modes
    @param         vinfo    array for modes (256 entries)
    @param         minfo    mode info array for mode numbers
    @param         mmask    bitmask for color modes: bit 0 = 8 bit, 
                            bit 1 = 15 bit, bit 2 = 16 bit, bit 3 = 24 bit,
                            bit 4 = 32 bit, bit 5 = 1 bit,  bit 6 = 4 bit.
                            By default 0x5F is used.
    @param   [out] memsize  Total video memory size, can be 0
    @return  number of supported modes (<=MAXVESA_MODES) */
u32t vesadetect(VBEModeInfo *vinfo, con_intinfo *minfo, u32t mmask, u32t *memsize) {
   u16t       rmbuff = boot_info.diskbuf_seg, *modes;
/* note: using real mode disk buffer to get vesa info.
   Vesa info and vesa mode info must use separate buffes, because at least VBox,
   puts mode list into VBEInfo->Reserved area */
   VBEInfo     *vib = (VBEInfo*)hlp_segtoflat(rmbuff);
   VBEModeInfo  *cm = (VBEModeInfo*)((u8t*)vib + Round1k(sizeof(VBEInfo)));
   struct rmcallregs_s rr;
   u32t  ii;
   if (memsize) *memsize = 0;

   memset(&rr, 0, sizeof(rr));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F00;
   // make vesa 2.x call
   vib->VBESignature = MAKEID4('V','B','E','2');
   // query list of modes
   if (!int10h(&rr)) return 0;
   if ((rr.r_eax&0xFFFF)!=0x004F) return 0;
   ii = vib->ModeTablePtr;
   // log_printf("mode list ptr: %04X:%04X\n", ii>>16, ii&0xFFFF);
   modes = (u16t*)hlp_rmtopm(ii);
   // total video memory size
   if (memsize) *memsize = (u32t)vib->TotalMemory<<16;
   log_printf("%ukb of vesa memory\n",vib->TotalMemory<<6);

   ii = 0;
   while (*modes!=0xFFFF && ii<MAXVESA_MODES) {
      memset(&rr, 0, sizeof(rr));
      memset(cm, 0, sizeof(VBEModeInfo));
      rr.r_ds  = rmbuff;
      rr.r_es  = rmbuff;
      rr.r_edi = Round1k(sizeof(VBEInfo));
      rr.r_eax = 0x4F01;
      rr.r_ecx = *modes;
      // query mode
      if (int10h(&rr)) {
         if ((rr.r_eax&0xFFFF)==0x004F) {
            do {
               u32t bpp = cm->NumberOfPlanes * cm->BitsPerPixel,
                   mmdl = cm->MemoryModel;
               if ((cm->ModeAttributes&VSMI_SUPPORTED)==0) break;
               if ((cm->ModeAttributes&VSMI_OPTINFO)==0) break;
               if (cm->NumberOfPlanes>1 && cm->BitsPerPixel!=1) break;
               // check graphic modes only
               if (cm->ModeAttributes&VSMI_GRAPHMODE) {
                  int   mvalue;
                  // non-LFB mode
                  if ((cm->ModeAttributes&VSMI_LINEAR)==0) break;
                  // check memory model
                  if (bpp==4 && mmdl!=3 || bpp==8 && mmdl!=4 ||
                      bpp>8 && !(mmdl==6||mmdl==4)) break;
                  // get bit for mmask
                  mvalue = bpp2bit(cm->BitsPerPixel);
                  // unknown number of pixels
                  if (mvalue<0) break;
                  // mode deprecated by user
                  if ((mmask&mvalue)==0) break;
                  if (modelimit_x && modelimit_x<cm->XResolution) break;
                  if (modelimit_y && modelimit_y<cm->YResolution) break;
               } else {
                  // drop incorrect modes
                  if (cm->XResolution>132 || cm->YResolution>60) break;
               }
               vinfo[ii] = *cm;
               minfo[ii].vesaref = ii;
               minfo[ii].modenum = *modes;

               ii++;
            } while (false);
         }
      }
      modes++;
   }

   return ii;
}

/// call mode info for single mode
u32t vesamodeinfo(VBEModeInfo *vinfo, u16t mode) {
   u16t     rmbuff = boot_info.diskbuf_seg, *modes;
   VBEModeInfo *cm = (VBEModeInfo*)hlp_segtoflat(rmbuff);
   struct rmcallregs_s rr;

   memset(&rr, 0, sizeof(rr));
   memset(cm, 0, sizeof(VBEModeInfo));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F01;
   rr.r_ecx = mode;
   // query mode
   if (int10h(&rr)) {
      *vinfo = *cm;
      return 1;
   }
   return 0;
}

/// call mode info for single mode
u32t vesasetmode(VBEModeInfo *vinfo, u16t mode, u32t flags) {
   struct rmcallregs_s rr;
   memset(&rr, 0, sizeof(rr));
   rr.r_eax = 0x4F02;
   rr.r_ebx = mode;

   if (flags&CON_NOSCREENCLEAR) rr.r_ebx|=0x8000;
   if ((flags&CON_GRAPHMODE)!=0) rr.r_ebx|=0x4000;
   // set mode
   if (!int10h(&rr)) return 0;
   if ((rr.r_eax&0xFFFF)!=0x004F) return 0;
   if ((flags&CON_GRAPHMODE)==0) {
      vio_intensity(1);
      vio_getmode(0,0);
      if ((flags&CON_NOSCREENCLEAR)==0) vio_clearscr(); else
         vio_setpos(0,0);
   }
   return 1;
}

static u32t _std out_copy(u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch, int write) {
   con_modeinfo *mi = modes+mode;
   if (mode>=mode_cnt) return 0;
   // copying memory
   if ((mi->flags&CON_GRAPHMODE)==0) {
      // normal or emulated text mode?
      if ((mi->flags&CON_EMULATED)==0) {
         u16t *b800 = (u16t*)hlp_segtoflat(TEXTMEM_SEG);

         if (write) {
            MoveBlock(buf, b800+y*mi->width+x, dx<<1, dy, pitch, mi->width<<1);
            // update shadow buffer
            if (mi->shadow) MoveBlock(buf, mi->shadow + y*mi->shadowpitch + x*2,
               dx<<1, dy, pitch, mi->shadowpitch);
         } else 
            MoveBlock(b800+y*mi->width+x, buf, dx<<1, dy, mi->width<<1, pitch);
      } else {
         if (write) evio_writebuf(x,y,dx,dy,buf,pitch);
            else evio_readbuf(x,y,dx,dy,buf,pitch);
      }
      return 1;
   } else
      return common_copy(mode, x, y, dx, dy, buf, pitch, write);
}

static u32t _std out_setmode(u32t x, u32t y, u32t flags) {
   if ((flags&CON_GRAPHMODE)==0 && x==80 && (y==25||y==43||y==50)) {
      vio_setmode(y);
      // con_unsetmode() is called from hook here
      current_mode = (y-25)/12;
      real_mode    = current_mode;
      mode_changed = 1;
      return 1;
   } else {
      int  idx = search_mode(x,y,flags),
        actual = idx;
      /* is this an emulated text mode? select graphic mode and force
         "shadow buffer" flag */
      if (idx>=0 && (modes[idx].flags&CON_EMULATED)!=0) {
         actual = mref[idx].realmode;
         flags |= CON_SHADOW;
         // internal error?
         if (actual<0) idx=-1;
      }

      if (idx>=0) {
         con_modeinfo *m_pub = modes + idx,    // public mode info
                      *m_act = modes + actual; // actual mode info
         VBEModeInfo  *mi    = vinfo + mref[actual].vesaref;

         // set mode
         if (!vesasetmode(mi,mref[actual].modenum,flags)) return 0;
         // free previous mode data
         con_unsetmode();
         // trying to update mode info
         vesamodeinfo(mi,mref[actual].modenum);
         // and pointers (for graphic/emulated text mode, not real text)
         if ((m_act->flags&CON_GRAPHMODE)!=0) {
            m_act->physaddr = mi->PhysBasePtr;
            m_act->mempitch = mi->BytesPerScanline;
            m_act->physmap  = (u8t*)pag_physmap(mi->PhysBasePtr,
               mi->BytesPerScanline*mi->YResolution,0);
            log_it(3,"%08X %d %d %d\n", mi->PhysBasePtr, mi->BytesPerScanline,
               mi->XResolution, mi->YResolution);
         }
         // allocate shadow buffer (allowed both for text & graphic mode)
         if (flags&CON_SHADOW)
            if (m_act->bits<8 || !alloc_shadow(m_act,flags&CON_NOSCREENCLEAR)) 
               flags&=~CON_SHADOW;

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
      if (mi->physmap) {
         pag_physunmap(mi->physmap);
         mi->physmap = 0;
      }
   } while (mi!=modes+current_mode);
}

static void _std out_init(void) {
   u32t   ii;

   if (!textonly) {
      vinfo = (VBEModeInfo*) malloc(sizeof(VBEModeInfo)*MAXVESA_MODES);
      memZero(vinfo);
      vesa_cnt = vesadetect(vinfo, mref+PREDEFINED_MODES, enabled_modemask, &vesa_memsize);
      if (!vesa_cnt) { free(vinfo); vinfo = 0; }
   } else
      vesa_cnt = 0;

   // total number of available modes
   mode_cnt   = PREDEFINED_MODES + vesa_cnt;
   mode_limit = mode_cnt + MAXEMU_MODES + 1;
   modes      = (con_modeinfo*)malloc(sizeof(con_modeinfo)*mode_limit);
   memZero(modes);
   // common text modes
   modes[0].width = modes[1].width = modes[2].width = 80;
   modes[0].height = 25; modes[0].font_x = 9; modes[0].font_y = 16; 
   modes[1].height = 43; modes[1].font_x = 8; modes[1].font_y = 8;
   modes[2].height = 50; modes[2].font_x = 8; modes[2].font_y = 8;

   for (ii = PREDEFINED_MODES; ii<mode_cnt; ii++) {
      u32t *fp;
      VBEModeInfo  *vi  = vinfo + mref[ii].vesaref;
      con_modeinfo *mi  = modes + ii;
      mi->width         = vi->XResolution;
      mi->height        = vi->YResolution;
      mref[ii].realmode = -1;

      if (vi->ModeAttributes&VSMI_GRAPHMODE) {
         static u32t  fbits[] = {1, 4, 8, 15, 16, 24, 32, 0};

         mi->bits   = vi->NumberOfPlanes * vi->BitsPerPixel;
         mi->flags  = CON_GRAPHMODE;
         // CON_COLOR_XXXX flags
         fp = memchrd(fbits, mi->bits, 8);
         if (fp) mi->flags|= (fp-fbits)<<8;

         if (mi->bits>8) {
            mi->rmask = (1<<vi->RedMaskSize)-1<<vi->RedFieldPosition;
            mi->gmask = (1<<vi->GreenMaskSize)-1<<vi->GreenFieldPosition;
            mi->bmask = (1<<vi->BlueMaskSize)-1<<vi->BlueFieldPosition;
            mi->amask = (1<<vi->RsvdMaskSize)-1<<vi->RsvdFieldPosition;
            // fix wrong BIOS value
            if (mi->bits==16 && mi->rmask==0x7C00 && mi->gmask==0x3E0) mi->bits=15;
         }
         mi->physaddr = vi->PhysBasePtr;
         mi->mempitch = vi->BytesPerScanline;
         mi->memsize  = vesa_memsize;
      } else {
         mi->physaddr = TEXTMEM_SEG<<4;
         mi->memsize  = 0x8000;
         mi->mempitch = vi->XResolution<<1;
         mi->font_x   = vi->XCharSize;
         mi->font_y   = vi->YCharSize;
      }
   }
}

static void out_close(void) {
   if (vinfo) { free(vinfo); vinfo=0; }
   vesa_memsize = 0;
   vesa_cnt     = 0;
}

static fontbits *addfont(int ebx, fontbits *fb) {
   struct rmcallregs_s rr;
   u8t  *font;
   int     dy;

   memset(&rr, 0, sizeof(rr));
   rr.r_eax = 0x1130;
   rr.r_ebx = ebx<<8;
   if (!int10h(&rr)) return 0;
   font = (u8t*)(hlp_segtoflat(rr.r_es)+(rr.r_ebp&0xFFFF));

   switch (ebx) {
      case 3:
        fb = (fontbits*)malloc(8*256+8);
        dy=4; fb->x=8; fb->y=8;
        break;
      case 4:
        // high 128 characters
        memcpy(&fb->bin[1024], font, 1024);
        return 0;
      case 5:
      case 7: {
        int diffs = 0;
        fb = (fontbits*)memDup(fb);
        fb->x=9;
        // copy alternate font data
        while (*font) {
           //log_it(3,"diff for char %02X %c\n", *font, *font);

           dy = fb->y**font++;
           memcpy(&fb->bin[dy], font, fb->y);
           font += fb->y;
           diffs++;
        }
        log_it(3,"9x%d font: %d diffs\n", fb->y, diffs);
        sysfnt->add(fb);
        return 0;
      }
      case 2:
      case 6:
        dy = ebx==2?14:16;
        fb = (fontbits*)malloc(dy*256+8);
        fb->x=8; fb->y=dy;
        break;
      default:
        return 0;
   }
   memcpy(&fb->bin, font, dy*256);
   sysfnt->add(fb);
   return fb;
}

static void out_addfonts(void) {
   fontbits *fb;
   if (!sysfnt) return;
   fb = addfont(2,0);     // 8 x 14, 9 x 14
   if (fb) addfont(5,fb);
   fb = addfont(6,0);     // 8 x 16, 9 x 16
   if (fb) addfont(7,fb);
   fb = addfont(3,0);     // 8 x 8
   if (fb) addfont(4,fb); 
}

int plinit_vesa(void) {
   pl_setmode   = out_setmode;
   pl_leavemode = out_leavemode;
   pl_copy      = out_copy;
   pl_flush     = common_flush;
   pl_scroll    = common_scroll;
   pl_clear     = common_clear;
   pl_addfont   = out_addfonts;
   pl_setup     = out_init;
   pl_close     = out_close;
   return 1;
}
