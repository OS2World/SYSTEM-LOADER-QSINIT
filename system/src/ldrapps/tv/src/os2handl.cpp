// os2handl.cpp
//
// - Functions that are not supplied by icc or emx but are used by TVision.
// - Threads and Semaphores
//
// There are a lot of #ifs in this file. It took a long time to get the thread stuff
// working, I kept all the unsuccessful attempts.
//
// Copyright 1993 by J”rn Sierwald

#define Uses_TEventQueue
#define Uses_TScreen
#define Uses_TThreaded
#define Uses_TKeys
#include <tv.h>
#include <tvdir.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifndef __QSINIT__
#include <process.h>
#include <assert.h>
#ifndef __IBMCPP__
#include <dos.h>
#endif // !__IBMCPP__
#endif

#if defined(__MSVC__)||defined(__IBMCPP__)
#define _dos_getdrive(old) {*old=_getdrive();}
#define _dos_setdrive(drive,dummy) _chdrive(drive)
#endif

#ifdef __IBMCPP__  /// THIS SET TThread initialization BEFORE any other...
#pragma priority(-2147482624) // INT_MIN +1024
#endif

#ifdef __OS2__

TiledStruct *TThreads::tiled;        // tiled will be allocated with DosAllocMem. The TiledStruct will
// not cross a 64K boundary, therefore the members can be used by
// 16Bit functions.
TID TThreads::mouseThreadID;
TID TThreads::keyboardThreadID;
static SEMRECORD evCombined[2];      // Two Event Semaphores, one for the Mouse and one for Keyboard.
// The main thread will do a MuxWait for both.
HEV TThreads::hevMouse1;
HEV TThreads::hevKeyboard1;
// Just an alias.
HEV TThreads::hevMouse2;                // _beginthread can't create a suspended thread, so
// we keep it waiting with this one
HEV TThreads::hevKeyboard2;             // Signals that the keypress is received.
HMTX TThreads::hmtxMouse1;              // Access to the internal mouse queue
HMUX TThreads::hmuxMaster;              // MuxWait for evCombined.
volatile int TThreads::shutDownFlag=0;

#ifndef __IBMCPP__
#define _Optlink
#endif

void _Optlink mouseThread(void *arg)    { TEventQueue::mouseThread(arg); }
void _Optlink keyboardThread(void *arg) { TEventQueue::keyboardThread(arg); }
// ICC doesn't want to call member functions as threads
void TThreads::deadEnd() {
   // Kill The current thread
   _endthread();
}

static TThreads mainobj;

TThreads::TThreads()  {}
TThreads::~TThreads() {}

void TThreads::resume() {
   //  cerr << "TThreads::resume\n";
   shutDownFlag=0;
   assert(! DosAllocMem((void **)&tiled,sizeof(TiledStruct),fALLOC | OBJ_TILE));
   tiled->modeInfo.cb = (unsigned short) sizeof(VIOMODEINFO);
#if 0
   printf("tiled=%lx\n", tiled);
   char line[5];
   gets(line);
#endif

   if (MouOpen((PSZ) 0, &tiled->mouseHandle) != 0) tiled->mouseHandle = 0xFFFF;

   assert(! DosCreateEventSem(NULL, &hevMouse1, 0, 0));
   assert(! DosCreateEventSem(NULL, &hevMouse2, 0, 0));
   assert(! DosCreateEventSem(NULL, &hevKeyboard1, 0, 0));
   assert(! DosCreateEventSem(NULL, &hevKeyboard2, 0, 0));
   assert(! DosCreateMutexSem(NULL, &hmtxMouse1, 0, 0));

   evCombined[0].ulUser=0;
   evCombined[0].hsemCur=(HSEM)hevMouse1;
   evCombined[1].ulUser=1;
   evCombined[1].hsemCur=(HSEM)hevKeyboard1;

   assert(!DosCreateMuxWaitSem(NULL,&hmuxMaster,2,evCombined,DCMW_WAIT_ANY));

   mouseThreadID = 0xFFFF;
   if (tiled->mouseHandle != 0xFFFF) {
#ifdef __BORLANDC__
      mouseThreadID = _beginthread(mouseThread,16384,NULL);
#else
      mouseThreadID = _beginthread(mouseThread,NULL,16384,NULL);
#endif
   }
   DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,31,mouseThreadID);
   DosPostEventSem(TThreads::hevMouse2);
