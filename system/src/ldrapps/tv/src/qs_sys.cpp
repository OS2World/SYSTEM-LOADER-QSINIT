#define Uses_TKeys
#include <tv.h>
#include <tvdir.h>

#include <stdlib.h>
#include <vio.h>
#include <qsutil.h>

unsigned long getTicks() {
   // return a value that can be used as a substitute for the DOS Ticker at [0040:006C]
   return tm_counter();
}

unsigned char getShiftState() {
   u32t st = key_status();
   u8t  rc = 0;
   if ((st&KEY_ALT)||(st&KEY_ALTLEFT)||(st&KEY_ALTRIGHT)) rc|=kbAltShift;
   if ((st&KEY_CTRL)||(st&KEY_CTRLLEFT)||(st&KEY_CTRLRIGHT)) rc|=kbCtrlShift;
   if ((st&KEY_SHIFT)&&(st&KEY_SHIFTLEFT)) rc|=kbLeftShift;
   if ((st&KEY_SHIFT)&&(st&KEY_SHIFTRIGHT)) rc|=kbRightShift;
   if ((st&KEY_NUMLOCK)!=0) rc|=kbNumState;
   if ((st&KEY_SCROLLLOCK)!=0) rc|=kbScrollState;
   if ((st&KEY_CAPSLOCK)!=0) rc|=kbCapsState;
   return rc;
}
