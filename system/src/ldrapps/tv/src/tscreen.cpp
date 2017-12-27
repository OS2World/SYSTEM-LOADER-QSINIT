/*------------------------------------------------------------*/
/* filename -       tscreen.cpp                               */
/*                                                            */
/* function(s)                                                */
/*                  TScreen member functions                  */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TScreen
#define Uses_TEvent
#define Uses_TThreaded
#include <tv.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef __MSDOS__
#include <dos.h>
ushort *TDisplay::equipment = (ushort *)MK_FP(0, 0x410);
uchar   *TDisplay::crtInfo = (uchar *)MK_FP(0, 0x487);
uchar   *TDisplay::crtRows = (uchar *)MK_FP(0, 0x484);
#endif

#ifdef __QSINIT__
#include <qsutil.h>
#include <vioext.h>
#endif

ushort TScreen::startupMode    = 0xFFFF;
ushort TScreen::startupCursor  = 0;
#ifdef __MSDOS__
ushort  TScreen::screenMode    = 0;
#else
ushort  TScreen::screenMode    = (80 << 8) | 25;
#endif
int     TScreen::screenWidth   = 0;
int     TScreen::screenHeight  = 0;
Boolean TScreen::hiResScreen   = False;
Boolean TScreen::checkSnow     = False;
uchar   *TScreen::screenBuffer = 0;
ushort  TScreen::cursorLines   = 0;
Boolean TScreen::clearOnSuspend = True;

//--------------------------------------------------------------------------
#define maxViewHeight 100

static void checksize(int height, int width) {
   if (height > maxViewHeight) {
#ifdef __QSINIT__
      char emsg[128];
      snprintf(emsg, 128, "The window is too high (max %u rows)!\n", maxViewHeight);
      vio_msgbox("Fatal Error!", emsg, MSG_RED|MSG_OK, 0);
#else
      fprintf(stderr,
              "\n\n\nFatal error: the window is too high (max %d rows)!\n", maxViewHeight);
#endif
      _exit(0);
   }
   if (width > maxViewWidth) {
#ifdef __QSINIT__
      char emsg[128];
      snprintf(emsg, 128, "The window is too wide (max %u columns)!\n", maxViewWidth);
      vio_msgbox("Fatal Error!", emsg, MSG_RED|MSG_OK, 0);
#else
      fprintf(stderr,
              "\n\n\nFatal error: the window is too wide (max %d columns)!\n", maxViewWidth);
#endif
      _exit(0);
   }
}

//--------------------------------------------------------------------------

#ifdef __MSDOS__
void TScreen::resume() {
   startupMode = getCrtMode();
   startupCursor = getCursorType();
   if (screenMode != startupMode)
      setCrtMode(screenMode);
   setCrtData();
}

TScreen::~TScreen() {
   suspend();
}
#endif // __MSDOS__

//--------------------------------------------------------------------------

#ifdef __DOS16__

ushort TDisplay::getCursorType() {
   _AH = 3;
   _BH = 0;
   videoInt();
   return _CX;
}

void TDisplay::setCursorType(ushort ct) {
   _AH = 1;
   _CX = ct;
   videoInt();
}

void TDisplay::clearScreen(int w, int h) {
   _BH = 0x07;
   _CX = 0;
   _DL = uchar(w);
   _DH = uchar(h - 1);
   _AX = 0x0600;
   videoInt();
   _AX = 0x1003;
   _BX = 0x0000;               // don't blink, just make intense
   videoInt();
}

void TDisplay::videoInt() {
   asm {
      PUSH    BP
      PUSH    ES
      INT     10h
      POP     ES
      POP     BP
   }
}

ushort TDisplay::getRows() {
   _AX = 0x1130;
   _BH = 0;
   _DL = 0;
   videoInt();
   if (_DL == 0)
      _DL = 24;
   return _DL + 1;
}

ushort TDisplay::getCols() {
   _AH = 0x0F;
   videoInt();
   return _AH;
}

ushort TDisplay::getCrtMode() {
   _AH = 0x0F;
   videoInt();
   ushort mode = _AL;
#ifndef __NOROW__
   if (getRows() > 25)
      mode |= smFont8x8;
#endif
   return mode;
}


