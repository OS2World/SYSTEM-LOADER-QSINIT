/*------------------------------------------------------------*/
/* filename -       tstrlist.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TStrListMaker member functions            */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TStringList
#define Uses_TStrIndexRec
#define Uses_TStrListMaker
#define Uses_opstream
#define Uses_ipstream
#include <tv.h>

#include <string.h>
#if !defined( __MSVC__)&&!defined(__IBMCPP__)&&!defined(__QSINIT__)
#include <mem.h>
#endif

const int MAXKEYS = 16;

TStrIndexRec::TStrIndexRec() :
   count(0) {
}

#ifndef NO_TV_STREAMS
TStrListMaker::TStrListMaker(ushort aStrSize, ushort aIndexSize) :
   strSize(aStrSize),
   indexSize(aIndexSize),
   strings(new char[aStrSize]),
   index(new TStrIndexRec[aIndexSize]),
   strPos(0),
   indexPos(0) {
}

TStrListMaker::~TStrListMaker() {
   delete strings;
   delete [] index;
}

void TStrListMaker::closeCurrent() {
   if (cur.count != 0) {
      index[indexPos++] = cur;
      cur.count = 0;
   }
}

void TStrListMaker::put(ushort key, char *str) {
   if (cur.count == MAXKEYS || key != cur.key + cur.count)
      closeCurrent();
   if (cur.count == 0) {
      cur.key = key;
      cur.offset = strPos;
   }
   int len = strlen(str);
   strings[strPos] = len;
   memmove(strings+strPos+1, str, len);
   strPos += len+1;
   cur.count++;
}

TStringList::TStringList(StreamableInit) :
   indexSize(0),
   index(0),
   basePos(0) {
}

TStringList::~TStringList() {
   delete [] index;
}

void TStringList::get(char *dest, ushort key) {
   if (indexSize == 0) {
      *dest = EOS;
      return;
   }

   TStrIndexRec *cur = index;
   while (cur->key + cur->count-1 < key && cur - index < indexSize)
      cur++;
   if ((cur->key + cur->count-1 < key) || (cur->key > key)) {
      *dest = EOS;
      return;
   }
   ip->seekg(basePos + cur->offset);
   int count = key - cur->key;
   do  {
      uchar sz = ip->readByte();
      ip->readBytes(dest, sz);
      dest[sz] = EOS;
   } while (count-- > 0);
}

void TStrListMaker::write(opstream &os) {
   closeCurrent();
   os << strPos;
   os.writeBytes(strings, strPos);
   os << indexPos;
   os.writeBytes(index, indexPos * sizeof(TStrIndexRec));
}

void *TStringList::read(ipstream &is) {
   ip = &is;

   ushort strSize;
   is >> strSize;

   basePos = is.tellg();
   is.seekg(basePos + strSize);
   is >> indexSize;
   index = new TStrIndexRec[indexSize];
   is.readBytes(index, indexSize * sizeof(TStrIndexRec));
   return this;
}

TStreamable *TStringList::build() {
   return new TStringList(streamableInit);
}
#endif  // ifndef NO_TV_STREAMS


