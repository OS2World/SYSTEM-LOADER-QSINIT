//
// QSINIT API
// fast memory manager
//
#ifndef QSINIT_EXTMEMMGR
#define QSINIT_EXTMEMMGR

/// @name mem_setopts() Flags
//@{
#define QSMEMMGR_PARANOIDALCHK 0x00000002  ///< check integrity at every call (ULTRA SLOW!)
#define QSMEMMGR_ZEROMEM       0x00000004  ///< zero blocks on alloc and realloc
#define QSMEMMGR_HALTONNOMEM   0x00000008  ///< halt on no mem
//@}

#ifdef __cplusplus
extern "C" {
#endif

void          __stdcall mem_setopts(long Options);
unsigned long __stdcall mem_getopts(void);

/** heap alloc.
    Owner is value in range 0..0xffffBfff (with some exceptions below)
    Pool  is value in range 0..0xfffffffE

    Some Owner values are reserved for system needs (see QSMEMOWNER_* consts
    below) and cannot be used. This is critical note, because batch free
    function mem_freepool() is used for it and any user block with the same
    Owner value will be gone as well */
void*         __stdcall mem_alloc(long Owner, long Pool, unsigned long Size);
/// the same as mem_alloc() by zero-fill it
void*         __stdcall mem_allocz(long Owner, long Pool, unsigned long Size);
void*         __stdcall mem_realloc(void*, unsigned long);
void          __stdcall mem_free(void*);
void*         __stdcall mem_dup(void*);

void          __stdcall mem_zero(void*);
/// query block size (without header, but rounded to 16/256)
unsigned long __stdcall mem_blocksize(void *M);

/** query block size and Owner/Pool.
    Unlike mem_blocksize(), this function returns 0 on incorrect pointer
    instead of going to panic screen.
    @param  M             memory block
    @param  [out] Owner   Owner value, can be 0.
    @param  [out] Pool    Pool value, can be 0.
    @return block size or 0 if pointer is incorrect. */
unsigned long __stdcall mem_getobjinfo(void *M, long *Owner, long *Pool);

/** change object`s Owner & Pool.
    Function is not recommended at all, because many Owner & Pool
    combinations are reserved and outside code can free this block by
    batch free functions (memFreePool, memFreeByOwner).
    @param  M             memory block
    @param  Owner         new Owner value, cannot be -1
    @param  Pool          new Pool value, cannot be -1.
    @return success flag = 1/0. */
int           __stdcall mem_setobjinfo(void *M, long Owner, long Pool);

/// get unique id. Owner is always==-2, and Pool is unique.
void          __stdcall mem_uniqueid(long *Owner, long *Pool);

/// return number of freed blocks
unsigned long __stdcall mem_freepool(long Owner, long Pool);
/// return number of freed blocks
unsigned long __stdcall mem_freeowner(long Owner);

void          __stdcall mem_dumplog(const char *TitleString);
int           __stdcall mem_checkmgr();

/// print short statistic. Warning!! it gets ~1kb in stack
void          __stdcall mem_stat();
/** print topcount top memory users to log.
    @attention it temporary gets x*64k of system memory. */
void          __stdcall mem_statmax(int topcount);

/// @name some known system owner values (do not use it!!!)
//@{
#define QSMEMOWNER_MODLDR      0x4243444D  ///< LX module loader
#define QSMEMOWNER_TRACE       0x42435254  ///< trace buffers
#define QSMEMOWNER_MTLIB       0x4243544D  ///< MTLIB internals
#define QSMEMOWNER_COLIB       0x42434F4C
#define QSMEMOWNER_COPROCESS   0x42434F50
#define QSMEMOWNER_LINENUM     0xFFFFE000  ///< 0xFFFFE000..0xFFFFFFFE
#define QSMEMOWNER_COTHREAD    0xFFFFD000  ///< 0xFFFFD000..0xFFFFDFFF
#define QSMEMOWNER_UNOWNER     0xFFFFC000
//@}

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXTMEMMGR
