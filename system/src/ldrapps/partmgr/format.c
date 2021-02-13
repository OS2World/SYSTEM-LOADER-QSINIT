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
#include "qcl/qslist.h"
#include "qcl/bitmaps.h"
#include "qcl/cplib.h"
#include "pscan.h"
#include "stdlib.h"
#include "qsint.h"
#include "qsio.h"
#include "qsstor.h"
#include "vio.h"

#define MIN_FAT16       4086    // min # of clusters for FAT16
#define MIN_FAT32      65526    // min # of clusters for FAT32
#define MAX_EXFAT 0x7FFFFFFD    // max # of exFAT clusters (limited by implementation)

#define FORCE_64K         48    // force 64k cluster starting from this number of Gbs

#define _FAT12             1
#define _FAT16             2
#define _FAT32             3

#define N_ROOTDIR        512    // number of root dir entries for FAT12/16
#define N_ROOTFDD        224
#define SZ_DIR            32    // directory entry size

extern char  bsect16[512],   // VBR code
             bsect32[512],
               bsectexf[];
extern u8t   exf_bsacount;   ///< # of addtional sectors in exFAT array

static int check_esc(u32t disk, u64t sector) {
   u16t key = key_wait(0);
   if ((key&0xFF)==27)
      if (confirm_dlg("Break format process?^Partition will be unusable!")) {
         // zero boot sector for incomplete partition format
         dsk_emptysector(disk|QDSK_DIRECT, sector, 1);
         return 1;
      }
   return 0;
}

#define CHECK_ESC()                               \
   if (kbask)                                     \
      if (check_esc(di.Disk, di.StartSector)) {   \
         if (pf) hlp_memfree(pf);                 \
         return E_SYS_UBREAK;                     \
      }

