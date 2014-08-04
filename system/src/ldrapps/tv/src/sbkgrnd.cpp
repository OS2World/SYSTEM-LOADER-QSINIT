/*------------------------------------------------------------*/
/* filename -       sbkgrnd.cpp                               */
/*                                                            */
/* Registeration object for the class TBackground             */
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

#define Uses_TStreamableClass
#define Uses_TBackground
#include <tv.h>
__link(RView)

TStreamableClass RBackground(TBackground::name,
                             TBackground::build,
                             __DELTA(TBackground)
                            );

#endif  // ifndef NO_TV_STREAMS
