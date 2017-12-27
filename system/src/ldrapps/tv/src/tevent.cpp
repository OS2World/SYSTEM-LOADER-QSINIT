/*------------------------------------------------------------*/
/* filename -       tevent.cpp                                */
/*                                                            */
/* function(s)                                                */
/*                  TEvent member functions                   */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TEventQueue
#define Uses_TEvent
#define Uses_TScreen
#define Uses_TKeys
#define Uses_TSystemError
#define Uses_TThreaded
#include <tv.h>
#include <stdio.h>

#ifdef __OS2__
#include <assert.h>
#endif

TEvent  TEventQueue::eventQueue[ eventQSize ] = { {0} };
TEvent *TEventQueue::eventQHead = TEventQueue::eventQueue;
TEvent *TEventQueue::eventQTail = TEventQueue::eventQueue;
Boolean TEventQueue::mouseIntFlag = False;

#ifdef __QSINIT__
#include <qsutil.h>
#include <vio.h>

#elif defined(__MSDOS__)

#include <dos.h>
#include <bios.h>

ushort *TEventQueue::Ticks = (ushort *)MK_FP(0x000, 0x046c);
unsigned long getTicks(void) { return *(TEventQueue::Ticks); }

#else   // __MSDOS__

TEvent TEventQueue::keyboardEvent;
unsigned char TEventQueue::shiftState;

#endif  // __MSDOS__

ushort  TEventQueue::eventCount = 0;
Boolean TEventQueue::mouseEvents = False;
Boolean TEventQueue::mouseReverse = False;
ushort  TEventQueue::doubleDelay = 8;
ushort  TEventQueue::repeatDelay = 8;
ushort  TEventQueue::autoTicks = 0;
ushort  TEventQueue::autoDelay = 0;

MouseEventType TEventQueue::lastMouse;
MouseEventType TEventQueue::curMouse;
MouseEventType TEventQueue::downMouse;
ushort TEventQueue::downTicks = 0;

TEventQueue::TEventQueue() {
#ifdef __MSDOS__
   resume();
#endif
}

void TEventQueue::resume() {
#ifdef __OS2__
   {
      KBDINFO ki;
      ki.cb = sizeof(ki);
      assert(!KbdGetStatus(&ki,0));
      ki.fsMask &= ~KEYBOARD_ASCII_MODE;
      ki.fsMask |= KEYBOARD_BINARY_MODE;
      assert(!KbdSetStatus(&ki,0));
   }
#endif
   if (mouse.present() == False)
      mouse.resume();
   if (mouse.present() == False)
      return;
   mouse.getEvent(curMouse);
   lastMouse = curMouse;
   mouse.registerHandler(0xFFFF, (void ( *)(void))mouseInt);
   mouseEvents = True;
   mouse.show();
   TMouse::setRange(TScreen::screenWidth-1, TScreen::screenHeight-1);
}

void TEventQueue::suspend() {
#ifdef __OS2__
   {
      KBDINFO ki;
      ki.cb = sizeof(ki);
      assert(!KbdGetStatus(&ki,0));
      ki.fsMask |= KEYBOARD_ASCII_MODE;
      ki.fsMask &= ~KEYBOARD_BINARY_MODE;
      assert(!KbdSetStatus(&ki,0));
   }
#endif
   mouse.suspend();
}

TEventQueue::~TEventQueue() {
#ifdef __MSDOS__
   suspend();
#endif
}

