//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
// ------------------------------------------------------------------
// code is not optimized at all, but it written only for PARTMGR
// testing, which requires a small number of sectors.
//
#include "stdlib.h"
#include "qslog.h"
#include "errno.h"
#include "dskinfo.h"
#include "qcl/rwdisk.h"
#include "qcl/bitmaps.h"
#include "qcl/qsedinfo.h"

#define SLICE_SECTORS          1024   ///< "sectors" per one index slice
#define HEADER_SPACE           4096
#define VHDD_SIGN        0x64644856   ///< VHdd string
#define VHDD_VERSION         0x0001

#define VHDD_OFSDD           0x0001   ///< 32-bit sector number

static u32t classid_emu = 0,          ///< qs_emudisk class id
            classid_ext = 0;          ///< qs_extdisk compatible class id

/// "qs_emudisk" class data
typedef struct {
   u32t           sign;
   qs_emudisk  selfptr;    // pointer to own instance
   s32t         qsdisk;
   FILE            *df;    // disk file
   char        *dfname;    // disk file name
   void         *index;
   u32t         slices;
   u32t         lshift;    // sector size shift value
   u32t       idxshift;
   disk_geo_data  info;
   bit_map         cbm;    // changed index slices bitmap
   qs_extdisk    einfo;    // attached to this disk ext.info class
} diskdata;

/// "qs_emudisk_ext" class data
typedef struct {
   u32t           sign;
   diskdata      *self;
} doptdata;

typedef struct {
   u32t           sign;
   u16t          flags;
   u16t        version;
} file_header;

// interface array for every possible disk number
qs_emudisk mounted[QDSK_DISKMASK+1];

#define instance_ret(type,inst,err)   \
   type *inst = (type*)data;          \
   if (inst->sign!=VHDD_SIGN) return err;

#define instance_void(type,inst)      \
   type *inst = (type*)data;          \
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

static inline u32t index_to_offset(diskdata *di, u32t idxpos) {
   return HEADER_SPACE + ((SLICE_SECTORS<<di->lshift) + 
      (SLICE_SECTORS<<di->idxshift)) * (idxpos/SLICE_SECTORS) + 
         (idxpos%SLICE_SECTORS<<di->lshift);
}

/// 32-bit offset in file
static u32t ed_sectofs(diskdata *di, u64t sector) {
   if (!di->df||!di->index||!di->slices) return 0; else {
      u32t idxpos;
      if (di->idxshift==3) {
         u64t *ofs = memchrq((u64t*)di->index, sector, di->slices*SLICE_SECTORS);
         if (!ofs) return 0;
         idxpos = ofs - (u64t*)di->index;
      } else {
         u32t *ofs = memchrd((u32t*)di->index, sector, di->slices*SLICE_SECTORS);
         if (!ofs) return 0;
         idxpos = ofs - (u32t*)di->index;
      }
      return index_to_offset(di, idxpos);
   }
}

static int flush_slices(diskdata *di) {
   if (!di->df) return 0; else {
      // step size
      u32t rsize = (SLICE_SECTORS<<di->lshift) + (SLICE_SECTORS<<di->idxshift), ii;
      // expand/shrink file
      if (chsize(fileno(di->df), HEADER_SPACE + rsize*di->slices)) return 0;
      // commit disk size/allocation
      fflush(di->df);
      // commit changed indexes
      ii = 0;
      while (1) {
         ii = di->cbm->findset(1, 1, ii);
         if (ii==FFFF) break;
         if (fseek(di->df, HEADER_SPACE + (ii+1)*rsize-(SLICE_SECTORS<<di->idxshift),
            SEEK_SET)) return 0;
         if (fwrite((u8t*)di->index + (SLICE_SECTORS*ii<<di->idxshift), 1,
            SLICE_SECTORS<<di->idxshift, di->df) != SLICE_SECTORS<<di->idxshift)
               return 0;
      }
      return 1;
   }
}

/** allocate one sector without check for presence!
    @param hint   Pos to start search from
    @return position of allocated sector (first founded free index) */
