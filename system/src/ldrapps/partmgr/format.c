//
// QSINIT
// partition management dll
// -----------------------------
// FAT formatting code, copied from ChaN`s FatFs with some additions:
//     4k alignment, boot sector, etc...
//
#include "qsutil.h"
#include "qsdm.h"
#include "parttab.h"
#include "qstime.h"
#include "qcl/qslist.h"
#include "pscan.h"
#include "stdlib.h"
#include "qsint.h"
#include "qsstor.h"
#include "vio.h"

#define MIN_FAT16    4086    // minimum number of clusters for FAT16
#define MIN_FAT32   65526    // minimum number of clusters for FAT32

#define FORCE_64K      48    // force 64k cluster starting from this number of Gbs

#define _FAT12          1
#define _FAT16          2
#define _FAT32          3

#define N_ROOTDIR     512    // number of root dir entries for FAT12/16
#define N_ROOTFDD     224
#define SZ_DIR         32    // directory entry size

extern char  bsect16[512],   // VBR code
             bsect32[512];

static int check_esc(u32t disk, u64t sector) {
   u16t key = key_wait(0);
   if ((key&0xFF)==27)
      if (confirm_dlg("Break format process?^Partition will be unusable!")) {
         // zero boot sector for incomplete partition format
         char   buffer[MAX_SECTOR_SIZE];
         memset(buffer, 0, MAX_SECTOR_SIZE);
         hlp_diskwrite(disk|QDSK_DIRECT, sector, 1, buffer);
         return 1;
      }
   return 0;
}

#define CHECK_ESC()                               \
   if (kbask)                                     \
      if (check_esc(di.Disk, di.StartSector)) {   \
         if (pf) hlp_memfree(pf);                 \
         return DFME_UBREAK;                      \
      }

