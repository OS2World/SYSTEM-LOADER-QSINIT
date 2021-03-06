/*------------------------------------------------------------*/
/* filename -       sfinfpne.cpp                              */
/*                                                            */
/* Registeration object for the class TFileInfoPane           */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TFileInfoPane
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RFileInfoPane(TFileInfoPane::name,
                               TFileInfoPane::build,
                               __DELTA(TFileInfoPane)
                              );

#endif  // ifndef NO_TV_STREAMS
