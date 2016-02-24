//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
// ------------------------------------------------------------------
// code is not optimized at all, but it was written only for PARTMGR
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
#define MAX_CBMHINT            _2MB   ///< size limit for single disk

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
   u32t       freehint;    // firts index to start free search
   int         tracebm;
   disk_geo_data  info;
   u32t        bmshift;
   bit_map         cbm;    // changed index slices bitmap
   bit_map      bmused;    // sector presence hint bitmap
   bit_map      bmfree;    // free sectors hint bitmap
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

/// 32-bit offset in file
static inline u32t index_to_offset(diskdata *di, u32t idxpos) {
   return HEADER_SPACE + ((SLICE_SECTORS<<di->lshift) +
      (SLICE_SECTORS<<di->idxshift)) * (idxpos/SLICE_SECTORS) +
         (idxpos%SLICE_SECTORS<<di->lshift);
}

/** search for sector pos
    @param hintidx   Position of previous sector (or 0) - just for speed up
    @return position of sector or FFFF if no one */
static u32t ed_sectorindex(diskdata *di, u64t sector, u32t hintidx) {
   if (!di->df||!di->index||!di->slices) return 0;
   // at least one sector in area is used?
   if (!di->bmused->check(1, sector>>di->bmshift, 1)) return FFFF; else {
      u32t maxnow = di->slices*SLICE_SECTORS;
      // this also drop invalid FFFF value ;)
      if (++hintidx>=maxnow) hintidx = 0;
      /* searching from hint to the end of array first, then from start to hint.
         This can speed up a bit on common tasks */
      if (di->idxshift==3) {
         if (((u64t*)di->index)[hintidx]==sector) return hintidx; else {
            u64t *ofs = memchrq((u64t*)di->index+hintidx, sector, maxnow-hintidx);
            if (!ofs) {
               if (hintidx) ofs = memchrq((u64t*)di->index, sector, hintidx);
               if (!ofs) return FFFF;
            }
            return ofs - (u64t*)di->index;
         }
      } else {
         if (((u32t*)di->index)[hintidx]==(u32t)sector) return hintidx; else {
            u32t *ofs = memchrd((u32t*)di->index+hintidx, sector, maxnow-hintidx);
            if (!ofs) {
               if (hintidx) ofs = memchrd((u32t*)di->index, sector, hintidx);
               if (!ofs) return FFFF;
            }
            return ofs - (u32t*)di->index;
         }
      }
   }
}

static int flush_slices(diskdata *di) {
   /* function can`t use hint bitmaps, they can be outdated at this moment
      (and no reason to use it here) */
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

// build hint bitmaps
static void build_hints(diskdata *di) {
   u32t ii, total = di->slices*SLICE_SECTORS, maxsize, alloc;
   union {
      u64t *p64;
      u32t *p32;
   } fptr;
   fptr.p32 = (u32t*)di->index;
   alloc    = MAX_CBMHINT;

   // if we have <200Mb of free mem - then shrink our`s bitmap
   hlp_memavail(&maxsize,0);
   if (maxsize/100 < alloc) {
      alloc = maxsize/100;
      if (alloc<_128KB) alloc = _128KB;
   }
   ii=0; alloc<<=3;
   while (di->info.TotalSectors>>ii > (u64t)alloc) ii++;
   di->bmshift = ii;
   alloc       = di->info.TotalSectors>>ii;
   di->bmused->alloc(alloc+1);
#if 0
   log_printf("hints for %LX sectors: %u bytes, shift %u\n",
      di->info.TotalSectors, alloc+1>>3, di->bmshift);
#endif
   di->bmfree->alloc(di->slices*SLICE_SECTORS);
   for (ii=0; ii<total; ii++)
      if (di->idxshift==3) {
         u64t sector = *fptr.p64++;
         if (sector==FFFF64) di->bmfree->set(1, ii, 1); else
            di->bmused->set(1, sector>>di->bmshift, 1);
      } else {
         u32t sector = *fptr.p32++;
         if (sector==FFFF) di->bmfree->set(1, ii, 1); else
            di->bmused->set(1, sector>>di->bmshift, 1);
      }
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
         di->dfname   = _fullpath(0, fname, 0);
         di->freehint = 0;
         // build hints for feature use
         build_hints(di);
         // commit 1st slice index
         return flush_slices(di)?0:EIO;
      }
   }
}

