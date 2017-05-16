#include "stdlib.h"
#include "conint.h"
#include "qsmodext.h"
#include "qsinit_ord.h"
#include "console.h"
#include "qsutil.h"
#include "qstask.h"
#include "vio.h"

static u32t       *fnt_data = 0,
                   m_x, m_y,       // mode size
               fnt_x, fnt_y,       // mode font size
               pos_x, pos_y,       // cursor position
                    top_ofs,       // offset of 0,0 from top of screen, pixels
                      vbpps,       // bytes per pixel in active mode
                 evio_hooks = 0;   // hooks on

static int          colored = 0;   // colored output
static u8t          txcolor = 7;
static u16t         txshape = 0;

extern u8t       stdpal_bin[51];   // text mode RGB colors
static u32t         m_color[16];

static con_drawinfo     cdi;

extern u32t* _std vio_ttylines();

static void writechar(u32t x, u32t y, char ch, u8t color) {
   con_modeinfo *mi = modes + real_mode;
   u32t        ypos = top_ofs+y*fnt_y,
               xpos = x*fnt_x*vbpps,
            *chdata = fnt_data + (u8t)ch * fnt_y,
              bgcol = m_color[color>>4],
              txcol = m_color[color&0xF];
   int       cursor = x==pos_x && y==pos_y;

   if (mi->shadow) {
      u32t llen = mi->shadowpitch;
      con_drawchar(&cdi, txcol, bgcol, chdata, mi->shadow + ypos*llen + xpos, 
         llen, cursor);
   }
   if (mi->physmap) {
      u32t llen = mi->mempitch;
      con_drawchar(&cdi, txcol, bgcol, chdata, mi->physmap + ypos*llen + xpos,
         llen, cursor);
   } else
      pl_flush(real_mode, x*fnt_x, ypos, fnt_x, fnt_y);
}

static void updatechar(u32t x, u32t y) {
   con_modeinfo *mi = modes + current_mode;
   u16t      chattr = ((u16t*)mi->shadow)[m_x*y+x];
   writechar(x, y, chattr, chattr>>8);
}

static u32t charout_common(char ch, int in_seq) {
   int  lines = 0;
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
         ((u16t*)modes[current_mode].shadow)[m_x*pos_y + pos_x++] = (u16t)txcolor<<8|ch;
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
         con_modeinfo *mi = modes + current_mode;
         u32t     linesm1 = fnt_y*(m_y-1),
                   tx_ofs = m_x*(m_y-1)*2;
         pl_scroll(real_mode, top_ofs+fnt_y, top_ofs, linesm1);
         pos_y--;
         // clear last line in shadow buffer and on screen
         pl_clear(real_mode, 0, top_ofs+linesm1, 0, fnt_y, m_color[txcolor>>4]);
         // update text mode buffer
         memmove(mi->shadow, mi->shadow + m_x*2, tx_ofs);
         memsetw((u16t*)(mi->shadow + tx_ofs), (u16t)txcolor<<8|0x20, m_x);
      }
   }
   // update cursor
   if (!in_seq && cdi.cdi_CurMask) updatechar(pos_x, pos_y);
   // update global line counter
   *vio_ttylines() += lines;

   return lines;
}

u32t _std evio_charout(char ch) {
   if (se_sesno()==SESN_DETACHED) return 0;
   return charout_common(ch,0);
}

u32t _std evio_strout(const char *str) {
   if (se_sesno()==SESN_DETACHED) return 0; else {
      u32t rc = 0;
      while (*str) rc+=charout_common(*str++,1);
      // update cursor after string output without cursor changes
      if (cdi.cdi_CurMask) updatechar(pos_x, pos_y);
      return rc;
   }
}

void _std evio_clearscr(void) {
   if (se_sesno()!=SESN_DETACHED) {
      con_modeinfo *mi = modes + current_mode;
      // fill screen
      pl_clear(real_mode, 0, top_ofs, 0, fnt_y*m_y, m_color[txcolor>>4]);
      // fill text buffer
      memsetw((u16t*)mi->shadow, (u16t)txcolor<<8|0x20, m_x*m_y);
      // cursor pos
      pos_x = 0;
      pos_y = 0;
      if (cdi.cdi_CurMask) updatechar(0,0);
   }
}