#ifdef __BORLANDC__
   keyboardThreadID = _beginthread(keyboardThread,16384,NULL);
#else
   keyboardThreadID = _beginthread(keyboardThread,NULL,16384,NULL);
#endif
   DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,31,keyboardThreadID);
}

void TThreads::suspend() {
   shutDownFlag=1;
   TID localTID=mouseThreadID;
   if (localTID != 0xFFFF) DosWaitThread(&localTID,DCWW_WAIT);
   //  cerr << " MouseThread has ended\n";
   ULONG count;
   assert(! DosQueryEventSem(TThreads::hevKeyboard2,&count));
   if (!count)
      assert(! DosPostEventSem(TThreads::hevKeyboard2));  // Make sure the thread is running.
   localTID=keyboardThreadID;
   DosWaitThread(&localTID,DCWW_WAIT);
   //  cerr << " KeyboardThread has ended\n";

   assert(!DosCloseMutexSem(hmtxMouse1));
   assert(!DosCloseEventSem(hevKeyboard2));
   assert(!DosCloseEventSem(hevKeyboard1));
   assert(!DosCloseEventSem(hevMouse2));
   assert(!DosCloseEventSem(hevMouse1));
   assert(!DosCloseMuxWaitSem(hmuxMaster));

   if (tiled->mouseHandle != 0xFFFF) MouClose(tiled->mouseHandle);   // This doesn't work, the mouseThread uses the handle.
   assert(! DosFreeMem(tiled));  // Better not, dito
}

unsigned long getTicks() {
   // return a value that can be used as a substitute for the DOS Ticker at [0040:006C]
   unsigned long m;
   DosQuerySysInfo(14, 14, &m, sizeof(m));
   return m/52;
}

unsigned char getShiftState() {
   // returns a value that can be used as a substitute for the shift state at [0040:0017]
   TThreads::tiled->keyboardShiftInfo.cb = 10;
   KbdGetStatus(&TThreads::tiled->keyboardShiftInfo, 0);
   return TThreads::tiled->keyboardShiftInfo.fsState & 0xFF;
}

#endif  // __OS2__

//---------------------------------------------------------------------------

#ifdef __NT__

HANDLE TThreads::chandle[2];
DWORD TThreads::consoleMode;
CONSOLE_CURSOR_INFO TThreads::crInfo;
CONSOLE_SCREEN_BUFFER_INFO TThreads::sbInfo;
INPUT_RECORD TThreads::ir;
DWORD TThreads::evpending;
int TThreads::macro_playing = 0;
static int inited = 0;

TThreads tvision_for_win32_starter;     // just to call resume()

void TThreads::resume() {
   if (!inited) {
      SetFileApisToOEM();
      chandle[cnInput]  = GetStdHandle(STD_INPUT_HANDLE);
      chandle[cnOutput] = GetStdHandle(STD_OUTPUT_HANDLE);
      GetConsoleCursorInfo(chandle[cnOutput], &crInfo);
      GetConsoleScreenBufferInfo(chandle[cnOutput], &sbInfo);
      GetConsoleMode(chandle[cnInput],&consoleMode);
      consoleMode &= ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_WINDOW_INPUT);
      SetConsoleMode(chandle[cnInput],consoleMode);

      evpending = 0;
      inited = 1;
   }
}

void TThreads::suspend() {
   inited = 0;
   //  SetConsoleMode(chandle[cnOutput],
   //               ENABLE_PROCESSED_OUTPUT|ENABLE_WRAP_AT_EOL_OUTPUT);
}