static int _std dsk_close(void *data);

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
         di->freehint = 0;
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
         // build hints for feature use
         build_hints(di);
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
   if (used)    *used    = di->index ? di->bmfree->total(0) : 0;
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
      u32t ii = 0, idx = 0;
      while (ii<count) {
         u32t rdc = 1;
         idx = ed_sectorindex(di, ii+sector, idx);
         // sector available?
         if (idx==FFFF) memset((char*)usrdata+(ii<<di->lshift), 0, 1<<di->lshift); else {
            // check for whole block of sectors
            while ((idx+rdc)%SLICE_SECTORS && ii+rdc<count &&
               (di->idxshift==3 && ((u64t*)di->index)[idx+rdc]==sector+ii+rdc ||
                di->idxshift==2 && ((u32t*)di->index)[idx+rdc]==sector+ii+rdc)) rdc++;

            if (fseek(di->df, index_to_offset(di,idx), SEEK_SET)) return ii;
            if (fread((char*)usrdata + (ii<<di->lshift), 1, rdc<<di->lshift,
               di->df) != rdc<<di->lshift)
                  return ii;
         }
         ii+=rdc;
      }
      return count;
   }
   return 0;
}

static void remove_chain(diskdata *di, u64t sector, u32t count) {
   u32t idxpos = 0;
   if (!di->df) return;
#if 0
   log_printf("remove_chain (%X, %LX, %u)\n", di->qsdisk, sector, count);
#endif
   while (count) {
      idxpos = ed_sectorindex(di, sector, idxpos);
      // mark as free
      if (idxpos!=FFFF) {
         u32t  chainlen = 0, slice;
         if (di->idxshift==3) {
            u64t *pidx = (u64t*)di->index + idxpos;
            // free entire chain
            do {
              *pidx++ = FFFF64;
              sector++; chainlen++;
            } while (--count && *pidx==sector);
         } else {
            u32t *pidx = (u32t*)di->index + idxpos;
            // free entire chain
            do {
              *pidx++ = FFFF;
              sector++; chainlen++;
            } while (--count && *pidx==sector);
         }
         // update hints
         di->bmfree->set(1, idxpos, chainlen);
         if (di->freehint>idxpos) di->freehint = idxpos;
         // flag for update
         slice = idxpos/SLICE_SECTORS;
         di->cbm->set(1, slice, 1);
         if (chainlen>1) {
            u32t lslice = (idxpos+chainlen-1)/SLICE_SECTORS;
            if (slice<lslice) di->cbm->set(1, slice+1, lslice-slice);
         }
      } else {
         sector++; count--;
      }
   }
}

/** write sectors.
    @return number of actually written sectors */
static u32t write_chain(diskdata *di, u64t sector, u32t count, char *data) {
   u32t maxnow = di->slices*SLICE_SECTORS,
          hint = di->freehint,
       stcount = count;
#if 0
   log_printf("write_chain (%X, %LX, %u, %08X)\n", di->qsdisk, sector, count, data);
#endif
   if (!di->df||!di->index||!di->slices) return 0; else
   while (count) {
      u8t     *fp = 0;
      u32t   clen = count;
      // search free pos
      if (hint < maxnow) {
         hint = di->bmfree->findset(1, clen, hint);
         // no single block? split it
         if (hint==FFFF) {
            clen = di->bmfree->longest(1, &hint);
            if (clen) di->bmfree->set(0, hint, clen);
         }
         if (clen) fp = (u8t*)di->index + (hint<<di->idxshift);
         // update freehint after smallest search
         if (clen==1) di->freehint = hint+1;
      }

      if (!fp) { // alloc new slice
         di->index = realloc(di->index, (SLICE_SECTORS<<di->idxshift)*(di->slices+1));
         hint      = SLICE_SECTORS*di->slices++;
         maxnow    = SLICE_SECTORS*di->slices;
         fp        = (u8t*)di->index + (hint<<di->idxshift);
         memset(fp, 0xFF, SLICE_SECTORS<<di->idxshift);

         di->cbm->resize(di->slices,1);
         di->bmfree->resize(di->slices*SLICE_SECTORS,1);
         // is it fits here?
         clen = count>SLICE_SECTORS ? SLICE_SECTORS : count;
         di->bmfree->set(0, hint, clen);
         // update freehint in any way (no empties before it)
         di->freehint = hint+clen;
         // flush only on creation of new slice!
         if (!flush_slices(di)) break;
      }
      { // update "slice touched" flag && write data
         u32t  slice = hint/SLICE_SECTORS,
              lslice = (hint+clen-1)/SLICE_SECTORS,
               total = clen;

         di->cbm->set(1, slice, lslice-slice+1);
         // write data - slice at one piece
         while (slice++<=lslice) {
            u32t scnt = SLICE_SECTORS - hint%SLICE_SECTORS, wrsize;
            if (scnt>total) scnt = total;
            wrsize = scnt<<di->lshift;

            if (fseek(di->df, index_to_offset(di,hint), SEEK_SET)) break;
            if (fwrite(data, 1, wrsize, di->df) != wrsize) break;

            hint+=scnt; data+=wrsize; total-=scnt;
         }
         // error in cycle?
         if (total) break;
         // update "area used" hint bits
         slice = sector>>di->bmshift;
         di->bmused->set(1, slice, 1);
         if (clen>1) {
            lslice = sector+clen-1 >> di->bmshift;
            if (slice<lslice) di->bmused->set(1, slice+1, lslice-slice);
         }
      }
      // update sector index
      count -= clen;

      while (clen--) {
         if (di->idxshift==3) *(u64t*)fp = sector; else *(u32t*)fp = sector;
         fp += 1<<di->idxshift;
         sector++;
      }
   }
   return stcount - count;
}

