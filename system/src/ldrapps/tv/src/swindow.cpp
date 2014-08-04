/*------------------------------------------------------------*/
/* filename -       swindow.cpp                               */
/*                                                            */
/* Registeration object for the class TWindow                 */
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

#define Uses_TWindow
#define Uses_TStreamableClass
#include <tv.h>
__link(RGroup)
__link(RFrame)

TStreamableClass RWindow(TWindow::name,
                         TWindow::build,
                         __DELTA(TWindow)
                        );

#endif  // ifndef NO_TV_STREAMS
