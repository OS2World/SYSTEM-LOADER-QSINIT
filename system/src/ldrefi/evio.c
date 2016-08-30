#include "qstypes.h"
#include "qsutil.h"
#include "qsint.h"
#include "clib.h"
#include "vio.h"
#include "qecall.h"

int  _std tolower(int cc);
void _std usleep(u32t usec);
void _std cache_ctrl(u32t action, u8t vol);

u32t _std vio_charout(char ch) {
   return call64(EFN_VIOCHAROUT, 0, 1, ch);
}

u32t _std vio_strout(const char *str) {
   return call64(EFN_VIOSTROUT, 0, 1, str);
}

void _std vio_clearscr(void) {
   call64(EFN_VIOCLEARSCR, 0, 0);
}

void _std vio_setpos(u8t line, u8t col) {
   call64(EFN_VIOSETPOS, 0, 2, line, col);
}

void _std vio_getpos(u32t *line, u32t *col) {
   call64(EFN_VIOGETPOS, 0, 2, line, col);
}

static u8t cur_start = 7, 
             cur_end = 8;

void _std vio_setshape(u8t start, u8t end) {
   call64(EFN_VIOSHOWCURSOR, 0, 1, start&0x20? 0 : 1);
}

u16t _std vio_getshape(void) {
   return (u16t)cur_start<<8 | cur_end;
}

void _std vio_defshape(u8t shape) {
   if (shape==VIO_SHAPE_NONE) vio_setshape(0x20,0); else
      vio_setshape(7,8);
}

void _std vio_setcolor(u16t color) {
   call64(EFN_VIOSETCOLOR, 0, 1, color);
}

void _std vio_setmode(u32t lines) {
   call64(EFN_VIOSETMODE, 0, 1, lines);
}

int _std vio_setmodeex(u32t cols, u32t lines) {
   return call64(EFN_VIOSETMODEEX, 0, 2, cols, lines);
}

void _std vio_resetmode(void) {
   u32t cols, lines;
   if (!vio_getmode(&cols, &lines)) cols = 0;
   if (cols!=80 || lines!=25) vio_setmode(25); else vio_clearscr();
}

u8t  _std vio_getmode(u32t *cols, u32t *lines) {
   return call64(EFN_VIOGETMODE, 0, 2, cols, lines);
}

void _std vio_getmodefast(u32t *cols, u32t *lines) {
   call64(EFN_VIOGETMODE, 0, 2, cols, lines);
}

void _std vio_intensity(u8t value) {
}

void _std vio_beep(u16t freq, u32t ms) {
}

u8t  _std vio_beepactive(void) {
   return 0;
}

// jumped here from vio_writebuf/vio_readbuf, so order of args is constant!
void _std vio_bufcommon(int toscr, u32t col, u32t line, u32t width,
   u32t height, void *buf, u32t pitch)
{
   u32t cols, lines;
   vio_getmodefast(&cols,&lines);
   if (line>=lines||col>=cols) return;
   if (!pitch) pitch = width*2;
   if (col+width  > cols ) width  = cols - col;
   if (line+height> lines) height = lines- line;
   if (width && height) {
      u8t *bptr = (u8t*)buf;
      // dumb simulation :(
      u32t  svline, svcol;
      u8t     pcol = VIO_COLOR_LWHITE;
      vio_setcolor(pcol);
      vio_getpos(&svline, &svcol);

      for (;height>0;height--) {
         vio_setpos(line++, col);
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
               if (col!=pcol) vio_setcolor(pcol = col);
               if (ii>1) {
                  stb[ii] = 0;
                  vio_strout(stb);
                  bx+=ii*2;
                  wx-=ii;
               } else {
                  register char ch = bx[0];
                  vio_charout(ch==8||ch==10||ch==13?' ':ch);
                  bx+=2;
                  wx--;
               }
            }
         } else {
            // do not simulate missing read, but return empty rect
            memsetw((u16t*)bptr, 0x0720, width);
         }
         bptr+=pitch;
      }
      vio_setpos(svline, svcol);
      vio_setcolor(VIO_COLOR_RESET);
   }
}

// -------------------------------------------------------------------------
// get system keys status
u32t _std key_status(void) {
   return call64(EFN_KEYSTATUS, 0, 0);
}

u16t _std key_read_int(void) {
   return call64(EFN_KEYREAD, 0, 0);
}

u8t  _std key_pressed(void) {
   return call64(EFN_KEYPRESSED, 0, 0);
}

static u8t _rate = 0, _delay = 0;

void _std key_speed(u8t rate, u8t delay) {
   mt_swlock();
   _rate  = rate  &= 0x1F;
   _delay = delay &= 3;
   // no way to set it in EFI :)
   //rmcall(setkeymode,2,(u32t)delay<<8|rate);
   mt_swunlock();
}

void _std key_getspeed(u8t *rate, u8t *delay) {
   mt_swlock();
   if (delay) *delay = _delay;
   if (rate )  *rate = _rate;
   mt_swunlock();
}

u8t _std key_push(u16t code) { return 0; }