void TDisplay::setCrtMode(ushort mode) {
   *equipment &= 0xFFCF;
   *equipment |= (mode == smMono) ? 0x30 : 0x20;
   *crtInfo &= 0x00FE;

   _AH = 0;
   _AL = mode;
   videoInt();

#ifndef __NOROW__
   if ((mode & smFont8x8) != 0) {
      _AX = 0x1112;
      _BL = 0;
      videoInt();

      if (getRows() > 25) {
         *crtInfo |= 1;

         _AH = 1;
         _CX = 0x0607;
         videoInt();

         _AH = 0x12;
         _BL = 0x20;
         videoInt();
      }
   }
#endif

}

ushort TScreen::fixCrtMode(ushort mode) {
   _AX = mode;
#ifndef __NOROW__
   if (_AL != smMono && _AL != smCO80 && _AL != smBW80)
      _AL = smCO80;
#endif
   return _AX;
}

void TScreen::setCrtData() {
   screenMode = getCrtMode();
   screenWidth = getCols();
   screenHeight = getRows();
   checksize(screenHeight, screenWidth);
   hiResScreen = Boolean(screenHeight > 25);

   if (screenMode == smMono) {
      screenBuffer = (uchar *)MK_FP(0xB000, 0);
      checkSnow = False;
   } else {
      screenBuffer = (uchar *)MK_FP(0xB800, 0);
      if (hiResScreen) checkSnow = False;
   }

   cursorLines = getCursorType();
   setCursorType(0x2000);

}

#endif // __DOS16_
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#ifdef __DOS32__

ushort TDisplay::getCursorType() {
   REGS r;
   r.h.ah = 3;
   r.h.bh = 0;
   int386(0x10,&r,&r);
   return r.w.cx;
}

void TDisplay::setCursorType(ushort ct) {
   REGS r;
   r.h.ah = 1;
   r.w.cx = ct;
   int386(0x10,&r,&r);
}

void TDisplay::clearScreen(int w, int h) {
   REGS r;
   r.w.ax = 0x0600;
   r.h.bh = 0x07;
   r.w.cx = 0;
   r.h.dl = uchar(w);
   r.h.dh = uchar(h - 1);
   int386(0x10,&r,&r);
   r.h.ah = 0x10;
   r.h.al = 0x03;
   r.h.bl = 0x00;        // don't blink, just make intense
   int386(0x10,&r,&r);
}

ushort TDisplay::getRows() {
   REGS r;
   r.w.ax = 0x1130;
   r.h.bh = 0x0;
   r.h.dl = 0x0;
   int386(0x10,&r,&r);
   if (r.h.dl == 0) r.h.dl = 24;
   return r.h.dl + 1;
}

ushort TDisplay::getCols() {
   REGS r;
   r.h.ah = 0x0F;
   int386(0x10,&r,&r);
   return r.h.ah;
}

ushort TDisplay::getCrtMode() {
   REGS r;
   r.h.ah = 0x0F;
   int386(0x10,&r,&r);
   ushort mode = r.h.al;
#ifndef __NOROW__
   if (getRows() > 25)
      mode |= smFont8x8;
#endif
   return mode;
}


void TDisplay::setCrtMode(ushort mode) {
   *equipment &= 0xFFCF;
   *equipment |= (mode == smMono) ? 0x30 : 0x20;
   *crtInfo &= 0x00FE;

   REGS r;
   r.h.ah = 0x0;
   r.h.al = mode;
   int386(0x10,&r,&r);

#ifndef __NOROW__
   if ((mode & smFont8x8) != 0) {
      r.w.ax = 0x1112;
      r.h.bl = 0;
      int386(0x10,&r,&r);

      if (getRows() > 25) {
         *crtInfo |= 1;

         r.h.ah = 1;
         r.w.cx = 0x0607;
         int386(0x10,&r,&r);

         r.h.ah = 0x12;
         r.h.bl = 0x20;
         int386(0x10,&r,&r);
      }
   }
#endif // __NOROW__
}

ushort TScreen::fixCrtMode(ushort mode) {
#ifdef __NOROW__
   return mode;
#else
   char m = mode;
   if (m != smMono && m != smCO80 && m != smBW80)
      m = smCO80;
   return (mode & ~0xFF) + m;
#endif // __NOROW__
}

void TScreen::setCrtData() {
   screenMode = getCrtMode();
   screenWidth = getCols();
   screenHeight = getRows();
   checksize(screenHeight, screenWidth);
   hiResScreen = Boolean(screenHeight > 25);

   if (screenMode == smMono) {
      screenBuffer = (uchar *)MK_FP(0, 0xB0000);
      checkSnow = False;
   } else {
      screenBuffer = (uchar *)MK_FP(0, 0xB8000);
      if (hiResScreen)
         checkSnow = False;
   }

   cursorLines = getCursorType();
   setCursorType(0x2000);

}

