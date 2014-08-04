//
// QSINIT API
// malloc/free
//

#ifndef QSINIT_MALLOC
#define QSINIT_MALLOC

#include "memmgr.h"

#ifndef malloc
#define MAXFILELINEMASK 0x1FFF

#define malloc(size) \
  memAlloc(__LINE__&MAXFILELINEMASK|0xFFFFE000,(unsigned long)__FILE__,size)

#define calloc(nn,size) \
  memAllocZ(__LINE__&MAXFILELINEMASK|0xFFFFE000,(unsigned long)__FILE__, \
    (nn)*(size))

#define free(pp) memFree(pp)

#ifdef __cplusplus
#define realloc(block,size) realloc_stub(block,size,__FILE__,__LINE__)
inline
void *realloc_stub(void *block,unsigned long newsize,char *file,int line) {
   return block?memRealloc(block,newsize):
      memAlloc(__LINE__&MAXFILELINEMASK|0xFFFFE000,(long)__FILE__,newsize);
}
#else
#define realloc(block,size) \
  (block?memRealloc(block,size) \
  :memAlloc(__LINE__&MAXFILELINEMASK|0xFFFFE000,(long)__FILE__,size))
#endif // __cplusplus

#endif // malloc

#endif // QSINIT_MALLOC