void TEventQueue::getMouseEvent(TEvent &ev) {
   if (mouseEvents == True) {

      getMouseState(ev);

      if (ev.mouse.buttons == 0 && lastMouse.buttons != 0) {
         ev.what = evMouseUp;
         lastMouse = ev.mouse;
         return;
      }

      if (ev.mouse.buttons != 0 && lastMouse.buttons == 0) {
         if (ev.mouse.buttons == downMouse.buttons &&
            ev.mouse.where == downMouse.where &&
               ev.what - downTicks <= doubleDelay &&
                 !(downMouse.eventFlags & meDoubleClick))
                    ev.mouse.eventFlags |= meDoubleClick;

          downMouse = ev.mouse;
          autoTicks = downTicks = ev.what;
          autoDelay = repeatDelay;
          ev.what = evMouseDown;
          lastMouse = ev.mouse;
          return;
      }

      ev.mouse.buttons = lastMouse.buttons;

      if (ev.mouse.where != lastMouse.where) {
         ev.what = evMouseMove;
         ev.mouse.eventFlags |= meMouseMoved;
         lastMouse = ev.mouse;
         return;
      }

      if (ev.mouse.buttons != 0 && ev.what - autoTicks > autoDelay) {
         autoTicks = ev.what;
         autoDelay = 1;
         ev.what = evMouseAuto;
         lastMouse = ev.mouse;
         return;
      }
   }

   ev.what = evNothing;
}

void TEventQueue::getMouseState(TEvent &ev) {
#ifdef __QSINIT__
   memset(&ev, 0, sizeof(ev));
   ev.what = getTicks();
#else
#ifdef __NT__
   ev.mouse.where   = lastMouse.where;
   ev.mouse.buttons = lastMouse.buttons;
   if (lastMouse.buttons!=0) ev.what = getTicks();  // Temporarily save tick count when event was read.

   INPUT_RECORD *irp = TThreads::get_next_event();
   if (irp == NULL || irp->EventType != MOUSE_EVENT) return;
   TThreads::accept_event();
   curMouse.where.x = irp->Event.MouseEvent.dwMousePosition.X;
   curMouse.where.y = irp->Event.MouseEvent.dwMousePosition.Y;
   curMouse.buttons = irp->Event.MouseEvent.dwButtonState;

   curMouse.eventFlags = 0;
   if (irp->Event.MouseEvent.dwEventFlags & DOUBLE_CLICK)
      curMouse.eventFlags |= meDoubleClick;
   curMouse.controlKeyState = irp->Event.MouseEvent.dwControlKeyState;

   ev.mouse = curMouse;
   if (lastMouse.buttons == 0) ev.what = getTicks();
#else   // __NT__
#ifdef __OS2__
   assert(! DosRequestMutexSem(TThreads::hmtxMouse1,SEM_INDEFINITE_WAIT));
#else
   _disable();
#endif
   if (eventCount==0) {
      ev.what = getTicks();
      ev.mouse = curMouse;
   } else {
      ev = *eventQHead;
      if (++eventQHead >= eventQueue + eventQSize)
         eventQHead = eventQueue;
      eventCount--;
   }
#ifdef __OS2__
   if (eventCount==0) {
      ULONG dummy;
      DosResetEventSem(TThreads::hevMouse1, &dummy);
   }
   assert(! DosReleaseMutexSem(TThreads::hmtxMouse1));
#else
   _enable();
#endif  // __OS2__
#endif  // __NT__
#endif  // __QSINIT__
   if (mouseReverse!=False && ev.mouse.buttons!=0 && ev.mouse.buttons!=3)
      ev.mouse.buttons ^= 3;
}

#ifdef __DOS16__
#pragma saveregs

void TEventQueue::mouseInt() {
   unsigned flag = _AX;
   MouseEventType tempMouse;

   tempMouse.buttons = _BL;
   tempMouse.eventFlags = 0;
   tempMouse.where.x = _CX >> 3;
   tempMouse.where.y = _DX >> 3;

   if ((flag & 0x1e) != 0 && eventCount < eventQSize) {
      eventQTail->what = *Ticks;
      eventQTail->mouse = curMouse;
      if (++eventQTail >= eventQueue + eventQSize)
         eventQTail = eventQueue;
      eventCount++;
   }

   curMouse = tempMouse;
   mouseIntFlag = True;
}

