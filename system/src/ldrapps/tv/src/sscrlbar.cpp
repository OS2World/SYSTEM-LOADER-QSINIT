/*------------------------------------------------------------*/
/* filename -       sscrlbar.cpp                              */
/*                                                            */
/* Registeration object for the class TScrollBar              */
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

#define Uses_TScrollBar
#define Uses_TStreamableClass
#include <tv.h>
__link(RView)

TStreamableClass RScrollBar(TScrollBar::name,
                            TScrollBar::build,
                            __DELTA(TScrollBar)
                           );

#endif  // ifndef NO_TV_STREAMS
