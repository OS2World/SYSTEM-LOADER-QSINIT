/*
 *              TTerminal::prevLines()
 *
 *              Rewritten on C++ by Ilfak Guilfanov
 *                      26.06.95
 *
 */

#define Uses_TTerminal
#include <tv.h>

#define DECPTR                          \
   {                                       \
      if ( ptr <= buffer ) ptr += bufSize;  \
      ptr--;                                \
   }

#define INCPTR                          \
   {                                       \
      ptr++;                                \
      if ( ptr >= buffer+bufSize ) ptr = buffer; \
   }

//------------------------------------------------------------------------
size_t TV_CDECL TTerminal::prevLines(size_t pos, int lines) {
   char *ptr = buffer + pos;
   if (lines != 0) {
      if (buffer+queBack == ptr) return queBack;
      DECPTR
      while (1) {
         //loop:
         int counter;
         if (ptr <= buffer+queBack)
            counter = ptr - buffer;
         else
            counter = ptr - (buffer+queBack);
         counter++;
         while (1) {
            if (*ptr-- == '\n') {
               lines--;
               if (lines == 0) goto ret;
               break;
            }
            counter--;
            if (counter == 0) {
               if (ptr-buffer+1 == queBack) return queBack;
               ptr = buffer + bufSize;
               break;
            }
         }
      }
   }
ret:
   INCPTR
   INCPTR
   return ptr-buffer;
}