void TEvent::getKeyEvent() {
   asm {
      MOV AH,1;
      INT 16h;
      JNZ keyWaiting;
   };
   what = evNothing;
   return;

keyWaiting:
   what = evKeyDown;
   asm {
      MOV AH,0;
      INT 16h;
   };
   keyDown.keyCode = _AX;
   asm {
      MOV AH,2;
      INT 16h;
   };
   keyDown.controlKeyState = _AL;
   return;
}

#endif // __DOS16__
//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
const unsigned char scSpaceKey          = 0x39;
const unsigned char scInsKey            = 0x52;
const unsigned char scDelKey            = 0x53;
const unsigned char scGreyEnterKey      = 0xE0;

#ifdef __DOS32__
#pragma off (check_stack)
void _loadds mouseInt(int flag,int buttons,int x,int y) {
   MouseEventType tempMouse;

   tempMouse.buttons = buttons;
   tempMouse.eventFlags = 0;
   tempMouse.where.x = x >> 3;
   tempMouse.where.y = y >> 3;

   if ((flag & 0x1e) != 0 && TEventQueue::eventCount < eventQSize) {
      TEventQueue::eventQTail->what = getTicks();
      TEventQueue::eventQTail->mouse.buttons    = TEventQueue::curMouse.buttons;
      TEventQueue::eventQTail->mouse.eventFlags = TEventQueue::curMouse.eventFlags;
      TEventQueue::eventQTail->mouse.where.x    = TEventQueue::curMouse.where.x;
      TEventQueue::eventQTail->mouse.where.y    = TEventQueue::curMouse.where.y;
      if (++TEventQueue::eventQTail >= TEventQueue::eventQueue + eventQSize)
         TEventQueue::eventQTail = TEventQueue::eventQueue;
      TEventQueue::eventCount++;
   }

   TEventQueue::curMouse.buttons    = tempMouse.buttons;
   TEventQueue::curMouse.eventFlags = tempMouse.eventFlags;
   TEventQueue::curMouse.where.x    = tempMouse.where.x;
   TEventQueue::curMouse.where.y    = tempMouse.where.y;
   TEventQueue::mouseIntFlag = True;
}

//#include <stdio.h>
void TEvent::getKeyEvent() {
   //
   // _bios_keybrd(_KEYBRD_READY) returns 0 if the user presses Ctrl-Break
   // even if there are other keystrokes in the buffer. Therefore we
   // call once _bios_keybrd(_KEYBRD_READ) to clear the input buffer
   // and set 'ctrlBreakHit' to 2 to indicate it.
   // If we don't, the keyboard hangs...
   //
   if (TSystemError::ctrlBreakHit == Boolean(1)) {
      _bios_keybrd(_KEYBRD_READ);
      TSystemError::ctrlBreakHit = Boolean(2);
   }
   if (_bios_keybrd(_KEYBRD_READY) == 0) {
      what = evNothing;
   } else {
      what = evKeyDown;
      keyDown.keyCode = _bios_keybrd(_KEYBRD_READ);
      int shift = getShiftState();
      keyDown.controlKeyState = shift;
      //printf("keyCode=%04X\n",keyDown.keyCode);
      switch (keyDown.charScan.scanCode) {
         case scSpaceKey:
            if (shift & kbAltShift) keyDown.keyCode = kbAltSpace;
            break;
         case scInsKey: {
            if (shift & kbCtrlShift) keyDown.keyCode = kbCtrlIns; else
               if (shift & (kbLeftShift|kbRightShift)) keyDown.keyCode = kbShiftIns;
         }
         break;
         case scDelKey: {
            if (shift & kbCtrlShift) keyDown.keyCode = kbCtrlDel; else
               if (shift & (kbLeftShift|kbRightShift)) keyDown.keyCode = kbShiftDel;
         }
         break;
      }
   }
}

#endif // __DOS32__
//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
#ifdef __OS2__

void TEventQueue::mouseInt() { /* no mouse... */ }

// These variables are used to reduce the CPU time used by the
// two threads if they don't receive any input from the user for some time.
volatile int mousePollingDelay = 0;
volatile int keyboardPollingDelay = 0;

