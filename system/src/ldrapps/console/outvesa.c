//
// QSINIT
// screen support dll: vesa output
//
#include "dpmi.h"
#include "stdlib.h"
#include "conint.h"
#include "qspage.h"
#include "vio.h"
#include "cpudef.h"
#include "qsxcpt.h"
#include "qshm.h"

#define VGA_TEXT_MODES 3
#define TEXTMEM_SEG    0xB800
#define NONFB_WINA     1
#define NONFB_WINB     2

u16t              vesa_ver = 0; // vesa version
u32t             nonfb_cnt = 0; // number of non-LFB modes
// disk r/w buffer, used for vesa calls
extern boot_data boot_info;
// current mode vars
static u32t   gran_per_win = 0, // granularity units in window
                  win_size = 0, // window size in bytes
               current_win = 0,
                mode_pitch = 0, // line length of current mode
                  mode_bpp = 0,
               mode_bshift = 0; // bytes<<width (possible since we drop 24-bit)
static u8t        *win_ptr = 0,
                   win_num = 0; // winA=0, winB=1

typedef struct {
   u16t       mx, my;
   u16t       cx, cy;
   u8t   bpp, mmodel;
   u8t   rsize, rpos;
   u8t   gsize, gpos;
   u8t   bsize, bpos;
   u8t   asize, apos;
} default_mode_info;

static default_mode_info vesa11_modes[0x1C] = {
           /*  x     y         bpp mm    red   green  blue   res */
/* 100h */ {  640,  400, 0,  0,  8, 4,  0, 0,  0, 0,  0, 0,  0, 0},
           {  640,  480, 0,  0,  8, 4,  0, 0,  0, 0,  0, 0,  0, 0},
           {  800,  600, 0,  0,  4, 3,  0, 0,  0, 0,  0, 0,  0, 0},
           {  800,  600, 0,  0,  8, 4,  0, 0,  0, 0,  0, 0,  0, 0},
/* 104h */ { 1024,  768, 0,  0,  4, 3,  0, 0,  0, 0,  0, 0,  0, 0},
           { 1024,  768, 0,  0,  8, 4,  0, 0,  0, 0,  0, 0,  0, 0},
           { 1280, 1024, 0,  0,  4, 3,  0, 0,  0, 0,  0, 0,  0, 0},
           { 1280, 1024, 0,  0,  8, 4,  0, 0,  0, 0,  0, 0,  0, 0},
/* 108h */ {   80,   60, 8,  8,  4, 0,  0, 0,  0, 0,  0, 0,  0, 0},
           {  132,   25, 8, 16,  4, 0,  0, 0,  0, 0,  0, 0,  0, 0},
           {  132,   43, 8,  8,  4, 0,  0, 0,  0, 0,  0, 0,  0, 0},
           {  132,   50, 8,  8,  4, 0,  0, 0,  0, 0,  0, 0,  0, 0},
/* 10Ch */ {  132,   60, 8,  8,  4, 0,  0, 0,  0, 0,  0, 0,  0, 0},
           {  320,  200, 0,  0, 16, 6,  5,10,  5, 5,  5, 0,  1,15},
           {  320,  200, 0,  0, 16, 6,  5,11,  6, 5,  5, 0,  0, 0},
           {  320,  200, 0,  0, 24, 6,  8,16,  8, 8,  8, 0,  0, 0},
/* 110h */ {  640,  480, 0,  0, 16, 6,  5,10,  5, 5,  5, 0,  1,15},
           {  640,  480, 0,  0, 16, 6,  5,11,  6, 5,  5, 0,  0, 0},
           {  640,  480, 0,  0, 24, 6,  8,16,  8, 8,  8, 0,  0, 0},
           {  800,  600, 0,  0, 16, 6,  5,10,  5, 5,  5, 0,  1,15},
/* 114h */ {  800,  600, 0,  0, 16, 6,  5,11,  6, 5,  5, 0,  0, 0},
           {  800,  600, 0,  0, 24, 6,  8,16,  8, 8,  8, 0,  0, 0},
           { 1024,  768, 0,  0, 16, 6,  5,10,  5, 5,  5, 0,  1,15},
           { 1024,  768, 0,  0, 16, 6,  5,11,  6, 5,  5, 0,  0, 0},
/* 118h */ { 1024,  768, 0,  0, 24, 6,  8,16,  8, 8,  8, 0,  0, 0},
           { 1280, 1024, 0,  0, 16, 6,  5,10,  5, 5,  5, 0,  1,15},
           { 1280, 1024, 0,  0, 16, 6,  5,11,  6, 5,  5, 0,  0, 0},
           { 1280, 1024, 0,  0, 24, 6,  8,16,  8, 8,  8, 0,  0, 0}
};

