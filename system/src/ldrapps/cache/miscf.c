#include "cache.h"
#include "qsutil.h"

/** get volume system area.
    @param        vol     Mounted volume
    @param  [out] disk    Physical disk
    @param  [out] start   Start sector (disk based)
    @param  [out] length  Sector count
    @return success flag (1/0) */
int get_sys_area(u8t vol, u32t *disk, u64t *start, u32t *length) {
   disk_volume_data vi;
   u32t         fstype = hlp_volinfo(vol,&vi);

   if (fstype!=FST_NOTMOUNTED) {
      if (disk)   *disk   = vi.Disk;
      if (start)  *start  = vi.StartSector;
      if (length) {
         *length = vi.DataSector - vi.StartSector;
         // add bitmap & root dir 1st cluster for exFAT
         if (fstype==FST_EXFAT) {
             u32t bmsize = ((Round8(vi.ClTotal)>>8) + vi.SectorSize - 1) / vi.SectorSize;
             // 1-2 clusters for upcase table, 1 for root dir + x for bitmap
             u32t clsize = 3 + (bmsize + vi.ClSize - 1) / vi.ClSize;
             *length += clsize * vi.ClSize;
         }
      }
      return 1;
   } else {
      if (disk)   *disk   = FFFF;
      if (start)  *start  = 0;
      if (length) *length = 0;
   }
   return 0;
}
