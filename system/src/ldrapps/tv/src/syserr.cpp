/*------------------------------------------------------------*/
/* filename -       syserr.cpp                                */
/*                                                            */
/* function(s)                                                */
/*          TSystemError member functions                     */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*                                                            */
/*                                                            */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*------------------------------------------------------------*/

#if defined(__BORLANDC__) && defined(__MSDOS__)
#pragma inline
#endif

#define Uses_TDrawBuffer
#define Uses_TSystemError
#define Uses_TScreen
#define Uses_TKeys
#define Uses_TThreaded
#include <tv.h>

#ifndef __QSINIT__
#ifndef __IBMCPP__
#include <dos.h>
#endif
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#endif

Boolean TSystemError::ctrlBreakHit = False;
short(* TV_CDECL TSystemError::sysErrorFunc)(short, uchar)
#ifdef __MSDOS__
#ifdef __BORLANDC__
   = (short(* TV_CDECL)(short,uchar))TSystemError::sysErr;
#else
   = TSystemError::sysErr;
#endif
#else
   = NULL;         // no error hanlding if not MS DOS
#endif

ushort  TSystemError::sysColorAttr = 0x4E4F;
ushort  TSystemError::sysMonoAttr = 0x7070;
Boolean TSystemError::saveCtrlBreak = False;
Boolean TSystemError::sysErrActive = False;
Boolean TSystemError::inIDE = False;

#ifdef __DOS16__

const SecretWord = 1495;
const productID  =  136;

static void checkIDE() {
   Int11trap trap;
   _BX = SecretWord;
   _AX = SecretWord;
   geninterrupt(0x12);
}
#endif // __DOS16__

TSystemError::TSystemError() {
#ifdef __DOS16__
   inIDE = False;
   checkIDE();
#endif // __DOS16__
   resume();
}

TSystemError::~TSystemError() {
   suspend();
}

#ifdef __DOS16__
static int getakey() {
   asm {
      MOV AH,1;
      INT 16h;
      JNZ keyWaiting;
   };
   return 0;

keyWaiting:

   asm {
      MOV AH,0;
      INT 16h;
   };
   return _AX;
}

#else // __DOS16__
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
#ifdef __DOS32__

#include <bios.h>

static int getakey() {
   if (_bios_keybrd(_KEYBRD_READY) == 0) return 0;
   return _bios_keybrd(_KEYBRD_READ);
}

typedef void __interrupt __far(*intvec_p)();
static intvec_p oldint09 = NULL;
static intvec_p oldint1B = NULL;
static intvec_p oldint23 = NULL;
static intvec_p oldint24 = NULL;

void set_es(void);
#pragma aux set_es = "push ds" "pop es";

static void _interrupt __far int24handler(union INTPACK r) {
   _enable();
   set_es();
   uchar code = r.w.di;
   ushort drive = r.w.ax;
   ushort devseg = r.w.bp;
   ushort devoff = r.w.si;
   if (code == 9) drive = 0xFF;          // Printer out of paper
   else if (drive & 0x8000) {
      code = 13;                          // Bad memory image of FAT
      char *devhd = (char *)MK_FP(devseg,devoff);
      if (devhd[5] & 0x80)                // Block device?
         code++;                           // Device access error
   }
   if (TSystemError::sysErrorFunc(code,drive) == 0) r.w.ax = 1;   // retry
   else r.w.ax = 3;                      // fail
   //  REGS rr;
   //  rr.w.ax = 0x5400;                   // Dummy function call to get
   //  int386(0x21,&rr,&rr);               // DOS into a stable state
}

static void _interrupt __far int1Bhandler(void) {
   *(char *)0x471 &= 0x7F;       // reset DOS Ctrl-Break bit
   TSystemError::ctrlBreakHit = True;
}

static void _interrupt __far int23handler(void) {}

