//
// QSINIT "start" module
// primitive "file cache" handler (not finished)
// -------------------------------------
// limitations are:
//  - cluster number is 32-bit
//  - anything above 4Gb in file is not cached at all and going though to
//    the sector-level cache
//
#include "qstypes.h"
#include "qserr.h"
#include "qsint.h"
#include "qsxcpt.h"
#include "qsinit.h"
#include "stdlib.h"
#include "qcl/sys/qsfilecc.h"
#include "classes.hpp"

#define FCC_OK                         1   ///< compile this module
#define FCC_DEBUG                      0   ///< debug level
#define FDCC_SIGN             0x43434446   ///< FDCC string
#define FDCE_SIGN             0x45434446   ///< FDCE string
#define CACHE_SHIFT                   16   ///
#define CACHE_STEP       (1<<CACHE_SHIFT)  ///< 64k - cache step
#define BIT_BUFFER  (1<<32-CACHE_SHIFT-3)  ///< 8k buffer for 64k step

static const char *memsig = "sfch";  ///< mark for debug memory list

struct _filedata;

typedef struct _fcentry {
   u32t              sign;
   u32t               pos;   ///< cache entry position (index!)
   clock_t          ltime;   ///< last use time
   struct _fcentry  *prev,
                    *next;   ///< global list
   u8t               *mem;
   struct _filedata   *fd;   ///< owner of this cache block
   u32t         nwsectors;   ///< # of non-written sectors
   u32t            nwmask[(1<<CACHE_SHIFT-9)/4];  // 128 bits for 512 bytes sector
} fcentry;

struct _filedata {
   u32t              sign;
   void          *userarg;
   cc_cladd            af;
   cc_clfree           df;
   cc_chattr           cf;
   u32t              attr;
   u32t              disk;
   u32t             clnum;   ///< number of clusters on disk
   u64t              size;   ///< current file size
   u64t            nzsize;   ///< data is valid until this pos
   u64t           dataofs;   ///< offset of data area on disk (zero cluster)
   io_time          ctime;
   io_time          atime;
   io_time          wtime;
   u8t            *sector;   ///< sector buffer, not allocated by default
   TUQList          *list;
   TPtrList          *fcl;   ///< cache block list
   u8t           secshift;   ///< disk sector size shift
   u8t            clshift;
   u8t                emb;   ///< cache uses bf buffer itself (for sz <8k)
   u8t              valid;   ///< file is opened
   u32t              opts;
   u32t         nwpresent;   ///< # of non-written sectors (1 when emb=1)
   u32t  bf[BIT_BUFFER/4];
};
typedef struct _filedata filedata;

u32t       cc_max_blocks = 0;   ///< max # of cache blocks
static u32t  used_blocks = 0;   ///< # of used cache blocks
static fcentry   *celist = 0,   ///< global cache memory list
                  *ceend = 0;   ///< end of list

#define instance_ret(inst,err)        \
   filedata *inst = (filedata*)data;  \
   if (inst->sign!=FDCC_SIGN) return err;

#define instance_void(inst)           \
   filedata *inst = (filedata*)data;  \
   if (inst->sign!=FDCC_SIGN) return;

#define checkbit(array,pos)  _bittest(array[(pos)>>5], (pos)&0x1F)
#define setbit(array,pos)    _bitset(array+((pos)>>5), (pos)&0x1F)
#define resetbit(array,pos)  _bitreset(array+((pos)>>5), (pos)&0x1F)

/** append cc_loc to list.
    @param cl           list
    @param list         source data
    @param climit       # of clusters on the device
    @param [out] total  number of saved clusters
    @return error code or 0 */
static qserr loc2list(TUQList *cl, cc_loc *list, u32t climit, u32t *total) {
   if (total) *total = 0;
   while (list->len) {
      if (list->cluster>=climit) return E_DSK_FILEALLOC;
      cl->Add((u64t)list->cluster<<32|list->len);
#if FCC_DEBUG>0
      log_it(3, "#cl=%u len=%u\n", list->cluster, list->len);
#endif
      if (total) *total += list->len;
      list++;
   }
   return 0;
}

/** convert list to cc_loc array.
    @param cl           list
    @param start        1st cluster of area to convert
    @param cutsrc       flag (1/0) to remove converted area from cl list
    @return cc_loc array in this module`s owned heap block */
