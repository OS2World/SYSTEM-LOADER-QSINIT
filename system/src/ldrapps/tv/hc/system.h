/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   SYSTEM.H                                                              */
/*                                                                         */
/*   defines the classes THWMouse, TMouse, TEventQueue, TDisplay,          */
/*   TScreen, and TSystemError                                             */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#include <tvvo.h>

#if !defined( __EVENT_CODES )
#define __EVENT_CODES

/* Event codes */

#define evMouseDown (0x0001)
#define evMouseUp   (0x0002)
#define evMouseMove (0x0004)
#define evMouseAuto (0x0008)
#define evKeyDown   (0x0010)
#define evCommand   (0x0100)
#define evBroadcast (0x0200)

/* Event masks */

#define evNothing   (0x0000)
#define evMouse     (0x000F)
#define evKeyboard  (0x0010)
#define evMessage   (0xFF00)

/* Mouse button state masks */

#define mbLeftButton  (0x01)
#define mbRightButton (0x02)

#define meMouseMoved  (0x01)
#define meDoubleClick (0x02)

#endif  // __EVENT_CODES


#if defined( Uses_TEvent ) && !defined( __TEvent )
#define __TEvent

struct MouseEventType {
   TPoint where;
   ulong eventFlags;           // Replacement for doubleClick.
   ulong controlKeyState;
   uchar buttons;
};

#ifdef __OS2__
class TRect;
#endif

class THWMouse {

protected:

   THWMouse();
   THWMouse(const THWMouse &) {};
   ~THWMouse();

   static void TV_CDECL show();
   static void TV_CDECL hide();
#ifdef __OS2__
   static void hide(const TRect &rect);
#endif

   static void setRange(ushort, ushort);
   static void getEvent(MouseEventType &);
   static void registerHandler(unsigned, void ( *)());
   static Boolean present();

   static void suspend();
   static void resume();
   static void inhibit();

//protected:

   static uchar buttonCount;

private:

   static Boolean handlerInstalled;
   static Boolean noMouse;

};

inline Boolean THWMouse::present() {
   return Boolean(buttonCount != 0);
}

inline void THWMouse::inhibit() {
   noMouse = True;
}

class TMouse : public THWMouse {

public:

   TMouse();
   ~TMouse();

   static void show();
   static void hide();
#ifdef __OS2__
   static void hide(const TRect &rect) { THWMouse::hide(rect); }
#endif

   static void setRange(ushort, ushort);

   static void getEvent(MouseEventType &);
   static void registerHandler(unsigned, void ( *)());
   static Boolean present();

   static void suspend() { THWMouse::suspend(); }
   static void resume() { THWMouse::resume(); show(); }

};

inline void TMouse::show() {
   THWMouse::show();
}

inline void TMouse::hide() {
   THWMouse::hide();
}

inline void TMouse::setRange(ushort rx, ushort ry) {
   THWMouse::setRange(rx, ry);
}

inline void TMouse::getEvent(MouseEventType &me) {
   THWMouse::getEvent(me);
}

inline void TMouse::registerHandler(unsigned mask, void (*func)()) {
   THWMouse::registerHandler(mask, func);
}

inline Boolean TMouse::present() {
   return THWMouse::present();
}

#pragma pack(1)
struct CharScanType {
   uchar charCode;
   uchar scanCode;
};
#pragma pack()

struct KeyDownEvent {
   union {
      ushort keyCode;
      CharScanType charScan;
   };
   ulong controlKeyState;
};

struct MessageEvent {
   ushort command;
   union {
      void *infoPtr;
      long infoLong;
      ushort infoWord;
      short infoInt;
      uchar infoByte;
      char infoChar;
   };
};

struct TEvent {
   ushort what;
   union {
      MouseEventType mouse;
      KeyDownEvent keyDown;
      MessageEvent message;
   };
   void getMouseEvent();
   void getKeyEvent();
};

#endif  // Uses_TEvent