inline ushort &KeyBufHead(void) { return *(ushort *)MK_FP(0x0,0x41A); }
inline ushort &KeyBufTail(void) { return *(ushort *)MK_FP(0x0,0x41C); }
#define KeyBufOrg 0x1E
#define KeyBufEnd 0x3E

static void _interrupt __far int09handler(void) {
   set_es();
   uchar code = inp(0x60);
   uchar shift = getShiftState();
   ushort tailoff = KeyBufTail();
   oldint09();
   if ((code & 0x80) == 0) {
      struct translation_t {
         uchar code;
         uchar shift;
         ushort answer;
      };
      const unsigned char scSpaceKey     = 0x39;
      const unsigned char scInsKey       = 0x52;
      const unsigned char scDelKey       = 0x53;
      static translation_t trans[] = {
         { scSpaceKey,kbAltShift,              kbAltSpace },
         { scInsKey,  kbCtrlShift,             kbCtrlIns  },
         { scInsKey,  kbLeftShift|kbRightShift,kbShiftIns },
         { scDelKey,  kbCtrlShift,             kbCtrlDel  },
         { scDelKey,  kbLeftShift|kbRightShift,kbShiftDel }
      };
#define TRANSNUM (sizeof(trans)/sizeof(trans[0]))
      for (int i=0; i < TRANSNUM; i++) {
         if (code == trans[i].code && (shift & trans[i].shift) != 0) {
            if (tailoff == KeyBufTail()) {
               tailoff += 2;
               if (tailoff == KeyBufEnd) tailoff = KeyBufOrg;
               if (tailoff == KeyBufHead()) break;   // no free space
               KeyBufTail() = tailoff;
            }
            *(ushort *)MK_FP(0x0,0x400+tailoff) = trans[i].answer;
            break;
         }
      }
   }
}

void TV_CDECL TSystemError::resume() {
   REGS r;
   r.w.ax = 0x3300;
   int386(0x21,&r,&r);
   saveCtrlBreak = Boolean(r.h.dl);
   r.w.ax = 0x3301;
   r.h.dl = 0;
   int386(0x21,&r,&r);
   if (oldint24 == NULL) {
      oldint24 = _dos_getvect(0x24);
      _dos_setvect(0x24,intvec_p(int24handler));
   }
   if (oldint23 == NULL) {
      oldint23 = _dos_getvect(0x23);
      _dos_setvect(0x23,intvec_p(int23handler));
   }
   if (oldint1B == NULL) {
      oldint1B = _dos_getvect(0x1B);
      _dos_setvect(0x1B,intvec_p(int1Bhandler));
   }
   if (oldint09 == NULL) {
      oldint09 = _dos_getvect(0x09);
      _dos_setvect(0x09,intvec_p(int09handler));
   }
}

void TV_CDECL TSystemError::suspend() {
   REGS r;
   if (oldint09 != NULL) {
      _dos_setvect(0x09,oldint09);
      oldint09 = NULL;
   }
   if (oldint1B != NULL) {
      _dos_setvect(0x1B,oldint1B);
      oldint1B = NULL;
   }
   if (oldint23 != NULL) {
      _dos_setvect(0x23,oldint23);
      oldint23 = NULL;
   }
   if (oldint24 != NULL) {
      _dos_setvect(0x24,oldint24);
      oldint24 = NULL;
   }
   r.w.ax = 0x3301;
   r.h.dl = saveCtrlBreak;
   int386(0x21,&r,&r);
}

#else   // __DOS32__

#ifdef __NT__

static int handler_set = 0;

