//
// QSINIT
// GPT partition management
//
#include "stdlib.h"
#include "qsdm.h"
#include "pscan.h"
#include "qsint.h"
#include "wchar.h"

#define GPT_MINSIZE  (16384/GPT_RECSIZE)

static qserr dsk_flushgpt(hdd_info *hi);

qserr _std dsk_gptinit(u32t disk) {
   FUNC_LOCK         lk;
   hdd_info         *hi = get_by_disk(disk);
   u32t           stype, ii;
   u8t       mbr_buffer[MAX_SECTOR_SIZE];
   disk_geo_data   vgeo;
   struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;

   if (!hi) return E_DSK_DISKNUM;
   dsk_ptrescan(disk,1);
   stype = dsk_sectortype(disk, 0, mbr_buffer);
   if (stype==DSKST_ERROR) return E_DSK_ERRREAD;

   log_it(2, "dsk_gptinit(%X)\n", disk);

   if (stype!=DSKST_EMPTY && stype!=DSKST_DATA) {
      if (hi->pt_size>4) return E_PTE_EMPTY;
      if (hi->pt_size)
         for (ii=0; ii<4; ii++)
            if (hi->pts[ii].PTE_Type) return E_PTE_EMPTY;
   }
   // clear pt entries in case of garbage data
   if (!dsk_newmbr(disk,DSKBR_GPTHEAD|DSKBR_GENDISKID|DSKBR_LVMINFO|DSKBR_GPTCODE))
      return E_DSK_ERRWRITE;
   if (hi->ghead)  { free(hi->ghead); hi->ghead=0; }
   if (hi->ghead2) { free(hi->ghead2); hi->ghead2=0; }
   if (hi->ptg)    { free(hi->ptg); hi->ptg=0; }

   hi->non_lvm     = 1;
   hi->gpt_present = 1;
   hi->hybrid      = 0;

   hi->gpthead     = 1;
   hi->gpt_size    = GPT_MINSIZE;
   hi->gpt_sectors = hi->gpt_size * GPT_RECSIZE / hi->info.SectorSize;
   // minimum data to perform dsk_flushgpt()
   hi->ghead  = (struct Disk_GPT *)mem_allocz(memOwner, memPool, MAX_SECTOR_SIZE);
   hi->ptg    = (struct GPT_Record*)mem_allocz(memOwner, memPool,
      sizeof(struct GPT_Record) * hi->gpt_size);
   if (hi->ghead) {
      struct Disk_GPT *pt = hi->ghead;

      pt->GPT_Sign      = GPT_SIGNMAIN;
      pt->GPT_Revision  = 0x10000;
      pt->GPT_HdrSize   = sizeof(struct GPT_Record);
      pt->GPT_Hdr1Pos   = hi->gpthead;
      pt->GPT_Hdr2Pos   = hi->info.TotalSectors - 1;
      pt->GPT_UserFirst = pt->GPT_Hdr1Pos + hi->gpt_sectors + 1;
      pt->GPT_UserLast  = pt->GPT_Hdr2Pos - hi->gpt_sectors - 1;

      pt->GPT_PtInfo    = pt->GPT_Hdr1Pos + 1;
      pt->GPT_PtCout    = hi->gpt_size;
      pt->GPT_PtEntrySize = GPT_RECSIZE;

      dsk_makeguid(pt->GPT_GUID);
   }
   // flush empty GPT data
   ii = dsk_flushgpt(hi);
   if (ii) return ii;
   // rescan to update free space list and actual state
   dsk_ptrescan(hi->disk, 1);

   return 0;
}