u32t _std vol_format(u8t vol, u32t flags, u32t unitsize, read_callback cbprint) {
   static u16t vst[] = { 1024,  512, 256, 128,  64,   32,  16,   8,   4,   2,  0};
   static u16t cst[] = {32768,16384,8192,4096,2048,16384,8192,4096,2048,1024,512};
   u32t ii, clusters, fatsize, reserved, dirsize, pfcnt, *pf, fmt,
         fatcnt = flags&DFMT_ONEFAT ?1:2, sectin4k, badcnt = 0,
         hidden = 0, rootsize;
   int    align = flags&DFMT_NOALIGN?0:1,  // AF align flag
          kbask = flags&DFMT_BREAK?1:0;    // allow keyboard ESC break
   u64t   loc_dir, loc_data, loc_fat, wsect;

   u8t   brdata[MAX_SECTOR_SIZE], media;
   long  volidx, ptbyte;
   struct Boot_Record    *brw = (struct Boot_Record    *)&brdata;
   struct Boot_RecordF32 *brd = (struct Boot_RecordF32 *)&brdata;
   disk_volume_data di;
   disk_geo_data   geo;

   hlp_volinfo(vol, &di);

   //log_it(2, "vol=%X, sz=%d\n", vol, di.TotalSectors);

   if (!di.TotalSectors) return DFME_NOMOUNT;
   volidx = vol_index(vol,0);
   // allow floppies and big floppies
   if (volidx<0 && di.StartSector) return DFME_VINDEX;
   if (volidx>=0) {
      if (dsk_isgpt(di.Disk,volidx)==1) {
         // GPT partition
         flags |=DFMT_NOPTYPE;
         hidden = di.StartSector>=_4GBLL ? FFFF : di.StartSector;
      } else {
         // MBR or hybrid partition
         dsk_ptquery(di.Disk,volidx,0,0,0,0,&hidden);
         if (hidden==FFFF) return DFME_EXTERR;
      }
   }
   // turn off align for floppies
   if ((di.Disk&QDSK_FLOPPY)!=0) align = 0;
   // sectors in 4k
   sectin4k = 4096 / di.SectorSize;
   if (!sectin4k || 4096 % di.SectorSize) return DFME_SSIZE;
   /* unmount will clear all cache and below all of r/w ops use QDSK_DIRECT
      flag, so no any volume caching will occur until the end of format */
   if (!hlp_unmountvol(vol)) return DFME_UMOUNT;
   // try to query actual values: LVM/PT first, BIOS - next
   if (!dsk_getptgeo(di.Disk,&geo)) {
      // ignore LVM surprise
      if (geo.SectOnTrack>63) geo.SectOnTrack = 63;
   } else
   if (!hlp_disksize(di.Disk,0,&geo)) {
      geo.Heads       = 255;
      geo.SectOnTrack =  63;
   }
   // force 64k cluster on huge partitions
   if (!unitsize && (u64t)di.TotalSectors*di.SectorSize >= (u64t)FORCE_64K*_1GB)
      unitsize = 65536;
   // cluster size auto selection
   if (!unitsize) {
      u32t vs = di.TotalSectors / (2000 / (di.SectorSize / 512));
      for (ii = 0; vs < vst[ii]; ii++) ;
      unitsize = cst[ii];
   }
   // sectors per cluster
   unitsize /= di.SectorSize;
   if (unitsize == 0) unitsize = 1;
   if (unitsize > 128) unitsize = 128;

   // pre-compute number of clusters and FAT sub-type
   clusters = di.TotalSectors / unitsize;
   fmt = _FAT12;
   if (clusters >= MIN_FAT16) fmt = _FAT16;
   if (clusters >= MIN_FAT32) fmt = _FAT32;

   // determine offset and size of FAT structure
   if (fmt == _FAT32) {
      fatsize  = (clusters * 4 + 8 + di.SectorSize - 1) / di.SectorSize;
      reserved = 32;
      dirsize  = 0;
   } else {
      rootsize = di.TotalSectors<4096? N_ROOTFDD : N_ROOTDIR;
      // turn off alignment for too small disks because of 224 dir entries
      if (di.TotalSectors<4096) align = 0;
      fatsize  = fmt==_FAT12? (clusters*3 + 1) / 2 + 3 : clusters*2 + 4;
      fatsize  = (fatsize + di.SectorSize - 1) / di.SectorSize;
      reserved = 1;
      dirsize  = rootsize * SZ_DIR / di.SectorSize;
   }
   // align 1st FAT copy to 4k
   if (align)
      reserved = ((di.StartSector + reserved + sectin4k - 1) & ~(sectin4k-1)) -
         di.StartSector;
   // align single FAT copy size to 4k
   if (align) fatsize = fatsize + sectin4k - 1 & ~(sectin4k-1);
   // calc locations
   loc_fat  = di.StartSector + reserved;      // FAT area start sector
   loc_dir  = loc_fat + fatsize * fatcnt;     // root directory start sector
   loc_data = loc_dir + dirsize;              // data area start sector
   // too small volume
   if (di.TotalSectors < loc_data + unitsize - di.StartSector)
      return DFME_SMALL;

   log_printf("Formatting vol %c, disk %02X, index %d with FAT%s\n",
      vol+'0', di.Disk, volidx, fmt==_FAT16?"16":(fmt==_FAT32?"32":"12"));
   log_it(2, "Reserved=%d, FAT size=%d, pos %010LX, Data pos %010LX\n",
      reserved, fatsize, loc_fat, loc_data);

   // align data to 4k
   if (align) {
#if 0 // both FAT copies now aligned by code above...

      /* dirsize is always even on FAT16, 1st FAT copy aligned to 4k,
         so our difference will always be even and we can divide it by
         fatcnt freely */
      ii = ((loc_data + sectin4k - 1) & ~(sectin4k - 1)) - loc_data;
      fatsize  += ii / fatcnt; // expand FAT size
      loc_data += ii;
      loc_dir  += ii;
      log_it(2, "Aligned: FAT size=%d, pos %010LX, Data pos %010LX\n",
         fatsize, loc_fat, loc_data);
#endif
   }
   // determine number of clusters and final check of validity of the FAT type
   clusters = (di.TotalSectors - reserved - fatsize * fatcnt - dirsize) / unitsize;
   if (fmt==_FAT16 && clusters < MIN_FAT16 || fmt==_FAT32 && clusters < MIN_FAT32)
      return DFME_FTYPE;

   media = di.StartSector?0xF8:0xF0;

   // create BPB
   memset(brw, 0, 512);
   brw->BR_BPB.BPB_BytePerSect = di.SectorSize;       // sector size
   brw->BR_BPB.BPB_SecPerClus  = (u8t)unitsize;       // sectors per cluster
   brw->BR_BPB.BPB_ResSectors  = reserved;            // reserved sectors
   brw->BR_BPB.BPB_FATCopies   = fatcnt;              // number of FATs
   if (di.TotalSectors < 0x10000)                     // number of total sectors
      brw->BR_BPB.BPB_TotalSec = di.TotalSectors;
   else
      brw->BR_BPB.BPB_TotalSecBig = di.TotalSectors;
   brw->BR_BPB.BPB_MediaByte   = media;               // media descriptor
   brw->BR_BPB.BPB_SecPerTrack = geo.SectOnTrack;     // number of sectors per track
   brw->BR_BPB.BPB_Heads       = geo.Heads;           // number of heads
   brw->BR_BPB.BPB_HiddenSec   = hidden;              // hidden sectors

   // use current time as VSN
   ii = tm_getdate();
   if (fmt==_FAT32) {
      struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsect32;
      brd->BR_F32_EBPB.EBPB_VolSerial = ii;           // VSN
      brd->BR_F32_BPB.FBPB_SecPerFAT  = fatsize;      // number of sectors per FAT
      brd->BR_F32_BPB.FBPB_RootClus   = 2;            // root directory start cluster
      brd->BR_F32_BPB.FBPB_FSInfo     = 1;            // FSInfo record offset (VBR+1)
      brd->BR_F32_BPB.FBPB_BootCopy   = 6;            // backup boot record offset (VBR+6)
      if ((di.Disk&QDSK_FLOPPY)==0)
         brd->BR_F32_EBPB.EBPB_PhysDisk  = 0x80;      // drive number
      brd->BR_F32_EBPB.EBPB_Sign      = 0x29;         // extended boot signature
      memcpy(brd->BR_F32_EBPB.EBPB_VolLabel, "NO NAME    ", 11); // volume label
      memcpy(brd->BR_F32_EBPB.EBPB_FSType  , "FAT32   ", 8);     // FAT signature
      memcpy(brd,bsect32,11);
      memcpy(brd+1,sbtd+1,512-sizeof(struct Boot_RecordF32));
   } else {
      struct Boot_Record *sbtw = (struct Boot_Record *)&bsect16;
      brw->BR_EBPB.EBPB_VolSerial     = ii;           // VSN
      brw->BR_BPB.BPB_RootEntries     = rootsize;     // rootdir entries
      brw->BR_BPB.BPB_SecPerFAT       = fatsize;      // number of sectors per FAT
      if ((di.Disk&QDSK_FLOPPY)==0)
         brw->BR_EBPB.EBPB_PhysDisk   = 0x80;         // drive number
      brw->BR_EBPB.EBPB_Sign          = 0x29;         // extended boot signature
      memcpy(brw->BR_EBPB.EBPB_VolLabel, "NO NAME    ", 11); // volume label
      memcpy(brw->BR_EBPB.EBPB_FSType, fmt==_FAT12?"FAT12   ":"FAT16   ", 8); // FAT signature
      memcpy(brw,bsect16,11);
      memcpy(brw+1,sbtw+1,512-sizeof(struct Boot_Record));
   }
   // write it to the VBR sector
   if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, di.StartSector, 1, brdata))
      return DFME_IOERR;

   pf     = (u32t*)hlp_memalloc(_32KB, 0),
   pfcnt  = _32KB/di.SectorSize;                       // sectors in 32k

   // clear "reserved" area
   if (reserved>1) {
      wsect = di.StartSector+1;
      ii    = reserved-1;
      while (ii) {
         u32t cpy = ii>pfcnt?pfcnt:ii;
         if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)) {
            hlp_memfree(pf);
            return DFME_IOERR;
         }
         wsect+= cpy;
         ii   -= cpy;
         // check for esc
         CHECK_ESC();
      }
   }
   // write backup VBR if needed (VBR+6)
   if (fmt==_FAT32)
      hlp_diskwrite(di.Disk|QDSK_DIRECT, di.StartSector+6, 1, brdata);

   // initialize FAT area
   wsect  = loc_fat;

   for (ii = 0; ii < fatcnt; ii++) {                  // initialize each FAT copy
      u32t  pv = media;                               // media descriptor byte

      if (fmt!=_FAT32) {
         pv   |= fmt==_FAT12?0x00FFFF00:0xFFFFFF00;
         pf[0] = pv;                                  // reserve cluster 0-1 (FAT12/16)
      } else {
         pv   |= 0xFFFFFF00;
         pf[0] = pv;                                  // reserve cluster 0-1 (FAT32)
         pf[1] = FFFF;
         pf[2] = 0x0FFFFFFF;                          // reserve cluster 2 for root dir
      }
      // fill FAT entries
      pv = fatsize;
      while (pv) {
         u32t cpy = pv>pfcnt?pfcnt:pv;
         // write 32k at time (i/o buffer size)
         if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)) {
            hlp_memfree(pf);
            return DFME_IOERR;
         }
         wsect+= cpy;
         pv   -= cpy;
         // zero buffer after first write
         if (pf[0]) { pf[0]=0; pf[1]=0; pf[2]=0; }
         // check for esc
         CHECK_ESC();
      }
   }
   // initialize root directory
   ii = fmt==_FAT32?unitsize:dirsize;
   while (ii) {
      u32t cpy = ii>pfcnt?pfcnt:ii;
      if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)) {
         hlp_memfree(pf);
         return DFME_IOERR;
      }
      wsect+= cpy;
      ii   -= cpy;
      // check for esc
      CHECK_ESC();
   }

   // read/erase data area
   if ((flags&DFMT_WIPE)!=0 || (flags&DFMT_QUICK)==0) {
      dd_list bclist = NEW(dd_list);
      int   readonly = flags&DFMT_WIPE?0:1;
      u64t     total;
      u32t   percent = 0;
      ii    = (clusters - (fmt==_FAT32?1:0)) * unitsize - 1;
      total = ii;
      // print callback
      if (cbprint) cbprint(0,0);
      // data to write
      memset(pf,FMT_FILL,_32KB);

      while (ii) {
         u32t  cpy = ii>pfcnt?pfcnt:ii,
           success = 0;
         // write 32k, read 32k and compare ;)
         if (readonly || hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)==cpy)
            if (hlp_diskread(di.Disk|QDSK_DIRECT, wsect, cpy, pf)==cpy)
               if (readonly || !memchrnb((u8t*)pf,FMT_FILL,_32KB)) success = 1;

         if (success) { // good 32k
            wsect+= cpy;
            ii   -= cpy;
         } else {       // at least one sector is bad, check it all
            u32t lastbad = bclist->max()>=0?bclist->value(bclist->max()):FFFF,
                 cluster;
            // clear array after error
            memset(pf,FMT_FILL,_32KB);

            while (cpy) {
               cluster = (wsect-loc_data) / unitsize + 2;
               // do not check other sectors of bad cluster
               if (cluster!=lastbad) {
                  success = 0;
                  if (readonly || hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, 1, pf))
                     if (hlp_diskread(di.Disk|QDSK_DIRECT, wsect, 1, pf))
                        if (readonly || !memchrnb((u8t*)pf,FMT_FILL,di.SectorSize))
                           success = 1;
                  // add cluster to bad list
                  if (!success) {
                     memset(pf,FMT_FILL,di.SectorSize);
                     bclist->add(lastbad = cluster);
                  }
               }
               ii--; wsect++; cpy--;
            }
         }
         if (cbprint) {
            u32t tmpp = (total - ii) * 100/ total;
            if (tmpp != percent) cbprint(percent=tmpp,0);
         }
         // check for esc
         CHECK_ESC();
      }
      // bad clusters found? reinitialize FAT
      if (bclist->count()) {
         u32t epbuf;
         // sort bad cluster list
         bclist->sort(0,1);
         badcnt = bclist->count();
         log_it(2, "%d bad clusters found\n", badcnt);
         // FAT entries per read buffer
         switch (fmt) {
            case _FAT12: epbuf = _32KB*3/2; break;
            case _FAT16: epbuf = _32KB/2; break;
            case _FAT32: epbuf = _32KB/4; break;
         }
         wsect = loc_fat;
         // zero buffer for FAT update
         memset(pf,0,_32KB);
         // update FAT with marked bad clusters on
         for (ii = 0; ii < fatcnt; ii++) {
            u32t  pv = media, clpos, bpos;

            if (fmt!=_FAT32) {
               pv   |= fmt==_FAT12?0x00FFFF00:0xFFFFFF00;
               pf[0] = pv;                            // reserve cluster 0-1 (FAT12/16)
            } else {
               pv   |= 0xFFFFFF00;
               pf[0] = pv;                            // reserve cluster 0-1 (FAT32)
               pf[1] = FFFF;
               pf[2] = 0x0FFFFFFF;                    // reserve cluster 2 for root dir
            }
            // fill FAT entries
            pv    = fatsize;
            clpos = 0;
            bpos  = 0;
            while (pv && bpos<bclist->count()) {
               u32t cpy = pv>pfcnt?pfcnt:pv;
               // fill bad entries for current potion of FAT
               while (bpos<bclist->count() && bclist->value(bpos) < clpos+epbuf) {
                  u32t bb = bclist->value(bpos++);
                  // mask cluster as bad
                  switch (fmt) {
                     case _FAT12: {
                        u16t* fptr = (u16t*)((u8t*)pf + (bb-clpos)*3/2);
                        *fptr |= bb&1?0xFF70:0xFF7;
                        break;
                     }
                     case _FAT16: ((u16t*)pf)[bb-clpos] = 0xFFF7; break;
                     case _FAT32: pf[bb-clpos] = 0x0FFFFFF7; break;
                  }
               }
               clpos+=epbuf;
               // write this 32k (i/o buffer size)
               if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)) {
                  hlp_memfree(pf);
                  return DFME_IOERR;
               }
               wsect+= cpy;
               pv   -= cpy;
               // zero buffer
               memset(pf,0,_32KB);
            }
         }
      }
      DELETE(bclist);
   }
   hlp_memfree(pf); pf = 0;

   // create FSInfo on FAT32
   if (fmt == _FAT32) {
      u32t *sptr = (u32t*)&brdata;
      memset(brdata, 0, di.SectorSize);
      sptr[0]     = 0x41615252;
      sptr[121]   = 0x61417272;
      sptr[122]   = clusters - 1 - badcnt;            // number of free clusters
      sptr[123]   = 2;                                // last allocated cluster
      brdata[510] = 0x55;
      brdata[511] = 0xAA;
      hlp_diskwrite(di.Disk|QDSK_DIRECT, di.StartSector+1, 1, brdata); // write original (VBR+1)
      hlp_diskwrite(di.Disk|QDSK_DIRECT, di.StartSector+7, 1, brdata); // write backup (VBR+7)
   }

   ptbyte = -1;
   if (volidx>=0 && (flags&DFMT_NOPTYPE)==0) {
      switch (fmt) {
         case _FAT12:  ptbyte = 0x01; break;
         case _FAT16:  ptbyte = di.TotalSectors < 0x10000? 0x04 : 0x06; break;
         default:      ptbyte = 0x0C;
      }
   }
   // update partition type & mount volume back
   return format_done(vol, di, ptbyte, volidx, badcnt);
}

