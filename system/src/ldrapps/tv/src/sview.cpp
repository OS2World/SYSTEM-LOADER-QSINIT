/*------------------------------------------------------------*/
/* filename -       sview.cpp                                 */
/*                                                            */
/* Registeration object for the class TView                   */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TView
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RView(TView::name,
                       TView::build,
                       __DELTA(TView)
                      );

#endif  // ifndef NO_TV_STREAMS