#define jsSuspendThread if (TThreads::shutDownFlag) break;

void TEventQueue::mouseThread(void *arg) {
   arg = arg;
   assert(! DosWaitEventSem(TThreads::hevMouse2, SEM_INDEFINITE_WAIT));
   MouseEventType tempMouse;
   MOUEVENTINFO *info = &TThreads::tiled->mouseInfo;

   while (1) {
      do {
         jsSuspendThread
         USHORT type = 0; // non-blocking read
         MouReadEventQue(info, &type, TThreads::tiled->mouseHandle);
         if (info->time==0) {
            DosSleep(mousePollingDelay);
            if (mousePollingDelay < 500) mousePollingDelay += 5;
         } else {
            mousePollingDelay=0;
         }
      } while (info->time==0);
      tempMouse.buttons = ((info->fs & 06)  != 0) +
                          (((info->fs & 030) != 0) << 1)+
                          (((info->fs & 0140) != 0) << 2);
      tempMouse.where.x = info->col;
      tempMouse.where.y = info->row;
      tempMouse.eventFlags = False;
      tempMouse.controlKeyState = getShiftState();

      jsSuspendThread
      DosRequestMutexSem(TThreads::hmtxMouse1,SEM_INDEFINITE_WAIT);
      if ((tempMouse.buttons!=curMouse.buttons) && eventCount < eventQSize) {
         eventQTail->what = info->time/52; // *Ticks
         eventQTail->mouse = tempMouse; // curMouse;
         if (++eventQTail >= eventQueue + eventQSize) eventQTail = eventQueue;
         eventCount++;
      }

      curMouse = tempMouse;
      ULONG counter = 0;
      DosQueryEventSem(TThreads::hevMouse1,&counter);
      if (counter == 0) {
         APIRET rc = DosPostEventSem(TThreads::hevMouse1);
         assert(rc==0 || rc==ERROR_ALREADY_POSTED);
      }
      assert(! DosReleaseMutexSem(TThreads::hmtxMouse1));
   }
   TThreads::deadEnd();
}


void TEventQueue::keyboardThread(void *arg) {
   arg = arg;
   KBDKEYINFO *info = &TThreads::tiled->keyboardInfo;

   while (1) {
      jsSuspendThread
      USHORT errCode = KbdCharIn(info, IO_NOWAIT, 0);
      jsSuspendThread
      if (errCode || (info->fbStatus & 0xC0)!=0x40 || info->fbStatus & 1) {  // no char
         keyboardEvent.what = evNothing;
         DosSleep(keyboardPollingDelay);
         if (keyboardPollingDelay < 500) keyboardPollingDelay += 5;
      } else {
         keyboardEvent.what = evKeyDown;

         if ((info->fbStatus & 2) && (info->chChar==0xE0)) info->chChar=0; // OS/2 cursor keys.
         keyboardEvent.keyDown.charScan.charCode=info->chChar;
         keyboardEvent.keyDown.charScan.scanCode=info->chScan;
         shiftState = info->fsState & 0xFF;

         jsSuspendThread

         assert(! DosPostEventSem(TThreads::hevKeyboard1));

         jsSuspendThread
         assert(! DosWaitEventSem(TThreads::hevKeyboard2, SEM_INDEFINITE_WAIT));
         keyboardEvent.what = evNothing;
         ULONG dummy;
         jsSuspendThread
         assert(! DosResetEventSem(TThreads::hevKeyboard2, &dummy));
         keyboardPollingDelay=0;
      }
   }
   TThreads::deadEnd();
}

// Some Scancode conversion.. in OS/2 ShiftIns/ShiftDel only
// I got understand the CtrlDel/Ins Conversion. Maybe this is
// XT-Keyboard only

static unsigned char translateTable1[] = {
   scSpaceKey,
   scInsKey, scDelKey,
   scInsKey, scDelKey
};