static cc_loc *list2loc(TUQList *cl, u32t start, int cutsrc) {
   u32t  chains = cl->Count(), ii, pos, reslen = 0,
         dellen = 0, delpos = FFFF;
   u64t     *pa = (u64t*)cl->Value();
   cc_loc  *res = (cc_loc*)calloc(chains+1,sizeof(cc_loc));

   for (ii=0, pos=0; ii<chains; ii++) {
      u32t clen = (u32t)pa[ii];
      // start of converting area
      if (!reslen) {
         if (pos>=start || pos+clen>start) {
            res[reslen].cluster = start;
            res[reslen].len     = pos+clen-start;
            if (cutsrc)
               if (start>pos) pa[ii]-=res[reslen].len; else
                  { delpos = ii; dellen++; }
            reslen++;
         }
      } else {
         res[reslen].cluster = pa[ii]>>32;
         res[reslen++].len   = clen;
         if (cutsrc) {
            if (delpos==FFFF) delpos = ii;
            dellen++;
         }
      }
      pos += clen;
   }
   // cut block
   if (reslen+1<chains)
      res = (cc_loc*)realloc(res, (reslen+1)*sizeof(cc_loc));
   // and source list (if asked)
   if (cutsrc && dellen) cl->Delete(delpos, dellen);
   return res;
}

/** calculate number of sectors to read.
   @param [out] first   number of bytes at the end of start sector
   @param [out] last    number of bytes at the start of last sector
   @return full # of *complete* sectors */
static u32t calcsector(u64t pos, u32t size, u32t secshift, u32t *first, u32t *last) {
   u32t  secsz = 1<<secshift,
           una = pos & secsz - 1;
   *last = 0;
   if (una) {
      una = secsz - una;
      if (una>size) { *first = size; return 0; }
      *first = una;
      pos   += una;
      size  -= una;
   }
   una = size & secsz - 1;
   if (una) {
      *last  = una;
      size  -= una;
   }
   return size>>secshift;
}

static qserr _exicc fc_setdisk(EXI_DATA, void *udata, cc_cladd af, cc_clfree df,
                            cc_chattr cf, u32t disk, u64t datapos, u32t cllim,
                            u8t clsize)
{
   u32t   secsz = 0;
   u64t   dsksz = hlp_disksize64(disk, &secsz);
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!dsksz) return E_DSK_NOTMOUNTED;
   if (fd->valid) return E_SYS_INITED;
   if (datapos>=dsksz) return E_SYS_INVPARM;

   fd->clnum    = 0;
   fd->clshift  = clsize;
   fd->secshift = bsf32(secsz);
   fd->dataofs  = datapos;
   // check disk length
   if ((u64t)cllim<<fd->clshift > dsksz-datapos<<fd->secshift)
      return E_SYS_INVPARM;
   fd->userarg  = udata;
   fd->af       = af;
   fd->df       = df;
   fd->cf       = cf;
   fd->disk     = disk;
   fd->clnum    = cllim;
   return 0;
}

static int is_ro(filedata *fd) {
   return fd->cf==0 || (fd->opts&FCCF_READONLY) || (fd->attr&IOFA_READONLY);
}

static qserr cc_flush(filedata *fd);
static void  cc_free (fcentry *fe);

static qserr _exicc fc_flush(EXI_DATA) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->valid) return E_SYS_NONINITOBJ;
   return cc_flush(fd);
}

static u32t _std dce_notify(u32t code, void *usr) {
   filedata *fd = (filedata*)usr;
   switch (code) {
      case DCN_MEM:
         if (fd->fcl)
            while (fd->fcl->Count()) cc_free((fcentry*)(*fd->fcl)[0]);
         return DCNR_SAFE;
      case DCN_COMMIT:
         return cc_flush(fd)?DCNR_UNSAFE:DCNR_SAFE;
   }
   return DCNR_SAFE;
}

// empty file data
static void fc_release(filedata *fd) {
   // free cache entries
   if (fd->fcl) {
      while (fd->fcl->Count()) cc_free((fcentry*)(*fd->fcl)[0]);
      delete fd->fcl;
      fd->fcl = 0;
   }
   if (fd->sector) { free(fd->sector); fd->sector = 0; }
   if (fd->list) { delete fd->list; fd->list = 0; }
   fd->size   = 0;
   fd->nzsize = 0;
   fd->attr   = 0;
   fd->valid  = 0;
   fd->opts   = 0;
}

static qserr _exicc fc_close(EXI_DATA) {
   qserr  res;
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->valid) return E_SYS_NONINITOBJ;
   // update access time
   io_timetoio(&fd->atime, time(0));
   // flush it, but drop error if object/fs is read-only
   res = cc_flush(fd);
   if (res==E_SYS_READONLY) res = 0;
   sys_dcedel(fd);
   fc_release(fd);
   return res;
}

static qserr _exicc fc_stat(EXI_DATA, io_handle_info *si) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!si) return E_SYS_ZEROPTR;
   if (!fd->valid) return E_SYS_NONINITOBJ;
   memset(si, 0, sizeof(io_handle_info));
   si->attrs  = fd->attr;
   si->fileno = -1;
   si->size   = fd->size;
   si->vsize  = fd->nzsize;
   si->vol    = 0xFF;
   memcpy(&si->ctime, &fd->ctime, sizeof(io_time));
   memcpy(&si->atime, &fd->atime, sizeof(io_time));
   memcpy(&si->wtime, &fd->wtime, sizeof(io_time));
   return 0;
}