static int is_virtualpc(void) {
   // we have CMOV and have no LAPIC?
   if (sys_isavail(SFEA_CMOV|SFEA_LAPIC)==SFEA_CMOV) {
      _try_ {
         __asm {  // VPC get time from host
            db  0Fh, 3Fh, 3, 0
         }
         _ret_in_try_(1);
      }
      _catch_(xcpt_all) {
      }
      _endcatch_
   }
   return 0;
}

static u32t int10h(struct rmcallregs_s *regs) {
   hlp_rmcallreg(0x10, regs, 0);
   return regs->r_flags&CPU_EFLAGS_CF?0:1;
}

static int fixmodeinfo(u16t mode, VBEModeInfo *cm) {
   if ((cm->ModeAttributes&VSMI_SUPPORTED)==0) return 0;

   /* I have S3 864 with VESA 1.0 somewhere, but too lazy
      to find, insert and test it, any way ;)
      But who knows these crazy BIOS writers ... and someone
      can dig up an ancient laptop with 1.1, for example */
   if ((cm->ModeAttributes&VSMI_OPTINFO)==0) {
      if (mode>=0x100 && mode<=0x11B) {
         default_mode_info *dmi = vesa11_modes + (mode-0x100);
         // mode type (text/graphic) must be equal!
         if (Xor(cm->ModeAttributes&VSMI_GRAPHMODE,dmi->mmodel)) return 0;

         cm->XResolution        = dmi->mx;
         cm->YResolution        = dmi->my;
         cm->XCharSize          = dmi->cx?dmi->cx:8;
         cm->YCharSize          = dmi->cy?dmi->cy:8;
         cm->MemoryModel        = dmi->mmodel;
         cm->BitsPerPixel       = dmi->bpp;
         cm->RedMaskSize        = dmi->rsize;
         cm->RedFieldPosition   = dmi->rpos;
         cm->GreenMaskSize      = dmi->gsize;
         cm->GreenFieldPosition = dmi->gpos;
         cm->BlueMaskSize       = dmi->bsize;
         cm->BlueFieldPosition  = dmi->bpos;
         cm->RsvdMaskSize       = dmi->asize;
         cm->RsvdFieldPosition  = dmi->apos;
      }
   }
   if (cm->ModeAttributes&VSMI_GRAPHMODE) {
      // LFB disabled
      if (!fbaddr_enabled) cm->ModeAttributes&=~VSMI_LINEAR;
      // non-LFB mode
      if ((cm->ModeAttributes&VSMI_LINEAR)==0) {
         /* just drop all 24-bit modes. *Censored* off this crazy
            handling of splitted pixel at the 64k border */
         if (cm->BitsPerPixel==24) return 0;
         /* determine output window.
            At least Tseng ET4000 was supplied with readable A
            and writable B. Use only write window because we
            have forced shadow buffer for readings */
         if ((cm->WinAAttributes&(VSWA_EXISTS|VSWA_WRITEABLE))==
            (VSWA_EXISTS|VSWA_WRITEABLE) && cm->WinASegment)
               cm->Reserved = NONFB_WINA;
         else
         if ((cm->WinBAttributes&(VSWA_EXISTS|VSWA_WRITEABLE))==
            (VSWA_EXISTS|VSWA_WRITEABLE) && cm->WinBSegment)
               cm->Reserved = NONFB_WINB;
         else
            cm->Reserved = 0;
      }
   }
   return 1;
}