static u32t alloc_one(diskdata *di, u64t sector, u32t hint) {
   u32t maxnow = di->slices*SLICE_SECTORS;

   if (!di->df||!di->index||!di->slices||hint>maxnow) return 0; else {
      u8t     *fp = 0;
      int flushme = 0;
      // search free pos
      if (hint < maxnow)
         fp = di->idxshift==2 ? 
            (u8t*)memchrd((u32t*)di->index + hint, FFFF, maxnow - hint):
            (u8t*)memchrq((u64t*)di->index + hint, FFFF64, maxnow - hint);
      if (!fp) { // alloc new slice
         di->index = realloc(di->index, (SLICE_SECTORS<<di->idxshift)*(di->slices+1));
         fp = (u8t*)di->index + (SLICE_SECTORS*di->slices++<<di->idxshift);
         memset(fp, 0xFF, SLICE_SECTORS<<di->idxshift);

         di->cbm->resize(di->slices);
         flushme = 1;
      }
      if (di->idxshift==3) *(u64t*)fp = sector; else *(u32t*)fp = sector;
      hint = fp - (u8t*)di->index >> di->idxshift;

      di->cbm->set(1, hint/SLICE_SECTORS, 1);
      // flush only on creation of new slice!
      if (flushme) flush_slices(di);

      return hint;
   }
}

// free disk data (sectors will be assumed as zeroed)
static int shrink_disk(diskdata *di, u64t sector, u32t count) {
   if (!di->df||!di->index||!di->slices) return 0;
   while (count--) {
      if (di->idxshift==3) {
         u64t *ofs = memchrq((u64t*)di->index, sector++, di->slices*SLICE_SECTORS);
         if (ofs) {
            *ofs = FFFF64;
            // flag for update
            di->cbm->set(1, (ofs - (u64t*)di->index)/SLICE_SECTORS, 1);
         }
      } else {
         u32t *ofs = memchrd((u32t*)di->index, sector++, di->slices*SLICE_SECTORS);
         if (ofs) {
            *ofs = FFFF;
            // flag for update
            di->cbm->set(1, (ofs - (u32t*)di->index)/SLICE_SECTORS, 1);
         }
      }
   }
   // truncate empty slices at the end of file
   while (di->slices>1) {
      // must be a whole slice index of pure 0xFFs (both 64-bit or 32-bit)
      u8t *ofs = memchrnb((u8t*)di->index+((di->slices-1)*SLICE_SECTORS<<di->idxshift),
         0xFF, SLICE_SECTORS<<di->idxshift);
      if (ofs) break; else di->cbm->resize(--di->slices);
   }
   return flush_slices(di);
}

