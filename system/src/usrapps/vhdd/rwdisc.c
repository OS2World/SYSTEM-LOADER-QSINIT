//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
// ------------------------------------------------------------------
// code is not optimized at all, but it written only for PARTMGR
// testing, which requires a small number of sectors.
//
#include "rwdisk.h"
#include "stdlib.h"
#include "qslog.h"
#include "errno.h"
#include "ioint13.h"
#include "dskinfo.h"

#define SLICE_SECTORS          1024
#define HEADER_SPACE           4096
#define VHDD_SIGN        0x64644856          // VHdd
#define VHDD_VERSION     0x00010000

static u32t classid = 0;

typedef struct {
   u32t          sign;
   emudisk    selfptr;    // pointer to own instance
   s32t        qsdisk;
   FILE           *df;    // disk file
   char       *dfname;    // disk file name
   u64t        *index;
   u32t        slices;
   u32t        lshift;    // sector size shift value
   disk_geo_data info;
} diskdata;

// interface array for every possible disk number
emudisk mounted[QDSK_DISKMASK+1];

#define instance_ret(inst,err)              \
   diskdata *inst = (diskdata*)data;        \
   if (inst->sign!=VHDD_SIGN) return err;

#define instance_void(inst)                 \
   diskdata *inst = (diskdata*)data;        \
   if (inst->sign!=VHDD_SIGN) return;

u32t _std diskio_read (u32t disk, u64t sector, u32t count, void *data) {
   disk &= ~QDSK_DIRECT;
   if (disk>QDSK_DISKMASK) return 0;
   if (!mounted[disk]) return 0;
   return mounted[disk]->read(sector, count, data);
}

u32t _std diskio_write(u32t disk, u64t sector, u32t count, void *data) {
   disk &= ~QDSK_DIRECT;
   if (disk>QDSK_DISKMASK) return 0;
   if (!mounted[disk]) return 0;
   return mounted[disk]->write(sector, count, data);
}

/// 32-bit offset in file
static u32t ed_sectofs(diskdata *di, u64t sector) {
   if (!di->df||!di->index||!di->slices) return 0; else {
      u64t *ofs = memchrq(di->index, sector, di->slices*SLICE_SECTORS), pos;
      if (!ofs) return 0;
      pos = ofs - di->index;
      return HEADER_SPACE + ((SLICE_SECTORS<<di->lshift) + SLICE_SECTORS*8) * 
         (pos/SLICE_SECTORS) + (pos%SLICE_SECTORS<<di->lshift);
   }
}

static int flush_slices(diskdata *di) {
   if (!di->df) return 0; else {
      // step size
      u32t rsize = (SLICE_SECTORS<<di->lshift) + SLICE_SECTORS*8, ii;
      // expand/shrink file
      if (chsize(fileno(di->df), HEADER_SPACE + rsize*di->slices)) return 0;
      // commit indexes
      for (ii=0; ii<di->slices; ii++) {
         if (fseek(di->df, HEADER_SPACE + (ii+1)*rsize-SLICE_SECTORS*8, SEEK_SET))
            return 0;
         if (fwrite(di->index+SLICE_SECTORS*ii, 1, SLICE_SECTORS*8, di->df) != SLICE_SECTORS*8)
            return 0;
      }
      return 1;
   }
}

// allocate space for new sectors
static int extend_disk(diskdata *di, u64t sector, u32t count) {
   if (!di->df||!di->index||!di->slices) return 0; else {
      u64t *fp = di->index;
      while (count--) {
         if (!ed_sectofs(di, sector)) {
            fp = memchrq(fp, FFFF64, di->slices*SLICE_SECTORS - (fp - di->index));
            if (!fp) { // alloc new slice
               di->index = (u64t*)realloc(di->index, SLICE_SECTORS*8*(di->slices+1));
               fp    = di->index + SLICE_SECTORS*di->slices++;
               memsetq(fp, FFFF64, SLICE_SECTORS);
            }
            *fp = sector;
         }
         sector++;
      }
      return flush_slices(di);
   }
}

