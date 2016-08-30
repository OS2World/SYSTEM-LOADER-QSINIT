//
// QSINIT "bootos2" module
// kernel module loading interface
//
#ifndef _OS2KRNL_FILE_LOAD
#define _OS2KRNL_FILE_LOAD

#ifdef __cplusplus
extern "C" {
#endif
#include "qsmodint.h"

// parse kernel LX header
module* krnl_readhdr(void *kernel, u32t size);

// setup fixup table address
void    krnl_fixuptable(u32t *fixtab, u16t *fixcnt);

/** load & fix one kernel object
    mod_object.address - place to place it
    mod_object.fixaddr - addr to fix it */
void    krnl_loadobj(u32t obj);

// done loading, return start address
u32t    krnl_done(void);

#ifdef __cplusplus
}
#endif
#endif // _OS2KRNL_FILE_LOAD