#endif // __DOS32__
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#ifdef __OS2__

#if defined(__IBMCPP__)||defined(__WATCOMC__)
#define _far16 _Seg16
#endif

ushort TDisplay::getCursorType() {
   VIOCURSORINFO cinfo;
   VioGetCurType(&cinfo,NULL);
   return (cinfo.yStart << 8) + cinfo.cEnd;
}

void TDisplay::setCursorType(ushort ct) {
   VIOCURSORINFO cinfo;
   VioGetCurType(&cinfo,NULL);
   if ((ct>>8) == 0x20) {
      cinfo.attr   = 0xFFFF;          // hide
   } else {
      cinfo.attr   = 0;               // show
      cinfo.yStart = uchar(ct>>8);
      cinfo.cEnd   = uchar(ct);
      cinfo.cx     = 0;
   }
   VioSetCurType(&cinfo,NULL);
}

#if defined(__WATCOMC__) && (__WATCOMC__>1270)
static unsigned char __far16 empty_scroll[] = { ' ', 15, 0 };
static unsigned char __far16 *pescroll = &empty_scroll[0];
#endif

void TDisplay::clearScreen(int w, int h) {
#if defined(__WATCOMC__) && (__WATCOMC__>1270)
   VioScrollUp(0,0,h,w,h,pescroll,NULL);
#else
   char *cell = " \x0F";
   VioScrollUp(0,0,h,w,h,cell,NULL);
#endif
}

ushort TDisplay::getRows() {
   VIOMODEINFO info;
   info.cb=sizeof(VIOMODEINFO);
   VioGetMode(&info,0);
   return info.row;
}

ushort TDisplay::getCols() {
   VIOMODEINFO info;
   info.cb=sizeof(VIOMODEINFO);
   VioGetMode(&info,0);
   return info.col;
}

ushort TDisplay::getCrtMode() {
   return getRows() + (getCols() << 8);
}

void TDisplay::setCrtMode(ushort mode) {
   VIOMODEINFO info;
   info.cb = sizeof(VIOMODEINFO);
   VioGetMode(&info,0);
   info.cb = 8;
   info.col = uchar(mode >> 8);
   info.row = uchar(mode);
   checksize(info.row, info.col);
   VioSetMode(&info,0);
   clearScreen(info.col,info.row);
}

void TScreen::resume() {
   // dixie  ??? is it correct for Dos Shell????
   //    if (screenMode != startupMode)
   //       setCrtMode( screenMode );
   setCrtData();
}

TScreen::~TScreen() {
   // dixie
   VIOMODEINFO info;
   info.cb = sizeof(VIOMODEINFO);
   VioGetMode(&info,0);
#if defined(__WATCOMC__) && (__WATCOMC__>1270)
   VioScrollUp(0,0,info.row,info.col,info.row,pescroll,NULL);
#else
   char *cell = " \x0F";
   VioScrollUp(0,0,info.row,info.col,info.row,cell,NULL);
#endif
}

ushort TScreen::fixCrtMode(ushort mode) {
   if (char(mode) == 0 || char(mode>>8) == 0) return 0;
   return mode;
}

void TScreen::setCrtData() {
   screenMode = getCrtMode();
   screenWidth = getCols();
   screenHeight = getRows();
   checksize(screenHeight, screenWidth);
   hiResScreen = Boolean(screenHeight > 25);

   uchar *_far16 s;
   ushort l;
   VioGetBuf((PULONG) &s, &l, 0);   // !!! OS/2 specific
   screenBuffer = s; // Automatic conversion

   checkSnow = False;
   cursorLines = getCursorType();
   setCursorType(0x2000);

}

#endif  // __OS2__

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#ifdef __NT__

ushort TDisplay::getCursorType() {
   GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&TThreads::crInfo);
   int ct = TThreads::crInfo.bVisible
            ? TThreads::crInfo.dwSize*31/100
            : 0x2000;
   return ct;
}

void TDisplay::setCursorType(ushort ct) {
   if ((ct>>8) == 0x20) {
      TThreads::crInfo.bVisible = False;
      TThreads::crInfo.dwSize = 1;
   } else {
      TThreads::crInfo.bVisible = True;
      TThreads::crInfo.dwSize = (uchar(ct)-uchar(ct>>8))*100/31;
   }
   SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&TThreads::crInfo);
}

