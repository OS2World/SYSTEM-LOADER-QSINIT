//
// QSINIT
// partition management
//
#include "stdlib.h"
#include "qsdm.h"
#include "pscan.h"
#include "qsint.h"

// use 0F for extended
#define EXT_TYPE  PTE_05_EXTENDED

static const char *ExtMngWarning = "Logical partition management "
        "still dangerous and weakly tested!^Continue at own risc?";
static int        ExtMngWarnFlag = 1;

static u32t dsk_shrinkslice(hdd_info *hi, u32t quad, u32t size, u32t heads);

void lba2chs(u32t heads, u32t spt, u32t start, u8t *phead, u16t *pcs) {
   u32t spercyl = heads * spt,
            cyl = start / spercyl;
   if (cyl < 1024) {
      u32t  sector = start % spercyl % spt,
              head = start % spercyl / spt;
      *pcs   = (sector&0x3F)+1 | (cyl&0x300)>>2 | (cyl&0xFF)<<8;
      *phead = head;
   } else {
      *phead = 0xFE;   // 1023 x 254 x 63
      *pcs   = 0xFFFF;
   }
}

qserr _std dsk_ptinit(u32t disk, int lvmmode) {
   FUNC_LOCK         lk;
   hdd_info         *hi = get_by_disk(disk);
   u32t       stype, ii;
   u8t       mbr_buffer[MAX_SECTOR_SIZE];
   disk_geo_data   vgeo;
   struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;

   if (!hi) return E_DSK_DISKNUM;
   if (lvmmode>2) return E_SYS_INVPARM;
   dsk_ptrescan(disk,1);
   stype = dsk_sectortype(disk, 0, mbr_buffer);
   if (stype==DSKST_ERROR) return E_DSK_ERRREAD;

   log_it(2, "dsk_ptinit(%X), stype %d\n", disk, stype);

   if (stype==DSKST_EMPTY || stype==DSKST_DATA) {
      // clear pt entries is case of garbage data
      if (!dsk_newmbr(disk,DSKBR_CLEARPT|DSKBR_GENDISKID)) return E_DSK_ERRWRITE;
   } else
   if (hi->pt_size>4) return E_PTE_EMPTY; else {
      if (hi->pt_size)
         for (ii=0; ii<4; ii++)
            if (hi->pts[ii].PTE_Type) return E_PTE_EMPTY;
      if (!dsk_newmbr(disk,DSKBR_GENDISKID)) return E_DSK_ERRWRITE;
   }
   // erase LVM info signatures if present and NOT asked
   if (!lvmmode) {
      if (hi->lvm_snum) {
         hi->lvm_snum = 0;
         hi->dlat[0].DLA_Signature1 = 0;
         hi->dlat[0].DLA_Signature2 = 0;
         lvm_flushdlat(disk,0);
      } else {
         dsk_emptysector(disk, hi->lvm_spt-1, 1);
         if (hi->lvm_spt>63) dsk_emptysector(disk, 62, 1);
      }
   }
   // get current geometry, but without DLAT assistance
   ii = dsk_getptgeo(disk,&vgeo);
   if (ii) return ii;
   /* special geometry processing for LVM mode
      using Daniella`s code here, because system will look to the
      disk through its prism ;) */
   if (lvmmode) {
      if (hi->info.Cylinders>1024) {
         // normal disk
         u32t spt = 63,
            heads = 255, cyls;

         if (hi->info.TotalSectors <= 1024 * 128 * 63) {
            heads = (hi->info.TotalSectors - 1) / (1024 * 63);
            if (heads<16) heads = 16; else
            if (heads<32) heads = 32; else
            if (heads<64) heads = 64; else heads = 128;
         }
         do {
            cyls = hi->info.TotalSectors / (heads * spt);
            if (cyls>>16)
               if (spt<128) spt = spt<<1|1; else cyls = 65535;
         } while (cyls>>16);

         vgeo.Cylinders   = cyls;
         vgeo.Heads       = heads;
         vgeo.SectOnTrack = spt;
      } else {
         // tiny /pae ram disk
         memcpy(&vgeo, &hi->info, sizeof(disk_geo_data));
      }
      // lvm_spt will be updated here
      ii = lvm_initdisk(disk, &vgeo, lvmmode==2?1:-1);
      if (ii) log_it(2, "lvm_initdisk(%X) err %d\n", disk, ii);
   } else
      hi->lvm_spt = vgeo.SectOnTrack;
   // rescan to update free space list and actual state
   dsk_ptrescan(disk, 1);

   return 0;
}

u32t _std dsk_ptgetfree(u32t disk, dsk_freeblock *info, u32t size) {
   FUNC_LOCK   lk;
   hdd_info   *hi = get_by_disk(disk);
   if (!hi) return 0;
   dsk_ptrescan(disk,0);
   if (!info||!size) return hi->fsp_size;
   if (hi->fsp_size) {
      if (size>hi->fsp_size) size=hi->fsp_size;
      memcpy(info, hi->fsp, size*sizeof(dsk_freeblock));
   }
   return hi->fsp_size;
}