static u64t _exicc fc_size(EXI_DATA) {
   instance_ret(fd, FFFF64);
   if (!fd->valid) return FFFF64;
   return fd->size;
}

static void cc_add(fcentry *fe) {
   hlp_blistadd(fe, FIELDOFFSET(fcentry,prev), (void**)&celist, (void**)&ceend);
}

/** get cache entry for the specified position.
   @param fd      file data
   @param index   cache intry index (position>>CACHE_SHIFT)
   @return full # of *complete* sectors */
static fcentry *cc_findpos(filedata *fd, u32t index) {
   if (fd->emb) return 0;

   if (!checkbit(fd->bf,index)) return 0; else {
      fcentry *res;
      for (u32t ii=0; ii<fd->fcl->Count(); ii++) {
         res = (fcentry*)(*fd->fcl)[ii];
         if (res->sign!=FDCE_SIGN) return 0;
         if (res->pos==index) break;
      }
      // move entry to the end of list
      if (res) {
         hlp_blistdel(res, FIELDOFFSET(fcentry,prev), (void**)&celist, (void**)&ceend);
         cc_add(res);
         res->ltime = sys_clock();
      }
      return res;
   }
}

/** commit all entries.
    commit file by file - to increase a chance to preserve valid fs state. */
static void cc_commit_all() {
   mt_swlock();

   u32t  nwerr = 0;
   fcentry *fe = celist;
   while (fe) {
      if (fe->nwsectors) cc_flush(fe->fd);
      if (fe->nwsectors) nwerr++;
      fe = fe->next;
   }
#if FCC_DEBUG>0
   log_it(3, "cc_commit_all: used %u max %u err %u\n", used_blocks, cc_max_blocks, nwerr);
#endif
   fe = celist;
   while (fe && cc_max_blocks<=used_blocks)
      if (!fe->nwsectors) {
         fcentry *next = fe->next;
         cc_free(fe);
         fe = next;
      } else
         fe = fe->next;

   mt_swunlock();
}

/// allocate new zero-filled fcentry (low level)
static fcentry* cc_allocentry(filedata *fd) {
   // use fd as pool value to allow batch free by it
   fcentry *re = (fcentry*)mem_allocz(QSMEMOWNER_FILECC, (u32t)fd, sizeof(fcentry));
   re->sign  = FDCE_SIGN;
   re->ltime = sys_clock();
   re->fd    = fd;

   if (cc_max_blocks<=used_blocks) cc_commit_all();

   re->mem   = (u8t*)hlp_memallocsig(CACHE_STEP, memsig, QSMA_RETERR|QSMA_NOCLEAR);
   if (!re->mem) { mem_free(re); re = 0; } else
      { fd->fcl->Add(re); used_blocks++; }
   return re;
}

/// alloc/fill new fcentry and (also) switch off embedded array usage
static fcentry *cc_alloc(filedata *fd, u32t index) {
   fcentry *fe;
   // convert embedded into the normal cache
   if (fd->emb) {
      int  sizeok = fd->nzsize<=BIT_BUFFER;
      fcentry *f0 = 0;

      if (sizeok && fd->nzsize) {
         f0 = cc_allocentry(fd);
         if (!f0) return 0;
         memcpy(f0->mem, &fd->bf, fd->nzsize);
         // only 1st slice is valid
         f0->pos      = 0;
         cc_add(f0);
      }
      // now bf array is valid
      memset(&fd->bf, 0, sizeof(fd->bf));
      fd->emb = 0;
      if (f0) {
         setbit(fd->bf,0);
         // this is just what caller wants
         if (index==0) return f0;
      }
   }
   fe = cc_allocentry(fd);
   if (!fe) return 0;
   fe->pos = index;
   setbit(fd->bf, index);
   cc_add(fe);
   return fe;
}

// custom message forces panic here
static void cc_check(fcentry *fe, char *msg) {
   if (!fe || fe->sign!=FDCE_SIGN || msg) {
      log_printf("invalid cache blk: %08X\n", fe);
      if (fe) {
         log_printf("%16lb\n", (u8t*)fe-16);
         log_printf("%16lb\n", fe);
      }
      _throwmsg_(msg?msg:"cache: memory violation");
   }
}

// pair for cc_alloc
static void cc_free(fcentry *fe) {
   cc_check(fe,0);
   filedata *fd = fe->fd;
   resetbit(fd->bf, fe->pos);

   hlp_blistdel(fe, FIELDOFFSET(fcentry,prev), (void**)&celist, (void**)&ceend);

   int idx = fd->fcl->IndexOf(fe);
   if (idx>=0) fd->fcl->Delete(idx);

   fe->sign = 0;
   if (fe->mem) { hlp_memfree(fe->mem); fe->mem = 0; used_blocks--; }
   free(fe);
}

