/*------------------------------------------------------------*/
/* filename -       sscrolle.cpp                              */
/*                                                            */
/* Registeration object for the class TScroller               */
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

#define Uses_TScroller
#define Uses_TStreamableClass
#include <tv.h>
__link(RView)

TStreamableClass RScroller(TScroller::name,
                           TScroller::build,
                           __DELTA(TScroller)
                          );

#endif  // ifndef NO_TV_STREAMS