qserr _std dsk_ptalign(u32t disk, u32t freeidx, u32t pts, u32t afl,
                       u64t *start, u64t *size)
{
   dsk_freeblock *fi;
   int       gptdisk = dsk_isgpt(disk,-1) > 0;
   u32t           rc = 0, sz;
   if (!start||!size) return E_SYS_INVPARM;
   *start = 0;
   *size  = 0;
   if (dsk_ptgetfree(disk,0,0) <= freeidx) return E_PTE_FINDEX;

   fi = (dsk_freeblock*)malloc((freeidx+1)*sizeof(dsk_freeblock));
   sz = dsk_ptgetfree(disk, fi, freeidx+1);
   // second get free space failed?
   if (sz<=freeidx) {
      rc = dsk_ptrescan(disk,0);
      if (!rc) rc = E_PTE_FINDEX;
   } else {
      u32t    ssize, // sector size
            cylsize = fi[freeidx].Heads * fi[freeidx].SectPerTrack,
                spt = fi[freeidx].SectPerTrack;
      u64t    stpos = fi[freeidx].StartSector, ptsize = 0,
               epos = stpos + fi[freeidx].Length;
      int   atstart = afl&DPAL_ESPC?0:1;
      hlp_disksize(disk,&ssize,0);
      // drop CHS flags on GPT disk
      if (gptdisk) afl &= ~(DPAL_CHSSTART|DPAL_CHSEND|DPAL_ESPC);

      if (pts) {
         // calc partition size
         if (afl&DPAL_PERCENT) {
            if (pts>100) pts = 100;
#if 0 // ?????
            // change 100% align to END to START
            if (pts ==100 && (afl&DPAL_CHSEND)!=0)
               afl = afl&~DPAL_CHSEND|DPAL_CHSSTART;
#endif
            // calc size
            ptsize = fi[freeidx].Length * pts/100;
            // log_it(2, "%08X sectors at %08X \n", ptsize, stpos);
         } else {
            ptsize = (u64t)pts * (1024*1024/ssize);
            if (ptsize > fi[freeidx].Length) { free(fi); return E_PTE_FSMALL; }
         }
      } else
         ptsize = fi[freeidx].Length;
      // partition at the start of free space
      if (atstart) {
         // start pos
         if ((afl&DPAL_CHSSTART) && stpos % cylsize && (stpos-spt) % cylsize) {
            u64t newpos = (stpos + cylsize - 1) / cylsize * cylsize;
            if (newpos < epos) {
               stpos = newpos;
               if (ptsize> epos-stpos) ptsize = epos-stpos;
            }
         }
         // end pos
         if ((afl&DPAL_CHSEND) && (stpos + ptsize) % cylsize) {
            u64t newsz2 = (stpos + ptsize + cylsize - 1) / cylsize * cylsize - stpos,
                 newsz1 = newsz2 - cylsize;
            if (stpos + newsz2 <= epos) ptsize = newsz2; else
            if (stpos + newsz1 > stpos) ptsize = newsz1;
         }
      } else {
         // end pos
         if ((afl&DPAL_CHSEND) && epos % cylsize) {
            u64t nepos = epos / cylsize * cylsize;
            if (nepos > stpos) {
               epos = nepos;
               if (epos - stpos < ptsize) ptsize = epos - stpos;
            }
         }
         // start pos
         if ((afl&DPAL_CHSSTART) && (epos-ptsize)!=spt && ((afl&DPAL_AF) ||
            (epos-ptsize) % cylsize))
         {
            u64t newpos2 = (epos - ptsize + cylsize - 1) / cylsize * cylsize,
                 newpos1 = newpos2 - cylsize;
            // try to align it down to the start of disk
            if (afl&DPAL_AF) {
               u64t   alv = newpos1;
               u32t  nsec = 4096 / ssize,
                    lgfix = afl&DPAL_LOGICAL?spt:0;
               while ((alv+lgfix) % nsec && alv>stpos && alv>cylsize) alv-=cylsize;
               if ((alv+lgfix) % nsec == 0) newpos1 = alv;
            }

            log_it(2, "2: %08LX <- %08LX -> %08LX | %08LX..%08LX %08LX\n",
               newpos1, epos-ptsize, newpos2, stpos, epos, ptsize);

            if (newpos1 >= stpos) ptsize = epos - newpos1; else
            if (newpos2 >= stpos && newpos2 < epos) ptsize = epos - newpos2;
         }
         stpos = epos - ptsize;
      }
      // AF align, but on GPT disks only
      if (gptdisk && (afl&DPAL_AF)) {
         u32t nsec  = 4096 / ssize;
         if (stpos % nsec) { // nsec is always power of 2
            u64t stpa = stpos + nsec - 1 & ~((u64t)nsec - 1);
            ptsize   -= stpa - stpos;
            stpos     = stpa;
         }
      }
      *start = stpos;
      *size  = ptsize;
   }
   free(fi);
   return rc;
}