static unsigned char translateTable2[] = {
   kbAltShift,
   kbCtrlShift, kbCtrlShift,
   kbLeftShift+kbRightShift, kbLeftShift+kbRightShift
};

static unsigned short translateTable3[]= {
   kbAltSpace,
   kbCtrlIns,  kbCtrlDel,
   kbShiftIns, kbShiftDel
};

const unsigned translateTableLength = 5;


void TEvent::getKeyEvent() {
   unsigned char shiftState = 0;
   ULONG dummy;
   assert(! DosResetEventSem(TThreads::hevKeyboard1, &dummy));

   *this = TEventQueue::keyboardEvent;
   shiftState = TEventQueue::shiftState;
   assert(! DosPostEventSem(TThreads::hevKeyboard2));

   for (int i=0; i<translateTableLength; i++) {
      if ((translateTable1[i]==    keyDown.charScan.scanCode) &&
          (translateTable2[i] & shiftState)) {
         keyDown.keyCode = translateTable3[i];
         break;
      }
   }
   if (keyDown.charScan.scanCode == scGreyEnterKey)
      keyDown.keyCode = kbEnter;
   keyDown.controlKeyState = shiftState;

   return;

}
#endif  // __OS2__

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#ifdef __NT__

struct win32_trans_t {
   ushort tkey;
   uchar vkey;
};

static win32_trans_t alt_trans[] = {
   { kbAlt0      , '0'       },
   { kbAlt1      , '1'       },
   { kbAlt2      , '2'       },
   { kbAlt3      , '3'       },
   { kbAlt4      , '4'       },
   { kbAlt5      , '5'       },
   { kbAlt6      , '6'       },
   { kbAlt7      , '7'       },
   { kbAlt8      , '8'       },
   { kbAlt9      , '9'       },
   { kbAltA      , 'A'       },
   { kbAltB      , 'B'       },
   { kbAltC      , 'C'       },
   { kbAltD      , 'D'       },
   { kbAltE      , 'E'       },
   { kbAltF      , 'F'       },
   { kbAltG      , 'G'       },
   { kbAltH      , 'H'       },
   { kbAltI      , 'I'       },
   { kbAltJ      , 'J'       },
   { kbAltK      , 'K'       },
   { kbAltL      , 'L'       },
   { kbAltM      , 'M'       },
   { kbAltN      , 'N'       },
   { kbAltO      , 'O'       },
   { kbAltP      , 'P'       },
   { kbAltQ      , 'Q'       },
   { kbAltR      , 'R'       },
   { kbAltS      , 'S'       },
   { kbAltT      , 'T'       },
   { kbAltU      , 'U'       },
   { kbAltV      , 'V'       },
   { kbAltW      , 'W'       },
   { kbAltX      , 'X'       },
   { kbAltY      , 'Y'       },
   { kbAltZ      , 'Z'       },
   { kbAltF1     , VK_F1     },
   { kbAltF2     , VK_F2     },
   { kbAltF3     , VK_F3     },
   { kbAltF4     , VK_F4     },
   { kbAltF5     , VK_F5     },
   { kbAltF6     , VK_F6     },
   { kbAltF7     , VK_F7     },
   { kbAltF8     , VK_F8     },
   { kbAltF9     , VK_F9     },
   { kbAltF10    , VK_F10    },
   { kbAltSpace  , VK_SPACE  },
   { kbAltEqual  , VK_ADD    },
   { kbAltMinus  , VK_SUBTRACT},
   { kbAltBack   , VK_BACK   },
   { kbAltEqual  , 0xBB      },
   { kbAltMinus  , 0xBD      },
   { 0           , 0         }
};

