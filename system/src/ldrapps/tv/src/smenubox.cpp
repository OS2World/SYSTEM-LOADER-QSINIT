/*------------------------------------------------------------*/
/* filename -       smenubox.cpp                              */
/*                                                            */
/* Registeration object for the class TMenuBox                */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TMenuBox
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RMenuBox(TMenuBox::name,
                          TMenuBox::build,
                          __DELTA(TMenuBox)
                         );

#endif  // ifndef NO_TV_STREAMS