u32t format_done(u8t vol, disk_volume_data di, long ptbyte, long volidx, u32t badcnt) {
   // determine and change ID in partition table
   if (volidx>=0 && ptbyte>=0) {
      u32t rc = dsk_setpttype(di.Disk, volidx, (u8t)ptbyte);
      log_it(2, "set partition type to %02X, rc %d\n", ptbyte, rc);
      // handle error codes
      switch (rc) {
         case DPTE_ERRREAD :
         case DPTE_ERRWRITE: return DFME_IOERR;
         case DPTE_PINDEX  : return DFME_VINDEX;
         case DPTE_INVALID : return DFME_PTERR;
         default: if (rc) return DFME_INTERNAL;
      }
   }
   // force rescanning of disk if it (big) floppy
   if (di.StartSector==0) dsk_ptrescan(di.Disk,1);
   // mount volume back
   if (!hlp_mountvol(vol, di.Disk, di.StartSector, di.TotalSectors))
      return DFME_MOUNT;
   /* update bad clusters count in internal volume info.
      this value will be saved only until unmount, but can be used by format
      command nice printing */
   if (badcnt) {
      vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
      if (extvol) {
         vol_data *vdta = extvol+vol;
         if ((vdta->flags&VDTA_ON)!=0) vdta->badclus = badcnt;
      }
   }
   return 0;
}

u32t _std vol_formatfs(u8t vol, char *fsname, u32t flags, u32t unitsize, 
   read_callback cbprint)
{
   if (fsname) {
      if (stricmp(fsname,"FAT")==0)
         return vol_format(vol, flags, unitsize, cbprint);
      if (stricmp(fsname,"HPFS")==0)
         return hpfs_format(vol, flags, cbprint);
   }
   return DFME_UNKFS;
}
