/*------------------------------------------------------------*/
/* filename -       tstatict.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TStaticText member functions              */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TStaticText
#define Uses_TDrawBuffer
#define Uses_opstream
#define Uses_ipstream
#include <tv.h>

#include <ctype.h>
#include <string.h>

#define cpStaticText "\x06"

#ifdef __DOS16__
#define TEXTBUF_SIZE    256
#else
#define TEXTBUF_SIZE    (4096+1)
#endif

TStaticText::TStaticText(const TRect &bounds, const char *aText) :
   TView(bounds),
   text(newStr(aText)) 
{
   growMode |= gfFixed;
}

TStaticText::~TStaticText() {
   delete(char *)text;
}

void TStaticText::draw() {
   uchar color;
   Boolean center;
   int i, j, l, p, y;
   TDrawBuffer b;
   char s[TEXTBUF_SIZE];

   color = getColor(1);
   getText(s);
   l = strlen(s);
   p = 0;
   y = 0;
   center = False;
   while (y < size.y) {
      b.moveChar(0, ' ', color, size.x);
      if (p < l) {
         if (s[p] == 3) {
            center = True;
            ++p;
         }
         i = p;
         do {
            j = p;
            while ((p < l) && (s[p] == ' '))
               ++p;
            while ((p < l) && (s[p] != ' ') && (s[p] != '\n'))
               ++p;
         } while ((p < l) && (p < i + size.x) && (s[p] != '\n'));
         if (p > i + size.x)
            if (j > i)
               p = j;
            else
               p = i + size.x;
         if (center == True)
            j = (size.x - p + i) / 2 ;
         else
            j = 0;
         b.moveBuf(j, &s[i], color, (p - i));
         while ((p < l) && (s[p] == ' '))
            p++;
         if ((p < l) && (s[p] == '\n')) {
            center = False;
            p++;
            if ((p < l) && (s[p] == 10))
               p++;
         }
      }
      writeLine(0, y++, size.x, 1, b);
   }
}

TPalette &TStaticText::getPalette() const {
   static TPalette palette(cpStaticText, sizeof(cpStaticText)-1);
   return palette;
}

void TStaticText::getText(char *s) {
   if (text == 0)
      *s = EOS;
   else {
      strncpy(s, text, TEXTBUF_SIZE);
      s[TEXTBUF_SIZE-1] = EOS;
   }
}

#ifndef NO_TV_STREAMS
void TStaticText::write(opstream &os) {
   TView::write(os);
   os.writeString(text);
}

void *TStaticText::read(ipstream &is) {
   TView::read(is);
   text = is.readString();
   return this;
}

TStreamable *TStaticText::build() {
   return new TStaticText(streamableInit);
}

TStaticText::TStaticText(StreamableInit) : TView(streamableInit) {
}
#endif  // ifndef NO_TV_STREAMS
