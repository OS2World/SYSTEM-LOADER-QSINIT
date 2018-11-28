#include "pscan.h"
#include "malloc.h"

qserr _std dsk_checkbm(u32t disk, long index, read_callback cbprint,
                       u32t *errinfo, str_list **errtext)
{
   FUNC_LOCK  lk;
   qserr      rc = 0;
   hdd_info  *hi = get_by_disk(disk);
   if (errinfo) *errinfo = 0;
   if (errtext) *errtext = 0;
   if (!hi) return E_DSK_DISKNUM;

   rc = dsk_ptrescan(disk, 0);
   if (!rc && index<0) return hi->gpt_present?E_PTE_GPTDISK:E_PTE_MBRDISK;
   if (rc)
      if (index>=0) return rc; else
         if (rc!=E_PTE_FLOPPY) return rc;

   u64t start = 0, size = 0;
   if (index<0) {
      start = 0;
      size  = hlp_disksize64(disk, 0);
      if (!size) return E_DSK_NOTREADY;
   } else
   if (!dsk_ptquery64(disk, index, &start, &size, 0, 0)) return E_PTE_PINDEX;
   if (!size || size>=_4GBLL) return E_DSK_PT2TB;

   void   *br = malloc_thread(MAX_SECTOR_SIZE);
   u32t    bs = dsk_sectortype(disk, start, (u8t*)br);

   if (bs==DSKST_BOOTFAT) {
/*      strncpy(filesys, br->BR_BPB.BPB_RootEntries==0?
        ((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_FSType:
           br->BR_EBPB.EBPB_FSType,8); */

   } else
   if (bs==DSKST_BOOTEXF) {

   } else
     rc = E_DSK_UNKFS;
   free(br);
   // just a stub now
   if (!rc) rc = E_SYS_UNSUPPORTED;
   return rc;
}
