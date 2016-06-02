//
// QSINIT "cache" module
// sector-level cache
//
#include "qsint.h"
#include "qslog.h"
#include "qsutil.h"
#include "stdlib.h"
#include "cache.h"
#include "qcl/sys/qsedinfo.h"

#undef  ADV_DEBUG

#define PRIO_ARRAY_STEP  _256KB   // 2Mb per one 2Tb HDD in addition to cache
#define MAX_CACHE_SIZE   _1GB     // up tp 2Gb (limited by u16t indexes)

#define checkbit(array,pos)  (array[pos>>5] & 1<<(pos&0x1F))

typedef struct {
   u64t       sectors;
   u32t         ssize;     // sector size
   u32t        sin32k;     // sectors in 32k
   int       s32shift;     // bit shift from sector size to 32k
   int         sshift;     // sector size shift
   int        enabled;
   int          valid;     // data is valid
   int       sysalloc;     // arrays allocation method
   u32t          disk;
   u32t         *prio;
   u32t       *bitmap;
} diskinfo;


#define BI_PRIO   (0x0001)      ///< priority on for this block

typedef struct {
   u16t          prev;     // previous in chain (not filled in free list)
   u16t          next;     // next in chain
   u16t         snext;     // next in same_size_chain
   u16t         flags;     //
   u32t           loc;     // location on disk (in 32k blocks)
   diskinfo      *pdi;     // ptr to disk info for this block
} blockinfo;

// export table
void _std cache_ioctl(u8t vol, u32t action);
u32t _std cache_read(u32t disk, u64t pos, u32t ssize, void *buf);
u32t _std cache_write(u32t disk, u64t pos, u32t ssize, void *buf);

cache_extptr exptable = { sizeof(cache_extptr)/sizeof(void*)-1,
                          &cache_ioctl, &cache_read, &cache_write };

static diskinfo *cdi = 0;     // disk info list
static u32t cdi_hdds = 0,     // number of hdds in cdi
            cdi_fdds = 0;     // number of fdds in cdi
static u8t    *cache = 0;     // REALLY BIG ARRAY :)
static u32t   *cidxa = 0;     // index array
static u16t freehead = 0,     // head of free list
           cachehead = 0;     // head of cache
static blockinfo *bi = 0;     // block info list;
static u32t  a_shift = 0,     // shift from 32k to PRIO_ARRAY_STEP
             dn_bits = 0;     // number of bits for disk number in index array

u32t    blocks_total = 0,     // number of 32k blocks in cache
          free_total = 0;     // number of free blocks
u64t      cache_hits = 0,
         cache_total = 0;
int          dblevel = 0;

static u32t alloc_disk(u32t disk, diskinfo *di) {
   disk_geo_data gd;
   qs_extdisk  edsk;
   u32t       bsize;
#if 0
   if ((disk&QDSK_FLOPPY)!=0 && hlp_fddline(disk)<0) {
      if (dblevel>0) log_it(2, "no change line for %X\n", disk);
      return 0;
   }
#endif
   // query cache enabling state in ext. info
   edsk = hlp_diskclass(disk, 0);
   if (edsk)
      if (edsk->state(EDSTATE_QUERY)&EDSTATE_NOCACHE) {
         log_it(2, "alloc_disk: cache disabled for disk %02X\n", disk);
         return 0;
      }
   bsize = hlp_disksize(disk, 0, &gd);
   // bsr will return -1 on 0
   if (bsize) {
      int  shift = bsf64(gd.SectorSize);
      di->sshift = shift;

      if (shift<0 || 1<<shift!=gd.SectorSize) bsize = 0; else
      // allow up to 4k sector only
      if ((shift = 15 - shift) < 3) bsize = 0; else
         di->s32shift = shift;
   }
   // wrong disk or sector size
   if (!bsize) {
      log_it(2, "alloc_disk: failed on disk %02X\n", disk);
      return 0;
   }
#if 0
   // just make sure ;)
   if (gd.TotalSectors>_32KB * (u64t)FFFF) gd.TotalSectors = _32KB * (u64t)FFFF;
#else
   /* filter disk size to 6Tb now (old BIOSes can return garbage in high dword
      of size */
   if (gd.TotalSectors>3 * (u64t)FFFF) {
      log_it(2, "alloc_disk: too large disk %02X: %LX\n", disk, gd.TotalSectors);
      return 0;
   }
#endif
   // fill disk data
   di->sectors  = gd.TotalSectors;
   di->ssize    = gd.SectorSize;
   di->sin32k   = 1<<di->s32shift;
   di->disk     = disk;
   // priority + bitmap bit arrays
   bsize        = ((u64t)gd.TotalSectors*gd.SectorSize+PRIO_ARRAY_STEP-1)/PRIO_ARRAY_STEP>>2;
   if (!bsize) {
      log_it(2, "alloc_disk: too small disk %02X\n", disk);
      return 0;
   } else
   if (bsize<2) bsize = 2;
   if (dblevel>0)
      log_it(2,"bsize for disk %X - %d kb x 2\n", disk, bsize>>1>>10);
   // array is 2 x 1Mb in size for 2Tb HDD
   // prio   - 1Mb / 2Tb
   // bitmap - 1Mb / 2Tb (256k step)
   di->sysalloc = bsize >= 48 * 1024;
   di->prio     = di->sysalloc?(u32t*)hlp_memallocsig(bsize, "ioch", QSMA_RETERR): malloc(bsize);
   if (!di->prio) return 0;
   if (!di->sysalloc) mem_zero(di->prio);
   di->bitmap   = (u32t*)((u8t*)di->prio + bsize/2);
   // all ok
   di->enabled  = 1;
   di->valid    = 1;
   return bsize;
}