/** query all available vesa modes
    @param        mmask     mask for color modes: bit 0 = 8 bit, 1 = 15 bit,
                            2 = 16 bit, 3 = 24 bit, 4 = 32 bit, 5 = 1 bit,
                            6 = 4 bit, 0x5F is used by default.
    @return number of supported modes */
void vesadetect(u32t mmask) {
   u16t       rmbuff = boot_info.diskbuf_seg, *mpt;
/* note: using real mode disk buffer to get vesa info.
   Vesa info and vesa mode info must use separate buffes, because at least VBox,
   puts mode list into VBEInfo->Reserved area */
   VBEInfo     *vib = (VBEInfo*)hlp_segtoflat(rmbuff);
   VBEModeInfo  *cm = (VBEModeInfo*)((u8t*)vib + Round1k(sizeof(VBEInfo)));
   struct rmcallregs_s rr;
   /* vpc 2007 reports LFB, but does not support it actually.
      This, also is a nice test of bank switching antiquity. */
   if (is_virtualpc()) {
      log_it(0, "VirtualPC detected, disabling LFB!\n");
      fbaddr_enabled = 0;
   }
   // lock is required here, because this buffer is used in disk i/o
   mt_swlock();

   memset(&rr, 0, sizeof(rr));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F00;
   // make vesa 2.x call
   vib->VBESignature = MAKEID4('V','B','E','2');
   // query list of modes
   if (!int10h(&rr)) { mt_swunlock(); return; }
   if ((rr.r_eax&0xFFFF)!=0x004F) { mt_swunlock(); return; }
   nonfb_cnt = 0;
   vesa_ver  = vib->VBEVersion;
   vmem_size = (u32t)vib->TotalMemory<<16;
   mpt       = (u16t*)hlp_rmtopm(vib->ModeTablePtr);
   // total video memory size
   log_printf("vesa %hx: %ukb\n", vesa_ver, vib->TotalMemory<<6);

   while (*mpt!=0xFFFF && mode_cnt<MAX_MODES) {
      u16t mode_num = *mpt;
      memset(&rr, 0, sizeof(rr));
      memset(cm, 0, sizeof(VBEModeInfo));
      rr.r_ds  = rmbuff;
      rr.r_es  = rmbuff;
      rr.r_edi = Round1k(sizeof(VBEInfo));
      rr.r_eax = 0x4F01;
      rr.r_ecx = mode_num;
      // query mode
      if (int10h(&rr)) {
         if ((rr.r_eax&0xFFFF)==0x004F) {
            do {
               u32t bpp, mmdl;
               if ((cm->ModeAttributes&VSMI_SUPPORTED)==0) break;
               // update some fields
               if (!fixmodeinfo(mode_num,cm)) break;

               bpp  = cm->NumberOfPlanes * cm->BitsPerPixel;
               mmdl = cm->MemoryModel;

               if (cm->NumberOfPlanes>1 && cm->BitsPerPixel!=1) break;
               // check graphic modes only
               if (cm->ModeAttributes&VSMI_GRAPHMODE) {
                  int   mvalue;
                  // no LFB
                  if ((cm->ModeAttributes&VSMI_LINEAR)==0) {
                     // and no banks?
                     if ((cm->ModeAttributes&VSMI_NOBANK)) break;
                     // or we can`t find writable one?
                     if (!cm->Reserved) break;
                  }
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
                  // increase counter for happy finalist
                  if ((cm->ModeAttributes&VSMI_LINEAR)==0) nonfb_cnt++;
               } else {
                  // drop incorrect modes
                  if (cm->XResolution>132 || cm->YResolution>60) break;
               }
               modes[mode_cnt] = malloc(sizeof(modeinfo)+sizeof(VBEModeInfo));
               memset(modes[mode_cnt], 0, sizeof(modeinfo));
               memcpy(modes[mode_cnt]+1, cm, sizeof(VBEModeInfo));
               /* log_printf("%4u x %4u -> %08X\n", cm->XResolution,
                  cm->YResolution, cm->PhysBasePtr); */
               modes[mode_cnt++]->modenum = *mpt;
            } while (false);
         }
      }
      mpt++;
   }
   mt_swunlock();
   // re-init handlers because we have bank switching :(
   if (nonfb_cnt) plinit_vesa();

   return;
}