#if defined( Uses_TEventQueue ) && !defined( __TEventQueue )
#define __TEventQueue

class TEventQueue {
public:
   TEventQueue();
   ~TEventQueue();

   static void getMouseEvent(TEvent &);
   static void suspend();
   static void resume();

   friend class TView;
   friend void genRefs();
   friend unsigned long getTicks(void);
   friend class TProgram;
   static ushort doubleDelay;
   static Boolean mouseReverse;

private:

   static TMouse mouse;
   static void getMouseState(TEvent &);
#ifdef __DOS32__
   friend void _loadds mouseInt(int flag,int buttons,int x,int y);
#   pragma aux mouseInt parm [EAX] [EBX] [ECX] [EDX];
#else
   static void mouseInt();
#endif

   static void setLast(TEvent &);

   static MouseEventType lastMouse;
   static MouseEventType curMouse;

   static MouseEventType downMouse;
   static ushort downTicks;

   static ushort *Ticks;
   static TEvent eventQueue[ eventQSize ];
   static TEvent *eventQHead;
   static TEvent *eventQTail;
   static Boolean mouseIntFlag;
   static ushort eventCount;

   static Boolean mouseEvents;

   static ushort repeatDelay;
   static ushort autoTicks;
   static ushort autoDelay;

#ifndef __MSDOS__
public:
   static void mouseThread(void *arg);
   static void keyboardThread(void *arg);
   static TEvent keyboardEvent;
   static unsigned char shiftState;
#endif

};

inline void TEvent::getMouseEvent() {
   TEventQueue::getMouseEvent(*this);
}

#endif  // Uses_TEventQueue

#if defined( Uses_TScreen ) && !defined( __TScreen )
#define __TScreen

#ifdef PROTECT

extern ushort monoSeg;
extern ushort colrSeg;
extern ushort biosSeg;

#endif

class TDisplay {

public:

   friend class TView;

   enum videoModes {
      smBW80      = 0x0002,
      smCO80      = 0x0003,
      smMono      = 0x0007,
      smFont8x8   = 0x0100
   };

   static void clearScreen(int, int);

   static void setCursorType(ushort);
   static ushort getCursorType();

   static ushort getRows();
   static ushort getCols();

   static void setCrtMode(ushort);
   static ushort getCrtMode();

protected:

   TDisplay() { updateIntlChars(); };
   TDisplay(const TDisplay &) { updateIntlChars(); };
   ~TDisplay() {};

private:

   static void videoInt();
   static void updateIntlChars();

   static ushort *equipment;
   static uchar *crtInfo;
   static uchar *crtRows;

};

class TScreen : public TDisplay {

public:

   TScreen();
   ~TScreen();

   static void setVideoMode(ushort mode);
   static void clearScreen();

   static ushort startupMode;
   static ushort startupCursor;
   static ushort screenMode;
   static int screenWidth;
   static int screenHeight;
   static Boolean hiResScreen;
   static Boolean checkSnow;
   static uchar *screenBuffer;
   static ushort cursorLines;
   static Boolean clearOnSuspend;

   static void setCrtData();
   static ushort fixCrtMode(ushort);

   static void suspend();
   static void resume();

};

#endif  // Uses_TScreen

#if defined( Uses_TSystemError ) && !defined( __TSystemError )
#define __TSystemError

class TDrawBuffer;

class TSystemError {

public:

   TSystemError();
   ~TSystemError();

   static Boolean ctrlBreakHit;

   static void TV_CDECL suspend();
   static void TV_CDECL resume();
   static short(* TV_CDECL sysErrorFunc)(short, uchar);

private:

   static ushort sysColorAttr;
   static ushort sysMonoAttr;
   static Boolean saveCtrlBreak;
   static Boolean sysErrActive;

   static void TV_CDECL swapStatusLine(TDrawBuffer &);
   static ushort selectKey();
   static short TV_CDECL sysErr(short, uchar);

   static Boolean inIDE;

   static const char *const errorString[24];
   static const char *sRetryOrCancel;

