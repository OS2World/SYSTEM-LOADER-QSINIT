#ifndef _TVRAND_H_
#define _TVRAND_H_

#include <time.h>
#include <stdlib.h>

inline random(int n) { return rand() % n; }
inline randomize() { srand(time(0)); }

#endif