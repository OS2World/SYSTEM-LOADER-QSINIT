//
// QSINIT
// some macroses for 16 bit watcom compiler
//
#ifndef QS_MACRO16W
#define QS_MACRO16W

#include "qstypes.h"

#if defined(__WATCOMC__) && defined(__X86__)

// real mode seg, off -> far pointer
char far *sotofar(u16t _seg, u16t _ofs);
#pragma aux sotofar = \
     parm [dx][ax]    \
     value [dx ax];   // do nothing ;))

// real mode seg, off -> physical address
u32t sotop(u16t _seg, u16t _ofs);
#pragma aux sotop =   \
     "mov   cx,dx"    \
     "shr   dx,12"    \
     "shl   cx,4"     \
     "add   ax,cx"    \
     "adc   dx,0"     \
     parm [dx][ax]    \
     value [dx ax]    \
     modify [cx];

// real mode seg -> physical address
u32t segtop(u16t _seg);
#pragma aux segtop =  \
     "xor   dx,dx"    \
     "shld  dx,ax,4"  \
     "shl   ax,4"     \
     parm   [ax]      \
     value [dx ax];

// physical address -> real Mode far pointer
char far *ptofar(u32t p);
#pragma aux ptofar =  \
     "shld  dx,ax,12" \
     "and   ax,0Fh"   \
     parm [dx ax]     \
     value [dx ax];

#else // watcom 16 bit
#warning Wrong compiler!
#endif

#endif // QS_MACRO16W