/// merge free slices in extended partition
qserr dsk_extmerge(u32t disk) {
   FUNC_LOCK      lk;
   hdd_info      *hi = get_by_disk(disk);
   u32t      ii, pti, *ucnt, heads, spt;
   disk_geo_data geo;

   if (!hi) return E_DSK_DISKNUM;
   dsk_ptrescan(disk,0);
   if (hi->scan_rc) return hi->scan_rc;
   // deny GPT disks
   if (hi->gpt_present) return E_PTE_GPTDISK;
   // no extended
   if (!hi->extpos) return 0;
   // get geometry for CHS calc
   if (dsk_getptgeo(disk,&geo)==0) {
      heads = geo.Heads;
      spt   = geo.SectOnTrack;
   } else {
      heads = 255;
      spt   = 63;
   }
   /* Some quads can contain partition, which do not fill free space entirely.
      At least "Partition Manager" can make such table.
      Just add empty record for all of it and then merge by code below */
   for (pti=1; pti<hi->pt_size>>2; pti++) {
      int lrec = dsk_findlogrec(hi, pti);
      if (lrec>=0) {
         struct MBR_Record *rec = hi->pts + lrec + pti * 4;
         u32t   ptend = rec->PTE_LBAStart + rec->PTE_LBASize,
                start = hi->ptspos[pti],
                csize = ((pti==(hi->pt_size>>2)-1 ? hi->extpos+hi->extlen :
                        hi->ptspos[pti+1])) - start, // current slice size
              cylsize = heads*spt;
         // overflow check
         if (ptend + cylsize > ptend) {
            // align to cylinder
            ii = ptend % cylsize;
            if (ii) ptend += cylsize - ii;
            // at least cylinder of lost space is present?
            if (ptend <= start + csize - cylsize) {
               u32t rc = dsk_shrinkslice(hi, pti, ptend-start, heads);
               if (rc) return rc;
               // recurse because data was changed
               return dsk_extmerge(disk);
            }
         }
      } else
      if (pti>1 && pti==(hi->pt_size>>2)-1) {
         /* another one funny thing - empty last slice, which not covers entire
            space until the end of extended */
         int prev_erec = dsk_findextrec(hi, pti-1);
         if (prev_erec<0) return E_PTE_EXTERR; else {
            struct MBR_Record *rec = hi->pts + prev_erec + (pti-1) * 4;
            u32t             ptend = rec->PTE_LBAStart + rec->PTE_LBASize;

            if (ptend<hi->extpos+hi->extlen) {
               rec->PTE_LBASize = hi->extpos+hi->extlen-rec->PTE_LBAStart;
               dsk_flushquad(hi->disk, pti-1);
               // recurse self after rescan
               dsk_ptrescan(disk,1);
               return dsk_extmerge(disk);
            }
         }
      }
   }
   // merge neighbouring free slices
   ucnt = (u32t*)malloc(sizeof(u32t) * (hi->pt_size>>2));
   ucnt[0] = FFFF;

   for (ii=4; ii<hi->pt_size; ii+=4) {
      u32t ptcount = 0, quadpos = ii>>2;
      for (pti=0; pti<4; pti++) {
         struct MBR_Record *rec = hi->pts+(ii+pti);
         u8t pt = rec->PTE_Type;
         if (pt)
            if (!IS_EXTENDED(pt)) ptcount++; else ptcount|=pti<<8;
      }
      ucnt[quadpos] = ptcount;
      // merge if two nearest are free
      if ((ucnt[quadpos]&0xFF)==0 && (ucnt[quadpos-1]&0xFF)==0) {
         u32t previdx = (ucnt[quadpos-1]>>8) + (quadpos-1) * 4,
              curridx = (ucnt[quadpos]>>8) + quadpos * 4,
             pprevidx = quadpos-1 > 1 ? (ucnt[quadpos-2]>>8) + (quadpos-2) * 4 : 0;
         struct MBR_Record *crec = hi->pts + curridx,
                           *prec = hi->pts + previdx;
         int             is_last = crec->PTE_Type==0;
         u32t       inc_size, rc = 0;

         free(ucnt);
         // check positions
         if (!is_last && !IS_EXTENDED(crec->PTE_Type) ||
            !IS_EXTENDED(prec->PTE_Type)) return E_PTE_EXTERR;
         // size to merge
         inc_size = !is_last? hi->ptspos[quadpos+1] - hi->ptspos[quadpos] :
                              prec->PTE_LBASize;
         // previous quad is not first slice in extended?
         if (pprevidx) {
            struct MBR_Record *pprec = hi->pts + pprevidx;

            if (!IS_EXTENDED(pprec->PTE_Type)) return E_PTE_EXTERR;
            // update size of previous block in it`s parent
            pprec->PTE_LBASize += inc_size;
            lba2chs(heads, spt, pprec->PTE_LBAStart+pprec->PTE_LBASize-1,
               &pprec->PTE_HEnd, &pprec->PTE_CSEnd);

            rc = dsk_flushquad(disk, quadpos-2);
            if (rc) return rc;
         }
         /* if quadpos is a last slice in extended - del it from previous
            entry, else replace it`s record by next slice info */
         if (is_last) memset(prec, 0, sizeof(struct MBR_Record));
            else memcpy(prec, crec, sizeof(struct MBR_Record));

         rc = dsk_flushquad(disk, quadpos-1);
         if (rc) return rc;
         // we can ignore errors here - this quad already removed in any way
         lvm_wipedlat(disk, quadpos);
         dsk_wipequad(disk, quadpos, 1);
         // recurse self after rescan
         dsk_ptrescan(disk,1);
         return dsk_extmerge(disk);
      }
   }
   free(ucnt);
   return 0;
}


/** delete empty extended partition
    @attention Function does not call rescan! */
static qserr _std dsk_extdelete_int(hdd_info *hi) {
   if (hi->pt_size==8) {
      int extidx = dsk_findextrec(hi,0);
      if (extidx<0) return E_PTE_EXTERR;
      // not emply?
      if (!dsk_isquadempty(hi,1,0)) return E_PTE_EXTPOP;
      /* wipe quad before calling to pt_action(), because it will force
         rescan after action */
      lvm_wipedlat(hi->disk, 1);
      dsk_wipequad(hi->disk, 1, 1);
      // delete record and update disk
      return pt_action(ACTION_DELETE|ACTION_NORESCAN|ACTION_DIRECT,
         hi->disk, extidx, hi->extpos);
   }
   // invalid call!
   return E_PTE_EXTERR;
}

qserr _std dsk_extdelete(u32t disk) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t       rc;
   // force rescan because this is DELETE
   dsk_ptrescan(disk,1);
   if (hi->scan_rc) return hi->scan_rc;
   // merge slices
   rc = dsk_extmerge(disk);
   if (rc) return rc;
   // finally, delete it
   return dsk_extdelete_int(hi);
}