static qserr cc_expand(filedata *fd, u64t newsize);

static qserr cc_shrink(filedata *fd, u64t newsize) {
   if (newsize>fd->size) return cc_expand(fd,newsize);
   if (!fd->df) return E_SYS_READONLY;

   if (fd->size!=newsize) {
      u32t ccl = fd->size>>fd->clshift,
           ncl = newsize>>fd->clshift;
      // update valid data limit
      if (fd->nzsize>newsize) fd->nzsize = newsize;

      if (ncl!=ccl) {
         cc_loc *dellist = list2loc(fd->list, ncl, 1);
         qserr        rc = fd->df(fd->userarg, dellist);
         free(dellist);
         return rc;
      } else
         fd->size = newsize;
   }
   return 0;
}

static qserr cc_expand(filedata *fd, u64t newsize) {
   if (newsize<fd->size) return cc_shrink(fd,newsize);
   if (!fd->af) return E_SYS_READONLY;

   if (newsize!=fd->size) {
      u32t ccl = fd->size>>fd->clshift,
           ncl = newsize>>fd->clshift;
      if (ncl!=ccl) {
         u32t  chains = fd->list->Count(),
                 hint = 0;
         cc_loc *list = 0, *lp;

         if (chains) {
            u64t lv = (*fd->list)[chains-1];
            hint    = (u32t)(lv>>32) + (u32t)lv;
         }
         qserr res = fd->af(fd->userarg, hint, ncl-ccl, &list);
         if (res) return res;
         lp = list;
         // append
         if (lp->cluster==hint && hint) (*fd->list)[chains-1] += lp++->len;
         res = loc2list(fd->list, lp, fd->clnum, 0);
         if (res) return res;

         free(list);
      } else
         fd->size = newsize;
   }
   return 0;
}

static qserr _exicc fc_chsize(EXI_DATA, u64t nsize) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (is_ro(fd)) return E_SYS_READONLY;

   if (nsize==fd->size) return 0; else {
      qserr rc = nsize>fd->size ? cc_expand(fd, nsize) : cc_shrink(fd, nsize);
      if (!rc) rc = cc_flush(fd);
      return rc;
   }
}

typedef struct {
   u32t     index;     ///< index of chain in list
   u32t       ofs;     ///< cluster position in chain
   u32t      srem;     ///< sectors, remain in the cluster
   u64t  lcsector;     ///< last call`s 1st sector (always updated, for error printing)
} rwcall_state;

/** find location of file sector in the chain list.
   @param fd            file data
   @param filesec       file sector to search for
   @param [out] rws     state record for rwcall()
   @return error code or 0 */
static qserr cc_listpos(filedata *fd, u64t filesec, rwcall_state *rws) {
   u32t   idx = 0,
         stcl = 0,
       chains = fd->list->Count();
   u8t    c2s = fd->clshift - fd->secshift;

   while (idx<chains) {
      u32t len = (u32t)(*fd->list)[idx];
      if (stcl<=filesec>>c2s && stcl+len>filesec>>c2s) {
         rws->index = idx;
         rws->ofs   = (filesec>>c2s) - stcl;
         rws->srem  = (1<<c2s) - (filesec & (1<<c2s) - 1);
#if FCC_DEBUG>1
         log_it(3, "cc_listpos %LX rws: idx=%u ofs=%u srem=%u\n", filesec,
            rws->index, rws->ofs, rws->srem );
#endif
         return 0;
      }
      stcl += len;
      idx++;
   }
   return E_DSK_FILEALLOC;
}

/// @name flags for rwcall()
//@{
#define RW_WRITE   1   ///< write action
#define RW_DIRECT  2   ///< use QDSK_DIRECT (1/0)
#define RW_NOSEEK  4   ///< stay in the same position (i.e. no index/ofs update)
//@}

/** call read/write sectors & update position in the cluster.
   @param fd            file data
   @param [in,out] rws  state of operation
   @param sectors       # of sectors to read/write
   @param buf           buffer for/with data
   @param flags         RW_* options (see above)
   @param [out] act     # of actually updated sectors
   @return error code or 0 */
