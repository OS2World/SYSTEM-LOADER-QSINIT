//
// QSINIT API
// memory manager (system heap)
//
#ifndef QSINIT_EXTMEMMGR
#define QSINIT_EXTMEMMGR

#include "qstypes.h"

/// @name mem_setopts() Flags
//@{
#define QSMEMMGR_PARANOIDALCHK 0x00000002  ///< check integrity at every call (ULTRA SLOW!)
#define QSMEMMGR_ZEROMEM       0x00000004  ///< zero blocks on alloc and realloc
#define QSMEMMGR_HALTONNOMEM   0x00000008  ///< halt on no mem
//@}

#ifdef __cplusplus
extern "C" {
#endif

void  _std mem_setopts   (u32t options);
u32t  _std mem_getopts   (void);

/** heap alloc.
    Owner is value in range 0..0xffffBfff (with some exceptions below)
    Pool  is value in range 0..0xfffffffE

    Some Owner values are reserved for system needs (see QSMEMOWNER_* consts
    below) and cannot be used. This is critical note, because batch free
    function mem_freepool() is used for it and any user block with the same
    Owner value will be gone as well */
void* _std mem_alloc     (long owner, long pool, u32t size);
/// the same as mem_alloc() by zero-fill it
void* _std mem_allocz    (long owner, long pool, u32t size);
void* _std mem_realloc   (void*, u32t newsize);
void  _std mem_free      (void*);
void* _std mem_dup       (void*);

void  _std mem_zero      (void*);
/// query block size (without header, but rounded to 16/256)
u32t  _std mem_blocksize (void *blk);

/** query block size and Owner/Pool.
    Unlike mem_blocksize(), this function returns 0 on incorrect pointer
    instead of going to panic screen.
    @param  blk           memory block
    @param  [out] owner   Owner value, can be 0.
    @param  [out] pool    Pool value, can be 0.
    @return block size or 0 if pointer is incorrect. */
u32t  _std mem_getobjinfo(void *blk, long *owner, long *pool);

/** change object`s Owner & Pool.
    Function is not recommended at all, because many Owner & Pool
    combinations are reserved and outside code can free this block by
    batch free functions (mem_freepool, mem_freeowner).
    @param  blk           memory block
    @param  owner         new Owner value, cannot be -1
    @param  pool          new Pool value, cannot be -1.
    @return success flag = 1/0. */
int   _std mem_setobjinfo(void *blk, long owner, long pool);

/** get unique id.
    "owner" is constant usually and "pool" is unique, this guarantee
    ~4G of unique values */
void  _std mem_uniqueid  (long *owner, long *pool);

/** free all blocks with Owner/Pool combination over system.
    @return number of freed blocks */
u32t  _std mem_freepool  (long owner, long pool);
/** free all blocks with Owner value over system.
    @return number of freed blocks */
u32t  _std mem_freeowner (long owner);

void  _std mem_dumplog   (const char *title);
int   _std mem_checkmgr  ();

/// print short statistic. Warning!! it gets ~1kb in stack
void  _std mem_stat      ();
/** print topcount top memory users to log.
    @attention it temporary gets x*64k of system memory. */
void  _std mem_statmax   (int topcount);

typedef struct {
   u32t            ph;   ///< in: pid or module handle
   u32t           tid;   ///< in: thread id
   u32t           mem;   ///< out: memory in use
   u32t          nblk;   ///< out: # of blocks in use
} mem_statdata;

/// @name mem_statinfo() type
//@{
#define QSMEMINFO_ALL          0  ///< return complete information
#define QSMEMINFO_GLOBAL       1  ///< return global heap blocks size
#define QSMEMINFO_MODULE       2  ///< size, owned by a module (handle in info->ph)
#define QSMEMINFO_PROCESS      3  ///< size, owned by a process id (info->ph)
#define QSMEMINFO_THREAD       4  ///< size, owned by a thread (info->ph/info->tid)
//@}

/** query total memory in use - by type or owner.
    Function does not check pid/tid or handle value at all. It just will
    return zero values in this case.
    Warning! Function just walks over entire heap every time, so it really slow.

    @param infotype       Type of requested information (QSMEMINFO_* value).
    @param [in,out] info  pid/tid/handle on enter and stat data on exit. */
void  _std mem_statinfo  (u32t infotype, mem_statdata *info);

/// @name some known system owner values (do not use it!!!)
//@{
#define QSMEMOWNER_FILECC      0x42434346  ///< file cache headers
#define QSMEMOWNER_MODLDR      0x4243444D  ///< LX module loader
#define QSMEMOWNER_SESTATE     0x42434D53  ///< session data
#define QSMEMOWNER_COLIB       0x42434F4C
#define QSMEMOWNER_COPROCESS   0x42434F50
#define QSMEMOWNER_COTHREAD    0x42434F54
#define QSMEMOWNER_TRACE       0x42435254  ///< trace buffers
#define QSMEMOWNER_LINENUM     0xFFFFE000  ///< 0xFFFFE000..0xFFFFFFFE
#define QSMEMOWNER_UNOWNER     0xFFFFC000
//@}

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXTMEMMGR
