//
// QSINIT API
// page mode and physical address mapping
//
#ifndef QSINIT_PAGEFUNC
#define QSINIT_PAGEFUNC

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Map physical address to logical address space.
    All maps, created in "non-paged" mode will be preserved in paged.
    Every map map call must be supplied with pag_physunmap() pair.

    In "paged" mode - function will try to use 2Mb pages if "address"
    is aligned to 2Mb and size >= 2Mb.

    @attention Function deny all maps, which cross the end of RAM in 
               first 4GBs. Use it only for mapping of hardware stuff,
               like video/misc card memory. Call sys_endofram() to
               query this border.

    @param  address  Physical address
    @param  size     Size of block
    @param  flags    Reserved
    @return pointer to start of block or zero */
void* _std pag_physmap(u64t address, u32t size, u32t flags);

/** Unmap previously mapped physical address.
    Function decrement usage counter first, when makes real unmap. */
u32t  _std pag_physunmap(void *address);

/** Turn on PAE paging mode.
    Operation is not reversable. Use sys_pagemode() to query current mode.
    @retval 0        on success
    @retval ENODEV   no PAE on this CPU
    @retval EEXIST   mode already on
    @retval ENOSYS   this is EFI host and EFI`s 64-bit paging active */
int   _std pag_enable(void);

/// Print all active page tables to log (VERY long dump).
void  _std pag_printall(void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_PAGEFUNC
