/*------------------------------------------------------------*/
/* filename -       tparamte.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TParamText member functions               */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TParamText
#include <tv.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __DOS16__
#define TEXTBUF_SIZE    256
#else
#define TEXTBUF_SIZE    (4096+1)
#endif

TParamText::TParamText(const TRect& bounds) :
   TStaticText(bounds, 0),
#ifdef __QSINIT__
   str(0) {
#else
   str(new char [TEXTBUF_SIZE])
{
   str[0] = EOS;
#endif
}

TParamText::~TParamText() {
#ifdef __QSINIT__
   if (str) { free(str); str = 0; }
#else
   delete str;
#endif
}

void TParamText::getText(char *s) {
   if (str) strcpy(s, str); else *s = EOS;
}

int TParamText::getTextLen() {
   return str ? strlen(str) : 0;
}

void TParamText::setText(char *fmt, ...) {
   va_list ap;

   va_start(ap, fmt );
#ifdef __QSINIT__
   if (str) free(str);
   str = vsprintf_dyn(fmt, ap);
#else
   vsprintf(str, fmt, ap );
#endif
   va_end( ap );

   drawView();
}

#ifndef NO_TV_STREAMS

void TParamText::write(opstream& os) {
   TStaticText::write(os);
   os.writeString(str);
}

void *TParamText::read(ipstream& is) {
   TStaticText::read(is);
   str = is.readString();
   return this;
}

TStreamable *TParamText::build() {
   return new TParamText( streamableInit );
}

TParamText::TParamText(StreamableInit) : TStaticText(streamableInit) {
}
#endif  // ifndef NO_TV_STREAMS
