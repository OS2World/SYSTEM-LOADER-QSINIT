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
#include "qsclass.h"

/** get volume system area.
    @param        vol     Mounted volume
    @param  [out] disk    Physical disk
    @param  [out] start   Start sector (disk based)
    @param  [out] length  Sector count
    @return success flag (1/0) */
int get_sys_area(u8t vol, u32t *disk, u64t *start, u32t *length);

void _exicc cc_setsize       (EXI_DATA, u32t size_mb);
u32t _exicc cc_setsize_str   (EXI_DATA, const char *size);
void _exicc cc_setprio       (EXI_DATA, u32t disk, u64t start, u32t size, int on);
int  _exicc cc_enable        (EXI_DATA, u32t disk, int enable);
void _exicc cc_invalidate    (EXI_DATA, u32t disk, u64t start, u64t size);
void _exicc cc_invalidate_vol(EXI_DATA, u8t drive);
u32t _exicc cc_stat          (EXI_DATA, u32t disk, u32t *pblocks);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CCINTERNAL
