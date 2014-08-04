#include "cache.h"
#include "qsutil.h"

/** get volume system area.
    @param        vol     Mounted volume
    @param  [out] disk    Physical disk
    @param  [out] start   Start sector (disk based)
    @param  [out] length  Sector count
    @return success flag (1/0) */
int get_sys_area(u8t vol, u32t *disk, u64t *start, u32t *length) {
   disk_volume_data info;
   if (hlp_volinfo(vol,&info)!=FST_NOTMOUNTED) {
      if (disk)   *disk   = info.Disk;
      if (start)  *start  = info.StartSector;
      if (length) *length = info.DataSector - info.StartSector;
      return 1;
   } else {
      if (disk)   *disk   = FFFF;
      if (start)  *start  = 0;
      if (length) *length = 0;
   }
   return 0;
}
