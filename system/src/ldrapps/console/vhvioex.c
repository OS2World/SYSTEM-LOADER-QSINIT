//
// QSINIT "console" module
// vio handler replacement: graphic console + common text modes support
//
#include "qcl/sys/qsvioint.h"
#include "conint.h"
#include "qserr.h"

u32t             gc_classid = 0;
static qs_vh           self = 0,
                     vh_vio = 0;

static u32t       *fnt_data = 0,
                   m_x, m_y,       // mode size
               fnt_x, fnt_y,       // mode font size
                   fnt_mult,       // 1..4
               pos_x, pos_y,       // cursor position
                    top_ofs,       // offset of 0,0 from top of screen, pixels
                   left_ofs,
                      vbpps;       // bytes per pixel in active mode

static u8t          txcolor = 7;
static u16t         txshape = 0,
                    vh_opts = 0;   // qs_vh options (garbage)
static u32t       tty_lines = 0;
static qs_kh       keyboard = 0;

extern u8t       stdpal_bin[51];   // text mode RGB colors
static u32t         m_color[16];

static con_drawinfo     cdi;

static void writechar(u32t x, u32t y, char ch, u8t color) {
   modeinfo   *mi = modes[real_mode];
   u32t       f_w = fnt_x*fnt_mult,
              f_h = fnt_y*fnt_mult,
             ypos = top_ofs+y*f_h,
             xpos = left_ofs+x*f_w,
          *chdata = fnt_data + (u8t)ch * fnt_y,
            bgcol = m_color[color>>4],
            txcol = m_color[color&0xF];
   int     cursor = x==pos_x && y==pos_y;

   if (mi->shadow) {
      u32t llen = mi->shadowpitch;
      con_drawchar(&cdi, txcol, bgcol, chdata, mi->shadow + ypos*llen +
                   xpos*vbpps, llen, cursor);
   }
   if (mi->physmap) {
      u32t llen = mi->mempitch;
      con_drawchar(&cdi, txcol, bgcol, chdata, mi->physmap + ypos*llen +
                   xpos*vbpps, llen, cursor);
   } else
      pl_flush(real_mode, xpos, ypos, f_w, f_h);
}

static void updatechar(u32t x, u32t y) {
   modeinfo *mi = modes[current_mode];
   u16t  chattr = ((u16t*)mi->shadow)[m_x*y+x];
   writechar(x, y, chattr, chattr>>8);
}

static u32t charout_common(char ch, int in_seq) {
   int  lines = 0;
   if (ch==9) {
      u32t rc = 0, ii;
      for (ii=0; ii<4; ii++) rc+=charout_common(' ', ii<3);
      return rc;
   } else
   if (ch==13) {
      u32t opx = pos_x;
      pos_x = 0;
      // update always, because no one will draw over old pos
      if (opx!=pos_x && cdi.cdi_CurMask) updatechar(opx, pos_y);
   } else {
      int scroll = 0; // eol was occur

      if (ch==10) {
         u32t opx = pos_x;
         pos_x = 0;
         // new line
         if (++pos_y==m_y) scroll = 1;
         lines = 1;
         // the same with \r
         if (cdi.cdi_CurMask) updatechar(opx, pos_y-1);
      } else {
         // update text mode buffer
         ((u16t*)modes[current_mode]->shadow)[m_x*pos_y + pos_x++] = (u16t)txcolor<<8|ch;
         // write over painted cursor will remove it, so no update here
         writechar(pos_x-1, pos_y, ch, txcolor);

         if (pos_x==m_x) {
            pos_x = 0;
            lines = 1;
            if (++pos_y==m_y) scroll = 1;
         }
      }
      // scroll screen
      if (scroll) {
         modeinfo   *mi = modes[current_mode];
         u32t   linesm1 = fnt_y*fnt_mult*(m_y-1),
                 tx_ofs = m_x*(m_y-1)*2;
         pl_scroll(real_mode, top_ofs+fnt_y*fnt_mult, top_ofs, linesm1);
         pos_y--;
         // clear last line in shadow buffer and on screen
         pl_clear(real_mode, 0, top_ofs+linesm1, 0, fnt_y*fnt_mult, m_color[txcolor>>4]);
         // update text mode buffer
         memmove(mi->shadow, mi->shadow + m_x*2, tx_ofs);
         memsetw((u16t*)(mi->shadow + tx_ofs), (u16t)txcolor<<8|0x20, m_x);
      }
   }
   // update cursor
   if (!in_seq && cdi.cdi_CurMask) updatechar(pos_x, pos_y);
   // update global line counter
   tty_lines += lines;

   return lines;
}

