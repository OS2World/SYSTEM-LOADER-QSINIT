/*------------------------------------------------------------*/
/* filename -       tnocsstr.cpp                              */


/*           TNoCaseStringCollection member functions

TNoCaseStringCollection is a descendent of TStringCollection implementing
a list of strings sorted without regards to case.  It is used
by TCombo but is also useful wherever a caseless string collection is
required.

Only one member function is overridden:

int TNoCaseStringCollection::compare( void *key1, void *key2 )
    Provides the comparison change to implement caseless sorting.

--------------------------------------------------------------*/
#define Uses_TStringCollection
#define Uses_TSItem
#ifndef NO_TV_STREAMS
#define Uses_opstream
#define Uses_ipstream
#endif

#if defined (TV2)
#   include <tvision\tv.h>
#   include <tvision\tkeys.h>
#elif defined (TV1)
#   include <tv.h>
#   include <tkeys.h>
#else
#   error TV1 or TV2 must be defined
#endif

#if !defined( __STRING_H )
#include <string.h>
#endif

#include "tcombo.h"

TNoCaseStringCollection::TNoCaseStringCollection( short aLimit, short aDelta ) :
    TStringCollection(aLimit, aDelta)
{
}

int TNoCaseStringCollection::compare( void *key1, void *key2 ) {
   return stricmp( (char *)key1, (char *)key2 );
}

#ifndef NO_TV_STREAMS
TStreamable *TNoCaseStringCollection::build() {
   return new TNoCaseStringCollection( streamableInit );
}

const char * const near TNoCaseStringCollection::name =
         "TNoCaseStringCollection";
#endif