/// unload disk info structs on DLL exit
void unload_all(void) {
   mt_swlock();
   if (cache) cc_setsize(0,0);
   if (cdi) {
      int ii;
      for (ii=0; ii<cdi_hdds+cdi_fdds; ii++) {
         if (cdi[ii].valid) {
            diskinfo *di = cdi + ii;
            if (di->prio)
               if (di->sysalloc) hlp_memfree(di->prio);
                  else free(di->prio);
            memset(di, 0, sizeof(diskinfo));
         }
      }
      free(cdi);
      cdi = 0;
   }
   cdi_hdds = 0;
   cdi_fdds = 0;
   mt_swunlock();
}


static void alloc_structs() {
   u32t   cnt, mem_total = 0;
   cdi_hdds = hlp_diskcount(&cdi_fdds);
   cnt      = cdi_hdds + cdi_fdds;
   if (cnt) {
      cdi = (diskinfo*)malloc(sizeof(diskinfo) * cnt);
      mem_zero(cdi);
      for (cnt = 0; cnt<cdi_hdds; cnt++) mem_total+=alloc_disk(cnt, cdi+cnt);
      for (cnt = 0; cnt<cdi_fdds; cnt++) {
         mem_total+=alloc_disk(cnt|QDSK_FLOPPY, cdi+cdi_hdds+cnt);
         // disable floppy caching by default
         cdi[cdi_hdds+cnt].enabled = 0;
      }
      // anyone is present? calc index sizes
      if (cdi_hdds+cdi_fdds) {
         /// loop until pack required bits into dword
         int diskbits = bsr64(cdi_hdds+cdi_fdds)+1;
         while (1) {
            // bits required for disk number and size
            int maxdsbits = 0;
            for (cnt = 0; cnt<cdi_hdds+cdi_fdds; cnt++) {
               int dskshift = bsr64(cdi[cnt].sectors)+1-cdi[cnt].s32shift-a_shift;
               if (dskshift > maxdsbits) maxdsbits = dskshift;
            }
            if (diskbits+maxdsbits<=32) {
               if (dblevel>0) log_it(2, "bits: disk %d, size %d -> %d\n", diskbits,
                  maxdsbits, diskbits+maxdsbits);
               dn_bits = diskbits;
               break;
            } else {
               for (cnt = 0; cnt<cdi_hdds+cdi_fdds; cnt++) {
                  u64t limitsize = (u64t)1<<maxdsbits-1+cdi[cnt].s32shift+a_shift;
                  if (cdi[cnt].sectors>=limitsize) cdi[cnt].sectors = limitsize-1;
               }
               if (dblevel>0) log_it(2, "disk bits limited to %d\n", maxdsbits-1);
            }
         }
         log_it(2, "memory for disk structs: %dkb\n", mem_total>>10);
      }
   }
}

static diskinfo *get_di(u32t disk) {
   if (!cdi) return 0;
   if ((disk&~QDSK_FLOPPY) > QDSK_DISKMASK) return 0; else {
      u32t num = disk & QDSK_DISKMASK;
      if (disk&QDSK_FLOPPY) {
         if (num>=cdi_fdds) return 0;
         return cdi+cdi_hdds+num;
      } else {
         if (num>=cdi_hdds) return 0;
         return cdi+num;
      }
   }
}

