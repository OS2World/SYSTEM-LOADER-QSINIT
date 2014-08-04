/*------------------------------------------------------------*/
/* filename -       tparamte.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TParamText member functions               */
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

#define Uses_TParamText
#define Uses_TEvent
#include <tv.h>

#include <stdio.h>
#include <stdarg.h>

TParamText::TParamText(const TRect &bounds,
                       const char *aText,
                       int aParamCount) :
   TStaticText(bounds, aText),
   paramList(0),
   paramCount(aParamCount) {
}

size_t TParamText::dataSize() {
   return paramCount * sizeof(long);
}

void TParamText::getText(char *s) {
   if (text == 0)
      *s = EOS;
   else
      //        vsprintf( s, text, (char **)paramList ); /?? VC
#if defined(__WATCOMC__)&&!defined(__QSINIT__)
      vsprintf(s, text, (char**)paramList);
#else
      vsprintf(s, text, (char *)paramList);
#endif
}

void TParamText::setData(void *rec) {
   paramList = rec;
}

#ifndef NO_TV_STREAMS
void TParamText::write(opstream &os) {
   TStaticText::write(os);
   os << paramCount;
}

void *TParamText::read(ipstream &is) {
   TStaticText::read(is);
   is >> paramCount;
   paramList = 0;
   return this;
}

TStreamable *TParamText::build() {
   return new TParamText(streamableInit);
}

TParamText::TParamText(StreamableInit) : TStaticText(streamableInit) {
}
#endif  // ifndef NO_TV_STREAMS