void _std evio_setpos(u8t line, u8t col) {
   if (se_sesno()!=SESN_DETACHED) {
      u32t opx = pos_x,
           opy = pos_y;
      
      if ((pos_y=line)>m_y) pos_y = m_y-1;
      if ((pos_x=col )>m_x) pos_x = m_x-1;
      // update cursor view
      if (cdi.cdi_CurMask && (opx!=pos_x || opy!=pos_y)) {
         updatechar(opx, opy);
         updatechar(pos_x, pos_y);
      }
      // fix global line counter
      *vio_ttylines() += pos_y-opy;
   }
}

void _std evio_getpos(u32t *line, u32t *col) {
   if (se_sesno()==SESN_DETACHED) {
      if (line) *line = 0;
      if (col)  *col  = 0;
   } else {
      if (line) *line = pos_y;
      if (col)  *col  = pos_x;
   }
}

static void evio_bufcommon(u32t col, u32t line, u32t width, u32t height,
   void *buf, u32t pitch, int tosrc)
{
   if (se_sesno()==SESN_DETACHED) return;
   if (line>=m_y||col>=m_x) return;
   if (!pitch) pitch = width*2;
   if (col+width   > m_x) width  = m_x - col;
   if (line+height > m_y) height = m_y - line;

   if (width && height) {
      con_modeinfo *mi = modes + current_mode,
                   *ri = modes + real_mode;
      u8t *bscr = mi->shadow + (line*m_x + col)*2,
          *bptr = (u8t*)buf;
      u32t   ln, ii;

      for (ln=0; ln<height; ln++) {
         memcpy(tosrc?bscr:bptr,tosrc?bptr:bscr,width*2);
         // copy characters to screen
         if (tosrc)
            for (ii=0; ii<width; ii++)
               writechar(col+ii, line+ln, bptr[ii<<1], bptr[(ii<<1)+1]);
         bscr+=m_x*2; bptr+=pitch;
      }
   }
}

void _std evio_writebuf(u32t col, u32t line, u32t width, u32t height, 
   void *buf, u32t pitch) 
{
   evio_bufcommon(col, line, width, height, buf, pitch, 1);
}

void _std evio_readbuf (u32t col, u32t line, u32t width, u32t height, 
   void *buf, u32t pitch)
{
   evio_bufcommon(col, line, width, height, buf, pitch, 0);
}

void _std evio_setshape(u8t start, u8t end) {
   if (se_sesno()==SESN_DETACHED) return;
   txshape = (u16t)end<<8|start;
   cdi.cdi_CurMask = 0; 
   if ((end&0x20)==0) {
      u8t ii;
      if (start>fnt_y-1) start = fnt_y-1;
      if (end  >fnt_y-1) end   = fnt_y-1;
      while (start<=end) cdi.cdi_CurMask |= 1<<fnt_y-1-start++;
   }
   updatechar(pos_x, pos_y);
}

u16t _std evio_getshape(void) {
   if (se_sesno()==SESN_DETACHED) return VIO_SHAPE_NONE;
   return txshape;
}

void _std evio_setcolor(u16t color) {
   if (se_sesno()==SESN_DETACHED) return;
   colored = color&0x100?0:1;
   txcolor = color&0xFF;
}

u8t  _std evio_getmode(u32t *cols, u32t *lines) {
   if (se_sesno()==SESN_DETACHED) {
      if (cols) *cols = 80;
      if (lines) *lines = 25;
   } else {
      if (cols)  *cols  = m_x;
      if (lines) *lines = m_y;
   }
   return 1;
}

void _std evio_defshape(u8t shape) {
   u8t bs,be;
   if (se_sesno()==SESN_DETACHED) return;
   switch (shape) {
      case VIO_SHAPE_FULL: bs=0; be=fnt_y-1; break;
      case VIO_SHAPE_HALF: be=fnt_y-1; bs=be>>1; break;
      case VIO_SHAPE_WIDE: bs=fnt_y-2; be=fnt_y-1; break;
      case VIO_SHAPE_NONE: bs=0; be=0x20; break;
      default:
         bs=be=fnt_y-1;
   }
   evio_setshape(bs, be);
}

