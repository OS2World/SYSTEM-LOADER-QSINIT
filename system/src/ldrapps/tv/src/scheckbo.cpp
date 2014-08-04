/*------------------------------------------------------------*/
/* filename -       scheckbo.cpp                              */
/*                                                            */
/* Registeration object for the class TCheckBoxes             */
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

#define Uses_TCheckBoxes
#define Uses_TStreamableClass
#include <tv.h>
__link(RCluster)

TStreamableClass RCheckBoxes(TCheckBoxes::name,
                             TCheckBoxes::build,
                             __DELTA(TCheckBoxes)
                            );

#endif  // ifndef NO_TV_STREAMS