static qserr _exicc gc_init(EXI_DATA, const char *setup) { return 0; }
static qs_kh _exicc gc_link(EXI_DATA) { return keyboard; }

static void evio_setshape(u16t shape) {
   txshape = shape;
   cdi.cdi_CurMask = 0; 
   if (shape!=VIO_SHAPE_NONE) {
      u8t   bs, be;
      switch (shape) {
         case VIO_SHAPE_FULL: bs=0; be=fnt_y-1; break;
         case VIO_SHAPE_HALF: be=fnt_y-1; bs=be>>1; break;
         case VIO_SHAPE_WIDE: bs=fnt_y-2; be=fnt_y-1; break;
         default:
            bs=be=fnt_y-1;
      }
      if (bs>fnt_y-1) bs = fnt_y-1;
      if (be>fnt_y-1) be = fnt_y-1;
      while (bs<=be) cdi.cdi_CurMask |= 1<<fnt_y-1-bs++;
   }
   updatechar(pos_x, pos_y);
}

static qserr _exicc gc_setopts(EXI_DATA, u32t opts) {
   if (in_native) return vh_vio->setopts(opts); 

   // line counter setup call
   if (opts&VHSET_LINES) {
      tty_lines = opts & x7FFF;
      return 0;
   }
   // shape settings
   if (opts&VHSET_SHAPE) {
      evio_setshape(opts&0xFFFF);
      opts &= ~(VHSET_SHAPE|0xFFFF);
   }
   // and then validate remains (but ignore them ;))
   if (opts&~(VHSET_WRAPX|VHSET_BLINK)) return E_SYS_INVPARM;
   vh_opts = opts;
   return 0;
}

static u32t hicolor_cvt(u8t rc, u8t gc, u8t bc, u32t mode) {
   u32t res = 0,
       bits = modes[mode]->bits;
   // fix 16 to 15 bits
   if (bits==16 && modes[mode]->rmask==0x7C00 && modes[mode]->gmask==0x3E0) bits=15;

   switch (bits) {
      case 15: res = bc>>3 | gc>>3<<5 | rc>>3<<10; break;
      case 16: res = bc>>3 | gc>>2<<5 | rc>>3<<11; break;
      case 24:
      case 32:
         if (modes[mode]->rmask==0xFF) res = rc | gc<<8 | bc<<16; 
            else res = bc | gc<<8 | rc<<16;
         break;
   }
   return res;
}

/* some of 4xxx ATI cards can`t change palette at all, so select orange index
   in default palette to make simular view */
#define CURSOR_COLOR_IDX   42