void _std cc_setprio(void *data, u32t disk, u64t start, u32t size, int on) {
   diskinfo   *di;

   mt_swlock();
   di = get_di(disk);
   if (di && di->valid) {
      u32t sp, ep;
      if (start < di->sectors) {
         if (start+size >= di->sectors) size = di->sectors - start;
         if (dblevel>1)
            log_it(2,"cc_setprio(%X,0x%LX,0x%X,%d)\n",disk,start,size,on);
         // size in bytes
         start <<= di->sshift;
         size  <<= di->sshift;
         // start/end pos in megabytes
         ep = (start + size + PRIO_ARRAY_STEP-1) / PRIO_ARRAY_STEP;
         sp = start / PRIO_ARRAY_STEP;
         // set bits in array
         setbits(di->prio, sp, ep-sp+1, on?SBIT_ON:0);
         // log it!
         if (dblevel>2) log_it(2,"cc_setprio done\n");
      }
   }
   mt_swunlock();
}

/// enable/disable cache for specified disk
int _std cc_enable(void *data, u32t disk, int enable) {
   diskinfo *di;
   int   result = -1;

   mt_swlock();
   if (!cdi) alloc_structs();
   if (dblevel>0) log_it(2,"cc_enable(%X,%d)\n", disk, enable);

   di = get_di(disk);
   // is it valid?
   if (di && di->valid) {
      result = di->enabled;
      if (enable>=0) {
         di->enabled = enable?1:0;
         // if cache is active - drop all cached data for this disk
         if (cache && !enable) cc_invalidate(data, disk, 0, FFFF64);
      }
   }
   mt_swunlock();
   return result;
}

/// set cache size in mbs
void _std cc_setsize(void *data, u32t size_mb) {
   mt_swlock();
   if (!size_mb) {
      hlp_runcache(0);
      if (cache) { hlp_memfree(cache); cache = 0; }
      if (cidxa) { free(cidxa); cidxa = 0; }
      if (bi) { free(bi); bi = 0; }
      blocks_total = 0;
      free_total   = 0;
      freehead     = 0;
      cachehead    = 0;
      cache_hits   = 0;
      cache_total  = 0;
   } else {
      u32t availmax;
      // turn off old cache
      if (blocks_total || cache) cc_setsize(data, 0);
      // check define and calc array`s shift
      a_shift = bsf64(PRIO_ARRAY_STEP);
      if (a_shift<15 || 1<<a_shift!=PRIO_ARRAY_STEP) {
         mt_swunlock();
         return;
      }
      a_shift  -= 15;
      // and process again
      hlp_memavail(&availmax,0);
      // bytes -> 64k blocks
      availmax>>= 16;
      // mbytes -> 64k blocks
      size_mb <<= 4;
      // use maximum 1/2 of max. available block
      if (size_mb > availmax/2) size_mb = availmax/2;
      // limit by maximum cache size
      if (size_mb > MAX_CACHE_SIZE>>16) size_mb = MAX_CACHE_SIZE>>16;

      if (!cdi) alloc_structs();
      cache = (u8t*)hlp_memallocsig(size_mb<<16, "ioch", QSMA_RETERR|QSMA_NOCLEAR);
      // memory avail?
      if (cache) {
         u32t  ii;
         // total blocks
         blocks_total = size_mb * 2;
         // block info
         bi = (blockinfo*)malloc(sizeof(blockinfo) * blocks_total);
         mem_zero(bi);
         // block index
         cidxa = (u32t*)malloc(sizeof(u32t) * blocks_total);
         memsetd(cidxa, FFFF, blocks_total);
         // fill free block list
         // block 0 is reserved and never be used to save index for "invalid" value
         freehead = 1;
         for (ii=1; ii<blocks_total; ii++) bi[ii].next  = ii+1;
         free_total = blocks_total-1;
         // end of list
         bi[blocks_total-1].next = 0;
         // no cached
         cachehead  = 0;
         // everything ready - catch read/write
         hlp_runcache(&exptable);
         // and set i/o pririty for FAT areas of mounted volumes
         for (ii=0; ii<DISK_COUNT; ii++)
            if (ii!=DISK_LDR)
               if (hlp_volinfo(ii,0)!=FST_NOTMOUNTED) cache_ioctl(ii,CC_MOUNT);
      } else
         log_it(2, "no memory?\n");
   }
   mt_swunlock();
}