static qserr rwcall(filedata *fd, rwcall_state *rws, u32t sectors, void *buf,
                    u32t flags, u32t *act)
{
   const u64t *pa = fd->list->Value();
   u32t    chains = fd->list->Count(),
              idx = rws->index, until_eoc,
             disk = (flags&RW_DIRECT) || (fd->opts&FCCF_DIRECT) ?
                    fd->disk|QDSK_DIRECT : fd->disk&~QDSK_DIRECT;
   u8t      *bpos = (u8t*)buf,
              c2s = fd->clshift - fd->secshift;
   qserr      res = flags&RW_WRITE?E_DSK_ERRWRITE:E_DSK_ERRREAD;
   u64t    sector;
#if FCC_DEBUG>1
   log_it(3, "rwcall(%08X,{idx:%u,ofs:%u,srem:%u},%u,%08X,%X) disk %X\n", fd,
      idx, rws->ofs, rws->srem, sectors, buf, flags, disk);
#endif
   // both should be internal error
   if (rws->index>=chains) return E_SYS_SOFTFAULT;
   if (rws->ofs>=(u32t)pa[idx]) return E_SYS_SOFTFAULT;
   // sectors until the end of current chain
   until_eoc = ((u32t)pa[idx] - rws->ofs - 1 << c2s) + rws->srem;
   sector    = ((pa[idx]>>32) + rws->ofs + 1 << c2s) - rws->srem;
   if (act) *act = 0;

   while (sectors) {
      u32t actsz, readsz;
      // rsw may point to the end of chain
      if (!until_eoc) {
         // internal error: size mismatch should be filtered in caller
         if (++idx==chains) return E_SYS_SOFTFAULT;
         sector    = (pa[idx]>>32) << c2s;
         until_eoc = (u32t)pa[idx] << c2s;
      }
      readsz = until_eoc;
      if (readsz>sectors) readsz = sectors;
      // read/write it
      actsz  = (*(flags&RW_WRITE?&hlp_diskwrite:hlp_diskread))
               (disk, rws->lcsector=fd->dataofs+sector, readsz, bpos);
      sectors   -= actsz;
      bpos      += actsz<<fd->secshift;
      readsz    -= actsz;
      if (act) *act += actsz;
      /* update position. Allow "end of chain" pos here to avoid "end of file"
         special case after update */
      if ((flags&RW_NOSEEK)==0) {
         u32t   rem = until_eoc - actsz,
              csize = 1<<c2s,
           chainlen = (u32t)pa[idx];
         rws->index = idx;
         // 0      |       | full-1    full-1 || full-1
         // full   |       | full           1 || 0
         if (rem) {
            rws->ofs  = chainlen - 1 - (rem-1 >> c2s);
            rws->srem = (rem-1 & csize-1) + 1;
         } else {
            rws->ofs  = chainlen - 1;
            rws->srem = 0;
         }
      }
      if (readsz) break;
      until_eoc -= actsz;
   }
   if (!sectors) res = 0;
   return res;
}

/** commit entry.
    @return number of unwritten sectors (write error) */
static u32t cc_commit(fcentry *fe) {
   filedata *fd;
   u32t  errcnt = 0;
   cc_check(fe,0);
#if FCC_DEBUG>1
   log_it(3, "cc_commit %X %u\n", fd, fe->nwsectors);
#endif
   if (fe->nwsectors) {
      filedata *fd = fe->fd;
      u32t  bmsize = CACHE_STEP>>fd->secshift,
               pos = 0, alen = 0, apos = 0, rlen = 0;
      while (pos<bmsize && fe->nwsectors) {
         int write = pos==bmsize-1;
         /* skip single zero bit (one sector), i.e. write it too to optimize
            number of i/o calls */
         if (checkbit(fe->nwmask,pos) || alen && pos<bmsize-1 &&
            checkbit(fe->nwmask,pos+1))
         {
            if (alen==0) apos = pos;
            alen++;
            rlen += resetbit(fe->nwmask,pos);
         } else
         if (alen) write = 1;

         if (write || fe->nwsectors<=rlen) {
            rwcall_state rws;
            qserr        res;
            u32t         act = 0;
            if (!alen || fe->nwsectors<rlen)
               cc_check(fe, "cache: bit count violation");

            res = cc_listpos(fd, (fe->pos<<CACHE_SHIFT-fd->secshift)+apos, &rws);
            if (res) cc_check(fe, "cache: file map violation");

            res = rwcall(fd, &rws, alen, fe->mem + (apos<<fd->secshift),
               RW_WRITE|RW_DIRECT|RW_NOSEEK, &act);
            // write error?
            if (res) {
               u32t errlen = alen-act;
               log_it(0, "cache write error: disk %X, sector %LX, len %u of %u\n",
                  fd->disk, rws.lcsector, act, alen);
               // set bits back
               setbits(&fe->nwmask, apos+act, errlen, SBIT_ON);
               fe->nwsectors += errlen;
               fd->nwpresent += errlen;
               errcnt += errlen;
            }
            fe->nwsectors -= rlen;
            fd->nwpresent -= rlen;

            alen = 0;
            rlen = 0;
         }
         pos++;
      }
   }
   return errcnt;
}

