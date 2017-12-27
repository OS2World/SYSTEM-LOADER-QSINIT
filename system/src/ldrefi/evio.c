#include "qstypes.h"
#include "qsutil.h"
#include "qsint.h"
#include "clib.h"
#include "vio.h"
#include "qecall.h"

u16t text_col   = 7;
u8t  cvio_blink = 0;

extern u32t cvio_ttylines;

int  _std tolower(int cc);
void _std usleep(u32t usec);
void _std cache_ctrl(u32t action, u8t vol);

u32t _std cvio_charout(char ch) {
   return call64(EFN_VIOCHAROUT, 0, 1, ch);
}

u32t _std cvio_strout(const char *str) {
   return call64(EFN_VIOSTROUT, 0, 1, str);
}

void _std cvio_clearscr(void) {
   call64(EFN_VIOCLEARSCR, 0, 0);
}

void _std cvio_setpos(u8t line, u8t col) {
   call64(EFN_VIOSETPOS, 0, 2, line, col);
}

void _std cvio_getpos(u32t *line, u32t *col) {
   call64(EFN_VIOGETPOS, 0, 2, line, col);
}

static u8t cur_shape = VIO_SHAPE_LINE;

void _std cvio_setshape(u16t shape) {
   cur_shape = shape;
   call64(EFN_VIOSHOWCURSOR, 0, 1, shape==VIO_SHAPE_NONE?0:1);
}

u16t _std cvio_getshape(void) {
   return cur_shape;
}

void _std cvio_setcolor(u16t color) {
   text_col = color;
   call64(EFN_VIOSETCOLOR, 0, 1, color);
}

u16t _std cvio_getcolor(void) {
   return text_col;
}

void _std cvio_setmode(u32t lines) {
   call64(EFN_VIOSETMODE, 0, 1, lines);
   cvio_ttylines = 0;
}

int _std cvio_setmodeex(u32t cols, u32t lines) {
   int rc = call64(EFN_VIOSETMODEEX, 0, 2, cols, lines);
   if (rc) cvio_ttylines = 0;
   return rc;
}

u8t  _std cvio_getmode(u32t *cols, u32t *lines) {
   return call64(EFN_VIOGETMODE, 0, 2, cols, lines);
}

void _std cvio_resetmode(void) {
   u32t cols, lines;
   if (!cvio_getmode(&cols, &lines)) cols = 0;
   if (cols!=80 || lines!=25) cvio_setmode(25); else cvio_clearscr();
}

void _std cvio_getmodefast(u32t *cols, u32t *lines) {
   call64(EFN_VIOGETMODE, 0, 2, cols, lines);
}

void _std cvio_intensity(u8t value) {
   // just ignore setup because it useless on EFI
   cvio_blink = value?0:1;
}

void _std vio_beep(u16t freq, u32t ms) {
}

u8t  _std vio_beepactive(void) {
   return 0;
}

// jumped here from vio_writebuf/vio_readbuf, so order of args is constant!
void _std cvio_bufcommon(int toscr, u32t col, u32t line, u32t width,
   u32t height, void *buf, u32t pitch)
{
   u32t cols, lines;
   cvio_getmodefast(&cols,&lines);
   if (line>=lines||col>=cols) return;
   if (!pitch) pitch = width*2;
   if (col+width  > cols ) width  = cols - col;
   if (line+height> lines) height = lines- line;
   if (width && height) {
      u8t *bptr = (u8t*)buf;
      // dumb simulation :(
      u32t  svline, svcol,
           svcolor = cvio_getcolor();
      u8t     pcol = VIO_COLOR_LWHITE;
      cvio_setcolor(pcol);
      cvio_getpos(&svline, &svcol);

      for (;height>0;height--) {
         cvio_setpos(line++, col);
         if (toscr) {
            u32t wx = width;
            u8t *bx = bptr;
            // drop bottom right char to prevent screen scrolling
            if (line==lines && col+width==cols) wx--;
            // char by char, very funny
            while (wx) {
               u32t  ii = 0;
               char stb[129];
               u8t  col = bx[1];
               if (wx>2) {
                  u32t limit = wx>128?128:wx;
                  /* not sure about acceleration, but native EFI console so slow,
                     so we can`t make it worse ;) */
                  while (ii<limit) {
                     register char ch = bx[ii*2];
                     if (bx[ii*2+1]!=col||ch==8||ch==10||ch==13) break;
                        else stb[ii++] = ch;
                  }
               }
               if (col!=pcol) cvio_setcolor(pcol = col);
               if (ii>1) {
                  stb[ii] = 0;
                  cvio_strout(stb);
                  bx+=ii*2;
                  wx-=ii;
               } else {
                  register char ch = bx[0];
                  cvio_charout(ch==8||ch==10||ch==13?' ':ch);
                  bx+=2;
                  wx--;
               }
            }
         } else {
            /* do not simulate missing read, but return empty rect.
               even this call is not required now, since BVIO readbuf uses
               session`s shadow buffer to read data from */
            memsetw((u16t*)bptr, 0x0720, width);
         }
         bptr+=pitch;
      }
      cvio_setpos(svline, svcol);
      cvio_setcolor(svcolor);
   }
}

// -------------------------------------------------------------------------
// get system keys status
u32t _std ckey_status(void) {
   return call64(EFN_KEYSTATUS, 0, 0);
}
/*
u16t _std key_read_int(void) {
   return call64(EFN_KEYREAD, 0, 0);
}*/

u16t _std key_read_nw(u32t *status) {
   return call64(EFN_KEYWAIT, 0, 2, 0, status);
}

u8t  _std ckey_pressed(void) {
   return call64(EFN_KEYPRESSED, 0, 0);
}

static u8t _rate = 0, _delay = 0;

void _std key_speed(u8t rate, u8t delay) {
   mt_swlock();
   _rate  = rate  &= 0x1F;
   _delay = delay &= 3;
   // no way to set it in EFI :)
   mt_swunlock();
}

void _std key_getspeed(u8t *rate, u8t *delay) {
   mt_swlock();
   if (delay) *delay = _delay;
   if (rate )  *rate = _rate;
   mt_swunlock();
}

u8t _std ckey_push(u16t code) { return 0; }