/// no any checks here!!
static void free_bi_entry(u32t index, int ignore_snext) {
   blockinfo *pbi = bi + index;
   if (pbi->pdi) {
      // clear index entry
      cidxa[index] = FFFF;
      // relink neighbors
      bi[pbi->prev].next = pbi->next;
      bi[pbi->next].prev = pbi->prev;
      // update cachehead
      if (index==cachehead) {
         cachehead = pbi->next;
         if (index==cachehead) cachehead = 0;
      }
      // remove from same-size list
      if (!ignore_snext) {
         if (pbi->snext) {
            u16t pb = index;
            while (bi[pb].snext!=index) pb = bi[pb].snext;
            // is block became the last one with the same size?
            bi[pb].snext = bi[index].snext==pb ? 0 : bi[index].snext;
         } else {
            // clear "cached" bit in bitmap
            setbits(pbi->pdi->bitmap, pbi->loc>>a_shift, 1, 0);
         }
      }
      // insert to free list
      pbi->next = freehead;
      freehead  = index;
      free_total++;
      // drop diskinfo (used as additional "unused entry" mark)
      pbi->pdi  = 0;
   }
}

/** expand free list to at least this number of entries.
    return 0 on invalid count only ( > 1/2 or total blocks ) */
static int expand_free(u32t count) {
   if (count > blocks_total>>1) return 0;
   // need to free some blocks
   if (count > free_total) {
      if (!cachehead) return 0;
      while (free_total < count) {
         blockinfo *pbi = bi + cachehead;
         /* drop priority from prioritized block and move it to the end of
            cache queue, else move block to free list */
         if (pbi->flags&BI_PRIO) {
            pbi->flags&=~BI_PRIO;
            cachehead = pbi->next;
         } else
            free_bi_entry(cachehead,0);
      }
   }
   return 1;
}

/** common cache update.
    @param di      dist info
    @param pos     start sector
    @param ssize   number of sectors
    @param buf     buffer with/for data
    @param write   0 - read, 1 - write, 2 - direct write
    @return number of readed sectors on read op, void on write */