unsigned long getTicks() {
   // return a value that can be used as a substitute for the DOS Ticker at [0040:006C]
   // To change units from ms to clock ticks.
   //   X ms * 1s/1000ms * 18.2ticks/s = X/55 ticks, roughly.
   return GetTickCount() / 55;
}

unsigned char getShiftState() {
   // returns a value that can be used as a substitute for the shift state at [0040:0017]
   uchar state = TThreads::ir.Event.KeyEvent.dwControlKeyState;
   uchar tvstate = 0;
   //  if ( state & (LEFT_ALT_PRESSED)  ) tvstate |= kbAltShift;
   if (state & (RIGHT_ALT_PRESSED |LEFT_ALT_PRESSED)) tvstate |= kbAltShift;
   if (state & (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)) tvstate |= kbCtrlShift;
   if (state & SHIFT_PRESSED) tvstate |= kbLeftShift;
   if (state & NUMLOCK_ON) tvstate |= kbNumState;
   if (state & SCROLLLOCK_ON) tvstate |= kbScrollState;
   if (state & CAPSLOCK_ON) tvstate |= kbCapsState;
   return tvstate;
}

#define IO_RAW_EVENT    0
#define IO_CHR_EVENT    1
#define IO_IGN_EVENT    2
static int event_type(INPUT_RECORD &ir) {
   if (ir.EventType == MOUSE_EVENT) return IO_RAW_EVENT;
   if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
      //  Skip shifts and caps,num,scroll locks
      static char ignore[] = { 0x1D,0x2A,0x38,0x36,0x3A,0x45,0x46,0 };
      if (strchr(ignore,ir.Event.KeyEvent.wVirtualScanCode) != NULL)
         return IO_IGN_EVENT;
      uchar chr = ir.Event.KeyEvent.uChar.AsciiChar;
      if (chr == 0) return IO_RAW_EVENT;
      // ++++++++++++ DEBUGGING CODE
#if 0
      printf("vcode=%x vscan=%x ascii=%c\n",
             ir.Event.KeyEvent.wVirtualKeyCode,
             ir.Event.KeyEvent.wVirtualScanCode,
             ir.Event.KeyEvent.uChar.AsciiChar);
#endif
      // ++++++++++++ END OF DEBUGGING CODE

      // the keypad keys are different from normal keys:
      switch (ir.Event.KeyEvent.wVirtualKeyCode) {
         case VK_ADD:
         case VK_SUBTRACT:
            return IO_RAW_EVENT;
      }

      int state = ir.Event.KeyEvent.dwControlKeyState;
      //    if ( state & (RIGHT_ALT_PRESSED|LEFT_CTRL_PRESSED) ) return IO_CHR_EVENT;
      if (state & (RIGHT_ALT_PRESSED
                   |LEFT_ALT_PRESSED
                   |RIGHT_CTRL_PRESSED
                   |LEFT_CTRL_PRESSED)) return IO_RAW_EVENT;
      static char chars[] = "`1234567890-=   ~!@#$%^&*()_+"
                            "qwertyuiop[]\\  QWERTYUIOP{}|"
                            "asdfghjkl;'     ASDFGHJKL:\""
                            "zxcvbnm,./      ZXCVBNM<>?";
      if (strchr(chars,chr) != NULL) return IO_CHR_EVENT;
      return IO_RAW_EVENT;
   }
   return IO_IGN_EVENT;
}

INPUT_RECORD *TThreads::get_next_event(void) {
   if (evpending) return &ir;
   PeekConsoleInput(chandle[cnInput],&ir,1,&evpending);
   if (evpending) {
      int code = event_type(ir);
      //    printf("evtype = %d\n",code);
      switch (code) {
         case IO_RAW_EVENT:
            ReadConsoleInput(chandle[cnInput],&ir,1,&evpending);
            break;
         case IO_CHR_EVENT:
            char chr;
            //      printf("before readconsole\n");
            ReadConsole(chandle[cnInput],&chr,1,&evpending,NULL);
            //      printf("key %x %d\n",chr,evpending);
            ir.Event.KeyEvent.uChar.AsciiChar = chr;
            break;
         case IO_IGN_EVENT:
            ReadConsoleInput(chandle[cnInput],&ir,1,&evpending);
            accept_event();
            break;
      }
   }
   return evpending ? &ir : NULL;
}

