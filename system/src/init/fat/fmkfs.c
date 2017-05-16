//
// QSINIT
// FAT12/16 format - for virtual disk
//
#include "qslocal.h"
#include "qstime.h"
#include "parttab.h"

#define N_ROOTDIR         128      ///< # of root dir entries for FAT12/16
#define N_FATS              1      ///< # of FAT copies
#define N_RES               1      ///< # of reserved sectors
#define MAX_FAT12       0xFF5      ///< max # of FAT12 clusters
#define	MAX_FAT16      0xFFF5      ///< max # of FAT16 clusters

qserr format_vol1() {
   static const u16t cst[] = {1, 4, 16, 64, 256, 512, 0};  // cluster size boundary for FAT12/16 volume (4k unit)
   vol_data          *vdta = extvol+DISK_LDR;
   u8t       bs[LDR_SSIZE];  // 1k
   struct Boot_Record  *br = (struct Boot_Record*)&bs;
   u32t             unitsz;  ///< sectors per cluster
   u32t    ii, fmt, n_clst, nn;
   u32t              fatsz,  ///< sectors per FAT
                    fatpos;  ///< 1st sector of FAT
   u8t               media = 0xF0;

   memset(br, 0, LDR_SSIZE);
   // volume size in 4k units
   nn = vdta->length >> 12;
   for (ii=0, unitsz=1; cst[ii] && cst[ii]<=nn; ii++, unitsz<<=1) ;
   do {
      u32t  dirsz;
      // pre-compute number of clusters and FAT sub-type
      n_clst = vdta->length / unitsz;
      fmt = FST_FAT12;
      if (n_clst > MAX_FAT12) fmt = FST_FAT16;
      if (n_clst > MAX_FAT16) return E_DSK_VLARGE;

      /* Determine offset and size of FAT structure */
      fatsz = fmt==FST_FAT12 ? (n_clst*3 + 1) / 2 + 3 : (n_clst*2) + 4;
      fatsz = (fatsz + LDR_SSIZE - 1) / LDR_SSIZE;
      dirsz = N_ROOTDIR * 32 / LDR_SSIZE;

      fatpos = vdta->start + N_RES;
      // too small volume
      if (vdta->length < fatpos + fatsz*N_FATS + dirsz + unitsz - vdta->start)
         return E_DSK_VSMALL;

      // final check of the FAT sub-type
      n_clst = (vdta->length - N_RES - fatsz*N_FATS - dirsz) / unitsz;
      if (fmt==FST_FAT16 && n_clst<=MAX_FAT12) {
         if (unitsz<128) unitsz<<=1; else unitsz>>=1;
         continue;
      }
      break;
   } while (1);

   memcpy(br, "\xEB\xFE\x90" "MSDOS5.0", 11);
   br->BR_BPB.BPB_BytePerSect = LDR_SSIZE;
   br->BR_BPB.BPB_SecPerClus  = unitsz;
   br->BR_BPB.BPB_ResSectors  = N_RES;
   br->BR_BPB.BPB_FATCopies   = N_FATS;
   br->BR_BPB.BPB_RootEntries = N_ROOTDIR;
   br->BR_BPB.BPB_MediaByte   = media;
   br->BR_BPB.BPB_SecPerFAT   = fatsz;
   br->BR_BPB.BPB_SecPerTrack = 63;
   br->BR_BPB.BPB_Heads       = 255;
   br->BR_BPB.BPB_HiddenSec   = vdta->start;

   if (vdta->length<0x10000) br->BR_BPB.BPB_TotalSec = vdta->length; else
      br->BR_BPB.BPB_TotalSecBig = vdta->length;

   br->BR_EBPB.EBPB_PhysDisk  = 0x80;
   br->BR_EBPB.EBPB_Sign      = 0x29;
   br->BR_EBPB.EBPB_VolSerial = tm_getdate();

   memcpy(&br->BR_EBPB.EBPB_FSType, "FAT1    ", 8);
   br->BR_EBPB.EBPB_FSType[4] = fmt==FST_FAT16?'6':'2';
   bs[510] = 0x55;
   bs[511] = 0xAA;
   // write boot sector
   if (!hlp_diskwrite(QDSK_VOLUME|DISK_LDR, vdta->start, 1, br)) return E_DSK_ERRWRITE;

   /* we have zero-filled disk space on enter, this cause skipping of
      some normal actions, like cleaning of FAT/root dir areas */

   // write 1st sector of FAT (single non-zero)
   for (ii=0; ii<N_FATS; ii++) {
      memset(br, 0, LDR_SSIZE);
      *(u32t*)br = (fmt==FST_FAT12?0x00FFFF00:0xFFFFFF00) | media;

      if (!hlp_diskwrite(QDSK_VOLUME|DISK_LDR, fatpos + ii*fatsz, 1, br))
         return E_DSK_ERRWRITE;
   }
   return 0;
}

