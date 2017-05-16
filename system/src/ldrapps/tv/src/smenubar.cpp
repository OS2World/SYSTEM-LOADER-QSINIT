/*------------------------------------------------------------*/
/* filename -       smenubar.cpp                              */
/*                                                            */
/* Registeration object for the class TMenuBar                */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TMenuBar
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RMenuBar(TMenuBar::name,
                          TMenuBar::build,
                          __DELTA(TMenuBar)
                         );

#endif  // ifndef NO_TV_STREAMS