static u32t cache_io(diskinfo *di, u64t pos, u32t ssize, u8t *buf, int write) {
   // sectors left in block
   u32t sbleft = ((u32t)pos+di->sin32k-1 & ~((1<<di->s32shift)-1)) - (u32t)pos,
       diskidx = di-cdi<<32-dn_bits,
            rc = 0,
        pos32k = pos>>di->s32shift,
          cpos = pos&(1<<di->s32shift)-1; // offset in 1st 32k-s

   if (dblevel>1) log_it(2,"cache_io(%X,%LX,%d,%08X,%d)\n",di->disk,pos,ssize,buf,write);

   if (!write) cache_total+= ssize;
   while (ssize) {
      u32t   posbm = pos32k>>a_shift,
             block = 0,  // required block
           slblock = 0;  // first founded from snext list
      // sectors to process in this iter
      if (!sbleft) sbleft = di->sin32k;
      if (sbleft>ssize) sbleft = ssize;

      //log_it(2,"posbm %d, %16b\n", posbm, di->bitmap);
      if (dblevel>2) log_it(2,"pos32k %08X, sbleft %d, ssize %d\n", pos32k, sbleft, ssize);

      // block present in cache?
      if (checkbit(di->bitmap,posbm)) {
         u32t *pos = 0;
         /* search back from current cache head (most of blocks is non-prioritized, so
            they are still ordered in cache list and cachehead give us a hint there to
            start) */
         pos = memrchrd(cidxa, diskidx|posbm, cachehead);
         if (!pos) pos = memrchrd(cidxa+cachehead, diskidx|posbm, blocks_total-cachehead);

         if (pos) slblock = pos-cidxa;
         if (slblock) {
            block = slblock;
            if (bi[block].snext)  // >1 in list
               while (bi[block].loc!=pos32k && bi[block].snext!=slblock)
                  block = bi[block].snext;
            // not found
            if (bi[block].loc!=pos32k) block = 0;
         }
         if (dblevel>2) log_it(2,"slblock %d block %d\n",slblock,block);
      }
      /* Process cache only if block was found or this a read op or full
         32k write in "cached write" mode.
         In "direct write" mode there is no new cache entries allocation, but
         updating of presented entries still occur (else whose entries became
         outdated). */
      if (block || !write || sbleft==di->sin32k && write<2) {
         blockinfo *pbi = 0;
         if (!block) {  // alloc cache block
            expand_free(1);
            if (free_total) {
               block = freehead;
               // read whole 32k to cache
               if (!write) {
                  u64t rpos = (u64t)pos32k<<di->s32shift;
                  u32t rcnt = di->sin32k;
                  // is end of disk?
                  if (rpos + rcnt > di->sectors) rcnt = di->sectors - rpos;
                  // read it!
                  if (hlp_diskread(di->disk|QDSK_DIRECT, rpos, rcnt,
                     cache+(block<<15))!=rcnt) break;
               }
               // readed - add entry to cache
               pbi      = bi + block;
               freehead = pbi->next;
               free_total--;
               if (!cachehead) {
                  pbi->next = block;
                  pbi->prev = block;
                  cachehead = block;
               } else {
                  pbi->next = cachehead;
                  pbi->prev = bi[cachehead].prev;
                  bi[pbi->prev].next = block;
                  bi[cachehead].prev = block;
               }
               pbi->loc     = pos32k;
               pbi->pdi     = di;
               pbi->flags   = 0;
               cidxa[block] = diskidx|posbm;

               if (dblevel>2) log_it(2,"add to cache: block %d, pos %08X, slblock %d\n",
                  block,pos32k,slblock);
               // update snext list
               if (!slblock) {
                  setbits(di->bitmap, posbm, 1, SBIT_ON);
                  pbi->snext = 0;
               } else {
                  pbi->snext = slblock;
                  if (!bi[slblock].snext) bi[slblock].snext = block; else {
                     u16t pb = slblock;
                     while (bi[pb].snext!=slblock) pb = bi[pb].snext;
                     bi[pb].snext = block;
                  }
#ifdef ADV_DEBUG
                  // print snext list
                  if (dblevel>3) {
                     char buf[128];
                     int  pb = slblock,
                         pos = sprintf(buf, "snext %d list:", slblock);
                     do {
                        pos+=sprintf(buf+pos, " %d", pb);
                        pb = bi[pb].snext;
                     } while (pb!=slblock);
                     strcpy(buf+pos,"\n");
                     log_it(2,buf);
                  }
#endif
               }
            }
         } else {       // block already present
            pbi = bi + block;
            // move block to the end of remove queue
            if (bi[cachehead].prev!=block) {
               if (block==cachehead) {
                  cachehead = pbi->next;
               } else {
                  bi[pbi->next].prev = pbi->prev;
                  bi[pbi->prev].next = pbi->next;
                  pbi->next = cachehead;
                  pbi->prev = bi[cachehead].prev;
                  bi[pbi->prev].next = block;
                  bi[cachehead].prev = block;
               }
            }
            if (!write) cache_hits+=sbleft;
         }
#ifdef ADV_DEBUG
         if (dblevel>3 && pbi) { // print list around of cachehead
            if (!cachehead) log_it(2,"cachehead==0!"); else {
               char buf[128];
               int  pb = bi[bi[cachehead].prev].prev, ii,
                   pos = sprintf(buf, "cachehead (%d):",cachehead);
               for (ii=0; ii<5; ii++) {
                  pos+=sprintf(buf+pos, " %s%d%s%s", ii==2?">":"", pb,
                     checkbit(bi[pb].pdi->prio,bi[pb].loc>>a_shift)?"+":"",
                        ii==2?"<":"");
                  pb = bi[pb].next;
               }
               strcpy(buf+pos,"\n");
               log_it(2,buf);
            }
         }
#endif
         if (pbi) {
            u8t *ptr = cache+(block<<15)+(cpos<<di->sshift);
            // set/update priority flag
            if (checkbit(di->prio,posbm)) pbi->flags|=BI_PRIO;
            //log_it(2,"block %d cpos %d ptr %X buf %X sbleft %d\n",block,cpos,ptr,buf,sbleft);
            //log_it(2,"ptr: %16b\n",ptr);
            // copy data
            if (write) memcpy(ptr, buf, sbleft<<di->sshift);
               else memcpy(buf, ptr, sbleft<<di->sshift);
         } else
         if (!write) break;
      }
      buf   += sbleft<<di->sshift;
      ssize -= sbleft;
      rc    += sbleft;
      sbleft = 0;
      cpos   = 0;
      pos32k++;
   }
   return rc;
}

// caller provides MT locking in own code
u32t cache_read(u32t disk, u64t pos, u32t ssize, void *buf) {
   diskinfo *di = cache?get_di(disk):0;
   // wrong disk, pos, size
   if (!di || !di->enabled || !ssize || pos>=di->sectors ||
      pos+ssize>di->sectors) return 0;
   // too long op for our`s cache (use more than half of size)
   if (ssize>>di->s32shift > blocks_total>>1) return 0;
      else return cache_io(di, pos, ssize, (u8t*)buf, 0);
}