// free disk data (sectors will be assumed as zeroed)
static int shrink_disk(diskdata *di, u64t sector, u32t count) {
   if (!di->df||!di->index||!di->slices) return 0;
   while (count--) {
      u64t *ofs = memchrq(di->index, sector++, di->slices*SLICE_SECTORS);
      if (ofs) *ofs = FFFF64;
   }
   // truncate empty slices at the end of file
   while (di->slices>1) {
      u64t *ofs = memchrnq(di->index+(di->slices-1)*SLICE_SECTORS, FFFF64, SLICE_SECTORS);
      if (ofs) break; else di->slices--;
   }
   return flush_slices(di);
}

/// close image file
static int _std dsk_close(void *data) {
   instance_ret(di,EINVAL);

   if (!di->df) return ENODEV;
   fclose(di->df);
   if (di->index) free(di->index);
   if (di->dfname) free(di->dfname);
   di->df     = 0;
   di->dfname = 0;
   di->index  = 0;
   di->slices = 0;
   return 0;
}

/// create new image file
static int _std dsk_make(void *data, const char *fname, u32t sectorsize, u64t sectors) {
   u32t  shift;
   instance_ret(di,EINVAL);
   // already opened?
   if (di->df) return EINVOP;
   // 512 tb is enough for everything! (c) ;)
   if (sectors>0xFFFFFFFFFFLL) return EFBIG;
   // check sectorsize for onebit-ness & range 512..4096
   shift = bsf64(sectorsize);
   if (bsr64(sectorsize)!=shift) return EINVAL;
   if (shift<9 || shift>12) return EINVAL;
   // file exists? return err!
   if (!access(fname,F_OK)) return EEXIST;
   // create it
   di->df = fopen(fname, "w+b");
   if (!di->df) return errno?errno:ENOENT; else {
      // preallocate initial file space
      if (chsize(fileno(di->df), HEADER_SPACE+(SLICE_SECTORS<<shift)+SLICE_SECTORS*8)) {
         fclose(di->df);
         di->df = 0;
         unlink(fname);
         return EIO;
      } else {
         u32t  mhdr[2];
     
         mhdr[0] = VHDD_SIGN;
         mhdr[1] = VHDD_VERSION;
         
         di->info.TotalSectors = sectors;
         di->info.Cylinders    = 1024;   
         di->info.Heads        = 31;       
         di->info.SectOnTrack  = 17; 

         while (di->info.Cylinders*di->info.SectOnTrack*di->info.Heads < di->info.TotalSectors) {
            if (di->info.SectOnTrack<63) {
               di->info.SectOnTrack = (di->info.SectOnTrack + 1 & 0xF0) * 2 - 1; continue; 
            }
            if (di->info.Heads<255) {
               di->info.Heads = (di->info.Heads + 1 & 0xF0) * 2 - 1; continue; 
            }
            break;
         }
         di->info.Cylinders = di->info.TotalSectors / (di->info.Heads*di->info.SectOnTrack);

         di->info.SectorSize   = sectorsize;
         di->lshift = shift;
         di->slices = 1;
         di->index  = (u64t*)malloc(SLICE_SECTORS*8);
         memsetq(di->index, FFFF64, SLICE_SECTORS);

         // detach file to make it process-independent
         fdetach(di->df);
         // write header
         fseek(di->df, 0, SEEK_SET);
         fwrite(&mhdr, 1, 8, di->df);
         fwrite(&di->info, 1, sizeof(disk_geo_data), di->df);
         // save full file name
         di->dfname = _fullpath(0, fname, 0);
         // commit 1st slice index
         return flush_slices(di)?0:EIO;
      }
   }
}