void evio_newmode() {
   modeinfo *mi = modes[current_mode];
   u32t  ii;
   if (fnt_data) free(fnt_data);

   fnt_x     = mi->font_x; 
   fnt_y     = mi->font_y;
   fnt_mult  = ((mi->flags&CON_FONTxMASK)>>CON_FSCALE) + 1;
   fnt_data  = con_buildfont(fnt_x, fnt_y);
   m_x       = mi->width;
   m_y       = mi->height;
   pos_x     = 0;
   pos_y     = 0;

   // current mode info
   top_ofs   = modes[real_mode]->height - m_y*fnt_y*fnt_mult >> 1;
   left_ofs  = modes[real_mode]->width  - m_x*fnt_x*fnt_mult >> 1;
   vbpps     = BytesBP(modes[real_mode]->bits);
   txcolor   = 7;
   txshape   = VIO_SHAPE_LINE;
   vh_opts   = 0;
   in_native = 0;

   for (ii=0; ii<16; ii++) {
      u8t *ple = &stdpal_bin[ii*3];
      u32t  rc = vbpps==1 ? ii : hicolor_cvt(ple[0],ple[1],ple[2],real_mode);
      m_color[ii] = rc;
   }
   /* cursor color can`t be 0 - this is limitation of con_drawchar() code */
   cdi.cdi_FontX    = fnt_x;
   cdi.cdi_FontY    = fnt_y;
   cdi.cdi_CurColor = vbpps==1 ? CURSOR_COLOR_IDX : hicolor_cvt(stdpal_bin[48],
      stdpal_bin[49],stdpal_bin[50],real_mode);
   cdi.cdi_vbpps    = vbpps;
   cdi.cdi_FontMult = fnt_mult;

   // set text mode palette in 8 bit mode
   if (modes[real_mode]->bits==8) {
      u8t pal[768];
      memset(pal+48, 0, 768-48);
      // text mode colors
      for (ii=0; ii<48; ii++) pal[ii] = stdpal_bin[ii]>>2;
      // cursor color
      for (ii=0; ii<3; ii++) pal[CURSOR_COLOR_IDX*3+ii] = stdpal_bin[48+ii]>>2;
      con_setvgapal(pal);
   }
   // set shape call to draw correct cursor
   evio_setshape(VIO_SHAPE_LINE);
   {
      modeinfo *mi = modes[real_mode];

      log_it(3, "%dx%d (%dx%d) font:%08X top:%u,%u bpp:%d shadow: %08X,%d phys: %08X,%d x%u\n",
         m_x, m_y, mi->width, mi->height, fnt_data, left_ofs, top_ofs, vbpps,
            mi->shadow, mi->shadowpitch, mi->physmap, mi->mempitch, fnt_mult);
   }
}

void evio_shutdown() {
   if (fnt_data) { free(fnt_data); fnt_data = 0; }
}

void _std con_fontupdate(void) {
   mt_swlock();
   if (fnt_data && fnt_x && fnt_y) {
      u32t *nfnt = con_buildfont(fnt_x, fnt_y);

      if (nfnt) {
         free(fnt_data);
         fnt_data = nfnt;
         log_it(3, "mode font changed (%ux%u)\n", fnt_x, fnt_y);
      }
   }
   mt_swunlock();
}

static qserr _exicc gc_setmode(EXI_DATA, u32t cols, u32t lines, void *extmem) {
   if (extmem) return E_SYS_INVPARM;
   if (setmode_locked) return E_CON_DETACHED;
/*
   if (cols==80 && (lines==25 || lines==43 || lines==50)) {
      cvio->vh_setmode(lines);
      in_native = 1;
   } else*/ {
      if (!con_setmode(cols, lines, 0)) return E_CON_MODERR;
   }
   return 0;
}

static qserr _exicc gc_setmodeid(EXI_DATA, u16t modeid) {
   if (setmode_locked) return E_CON_DETACHED;
   if (--modeid>=mode_cnt) return E_CON_BADMODEID;
   return pl_setmodeid(modeid)?0:E_CON_MODERR;
}

static qserr _exicc gc_reset(EXI_DATA) {
   u32t mx, my;
   if (setmode_locked) return E_CON_DETACHED;
   con_unsetmode();
   cvio->vh_resetmode();
   cvio->vh_getmfast(&mx, &my);
   current_mode = search_mode(mx, my, 0);
   real_mode    = current_mode;
   in_native    = 1;
   return 0;
}

static qserr _exicc gc_getmode(EXI_DATA, u32t *cols, u32t *lines, u16t *modeid) {
   if (in_native) return vh_vio->getmode(cols, lines, modeid);
   if (cols)  *cols  = m_x;
   if (lines) *lines = m_y;
   if (modeid) *modeid = current_mode+1;
   return 0;
}

static void  _exicc gc_setcolor(EXI_DATA, u16t color) {
   if (in_native) cvio->vh_setcolor(color); else {
      txcolor = color&0xFF;
   }
}

static void _exicc gc_clear(EXI_DATA) {
   if (in_native) cvio->vh_clearscr(); else {
      modeinfo *mi = modes[current_mode];
      // fill screen
      pl_clear(real_mode, 0, 0, modes[real_mode]->width, modes[real_mode]->height,
               m_color[txcolor>>4]);
      // fill text buffer
      memsetw((u16t*)mi->shadow, (u16t)txcolor<<8|0x20, m_x*m_y);
      // cursor pos
      pos_x = 0;
      pos_y = 0;
      if (cdi.cdi_CurMask) updatechar(0,0);
   }
}