   friend class Int11trap;

};

#ifdef __DOS16__

class Int11trap {

public:

   Int11trap();
   ~Int11trap();

private:

   static void interrupt handler(...);
   static void interrupt(*oldHandler)(...);

};

#endif  // __DOS16__

#endif  // Uses_TSystemError

#if defined( Uses_TThreaded ) && !defined( __TThreaded)
#define __TThreaded

#ifdef __OS2__
#define INCL_NOPMAPI
#define INCL_KBD
#define INCL_MOU
#define INCL_VIO
#define INCL_DOSPROCESS
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSMISC
#include <os2.h>

#pragma pack()
#pragma options -a.

// These must not cross a 64K boundary
struct TiledStruct {
   HMOU mouseHandle;
   MOUEVENTINFO mouseInfo;
   NOPTRRECT ptrArea;
   PTRLOC ptrLoc;
   VIOCURSORINFO cursorInfo;
   VIOMODEINFO modeInfo;
   KBDKEYINFO keyboardInfo;
   KBDINFO    keyboardShiftInfo;
};

class TThreads {
public:
   TThreads();
   ~TThreads();
   static void resume();
   static void suspend();
   static void deadEnd();
   static TiledStruct *tiled; // tiled will be allocated with DosAllocMem
   static TID mouseThreadID;
   static TID keyboardThreadID;
   static HEV hevMouse1;
   static HEV hevKeyboard1;
   static HEV hevKeyboard2;
   static HEV hevMouse2;
   static HMTX hmtxMouse1;
   static HMUX hmuxMaster;
   static volatile int shutDownFlag;
};

#endif // __OS2__

#ifdef __NT__
#define WIN32_LEAN_AND_MEAN
//#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
//#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
//#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
//#define NOSYSMETRICS      // SM_*
//#define NOMENUS           // MF_*
//#define NOICONS           // IDI_*
//#define NOKEYSTATES       // MK_*
//#define NOSYSCOMMANDS     // SC_*
//#define NORASTEROPS       // Binary and Tertiary raster ops
//#define NOSHOWWINDOW      // SW_*
//#define OEMRESOURCE       // OEM Resource values
//#define NOATOM            // Atom Manager routines
//#define NOCLIPBOARD       // Clipboard routines
//#define NOCOLOR           // Screen colors
//#define NODRAWTEXT        // DrawText() and DT_*
//#define NONLS             // All NLS defines and routines
//#define NOMB              // MB_* and MessageBox()
//#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
//#define NOMETAFILE        // typedef METAFILEPICT
//#define NOMINMAX          // Macros min(a,b) and max(a,b)
//#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
//#define NOSCROLL          // SB_* and scrolling routines
//#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
//#define NOSOUND           // Sound driver routines
//#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
//#define NOWH              // SetWindowsHook and WH_*
//#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
//#define NOCOMM            // COMM driver routines
//#define NOKANJI           // Kanji support stuff.
//#define NOHELP            // Help engine interface.
//#define NOPROFILER        // Profiler interface.
//#define NODEFERWINDOWPOS  // DeferWindowPos routines
#include <windows.h>

#pragma pack()
#ifndef __MSVC__
#pragma options -a.
#endif

class TThreads {
public:
   TThreads(void)        { resume(); }
   ~TThreads(void)       { suspend(); }
   static void resume();
   static void suspend();
   static HANDLE chandle[2];
#define cnInput  0
#define cnOutput 1
   static DWORD consoleMode;
   static CONSOLE_CURSOR_INFO crInfo;
   static CONSOLE_SCREEN_BUFFER_INFO sbInfo;
   static INPUT_RECORD ir;
   static DWORD evpending;       // is event present in 'ir'?
   static INPUT_RECORD *get_next_event(void);
   static inline int ispending(void) { return evpending; }
   static inline void accept_event(void) { evpending = 0; }
   static int macro_playing;
};

#endif // __NT__
#endif // Uses_TThreaded

#include <tvvo2.h>