void TDisplay::clearScreen(int w, int h) {
   COORD coord = { 0, 0 };
   DWORD read;

   FillConsoleOutputAttribute(TThreads::chandle[cnOutput], 0x07, w*h, coord, &read);
   FillConsoleOutputCharacterA(TThreads::chandle[cnOutput], ' ', w*h, coord, &read);
}

ushort TDisplay::getRows() {
   GetConsoleScreenBufferInfo(TThreads::chandle[cnOutput], &TThreads::sbInfo);
   return TThreads::sbInfo.dwSize.Y;
}

ushort TDisplay::getCols() {
   GetConsoleScreenBufferInfo(TThreads::chandle[cnOutput], &TThreads::sbInfo);
   return TThreads::sbInfo.dwSize.X;
}

ushort TDisplay::getCrtMode() {
   return getRows() + (getCols() << 8);
}

void TDisplay::setCrtMode(ushort mode) {
   int oldr = getRows();
   int oldc = getCols();
   int cols = uchar(mode >> 8);
   int rows = uchar(mode);
   if (cols == 0) cols = oldc;
   if (rows == 0) rows = oldr;
   checksize(rows, cols);
   COORD newSize = { cols, rows };
   SMALL_RECT rect = { 0, 0, cols-1, rows-1 };

   //#if 0   // - don't overdo! the user asked video mode - he gets it
#if 1   // - overdo! (c) dixie
   COORD maxSize = GetLargestConsoleWindowSize(TThreads::chandle[cnOutput]);
   if (newSize.X > maxSize.X) {
      newSize.X = maxSize.X;
      rect.Right = newSize.X-1;
   }
   if (newSize.Y > maxSize.Y) {
      newSize.Y = maxSize.Y;
      rect.Bottom = newSize.Y-1;
   }
   //printf("h=%d x=%d y=%d\n",TThreads::chandle[cnOutput],newSize.X,newSize.Y); getchar();
#endif
   if (oldr <= rows) {
      if (oldc <= cols) {
         // increasing both dimensions
BUFWIN:
         SetConsoleScreenBufferSize(TThreads::chandle[cnOutput], newSize);
         SetConsoleWindowInfo(TThreads::chandle[cnOutput], True, &rect);
      } else {
         // cols--, rows+
         SMALL_RECT tmp = { 0, 0, cols-1, oldr-1 };
         SetConsoleWindowInfo(TThreads::chandle[cnOutput], True, &tmp);
         goto BUFWIN;
      }
   } else {
      if (oldc <= cols) {
         // cols+, rows--
         SMALL_RECT tmp = { 0, 0, oldc-1, rows-1 };
         SetConsoleWindowInfo(TThreads::chandle[cnOutput], True, &tmp);
         goto BUFWIN;
      } else {
         // cols--, rows--
         SetConsoleWindowInfo(TThreads::chandle[cnOutput], True, &rect);
         SetConsoleScreenBufferSize(TThreads::chandle[cnOutput], newSize);
      }
   }
   GetConsoleScreenBufferInfo(TThreads::chandle[cnOutput], &TThreads::sbInfo);
}

void TScreen::resume() {
   //  fprintf(stderr, "\nscreenMode %x startupMode %x\n", screenMode, startupMode); getchar();
   if (startupMode == 0xFFFF) screenMode=getCrtMode(); else if (screenMode != startupMode)
      setCrtMode(screenMode);
   setCrtData();
}

TScreen::~TScreen() {
   if (screenBuffer != NULL) VirtualFree(screenBuffer, 0, MEM_RELEASE);
   screenBuffer = NULL;
}

ushort TScreen::fixCrtMode(ushort mode) {
   if (char(mode) == 0 || char(mode>>8) == 0) return 0;
   return mode;
}

void TScreen::setCrtData() {
   screenMode = getCrtMode();
   screenWidth = getCols();
   screenHeight = getRows();

   if (screenWidth>maxViewWidth || screenHeight>maxViewHeight) {
      int xsz=screenWidth >maxViewWidth ?maxViewWidth :screenWidth,
          ysz=screenHeight>maxViewHeight?(screenWidth==80?25:maxViewHeight):screenHeight;
      setCrtMode(xsz<<8|ysz);
      screenMode = getCrtMode();
      screenWidth = getCols();
      screenHeight = getRows();
   }

   checksize(screenHeight, screenWidth);
   hiResScreen = Boolean(screenHeight > 25);

   short x = screenWidth;
   short y = screenHeight;
   if (x < maxViewWidth) x = maxViewWidth;
   if (y < maxViewHeight) y = maxViewHeight;    // 512*100*2 = 1024*100 = 102 400

#if 1
   if (screenBuffer == NULL)
      screenBuffer = (uchar *) VirtualAlloc(0, x * y * sizeof(ushort), MEM_COMMIT, PAGE_READWRITE);
#else
   if (screenBuffer != NULL) VirtualFree(screenBuffer, 0, MEM_RELEASE);
   screenBuffer = (uchar *) VirtualAlloc(0, x * y * sizeof(ushort), MEM_COMMIT, PAGE_READWRITE);
#endif
   if (screenBuffer == NULL) {
      fprintf(stderr, "\nFATAL: Can't allocate a screen buffer!\n");
      _exit(1);
   }

   checkSnow = False;
   cursorLines = getCursorType();
   setCursorType(0x2000);
}

