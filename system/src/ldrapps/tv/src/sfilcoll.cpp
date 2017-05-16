/*------------------------------------------------------------*/
/* filename -       sfilcoll.cpp                              */
/*                                                            */
/* Registeration object for the class TFileCollection         */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#define Uses_TFileCollection
#define Uses_TStreamableClass
#include <tv.h>

TStreamableClass RFileCollection(TFileCollection::name,
                                 TFileCollection::build,
                                 __DELTA(TFileCollection)
                                );

#endif  // ifndef NO_TV_STREAMS