static int __stdcall sort1_fcentry(const void *lp1, const void *lp2) {
   fcentry *e1 = *(fcentry**)lp1,
           *e2 = *(fcentry**)lp2;
   if (e1->pos==e2->pos) return 0;
   return e1->pos<e2->pos?-1:1;
}

static int __stdcall sort2_fcentry(const void *lp1, const void *lp2) {
   fcentry *e1 = *(fcentry**)lp1,
           *e2 = *(fcentry**)lp2;
   // sort records by pos, but put uncommitted first
   if (e1->pos==e2->pos) return 0;
   if (!e1->nwsectors && e2->nwsectors) return 1;
   if ((!e1->nwsectors || e2->nwsectors) && e1->pos>e2->pos) return 1;
   return -1;
}

/** a bit dumb method of sorting file cache entries.
   @param fd            file data
   @param unwr1st       put uncommitted cache entries to the start of list */
static void cc_sort(filedata *fd, int unwr1st) {
   if (fd && fd->fcl && fd->fcl->Count())
      qsort((void*)fd->fcl->Value(), fd->fcl->Count(), sizeof(void*),
         unwr1st?sort2_fcentry:sort1_fcentry);
}

static qserr cc_flush(filedata *fd) {
   io_handle_info  fi;
   if (is_ro(fd)) return E_SYS_READONLY;

   if (fd->nwpresent) {
      if (fd->emb) {
         if (fd->nzsize) {
            rwcall_state rws;
            u32t  una1, una2, rwres,
                        nsec = calcsector(0, fd->nzsize, fd->secshift, &una1, &una2);
            qserr        res = cc_listpos(fd, 0, &rws);
            if (res) return res;
            // zero remaining part of the last sector
            if (una2) memset((u8t*)&fd->bf + (nsec<<fd->secshift) + una2, 0,
               (1<<fd->secshift) - una2);

            res = rwcall(fd, &rws, nsec+(una2?1:0), &fd->bf, RW_DIRECT|RW_WRITE|RW_NOSEEK, 0);
            if (res) return res;
         }
         // flag that data was saved
         fd->nwpresent = 0;
      } else {
         u32t ii, errcnt = 0;

         cc_sort(fd, 1);
         for (ii=0; ii<fd->fcl->Count(); ii++) {
            fcentry *fe = (fcentry*)(*fd->fcl)[ii];
            /* list is sorted above amd first entry without sector to commit
               means the end */
            if (!fe->nwsectors) break;
            errcnt += cc_commit(fe);
         }
         if (errcnt || fd->nwpresent) return E_DSK_ERRWRITE;
      }
   }
   fi.fileno = -1;
   fi.vol    = 0xFF;
   fi.size   = fd->size;
   fi.attrs  = fd->attr;
   fi.vsize  = fd->nzsize;
   memcpy(&fi.ctime, &fd->ctime, sizeof(io_time));
   memcpy(&fi.atime, &fd->atime, sizeof(io_time));
   memcpy(&fi.wtime, &fd->wtime, sizeof(io_time));
   return fd->cf(fd->userarg, &fi);
}

