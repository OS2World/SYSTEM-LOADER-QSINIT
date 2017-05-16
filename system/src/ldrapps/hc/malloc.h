//
// QSINIT API
// malloc/free
//

/** @file malloc.h
@ingroup API2
API: malloc functions and macroses.

Heap actually is the one for entire system and processes.

But there are four types of allocation available:
@li common, belonging to current process - malloc(), realloc(), calloc().
    System will free it when process exits.
    API & clib functions returns such memory.

@li thread-owned memory, which is auto-released when THREAD exits -
    malloc_thread(), calloc_thread(), realloc_thread().

@li belonging to DLL module. This type is set by default by any malloc(),
    realloc() and free() inside DLL module code. It works as shared until
    DLL unload - then all such blocks will be released.

    @attention Note, what such type is returned by this 3 functions only,
    other clib and APIs still returns process owned memory when called from
    DLL!

@li shared over system, which can be released only by free() function:
    malloc_shared(), realloc_shared(), calloc_shared().

All these types are compatible in between and can be converted to each other
(with some limitations).

Note, what realloc() function is a special case: with zero arg it works as
malloc(), but when it changing ready block - it preserves its original type.

But realloc_shared() and realloc_thread() are <b>always</b> returns shared and
thread memory, even it wasn`t so before.

In practice, this mean, what EXE module writing supplied with automatic garbage
collection on exit. DLL module writing is a bit harder task, because API
functions returns blocks, attached to current process context, although DLL
modules are global and shared over system. This requires additional block type
setup call in some cases. */

#ifndef QSINIT_MALLOC
#define QSINIT_MALLOC

#include "memmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Watcom C runtime _msize() function analogue
#define _msize(block) mem_blocksize(block)

void* __stdcall malloc (unsigned long size);

void* __stdcall calloc (unsigned long n, unsigned long size);

void* __stdcall realloc(void *old_blk, unsigned long size);

void  __stdcall free   (void *ptr);

void* __stdcall malloc_thread(unsigned long size);

void* __stdcall calloc_thread(unsigned long n, unsigned long size);

void* __stdcall realloc_thread(void *old_blk, unsigned long size);

#define MAXFILELINEMASK (~QSMEMOWNER_LINENUM)

#define malloc_shared(size) \
  mem_alloc((__LINE__&MAXFILELINEMASK)-1|QSMEMOWNER_LINENUM, (unsigned long)__FILE__, size)

#define calloc_shared(nn,size) \
  mem_allocz((__LINE__&MAXFILELINEMASK)-1|QSMEMOWNER_LINENUM, (unsigned long)__FILE__, \
    (nn)*(size))

void* __stdcall __realloc_shared_rt(void *old_blk, unsigned long size,
                                    const char *file, unsigned long line);

#define realloc_shared(block,size) \
   __realloc_shared_rt(block, size, __FILE__, __LINE__)

/** change block type to shared.
    @return success flag (1/0) */
#define mem_shareblock(p) \
   mem_setobjinfo(p, (__LINE__&MAXFILELINEMASK)-1|QSMEMOWNER_LINENUM, (long)__FILE__);

/** change block type to owned by current process.
    @param  block    memory pointer
    @return success flag (1/0) */
int __stdcall mem_localblock(void *block);

/** change block type to owned by current thread!
    @param  block    memory pointer
    @return success flag (1/0) */
int __stdcall mem_threadblock(void *block);

/** change block type to owned by current DLL module.
    Note, what function available in EXE runtime too, and block will be
    attached to EXE module (to be free while module unload).
    @param  block    memory pointer
    @return success flag (1/0) */
int __stdcall mem_modblock(void *block);

/** set string info for shared memory block.
    String is visible in heap dump as a "file name".
    This macro also switches block type to shared if it was not so.
    @param  str     string, must be valid until the end of use.
    @param  info    misc info value, 0...8190 */
#define __set_shared_block_info(p, str, info) \
   mem_setobjinfo(p, (info)|QSMEMOWNER_LINENUM, (long)str);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_MALLOC