/// call mode info for single mode
u32t vesamodeinfo(VBEModeInfo *vinfo, u16t mode) {
   u16t     rmbuff = boot_info.diskbuf_seg, *modes;
   VBEModeInfo *cm = (VBEModeInfo*)hlp_segtoflat(rmbuff);
   u32t        res = 0;
   struct rmcallregs_s rr;
   // lock is required here, because this buffer is used in disk i/o
   mt_swlock();
   memset(&rr, 0, sizeof(rr));
   memset(cm, 0, sizeof(VBEModeInfo));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F01;
   rr.r_ecx = mode;
   // query mode
   if (int10h(&rr)) {
      // update it and hope it will be the same ;)
      if (fixmodeinfo(mode,cm)) *vinfo = *cm;
      res = 1;
   }
   mt_swunlock();
   return res;
}

/// call mode info for single mode
u32t vesasetmode(VBEModeInfo *vinfo, u16t mode, u32t flags) {
   struct rmcallregs_s rr;
   memset(&rr, 0, sizeof(rr));
   rr.r_eax = 0x4F02;
   rr.r_ebx = mode;

   if (flags&CON_NOSCREENCLEAR) rr.r_ebx|=0x8000;
   if ((flags&CON_GRAPHMODE) && (vinfo->ModeAttributes&VSMI_LINEAR))
      rr.r_ebx|=0x4000;
   // set mode
   if (!int10h(&rr)) return 0;
   if ((rr.r_eax&0xFFFF)!=0x004F) return 0;
/*
   if ((flags&CON_GRAPHMODE)==0) {
      vio_intensity(1);
      vio_getmode(0,0);
      if ((flags&CON_NOSCREENCLEAR)==0) vio_clearscr(); else
         vio_setpos(0,0);
   } */
   return 1;
}

static u32t set_window(u32t fbofs) {
   if (win_ptr) {
      u32t win = fbofs/win_size,
           ofs = fbofs%win_size;
      if (win!=current_win) {
         struct rmcallregs_s rr;
         memset(&rr, 0, sizeof(rr));
         rr.r_eax = 0x4F05;
         rr.r_ebx = win_num;
         rr.r_edx = win*gran_per_win;

         int10h(&rr);

         current_win = win;
      }
      return ofs;
   }
   return 0;
}


static u32t _std out_dirclear(u32t x, u32t y, u32t dx, u32t dy, u32t color) {
   u32t  start = y*mode_pitch + (x<<mode_bshift),
           pos = start,
       linelen = dx<<mode_bshift;

   while (dy) {
      u32t ofs = set_window(pos),
           rem = win_size - ofs;
      if (rem>=linelen) rem = linelen;

      con_fillblock(win_ptr + ofs, rem>>mode_bshift, 1, mode_pitch, color, mode_bpp);

      if (rem==linelen) {
         start+= mode_pitch;
         pos   = start;
         dy--;
      } else {
         pos  += rem;
      }
   }
   return 1;
}

static u32t _std out_dirblit(u32t x, u32t y, u32t dx, u32t dy, void *src, u32t srcpitch) {
   u32t  start = y*mode_pitch + (x<<mode_bshift),
           pos = start,
       linelen = dx<<mode_bshift;
   u8t   *sptr = (u8t*)src, *spos = sptr;

   while (dy) {
      u32t ofs = set_window(pos),
           rem = win_size - ofs;
      if (rem>=linelen) rem = linelen;
      memcpy(win_ptr + ofs, spos, rem);
      if (rem==linelen) {
         sptr += srcpitch;
         spos  = sptr;
         start+= mode_pitch;
         pos   = start;
         dy--;
      } else {
         spos += rem;
         pos  += rem;
      }
   }
   return 1;
}


