/*------------------------------------------------------------*/
/* filename -       stddlg.cpp                                */
/*                                                            */
/* function(s)                                                */
/*                  Member functions of following classes     */
/*                      TFileInputLine                        */
/*                      TSortedListBox                        */
/*                      TSearchRec                            */
/*                      TFileInfoPane                         */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*                                                            */
/*                                                            */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*------------------------------------------------------------*/


#define Uses_MsgBox
#define Uses_TKeys
#define Uses_TFileInputLine
#define Uses_TEvent
#define Uses_TSortedListBox
#define Uses_TSearchRec
#define Uses_TFileInfoPane
#define Uses_TDrawBuffer
#define Uses_TFileDialog
#define Uses_TSortedCollection
#include <tv.h>

#ifndef __QSINIT__
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <io.h>
#ifndef __IBMCPP__
#include <dos.h>
#endif
#else
#include <stdlib.h>
#endif

#ifndef __BORLANDC__
struct  ftime   {
   unsigned    ft_tsec  : 5;   /* Two second interval */
   unsigned    ft_min   : 6;   /* Minutes */
   unsigned    ft_hour  : 5;   /* Hours */
   unsigned    ft_day   : 5;   /* Days */
   unsigned    ft_month : 4;   /* Months */
   unsigned    ft_year  : 7;   /* Year */
};
#endif

#ifdef __MSDOS__
char &shiftKeys = *(char *)MK_FP(0x0, 0x417);
unsigned char getShiftState(void) { return shiftKeys; }
#endif // __MSDOS__

void fexpand(char *);

#define cpInfoPane "\x1E"

TFileInputLine::TFileInputLine(const TRect &bounds, short aMaxLen) :
   TInputLine(bounds, aMaxLen) {
   eventMask = eventMask | evBroadcast;
}

void TFileInputLine::handleEvent(TEvent &event) {
   TInputLine::handleEvent(event);
   if (event.what == evBroadcast &&
       event.message.command == cmFileFocused &&
       !(state & sfSelected)
      ) {
      if ((((TSearchRec *)event.message.infoPtr)->attr & FA_DIREC) != 0) {
         strcpy(data, ((TSearchRec *)event.message.infoPtr)->name);
         strcat(data, "\\");
         strcat(data, ((TFileDialog *)owner)->wildCard);
      } else
         strcpy(data, ((TSearchRec *)event.message.infoPtr)->name);
      drawView();
   }
}

TSortedListBox::TSortedListBox(const TRect &bounds,
                               ushort aNumCols,
                               TScrollBar *aScrollBar) :
   TListBox(bounds, aNumCols, aScrollBar),
   searchPos(-1),
   shiftState(0) {
   showCursor();
   setCursor(1, 0);
}

static Boolean equal(const char *s1, const char *s2, ushort count) {
   return Boolean(strnicmp(s1, s2, count) == 0);
}

void TSortedListBox::handleEvent(TEvent &event) {
   char curString[maxViewWidth], newString[maxViewWidth];  // 256->maxViewWidth, ig 04.02.99
   void *k;
   int value, oldPos, oldValue;

   oldValue = focused;
   TListBox::handleEvent(event);
   if (oldValue != focused) searchPos = -1;

   if (event.what == evKeyDown) {
      if (event.keyDown.charScan.charCode != 0) {
         value = focused;
         if (value < range)
            getText(curString, value, sizeof(curString)-1);
         else
            *curString = EOS;
         oldPos = searchPos;
         if (event.keyDown.keyCode == kbBack) {
            if (searchPos == -1)
               return;
            curString[searchPos] = EOS;
            searchPos--;
            if (searchPos == -1)
               shiftState = getShiftState();
         } else if ((event.keyDown.charScan.charCode == '.')) {
            char *loc = strchr(curString, '.');
            if (loc == 0)
               searchPos = -1;
            else
               searchPos = ushort(loc - curString);
         } else {
            searchPos++;
            if (searchPos == 0)
               shiftState = getShiftState();
            curString[searchPos] = event.keyDown.charScan.charCode;
            curString[searchPos+1] = EOS;
         }
         k = getKey(curString);
         list()->search(k, value);
         if (value < range) {
            getText(newString, value, sizeof(newString)-1);
            if (equal(curString, newString, searchPos+1)) {
               if (value != oldValue) {
                  focusItem(value);
                  setCursor(cursor.x+searchPos, cursor.y);
               } else
                  setCursor(cursor.x+(searchPos-oldPos), cursor.y);
            } else
               searchPos = oldPos;
         } else
            searchPos = oldPos;
         if (searchPos != oldPos ||
             isalpha(event.keyDown.charScan.charCode)
            )
            clearEvent(event);
      }
   }
}

