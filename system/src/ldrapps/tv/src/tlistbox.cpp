/*------------------------------------------------------------*/
/* filename -       tlistbox.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TListBox member functions                 */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TListBox
#define Uses_TEvent
#define Uses_TCollection
#define Uses_opstream
#define Uses_ipstream
#include <tv.h>

#include <string.h>

TListBox::TListBox(const TRect &bounds,
                   ushort aNumCols,
                   TScrollBar *aScrollBar) :
   TListViewer(bounds, aNumCols, 0, aScrollBar),
   items(0) 
{
   setRange(0);
}

TListBox::~TListBox() {
}

size_t TListBox::dataSize() {
   return sizeof(TListBoxRec);
}

void TListBox::getData(void *rec) {
   TListBoxRec *p = (TListBoxRec *)rec;
   p->items = items;
   p->selection = focused;
}

void TListBox::getText(char *dest, int item, int maxChars) {
   if (items != 0) {
      strncpy(dest, (const char *)(items->at(item)), maxChars);
      dest[maxChars] = '\0';
   } else
      *dest = EOS;
}

void TListBox::newList(TCollection *aList) {
   destroy(items);
   items = aList;
   if (aList != 0)
      setRange(aList->getCount());
   else
      setRange(0);
   if (range > 0)
      focusItem(0);
   drawView();
}

void TListBox::setData(void *rec) {
   TListBoxRec *p = (TListBoxRec *)rec;
   newList(p->items);
   focusItem(p->selection);
   drawView();
}

#ifndef NO_TV_STREAMS
void TListBox::write(opstream &os) {
   TListViewer::write(os);
   os << items;
}

void *TListBox::read(ipstream &is) {
   TListViewer::read(is);
   is >> items;
   return this;
}

TStreamable *TListBox::build() {
   return new TListBox(streamableInit);
}

TListBox::TListBox(StreamableInit) : TListViewer(streamableInit) {
}
#endif  // ifndef NO_TV_STREAMS