static u32t _std out_copy(u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch, int write) {
   modeinfo *mi = modes[mode];
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

static u32t _std out_setmodeid(u32t mode_id) {
   if (mode_id>=mode_cnt) return 0; else
   if (mode_id<3) {
      con_unsetmode();
      cvio->vh_setmode(mode_id==0?25:(mode_id==1?43:50));
      // con_unsetmode() is called from hook here
      current_mode = mode_id;
      real_mode    = current_mode;
      mode_changed = 1;
      in_native    = 1;
      return 1;
   } else {
      int    idx = mode_id,
          actual = idx;
      u32t flags = modes[idx]->flags;
      /* is this an emulated text mode? select graphic mode and force
         "shadow buffer" flag */
      if (flags&CON_EMULATED) {
         actual = modes[idx]->realmode;
         flags |= CON_SHADOW;
         // internal error?
         if (actual<0) idx=-1;
      }

      if (idx>=0) {
         modeinfo    *m_pub = modes[idx],    // public mode info
                     *m_act = modes[actual]; // actual mode info
         VBEModeInfo    *mi = (VBEModeInfo *)(m_act+1);
         int          no_fb = (mi->ModeAttributes&VSMI_LINEAR)==0;
         // force shadow if no LFB here
         if (no_fb) flags|=CON_SHADOW;
         /* set mode, but only if we it does not match to current real mode */
         if (real_mode==actual)
            pl_clear(real_mode, 0, 0, m_act->width, m_act->height, 0);
         else
            if (!vesasetmode(mi, m_act->modenum, flags)) return 0;
         // free previous mode data
         con_unsetmode();
         // trying to update mode info
         vesamodeinfo(mi, m_act->modenum);
         // and pointers (for graphic/emulated text mode, not real text)
         if ((m_act->flags&CON_GRAPHMODE)!=0) {
            static const char *cstr[MTRRF_TYPEMASK+1] = { "UC", "WC", "??", "??",
              "WT", "WP", "WB", "Mixed" };
            u32t     cstart,
                   ct, clen = mi->BytesPerScanline*mi->YResolution;

            m_act->physaddr = no_fb ? 0 : mi->PhysBasePtr;
            m_act->mempitch = mi->BytesPerScanline;
            m_act->physmap  = no_fb ? 0 : (u8t*)pag_physmap(mi->PhysBasePtr,clen,0);

            if (no_fb) {
               gran_per_win = mi->WinSize/mi->Granularity;
               win_size     = (mi->Granularity<<10) * gran_per_win;
               current_win  = FFFF;
               win_num      = mi->Reserved==NONFB_WINA?0:1;
               cstart       = hlp_segtoflat(win_num?mi->WinBSegment:mi->WinASegment);
               win_ptr      = (u8t*)cstart;
               clen         = mi->WinSize<<10;
            } else {
               win_ptr      = 0;
               cstart       = m_act->physaddr;
            }
            // log used video memory cache type
            ct = hlp_mtrrsum(cstart, clen);

            log_it(3,"%dx%d %d, %s now on %08X..%08X\n", mi->XResolution, mi->YResolution,
               mi->BytesPerScanline, cstr[ct], cstart, cstart+clen-1);

            mode_pitch     = mi->BytesPerScanline;
            mode_bpp       = BytesBP(mi->BitsPerPixel);
            mode_bshift    = bsf32(mode_bpp);
         }
         // allocate shadow buffer (allowed both for text & graphic mode)
         if (flags&CON_SHADOW)
            if (m_act->bits<8 || !alloc_shadow(m_act,flags&CON_NOSCREENCLEAR))
               flags&=~CON_SHADOW;

         current_mode   = idx;
         real_mode      = actual;
         current_flags  = flags;
         mode_changed   = 1;
         in_native      = m_pub->flags&(CON_GRAPHMODE|CON_EMULATED)?0:1;
         /* this call sync cvio mode information with BIOS data (BIOS _must_
            set it properly) */
         if (in_native) cvio->vh_getmfast(0,0);
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
   int  idx = search_mode(x,y,flags);
   if (idx<0) return 0;
   return out_setmodeid(idx);
}

static void _std out_leavemode(void) {
   modeinfo *mi = 0;
   do {
      mi = modes[mi?current_mode:real_mode];
      if (mi->physmap) {
         pag_physunmap(mi->physmap);
         mi->physmap = 0;
      }
   } while (mi!=modes[current_mode]);
}

static void _std out_init(void) {
   u32t   ii;
   // common text modes
   for (ii=0; ii<VGA_TEXT_MODES; ii++) {
      modeinfo *mi = (modeinfo*)malloc(sizeof(modeinfo));
      memset(mi, 0, sizeof(modeinfo));
      modes[ii]  = mi;
      mi->width  = 80;
      mi->height = ii==0?25:(ii==1?43:50);
      mi->font_x = ii?8:9;
      mi->font_y = ii?8:16;
   }
   mode_cnt = VGA_TEXT_MODES;

   if (!textonly) vesadetect(enabled_modemask);

   for (ii = VGA_TEXT_MODES; ii<mode_cnt; ii++) {
      u32t *fp;
      modeinfo    *mi = modes[ii];
      VBEModeInfo *vi = (VBEModeInfo *)(mi+1);
      mi->width       = vi->XResolution;
      mi->height      = vi->YResolution;
      mi->realmode    = -1;

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
         mi->physaddr = vi->ModeAttributes&VSMI_LINEAR ? vi->PhysBasePtr : 0;
         // detect, check & save lowest used memory address value
         if (mi->physaddr && mi->physaddr>_1MB) {
            if (!vmem_addr) vmem_addr = mi->physaddr; else
            if ((u32t)abs((int)vmem_addr - (int)mi->physaddr)<vmem_size) {
               if (mi->physaddr<vmem_addr) vmem_addr = mi->physaddr;
            } else {
               log_it(2,"Video mem (%dkb) mismatch: %08X for previous mode(s) "
                  "and %08X for mode %dx%dx%d\n", vmem_size>>10, vmem_addr,
                     mi->physaddr, mi->width, mi->height, mi->bits);
            }
         }

         mi->mempitch = vi->BytesPerScanline;
      } else {
         mi->physaddr = TEXTMEM_SEG<<4;
         mi->mempitch = vi->XResolution<<1;
         mi->font_x   = vi->XCharSize;
         mi->font_y   = vi->YCharSize;
      }
   }
}

static void out_close(void) {
   vmem_size = 0;
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
   if (rr.r_es==0) return 0;

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
        fb = (fontbits*)mem_dup(fb);
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
        con_addfontmodes(fb->x, fb->y);
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
   con_addfontmodes(fb->x, fb->y);
   return fb;
}

static void out_addfonts(void) {
   fontbits *fb;
   if (!sysfnt) return;
   // 8x14 is broken in most of BIOS, which I seen
#if 0
   fb = addfont(2,0);     // 8 x 14, 9 x 14
   if (fb) addfont(5,fb);
#endif
   fb = addfont(6,0);     // 8 x 16, 9 x 16
   if (fb) addfont(7,fb); else
      log_it(2,"8x16 font error!\n");
   fb = addfont(3,0);     // 8 x 8
   if (fb) addfont(4,fb); else
      log_it(2,"8x8 font error!\n");
}

int plinit_vesa(void) {
   pl_setmode   = out_setmode;
   pl_setmodeid = out_setmodeid;
   pl_leavemode = out_leavemode;
   pl_copy      = out_copy;
   pl_flush     = nonfb_cnt ? common_flush_nofb : common_flush;
   pl_scroll    = nonfb_cnt ? common_scroll_nofb: common_scroll;
   pl_clear     = nonfb_cnt ? common_clear_nofb : common_clear;
   pl_addfont   = out_addfonts;
   pl_setup     = out_init;
   pl_close     = out_close;
   pl_dirclear  = out_dirclear;
   pl_dirblit   = out_dirblit;
   return 1;
}
