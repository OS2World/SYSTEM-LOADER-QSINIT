#define Uses_TSystemError
#define Uses_TScreen
#include <tv.h>

#pragma argsused
#pragma off(unreferenced)

void TV_CDECL TSystemError::swapStatusLine(TDrawBuffer &tb) {
#ifdef __DOS32__
   ushort *scr = (ushort *)(TScreen::screenBuffer + TScreen::screenWidth * (TScreen::screenHeight-1) * 2);
   ushort *buf = (ushort *)&tb;
   for (int i=0; i < TScreen::screenWidth; i++) {
      ushort x = *scr;
      *scr++ = *buf;
      *buf++ = x;
   }
#endif
}