#endif  // __NT__

//---------------------------------------------------------------------------

#ifndef __BORLANDC__

int fnsplit(const char *__path,
            char *__drive,
            char *__dir,
            char *__name,
            char *__ext) {
   _splitpath(__path,__drive,__dir,__name,__ext);
   int code = 0;
   if (__drive != NULL && __drive[0] != '\0') code |= DRIVE;
   if (__dir   != NULL && __dir  [0] != '\0') code |= DIRECTORY;
   if (__name  != NULL && __name [0] != '\0') code |= FILENAME;
   if (__ext   != NULL && __ext  [0] != '\0') code |= EXTENSION;
   return code;
}

/* makepath -- Copyright (c) 1993 by Eberhard Mattes */

static void my_makepath(char *dst, const char *drive, const char *dir,
                        const char *fname, const char *ext) {
   int n;
   char slash;

   n = 0; slash = '/';
   if (drive != NULL && *drive != 0) {
      dst[n++] = *drive;
      dst[n++] = ':';
   }
   if (dir != NULL && *dir != 0) {
      while (n < MAXPATH - 1 && *dir != 0) {
         if (*dir == '\\')
            slash = '\\';
         dst[n++] = *dir++;
      }
      if (dst[n-1] != '\\' && dst[n-1] != '/' && n < MAXPATH - 1)
         dst[n++] = slash;
   }
   if (fname != NULL) {
      while (n < MAXPATH - 1 && *fname != 0)
         dst[n++] = *fname++;
   }
   if (ext != NULL && *ext != 0) {
      if (*ext != '.' && n < MAXPATH - 1)
         dst[n++] = '.';
      while (n < MAXPATH - 1 && *ext != 0)
         dst[n++] = *ext++;
   }
   dst[n] = 0;
}

void fnmerge(char *__path,
             const char *__drive,
             const char *__dir,
             const char *__name,
             const char *__ext) {
   my_makepath(__path,__drive,__dir,__name,__ext);
}

int getdisk(void) {
   unsigned int mydrive;
   _dos_getdrive(&mydrive);
   return mydrive-1;
}

// getcurdir should fill buffer with the current directory without
// drive char and without a leading backslash.

int getcurdir(int __drive, char *buffer) {
#define MAXPATHLEN MAXPATH
   long size=MAXPATHLEN;
   char tmp[MAXPATHLEN+1];
   tmp[0]='\\';
#if 1
   unsigned int old, dummy;
   _dos_getdrive(&old);
   _dos_setdrive(__drive,&dummy);
   if (getcwd(tmp, size) == 0) {  // getcwd returns a leading backslash
      _dos_setdrive(old,&dummy);
      return 0;
   }
   _dos_setdrive(old,&dummy);
   strcpy(buffer,&tmp[3]);
   return 1;
#endif  // 1
#if 0
#if defined(__OS2__)
   size_t len;
   char *p;
   unsigned long l = MAXPATHLEN;
   if (DosQueryCurrentDir(__drive,tmp+1,&l)) return NULL;
   len = strlen(tmp) + 2;
   if (buffer == NULL) {
      if (size < len) size = len;
      buffer = new char[size];
   }
   if (buffer == NULL) {
      errno = ENOMEM;
      return (0);
   }
   for (p = tmp; *p != 0; ++p) // no forward slashes please.
      if (*p == '/') *p = '\\';
   strcpy(buffer, tmp+1);
   return 1;
#endif  // __OS2__
#endif  // 0
}

#endif // !__BORLANDC__ ig