/** resize extended partition.
    @attention Function does not check free size presence! Caller
               (dsk_ptcreate()) must do this.

    E_PTE_NOFREE will be returned only from some extended partition checks.
    Empty extended partition can be deleted by this function.
    Process is very simple - it truncate all, what it can truncate ;)

    @param disk     Disk number
    @param atend    Add/remove pos (0 - on start, 1 - at the end)
    @param add      Add/remove action (1/0)
    @param size     Size change.
    @return 0 on success, or E_PTE_* error. */
extern "C" qserr _std dsk_extresize(u32t disk, int atend, int add, u32t size) {
   FUNC_LOCK            lk;
   hdd_info            *hi = get_by_disk(disk);
   disk_geo_data       geo;
   u32t      ii, rc, heads, spt, lvmstate,
                cm_on_exit = 0,   // call ext_merge() on exit
             flush_on_exit = 0;   // write on exit all exts starting from this
   struct MBR_Record *erec;
   int              extidx;
   if (!hi) return E_DSK_DISKNUM;

   dsk_ptrescan(disk,0);
   if (hi->scan_rc) return hi->scan_rc;
   // no extended?
   if (!hi->extpos) return E_PTE_EXTERR;
   log_it(2, "extresize on %02X, %c %08X, %c\n", disk, add?'+':'-',
      size, atend?'e':'s');
   // full match?
   if (!size) return 0;
   if (!hi->extpos) return E_PTE_EXTERR;
   // merge free slices
   rc = dsk_extmerge(disk);
   if (rc) return rc;
   // get geometry for CHS calc
   if (dsk_getptgeo(disk,&geo)==0) {
      heads = geo.Heads;
      spt   = geo.SectOnTrack;
   } else {
      heads = 255;
      spt   = 63;
   }
   // index of extended in main MBR
   extidx = dsk_findextrec(hi,0);
   if (extidx<0) return E_PTE_EXTERR;
   // first size check
   if (!add && hi->extlen < size) return E_PTE_NOFREE;
   // one empty slice? just delete extended!
   if (!add && hi->pt_size==8) {
      if (!dsk_isquadempty(hi,1,0)) return E_PTE_EXTERR;
      return dsk_extdelete_int(hi);
   }
   // is LVM present?
   lvmstate = lvm_checkinfo(disk);
   erec     = hi->pts + extidx;
   // add / remove
   if (!add) {
      int   eepos = dsk_findextrec(hi,1);
      u32t  wquad = 0;
      // we delete single empty slice in code above ...
      if (eepos<0) return E_PTE_EXTERR;
      if (atend) {
         struct MBR_Record *trec;
         int                extp;

         wquad = (hi->pt_size>>2) - 1;
         // the end of extended must be a free slice
         if (wquad<=1 || !dsk_isquadempty(hi,wquad,0)) return E_PTE_NOFREE;
         // with appropriate size
         extp = dsk_findextrec(hi, wquad-1);
         if (extp<0) return E_PTE_EXTERR;
         trec = hi->pts + (wquad-1<<2) + extp;
         if (trec->PTE_LBASize < size) return E_PTE_NOFREE;
         // delete entire slice
         size = trec->PTE_LBASize;
         // empty parent record and flush it
         memset(trec, 0, sizeof(struct MBR_Record));
         rc = dsk_flushquad(disk, wquad-1);
         if (rc) return rc;
      } else {
         // multiple slices in partition, first is free
         struct MBR_Record *nrec = hi->pts + 4 + eepos;
         // check size/free slice presence
         if (!dsk_isquadempty(hi,1,1)) return E_PTE_NOFREE;
         if (nrec->PTE_LBAStart - hi->extpos < size) return E_PTE_NOFREE;
         // update size
         size  = nrec->PTE_LBAStart - hi->extpos;
         wquad = 1;
      }
      if (wquad) {
         // optionally wipe asked quad
         lvm_wipedlat(disk, wquad);
         dsk_wipequad(disk, wquad, 1);
      }
      // change partition size
      erec->PTE_LBASize -= size;
      if (atend)
         lba2chs(heads, spt, erec->PTE_LBAStart+erec->PTE_LBASize-1,
            &erec->PTE_HEnd, &erec->PTE_CSEnd);
      else {
         erec->PTE_LBAStart += size;
         lba2chs(heads, spt, erec->PTE_LBAStart, &erec->PTE_HStart,
            &erec->PTE_CSStart);
         // update other extended`s slices
         flush_on_exit = 2;
      }
   } else { // add space
      int is_empty = dsk_isquadempty(hi,1,0);
      /* if extended partition is empty we`re just add "size" to it, if not,
         but we`re adding to the end of partition - we doing the same and call
         dsk_extmerge() to let them add empty quad for the added space */
      if (is_empty || atend) {
         erec->PTE_LBASize += size;
         if (atend)
            lba2chs(heads, spt, erec->PTE_LBAStart+erec->PTE_LBASize-1,
               &erec->PTE_HEnd, &erec->PTE_CSEnd);
         else {
            erec->PTE_LBAStart -= size;
            lba2chs(heads, spt, erec->PTE_LBAStart, &erec->PTE_HStart,
               &erec->PTE_CSStart);
            // write new 1st quad
            rc = dsk_emptypt(disk, erec->PTE_LBAStart, 0);
            if (rc) return rc;
            // and delete old
            lvm_wipedlat(disk, 1);
            dsk_wipequad(disk, 1, 1);
         }
      } else { // add space to the start of extended
         int eepos = dsk_findextrec(hi,1);
         struct MBR_Record npt[4], *next;
         u32t   nextpos;

         if (size<spt*2) return E_PTE_EXTERR;
         memset(&npt, 0, sizeof(npt));
         // calc the end of current slice
         if (eepos<0) nextpos = hi->extpos + hi->extlen; else {
            struct MBR_Record *next = hi->pts + 4 + eepos;
            nextpos = next->PTE_LBAStart;
         }
         /* creating entry with disk offsets - dsk_emptypt() do not fix LBA
            offsets while writing as dsk_flushquad() do */
         npt[0].PTE_Type     = EXT_TYPE;
         npt[0].PTE_LBAStart = size;
         npt[0].PTE_LBASize  = nextpos - erec->PTE_LBAStart;
         npt[0].PTE_Active   = 0;
         lba2chs(heads, spt, erec->PTE_LBAStart, &npt[0].PTE_HStart, &npt[0].PTE_CSStart);
         lba2chs(heads, spt, nextpos-1, &npt[0].PTE_HEnd, &npt[0].PTE_CSEnd);
         // modify extended`s start and size
         erec->PTE_LBAStart -= size;
         erec->PTE_LBASize  += size;
         lba2chs(heads, spt, erec->PTE_LBAStart, &erec->PTE_HStart,
            &erec->PTE_CSStart);
         // write new 1st quad
         rc = dsk_emptypt(disk, erec->PTE_LBAStart, npt);
         if (rc) return rc;
         // zero sectors until the end of "track"
         dsk_emptysector(disk, erec->PTE_LBAStart+1, hi->lvm_spt-1);
         // update all extended`s slices
         flush_on_exit = 1;
      }
      /* call to dsk_extmerge: it will merge two free slices at the start and
         create empty slice at the end */
      if (!is_empty) cm_on_exit = 1;
   }
   /* update all extended slices to new partition start, but
      ignore all errors here to update MBR too */
   if (flush_on_exit) {
      hi->extpos = erec->PTE_LBAStart;
      for (ii=flush_on_exit; ii<hi->pt_size>>2; ii++)
         if ((rc=dsk_flushquad(disk,ii))!=0)
            log_it(2, "dsk_flushquad(%u) err %d ignored!\n", ii, rc);
   }
   // flush MBR with new extended pos/size
   rc = dsk_flushquad(disk, 0);
   if (rc) return rc;
   // LVM will force rescan or we do it manually ;)
   if (lvmstate!=E_LVM_NOINFO) lvm_initdisk(disk,0,-1); else
      dsk_ptrescan(disk, 1);
   // return rescan error code
   if (hi->scan_rc || !cm_on_exit) return hi->scan_rc;
   // merge was asked above
   return dsk_extmerge(disk);
}

