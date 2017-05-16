/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   TTYPES.H                                                              */
/*                                                                         */
/*   Copyright (c) Borland International 1991                              */
/*   All Rights Reserved.                                                  */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if !defined( __TTYPES_H )
#define __TTYPES_H

#if defined(_MSC_VER)
#undef uchar
#endif

enum Boolean { False, True };

typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned long ulong;

const char EOS = '\0';

enum StreamableInit { streamableInit };

class ipstream;
class opstream;
class TStreamable;
class TStreamableTypes;

typedef int ccIndex;
typedef Boolean(*ccTestFunc)(void *, void *);
typedef void (*ccAppFunc)(void *, void *);

const int ccNotFound = -1;

extern const uchar specialChars[];


#endif  // __TTYPES_H
