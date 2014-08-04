/*------------------------------------------------------------*/
/* filename -       sstatusl.cpp                              */
/*                                                            */
/* Registeration object for the class TStatusLine             */
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

#ifndef NO_TV_STREAMS

#ifdef __WATCOMC__
#pragma off(unreferenced)
#endif

#define Uses_TStatusLine
#define Uses_TStreamableClass
#include <tv.h>
__link(RView)

TStreamableClass RStatusLine(TStatusLine::name,
                             TStatusLine::build,
                             __DELTA(TStatusLine)
                            );

#endif  // ifndef NO_TV_STREAMS