static u32t _std vol_scansurface(u32t disk, int checkonly, u64t startsector,
   u32t datapos, u32t clusters, u32t cloffset, u32t unitsize, dd_list bclist,
   void *buf32k, int kbask, read_callback cbprint)
{
   u32t   percent = 0,
         sectsize = dsk_sectorsize(disk),
           sin32k = _32KB/sectsize,          // sectors in 32k
               ii = (clusters - cloffset) * unitsize;
   u64t     total = ii,
            wsect = startsector + datapos + unitsize * cloffset;
   // print callback
   if (cbprint) cbprint(0,0);
   // data to write
   memset(buf32k, FMT_FILL, _32KB);

   disk |= QDSK_DIRECT;

   while (ii) {
      u32t  cpy = ii>sin32k?sin32k:ii,
        success = 0;
      // write 32k, read 32k and compare ;)
      if (checkonly || hlp_diskwrite(disk, wsect, cpy, buf32k)==cpy)
         if (hlp_diskread(disk, wsect, cpy, buf32k)==cpy)
            if (checkonly || !memchrnb((u8t*)buf32k, FMT_FILL, _32KB))
               success = 1;
      if (success) { // good 32k
         wsect+= cpy;
         ii   -= cpy;
      } else {       // at least one sector is bad, check it all
         u32t lastbad = bclist->max()>=0?bclist->value(bclist->max()):FFFF,
              cluster;
         // clear array after error
         memset(buf32k, FMT_FILL, _32KB);

         while (cpy) {
            cluster = (wsect-datapos-startsector) / unitsize + 2;
            // do not check other sectors of bad cluster
            if (cluster!=lastbad) {
               success = 0;
               if (checkonly || hlp_diskwrite(disk, wsect, 1, buf32k))
                  if (hlp_diskread(disk, wsect, 1, buf32k))
                     if (checkonly || !memchrnb((u8t*)buf32k, FMT_FILL, sectsize))
                        success = 1;
               // add cluster to bad list
               if (!success) {
                  memset(buf32k, FMT_FILL, sectsize);
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
      if (kbask)
         if (check_esc(disk, startsector)) return E_SYS_UBREAK;
   }
   if (bclist->count()) log_it(2, "%d bad clusters found\n", bclist->count());
   return 0;
}

qserr _std vol_format(u8t vol, u32t flags, u32t unitsize, read_callback cbprint) {
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
   if (!di.TotalSectors) return E_DSK_NOTMOUNTED;
   if (di.InfoFlags&VIF_VFS) return E_DSK_NOTPHYS;
   if (di.InfoFlags&VIF_NOWRITE) return E_DSK_WP;

   volidx = vol_index(vol,0);
   // allow floppies and big floppies
   if (volidx<0 && di.StartSector) return E_PTE_PINDEX;
   if (volidx>=0) {
      if (dsk_isgpt(di.Disk,volidx)==1) {
         // GPT partition
         flags |=DFMT_NOPTYPE;
         hidden = di.StartSector>=_4GBLL ? FFFF : di.StartSector;
      } else {
         // MBR or hybrid partition
         dsk_ptquery(di.Disk,volidx,0,0,0,0,&hidden);
         if (hidden==FFFF) return E_PTE_EXTERR;
      }
   }
   // turn off align for floppies
   if ((di.Disk&QDSK_FLOPPY)!=0) align = 0;
   // sectors in 4k
   sectin4k = 4096 / di.SectorSize;
   if (!sectin4k || 4096 % di.SectorSize) return E_DSK_SSIZE;
   /* unmount will clear all cache and below all of r/w ops use QDSK_DIRECT
      flag, so no any volume caching will occur until the end of format */
   if (io_unmount(vol, flags&DFMT_FORCE?IOUM_FORCE:0)) return E_DSK_UMOUNTERR;
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
      if ((u64t)di.TotalSectors*di.SectorSize < (u64t)_2GB && di.SectorSize<=_1KB) {
         // use IBM algorithm for a "common FAT"
         static u16t vsz[] = { 16, 128, 256, 512, 1024, 2048, 0};
         static u16t csz[] = {  8,   4,   8,  16,   32,   64, 0};
         // default
         u32t sperc = 4,
                tsz = di.TotalSectors*di.SectorSize;

         if (tsz <= _2GB-_1MB) tsz += _1MB - 1;
         tsz /= _1MB;

         for (ii=0; vsz[ii]; ii++)
            if (tsz < vsz[ii]) { sperc = csz[ii]; break; }

         unitsize = sperc*di.SectorSize;
      } else {
         static u16t vst[] = { 1024,  512, 256, 128,  64,   32,  16,   8,   4,   2,  0};
         static u16t cst[] = {32768,16384,8192,4096,2048,16384,8192,4096,2048,1024,512};

         u32t vs = di.TotalSectors / (2000 / (di.SectorSize / 512));
         for (ii = 0; vs < vst[ii]; ii++) ;
         unitsize = cst[ii];
      }
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
      return E_DSK_VSMALL;

   // determine number of clusters and final check of validity of the FAT type
   clusters = (di.TotalSectors - reserved - fatsize * fatcnt - dirsize) / unitsize;

   log_printf("Formatting vol %c, disk %02X, index %d to FAT%s\n",
      vol+'A', di.Disk, volidx, fmt==_FAT16?"16":(fmt==_FAT32?"32":"12"));
   log_it(2, "Reserved=%d, FAT size=%d, pos %010LX, Data pos %010LX, Clusters %u\n",
      reserved, fatsize, loc_fat, loc_data, clusters);

   if (fmt==_FAT16 && clusters < MIN_FAT16 || fmt==_FAT32 && clusters < MIN_FAT32)
      return E_DSK_SELTYPE;

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
      return E_DSK_ERRWRITE;

   pf     = (u32t*)hlp_memalloc(_32KB, QSMA_LOCAL),
   pfcnt  = _32KB/di.SectorSize;                       // sectors in 32k

   // clear "reserved" area
   if (reserved>1) {
      wsect = di.StartSector+1;
      ii    = reserved-1;
      while (ii) {
         u32t cpy = ii>pfcnt?pfcnt:ii;
         if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf)!=cpy) {
            hlp_memfree(pf);
            return E_DSK_ERRWRITE;
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
         if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf) != cpy) {
            hlp_memfree(pf);
            return E_DSK_ERRWRITE;
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
      if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf) != cpy) {
         hlp_memfree(pf);
         return E_DSK_ERRWRITE;
      }
      wsect+= cpy;
      ii   -= cpy;
      // check for esc
      CHECK_ESC();
   }

   // read/erase data area
   if ((flags&DFMT_WIPE) || (flags&DFMT_QUICK)==0) {
      dd_list bclist = NEW(dd_list);
      u32t   scanres = vol_scansurface(di.Disk, flags&DFMT_WIPE?0:1, di.StartSector,
         loc_data-di.StartSector, clusters, fmt==_FAT32?1:0, unitsize, bclist, pf,
            kbask, cbprint);
      if (scanres) {
         if (pf) hlp_memfree(pf);
         return scanres;
      }
      // bad clusters found? reinitialize FAT
      if (bclist->count()) {
         u32t epbuf;
         // sort bad cluster list
         bclist->sort(0,1);
         badcnt = bclist->count();
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
               if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf) != cpy) {
                  hlp_memfree(pf);
                  return E_DSK_ERRWRITE;
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
      qserr rc = dsk_setpttype(di.Disk, volidx, (u8t)ptbyte);
      log_it(2, "set partition type to %02X, rc %X\n", ptbyte, rc);
      if (rc) return rc;
   }
   // force rescanning of disk if it is a (big) floppy
   if (di.StartSector==0) dsk_ptrescan(di.Disk,1);
   // mount volume back
   if (!hlp_mountvol(vol, di.Disk, di.StartSector, di.TotalSectors))
      return E_DSK_MOUNTERR;
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

qserr _std vol_formatfs(u8t vol, char *fsname, u32t flags, u32t unitsize,
   read_callback cbprint)
{
   if (fsname) {
      if (stricmp(fsname,"FAT")==0)
         return vol_format(vol, flags, unitsize, cbprint);
      if (stricmp(fsname,"HPFS")==0)
         return hpfs_format(vol, flags, cbprint);
      if (stricmp(fsname,"EXFAT")==0)
         return exf_format(vol, flags, unitsize, cbprint);
   }
   return E_SYS_INVPARM;
}

u32t exf_sum(u8t src, u32t sum) {
   return (sum&1?0x80000000:0) + (sum>>1) + src;
}

qserr _std exf_format(u8t vol, u32t flags, u32t unitsize, read_callback cbprint) {
   int            align = flags&DFMT_NOALIGN?0:1,  // AF align flag
                  kbask = flags&DFMT_BREAK?1:0;    // allow keyboard ESC break
   u8t             *buf, secshift;
   long          volidx, ptbyte;
   u32t         bufsize, fatpos, fatsize, bmsize, bm_nsec, bufnsec, utsize,
                 badcnt = 0, sectin4k, datapos, ii, jj, sum, err, uptab_crc,
             n_clusters;
   u32t         ftab[3];
   u64t           wsect;
   bit_map          cbm = 0;
   dd_list       bclist = 0;
   qs_cpconvert     cpi = 0;
   struct Boot_RecExFAT *bre;
   disk_volume_data  di;

   hlp_volinfo(vol, &di);
   if (!di.TotalSectors) return E_DSK_NOTMOUNTED;
   if (di.InfoFlags&VIF_VFS) return E_DSK_NOTPHYS;
   if (di.TotalSectors<0x1000) return E_DSK_VSMALL;

   volidx = vol_index(vol,0);
   // allow floppies and big floppies
   if (volidx<0 && di.StartSector) return E_PTE_RESCAN;
   // GPT partition?
   if (volidx>=0)
      if (dsk_isgpt(di.Disk,volidx)==1) flags|=DFMT_NOPTYPE;
   // turn off align for floppies
   if ((di.Disk&QDSK_FLOPPY)!=0) align = 0;
   // sectors in 4k
   sectin4k = 4096 / di.SectorSize;
   if (!sectin4k || 4096 % di.SectorSize) return E_DSK_SSIZE;
   /* unmount will clear all cache and below all of r/w ops use QDSK_DIRECT
      flag, so no any volume caching will occur until the end of format */
   if (io_unmount(vol, flags&DFMT_FORCE?IOUM_FORCE:0)) return E_DSK_UMOUNTERR;
   // cluster size auto selection
   if (!unitsize) {
      unitsize = 8;
      if (di.TotalSectors>=0x70000000) unitsize = 512; else // >= 900Gb
      if (di.TotalSectors>=0x4000000) unitsize = 256; else  // >= 64MS
      if (di.TotalSectors>=0x80000) unitsize = 64;          // >= 512KS
   } else {
      // should be a single bit
      if (bsf32(unitsize)!=bsr32(unitsize)) return E_DSK_CLSIZE;
      unitsize /= di.SectorSize;
      if (unitsize == 0) unitsize = 1;
      if (unitsize*di.SectorSize > _32MB) unitsize = _32MB/di.SectorSize;
   }
   fatpos     = 32;
   // align FAT to 4k
   if (align)
      fatpos  = ((di.StartSector + fatpos + sectin4k - 1) & ~(sectin4k-1)) -
         di.StartSector;
   n_clusters = (di.TotalSectors - fatpos) / unitsize;
   fatsize    = (n_clusters * 4 + 8 + di.SectorSize - 1) / di.SectorSize;
   // align FAT size to 4k
   if (align) fatsize = fatsize + sectin4k - 1 & ~(sectin4k-1);
   // it must be aligned by FAT`s pos/size
   datapos    = fatpos + fatsize;
   secshift   = bsf32(di.SectorSize);
   n_clusters = (di.TotalSectors - datapos) / unitsize;

   if (datapos >= di.TotalSectors/2) return E_DSK_VSMALL;
   if (n_clusters<16) return E_DSK_VSMALL;
   if (n_clusters>MAX_EXFAT) return E_DSK_VLARGE;
   // unable to operate without CPLIB here!
   cpi     = NEW(qs_cpconvert);
   if (!cpi) return E_SYS_CPLIB;

   log_printf("Formatting vol %c, disk %02X, index %d to exFAT (%u)\n",
      vol+'A', di.Disk, volidx, unitsize);
   log_it(2, "FAT size=%u, pos %010LX, Data pos %010LX, Clusters %u\n",
      fatsize, di.StartSector+fatpos, di.StartSector+datapos, n_clusters);

   // size of cluster bitmap
   bmsize  = Round8(n_clusters)>>3;
   // # of bitmap file clusters & sectors
   ftab[0] = (bmsize + (unitsize<<secshift) - 1) / (unitsize<<secshift);
   bm_nsec = (bmsize + di.SectorSize - 1) / di.SectorSize;
   // # of rootdir clusters
   ftab[2] = 1;
   buf     = (u8t*)malloc_thread(bufsize = _32KB);
   bufnsec = bufsize>>secshift;    // will be at least 8

   // create a compressed uppercase table
   wsect  = di.StartSector + datapos + unitsize * ftab[0];  // start sector of table file
   err    = 0;
   if (cpi) {
      wchar_t si = 0, ch;
      u32t    st = 0;
      uptab_crc=0; ii=0; jj=0; utsize=0;
      do {
         switch (st) {
            case 0:
               ch = cpi->towupper(si);
               if (ch!=si) { si++; break; } // store the up-case char if exist
               // get run length of no-case block
               for (jj=1; (wchar_t)(si+jj) && (wchar_t)(si+jj)==cpi->towupper((wchar_t)(si+jj)); jj++) ;
               // compress the no-case block if run is >= 128
               if (jj>=128) { ch = 0xFFFF; st = 2; break; }
               // do not compress short run
               st = 1;
            case 1:
               ch = si++; // fill the short run
               if (--jj==0) st = 0;
               break;
            default:
               ch = (wchar_t)jj; si+=jj; // # of characters to skip
               st = 0;
         }
         uptab_crc = exf_sum(buf[ii+0] = (u8t)ch, uptab_crc);       /* Put it into the write buffer */
         uptab_crc = exf_sum(buf[ii+1] = (u8t)(ch>>8), uptab_crc);
         ii+=2; utsize+=2;
         if (!si || ii==bufsize) {      /* Write buffered data when buffer full or end of process */
            sum = ii + di.SectorSize - 1 >> secshift;
            if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, sum, buf)!=sum) {
               err = E_DSK_ERRWRITE;
               break;
            }
            wsect+=sum; ii=0;
         }
      } while (si);
      DELETE(cpi);
      cpi  = 0;
   }
   if (!err) {
      u32t ncused;
      // # of upcase file clusters
      ftab[1] = (utsize + (unitsize<<secshift) - 1) / (unitsize<<secshift);
      cbm     = NEW(bit_map);
      // make internal bitmap buffer sector-aligned to write from it directly
      cbm->alloc(bm_nsec<<secshift+3);
      log_it(2, "n_clusters %X cbm alloc %X\n", n_clusters, cbm->size());
      // mark used clusters
      ncused = ftab[0]+ftab[1]+ftab[2];
      cbm->set(1, 0, ncused);

      // read/erase data area
      if ((flags&DFMT_WIPE) || (flags&DFMT_QUICK)==0) {
         bclist = NEW(dd_list);
         err    = vol_scansurface(di.Disk, flags&DFMT_WIPE?0:1, di.StartSector,
            datapos, n_clusters, ncused, unitsize, bclist, buf, kbask, cbprint);
         badcnt = bclist->count();
      }
   }
   if (!err) {
      bre    = (struct Boot_RecExFAT*)buf;
      memset(bre, 0, 1+exf_bsacount<<secshift);
      // we should never reach 4k * 8 sectors
      for (ii = 0; ii<=exf_bsacount; ii++)
         memcpy(buf+(ii<<secshift), &bsectexf[ii*512], 512);
      // exFAT bpb
      bre->BR_ExF_VolStart = di.StartSector;
      bre->BR_ExF_VolSize  = di.TotalSectors;
      bre->BR_ExF_FATPos   = fatpos;
      bre->BR_ExF_FATSize  = fatsize;
      bre->BR_ExF_DataPos  = datapos;
      bre->BR_ExF_NumClus  = n_clusters;
      bre->BR_ExF_RootClus = 2 + ftab[0] + ftab[1];
      bre->BR_ExF_Serial   = tm_getdate();
      bre->BR_ExF_FsVer    = 0x100;
      bre->BR_ExF_SecSize  = secshift;
      bre->BR_ExF_ClusSize = bsf32(unitsize);
      bre->BR_ExF_FATCnt   = 1;
      bre->BR_ExF_PhysDisk = 0x80;

      if (!exf_updatevbr(di.Disk|QDSK_DIRECT, di.StartSector, buf, 1+exf_bsacount, 1))
         err = E_DSK_ERRWRITE;
   }
   // write FAT
   if (!err) {
      u32t *pf = (u32t*)buf, pv, clpos, bpos;
      wsect = di.StartSector+fatpos;
      // zero buffer
      memset(pf, 0, _32KB);
      pf[0] = 0xFFFFFFF8;
      pf[1] = FFFF;
      // entries for bitmap, upcase file and root dir
      for (jj = 0, ii = 2; jj < 3; jj++)
         for (pv = ftab[jj]; pv; pv--) {  pf[ii] = pv>=2?ii+1:FFFF; ii++; }
      // fill FAT entries
      pv    = fatsize;
      clpos = 0;
      bpos  = 0;
      while (pv) {
         u32t cpy = pv>bufnsec ? bufnsec : pv;
         // fill bad entries for current potion of FAT
         if (bclist)
            while (bpos<bclist->count() && bclist->value(bpos) < clpos+_32KB/4) {
               u32t bb = bclist->value(bpos++);
               // mask cluster as bad both in FAT & bitmap
               cbm->set(1, bb-2, 1);
               pf[bb-clpos] = 0xFFFFFFF7;
            }
         clpos+= _32KB/4;
         // write this 32k (i/o buffer size)
         if (hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, cpy, pf) != cpy) {
            err = E_DSK_ERRWRITE;
            break;
         }
         wsect+= cpy;
         pv   -= cpy;
         // zero buffer
         memset(pf, 0, _32KB);
      }
   }
   // write bitmap
   if (!err)
      if (hlp_diskwrite(di.Disk|QDSK_DIRECT, di.StartSector+datapos, bm_nsec,
         cbm->mem()) < bm_nsec) err = E_DSK_ERRWRITE;
   // write root directory
   if (!err) {
      memset(buf, 0, di.SectorSize);
      buf[SZ_DIR * 0 + 0] = 0x83;     /* 83 entry (volume label) */
      buf[SZ_DIR * 1 + 0] = 0x81;     /* 81 entry (allocation bitmap) */
      *(u32t*)(buf + SZ_DIR * 1 + 20) = 2;
      *(u32t*)(buf + SZ_DIR * 1 + 24) = bmsize;
      buf[SZ_DIR * 2 + 0] = 0x82;     /* 82 entry (up-case table) */
      *(u32t*)(buf + SZ_DIR * 2 + 4)  = uptab_crc;
      *(u32t*)(buf + SZ_DIR * 2 + 20) = 2 + ftab[0];
      *(u32t*)(buf + SZ_DIR * 2 + 24) = utsize;
      wsect = di.StartSector + datapos + unitsize * (ftab[0] + ftab[1]);

      if (!hlp_diskwrite(di.Disk|QDSK_DIRECT, wsect, 1, buf) ||
         unitsize>1 && dsk_emptysector(di.Disk|QDSK_DIRECT, wsect+1, unitsize-1))
            err = E_DSK_ERRWRITE;
   }
   if (bclist) DELETE(bclist);
   DELETE(cbm);
   free(buf);

   if (err) {
      // error - wipe boot sector & exit
      dsk_emptysector(di.Disk|QDSK_DIRECT, di.StartSector, 1);
      return err;
   }
   // update partition type & mount volume back
   return format_done(vol, di, flags&DFMT_NOPTYPE?-1:PTE_07_MSFS, volidx, badcnt);
}
