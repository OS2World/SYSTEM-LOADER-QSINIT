//
// QSINIT
// le/lx module common runtime code (both exe & dll)
//

#include "clib.h"

extern u32t _RandSeed;
u32t __stdcall random(u32t range);

int  __stdcall rand(void) { return random(32768); }

void __stdcall srand(u32t seed) { _RandSeed = seed; }