// caller provides MT locking in own code
u32t cache_write(u32t disk, u64t pos, u32t ssize, void *buf) {
   int   direct = disk&QDSK_DIRECT?1:0;
   diskinfo *di = cache?get_di(disk&~QDSK_DIRECT):0;
   // wrong disk, pos, size
   if (!di || !di->enabled || !ssize || pos>=di->sectors ||
      pos+ssize>di->sectors) return 0;
   // too long op for our`s cache (use more than half of size)
   if (ssize>>di->s32shift > blocks_total>>1) {
      cc_invalidate(0, disk, pos, ssize);
      return 0; 
   } else {
      // write first
      u32t rc = hlp_diskwrite(disk|QDSK_DIRECT, pos, ssize, buf);
      // update written part
      if (rc) cache_io(di, pos, rc, (u8t*)buf, 1+direct);
      // return to common hlp_diskwrite()
      return rc;
   }
}

// called WITHOUT MT locking, but all ops here are safe
void cache_ioctl(u8t vol, u32t action) {
   if (dblevel>1 && action!=CC_IDLE)
      log_it(2,"cache_ioctl(%d,%d)\n",vol,action);
   switch (action) {
      case CC_MOUNT    : // Mount/Unmount volume
      case CC_UMOUNT   : {
         u64t start;
         u32t  disk, length;
         if (get_sys_area(vol, &disk, &start, &length))
            cc_setprio(0,disk,start,length,action==CC_MOUNT);

         if (action==CC_UMOUNT) cc_invalidate_vol(0,vol);
         return;
      }
      case CC_SHUTDOWN : // Disk i/o shutdown
         unload_all();
         return;
      case CC_RESET    : // FatFs disk_initialize()
         return;
      case CC_SYNC     : // FatFs CTRL_SYNC command
      case CC_IDLE     : // Idle action (keyboard read)
      case CC_FLUSH    : // Flush all
         return;
      default:
         log_it(2,"invalid cache_ioctl(%d,%d)\n",vol,action);
         return;
   }
}

void _std cc_invalidate_vol(void *data, u8t drive) {
   disk_volume_data info;
   hlp_volinfo(drive,&info);
   if (info.TotalSectors)
      cc_invalidate(0, info.Disk, info.StartSector, info.TotalSectors);
}

// warning! it called with data=0 from cache_write() above
void _std cc_invalidate(void *data, u32t disk, u64t start, u64t size) {
   diskinfo  *di;

   mt_swlock();
   di = get_di(disk);
   if (di && cache && di->valid) {
      // check arguments
      if (start>di->sectors) { mt_swunlock(); return; }
      if (start+size>di->sectors) size = di->sectors - start;
      if (dblevel>1)
         log_it(2,"cc_invalidate(%X,%LX,%LX)\n",disk,start,size);
      // full or partial clear?
      if (start==0 && size==di->sectors) {
         u32t ii = blocks_total;
         // free all entries for this disk
         while (ii--)
            if (bi[ii].pdi)
               if (bi[ii].pdi->disk==disk) free_bi_entry(ii,1);
         // zero both prio & bitmap arrays
         if (!di->sysalloc) mem_zero(di->prio); else
            memset(di->prio, 0, hlp_memgetsize(di->prio));
      } else  {
         u32t min_loc = start>>di->s32shift,
              max_loc = start+size-1>>di->s32shift,
                   ii = blocks_total;
         // free entries by location
         while (ii--) {
            blockinfo *pbi = bi+ii;
            if (pbi->pdi)
               if (pbi->pdi->disk==disk && pbi->loc>=min_loc && pbi->loc<=max_loc)
                  free_bi_entry(ii,0);
         }
      }
   }
   mt_swunlock();
}

u32t _std cc_stat(void *data, u32t disk, u32t *pblocks) {
   diskinfo   *di;
   u32t     count = 0;

   mt_swlock();
   di = get_di(disk);
   if (pblocks) *pblocks = 0;
   if (di && cache && di->valid) {
      u32t ii = blocks_total, pcount = 0;
      // free all entries for this disk
      while (ii--)
         if (bi[ii].pdi)
            if (bi[ii].pdi->disk==disk) {
               count++;
               if (pblocks && (bi[ii].flags&BI_PRIO)) pcount++;
            }
      if (pblocks) *pblocks = pcount;
   }
   mt_swunlock();
   return count;
}
