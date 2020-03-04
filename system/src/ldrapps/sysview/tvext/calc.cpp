/*------------------------------------------------------------*/
/*                                                            */
/*   Turbo Vision 1.0                                         */
/*   Copyright (c) 1991 by Borland International              */
/*                                                            */
/*   Calc.cpp:  TCalcDisplay member functions and TCalculator */
/*              constructor                                   */
/*------------------------------------------------------------*/

#define Uses_TRect
#define Uses_TEvent
#define Uses_TButton
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TView
#define Uses_TDialog
#include <tv.h>
#include <string.h>
#include <ctype.h>
#include "calc.h"
#ifdef __QSINIT__
#define ATOI64     atoi64
#define STRTOUL64  strtoull
#else
#include "sp_defs.h"
#endif

#define cpCalcPalette "\x13"
#define cpLightButton "\x11\x0B\x0C\x0D\x12\x12\x12\x0F"

//
// TCalcDisplay functions
//
TCalcDisplay::TCalcDisplay(TRect &r) : TView(r) {
   options  |= ofSelectable;
   eventMask = (evKeyboard | evBroadcast);
   number    = new char[DISPLAYLEN];
   cvtbase   = 0;
   memory    = 0;
   clear();
}

TCalcDisplay::~TCalcDisplay() {
   delete number;
}

void TCalcDisplay::handleEvent(TEvent &event) {
   TView::handleEvent(event);

   switch (event.what) {
   case evKeyboard:
      calcKey(event.keyDown.charScan.charCode);
      clearEvent(event);
      break;
   case evBroadcast:
      if (event.message.command == cmCalcButton) {
         TButton *bt = (TButton *)event.message.infoPtr;
         char     ch = bt->title[0];
         if (ch=='~') ch = bt->title[1];
         calcKey(ch);
         clearEvent(event);
      }
      break;
   }
}

TPalette& TCalcDisplay::getPalette() const {
   static TPalette palette(cpCalcPalette, sizeof(cpCalcPalette)-1);
   return palette;
}

void TCalcDisplay::draw() {
   char color = getColor(1);
   int i;
   TDrawBuffer buf;

   i = size.x - strlen(number) - 2;
   buf.moveChar(0, ' ', color, size.x);
   if (cvtbase)
      buf.moveChar(1, cvtbase==8?'o':'x', color&0xF0|0xC , 1);
   if (memory)
      buf.moveChar(2, 'm', color&0xF0|0xD , 1);
   buf.moveChar(i, sign, color, 1);
   buf.moveStr(i + 1, number, color);
   writeLine(0, 0, size.x, 1, buf);
}


void TCalcDisplay::error() {
   status = csError;
   strcpy(number, "Error");
   sign = ' ';
}


void TCalcDisplay::clear() {
   status = csFirst;
   strcpy(number, "0");
   sign = ' ';
   operate = '=';
}

s64t TCalcDisplay::getDisplay() { 
   return cvtbase ? STRTOUL64(number,0,cvtbase) : ATOI64(number);
}

void TCalcDisplay::setDisplay(s64t rv) {
   int  len;
   char str[64];

   if (cvtbase) {
      sign = ' ';
      sprintf(str, cvtbase==8?"%Lo":"%LX", rv);
   } else
   if (rv < 0) {
      sign = '-';
      sprintf(str, "%Lu", -rv);
   } else {
      sign = ' ';
      sprintf(str, "%Lu", rv);
   }

   len = strlen(str) - 1;          // Minus one so we can use as an index.

   if (len > DISPLAYLEN)
      error();
   else
      strcpy(number, str);
}


void TCalcDisplay::checkFirst() {
   if (status == csFirst) {
      status = csValid;
      strcpy(number, "0");
      sign = ' ';
   }
}


