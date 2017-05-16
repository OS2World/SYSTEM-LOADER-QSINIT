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
    Every map call must be supplied with pag_physunmap() pair.

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
    Function decrement usage counter first, then makes real unmap. */
u32t  _std pag_physunmap(void *address);

/** Turn on PAE paging mode.
    Operation is non-reversable. Use sys_pagemode() to query current mode.
    @attention Function can be called by some system functions like 
               sys_memhicopy() without any asking. I.e. application should
               not make any assumptions about current mode.
    @retval 0                  on success
    @retval E_SYS_UNSUPPORTED  no PAE on this CPU
    @retval E_SYS_INITED       mode already on
    @retval E_SYS_EFIHOST      this is EFI host and EFI`s 64-bit paging active */
qserr _std pag_enable(void);

/// @name pag_query() result code
//@{
#define PGEA_UNKNOWN         (  0)  ///< unable to determine
#define PGEA_NOTPRESENT      (  1)  ///< page not present
#define PGEA_READ            (  2)  ///< read access ok
#define PGEA_WRITE           (  3)  ///< write access ok
//@}

/** test access of specified page.
    @param  addr     Address to test
    @return PGEA_* value. */
u32t  _std pag_query(void *addr);

/// Print all active page tables to log (VERY long dump, especially in EFI).
void  _std pag_printall(void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_PAGEFUNC