/** should be called inside mutex lock!
    Function UNLOCKs it while shows the message! */
static int dsk_extwarning(u32t disk) {
   // little warning ;) only for logical & and REAL disks
   if (ExtMngWarnFlag && (hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)==0) {
      scan_unlock();
      int rc = confirm_dlg(ExtMngWarning);
      scan_lock();
      if (!rc) return 0;
      ExtMngWarnFlag = 0;
   }
   return 1;
}

qserr dsk_ptdel(u32t disk, u32t index, u64t start) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t   ii, rc, flags = 0, lvmstate;
   if (!hi) return E_DSK_DISKNUM;
   // GPT disk processing
   if (hi->gpt_present) return dsk_gptdel(disk, index, start);
   // is it primary?
   dsk_ptquery(disk, index, 0, 0, 0, &flags, 0);
   /* warn about extended partition management!
      dsk_extwarning() release lock while shows the message! */
   if ((flags&DPTF_PRIMARY)==0)
      if (!dsk_extwarning(disk)) return E_SYS_UBREAK;
   // unmount volumes
   for (ii=2; ii<DISK_COUNT; ii++) {
      disk_volume_data vi;
      hlp_volinfo(ii,&vi);
      if (vi.StartSector==(u64t)start && vi.Disk==disk) hlp_unmountvol(ii);
   }
   lvmstate = lvm_checkinfo(disk);

   log_it(2, "delete on %02X, index %u, start %08X\n", disk, index, start);
   // delete record and update disk
   rc = pt_action(ACTION_DELETE, disk, index, start);
   if (rc) return rc;
   // refresh LVM if available
   if (lvmstate!=E_LVM_NOINFO) lvm_initdisk(disk,0,-1);
   // all is ok if this was a primary
   if (flags&DPTF_PRIMARY) return 0;
   // else merge free slices in extended
   rc = dsk_extmerge(disk);
   if (rc) return rc;
   /* we have empty first slice or entire extended partition after delete?
      then feed it to dsk_extresize() - it can shrink it or delete at all.
      Size arg it not a reason here, because it just delete entire slice */
   if (dsk_isquadempty(hi,1,1)) rc = dsk_extresize(disk, 0, 0, 1);
   return rc;
}

/** exclude space from free logical slice.
    Function exclude space from the end of extended slice and creates a
    new empty slice for it.
    Function does not align anything to cylinder, but fails if difference with
    current size is smaller than cylinder size.

    @param hi       disk info
    @param quad     quad to split
    @param size     new size
    @param heads    assumed number of CHS heads
    @return 0 on success, or E_PTE_* error. */