static void  _exicc gc_setpos(EXI_DATA, u32t line, u32t col) {
   if (in_native) cvio->vh_setpos(line, col); else {
      u32t opx = pos_x,
           opy = pos_y;
      
      if ((pos_y=line)>m_y) pos_y = m_y-1;
      if ((pos_x=col )>m_x) pos_x = m_x-1;
      // update cursor view
      if (cdi.cdi_CurMask && (opx!=pos_x || opy!=pos_y)) {
         updatechar(opx, opy);
         updatechar(pos_x, pos_y);
      }
      tty_lines += pos_y-opy;
   }
}

static u32t _exicc gc_info(EXI_DATA, vio_mode_info **mout, char *prnname) {
   if (mout) {
      u32t ii, idx;
      //heap block, owned by THIS module
      *mout = (vio_mode_info*)malloc(sizeof(vio_mode_info)*mode_cnt + 4);
      mem_zero(*mout);

      for (ii=0, idx=0; ii<mode_cnt; ii++) {
         modeinfo  *ci = modes[ii];

         if ((ci->flags&CON_GRAPHMODE)==0) {
            vio_mode_info *mi = *mout+idx;

            mi->size    = VIO_MI_FULL;
            mi->mx      = ci->width;
            mi->my      = ci->height;
            mi->fontx   = ci->font_x;
            mi->fonty   = ci->font_y;
            mi->mode_id = ii+1;
            if (ci->flags&CON_EMULATED) {
               ci = modes[ci->realmode];
               mi->gmx     = ci->width;
               mi->gmy     = ci->height;
               mi->gmbits  = ci->bits;
               mi->rmask   = ci->rmask;
               mi->gmask   = ci->gmask;
               mi->bmask   = ci->bmask;
               mi->amask   = ci->amask;
               mi->gmpitch = ci->mempitch;
               mi->flags   = VMF_GRAPHMODE|(ci->bits==8?VMF_VGAPALETTE:0)|
                             (ci->flags&CON_FONTxMASK)>>CON2VMF_FSCALE;
            }
            idx++;
         }
      }
      // realloc output buffer to smaller size
      if (idx<mode_cnt)
         *mout = (vio_mode_info*)realloc(*mout, sizeof(vio_mode_info)*idx + 4);
   }
   // show self as VIO too
   if (prnname) strcpy(prnname, "vio");
   /* always use SLOW here, this will slow down a bit VGA text modes, but
      speed up both graphic console & native EFI console */
   return VHI_MEMORY|VHI_BLINK|VHI_SLOW;
}

static qserr _exicc gc_state(EXI_DATA, vh_settings *sb) {
   if (!sb) return E_SYS_ZEROPTR;

   if (in_native) return vh_vio->state(sb); else {
      // fill values
      sb->color    = txcolor;
      sb->shape    = txshape;
      sb->px       = pos_x;
      sb->py       = pos_y;
      sb->opts     = vh_opts;
      sb->ttylines = tty_lines;
   }
   return 0;
}

static u16t* _exicc gc_memory(EXI_DATA) { return 0; }
static qs_gh _exicc gc_graphic(EXI_DATA) { return 0; }

static u32t _exicc gc_charout(EXI_DATA, char ch) {
   if (in_native) return cvio->vh_charout(ch);
   return charout_common(ch,0);
}

static u32t _exicc gc_strout(EXI_DATA, const char *str) {
   if (!str) return 0;
   if (in_native) return cvio->vh_strout(str); else {
      u32t rc = 0;
      while (*str) rc+=charout_common(*str++,1);
      // update cursor after string output without cursor changes
      if (cdi.cdi_CurMask) updatechar(pos_x, pos_y);
      return rc;
   }
}