#endif  // __NT__

//---------------------------------------------------------------------------
#ifdef __QSINIT__

extern "C"
void _std vio_getmodefast(u32t *cols, u32t *lines);

/* A bit incompatible way - send QSINIT`s internal constants to TV code
   and get it back to setCursorType().
   Any other platforms simulate CGA cursor bytes */
ushort TDisplay::getCursorType() { return vio_getshape(); }

void TDisplay::setCursorType(ushort ct) {
   // this should be eliminated, but who knows - just recode
   if (ct==0x2000) ct = VIO_SHAPE_NONE;
   vio_setshape(ct);
}

void TDisplay::clearScreen(int w, int h) {
   vio_clearscr();
}

ushort TDisplay::getRows() {
   u32t lines=25;
   vio_getmodefast(0,&lines);
   return lines;
}

ushort TDisplay::getCols() {
   u32t cols=80;
   vio_getmodefast(&cols,0);
   return cols;
}

ushort TDisplay::getCrtMode() {
   u32t cols=80, lines=25;
   vio_getmodefast(&cols,&lines);

   if (cols>255 || lines >255) return 0x8025; else
      return cols<<8 | lines;
}

void TDisplay::setCrtMode(ushort mode) {
   u8t cols = mode>>8,
      lines = mode;
   if (!cols) cols = 80;
   if (!lines) lines = 25;

   vio_setmodeex(cols, lines);

   //if (getRows() > 25) vio_setshape(VIO_SHAPE_LINE);
}

ushort TScreen::fixCrtMode(ushort mode) {
   if ((mode&0xFF)==0 || mode>>8==0) return 0;
   return mode;
}

void TScreen::setCrtData() {
   screenMode   = getCrtMode();
   screenWidth  = getCols();
   screenHeight = getRows();
   checksize(screenHeight, screenWidth);
   hiResScreen  = Boolean(screenHeight > 25);

   if (!screenBuffer) screenBuffer = (uchar *) hlp_memalloc(screenWidth *
      screenHeight * sizeof(ushort), QSMA_RETERR|QSMA_LOCAL);

   if (!screenBuffer) {
      printf("\nFATAL: Can't allocate a screen buffer!\n");
      _exit(1);
   }
   checkSnow    = False;
   cursorLines  = getCursorType();
   vio_setshape(VIO_SHAPE_NONE);
}

TScreen::~TScreen() {
   if (screenBuffer) { hlp_memfree(screenBuffer); screenBuffer = 0; }
   vio_clearscr();
}

void TScreen::resume() {
   //fprintf(stderr, "\nscreenMode %x startupMode %x\n", screenMode, startupMode); getchar();
   if (startupMode==0xFFFF) {
      startupCursor = getCursorType();
      startupMode = screenMode = getCrtMode();
   } else {
      startupMode = getCrtMode();
      if (screenMode!=startupMode) setCrtMode(screenMode);
   }
   setCrtData();
}

#endif // __QSINIT__
//---------------------------------------------------------------------------

TScreen::TScreen() {
#ifdef __NT__
   TThreads::resume();
#endif
   if (startupMode == 0xFFFF) {
      startupMode = getCrtMode();
      startupCursor = getCursorType();
#ifdef __QSINIT__
      log_printf("shape=%04X\n",startupCursor);
#endif
      setCrtData();
   }
}

void TScreen::suspend() {
   if (startupMode != screenMode)
      setVideoMode(startupMode);
   if (clearOnSuspend)
      clearScreen();
   setCursorType(startupCursor);
}

void TScreen::clearScreen() {
   TDisplay::clearScreen(screenWidth, screenHeight);
}

void TScreen::setVideoMode(ushort mode) {
   setCrtMode(fixCrtMode(mode));
   setCrtData();
}
