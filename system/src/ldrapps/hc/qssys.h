//
// QSINIT API
// various system calls
//
#ifndef QSINIT_SYSTEMAPI
#define QSINIT_SYSTEMAPI

#ifdef __cplusplus
extern "C" {
#endif

#include "qsutil.h"

/** get interrupt vector.
    @param   [in]  vector  Vector (0..255)
    @param   [out] addr    Buffer for far 48bit address */
void     _std sys_getint(u8t vector, u64t *addr);

/** set interrupt vector.
    @param   [in]  vector  Vector (0..255)
    @param   [in]  addr    Buffer with far 48bit address
    @return boolean success flag (1/0) */
int      _std sys_setint(u8t vector, u64t *addr);

/** set task gate interrupt handler.
    @param   [in]  vector  Vector (0..255)
    @param   [in]  sel     TSS selector
    @return boolean success flag (1/0) */
int      _std sys_intgate(u8t vector, u16t sel);

/** allocate TSS selector.
    Function allocates selector pair - TSS and writable TSS+8 for
    easy access.
    @param   tssdata       TSS data (must not cross page boundary, as Intel says)
    @param   limit         TSS limit (use 0 for default)
    @return  tss selector or 0 on error. */
u16t     _std sys_tssalloc(void *tssdata, u16t limit);

/** free TSS selector.
    If next selector have the same base and limit - it will be
    freed too.
    Function will fail if TSS is current task or linked to current task.
    @param   sel           TSS selector
    @return success flag (1/0) */
int      _std sys_tssfree(u16t sel);

// include seldesc.inc to get access here
#ifdef QS_BASE_SELDESC

/** GDT descriptor setup.
    This function get ready descriptor data (unlike hlp_selsetup()).
    @param  selector  GDT offset (i.e. selector ith 0 RPL field)
    @param  desc      Descriptor data (8 bytes)
    @return bool - success flag. */
int      _std sys_seldesc(u16t sel, struct desctab_s *desc);

#endif // QS_BASE_SELDESC

/** enable/disable interrupts.
    It is good to call it with saving previous state:
@code
    int state = sys_intstate(0);
    // do something...
    sys_intstate(state);
@endcode
    @param   on      State to set (enable = 1/disable = 0)
    @return previous state. */
int      _std sys_intstate(int on);

/** query current working mode (paging/flat).
    @return 1 if paging mode active, else 0 */
int      _std sys_pagemode(void);

typedef struct {
   u64t      start;                 ///< start of memory block
   u32t      pages;                 ///< number of 4k pages in block
   u32t      flags;                 ///< block flags
} pcmem_entry;

#define PCMEM_FREE        0x0000    ///< free block
#define PCMEM_RESERVED    0x0001    ///< memory, reserved by hardware (BIOS)
#define PCMEM_QSINIT      0x0002    ///< memory, used by QSINIT memory manager
#define PCMEM_RAMDISK     0x0003    ///< memory, used by ram disk app
#define PCMEM_PHYSALLOC   0x0004    ///< memory, reserved by hlp_memisfree() call
#define PCMEM_USERAPP     0x0005    ///< memory, used by any custom app

#define PCMEM_HIDE        0x8000    ///< add this flag to hide memory from OS/2
#define PCMEM_TYPEMASK    0x7FFF

/** query PC memory above 1Mb (including entries above 4Gb limit).
    @param   freeonly   Flag to return only free memory blocks.
    @return pcmem_entry array with pages=0 in last entry. Entry must be free()-d. */
pcmem_entry * _std sys_getpcmem(int freeonly);

/** mark PC memory as used.
    This is NOT a malloc call. This function manages PC memory (for
    information reason, basically - and resolving possible conflicts).
    QSINIT self - use only blocks marked as PCMEM_QSINIT in this list.
    Other FREE memory is available for any kind of use.

    OS/2 boot will use only memory, not marked by PCMEM_HIDE flag, so
    you can hide an existing block of physical memory here.

    @param   address    Start address (64 bit).
    @param   pages      Number of 4k pages.
    @param   owner      PCMEM_* constant (except PCMEM_FREE).
    @return 0 on success or error code: EINVAL on non-aligned address, ENOENT
            if range is not intersected with PC memory and EACCES if one of
            touched blocks is used */
u32t     _std sys_markmem(u64t address, u32t pages, u32t owner);

/** unmark PC memory.
    Only PCMEM_RAMDISK..PCMEM_USERAPP entries can be unmapped.

    @param   address    Start address (64 bit).
    @param   pages      Number of 4k pages.
    @return success flag (1/0) */
u32t     _std sys_unmarkmem(u64t address, u32t pages);

/** function return end of RAM in 1st 4Gb of memory.
    There is no mapped pages in paging mode beyond this border by default
    @return page aligned address */
u32t     _std sys_endofram(void);

/// dump GDT to log
void     _std sys_gdtdump(void);

/// dump IDT to log
void     _std sys_idtdump(void);

/// flags for hlp_setupmem()
//@{
#define SETM_SPLIT16M  0x0001 // split memory block at 16M border
//@}

/** setup system memory usage (for OS/2 boot basically).
    Result will be written into physmem array (can be readed from storage
    key with STOKEY_PHYSMEM constant name).
    This call does not affect QSINIT memory use, it only calculates address
    and size of usable memory blocks below 4Gb to use by OS/2 boot process.

    @param [in]     fakemem    memory limit size, Mb (or 0)
    @param [in,out] logsize    ptr to doshlp log size, in 64kb blocks (or 0)
    @param [in]     removemem  zero-term list of block's heading address to
                               remove from use (or 0)
    @param [in]     flags      flags
    @return return physical log address (or 0 if no log or failed to alloc)
    and corrected log size in *logsize variable */
u32t _std hlp_setupmem(u32t fakemem, u32t *logsize, u32t *removemem, u32t flags);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_SYSTEMAPI