static qserr dsk_flushgpt(hdd_info *hi) {
   struct Disk_GPT *pt = hi->ghead,
                  *pt2 = hi->ghead2;

   if (!hi || !hi->gpthead || !hi->ptg || !pt) return E_SYS_INVPARM;
   // update partition data crc32
   pt ->GPT_PtCRC = crc32(crc32(0,0,0), (u8t*)hi->ptg, sizeof(struct GPT_Record) *
      pt->GPT_PtCout);
   // there is no second copy? reconstruct one
   if (!pt2) {
      // check location
      if (!pt->GPT_Hdr2Pos || pt->GPT_Hdr2Pos==FFFF64)
         pt->GPT_Hdr2Pos = hi->info.TotalSectors - 1;
      if (pt->GPT_Hdr2Pos - hi->gpt_sectors <= pt->GPT_UserLast)
         return E_PTE_GPTHDR;
      // makes a copy
      hi->ghead2      = pt2 = (struct Disk_GPT *)mem_dup(hi->ghead);
      pt2->GPT_PtInfo = pt->GPT_Hdr2Pos - hi->gpt_sectors;
   }
   // update crc32 in headers
   pt->GPT_HdrCRC  = 0;
   pt->GPT_HdrCRC  = crc32(crc32(0,0,0), (u8t*)pt, sizeof(struct Disk_GPT));
   pt2->GPT_HdrCRC = 0;
   pt2->GPT_HdrCRC = crc32(crc32(0,0,0), (u8t*)pt2, sizeof(struct Disk_GPT));

   // write second copy of partition data
   if (hlp_diskwrite(hi->disk, pt2->GPT_PtInfo, hi->gpt_sectors, hi->ptg) !=
      hi->gpt_sectors) return E_DSK_ERRWRITE;
   // write second header
   if (!hlp_diskwrite(hi->disk, pt->GPT_Hdr2Pos, 1, pt2)) return E_DSK_ERRWRITE;
   // write first copy of partition data
   if (hlp_diskwrite(hi->disk, pt->GPT_PtInfo, hi->gpt_sectors, hi->ptg) !=
      hi->gpt_sectors) return E_DSK_ERRWRITE;
   // write first header
   if (!hlp_diskwrite(hi->disk, pt->GPT_Hdr1Pos, 1, pt)) return E_DSK_ERRWRITE;

   return 0;
}

/// flush changes and rescan
static u32t update_rescan(hdd_info *hi) {
   u32t rc = dsk_flushgpt(hi);
   if (rc) log_it(2, "Warning! GPT flush error %d\n", rc);
   dsk_ptrescan(hi->disk, 1);
   return hi->scan_rc;
}

qserr dsk_gptdel(u32t disk, u32t index, u64t start) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t       ii;
   if (!hi) return E_DSK_DISKNUM;
   if (hi->scan_rc) return hi->scan_rc;
   if (!hi->gpt_present) return E_SYS_INVPARM;
   /* actually - we can delete pure GPT partitions from hybrid disk, but
      better to not touch it at all */
   if (hi->hybrid) return E_PTE_HYBRID;
   // unmount volumes
   for (ii=2; ii<DISK_COUNT; ii++) {
      disk_volume_data vi;
      hlp_volinfo(ii,&vi);
      if (vi.StartSector==(u64t)start && vi.Disk==disk) hlp_unmountvol(ii);
   }
   log_it(2, "delete on %02X, index %u, start %09LX\n", disk, index, start);

   if (hi->gpt_view<=index) return E_PTE_PINDEX; else {
      u32t *pos = memchrd(hi->gpt_index, index, hi->gpt_size);

      if (!pos) return E_PTE_PINDEX; else {
         u32t          rc, pidx = pos - hi->gpt_index;
         struct GPT_Record *pte = hi->ptg + pidx;
         // check partition
         if (pte->PTG_FirstSec != start) return E_PTE_RESCAN;
         // remove this record
         if (pidx < hi->gpt_size-1)
            memmove(pte, pte+1, sizeof(struct GPT_Record) * (hi->gpt_size-pidx-1));
         memset(hi->ptg + hi->gpt_size - 1, 0, sizeof(struct GPT_Record));
         // flush changes and rescan
         return update_rescan(hi);
      }
   }
}

static int __stdcall ptrec_compare(const void *b1, const void *b2) {
   struct GPT_Record *rp1 = (struct GPT_Record*)b1,
                     *rp2 = (struct GPT_Record*)b2;
   // sort records by pos, but put zeroed - to the end
   if (rp1->PTG_FirstSec == rp2->PTG_FirstSec) return 0;
   if (!rp1->PTG_FirstSec && rp2->PTG_FirstSec) return 1;
   if (rp2->PTG_FirstSec && rp1->PTG_FirstSec > rp2->PTG_FirstSec) return 1;
   return -1;
}