void *TSortedListBox::getKey(const char *s) {
   return (void *)s;
}

void TSortedListBox::newList(TSortedCollection *aList) {
   TListBox::newList(aList);
   searchPos = -1;
}

TFileInfoPane::TFileInfoPane(const TRect &bounds) :
   TView(bounds) {
   eventMask |= evBroadcast;
}

void TFileInfoPane::draw() {
   Boolean PM;
   TDrawBuffer b;
   ushort  color;
   ftime *time;
   char path[MAXPATH];

   strcpy(path, ((TFileDialog *)owner)->directory);
   strcat(path, ((TFileDialog *)owner)->wildCard);
   fexpand(path);

   color = getColor(0x01);
   b.moveChar(0, ' ', color, size.x);
   if (strlen(path)+3 > size.x) {
      b.moveStr(3, path+(strlen(path)-size.x+4), color);
      b.moveStr(1, "..", color);
   } else {
      b.moveStr(1, path, color);
   }
   writeLine(0, 0, size.x, 1, b);

   b.moveChar(0, ' ', color, size.x);
   b.moveStr(1, file_block.name, color);

   if (*(file_block.name) != EOS) {

      char buf[10];
      ltoa(file_block.size, buf, 10);
      b.moveStr(14, buf, color);

      time = (ftime *) &file_block.time;
      b.moveStr(25, months[time->ft_month], color);

      if (time->ft_day >= 10)
         itoa(time->ft_day, buf, 10);
      else {
         buf[0] = '0';
         itoa(time->ft_day, buf+1, 10);
      }
      b.moveStr(29, buf, color);

      b.putChar(31, ',');

      itoa(time->ft_year+1980, buf, 10);
      b.moveStr(32, buf, color);

      PM = Boolean(time->ft_hour >= 12);
      time->ft_hour %= 12;

      if (time->ft_hour == 0)
         time->ft_hour = 12;

      if (time->ft_hour >= 10)
         itoa(time->ft_hour, buf, 10);
      else {
         buf[0] = '0';
         itoa(time->ft_hour, buf+1, 10);
      }
      b.moveStr(38, buf, color);
      b.putChar(40, ':');

      if (time->ft_min >= 10)
         itoa(time->ft_min, buf, 10);
      else {
         buf[0] = '0';
         itoa(time->ft_min, buf+1, 10);
      }
      b.moveStr(41, buf, color);

      if (PM)
         b.moveStr(43, pmText, color);
      else
         b.moveStr(43, amText, color);
   }

   writeLine(0, 1, size.x, 1, b);
   b.moveChar(0, ' ', color, size.x);
   writeLine(0, 2, size.x, size.y-2, b);
}

TPalette &TFileInfoPane::getPalette() const {
   static TPalette palette(cpInfoPane, sizeof(cpInfoPane)-1);
   return palette;
}

void TFileInfoPane::handleEvent(TEvent &event) {
   TView::handleEvent(event);
   if (event.what == evBroadcast && event.message.command == cmFileFocused) {
      file_block = *((TSearchRec *)(event.message.infoPtr));
      drawView();
   }
}

#ifndef NO_TV_STREAMS
TStreamable *TFileInfoPane::build() {
   return new TFileInfoPane(streamableInit);
}
#endif  // ifndef NO_TV_STREAMS