/// close image file
static int _std dsk_close(void *data) {
   instance_ret(diskdata,di,EINVAL);

   if (!di->df) return ENODEV;
   // flush something forgotten
   flush_slices(di);
   // and close file
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
   instance_ret(diskdata,di,EINVAL);
   // already opened?
   if (di->df) return EINVOP;
   // 1 pb is enough for everything! (c) ;)
   if (sectors>0x1FFFFFFFFFFLL) return EFBIG;
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
      if (chsize(fileno(di->df), HEADER_SPACE+(SLICE_SECTORS<<shift)+(SLICE_SECTORS<<di->idxshift))) {
         fclose(di->df);
         di->df = 0;
         unlink(fname);
         return EIO;
      } else {
         file_header  mhdr;
     
         mhdr.sign    = VHDD_SIGN;
         mhdr.version = VHDD_VERSION;
         mhdr.flags   = sectors>=_4GBLL ? 0 : VHDD_OFSDD;
         di->idxshift = mhdr.flags&VHDD_OFSDD ? 2 : 3;

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
         di->index  = malloc(SLICE_SECTORS<<di->idxshift);
         memset(di->index, 0xFF, SLICE_SECTORS<<di->idxshift);
         // realloc/zero cbm
         di->cbm->alloc(1);
         di->cbm->set(1, 0, 1);

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
   instance_ret(diskdata,di,EINVAL);
   // already opened?
   if (di->df) return EINVOP;

   di->df = fopen(fname, "r+b");
   if (!di->df) return errno?errno:ENOENT; else {
      file_header  mhdr;
      u32t   ii, rc = 0, rsize,
                 sz = filelength(fileno(di->df));

      fread(&mhdr, 1, 8, di->df);
      if (fread(&di->info, 1, sizeof(disk_geo_data), di->df)!=sizeof(disk_geo_data))
         rc = EIO;

      if (mhdr.sign!=VHDD_SIGN || mhdr.version!=VHDD_VERSION) rc = EBADF;
      if (!rc) {
         di->lshift   = bsf64(di->info.SectorSize);
         di->idxshift = mhdr.flags&VHDD_OFSDD ? 2 : 3;
         rsize        = (SLICE_SECTORS<<di->lshift) + (SLICE_SECTORS<<di->idxshift);
         if (sz<HEADER_SPACE+rsize) rc = EIO;
      }
      if (!rc) {
         u32t  slices = sz - HEADER_SPACE;
         void *idxmem;

         if (slices % rsize)
            log_printf("%s is not aligned to slice (%d + %d bytes)!\n",
               fname, slices/rsize, slices%rsize);
         di->slices = slices/rsize;
         // realloc/zero cbm
         di->cbm->alloc(di->slices);
         
         di->index = malloc(di->slices*SLICE_SECTORS<<di->idxshift);
         for (ii=0; ii<di->slices; ii++) {
            if (fseek(di->df, HEADER_SPACE + rsize*(ii+1) - (SLICE_SECTORS<<di->idxshift),
               SEEK_SET)) { rc = EIO; break; }
            if (fread((u8t*)di->index + (SLICE_SECTORS*ii<<di->idxshift), 1,
               SLICE_SECTORS<<di->idxshift, di->df) != SLICE_SECTORS<<di->idxshift)
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
   instance_ret(diskdata,di,EINVAL);
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
            if (di->idxshift==3) {
               if (((u64t*)di->index)[ii]!=FFFF64) cnt++;
            } else {
               if (((u32t*)di->index)[ii]!=FFFF) cnt++;
            }
      *used = cnt;
   }
   return 0;
}

/// read "sectors"
static u32t _std dsk_read(void *data, u64t sector, u32t count, void *usrdata) {
   instance_ret(diskdata,di,0);
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
   u32t   ii, lastindex = 0, rc = count;
   instance_ret(diskdata,di,0);
   // no file?
   if (!di->df) return 0;
   // check limits
   if (sector>=di->info.TotalSectors) return 0;
   if (sector+count>di->info.TotalSectors) count = di->info.TotalSectors - sector;

   for (ii=0; ii<count; ii++, sector++) {
      u32t pos = ed_sectofs(di, sector);
      // is sector zero filled?
      if (memchrnb((char*)usrdata+(ii<<di->lshift),0,1<<di->lshift)==0)
         shrink_disk(di,sector,1);
      else {
         // allocate storage for a sector
         if (!pos) {
            lastindex = alloc_one(di, sector, lastindex+1);
            if (!lastindex) { rc = 0; break; }
            pos = index_to_offset(di, lastindex);
         }
         if (fseek(di->df, pos, SEEK_SET)) { rc = ii; break; }
         if (fwrite((char*)usrdata + (ii<<di->lshift), 1, 1<<di->lshift, di->df)!=1<<di->lshift)
            { rc = ii; break; }
      }
   }
   // alloc_one() requires index update at the end
   flush_slices(di);
   return rc;
}

/** mount as QSINIT disk.
    @return disk number or negative value on error */
static s32t _std dsk_mount(void *data) {
   struct qs_diskinfo dp;
   instance_ret(diskdata,di,-1);

   memset(&dp, 0, sizeof(dp));
   dp.qd_sectors     = di->info.TotalSectors;
   dp.qd_cyls        = di->info.Cylinders;
   dp.qd_heads       = di->info.Heads;
   dp.qd_spt         = di->info.SectOnTrack;
   dp.qd_extread     = diskio_read;
   dp.qd_extwrite    = diskio_write;
   dp.qd_sectorsize  = di->info.SectorSize;
   dp.qd_sectorshift = di->lshift;

   if (classid_ext) {
      qs_extdisk  eptr = (qs_extdisk)exi_createid(classid_ext);
      if (eptr) {
         doptdata *dod = (doptdata*)exi_instdata(eptr);
         dod->self     = di;
         dp.qd_extptr  = eptr;
         di->einfo     = eptr;
      }
   }
   // install new "hdd"
   di->qsdisk = hlp_diskadd(&dp);
   if (di->qsdisk<0 || di->qsdisk>QDSK_DISKMASK) log_printf("hlp_diskadd error!\n");
      else mounted[di->qsdisk] = di->selfptr;

   return di->qsdisk;
}

/** query QSINIT disk number.
    @return disk number or negative value if not mounted */
static s32t _std dsk_disk(void *data) {
   instance_ret(diskdata,di,-1);
   return di->qsdisk;
}

/// unmount "disk" from QSINIT
static int _std dsk_umount(void *data) {
   instance_ret(diskdata,di,EINVAL);

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
   void *sidx;
   instance_ret(diskdata,di,0);
   if (!di->df||!di->index||!di->slices) return 0;

   sptr = hlp_memalloc(SLICE_SECTORS<<di->lshift, QSMA_RETERR|QSMA_NOCLEAR);
   sidx = malloc(SLICE_SECTORS<<di->idxshift);

   if (!sptr) return 0; else {
      u32t   ii, fcount = 0, idx,
          rsize = (SLICE_SECTORS<<di->lshift) + (SLICE_SECTORS<<di->idxshift);
      // flush it first!
      flush_slices(di);

      for (ii=0; ii<di->slices; ii++) {
         u32t  ncount = fcount;
         if (fseek(di->df, HEADER_SPACE + ii*rsize, SEEK_SET)) break;
         if (fread(sptr, 1, SLICE_SECTORS<<di->lshift, di->df) != SLICE_SECTORS<<di->lshift)
            break;
         if (fread(sidx, 1, SLICE_SECTORS<<di->idxshift, di->df) != SLICE_SECTORS<<di->idxshift)
            break;
         // check for empty data in used sectors
         for (idx=0; idx<SLICE_SECTORS; idx++)
            if (di->idxshift==2 && ((u32t*)sidx)[idx]!=FFFF ||
                di->idxshift==3 && ((u64t*)sidx)[idx]!=FFFF64)
               if (memchrnb(sptr+(idx<<di->lshift), 0, 1<<di->lshift)==0 || freeF6 &&
                  memchrnb(sptr+(idx<<di->lshift), 0xF6, 1<<di->lshift)==0)
               { 
                  ncount++; 
                  if (di->idxshift==2) ((u32t*)sidx)[idx] = FFFF; else
                  if (di->idxshift==3) ((u64t*)sidx)[idx] = FFFF64;
               }
         // update index
         if (fcount!=ncount) {
            if (fseek(di->df, -(SLICE_SECTORS<<di->idxshift), SEEK_CUR)) break;
            if (fwrite(sidx, 1, SLICE_SECTORS<<di->idxshift, di->df) != 
               SLICE_SECTORS<<di->idxshift) break;
         }
         fcount = ncount;
      }
      hlp_memfree(sptr);
      free(sidx);
      return fcount;
   }
}

static u32t _std dopt_getgeo(void *data, disk_geo_data *geo) {
   instance_ret(doptdata,dod,EDERR_INVARG);
   if (!geo) return EDERR_INVARG;
   if (!dod->self->df) return EDERR_INVDISK;
   memcpy(geo, &dod->self->info, sizeof(disk_geo_data));
   return 0;
}

static u32t _std dopt_setgeo(void *data, disk_geo_data *geo) {
   diskdata  *di;
   instance_ret(doptdata,dod,EDERR_INVARG);
   if (!geo) return EDERR_INVARG;
   di = dod->self;
   if (!di->df) return EDERR_INVDISK;
   if (di->info.TotalSectors != geo->TotalSectors) return EDERR_INVARG;
   if (di->info.SectorSize != geo->SectorSize) return EDERR_INVARG;

   // check CHS (at least minimal)
   if (!geo->Heads || geo->Heads>255 || !geo->SectOnTrack ||
      geo->SectOnTrack>255 || !geo->Cylinders || (u64t)geo->Cylinders * 
         geo->Heads * geo->SectOnTrack > geo->TotalSectors) return EDERR_INVARG;

   if (fseek(di->df, 8, SEEK_SET)) return EDERR_IOERR;
   // write it first, then replace currently used
   if (fwrite(geo, 1, sizeof(disk_geo_data), di->df) != sizeof(disk_geo_data))
       return EDERR_IOERR;
   memcpy(&di->info, geo, sizeof(disk_geo_data));
   // if disk is mounted - update CHS value in system info
   if (di->qsdisk>=0) {
      struct qs_diskinfo *qdi = hlp_diskstruct(di->qsdisk, 0);
      // this WRONG! we patching it in system struct directly
      if (qdi) {
         qdi->qd_cyls  = geo->Cylinders;
         qdi->qd_heads = geo->Heads;
         qdi->qd_spt   = geo->SectOnTrack;
      }
   }
   return 0;
}

static char* _std dopt_getname(void *data) {
   char *rc;
   instance_ret(doptdata,dod,0);
   if (!dod->self->df) return 0;

   rc = strdup("VHDD disk. File ");
   rc = strcat_dyn(rc, dod->self->dfname);
   return rc;
}

static int _std dopt_setro(void *data, int state) {
   instance_ret(doptdata,dod,EDERR_INVARG);
   // not implemented now
   return 0;
}


static void *qs_emudisk_list[] = { dsk_make, dsk_open, dsk_query, dsk_read,
   dsk_write, dsk_compact, dsk_mount, dsk_disk, dsk_umount, dsk_close };

static void *qs_extdisk_list[] = { dopt_getgeo, dopt_setgeo, dopt_getname,
   dopt_setro };

static void _std dsk_init(void *instance, void *data) {
   diskdata *di = (diskdata*)data;
   di->sign     = VHDD_SIGN;
   di->qsdisk   = -1;
   di->df       = 0;
   di->dfname   = 0;
   di->index    = 0;
   di->slices   = 0;
   di->selfptr  = instance;
   di->cbm      = NEW(bit_map);
   di->einfo    = 0;
}

static void _std dsk_done(void *instance, void *data) {
   instance_void(diskdata,di);

   if (di->qsdisk>=0) dsk_umount(data);
   if (di->einfo) DELETE(di->einfo);
   if (di->df) dsk_close(data);
   if (di->cbm) DELETE(di->cbm);

   memset(di, 0, sizeof(diskdata));
}

static void _std dopt_init(void *instance, void *data) {
   doptdata *dod = (doptdata*)data;
   dod->sign     = VHDD_SIGN;
}

static void _std dopt_done(void *instance, void *data) {
   instance_void(doptdata,dod);
   memset(dod, 0, sizeof(doptdata));
}

int init_rwdisk(void) {
   if (!classid_emu) {
      classid_emu = exi_register("qs_emudisk", qs_emudisk_list, 
         sizeof(qs_emudisk_list)/sizeof(void*), sizeof(diskdata), 
            dsk_init, dsk_done, 0);
      memset(mounted, 0, sizeof(mounted));
   }
   if (!classid_ext)
      classid_ext = exi_register("qs_emudisk_ext", qs_extdisk_list,
         sizeof(qs_extdisk_list)/sizeof(void*), sizeof(doptdata),
            dopt_init, dopt_done, 0);

   return classid_emu && classid_ext?1:0;
}

int done_rwdisk(void) {
   if (classid_emu || classid_ext) {
      u32t ii;
      // call delete, it calls umount & close disk file for every mounted disk
      for (ii=0; ii<=QDSK_DISKMASK; ii++)
         if (mounted[ii]) exi_free(mounted[ii]);

      memset(mounted, 0, sizeof(mounted));
      // trying to unregister classes
      if (exi_unregister(classid_ext)) classid_ext = 0;
         else return 0;
      if (exi_unregister(classid_emu)) classid_emu = 0;
         else return 0;
   }
   return 1;
}