/** create GPT partition.
    Use dsk_ptcreate() for MBR disks, this function will return E_PTE_NOFREE
    if disk is MBR and E_PTE_HYBRID if partition table is hybrid.

    @param disk     Disk number.
    @param start    Start sector of new partition
    @param size     Size of new partition in sectors, can be 0 for full free
                    block size.
    @param flags    Flags for creation (only DFBA_PRIMARY accepted).
    @param guid     Partition type GUID.
    @return 0 on success, or E_PTE_* error. */
qserr dsk_gptcreate(u32t disk, u64t start, u64t size, u32t flags, void *guid) {
   FUNC_LOCK      lk;
   dsk_freeblock *fb;
   hdd_info      *hi = get_by_disk(disk);
   u32t     ii, widx;
   if (!hi) return E_DSK_DISKNUM;
   if (hi->scan_rc) return hi->scan_rc;
   if (!hi->gpt_present) return E_PTE_NOFREE;
   if (hi->hybrid) return E_PTE_HYBRID;
   // invalid flags
   if (flags&~DFBA_PRIMARY) return E_SYS_INVPARM;

   // search for free record
   for (ii=0; ii<hi->fsp_size; ii++) {
      fb = hi->fsp + ii;
      if (fb->StartSector<=start && fb->StartSector+fb->Length>start &&
         (!size || start+size<=fb->StartSector+fb->Length)) break;
      fb = 0;
   }
   if (!fb) return E_PTE_NOFREE;
   // size auto-subst
   if (!size) size = fb->StartSector + fb->Length - start;

   widx = FFFF;
   /* sort and compress GPT partition list (not required, I think, but who
      knows */
   for (ii=0; ii<hi->gpt_size; ii++)
      if (!hi->ptg[ii].PTG_FirstSec)
         { hi->ptg[widx = ii].PTG_FirstSec = start; break; }
   if (widx < hi->gpt_size) {
      // sort used blocks list
      qsort(hi->ptg, hi->gpt_size, sizeof(struct GPT_Record), ptrec_compare);
      // searching for our`s target location after sorting
      for (widx=0; widx<hi->gpt_size; widx++)
         if (hi->ptg[widx].PTG_FirstSec==start) break;
   }

   if (widx < hi->gpt_size) {
      // make happy this imbecile whar_t implementaion in OW 1.9 (requires type cast)
      static u16t *str_unnamed = (u16t*)L"Custom (QSINIT)";
      struct GPT_Record    *ne = hi->ptg + widx;
      static u32t  windataguid[4] = {0xEBD0A0A2, 0x4433B9E5, 0xB668C087, 0xC79926B7};
      char             guidstr[40], *ptstr;

      memset(ne, 0, sizeof(struct GPT_Record));
      // query printable partition name (and use "Custom" if failed)
      if (!guid) guid = windataguid;
      dsk_guidtostr(guid, guidstr);
      ptstr = guidstr[0] ? cmd_shellgetmsg(guidstr) : 0;
      if (ptstr) {
         // ugly conversion to unicode
         for (ii=0; ii<35; ii++)
            if (!ptstr[ii]) break; else ne->PTG_Name[ii] = ptstr[ii];
         free(ptstr);
      } else {
         wcsncpy(ne->PTG_Name, str_unnamed, 35);
         ne->PTG_Name[35] = 0;
      }
      memcpy(ne->PTG_TypeGUID, guid, 16);
      dsk_makeguid(ne->PTG_GUID);

      log_it(2, "create on %02X, start %09LX end %09LX, type \"%S\" (%s)\n",
         disk, start, start+size-1, ne->PTG_Name, guidstr);

      ne->PTG_FirstSec = start;
      ne->PTG_LastSec  = start + size - 1;
      ne->PTG_Attrs    = 0;

      // flush changes and rescan
      return update_rescan(hi);
   } else
      return E_PTE_NOPRFREE;

   return E_PTE_NOFREE;
}

