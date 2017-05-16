/*------------------------------------------------------------*/
/* filename -       sdircoll.cpp                              */
/*                                                            */
/* Registeration object for the class TDirCollection          */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TDirCollection
#define Uses_TDirEntry
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RDirCollection(TDirCollection::name,
                                TDirCollection::build,
                                __DELTA(TDirCollection)
                               );

#endif  // ifndef NO_TV_STREAMS