BOOL __stdcall ctrlBreakHandler(DWORD dwCtrlType) {
   if (dwCtrlType == CTRL_BREAK_EVENT) {
      TSystemError::ctrlBreakHit = True;
      FlushConsoleInputBuffer(TThreads::chandle[cnInput]);
      while (TThreads::ispending()) TThreads::accept_event();
      return TRUE;
   }
   if (dwCtrlType == CTRL_C_EVENT) {
      DWORD cnt;
      INPUT_RECORD ir;
      ir.EventType = KEY_EVENT;
      ir.Event.KeyEvent.bKeyDown = 1;
      ir.Event.KeyEvent.wVirtualScanCode = 0x2E;
      ir.Event.KeyEvent.uChar.AsciiChar  = 3;
      TThreads::consoleMode &= ~ENABLE_PROCESSED_INPUT;
      SetConsoleMode(TThreads::chandle[cnInput],TThreads::consoleMode);
      WriteConsoleInput(TThreads::chandle[cnInput],&ir,1,&cnt);
      TThreads::consoleMode |=  ENABLE_PROCESSED_INPUT;
      SetConsoleMode(TThreads::chandle[cnInput],TThreads::consoleMode);
      return TRUE;
   }
   return FALSE; // Don't handle 'CLOSE', 'LOGOFF' or 'SHUTDOWN' events.
}

void TV_CDECL TSystemError::resume()  {
   if (!handler_set) SetConsoleCtrlHandler(ctrlBreakHandler, TRUE);
   handler_set = 1;
}

void TV_CDECL TSystemError::suspend() {
   if (handler_set) SetConsoleCtrlHandler(ctrlBreakHandler, FALSE);
   handler_set = 0;
}

#else   // __NT__
void TV_CDECL TSystemError::resume()  {}
void TV_CDECL TSystemError::suspend() {}
#endif  // __NT__
#endif  // __DOS32__
#endif  // __DOS16__

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

#ifdef __MSDOS__

ushort TSystemError::selectKey() {
   ushort crsrType = TScreen::getCursorType();

   TScreen::setCursorType(0x2000);

   while (getakey())
      ;

   int ch = getakey() & 0xFF;
   while (ch != 13 && ch != 27)
      ch = getakey() & 0xFF;

   TScreen::setCursorType(crsrType);
   return ch == 27;
}

short TV_CDECL TSystemError::sysErr(short errorCode, uchar drive) {
   ushort c = ((TScreen::screenMode & 0x00fF) != TDisplay::smMono) ?
              sysColorAttr : sysMonoAttr;
#ifdef __DOS32__
   char s[ 63 ];
   char *p;
   TDrawBuffer b;

   if (errorCode < sizeof(errorString)/sizeof(errorString[0])) {
      strcpy(s, errorString[ errorCode ]);
      p = strchr(s,'%');
   } else {
      strcpy(s,"Unknown error on drive ");
      p = strchr(s,'\0');
   }
   if (p != NULL) {
      *p++ = drive + 'a';
      *p++ = '\0';
   }
#else
   char s[ 63 ];
   TDrawBuffer b;

   if (errorCode < sizeof(errorString)/sizeof(errorString[0]))
      sprintf(s, errorString[ errorCode ], drive + 'a');
   else
      sprintf(s, "Unknown error %d on drive %c", errorCode, drive + 'a');
#endif
   b.moveChar(0, ' ', c, TScreen::screenWidth);
   b.moveCStr(1, s, c);
   b.moveCStr(TScreen::screenWidth-1-cstrlen(sRetryOrCancel), sRetryOrCancel, c);
   swapStatusLine(b);
   int res = selectKey();
   swapStatusLine(b);
   return res;
}

#endif  // __MSDOS__

#ifdef __DOS16__

Int11trap::Int11trap() {
   oldHandler = getvect(0x11);
   setvect(0x11, Int11trap::handler);
}

Int11trap::~Int11trap() {
   setvect(0x11, oldHandler);
}

void interrupt(* Int11trap::oldHandler)(...) = 0;

#pragma warn -eas

void interrupt Int11trap::handler(...) {
   if (_AX == SecretWord && _BX == productID)
      TSystemError::inIDE++;
   oldHandler();
}

#pragma warn .eas

#endif // __DOS16__