/// open existing image file
static int _std dsk_open(void *data, const char *fname) {
   instance_ret(di,EINVAL);
   // already opened?
   if (di->df) return EINVOP;

   di->df = fopen(fname, "r+b");
   if (!di->df) return errno?errno:ENOENT; else {
      u32t   ii, rc = 0, mhdr[2], rsize,
             sz = filelength(fileno(di->df));

      fread(&mhdr, 1, 8, di->df);
      if (fread(&di->info, 1, sizeof(disk_geo_data), di->df)!=sizeof(disk_geo_data))
         rc = EIO;

      if (mhdr[0]!=VHDD_SIGN || mhdr[1]!=VHDD_VERSION) rc = EBADF;
      if (!rc) {
         di->lshift = bsf64(di->info.SectorSize);
         rsize      = (SLICE_SECTORS<<di->lshift) + SLICE_SECTORS*8;
         if (sz<HEADER_SPACE+rsize) rc = EIO;
      }
      if (!rc) {
         u32t  slices = sz - HEADER_SPACE;

         if (slices % rsize)
            log_printf("%s is not aligned to slice (%d + %d bytes)!\n",
               fname, slices/rsize, slices%rsize);
         di->slices = slices/rsize;
         
         di->index = (u64t*)malloc(di->slices*SLICE_SECTORS*8);
         for (ii=0; ii<di->slices; ii++) {
            if (fseek(di->df, HEADER_SPACE + rsize*(ii+1) - SLICE_SECTORS*8, SEEK_SET))
               { rc = EIO; break; }
            if (fread(di->index + SLICE_SECTORS*ii, 1, SLICE_SECTORS*8, di->df)!=SLICE_SECTORS*8)
               { rc = EIO; break; }
         }
      }
      // detach it to make process-independent
      if (rc) dsk_close(data); else {
         fdetach(di->df);
         // save full file name
         di->dfname = _fullpath(0, fname, 0);
      }
      return rc;
   }
}

/// query "disk" info
static int _std dsk_query(void *data, disk_geo_data *info, char *fname, u32t *sectors, u32t *used) {
   instance_ret(di,EINVAL);
   if (!info) return EINVAL;

   if (info)
      if (di->df) memcpy(info, &di->info, sizeof(disk_geo_data));
         else memset(info, 0, sizeof(disk_geo_data));
   if (fname)
      if (di->dfname) strcpy(fname, di->dfname); else *fname = 0;
   if (sectors) *sectors = di->slices * SLICE_SECTORS;
   if (used) {
      u32t ii, cnt = 0;
      if (di->index)
         for (ii=0; ii<di->slices * SLICE_SECTORS; ii++)
            if (di->index[ii]!=FFFF64) cnt++;
      *used = cnt;
   }
   return 0;
}

/// read "sectors"
static u32t _std dsk_read(void *data, u64t sector, u32t count, void *usrdata) {
   instance_ret(di,0);
   // no file?
   if (!di->df) return 0;
   // check limits
   if (sector>=di->info.TotalSectors) return 0;
   if (sector+count>di->info.TotalSectors) count = di->info.TotalSectors - sector;

   if (count) {
      u32t ii;
      for (ii=0; ii<count; ii++) {
         u32t ofs = ed_sectofs(di, ii+sector);
         // sector available?
         if (!ofs) memset((char*)usrdata+(ii<<di->lshift), 0, 1<<di->lshift); else {
            if (fseek(di->df, ofs, SEEK_SET)) return ii;
            if (fread((char*)usrdata + (ii<<di->lshift), 1, 1<<di->lshift, di->df)!=1<<di->lshift)
               return ii;
         }
      }
      return count;
   }
   return 0;
}

/// write "sectors"
static u32t _std dsk_write(void *data, u64t sector, u32t count, void *usrdata) {
   instance_ret(di,0);
   // no file?
   if (!di->df) return 0;
   // check limits
   if (sector>=di->info.TotalSectors) return 0;
   if (sector+count>di->info.TotalSectors) count = di->info.TotalSectors - sector;

   if (count) {
      u32t  ii = 0;
      int zerodata = memchrnb(usrdata,0,count<<di->lshift)?0:1;
      // is all data zeroed?
      if (zerodata) { shrink_disk(di,sector,count); return count; }
      // check/allocate space
      if (!extend_disk(di,sector,count)) return 0;

      for (ii=0; ii<count; ii++) {
         u32t ofs = ed_sectofs(di, ii+sector);
         if (fseek(di->df, ofs, SEEK_SET)) return ii;
         if (fwrite((char*)usrdata + (ii<<di->lshift), 1, 1<<di->lshift, di->df)!=1<<di->lshift)
            return ii;
      }
      return count;
   }
   return 0;
}

/** mount as QSINIT disk.
    @return disk number or negative value on error */
static s32t _std dsk_mount(void *data) {
   struct qs_diskinfo dp;
   instance_ret(di,-1);

   dp.qd_sectors     = di->info.TotalSectors;
   dp.qd_cyls        = di->info.Cylinders;
   dp.qd_heads       = di->info.Heads;
   dp.qd_spt         = di->info.SectOnTrack;
   dp.qd_extread     = diskio_read;
   dp.qd_extwrite    = diskio_write;
   dp.qd_sectorsize  = di->info.SectorSize;
   dp.qd_sectorshift = di->lshift;

   // install new "hdd"
   di->qsdisk = hlp_diskadd(&dp);
   if (di->qsdisk<0 || di->qsdisk>QDSK_DISKMASK) log_printf("hlp_diskadd error!\n");
      else mounted[di->qsdisk] = di->selfptr;

   return di->qsdisk;
}

