/*------------------------------------------------------------*/
/* filename -       sfinputl.cpp                              */
/*                                                            */
/* Registeration object for the class TFileInputLine          */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#ifdef __WATCOMC__
#pragma off(unreferenced)
#endif

#define Uses_TFileInputLine
#define Uses_TStreamableClass
#include <tv.h>
__link(RInputLine)

TFileInputLine::TFileInputLine(StreamableInit) :
   TInputLine(streamableInit) {
}

TStreamable *TFileInputLine::build() {
   return new TFileInputLine(streamableInit);
}

TStreamableClass RFileInputLine(TFileInputLine::name,
                                TFileInputLine::build,
                                __DELTA(TFileInputLine)
                               );

#endif  // ifndef NO_TV_STREAMS
