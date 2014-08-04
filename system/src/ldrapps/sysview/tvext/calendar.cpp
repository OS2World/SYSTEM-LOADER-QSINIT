/*---------------------------------------------------------*/
/*                                                         */
/*   Turbo Vision 1.0                                      */
/*   Copyright (c) 1991 by Borland International           */
/*                                                         */
/*   Calendar.cpp:  TCalenderWindow member functions.      */
/*---------------------------------------------------------*/

#define Uses_TRect
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TView
#define Uses_TWindow
#include <tv.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "calendar.h"

static char *monthNames[] = { "",
   "January",  "February", "March",    "April",    "May",      "June",
   "July",     "August",   "September","October",  "November", "December"
};

static unsigned char daysInMonth[] = {
   0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

TCalendarView::TCalendarView(TRect& r) : TView( r ) {
   time_t now    = time(0);
   struct tm tme = *localtime(&now);

   options |= ofSelectable;
   eventMask |= evMouseAuto;

   year = curYear = tme.tm_year + 1900;
   month = curMonth = tme.tm_mon + 1;
   curDay = tme.tm_mday;

   drawView();
}

unsigned dayOfWeek(unsigned day, unsigned month, unsigned year) {
   int century, yr, dw;
   if (month<3) { month+=10; year--; } else month-=2;
   century = year / 100;
   yr = year % 100;
   dw = ((26*(int)month-2)/10 + (int)day + yr + yr/4 + century/4 - 2*century)%7;
   if (--dw<0) dw+=7;
   return ((unsigned)dw);
}


void TCalendarView::draw() {
   char str[28];
   char current = 1 - dayOfWeek(1, month, year);
   char    days = daysInMonth[month] + (year%4==0 && month==2?1:0);
   char   color = getColor(6),
         hcolor = (color&0xF0)+0x09,
         tcolor = 0x2F,
        ticolor = (color&0xF0)>>4|(tcolor&0xF0),
        odcolor = (color&0xF0)+0x01,
        wdcolor = (color&0xF0)+0x04;
   int  ii, jj;
   TDrawBuffer buf;

   buf.moveChar(0, ' ', color, 24);
   snprintf(str, 28,"%12s %4d \x1E \x1F", monthNames[month], year);
   buf.moveStr(0, str, hcolor);
   writeLine(0, 0, 24, 1, buf);

   buf.moveChar(0, ' ', color, 22);
   buf.moveStr(0, " Mo Tu We Th Fr Sa Su", hcolor);
   writeLine(0, 1, 24, 1, buf);

   for (ii=1; ii<=6; ii++) {
      buf.moveChar(0, ' ', color, 24);
      for (jj=0; jj<=6; jj++) {
         if (current<1 || current>days) buf.moveStr(1+jj*3, "   ", color);
         else {
            int today = year==curYear&&month==curMonth&&current==curDay,
              onechar = current<10?1:0;
            snprintf(str, 24, "%d", current);
            buf.moveStr(1+jj*3+onechar, str, today?tcolor:(jj<5?odcolor:wdcolor));
            if (today) {
               buf.moveChar(1+jj*3-1+onechar,0xDD,ticolor,1);
               buf.moveChar(1+jj*3+2,0xDE,ticolor,1);
            }
         }
         current++;
      }
      writeLine(0, ii+1, 24, 1, buf);
   }
}


void TCalendarView::handleEvent(TEvent& event) {
   TPoint point;

   TView::handleEvent(event);
   if (state && sfSelected) {
      if ((event.what&evMouse) && (evMouseDown||evMouseAuto)) {
         point = makeLocal(event.mouse.where);
         if (point.x == 18 && point.y == 0) {
            month++;
            if (month>12) { year++; month=1; }
            drawView();
         } else 
         if (point.x == 20 && point.y == 0) {
            month--;
            if (month<1) { year--; month=12; }
            drawView();
         }
      } else 
      if (event.what == evKeyboard) {
         if ((loByte(event.keyDown.keyCode) == '+') ||
            event.keyDown.keyCode==kbDown)
         {
            month++;
            if (month>12) { year++; month=1; }
         } else 
         if ((loByte(event.keyDown.keyCode) == '-') ||
            event.keyDown.keyCode==kbUp)
         {
            month--;
            if (month<1) { year--; month=12; }
         }
         drawView();
      }
   }
}

TCalendarWindow::TCalendarWindow() :
    TWindow( TRect(1, 1, 25, 11), "Calendar", wnNoNumber ),
    TWindowInit( &TCalendarWindow::initFrame )
{
   TRect rr(getExtent());

   flags &= ~(wfZoom|wfGrow);
   growMode = 0;

   palette = wpGrayWindow;//wpCyanWindow;

   rr.grow(-1, -1);
   insert(new TCalendarView(rr));
}