static qserr rwaction(filedata *fd, u64t pos, void *buf, u32t size, u32t *act,
                      int wr)
{
   rwcall_state rws;
   qserr        res = 0;
   u8t          *bp = (u8t*)buf;
   u32t     secsize = 1<<fd->secshift;

   if (!act) return E_SYS_ZEROPTR;
   *act = 0;
   if (!size) return 0;
   if (!buf) return E_SYS_ZEROPTR;
   if (pos>fd->size) return E_SYS_INVPARM;
#if FCC_DEBUG>0
   log_it(3, "%c: %X pos=%Lu size=%u nz=%Lu\n", wr?'w':'r', fd, pos, size, fd->nzsize);
#endif
   if (pos+size>fd->size) {
      if (!wr) {
         size = fd->size - pos;
         if (!size) return E_SYS_EOF;
      } else {
         res  = cc_expand(fd, pos+size);
         if (res) return res;
      }
   }
   if (pos<_4GBLL && (fd->opts&FCCF_NOCACHE)==0) {
      if (fd->emb) {
          // we have reached embedded buffer size
          if (wr && pos+size>BIT_BUFFER) {
             res = cc_flush(fd);
             if (res) return res;
             /* we flushed emb in the line above, so able to drop it
                if failed to convert into regular entry */
             if (!cc_alloc(fd,0)) fd->emb = 0;
          } else {
             u8t *ca = (u8t*)&fd->bf;
             u32t pd = (u32t)pos;      // r/w pos here < BIT_BUFFER
             if (wr) {
                memcpy(ca+pd, buf, size);
                fd->nwpresent = 1;
             } else {
                u64t p_end = pos+size;
                /* take valid file data size in mind (it can be 64-bit, but
                   with 8k of valid data) */
                if (pos<fd->nzsize) {
                   memcpy(bp, ca+pd, p_end<=fd->nzsize?size:fd->nzsize-pd);
                   if (p_end>fd->nzsize)
                      memset(bp+(fd->nzsize-pd), 0, p_end-fd->nzsize);
                } else
                   memset(buf, 0, size);
             }
             // we are done here
             *act += size;
             size  = 0;
          }
      }
      if (!fd->emb) {
         u32t   usize = pos+size<=_4GBLL ? size : _4GBLL-pos;

         do {
            u32t   index = pos>>CACHE_SHIFT, una1, una2, nsec, fcepos,
                     csz = ((u64t)(index+1)<<CACHE_SHIFT) - pos;
            fcentry *fce = cc_findpos(fd, index);
            // size to read until the end of current cache block
            if (csz>usize) csz = usize;

            // # of *full* sectors to read + lead & trail size in bytes
            if (!fce || wr)
               nsec = calcsector(pos, csz, fd->secshift, &una1, &una2);

            if (!fce) {
               u32t filepos = (u64t)index<<CACHE_SHIFT, n2read;
               // size, need to be read really
               if (filepos>=fd->nzsize) n2read = 0; else {
                  n2read = fd->nzsize - filepos;
                  if (n2read>CACHE_STEP) n2read = CACHE_STEP;
               }
               // read after the end of available data - just skip allocation
               if (wr || n2read) {
                  fce = cc_alloc(fd, index);
                  // go to sector cache on allocation error
                  if (!fce) break;

                  if (n2read) {
                     u32t  seccnt = 1<<CACHE_SHIFT-fd->secshift, asz = 0;

                     if (n2read<CACHE_STEP)
                        seccnt = n2read + secsize - 1 >> fd->secshift;

                     res = cc_listpos(fd, filepos>>fd->secshift, &rws);
                     if (res) return res;
                     /* read block (until the end of existing data), but ignore
                        error and let the sector level read to try it again */
                     res = rwcall(fd, &rws, seccnt, fce->mem, RW_DIRECT|RW_NOSEEK, &asz);
                     if (res) { cc_free(fce); break; }
                     // zero remaining part
                     if (n2read<CACHE_STEP)
                        memset(fce->mem+n2read, 0, CACHE_STEP-n2read);
                  } else
                     memset(fce->mem, 0, CACHE_STEP);
               }
            }
            fcepos = pos & CACHE_STEP-1;
            if (wr) {
               nsec += (una1?1:0) + (una2?1:0);
               memcpy(fce->mem+fcepos, bp, csz);
               // update bit mask & sums for the future writings
               setbits(&fce->nwmask, fcepos>>fd->secshift, nsec>>fd->secshift, SBIT_ON);
               fce->nwsectors += nsec;
               fd->nwpresent  += nsec;
            } else
            if (!fce) memset(bp, 0, csz); else
               memcpy(bp, fce->mem+fcepos, csz);
            usize -= csz;
            size  -= csz;
            *act  += csz;
            bp    += csz;
            pos   += csz;
         } while (usize);
      }
   }

   /* area above 4Gb just forwarded to the sector level cache,
      as well as FCCF_NOCACHE flag and any other cache failure above */
   if (size) {
      u32t  una1, una2, rwres,
            nsec = calcsector(pos, size, fd->secshift, &una1, &una2);
      // allocate sector for the first time unaligned read
      if (una1 || una2)
         if (!fd->sector) fd->sector = (u8t*)malloc(1<<fd->secshift);

      while (una1 || una2 || nsec) {
         u32t opsize;
         // read/update first/last misaligned sector
         if (una1 || !nsec && una2) {
            opsize = una1?una1:una2;
            res    = rwcall(fd, &rws, 1, fd->sector, wr?RW_NOSEEK:0, &rwres);
            if (res) return res;

            if (wr) {
               if (una1) memcpy(fd->sector+(secsize-una1), bp, una1); else
                  memcpy(fd->sector, bp, una2);
               res = rwcall(fd, &rws, 1, fd->sector, RW_WRITE, &rwres);
               if (res) return res;
            } else
               if (una1) memcpy(bp, fd->sector+(secsize-una1), una1); else
                  memcpy(bp, fd->sector, una2);
            *act  += opsize;
            if (una1) una1 = 0; else una2 = 0;
         } else
         if (nsec) {
            res    = rwcall(fd, &rws, nsec, bp, wr?RW_WRITE:0, &rwres);
            opsize = rwres<<fd->secshift;
            *act  += opsize;
            if (res) return res;
            nsec   = 0;
         }
         size -= opsize;
         bp   += opsize;
      }
   }
   if (wr && !res) io_timetoio(&fd->wtime, time(0));
   return res;
}

