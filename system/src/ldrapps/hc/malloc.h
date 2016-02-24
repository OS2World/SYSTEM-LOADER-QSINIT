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

/// change heap block info to file/line value
#define __set_block_info(p) \
   memSetObjInfo(p,__LINE__&MAXFILELINEMASK|0xFFFFE000,(long)__FILE__);

/** set block string info.
    string will be visible in heap dump as a "file name".
    @param  str     string, must be valid until end of use.
    @param  info    misc info value, 0...8191 */
#define __set_block_string(p, str, info) \
   memSetObjInfo(p,info|0xFFFFE000,(long)str);

#endif // malloc

#endif // QSINIT_MALLOC