void _std evio_intensity(u8t value) {
}

static u32t hicolor_cvt(u8t rc, u8t gc, u8t bc, u32t mode) {
   u32t res = 0,
       bits = modes[mode].bits;
   // fix 16 to 15 bits
   if (bits==16 && modes[mode].rmask==0x7C00 && modes[mode].gmask==0x3E0) bits=15;

   switch (bits) {
      case 15: res = bc>>3 | gc>>3<<5 | rc>>3<<10; break;
      case 16: res = bc>>3 | gc>>2<<5 | rc>>3<<11; break;
      case 24:
      case 32: res = bc | gc<<8 | rc<<16; break;
   }
   return res;
}

/* some of 4xxx ATI cards can`t change palette at all, so select orange index
   in default palette to make simular view */
#define CURSOR_COLOR_IDX   42

static u16t     replace_ord[] = { ORD_QSINIT_vio_getpos, 
   ORD_QSINIT_vio_getmodefast,    ORD_QSINIT_vio_defshape,
   ORD_QSINIT_vio_intensity,      ORD_QSINIT_vio_charout,
   ORD_QSINIT_vio_strout,         ORD_QSINIT_vio_clearscr,
   ORD_QSINIT_vio_setpos,         ORD_QSINIT_vio_setshape,
   ORD_QSINIT_vio_setcolor,       ORD_QSINIT_vio_getmode,
   ORD_QSINIT_vio_getshape,       ORD_QSINIT_vio_writebuf,
   ORD_QSINIT_vio_readbuf,        0 };

static void    *replace_ptr[] = { evio_getpos,
   evio_getmode,                  evio_defshape,
   evio_intensity,                evio_charout,
   evio_strout,                   evio_clearscr,
   evio_setpos,                   evio_setshape,
   evio_setcolor,                 evio_getmode,
   evio_getshape,                 evio_writebuf,
   evio_readbuf };

void evio_newmode() {
   con_modeinfo *mi = modes + current_mode;
   u32t  ii;
   if (fnt_data) free(fnt_data);

   fnt_x    = mi->font_x; 
   fnt_y    = mi->font_y;
   fnt_data = con_buildfont(fnt_x, fnt_y);
   m_x      = mi->width;
   m_y      = mi->height;
   pos_x    = 0;
   pos_y    = 0;

   // current mode info
   top_ofs  = modes[real_mode].height - m_y*fnt_y >> 1;
   vbpps    = BytesBP(modes[real_mode].bits);
   colored  = 0;
   txcolor  = 7;
   evio_defshape(VIO_SHAPE_LINE);

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

   // set text mode palette in 8 bit mode
   if (modes[real_mode].bits==8) {
      u8t pal[768];
      memset(pal+48, 0, 768-48);
      // text mode colors
      for (ii=0; ii<48; ii++) pal[ii] = stdpal_bin[ii]>>2;
      // cursor color
      for (ii=0; ii<3; ii++) pal[CURSOR_COLOR_IDX*3+ii] = stdpal_bin[48+ii]>>2;
      con_setvgapal(pal);
   }

   if (!evio_hooks) {
      u32t qsh = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsh) {
         ii = 0;
         while (replace_ord[ii]) {
            mod_apichain(qsh, replace_ord[ii], APICN_REPLACE, replace_ptr[ii]);
            ii++;
         }
         evio_hooks = 1;
      }
   }
   {
      con_modeinfo *mi = modes + real_mode;

      log_it(3, "%dx%d (%dx%d) font:%08X top:%d, bpp:%d, shadow: %08X,%d phys: %08X,%d\n",
         m_x, m_y, mi->width, mi->height, fnt_data, top_ofs, vbpps, mi->shadow,
            mi->shadowpitch, mi->physmap, mi->mempitch);
   }
}

void evio_shutdown() {
   if (fnt_data) { free(fnt_data); fnt_data = 0; }

   if (evio_hooks) {
      u32t qsh = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsh) {
         u32t  ii = 0;
         while (replace_ord[ii]) {
            mod_apiunchain(qsh, replace_ord[ii], APICN_REPLACE, replace_ptr[ii]);
            ii++;
         }
         evio_hooks = 0;
      }
   }
}