void TCalcDisplay::calcKey(unsigned char key) {
   char stub[2] = " ";
   s64t   rv;

   key = toupper(key);
   if (status == csError && key != 27)
      key = ' ';

   switch (key) {
   case '8':   case '9':
      if (cvtbase==8) break;
   case '0':   case '1':   case '2':   case '3':   case '4':
   case '5':   case '6':   case '7':
      checkFirst();
      if (strlen(number) < (cvtbase==8?22:(cvtbase==16?16:19))) {
         // 19 is max s64t length, 22 is octal -1 (1777777777777777777777)
         if (strcmp(number, "0") == NULL)
            number[0] = '\0';
         stub[0] = key;
         strcat(number, stub);
      }
      break;
   case 'A':   case 'B':   case 'C':   case 'D':   case 'E':   case 'F':
      checkFirst();
      if (cvtbase==16 && strlen(number) < 16) {
         // 21 is max visible display length
         if (strcmp(number, "0") == NULL)
            number[0] = '\0';
         stub[0] = key;
         strcat(number, stub);
      }
      break;

   case 8:
      int len;

      checkFirst();
      if ((len = strlen(number)) == 1)
         strcpy(number, "0");
      else
         number[len - 1] = '\0';
      break;

   case '_':                   // underscore (keyboard version of +/-)
   case 241:                   // +/- extended character.
      if (!cvtbase) sign = (sign == ' ') ? '-' : ' ';
      break;

   case 'M':
      memory = getDisplay();
      if (sign == '-') memory = -memory;
      break;

   case 'R':
      checkFirst();
      setDisplay(memory);
      break;

   case 'X':
      rv = getDisplay();
      if (sign == '-') rv = -rv;
      cvtbase = cvtbase==16 ? 0 : 16;
      setDisplay(rv);
      break;

   case 'O':
      rv = getDisplay();
      if (sign == '-') rv = -rv;
      cvtbase = cvtbase==8 ? 0 : 8;
      setDisplay(rv);
      break;

   case '!':
      rv = getDisplay();
      if (sign == '-') rv = -rv;
      setDisplay(~rv);
      break;

   case '+':   case '-':   case '*':   case '/':
   case '^':   case '<':   case '|':   case '>':
   case '&':   case 'P':   case '%':   case '=':   case 13:
      if (status == csValid) {
         status = csFirst;
         rv = getDisplay();
         if (sign == '-') rv = -rv;

         switch (operate) {
            case '+': setDisplay(operand + rv); break;
            case '-': setDisplay(operand - rv); break;
            case '*': setDisplay(operand * rv); break;
            case '^': setDisplay(operand ^ rv); break;
            case '|': setDisplay(operand | rv); break;
            case '&': setDisplay(operand & rv); break;
            case '<': setDisplay(operand << rv); break;
            case '>': setDisplay(operand >> rv); break;
            case 'P':
               if (rv==0) operand = 1; else {
                  s64t pw = operand;
                  while (rv-->1) operand *= pw;
               }
               setDisplay(operand); 
               break;
            case '/':
            case '%': if (rv == 0) error();
               else setDisplay(operate=='%' ? operand % rv : operand / rv);
               break;
         }
      }
      operate = key;
      operand = getDisplay();
      if (sign == '-') operand = -operand;
      break;

   case 27:
      clear();
      break;

   }
   drawView();
}


class TLightButton : public TButton {
public:
   TLightButton(const TRect &bounds, const char *aTitle,
      ushort aCommand, ushort aFlags) : TButton(bounds, aTitle, aCommand, aFlags)
         {}

   virtual TPalette &getPalette() const {
      static TPalette palette(cpLightButton, sizeof(cpLightButton)-1);
      return palette;
   }
};

static char *keyChar[35] = {
   "C", "\x1B",  "x", "\xF1",  "M",  "R",  "=",   // 0x1B is escape, 0xF1 is +/- char.
   "7",    "8",  "9",    "E",  "F",  "^",  "<",    
   "4",    "5",  "6",    "C",  "D",  "|",  ">",    
   "1",    "2",  "3",    "A",  "B",  "&",  "p",    
   "0",    "+",  "-",    "*",  "/",  "!",  "%"
};

//
// TCalculator functions
//
TCalculatorWindow::TCalculatorWindow() :
   TAppWindow(TRect(5, 3, 44, 18), "Calculator", AppW_Calc),
   TWindowInit(&TCalculatorWindow::initFrame) 
{
   TView *tv;
   TRect  rr;
   int    ii;

   options |= ofFirstClick;
   flags   &= ~(wfZoom|wfGrow);
   growMode = 0;
   helpCtx  = hcCalculator;

   for (ii = 0; ii < 35; ii++) {
      int x = (ii % 7) * 5 + 2;
      int y = (ii / 7) * 2 + 4;
      rr = TRect(x, y, x + 5, y + 2);
      tv = ii>=6 ? new TButton(rr, keyChar[ii], cmCalcButton, bfNormal | bfBroadcast) :
              new TLightButton(rr, keyChar[ii], cmCalcButton, bfNormal | bfBroadcast);
      tv->options &= ~ofSelectable;
      insert(tv);
   }
   rr = TRect(9, 2, 36, 3);
   insert(new TCalcDisplay(rr));
}

TPalette &TCalculatorWindow::getPalette() const {
   static TPalette palette1(cpDialog, sizeof(cpDialog)-1);

   return palette1;
}