qserr _std dsk_gptpset(u32t disk, u32t index, dsk_gptpartinfo *pinfo) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);

   /* we ALLOW hybrid changing in this function!!!
      This is only GPT record update, so, I hope - it cannot kill ;) */

   if (!hi) return E_DSK_DISKNUM; else
   if (!pinfo) return E_SYS_INVPARM; else {
      struct GPT_Record  *pte;
      u32t   *pos;
      int    type = dsk_isgpt(disk, index), emptyname;
      if (type<=0) return E_PTE_MBRDISK;

      pos = memchrd(hi->gpt_index, index, hi->gpt_size);
      if (!pos) return E_PTE_PINDEX;
      pte = hi->ptg + (pos - hi->gpt_index);

      if (pte->PTG_FirstSec!=pinfo->StartSector ||
         pinfo->Length != pte->PTG_LastSec - pte->PTG_FirstSec + 1)
            return E_PTE_RESCAN;
      /* linux, as always, made an additional trouble - empty partition name,
         so need to check it, else pair getinfo()->setinfo() will fail on
         the next statement */
      emptyname = memchrnw(pte->PTG_Name, 0, 36) ? 0 : 1;

      // can`t find anything except zeroes in one of required fields
      if (!memchrnb(pinfo->TypeGUID, 0, 16) || !memchrnb(pinfo->GUID, 0, 16)
         || !emptyname && !memchrnw(pinfo->Name, 0, 36)) return E_SYS_INVPARM;

      memcpy(pte->PTG_TypeGUID, pinfo->TypeGUID, 16);
      memcpy(pte->PTG_GUID, pinfo->GUID, 16);
      memcpy(pte->PTG_Name, pinfo->Name, sizeof(pinfo->Name));
      pte->PTG_Attrs = pinfo->Attr;

      // flush changes and rescan
      return update_rescan(hi);
   }
}

qserr _std dsk_gptfind(void *guid, u32t guidtype, u32t *disk, u32t *index) {
   if (!disk||!guid||(guidtype&~(GPTN_PARTITION|GPTN_PARTTYPE)))
      return E_SYS_INVPARM;
   // empty GUID arg
   if (!memchrnb((u8t*)guid,0,16)) return E_SYS_INVPARM;
   if (guidtype!=GPTN_DISK && !index) return E_SYS_INVPARM;

   FUNC_LOCK  lk;

   if (guidtype==GPTN_DISK || *disk==FFFF) {
      u32t hdds = hlp_diskcount(0), ii;

      for (ii=0; ii<hdds; ii++) {
         hdd_info *hi = get_by_disk(ii);
         if (!hi) continue;
         dsk_ptrescan(ii,0);
         if (!hi->gpt_size) continue;

         if (guidtype==GPTN_DISK) {
            if (memcmp(hi->ghead->GPT_GUID, guid, 16)==0) {
               *disk = ii;
               return 0;
            }
         } else
         if (!dsk_gptfind(guid, guidtype, &ii, index)) {
            *disk = ii;
            return 0;
         }
      }
      return E_PTE_PINDEX;
   } else {
      hdd_info *hi = get_by_disk(*disk);
      u32t     idx;
      if (!hi) return E_DSK_DISKNUM;
      *index = FFFF;
      dsk_ptrescan(*disk,0);
      if (!hi->gpt_size) return E_PTE_MBRDISK;

      for (idx=0; idx<hi->gpt_size; idx++)
         if (hi->ptg[idx].PTG_FirstSec) {
            struct GPT_Record  *pte = hi->ptg + idx;
            // if partition is not indexed, then skip it and search next
            if (memcmp(guidtype&GPTN_PARTITION ? pte->PTG_GUID : pte->PTG_TypeGUID,
               guid, 16)==0)
               if (hi->gpt_index[idx]!=FFFF) {
                  *index = hi->gpt_index[idx];
                  return 0;
               }
         }
      return E_PTE_PINDEX;
   }
}