static u32t dsk_shrinkslice(hdd_info *hi, u32t quad, u32t size, u32t heads) {
   int       lrec, erec;
   u32t  cylsize, csize, npsize, rc = 0, lvmstate, npos;

   if (!hi || !size || !quad || quad >= hi->pt_size>>2) return E_SYS_INVPARM;
   lrec = dsk_findlogrec(hi, quad);
   erec = dsk_findextrec(hi, quad);
   // last slice?
   if (erec<0 && quad<(hi->pt_size>>2)-1) return E_PTE_EXTERR;

   log_it(2, "shrink %d to %08X\n", quad, size);

   if (lrec>=0) {
      struct MBR_Record *rec = hi->pts + lrec + quad * 4;
      u32t             lsize = rec->PTE_LBAStart - hi->ptspos[quad] + rec->PTE_LBASize;

      if (lsize > size) {
         log_it(2, "shrink to %08X asked, but logical size is %08X (quad %d)\n",
            size, lsize, quad);
         return E_PTE_EXTERR;
      }
   }
   // cylinder size
   cylsize = heads*hi->lvm_spt;
   // current slice size
   csize   = ((quad==(hi->pt_size>>2)-1 ? hi->extpos+hi->extlen : hi->ptspos[quad+1])) -
             hi->ptspos[quad];
   // check size change
   if (csize <= size) {
      log_it(2, "shrink args: size %08X, asked %08X\n", csize, size);
      return E_SYS_INVPARM;
   }
   // size of splitted part
   if ((npsize = csize - size) < cylsize) {
      log_it(2, "shrink < cylinder size (%dx%d)\n", heads, hi->lvm_spt);
      return E_PTE_EXTERR;
   }
   lvmstate = lvm_checkinfo(hi->disk);
   npos     = hi->ptspos[quad]+size;
   // new quad for freed part
   if (erec>=0) {
      struct MBR_Record npt[4];
      memset(&npt, 0, sizeof(npt));
      memcpy(&npt[0], hi->pts + erec + quad * 4, sizeof(struct MBR_Record));
      // fix offset for dsk_flushquad()
      npt[0].PTE_LBAStart -= hi->extpos;
      // write new quad
      rc = dsk_emptypt(hi->disk, npos, npt);
   } else
      rc = dsk_emptypt(hi->disk, npos, 0);
   // zero sectors until the end of "track"
   dsk_emptysector(hi->disk, npos+1, hi->lvm_spt-1);

   if (rc) return rc; else {
      struct MBR_Record *rec;
      // adjust ext size in previous quad
      if (quad>1) {
         int prev_erec = dsk_findextrec(hi, quad-1);
         if (prev_erec<0) return E_PTE_EXTERR;

         rec = hi->pts + prev_erec + (quad-1) * 4;
         rec->PTE_LBASize = size;
      }
      if (erec<0) erec = lrec?0:1;
      rec = hi->pts + erec + quad * 4;
      // record for splitted part
      rec->PTE_Type     = EXT_TYPE;
      rec->PTE_LBAStart = hi->ptspos[quad] + size;
      rec->PTE_LBASize  = npsize;
      rec->PTE_Active   = 0;
      lba2chs(heads, hi->lvm_spt, rec->PTE_LBAStart, &rec->PTE_HStart, &rec->PTE_CSStart);
      lba2chs(heads, hi->lvm_spt, rec->PTE_LBAStart+npsize-1, &rec->PTE_HEnd, &rec->PTE_CSEnd);
      // flush previous and current ext pt records (quads)
      if (quad>1) rc = dsk_flushquad(hi->disk, quad-1);
      if (!rc) rc = dsk_flushquad(hi->disk, quad);
      // LVM will force rescan or we made it self ;)
      if (lvmstate!=E_LVM_NOINFO) lvm_initdisk(hi->disk,0,-1);
         else dsk_ptrescan(hi->disk, 1);
      // return rescan error code
      return hi->scan_rc;
   }
}

