/*------------------------------------------------------------*/
/* filename -       smnuview.cpp                              */
/*                                                            */
/* Registeration object for the class TMenuView               */
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

#define Uses_TMenuView
#define Uses_TStreamableClass
#include <tv.h>
__link(RView)

TStreamableClass RMenuView(TMenuView::name,
                           TMenuView::build,
                           __DELTA(TMenuView)
                          );

#endif  // ifndef NO_TV_STREAMS