/** update GPT disk info.
    Function set new GUID.
    Function allow to change UserFirst/UserLast positions (!), but only if all
    existing partitions still covered by new UserFirst..UserLast value
    (else E_PTE_NOFREE returned). Set both UserFirst/UserLast to 0 to skip this.
    Hdr1Pos and Hdr2Pos values ignored.

    @param  disk    Disk number
    @param  dinfo   Buffer with new disk info
    @return 0 on success or E_PTE_* constant (E_PTE_MBRDISK if disk is MBR). */
qserr _std dsk_gptdset(u32t disk, dsk_gptdiskinfo *dinfo) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   int      type;
   /* ALLOW hybrid changing in this function, but GUID only,
      not UserFirst/UserLast */
   if (!hi) return E_DSK_DISKNUM; else
   if (!dinfo) return E_SYS_INVPARM;
   // this call scan disk if required
   type = dsk_isgpt(disk, -1);
   if (!type) return E_PTE_MBRDISK;
   // can`t find anything except zeroes in guid
   if (!memchrnb(dinfo->GUID, 0, 16)) return E_SYS_INVPARM;
   // positions changed
   if ((dinfo->UserFirst || dinfo->UserLast) && (dinfo->UserFirst!=
      hi->ghead->GPT_UserFirst || dinfo->UserLast!=hi->ghead->GPT_UserLast))
   {
      dsk_mapblock *mb, *mp;
      if (hi->hybrid) return E_PTE_HYBRID;
      if (dinfo->UserFirst > dinfo->UserLast) return E_SYS_INVPARM;
      if (!hi->ghead2) return E_PTE_RESCAN;
      // unsupported order
      if (hi->ghead->GPT_Hdr1Pos > hi->ghead->GPT_Hdr2Pos) return E_PTE_GPTHDR;
      // start pos below 1st header
      if (dinfo->UserFirst <= hi->ghead->GPT_Hdr1Pos || dinfo->UserFirst <
         hi->ghead->GPT_PtInfo + hi->gpt_sectors) return E_PTE_NOFREE;
      // end pos above 2nd header
      if (dinfo->UserLast >= hi->ghead->GPT_Hdr2Pos || dinfo->UserLast >=
         hi->ghead2->GPT_PtInfo) return E_PTE_NOFREE;
      // check existing partitions
      mp = mb = dsk_getmap(disk);
      if (!mb) return E_PTE_NOFREE; else
      do {
         if (mp->Type && (mp->StartSector < dinfo->UserFirst ||
            mp->StartSector+mp->Length-1 > dinfo->UserLast))
               { free(mb); return E_PTE_NOFREE; }
      } while ((mp++->Flags&DMAP_LEND)==0);
      free(mb);
      // update headers
      hi->ghead ->GPT_UserFirst = dinfo->UserFirst;
      hi->ghead ->GPT_UserLast  = dinfo->UserLast;
      hi->ghead2->GPT_UserFirst = dinfo->UserFirst;
      hi->ghead2->GPT_UserLast  = dinfo->UserLast;
   }
   memcpy(hi->ghead->GPT_GUID, dinfo->GUID, 16);
   // flush changes and rescan
   return update_rescan(hi);
}

qserr _std dsk_gptactive(u32t disk, u32t index) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t  ii, changed;

   if (!hi) return E_DSK_DISKNUM;
   // function call rescan if required
   if (dsk_isgpt(disk,-1)<=0) return E_PTE_MBRDISK;
   // check index
   if (index>=hi->gpt_view) return E_PTE_PINDEX;
   if (!memchrd(hi->gpt_index, index, hi->gpt_size)) return E_PTE_PINDEX;

   for (ii=0, changed=0; ii<hi->gpt_size; ii++)
      if (hi->ptg[ii].PTG_FirstSec) {
         struct GPT_Record *pte = hi->ptg + ii;
         int             target = hi->gpt_index[ii]==index;
         // bit mismatch for this entry
         if (Xor(pte->PTG_Attrs & (1LL << GPTATTR_BIOSBOOT),target)) {
            changed++;
            if (target) pte->PTG_Attrs |= 1LL << GPTATTR_BIOSBOOT; else
               pte->PTG_Attrs &= ~(1LL << GPTATTR_BIOSBOOT);
         }
      }
   return changed ? update_rescan(hi) : 0;
}