static win32_trans_t ctrl_trans[] = {
   { kbCtrlBack  , VK_BACK    },
   { kbCtrlDel   , VK_DELETE  },
   { kbCtrlEnd   , VK_END     },
   { kbCtrlEnter , VK_RETURN  },
   { kbCtrlF1    , VK_F1      },
   { kbCtrlF2    , VK_F2      },
   { kbCtrlF3    , VK_F3      },
   { kbCtrlF4    , VK_F4      },
   { kbCtrlF5    , VK_F5      },
   { kbCtrlF6    , VK_F6      },
   { kbCtrlF7    , VK_F7      },
   { kbCtrlF8    , VK_F8      },
   { kbCtrlF9    , VK_F9      },
   { kbCtrlF10   , VK_F10     },
   { kbCtrlHome  , VK_HOME    },
   { kbCtrlIns   , VK_INSERT  },
   { kbCtrlLeft  , VK_LEFT    },
   { kbCtrlPgDn  , VK_NEXT    },
   { kbCtrlPgUp  , VK_PRIOR   },
   { kbCtrlPrtSc , VK_SNAPSHOT},
   { kbCtrlRight , VK_RIGHT   },
   { 0           , 0          }
};

static win32_trans_t shift_trans[] = {
   { kbShiftDel  , VK_DELETE  },
   { kbShiftF1   , VK_F1      },
   { kbShiftF2   , VK_F2      },
   { kbShiftF3   , VK_F3      },
   { kbShiftF4   , VK_F4      },
   { kbShiftF5   , VK_F5      },
   { kbShiftF6   , VK_F6      },
   { kbShiftF7   , VK_F7      },
   { kbShiftF8   , VK_F8      },
   { kbShiftF9   , VK_F9      },
   { kbShiftF10  , VK_F10     },
   { kbShiftIns  , VK_INSERT  },
   { kbShiftTab  , VK_TAB     },
   { kbEsc       , VK_ESCAPE  },      // Translate Shift-Esc to Esc
   { 0           , 0          }
};

static win32_trans_t plain_trans[] = {
   { kbBack      , VK_BACK    },
   { kbTab       , VK_TAB     },
   { kbEnter     , VK_RETURN  },
   { kbEsc       , VK_ESCAPE  },
   { kbPgUp      , VK_PRIOR   },
   { kbPgDn      , VK_NEXT    },
   { kbEnd       , VK_END     },
   { kbHome      , VK_HOME    },
   { kbLeft      , VK_LEFT    },
   { kbUp        , VK_UP      },
   { kbRight     , VK_RIGHT   },
   { kbDown      , VK_DOWN    },
   { kbIns       , VK_INSERT  },
   { kbDel       , VK_DELETE  },
   { kbF1        , VK_F1      },
   { kbF2        , VK_F2      },
   { kbF3        , VK_F3      },
   { kbF4        , VK_F4      },
   { kbF5        , VK_F5      },
   { kbF6        , VK_F6      },
   { kbF7        , VK_F7      },
   { kbF8        , VK_F8      },
   { kbF9        , VK_F9      },
   { kbF10       , VK_F10     },
   { kbGrayPlus  , VK_ADD     },
   { kbGrayMinus , VK_SUBTRACT},
   { 0           , 0          }
};

void TEventQueue::mouseInt() { /* no mouse... */ }

ushort translate_vkey(win32_trans_t *table,uchar vkey) {
   while (table->vkey != 0) {
      if (table->vkey == vkey) return table->tkey;
      table++;
   }
   return 0;
}