/** write "sectors".
    Function optimized to be usable on cloning 2-3Gb partition to VHDD file.
    However, dsk_read() is not optimized at all, because we have physicial disk
    read cache :) */
static u32t _std dsk_write(void *data, u64t sector, u32t count, void *usrdata) {
   u32t   ii, rc, ecount = 0, necount = 0;
   u64t   start  = sector;
   char  *ep, *sp;
   instance_ret(diskdata,di,0);
   // no file?
   if (!di->df) return 0;
   // check limits
   if (sector>=di->info.TotalSectors) return 0;
   if (sector+count>di->info.TotalSectors) count = di->info.TotalSectors - sector;
   rc = count;

   /* just removes all used sectors and then allocates it again.
      This can be even faster than searching one by one and will remove
      possible fragmentation too.
      Max slowdown produced by sector-by-sector writes of huge data, so try
      to allocate/write a long chains instead. */
   remove_chain(di, sector, count);
   /* scanning input data for zeroed sectors and add/remove actual storage
      space:  1. find 1st empty range
              2. find 2nd empty range
              3. write used sectors between
              4. goto 2 */
   sp = (char*)usrdata;
   while (count+necount+ecount) {
      u32t zcnt = 0;
      // still have input data?
      if (count) {
         ep    = memchrnb(sp, 0, count<<di->lshift);
         zcnt  = ep ? ep-sp>>di->lshift : count;
         count-= zcnt;
         sp   += zcnt+1<<di->lshift;
      }
//log_printf("zcnt=%u %u %u %Lu count=%u\n", zcnt, necount, ecount, sector, count);
      if (zcnt || !count) {
         if (ecount || necount) {
            // 1st empty range
            if (ecount) sector += ecount;
            // write data sectors
            if (necount) {
               u32t wrc = write_chain(di, sector, necount, (char*)usrdata+(sector-start<<di->lshift));
               // file write error?
               if (wrc<necount) return sector-start+wrc;
               sector += necount;
               necount = 0;
            }
         }
         // # of empty
         ecount  = zcnt;
      }
      // next one has data within
      if (count) { necount++; count--; }
   }
   // write_chain() requires index update at the end
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

static u32t _std dsk_compact_int(void *data, int freeF6, int refresh) {
   u8t  *sptr;
   instance_ret(diskdata,di,0);
   if (!di->df||!di->index||!di->slices) return 0;
   // flush it first!
   flush_slices(di);

   sptr = hlp_memalloc(SLICE_SECTORS<<di->lshift, QSMA_RETERR|QSMA_NOCLEAR);

   if (!sptr) return 0; else {
      u32t   ii, fcount = 0, idx,
          rsize = (SLICE_SECTORS<<di->lshift) + (SLICE_SECTORS<<di->idxshift);
      union {
         u64t *p8;
         u32t *p4;
         u8t  *p1;
      } sidx;

      for (ii=0; ii<di->slices; ii++) {
         u32t  ncount = fcount;
         // slice pos in index
         sidx.p1 = (u8t*)di->index + (SLICE_SECTORS*ii<<di->idxshift);

         if (fseek(di->df, HEADER_SPACE + ii*rsize, SEEK_SET)) break;
         if (fread(sptr, 1, SLICE_SECTORS<<di->lshift, di->df) != SLICE_SECTORS<<di->lshift)
            break;
         // check for empty data in used sectors
         for (idx=0; idx<SLICE_SECTORS; idx++)
            if (di->idxshift==2 && sidx.p4[idx]!=FFFF || di->idxshift==3 && sidx.p8[idx]!=FFFF64)
               if (memchrnb(sptr+(idx<<di->lshift), 0, 1<<di->lshift)==0 || freeF6 &&
                  memchrnb(sptr+(idx<<di->lshift), 0xF6, 1<<di->lshift)==0)
               {
                  ncount++;
                  if (di->idxshift==2) sidx.p4[idx] = FFFF; else
                  if (di->idxshift==3) sidx.p8[idx] = FFFF64;
               }
         // flag index changes
         if (fcount!=ncount) di->cbm->set(1, ii, 1);
         fcount = ncount;
      }
      hlp_memfree(sptr);
      // truncate empty slices at the end of file
      for (ii=di->slices; di->slices>1; ) {
         // must be a whole slice index of pure 0xFFs (both 64-bit or 32-bit)
         u8t *ofs = memchrnb((u8t*)di->index+((di->slices-1)*SLICE_SECTORS<<di->idxshift),
            0xFF, SLICE_SECTORS<<di->idxshift);
         if (ofs) break; else di->cbm->resize(--di->slices,0);
      }
      // update indexes on disk
      flush_slices(di);
      // update hints
      di->freehint = 0;
      if (refresh) build_hints(di);

      return fcount;
   }
}

static u32t _std dsk_compact(void *data, int freeF6) {
   return dsk_compact_int(data, freeF6, 1);
}

/// close image file
static int _std dsk_close(void *data) {
   instance_ret(diskdata,di,EINVAL);

   if (!di->df) return ENODEV;
   // flush something forgotten
   flush_slices(di);
   // free bitmap memory
   di->cbm->alloc(0); di->bmused->alloc(0); di->bmfree->alloc(0);
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

/** enable/disable tracing of this disk instance calls.
    By default tracing of qs_emudisk (self) is enabled, but tracing of
    internal bitmaps is disabled */
static void _std dsk_trace(void *data, int self, int bitmaps) {
   instance_void(diskdata,di);
   // self - query it, because user able to change it by exi_chainset() too ;)
   if (self>=0) {
      int cstate = exi_chainset(di->selfptr,-1);
      if (cstate>=0)
         if (Xor(self,cstate)) exi_chainset(di->selfptr, self);
   }
   // bitmaps
   if (bitmaps>=0)
      if (Xor(bitmaps,di->tracebm)) {
         di->tracebm = bitmaps;
         exi_chainset(di->cbm, bitmaps);
         exi_chainset(di->bmused, bitmaps);
         exi_chainset(di->bmfree, bitmaps);
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
      // this is WRONG! we patching it in system struct directly
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

static u32t _std dopt_state(void *data, u32t state) {
   instance_ret(doptdata,dod,EDERR_INVARG);
   // set r/o is not supported, cache denied for this disk
   return EDSTATE_NOCACHE;
}

static void *qs_emudisk_list[] = { dsk_make, dsk_open, dsk_query, dsk_read,
   dsk_write, dsk_compact, dsk_mount, dsk_disk, dsk_umount, dsk_close,
   dsk_trace };

static void *qs_extdisk_list[] = { dopt_getgeo, dopt_setgeo, dopt_getname,
   dopt_state };

static void _std dsk_init(void *instance, void *data) {
   diskdata  *di = (diskdata*)data;
   di->sign      = VHDD_SIGN;
   di->qsdisk    = -1;
   di->df        = 0;
   di->dfname    = 0;
   di->index     = 0;
   di->slices    = 0;
   di->tracebm   = 0;
   di->selfptr   = instance;
   di->cbm       = NEW(bit_map); exi_chainset(di->cbm, 0);
   di->bmused    = NEW(bit_map); exi_chainset(di->bmused, 0);
   di->bmfree    = NEW(bit_map); exi_chainset(di->bmfree, 0);
   di->einfo     = 0;
}

static void _std dsk_done(void *instance, void *data) {
   instance_void(diskdata,di);

   if (di->qsdisk>=0) dsk_umount(data);
   if (di->einfo) DELETE(di->einfo);
   if (di->df) dsk_close(data);
   if (di->cbm) DELETE(di->cbm);
   if (di->bmused) DELETE(di->bmused);
   if (di->bmfree) DELETE(di->bmfree);

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