static qserr _exicc gc_writebuf(EXI_DATA, u32t col, u32t line, u32t width,
                                u32t height, void *buf, u32t pitch)
{
   if (!width || !height) return E_SYS_INVPARM;
   if (!buf) return E_SYS_ZEROPTR;
   if (in_native) cvio->vh_writebuf(col, line, width, height, buf, pitch); else {
      if (line>=m_y || col>=m_x) return E_SYS_INVPARM;
      if (!pitch) pitch = width*2;
      if (col+width   > m_x) width  = m_x - col;
      if (line+height > m_y) height = m_y - line;
      
      if (width && height) {
         modeinfo *mi = modes[current_mode],
                  *ri = modes[real_mode];
         u8t    *bscr = mi->shadow + (line*m_x + col)*2,
                *bptr = (u8t*)buf;
         u32t  ln, ii;
         for (ln=0; ln<height; ln++) {
            memcpy(bscr, bptr, width*2);
            // copy characters to screen
            for (ii=0; ii<width; ii++) writechar(col+ii, line+ln, bptr[ii<<1], bptr[(ii<<1)+1]);
            bscr+=m_x*2; bptr+=pitch;
         }
      }
   }
   return 0;
}

static qserr _exicc gc_readbuf(EXI_DATA, u32t col, u32t line, u32t width,
                               u32t height, void *buf, u32t pitch)
{
   /* current session handling code ignores this function (it uses cache
      buffer`s one instead) */
   return E_SYS_UNSUPPORTED;
}

static void _exicc gc_fillrect(EXI_DATA, u32t col, u32t line, u32t width,
                               u32t height, char ch, u16t color)
{
   u16t  *vb;
   if (!width || !height) return;
   if (!ch) ch = ' ';
   // alloc dynamic buffer
   vb = (u16t*)malloc_thread(width*height*2);
   memsetw(vb, color<<8|ch, width*height);
   if (in_native) cvio->vh_writebuf(col, line, width, height, vb, width<<1); else
      gc_writebuf(0, 0, col, line, width, height, vb, width<<1);
   free(vb);
}

static void *gc_methods_list[] = { gc_init, gc_link, gc_graphic, gc_info,
   gc_setmode, gc_setmodeid, gc_reset, gc_clear, gc_getmode, gc_setcolor,
   gc_setpos, gc_setopts, gc_state, gc_charout, gc_strout, gc_writebuf,
   gc_readbuf, gc_memory, gc_fillrect };
   
static void _std gc_initialize(void *instance, void *data) {}
static void _std gc_finalize(void *instance, void *data) {}

int con_catchvio(void) {
   if (sizeof(gc_methods_list)!=sizeof(_qs_vh))
      { log_it(1, "interface mismatch!\n"); return 0; }
   /* register class as EXIC_EXCLUSIVE - this mean single possible instance
      per system */
   gc_classid = exi_register("qs_vh_gc", gc_methods_list, sizeof(gc_methods_list)/
      sizeof(void*), 0, EXIC_EXCLUSIVE, gc_initialize, gc_finalize, 0);
   if (!gc_classid) { log_it(1, "class registration error!\n"); return 0; }

   fnt_data  = 0;
   in_native = 1;
   self      = (qs_vh)exi_createid(gc_classid, EXIF_SHARED);
   if (self) {
      qserr     err;
      se_stats *fse = se_stat(se_foreground());
      // try to make it valid before get/setmode calls from se_deviceswap()
      if (fse) {
         real_mode = current_mode = search_mode(fse->mx, fse->my, 0);
         free(fse);
      }
      err = se_deviceswap(0, self, &vh_vio);
      if (err) {
         log_it(1, "vh init error %05X\n", err);
         DELETE(self);
         self = 0;
      } else
         keyboard = vh_vio->input();
   } else 
      log_it(1, "createid error!\n");

   return self?1:0;
}

int con_releasevio(void) {
   if (fnt_data) { free(fnt_data); fnt_data = 0; }

   if (self) {
      qs_vh tmp;
      /* function will deny device swap if we still have graphic console
         modes in some sessions (i.e. new device has no it in its mode list) */
      qserr err = se_deviceswap(0, vh_vio, &tmp);
      if (err) {
         log_it(1, "device swap error %05X\n", err);
         return 0;
      }
      DELETE(self);
      keyboard = 0;
      vh_vio   = 0;
      self     = 0;
   }
   // error should never occur, because class has EXIC_EXCLUSIVE flag
   if (!exi_unregister(gc_classid)) {
      log_it(1, "unable to unregister vh class!\n");
      return 0;
   }
   gc_classid = 0;
   return 1;
}
