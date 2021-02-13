//
// QSINIT
// FAT12/16 format - for virtual disk
// 2.88    - 2 per cluster, 36 per track, 240 dir size
// 1.84    - 23 per track, 224 dir size
// 
//
#include "fmkfs.h"
#include "errno.h"

#define N_ROOTDIR         224      ///< # of root dir entries for FAT12/16
#define N_FATS              2      ///< # of FAT copies
#define N_RES               1      ///< # of reserved sectors
#define MAX_FAT12       0xFF5      ///< max # of FAT12 clusters
#define MAX_FAT16      0xFFF5      ///< max # of FAT16 clusters

extern BYTE bsdata[];

int format(DWORD vol_start, DWORD vol_size, DWORD unitsz, WORD heads, WORD spt,
           WORD n_rootdir,  DWORD hsec_offset)
{
   static const WORD cst[] = {1, 4, 16, 64, 256, 512, 0};
   BYTE            bs[512];  // 1k
   struct Boot_Record  *br = (struct Boot_Record*)&bs;
   DWORD   ii, fmt, n_clst, nn, wrpos;
   DWORD             fatsz,  ///< sectors per FAT
                    fatpos,  ///< 1st sector of FAT
                     dirsz;
   BYTE              media = vol_start?0xF8:0xF0;

   if (!heads || !spt) return -EINVAL;
   // FAT12/16 boot code
   memcpy(br, &bsdata[512], 512);

   if (!unitsz) {
      // volume size in 4k units
      nn = vol_size >> 12;
      for (ii=0, unitsz=1; cst[ii] && cst[ii]<=nn; ii++, unitsz<<=1) ;
   }
   do {
      // pre-compute number of clusters and FAT sub-type
      n_clst = vol_size / unitsz;
      fmt = FST_FAT12;
      if (n_clst > MAX_FAT12) fmt = FST_FAT16;
      if (n_clst > MAX_FAT16) return -ENFILE;
      // fix it and check
      n_rootdir &= ~0xF;
      if (!n_rootdir) n_rootdir = N_ROOTDIR;
      /* Determine offset and size of FAT structure */
      fatsz = fmt==FST_FAT12 ? (n_clst*3 + 1) / 2 + 3 : (n_clst*2) + 4;
      fatsz = (fatsz + 512 - 1) >> 9;
      dirsz = n_rootdir * 32 >> 9;

      fatpos = vol_start + N_RES;
      // too small volume
      if (vol_size < fatpos + fatsz*N_FATS + dirsz + unitsz - vol_start)
         return -ENOSPC;

      // final check of the FAT sub-type
      n_clst = (vol_size - N_RES - fatsz*N_FATS - dirsz) / unitsz;
      if (fmt==FST_FAT16 && n_clst<=MAX_FAT12) {
         if (unitsz<128) unitsz<<=1; else unitsz>>=1;
         continue;
      }
      break;
   } while (1);

   memcpy(&br->BR_OEM, "MSDOS5.0", 8);
   br->BR_BPB.BPB_BytePerSect = 512;
   br->BR_BPB.BPB_SecPerClus  = unitsz;
   br->BR_BPB.BPB_ResSectors  = N_RES;
   br->BR_BPB.BPB_FATCopies   = N_FATS;
   br->BR_BPB.BPB_RootEntries = n_rootdir;
   br->BR_BPB.BPB_MediaByte   = media;
   br->BR_BPB.BPB_SecPerFAT   = fatsz;
   br->BR_BPB.BPB_SecPerTrack = spt;
   br->BR_BPB.BPB_Heads       = heads;
   br->BR_BPB.BPB_HiddenSec   = vol_start + hsec_offset;
   br->BR_BPB.BPB_TotalSec    = 0;
   br->BR_BPB.BPB_TotalSecBig = 0;

   if (vol_size<0x10000) br->BR_BPB.BPB_TotalSec = vol_size; else
      br->BR_BPB.BPB_TotalSecBig = vol_size;

   br->BR_EBPB.EBPB_PhysDisk  = vol_start?0x80:0x00;
   br->BR_EBPB.EBPB_Sign      = 0x29;
   br->BR_EBPB.EBPB_VolSerial = get_fattime();

   memcpy(&br->BR_EBPB.EBPB_FSType, "FAT1    ", 8);
   br->BR_EBPB.EBPB_FSType[4] = fmt==FST_FAT16?'6':'2';
   // write boot sector
   if (disk_write(0, bs, vol_start, 1)!=RES_OK) return -EIO;
   // initialize FAT area
   for (ii=0, wrpos=fatpos; ii<N_FATS; ii++) {
      memset(br, 0, 512);
      *(DWORD*)br = (fmt==FST_FAT12?0x00FFFF00:0xFFFFFF00) | media;
      if (disk_write(0, bs, wrpos++, 1)!=RES_OK) return -EIO;
      *(DWORD*)br = 0;
      for (nn=1; nn<fatsz; nn++)
         if (disk_write(0, bs, wrpos++, 1)!=RES_OK) return -EIO;
   }
   // initialize root directory
   ii = dirsz;
   do {
      if (disk_write(0, bs, wrpos++, 1)!=RES_OK) return -EIO;
   } while (--ii);

   return fmt;
}
