//
// QSINIT API
// fast memory manager
//
#ifndef QSINIT_EXTMEMMGR
#define QSINIT_EXTMEMMGR

/// @name memSetOptions Flags
//@{
#define QSMEMMGR_PARANOIDALCHK 0x00000002  ///< check integrity at every call (ULTRA SLOW!)
#define QSMEMMGR_ZEROMEM       0x00000004  ///< zero blocks on alloc and realloc
#define QSMEMMGR_HALTONNOMEM   0x00000008  ///< halt on no mem
//@}

#ifdef __cplusplus
extern "C" {
#endif

void          __stdcall memInit(void);
void          __stdcall memDone(void);

void          __stdcall memSetOptions(long Options);
unsigned long __stdcall memGetOptions(void);

void*         __stdcall memAlloc(long Owner,long Pool,unsigned long Size);
void*         __stdcall memAllocZ(long Owner,long Pool,unsigned long Size);
void*         __stdcall memRealloc(void*,unsigned long);
void          __stdcall memFree(void*);
void*         __stdcall memDup(void*);

void          __stdcall memZero(void*);
/// query block size (without header, but rounded to 16/256)
unsigned long __stdcall memBlockSize(void *M);

/** query block size and Owner/Pool.
    Unlike memBlockSize, this function return 0 on incorrect pointer and
    does not go to panic screen.
    @param  M             memory block
    @param  [out] Owner   Owner value, can be 0.
    @param  [out] Pool    Pool value, can be 0.
    @return block size or 0 if pointer is incorrect. */
unsigned long __stdcall memGetObjInfo(void *M,long *Owner,long *Pool);

/// get unique id. Owner is always==-2, and Pool is unique.
void          __stdcall memGetUniqueID(long *Owner,long *Pool);

/// return number of freed blocks
unsigned long __stdcall memFreePool(long Owner,long Pool);
/// return number of freed blocks
unsigned long __stdcall memFreeByPool(long Pool);
/// return number of freed blocks
unsigned long __stdcall memFreeByOwner(long Owner);
void          __stdcall memFreeAll();

void          __stdcall memDumpLog(const char *TitleString);
int           __stdcall memCheckMgr();

/// print short statistic. Warning!! it get ~1kb in stack
void          __stdcall memStat();
/// print topcount top memory users. Warning!! it temporary get x*64k of system memory
void          __stdcall memStatMax(int topcount);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXTMEMMGR
