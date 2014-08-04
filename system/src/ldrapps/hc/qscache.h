//
// QSINIT API
// internal disk cache system
//
#ifndef QSINIT_SECTORCACHE
#define QSINIT_SECTORCACHE

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** start/stop disk caching.
    @param   size_mb    Size of cache, in Mb. Use 0 to turn cache off */
void _std hlp_cachesize(u32t size_mb);

/** set cache priority for a part of disk.
    This function is a HINT for caching system, nothing is guaranteed really.
    Another bad thing is a rounding of priority borders to 1 Mb internally. So
    priority changes can interference each other.
    Must be called with ON to FAT areas, at least.
    @param   disk     Disk number
    @param   start    Start sector
    @param   size     Number of sectors
    @param   on       On/Off flag */
void _std hlp_cachepriority(u32t disk, u64t start, u32t size, int on);

/** enable/disable cache for specified disk.
    @param   disk     Disk number
    @param   enable   New state: 1/0 - to change, -1 to query current state only
    @return previous state (current, if enable==-1) in form: -1 - cache is
            not possible for this disk, 0 - cache is off, 1 - cache is on */
int  _std hlp_cacheon(u32t disk, int enable);

/** invalidate cache for a part of specified disk.
    @param   disk     Disk number 
    @param   start    Start sector
    @param   size     Length of block (use FFFF64 for full disk) */
void _std hlp_cacheinv(u32t disk, u64t start, u64t size);

/** invalidate cache for specified mounted volume.
    @param   drive    Voume drive letter */
void _std hlp_cacheinvvol(u8t drive);

/** return number of cached blocks of specified disk.
    @param       disk     Disk number
    @param [out] pblocks  Ptr to number of blocks with priority, can be 0.
    @return number of 32k blocks */
u32t _std hlp_cachestat(u32t disk, u32t *pblocks);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_SECTORCACHE
