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

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CCINTERNAL
