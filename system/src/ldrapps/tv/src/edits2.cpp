// edits.cpp defines functions previously found in edits.asm

#define Uses_TEditor
#include <tv.h>

char TV_CDECL TEditor::bufChar(size_t p) {
   return buffer[p + ((p >= curPtr) ? gapLen : 0)];
}

size_t TV_CDECL TEditor::bufPtr(size_t p) {
   return (p >= curPtr) ? p + gapLen : p;
}

void TV_CDECL TEditor::formatLine(void *DrawBuf, size_t LinePtr,
                                  int Width, ushort Color) {
   register int i = 0;       // index in the DrawBuf
   register size_t p = LinePtr; // index in the Buffer
   ushort curColor;
   while ((p < curPtr) && (buffer[p] != 0x0D) && (i <= Width)) {
      curColor = (p>=selStart && p<selEnd) ? (Color & 0xFF00) : ((Color & 0xFF) << 8);
      if (buffer[p] == 0x9) {
         do {
            ((ushort *) DrawBuf) [i] = ' ' + curColor;
            i++;
         } while ((i % 8) && (i <= Width));
         p++;
      } else {
         ((ushort *) DrawBuf) [i] = (unsigned char)buffer[p] + curColor;
         p++; i++;
      }
   }

   if (p >= curPtr) {
      p += gapLen;

      while ((p < bufSize) && (buffer[p] != 0x0D) && (i <= Width)) {
         curColor = ((p-gapLen)>=selStart && (p-gapLen)<selEnd)
                    ? (Color & 0xFF00)
                    : ((Color & 0xFF) << 8);
         if (buffer[p] == 0x9) {
            do {
               ((ushort *) DrawBuf) [i] = ' ' + curColor;
               i++;
            } while ((i % 8) && (i <= Width));
            p++;
         } else {
            ((ushort *) DrawBuf) [i] = (unsigned char)buffer[p] + curColor;
            p++; i++;
         }
      }
      p -= gapLen;
   }

   curColor = (p>=selStart && p<selEnd) ? (Color & 0xFF00) : ((Color & 0xFF) << 8);
   while (i <= Width) {
      ((ushort *) DrawBuf) [i] = ' ' + curColor;
      i++;
   }
}

size_t TV_CDECL TEditor::lineEnd(size_t p) {
   /*
       while (p < curPtr)
           if (buffer[p] == 0x0D)
               return p;
           else
               p++;

       if (curPtr == bufLen)
           return curPtr;

       while (p + gapLen < bufLen)
           if (buffer[p + gapLen] == 0x0D)
               return p;
           else
               p++;

       return p;
   */
   if (p < curPtr) {
      while (p < curPtr)
         if (buffer[p] == 0x0D)
            return p;
         else
            p++;

      if (curPtr == bufLen)
         return bufLen;


   } else {
      if (p == bufLen)
         return bufLen;
   }

   while (p + gapLen < bufSize)
      if (buffer[p + gapLen] == 0x0D)
         return p;
      else
         p++;

   return p;

}

size_t TV_CDECL TEditor::lineStart(size_t p) {
   /*
       while (p - gapLen > curPtr)
           if (buffer[--p + gapLen] == 0x0D)
               return p + 2;

       if (curPtr == 0)
           return 0;

       while (p > 0)
           if (buffer[--p] == 0x0D)
               return p + 2;

       return 0;
   */
   while (p > curPtr)
      if (buffer[--p + gapLen] == 0x0D)
         if (p+1 == bufLen || buffer[p+gapLen+1] != 0x0A)
            return p + 1;
         else
            return p + 2;

   if (curPtr == 0)
      return 0;

   while (p > 0)
      if (buffer[--p] == 0x0D)
         if (p+1 == bufLen || p+1 == curPtr || buffer[p+1] != 0x0A)
            return p + 1;
         else
            return p + 2;

   return 0;
}

size_t TV_CDECL TEditor::nextChar(size_t p) {
   if (p == bufLen)   return p;
   if (++p == bufLen) return p;

   int t = (p >= curPtr) ? p + gapLen : p;

   return ((buffer [t-1] == 0x0D) && (buffer [t] == 0x0A)) ? p + 1 : p;
}

size_t TV_CDECL TEditor::prevChar(size_t p) {
   if (p == 0)   return p;
   if (--p == 0) return p;

   int t = (p >= curPtr) ? p + gapLen : p;

   return ((buffer [t-1] == 0x0D) && (buffer [t] == 0x0A)) ? p - 1 : p;
}