void TEvent::getKeyEvent() {
   what = evNothing;
   INPUT_RECORD *irp = TThreads::get_next_event();
   if (irp == NULL || irp->EventType != KEY_EVENT) return;
   TThreads::accept_event();
   keyDown.charScan.scanCode = irp->Event.KeyEvent.wVirtualScanCode;
   keyDown.charScan.charCode = irp->Event.KeyEvent.uChar.AsciiChar;

   what = evKeyDown;

   int shift = getShiftState();

   // Convert NT style virtual key codes to Tvision keycodes

   uchar vk = irp->Event.KeyEvent.wVirtualKeyCode;
   ushort tk;
   if (shift & kbShift) tk = translate_vkey(shift_trans, vk); else
      if (shift & kbCtrlShift) tk = translate_vkey(ctrl_trans, vk); else
         if (shift & kbAltShift) tk = translate_vkey(alt_trans, vk); else
            tk = translate_vkey(plain_trans, vk);
   if (tk!=0) {
      keyDown.keyCode = tk;
      keyDown.controlKeyState = shift;
   }
#if 0
   static FILE *fp = NULL;
   if (fp == NULL) fp = fopen("keys.out","w");
   fprintf(fp,"vcode=%x vscan=%x ascii=%c shift=%d keycode=%x\n",
           irp->Event.KeyEvent.wVirtualKeyCode,
           irp->Event.KeyEvent.wVirtualScanCode,
           irp->Event.KeyEvent.uChar.AsciiChar,
           getShiftState(),
           keyDown.keyCode);
   printf("vcode=%x vscan=%x ascii=%c shift=%d keycode=%x\n",
          irp->Event.KeyEvent.wVirtualKeyCode,
          irp->Event.KeyEvent.wVirtualScanCode,
          irp->Event.KeyEvent.uChar.AsciiChar,
          getShiftState(),
          keyDown.keyCode);
#if 0
   printf("vcode=%x vscan=%x ascii=%c keycode=%x VkKeyScan=%x OemKeyScan=%x\n",
          irp->Event.KeyEvent.wVirtualKeyCode,
          irp->Event.KeyEvent.wVirtualScanCode,
          irp->Event.KeyEvent.uChar.AsciiChar,
          keyDown.keyCode,
          VkKeyScan(irp->Event.KeyEvent.uChar.AsciiChar),
          OemKeyScan(irp->Event.KeyEvent.uChar.AsciiChar));
#endif
#endif
}

#endif // __NT__

//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------

#ifdef __QSINIT__
void TEventQueue::mouseInt() {
}

static int isLetterKey(uchar scancode) {
   return scancode>=0x10 && scancode<=0x19 ||
          scancode>=0x1E && scancode<=0x26 ||
          scancode>=0x2C && scancode<=0x32 ? True : False;
}

static int isFnKey(uchar scancode) {
   return scancode>=0x3b && scancode<=0x44 ? True : False;
}

void TEvent::getKeyEvent() {
   if (!key_pressed()) {
      what = evNothing;
   } else {
      what = evKeyDown;
      keyDown.keyCode = key_read();
      keyDown.controlKeyState = getShiftState();
      // ignore extended keyboard
      if (keyDown.charScan.charCode==0xE0) keyDown.charScan.charCode = 0;
      // drop scan code to make Ctrl-char working
      if (keyDown.charScan.charCode>=kbCtrlA &&
          keyDown.charScan.charCode<=kbCtrlZ &&
          isLetterKey(keyDown.charScan.scanCode)) keyDown.charScan.scanCode = 0;
      // some EFI fixes ;)
      if (isFnKey(keyDown.charScan.scanCode)) keyDown.charScan.charCode = 0;
      //printf("keyCode=%04X\n",keyDown.keyCode);
      switch (keyDown.charScan.scanCode) {
         case scSpaceKey:
            if (keyDown.controlKeyState&kbAltShift) keyDown.keyCode = kbAltSpace;
            break;
         case 0x92:
         case scInsKey: {
            unsigned char state = getShiftState();
            if (keyDown.controlKeyState&kbCtrlShift) keyDown.keyCode = kbCtrlIns;
               else 
            if (keyDown.controlKeyState&(kbLeftShift|kbRightShift))
               keyDown.keyCode = kbShiftIns;
            break;
         }
         case 0x93:
         case scDelKey: {
            if (keyDown.controlKeyState&kbCtrlShift) keyDown.keyCode = kbCtrlDel;
               else 
            if (keyDown.controlKeyState&(kbLeftShift|kbRightShift))
               keyDown.keyCode = kbShiftDel;
            break;
         }
      }
   }
}

#endif // __QSINIT__
