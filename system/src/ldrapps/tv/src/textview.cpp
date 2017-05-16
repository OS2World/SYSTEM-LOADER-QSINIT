/*-------------------------------------------------------------*/
/* filename -       textview.cpp                               */
/*                                                             */
/* function(s)                                                 */
/*                  TTerminal and TTextDevice member functions */
/*-------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TTextDevice
#define Uses_TTerminal
#define Uses_TEvent
#define Uses_otstream
#include <tv.h>

#include <string.h>

TTextDevice::TTextDevice(const TRect &bounds,
                         TScrollBar *aHScrollBar,
                         TScrollBar *aVScrollBar) :
   TScroller(bounds,aHScrollBar,aVScrollBar)
{
}

#if defined(__OS2__) && defined(__BORLANDC__)
int _RTLENTRY TTextDevice::overflow(int c)
#else
int TTextDevice::overflow(int c)
#endif
{
   if (c != EOF) {
      char b = c;
      do_sputn(&b, 1);
   }
   return 1;
}

TTerminal::TTerminal(const TRect &bounds,
                     TScrollBar *aHScrollBar,
                     TScrollBar *aVScrollBar,
                     size_t aBufSize) :
   TTextDevice(bounds, aHScrollBar, aVScrollBar),
   getLineColor(0),
   queFront(0),
   queBack(0),
   curLineWidth(0)  // Added for horizontal cursor tracking.
{
   growMode = gfGrowHiX + gfGrowHiY;
   bufSize = min(maxTerminalSize, aBufSize);
   buffer = new char[ bufSize ];
   setLimit(0, 1);
   setCursor(0, 0);
   showCursor();
}

void TTerminal::shutDown() {
   delete buffer;
   bufSize = 0;
   buffer  = 0;
   TTextDevice::shutDown();
}

void TTerminal::bufDec(size_t &val) {
   if (val == 0)
      val = bufSize - 1;
   else
      val--;
}

void TTerminal::bufInc(size_t &val) {
   if (++val >= bufSize)
      val = 0;
}

Boolean TTerminal::canInsert(size_t amount) {
   long T = (queFront < queBack) ?
            (queFront +  amount) :
            (long(queFront) - bufSize + amount);    // cast needed so we get
   // signed comparison
   return Boolean(int(queBack) > T);
}

void TTerminal::draw() {
   short  i;
   size_t begLine, endLine;
   char s[ maxViewWidth + 1 ]; // KRW: Bug Fix/Enhancement: 95/01/05
   // Do *NOT* attempt to display more
   // than maxViewWidth chars
   // ** assume size.x <= maxViewWidth **
   ushort bottomLine = size.y + delta.y;

   if (limit.y > bottomLine) {
      endLine = prevLines(queFront, limit.y - bottomLine);
      bufDec(endLine);
   } else
      endLine = queFront;

   if (limit.y > size.y)
      i = size.y - 1;
   else {
      for (i = limit.y; i <= size.y - 1; i++)
         writeChar(0, i, ' ', 1, size.x);
      i =  limit.y -  1;
   }

   for (; i >= 0; i--) {
      // KRW: Bug fix/enhancement - handle any length line by only copying
      //   to s those characters to be displayed, which we assume will
      //   be < maxViewWidth
      //  This whole block rewritten
      memset(s, ' ', size.x);   // must display blanks if no characters!
      begLine = prevLines(endLine, 1);
      if (endLine >= begLine) {
         int T = int(endLine - begLine);
         T = min(T,sizeof(s)-2); // bugfix JS 26.11.94
         memcpy(s, &buffer[begLine], T);
         s[T] = EOS;
      } else {
         int T = int(bufSize - begLine);
         if (T>sizeof(s)-2) {         // bugfix JS 26.11.94
            memcpy(s, &buffer[begLine], sizeof(s)-2);
            s[sizeof(s)-2] = EOS;
         } else {
            memcpy(s, &buffer[begLine], T);
            if (T+endLine>sizeof(s)-2) {
               memcpy(s+T, buffer, sizeof(s)-2-T);
               s[sizeof(s)-2] = EOS;
            } else {
               memcpy(s+T, buffer, endLine);
               s[T+endLine] = EOS;
            }
         }
      }
      int col = -1;
      if (getLineColor) col = getLineColor(s);

      if (delta.x >= strlen(s))
         *s = EOS;
      else
         strcpy(s, &s[delta.x]);

      s[maxViewWidth-1] = EOS;
      if (col==-1 || *s == EOS) writeStr(0, i, s, 1); else {
         int ln = strlen(s), ln2 = ln;
         if (ln) {
            char *str = s;
            ushort pv =((ushort)mapColor(1)&0xF0|col&0x0F) << 8,
                  buf[maxViewWidth],
                  *pp = buf;
            while (*pp++ = pv+(*str++), --ln);
            writeView(0, ln2, i, buf);
         }
      }

      const size_t sl=strlen(s);
      if (sl < size.x) // bugfix JS
         writeChar(sl, i, ' ', 1, /*size.x*/ size.x-sl);
      endLine = begLine;
      bufDec(endLine);
   }
}

size_t TTerminal::nextLine(size_t pos) {
   if (pos != queFront) {
      while (buffer[pos] != '\n' && pos != queFront)
         bufInc(pos);
      if (pos != queFront)
         bufInc(pos);
   }
   return pos;
}

int TTerminal::do_sputn(const char *s, int count) {
   ushort screenWidth = limit.x;
   ushort screenLines = limit.y;
   size_t i;
   // BUG FIX - Mon 07/25/94 - EFW: Made TTerminal adjust horizontal
   // scrollbar limit too.
   for (i = 0; i < count; i++, curLineWidth++)
      if (s[i] == '\n') {
         screenLines++;

         // Check width when an LF is seen.
         if (curLineWidth > screenWidth)
            screenWidth = curLineWidth;

         // Reset width for the next line.
         curLineWidth = 0;
      }

   // One last check for width for cases where whole lines aren't
   // received, maybe just a character or two.
   if (curLineWidth > screenWidth)
      screenWidth = curLineWidth;

   while (!canInsert(count)) {
      queBack = nextLine(queBack);
      screenLines--;
   }

   if (queFront + count >= bufSize) {
      i = bufSize - queFront;
      memcpy(&buffer[queFront], s, i);
      memcpy(buffer, &s[i], count - i);
      queFront = count - i;
   } else {
      memcpy(&buffer[queFront], s, count);
      queFront += count;
   }

   setLimit(screenWidth, screenLines);
   scrollTo(0, screenLines + 1);
   i = prevLines(queFront, 1);
   if (i <= queFront)
      i = queFront - i;
   else
      i = bufSize - (i - queFront);
   setCursor(i, screenLines - delta.y - 1);
   drawView();
   return count;
}

Boolean TTerminal::queEmpty() {
   return Boolean(queBack == queFront);
}

#if !defined(NO_TV_STREAMS)
otstream::otstream(TTerminal *tt) {
   ios::init(tt);
}
#else
otstream &otstream::operator<<(char const *str) {

   if (str != NULL) tty->do_sputn(str, strlen(str));
   return(*this);
}

otstream &otstream::operator<<(char c) {
   tty->do_sputn(&c, 1);
   return(*this);
}
#endif