// logical
static u32t dsk_lptcreate(u32t disk, u32t start, u32t size, u8t type, u32t heads) {
   hdd_info *hi = get_by_disk(disk);
   u32t      rc = 0, ii, tgtquad = 0, cylsize, cm_on_exit = 0, lvmstate;
   if (!hi) return E_DSK_DISKNUM;
   // check it again ;)
   if (!type || !size || !start) return E_SYS_INVPARM;
   if (!hi->extpos || start<hi->extpos || start+size>hi->extpos+hi->extlen)
      return E_PTE_NOFREE;
   cylsize = heads * hi->lvm_spt;
   log_it(2, "dsk_lptcreate(%08X,%08X,%02X,%d)\n", start, size, type, heads);
   // searching for suitable present quad to place us
   for (ii=1; ii<hi->pt_size>>2; ii++)
      if (hi->ptspos[ii]==start || start-hi->ptspos[ii]<=hi->lvm_spt)
         if (!dsk_isquadempty(hi,tgtquad = ii,1)) return E_PTE_EXTERR;
            else break;
   lvmstate = lvm_checkinfo(disk);

   log_it(2, "target quad=%u\n", tgtquad);

   if (!tgtquad) {
      int  is_first, is_last;
      u32t   pos, next, diff;

      for (ii=1; ii<hi->pt_size>>2; ii++)
         if (hi->ptspos[ii]>start) break;
      is_last  = ii--==hi->pt_size>>2;
      is_first = !ii;

      pos  = hi->ptspos[ii];
      next = is_last ? hi->extpos+hi->extlen : hi->ptspos[ii+1];
      diff = next - pos;

      log_it(2, "quad=%u, pos %08X, next %08X (%08X), start %08X\n", ii,
         pos, next, diff, start);

      if (diff < size) return E_PTE_NOFREE;
      // too close to this quad - switch to easy processing
      if (dsk_isquadempty(hi,ii,1) && start - pos < cylsize) {
         size += start-pos; start = pos;
         tgtquad = ii;
      } else { /* else shrink slice and switch to easy processing too :)
         quad can be USED partially - by other partition */
         rc = dsk_shrinkslice(hi, ii, start-pos, heads);
         if (rc) return rc;
         // recurse because data was changed
         return dsk_lptcreate(disk, start, size, type, heads);
      }
   }
   if (tgtquad) {
      int is_last = tgtquad==(hi->pt_size>>2)-1;
      u32t   next = is_last ? hi->extpos+hi->extlen : hi->ptspos[tgtquad+1],
             diff = next - hi->ptspos[tgtquad];
      log_it(2, "diff=%08X, size %08X, cylsize %d\n", diff, size, cylsize);

      if (diff < size) return E_PTE_NOFREE;
      if (diff-size >= cylsize) {
         rc = dsk_shrinkslice(hi, tgtquad, size, heads);
         if (rc) return rc;
         // recurse because data was changed
         return dsk_lptcreate(disk, start, size, type, heads);
      } else {
        int    epos = dsk_findextrec(hi,tgtquad);
        struct MBR_Record *rec = hi->pts + (tgtquad<<2);
        // if extrec on 1st pos in quad then move it to second
        if (epos==0) memcpy(rec+1, rec, sizeof(struct MBR_Record));
        // fill pt entry
        rec->PTE_Type     = type;
        rec->PTE_LBAStart = hi->ptspos[tgtquad] + hi->lvm_spt;
        // del quad sectors from size if it cyl-aligned or no space for it
        if (size%cylsize==0 || rec->PTE_LBAStart+size>next) size-=hi->lvm_spt;
        rec->PTE_LBASize  = size;
        rec->PTE_Active   = 0;
        lba2chs(heads, hi->lvm_spt, rec->PTE_LBAStart, &rec->PTE_HStart, &rec->PTE_CSStart);
        lba2chs(heads, hi->lvm_spt, rec->PTE_LBAStart+size-1, &rec->PTE_HEnd, &rec->PTE_CSEnd);
        rc = dsk_flushquad(disk,tgtquad);
        if (rc) return rc;
        // wipe 55AA in boot sector
        dsk_wipevbs(disk, rec->PTE_LBAStart);
      }
   }
   // LVM will force rescan or we made it self ;)
   if (lvmstate!=E_LVM_NOINFO) lvm_initdisk(disk,0,-1);
      else dsk_ptrescan(disk, 1);
   // return rescan error code
   if (hi->scan_rc || !cm_on_exit) return hi->scan_rc;
   // merge was asked above
   return dsk_extmerge(disk);
}


