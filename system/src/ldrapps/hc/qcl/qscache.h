//
// QSINIT API
// internal disk cache system
//
#ifndef QSINIT_SECTORCACHE
#define QSINIT_SECTORCACHE

#include "qstypes.h"
#include "qsclass.h"

#ifdef __cplusplus
extern "C" {
#endif

/** cache shared control class.
    Control functions designed as shared class to prevent static linking of
    CACHE.DLL. This made it optional, i.e. NEW in example below can return
    NULL is case of missing cache module.
    Usage:
    @code
       qs_cachectrl cc = NEW(qs_cachectrl);
       cc->enable(QDSK_FLOPPY+0, 0);
       cc->setsize(100);
       DELETE(cc);
    @endcode */
typedef struct qs_cachectrl_s {
   /** start/stop disk caching.
       @param   size_mb    Size of cache, in Mb. Use 0 to turn cache off */
   void _exicc (*setsize)    (u32t size_mb);

   /** start/stop disk caching.
       @param   size       size string in CACHE command format (i.e. with
                           persent and "off" string) 
       @return success flag (i.e. non-zero if string valid and was processed) */
   u32t _exicc (*setsize_str)(const char *size);

   /** set cache priority for a part of disk.
       This function is a HINT for caching system, nothing is guaranteed
       really. Another bad thing is a rounding of priority borders to 1 Mb
       internally. So priority changes can interference each other.
       Must be called with ON to FAT areas, at least.
       @param   disk     Disk number
       @param   start    Start sector
       @param   size     Number of sectors
       @param   on       On/Off flag */
   void _exicc (*setprio)    (u32t disk, u64t start, u32t size, int on);

   /** enable/disable cache for specified disk.
       @param   disk     Disk number
       @param   enable   New state: 1/0 - to change, -1 to query current 
                         state only
       @return previous state (current, if enable==-1) in form: -1 - cache is
               not possible for this disk, 0 - cache is off, 1 - cache is on */
   int  _exicc (*enable)     (u32t disk, int enable);

   /** invalidate cache for a part of specified disk.
       @param   disk     Disk number 
       @param   start    Start sector
       @param   size     Length of block (use FFFF64 for full disk) */
   void _exicc (*invalidate) (u32t disk, u64t start, u64t size);

   /** invalidate cache for specified mounted volume.
       @param   drive    Voume drive letter */
   void _exicc (*invalidate_vol)(u8t drive);

   /** return number of cached blocks of specified disk.
       @param       disk     Disk number
       @param [out] pblocks  Ptr to number of blocks with priority, can be 0.
       @return number of 32k blocks */
   u32t _exicc (*stat)       (u32t disk, u32t *pblocks);
} _qs_cachectrl, *qs_cachectrl;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_SECTORCACHE
