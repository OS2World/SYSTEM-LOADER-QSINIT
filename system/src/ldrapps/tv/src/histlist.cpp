/*------------------------------------------------------------*/
/* filename - histlist.cpp                                    */
/*                                                            */
/* function(s)                                                */
/*          startId                                           */
/*          historyCount                                      */
/*          historyAdd                                        */
/*          historyStr                                        */
/*          clearHistory                                      */
/*          initHistory                                       */
/*          doneHistory                                       */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

/*------------------------------------------------------------*/
/*  insertString ( => historyAdd ) behavior changed:          */
/*  Now by default the new item is added at the beginning of  */
/*  the history, not at the end. If you need adding at the    */
/*  end for some reason, then #define TV_HIST_ADD_AT_END.     */
/*------------------------------------------------------------*/
//#define TV_HIST_ADD_AT_END

#include <tv.h>

#include <string.h>
#ifndef __IBMCPP__
#ifndef __MSVC__
//#include <mem.h>
#endif
#endif
#include <stdlib.h>

class HistRec {

public:

   HistRec(uchar nId, const char *nStr);

   void *operator new(size_t);
   void *operator new(size_t, HistRec *);

   uchar id;
   ushort len;
   char str[1];

};

void *HistRec::operator new(size_t, HistRec *hr) {
   return hr;
}

void *HistRec::operator new(size_t) {
   abort();
   return 0;
}

inline HistRec::HistRec(uchar nId, const char *nStr) :
   id(nId),
   len(strlen(nStr) + 5) {
   strcpy(str, nStr);
}


inline HistRec *advance(HistRec *ptr, size_t s) {
   return (HistRec *)((char *)ptr + s);
}

inline HistRec *backup(HistRec *ptr, size_t s) {
   return (HistRec *)((char *)ptr - s);
}

inline HistRec *next(HistRec *ptr) {
   return advance(ptr, ptr->len);
}

ushort historySize = 1024;  // initial size of history block

static uchar curId;
static HistRec *curRec;
static HistRec *historyBlock;
static HistRec *lastRec;

/*
 I reimplemented this function, because it's original implementation
      return backup( ptr, ptr->len );
 simply could not work.
 It is not used by the 'normal' TV, so borland never tested it, so they
 could not realize the bug...
 It is only needed, when the new items are inserted at the beginning of the
 history (this is the default now).
*/
#ifndef TV_HIST_ADD_AT_END
HistRec *prev(HistRec *ptr) {
   HistRec *cRec = historyBlock, *nRec;
   while ((nRec = next(cRec)) != ptr)
      cRec = nRec;
   return (cRec);
}
#endif

void advanceStringPointer() {
   curRec = next(curRec);
   while (curRec < lastRec && curRec->id != curId)
      curRec = next(curRec);
   if (curRec >= lastRec)
      curRec = 0;
}

void deleteString() {
   size_t len = curRec->len;
   memmove(curRec, next(curRec), size_t((char *)lastRec - (char *)curRec));
   lastRec = backup(lastRec, len);
}

void insertString(uchar id, const char *str) {
   ushort len = strlen(str) + 5;
#ifdef TV_HIST_ADD_AT_END
   while (len > historySize - ((char *)lastRec - (char *)historyBlock)) {
      /*
         I incremented everything by 5 here, because the original code would
         'push out' the dummy record, but the other functions still would
         simply skip the 1st item.
      */
      HistRec *dst = advance(historyBlock, 5);
      HistRec *src = next(dst);
      ushort firstLen = dst->len;
      memmove(dst, src, size_t((char *)lastRec - (char *)src));
      lastRec = backup(lastRec, firstLen);
   }
   new(lastRec) HistRec(id, str);
   lastRec = next(lastRec);
#else
   while (len > historySize - ((char *)lastRec - (char *)historyBlock))
      lastRec = prev(lastRec);
   char *src = (char *)historyBlock + 5;
   char *dst = src + len;
   memmove(dst, src, size_t((char *)lastRec - src));
   lastRec = advance(lastRec, len);
   new((HistRec *)src) HistRec(id, str);
#endif
}

void startId(uchar id) {
   curId = id;
   curRec = historyBlock;
}

ushort historyCount(uchar id) {
   startId(id);
   ushort count =  0;
   advanceStringPointer();
   while (curRec != 0) {
      count++;
      advanceStringPointer();
   }
   return count;
}

void historyAdd(uchar id, const char *str) {
   if (str[0] == EOS)
      return;
   startId(id);
   advanceStringPointer();
   while (curRec != 0) {
      if (strcmp(str, curRec->str) == 0)
         deleteString();
      advanceStringPointer();
   }
   insertString(id, str);
}

const char *historyStr(uchar id, int index) {
   startId(id);
   for (short i = 0; i <= index; i++)
      advanceStringPointer();
   if (curRec != 0)
      return curRec->str;
   else
      return 0;
}

void clearHistory() {
   new(historyBlock) HistRec(0, "");
   lastRec = next(historyBlock);
}

void initHistory() {
   historyBlock = (HistRec *) new char[historySize];
   clearHistory();
}

void doneHistory() {
   delete historyBlock;
}