qserr dsk_ptcreate(u32t disk, u32t start, u32t size, u32t flags, u8t type) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t       rc;
   if (!hi) return E_DSK_DISKNUM;
   // call dsk_gptcreate() for GPT disks
   if (hi->gpt_present) return E_PTE_GPTDISK;
   // minimal check
   if ((flags&DFBA_LOGICAL)!=0 && IS_EXTENDED(type)) return E_SYS_INVPARM;
   if (!type || !start) return E_SYS_INVPARM;
   if (size && size<hi->lvm_spt) return E_SYS_INVPARM;
   if ((flags&(DFBA_LOGICAL|DFBA_PRIMARY))==0 ||
      (flags&(DFBA_LOGICAL|DFBA_PRIMARY))==(DFBA_LOGICAL|DFBA_PRIMARY))
         return E_SYS_INVPARM;
   // rescan disk
   if ((flags&DFBA_NRESCAN)==0) {
      dsk_ptrescan(disk,1);
      /* little warning ;) only for logical & and REAL disks.
         dsk_extwarning() release lock while shows the message! */
      if ((flags&DFBA_LOGICAL)!=0)
         if (!dsk_extwarning(disk)) return E_SYS_UBREAK;
      // normalizing it on first entry, else we can get wrong free space index
      if ((flags&DFBA_LOGICAL)!=0) dsk_extmerge(disk);
   }
   if (hi->scan_rc) return hi->scan_rc;

   rc = E_PTE_NOFREE;
   while (hi->fsp_size) {
      dsk_freeblock *fb;
      u32t ii, extfin;
      enum { PtPrim,      // free block for primary partition
             PtBoundL,    // free block left-bounded to the extended partition
             PrCrossL,    // free block cross left border of extended partition
             PtBoundInL,  // free block on the left border of extended partition
             PtIn,        // free block in the deep of extended partition
             PtInclude,   // free block covers extended partition (and space around it)
             PtBoundInR,  // free block on the right border of extended partition
             PrCrossR,    // free block cross right border of extended partition
             PtBoundR }   // free block right-bounded to the extended partition
             bt;
      // search for free record
      for (ii=0; ii<hi->fsp_size; ii++) {
         fb = hi->fsp + ii;
         if (fb->StartSector<=start && fb->StartSector+fb->Length>start &&
            (!size || start+size<=fb->StartSector+fb->Length)) break;
         fb = 0;
      }
      if (!fb) break;
      if (!size) size = fb->StartSector+fb->Length-start;
      if (size<hi->lvm_spt*2) { rc = E_PTE_SMALL; break; }
      // next common error type
      rc = E_PTE_EXTERR;
      // end of extended partition
      extfin = hi->extpos+hi->extlen;
      // location type
      if (start<=hi->extpos && start+size>=extfin) {
         if (!hi->extstart) bt = PtInclude; else break;
      } else
      if (start<hi->extpos && start+size>hi->extpos) {
         if (start+size<=hi->extstart) bt = PrCrossL; else break;
      } else
      if (start<extfin && start+size>extfin) {
         if (start>=hi->extend) bt = PrCrossR; else break;
      } else
      if (start+size<=hi->extpos && fb->StartSector+fb->Length>=hi->extpos) bt=PtBoundL;
         else
      if (start>=extfin && fb->StartSector<=extfin) bt=PtBoundR;
         else
      if (start+size<=hi->extpos || start>=extfin) bt = PtPrim;
         else
      if (start>=hi->extpos && (start+size<=hi->extstart || !hi->extstart &&
         start+size<=extfin)) bt = PtBoundInL; else
      if (start>hi->extstart && start+size<hi->extend) bt=PtIn;
         else
      if (hi->extend && start<extfin && start>=hi->extend) bt=PtBoundInR;
         else {
         log_it(2, "loc.detect error: start %08X end %08X, ext: [%08X..%08X||%08X..%08X]\n",
            start, start+size-1, hi->extpos, hi->extstart, (u32t)hi->extend, extfin);
         break;
      }
      log_it(2, "create on %02X, start %08X end %08X, bound %d\n", disk, start,
         start+size-1, (u32t)bt);

      if (flags&DFBA_PRIMARY) {
         if (bt==PtIn) break; else
         if (bt==PtInclude) {
            int ext = dsk_findextrec(hi,0);
            if (ext<0) return E_PTE_EXTERR;
            rc = pt_action(ACTION_DELETE|ACTION_NORESCAN|ACTION_DIRECT, disk, ext,
               hi->pts[ext].PTE_LBAStart);
            if (rc) break;
            return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
         } else
         if (bt==PtBoundInL || bt==PrCrossL) {
            // include extended pt header to partition
            if (start == hi->extpos+fb->SectPerTrack) {
               start = hi->extpos;
               size += fb->SectPerTrack;
            }
            // overflow can occur here for PtBoundInL, but it valid
            rc = dsk_extresize(disk, 0, 0, size-(hi->extpos-start));
            if (rc) break;
            // recursive call because of changed structs and disk data
            return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
         } else
         if (bt==PtBoundInR || bt==PrCrossR) {
            // include extended pt header to partition
            if (memchrd(hi->ptspos+1, start-fb->SectPerTrack, (hi->pt_size>>2)-1)) {
               start-= fb->SectPerTrack;
               size += fb->SectPerTrack;
            }
            // the same as above, in PtBoundInL
            rc = dsk_extresize(disk, 1, 0, size-((start+size)-extfin));
            if (rc) break;
            // recursive call because of changed structs and disk data
            return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
         }
         // search for free record
         for (ii=0; ii<4; ii++)
            if (hi->pts[ii].PTE_Type==0) break;
         if (ii==4) rc = E_PTE_NOPRFREE; else {
            struct MBR_Record *rec = hi->pts+ii;
            rec->PTE_Type     = type;
            rec->PTE_LBAStart = start;
            rec->PTE_LBASize  = size;
            rec->PTE_Active   = 0;

            lba2chs(fb->Heads, fb->SectPerTrack, start, &rec->PTE_HStart,
               &rec->PTE_CSStart);
            lba2chs(fb->Heads, fb->SectPerTrack, start+size-1, &rec->PTE_HEnd,
               &rec->PTE_CSEnd);

            rc = dsk_flushquad(disk, 0);
            // remove old 55AA from boot sector
            if (!IS_EXTENDED(type)) dsk_wipevbs(disk, start);
            // rescan to update actual state
            dsk_ptrescan(disk, 1);
         }
      } else {
         if (bt==PtPrim) {
            // extended present?
            if (dsk_findextrec(hi,0)>=0) break;
            // no, need to create it ;)
            rc = dsk_emptypt(disk, start, 0);
            if (rc) break;
            rc = dsk_ptcreate(disk, start, size, DFBA_PRIMARY|DFBA_NRESCAN, PTE_0F_EXTENDED);
            if (rc) break;
            // recursive call because of changed structs and disk data
            return dsk_ptcreate(disk, start+hi->lvm_spt, size-hi->lvm_spt,
               flags|DFBA_NRESCAN, type);
         } else
         if (bt==PtInclude) {
            if (start<hi->extpos) {
               rc = dsk_extresize(disk, 0, 1, hi->extpos-start);
               if (rc) break;
               return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
            }
            if (start+size>extfin) {
               rc = dsk_extresize(disk, 1, 1, (start+size)-extfin);
               if (rc) break;
               return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
            }
            // recursive call because of changed structs and disk data
            return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
         } else
         if (bt==PtBoundL||bt==PtBoundR||bt==PrCrossL||bt==PrCrossR) {
            int  endpos = bt==PtBoundR||bt==PrCrossR;
            u32t   diff = endpos ? (start+size)-extfin : hi->extpos-start;

            log_it(2, "diff = %08X end = %i\n", diff, endpos);

            // resize extended partition to include required free space into it
            rc = dsk_extresize(disk, endpos, 1, diff);
            if (rc) break;
            // recursive call because of changed structs and disk data
            return dsk_ptcreate(disk, start, size, flags|DFBA_NRESCAN, type);
         }
         // normalize it before processing
         rc = dsk_extmerge(disk);
         // dsk_lptcreate() will update LVM internally
         if (!rc) return dsk_lptcreate(disk, start, size, type, fb->Heads);
      }
      if (!rc && hi->lvm_snum) {
         u32t lvmres = lvm_initdisk(disk,0,-1);
         if (lvmres) log_it(2, "lvm update result: %d\n", lvmres);
      }
      break;
   }
   return rc;
}