/** query QSINIT disk number.
    @return disk number or negative value if not mounted */
static s32t _std dsk_disk(void *data) {
   instance_ret(di,-1);
   return di->qsdisk;
}

/// unmount "disk" from QSINIT
static int _std dsk_umount(void *data) {
   instance_ret(di,EINVAL);

   if (di->qsdisk>=0) {
      if (di->qsdisk<=QDSK_DISKMASK) mounted[di->qsdisk] = 0;
      hlp_diskremove(di->qsdisk);
      di->qsdisk = -1;
      return 0;
   }
   return ENOMNT;
}

static u32t _std dsk_compact(void *data, int freeF6) {
   u8t  *sptr;
   u64t *sidx;
   instance_ret(di,0);
   if (!di->df||!di->index||!di->slices) return 0;

   sptr = hlp_memalloc(SLICE_SECTORS<<di->lshift, QSMA_RETERR|QSMA_NOCLEAR);
   sidx = (u64t*)malloc(SLICE_SECTORS*8);

   if (!sptr) return 0; else {
      u32t   ii, fcount = 0, idx,
          rsize = (SLICE_SECTORS<<di->lshift) + SLICE_SECTORS*8;

      for (ii=0; ii<di->slices; ii++) {
         u32t  ncount = fcount;
         if (fseek(di->df, HEADER_SPACE + ii*rsize, SEEK_SET)) break;
         if (fread(sptr, 1, SLICE_SECTORS<<di->lshift, di->df) != SLICE_SECTORS<<di->lshift)
            break;
         if (fread(sidx, 1, SLICE_SECTORS*8, di->df) != SLICE_SECTORS*8) break;
         // check for empty data in used sectors
         for (idx=0; idx<SLICE_SECTORS; idx++)
            if (sidx[idx]!=FFFF64)
               if (memchrnb(sptr+(idx<<di->lshift), 0, 1<<di->lshift)==0 || freeF6 &&
                  memchrnb(sptr+(idx<<di->lshift), 0xF6, 1<<di->lshift)==0)
                     { sidx[idx]=FFFF64; ncount++; }
         // update index
         if (fcount!=ncount) {
            if (fseek(di->df, -SLICE_SECTORS*8, SEEK_CUR)) break;
            if (fwrite(sidx, 1, SLICE_SECTORS*8, di->df)!=SLICE_SECTORS*8) break;
         }
         fcount = ncount;
      }
      hlp_memfree(sptr);
      free(sidx);
      return fcount;
   }
}

static void *methods_list[] = { dsk_make, dsk_open, dsk_query, dsk_read,
   dsk_write, dsk_compact, dsk_mount, dsk_disk, dsk_umount, dsk_close };

static void _std dsk_init(void *instance, void *data) {
   diskdata *di = (diskdata*)data;
   di->sign     = VHDD_SIGN;
   di->qsdisk   = -1;
   di->df       = 0;
   di->dfname   = 0;
   di->index    = 0;
   di->slices   = 0;
   di->selfptr  = instance;
}

static void _std dsk_done(void *instance, void *data) {
   instance_void(di);

   if (di->qsdisk>=0) dsk_umount(data);
   if (di->df) dsk_close(data);

   memset(di, 0, sizeof(diskdata));
}

int init_rwdisk(void) {
   if (!classid) {
      classid = exi_register("emudisk", methods_list, sizeof(methods_list)/sizeof(void*),
         sizeof(diskdata), dsk_init, dsk_done);

      memset(mounted, 0, sizeof(mounted));
   }
   return classid?1:0;
}

void done_rwdisk(void) {
   if (classid) {
      u32t ii;
      // call delete, it calls umount & close disk file for every mounted disk
      for (ii=0; ii<=QDSK_DISKMASK; ii++)
         if (mounted[ii]) exi_free(mounted[ii]);

      memset(mounted, 0, sizeof(mounted));
      // unregister class
      exi_unregister(classid);
      classid = 0;
   }
}
