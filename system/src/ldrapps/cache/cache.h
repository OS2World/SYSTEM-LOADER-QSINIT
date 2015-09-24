//
// QSINIT "cache" module
// internal functions (not exported)
//
#ifndef QSINIT_CCINTERNAL
#define QSINIT_CCINTERNAL

#ifdef __cplusplus
extern "C" {
#endif

#include "qstypes.h"

/** get volume system area.
    @param        vol     Mounted volume
    @param  [out] disk    Physical disk
    @param  [out] start   Start sector (disk based)
    @param  [out] length  Sector count
    @return success flag (1/0) */
int get_sys_area(u8t vol, u32t *disk, u64t *start, u32t *length);

void _std cc_setsize(void *data, u32t size_mb);
u32t _std cc_setsize_str(void *data, const char *size);
void _std cc_setprio(void *data, u32t disk, u64t start, u32t size, int on);
int  _std cc_enable(void *data, u32t disk, int enable);
void _std cc_invalidate(void *data, u32t disk, u64t start, u64t size);
void _std cc_invalidate_vol(void *data, u8t drive);
u32t _std cc_stat(void *data, u32t disk, u32t *pblocks);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CCINTERNAL