static qserr _exicc fc_read(EXI_DATA, u64t pos, void *buf, u32t size, u32t *act) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->valid) return E_SYS_NONINITOBJ;
   return rwaction(fd, pos, buf, size, act, 0);
}

static qserr _exicc fc_write(EXI_DATA, u64t pos, void *buf, u32t size, u32t *act) {
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fd->valid) return E_SYS_NONINITOBJ;
   if (!act) return E_SYS_ZEROPTR;
   *act = 0;

   if (is_ro(fd)) return E_SYS_READONLY; else {
      qserr res = rwaction(fd, pos, buf, size, act, 1);
      // update valid data limit
      if (pos+*act > fd->nzsize) fd->nzsize = pos+*act;
      return res;
   }
}

static qserr _exicc fc_init(EXI_DATA, io_handle_info *fi, cc_loc *list, u32t opts) {
   u32t clnum = 0;
   instance_ret(fd, E_SYS_INVOBJECT);
   if (!fi) return E_SYS_ZEROPTR;
   if (Xor(fi->size,list) || fi->size<fi->vsize) return E_SYS_INVPARM;
   // setdisk should be called at least once
   if (!fd->clnum) return E_SYS_NONINITOBJ;
   // previous file must be closed
   if (fd->list || fd->fcl) return E_SYS_INITED;

   fd->valid  = 0;
   fd->attr   = fi->attrs;
   fd->size   = fi->size;
   fd->opts   = opts;
   fd->nzsize = fi->vsize;
   if ((s64t)fd->size<0) return E_SYS_INVPARM;
   // limit file size to be 32-bit in clusters
   if (fd->size>>fd->clshift > (u64t)FFFF) return E_SYS_TOOLARGE;

   memcpy(&fd->ctime, &fi->ctime, sizeof(io_time));
   memcpy(&fd->atime, &fi->atime, sizeof(io_time));
   memcpy(&fd->wtime, &fi->wtime, sizeof(io_time));
   // alloc lists
   fd->list = new TUQList;
   fd->fcl  = new TPtrList;

   if (list) {
      u32t     ii;
      s64t   diff;
      qserr   res;

      res  = loc2list(fd->list, list, fd->clnum, &clnum);
      if (res) return res;
      diff = ((s64t)clnum<<fd->clshift) - (s64t)fd->size;

      if (diff<0 || diff>=1<<fd->clshift) return E_DSK_FILESIZE;
   }
   // preload data into the bit buffer if valid data size is smaller than it
   if (fd->nzsize<BIT_BUFFER && (opts&FCCF_NOCACHE)==0) {
      if (fd->nzsize) {
         rwcall_state rws;
         u32t        nsec = fd->nzsize + (1<<fd->secshift) - 1 >> fd->secshift, act;
         qserr        res = cc_listpos(fd, 0, &rws);
         if (res) return res;
         /* direct read to bf buffer. bf size can be 4k (for 128k step) or
            8k (for 64k step) - both cases are well aligned */
         res = rwcall(fd, &rws, nsec, &fd->bf, RW_DIRECT, &act);
         if (res) return res;
      }
      fd->emb = 1;
   } else {
      memset(&fd->bf, 0, sizeof(fd->bf));
      fd->emb = 0;
   }
#if FCC_DEBUG>0
   log_it(2, "size=%Lu, nzsize=%Lu, #cl=%u emb=%u\n", fd->size, fd->nzsize, clnum, fd->emb);
#endif
   fd->valid = 1;
   sys_dcenew(fd, DCF_NOTIMER, 0, dce_notify);
   return 0;
}

static void _std fc_new(void *instance, void *userdata) {
   filedata *fd = (filedata*)userdata;
   fd->sign     = FDCC_SIGN;
}

static void _std fc_del(void *instance, void *userdata) {
   filedata *fd = (filedata*)userdata;
   if (fd->sign==FDCC_SIGN) {
      if (fd->valid) log_it(2, "Open file destructor? (%08X)\n", instance);
      fc_release(fd);
   }
   memset(fd, 0, sizeof(filedata));
}

#if FCC_OK > 0
static void *m_list[] = { fc_setdisk, fc_init, fc_flush, fc_close, fc_chsize,
   fc_stat, fc_size, fc_read, fc_write  };
#endif

void exi_register_filecc(void) {
   u32t id;
   // something forgotten! interface part is not match to implementation
#if FCC_OK > 0
   if (sizeof(_qs_filecc)!=sizeof(m_list))
      _throwmsg_("qs_filecc: size mismatch");
   id = exi_register("qs_filecc", m_list, sizeof(m_list)/sizeof(void*),
      sizeof(filedata), 0, fc_new, fc_del, 0);
   if (!id) log_printf("qs_filecc registration error!\n");
#endif
   // use 1/8 of memory for the file cache
   cc_max_blocks = hlp_memavail(0,0)>>CACHE_SHIFT+3;
}
