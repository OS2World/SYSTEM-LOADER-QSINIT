//
// QSINIT
// partition scan
//
#include "stdlib.h"
#include "qslog.h"
#include "qsdm.h"
#include "memmgr.h"
#include "pscan.h"
#include "qcl/qslist.h"
#include "qsint.h"
#include "qsinit.h"
#include "vioext.h"
#include "errno.h"
#include "qcl/sys/qsedinfo.h"
#include "seldesc.h"          // next 3 is for hook only
#include "qsmodext.h"
#include "qsinit_ord.h"

#define PRINT_ENTRIES

long memOwner=-1,
      memPool=-1;

void lvm_buildcrc(void);

typedef struct {
   u64t      start;
   u64t     length;
} dsk_pos;

// grow by 4 records
static void grow_mbr(hdd_info *di) {
   long sz = di->pt_size + 4,
       asz = sizeof(struct MBR_Record)*sz,
       isz = sizeof(u32t)*sz>>2,
      dlsz = sizeof(DLA_Table_Sector)*(sz>>2);
   // alloc with specified owner & pool to use batch free in scan_free()
   di->pts = (struct MBR_Record*)(sz ? realloc(di->pts, asz):
      mem_alloc(memOwner, memPool, asz));
   di->ptspos = (u32t*)(sz ? realloc(di->ptspos, isz):
      mem_alloc(memOwner, memPool, isz));
   di->dlat   = (DLA_Table_Sector*)(sz ? realloc(di->dlat, dlsz):
      mem_alloc(memOwner, memPool, dlsz));
}

static int __stdcall dsk_pos_compare(const void *b1, const void *b2) {
   register dsk_pos **dm1 = (dsk_pos**)b1,
                    **dm2 = (dsk_pos**)b2;
   // sort pointers in list actually, not data itself ;)
   if ((*dm1)->start > (*dm2)->start) return 1;
   if ((*dm1)->start == (*dm2)->start) return 0;
   return -1;
}

static dsk_pos *new_item(u64t start, u64t size) {
   dsk_pos *rc = (dsk_pos*)malloc(sizeof(dsk_pos));
   rc->start   = start;
   rc->length  = size;
   return rc;
}

static struct Disk_GPT *read_hdr2(u32t disk, u64t pos) {
   void *mem = malloc(MAX_SECTOR_SIZE);
   if (!hlp_diskread(disk, pos, 1, mem)) { free(mem); return 0; }
   return (struct Disk_GPT *)mem;
}

static u32t scan_gpt(hdd_info *drec) {
   struct Disk_GPT *pt;
   char        guidstr[40];
   u32t          stype, recinsec;
   // buffers for both headers
   drec->ghead  = (struct Disk_GPT *)mem_allocz(memOwner, memPool, MAX_SECTOR_SIZE);
   drec->ghead2 = (struct Disk_GPT *)mem_allocz(memOwner, memPool, MAX_SECTOR_SIZE);
   // read header #1
   pt    = drec->ghead;
   stype = dsk_sectortype(drec->disk, drec->gpthead, (u8t*)drec->ghead);
   if (stype != DSKST_GPTHEAD) return DPTE_GPTHDR;
   if (pt->GPT_Hdr1Pos != drec->gpthead) return DPTE_GPTHDR;
   // too large partition table size (who can made it?)
   if (pt->GPT_PtCout > _1MB) return DPTE_GPTLARGE;

   dsk_guidtostr(pt->GPT_GUID, guidstr);

   log_it(3, "GPT: h1=%LX,h2=%LX %LX..%LX pt=%LX,%u %s\n",
      pt->GPT_Hdr1Pos, pt->GPT_Hdr2Pos, pt->GPT_UserFirst, pt->GPT_UserLast,
         pt->GPT_PtInfo, pt->GPT_PtCout, guidstr);

   // number of sectors in partition data
   recinsec          = drec->info.SectorSize / GPT_RECSIZE;
   drec->gpt_size    = pt->GPT_PtCout;
   drec->gpt_sectors = drec->gpt_size / recinsec;
   if (drec->gpt_size % recinsec) {
      log_it(3, "misaligned GPT?\n");
      drec->gpt_sectors++;
   }

   // read header #2
   if (pt->GPT_Hdr2Pos && pt->GPT_Hdr2Pos!=FFFF64) {
      if (pt->GPT_Hdr2Pos >= drec->info.TotalSectors) return DPTE_GPTHDR;

      stype = dsk_sectortype(drec->disk, pt->GPT_Hdr2Pos, (u8t*)drec->ghead2);
      if (stype != DSKST_GPTHEAD) {
         log_it(0, "GPT header #2 error (%u)!\n", stype);
         // error on access above 2TB?
         if (pt->GPT_Hdr2Pos >= _4GBLL) return DPTE_2TBERR;
         /* no header #2 and no free space to write it with partition table
            data - so no GPT updates possible - exit */
         if (pt->GPT_Hdr2Pos - drec->gpt_sectors <= pt->GPT_UserLast)
             return DPTE_GPTHDR;
         // flag error in header #2
         free(drec->ghead2); drec->ghead2 = 0;
      } else
         if (drec->ghead2->GPT_PtInfo <= pt->GPT_UserLast) return DPTE_GPTHDR2;
   }
   // allocate and read partition data
   drec->ptg = (struct GPT_Record*)mem_alloc(memOwner, memPool,
      sizeof(struct GPT_Record) * pt->GPT_PtCout);
   
   if (!drec->ptg) return DPTE_GPTLARGE; else {
      u32t   crc;
      // unable to read first header, use second
      if (hlp_diskread(drec->disk, pt->GPT_PtInfo, drec->gpt_sectors,
         drec->ptg) != drec->gpt_sectors)
      {
         log_it(0, "GPT records read error!\n");
         mem_zero(drec->ptg);
         // header #2 available?
         if (!drec->ghead2) return DPTE_ERRREAD;
         // unable to read partition records?
         if (hlp_diskread(drec->disk, drec->ghead2->GPT_PtInfo, drec->gpt_sectors,
            drec->ptg) != drec->gpt_sectors) return DPTE_ERRREAD;
      }
      // check crc32, but ignore it
      crc = crc32(0,0,0); 
      crc = crc32(crc, (u8t*)drec->ptg, sizeof(struct GPT_Record) * pt->GPT_PtCout);

      if (pt->GPT_PtCRC != crc)
         log_it(2, "GPT records CRC mismatch: %08X instead of %08X!\n", crc,
            pt->GPT_PtCRC);
      // viewable index for GPTs 
      drec->gpt_index = (u32t*)mem_alloc(memOwner, memPool, drec->gpt_size*4);
      memset(drec->gpt_index, 0xFF, drec->gpt_size*4);
   }
   return 0;      
}

static u32t scan_disk(hdd_info *drec) {
   u32t sector = 0, chssec = 0, errcnt, chsread = 0, ii, rc = 0, lowestLBA = 0;

   if (drec->pts) { free(drec->pts); drec->pts=0; }
   if (drec->ptg) { free(drec->ptg); drec->ptg=0; }
   if (drec->fsp) { free(drec->fsp); drec->fsp=0; }
   if (drec->dlat) { free(drec->dlat); drec->dlat=0; }
   if (drec->index) { free(drec->index); drec->index=0; }
   if (drec->ghead) { free(drec->ghead); drec->ghead=0; }
   if (drec->ghead2) { free(drec->ghead2); drec->ghead2=0; }
   if (drec->ptspos) { free(drec->ptspos); drec->ptspos=0; }
   if (drec->gpt_index) { free(drec->gpt_index); drec->gpt_index=0; }
   drec->pt_size     = 0;
   drec->pt_view     = 0;
   drec->extpos      = 0;
   drec->extlen      = 0;
   drec->lba_error   = 0;
   drec->lvm_snum    = 0;
   drec->lvm_bsnum   = 0;
   drec->fsp_size    = 0;
   drec->extstart    = 0;
   drec->extend      = 0;
   drec->gpt_present = 0;
   drec->hybrid      = 0;
   drec->non_lvm     = 0;
   drec->gpthead     = 0;
   drec->gpt_size    = 0;
   drec->gpt_sectors = 0;
   drec->gpt_view    = 0;
   drec->usedfirst   = FFFF64;
   drec->usedlast    = FFFF64;

   // assume <=63 sectors, but LVM can grow it to 255 and write bad CHSs in PT
   drec->lvm_spt   = drec->info.SectOnTrack;

   do {
      u8t pt_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR *pt = (struct Disk_MBR *)&pt_buffer;
      u32t stype = dsk_sectortype(drec->disk, chsread?chssec:sector, pt_buffer);

      if (stype==DSKST_ERROR) rc = DPTE_ERRREAD; else
      if (stype==DSKST_BOOTFAT || stype==DSKST_BOOTBPB || stype==DSKST_BOOTEXF ||
         stype==DSKST_BOOT) rc = DPTE_FLOPPY; else
      if (stype==DSKST_EMPTY || stype==DSKST_DATA) rc = DPTE_EMPTY;

      if (sector==0 && rc) return rc;
      errcnt = 0;

      if (stype==DSKST_PTABLE) {
         u64t disksize = drec->info.TotalSectors>(u64t)FFFF?_4GBLL:drec->info.TotalSectors;
         u32t   offset = chsread?chssec:sector, extnext, extpart;

         grow_mbr(drec);
         // zero DLAT info buffer for current table
         memset(drec->dlat + (drec->pt_size>>2), 0, sizeof(DLA_Table_Sector));
         // current 4 entries
         memcpy(drec->pts+drec->pt_size, &pt->MBR_Table, sizeof(struct MBR_Record)*4);
         // and physical sector with it
         drec->ptspos[drec->pt_size>>2] = offset;

         for (ii=0, extnext=0, extpart=0; ii<4; ii++) {
            struct MBR_Record *rec = drec->pts+drec->pt_size+ii;
            u8t   pt = rec->PTE_Type;
            u32t pst = rec->PTE_LBAStart,
               isext = IS_EXTENDED(pt);
            // patch to absolute sector number
            pst += drec->extpos && isext ? drec->extpos : offset;

            if (drec->pt_size)
               if (isext) extnext++; else
                  if (pt) extpart++;
#ifdef PRINT_ENTRIES
            if (pt) log_it(3, "%2d. %02X %02X %08X->%08X..%08X  %08X of %08LX\n",
               drec->pt_size+ii, pt, rec->PTE_Active, rec->PTE_LBAStart, pst,
                  pst+rec->PTE_LBASize-1, rec->PTE_LBASize, disksize);
#endif
            if (rec->PTE_LBAStart) rec->PTE_LBAStart = pst;
            // check extended partition
            if (extnext>1 || extpart>1) errcnt++;
            // and check it
            if (rec->PTE_Active<0x80 && rec->PTE_Active!=0) errcnt++;
            if (rec->PTE_Type) {
               if (!rec->PTE_LBAStart || !rec->PTE_LBASize ||
                  FFFF - rec->PTE_LBAStart + 1 < rec->PTE_LBASize ||
                     rec->PTE_LBAStart+rec->PTE_LBASize > disksize ||
                       disksize < drec->info.SectorSize) errcnt++;
               // check 1st MBR
               if (!drec->pt_size) {
                  // GPT partition?
                  if (rec->PTE_Type==PTE_EE_UEFI || rec->PTE_Type==PTE_EF_UEFI) {
                     if (!drec->gpthead || rec->PTE_LBAStart<drec->gpthead)
                        drec->gpthead = rec->PTE_LBAStart;
                     drec->gpt_present++;
                  }
                  // too low partition?
                  if (rec->PTE_LBAStart<drec->lvm_spt) drec->non_lvm = 1;
                  // remember lowest LBA value for FLAT searching
                  if (lowestLBA && rec->PTE_LBAStart<lowestLBA || !lowestLBA)
                     lowestLBA = rec->PTE_LBAStart;
               }
            }
         }
         /* searching for DLAT in "Sectors_Per_Track - 1" and in sector 126/254
            (for 127/255 sector disks, made by danis + LVM combination).
            In last case ALL CHS values in partition table is broken and
            most of boot managers go crazy on boot */
         for (ii=0; ii<(offset?1:2); ii++) {
            u32t ofs, crcorg, crc;

            // normal geometry on first pass and ugly on second
            if (!ii) ofs = drec->lvm_spt; else
               ofs = disksize < 127*255*65535?127:255;
            // too low partition
            if (lowestLBA && ofs>lowestLBA) {
               // disk is unsuitable for LVM only if partition`s LBA < 63.
               if (!ii) drec->non_lvm = 1;
               break;
            }

            if (hlp_diskread(drec->disk, offset + ofs - 1, 1, pt_buffer)) {
               DLA_Table_Sector *dlat = (DLA_Table_Sector *)&pt_buffer;
               if (dlat->DLA_Signature1 == DLA_TABLE_SIGNATURE1 &&
                   dlat->DLA_Signature2 == DLA_TABLE_SIGNATURE2)
               {
                  if (!drec->lvm_bsnum) {
                     drec->lvm_bsnum = dlat->Boot_Disk_Serial;
                     drec->lvm_snum  = dlat->Disk_Serial;
                  } else
                  if (drec->lvm_snum != dlat->Disk_Serial) {
                     log_it(2, "LVM`s disk serial mismatch: %08X != %08X (sector %08X)\n",
                        drec->lvm_snum, dlat->Disk_Serial, offset + ofs - 1);
                     break;
                  }
                  if (ii)
                     if (ofs!=dlat->Sectors_Per_Track) {
                        log_it(2, "1st DLAT in %d, but report %d spt\n", ofs-1,
                           dlat->Sectors_Per_Track);
                        break;
                     }
                  // copy LVM info
                  memcpy(drec->dlat + (drec->pt_size>>2), dlat, sizeof(DLA_Table_Sector));

                  crcorg = dlat->DLA_CRC;
                  dlat->DLA_CRC = 0;
                  crc    = lvm_crc32(LVM_INITCRC, pt_buffer, drec->info.SectorSize);
                  if (crc != crcorg)
                     log_it(3, "DLAT CRC mismatch: %08X instead of %08X\n", crc, crcorg);

                  if (ii) {
                     drec->lvm_spt = ofs;
                     log_it(3, "%d sectors geometry detected\n", ofs);
                  }
                  break;
               }
            }
         }
         // DLAT was found
         if (drec->dlat[drec->pt_size>>2].DLA_Signature1) {
            for (ii=0; ii<4; ii++) {
               DLA_Entry *de = &drec->dlat[drec->pt_size>>2].DLA_Array[ii];
#ifdef PRINT_ENTRIES
               if (de->Partition_Size)
                  log_it(3, "DLAT: %2d. %-20s %08X %08X bm:%s vol:%c\n",
                     drec->pt_size+ii, de->Partition_Name, de->Partition_Start,
                        de->Partition_Size, de->On_Boot_Manager_Menu?"on ":"off",
                           !de->Drive_Letter?'-':de->Drive_Letter);
#endif
            }
         }

      }
      if (stype!=DSKST_PTABLE || errcnt) {
         log_it(3, "Invalid PT: disk %X sector %08X\n", drec->disk,
            chsread?chssec:sector);
         // drop error and try it with CHS
         if (sector && sector!=chssec && !chsread && sector<255*63*1024) {
            log_it(3, "Trying CHS %08X, instead of LBA %08X\n", chssec, sector);
            chsread = 1;
            rc = 0;
            continue;
         } else {
            if (!rc) rc = DPTE_INVALID;
         }
      } else {
         u32t ext1 = 0;
         if (chsread) drec->lba_error++;

         sector = FFFF;

         for (ii=0; ii<4; ii++) {
            struct MBR_Record *rec = drec->pts+drec->pt_size+ii;
            u32t pt = rec->PTE_Type;

            if (IS_EXTENDED(pt)) {
               if (ext1) {
                  log_it(3, "Two extended PTs: disk %X sector %08X: %02X,%02X\n",
                     drec->disk, chsread?chssec:sector, ext1, pt);
                  if (pt==PTE_15_EXTENDED || pt==PTE_1F_EXTENDED || pt==PTE_91_EXTENDED
                     || pt==PTE_9B_EXTENDED) continue;
               }
               ext1 = pt;
               sector = rec->PTE_LBAStart;
               if (rec->PTE_CSStart==0xFFFF || rec->PTE_HStart==0 && rec->PTE_CSStart==0)
                  chssec = sector;
               else {
                  u32t cyl = rec->PTE_CSStart>>8 | rec->PTE_CSStart<<2 & 0x300;
                  chssec = cyl * drec->info.Heads * drec->info.SectOnTrack +
                     rec->PTE_HStart * drec->info.SectOnTrack + (rec->PTE_CSStart&0x3F)-1;
               }
               // start/size of selected extended partition
               if (!drec->pt_size) {
                  drec->extpos = rec->PTE_LBAStart;
                  drec->extlen = rec->PTE_LBASize;
               }
            }
         }
         drec->pt_size+=4;
      }
      chsread = 0;
   } while (!rc && sector!=FFFF);

   if (!rc && drec->gpt_present) rc = scan_gpt(drec);

   if (drec->pt_size) {
      ptr_list ptl = NEW(ptr_list);
      u32t     idx = 0,
           extlerr = 0, // error in length of extended partition
          primfree = 0, // at least one free primary entry is exists
           maxhead = 0;
      drec->index  = (u32t*)mem_alloc(memOwner,memPool,drec->pt_size*4);
      memset(drec->index, 0xFF, drec->pt_size*4);
      // build viewable pt index and full list of block to calc free space
      for (ii=0; ii<drec->pt_size; ii++) {
         u8t pt = drec->pts[ii].PTE_Type;
         if (pt) {
            u32t pt_start = drec->pts[ii].PTE_LBAStart,
                   pt_len = drec->pts[ii].PTE_LBASize;
            u64t  vlstart = pt_start,
                    vllen = 0;
            // non-GPT partition found, flag hybrid type
            if (drec->gpt_present && drec->pts[ii].PTE_Type!=PTE_EE_UEFI && 
               drec->pts[ii].PTE_Type!=PTE_EF_UEFI) drec->hybrid = 1;
            // trying to search for max. head value
            if (drec->pts[ii].PTE_HEnd > maxhead) maxhead = drec->pts[ii].PTE_HEnd;
            /* get any of secondary or hidden extended partitions as one big partition
               (and hope what length is valid) */
            if (IS_EXTENDED(pt) && (ii>=4 || drec->extpos==pt_start)) {
               /* include extended header into used space only if it belongs to
                  existing partition */
               if (!dsk_isquadempty(drec,(ii>>2)+1,1)) vllen += drec->lvm_spt;
            } else 
            if (!IS_GPTPART(pt)) {
               drec->index[ii] = idx++;
               vllen += pt_len;
               // check logical partitions for extended partition length
               if (ii>=4) {
                  if (pt_start < drec->extpos || (u64t)pt_start + pt_len >
                     (u64t)drec->extpos + drec->extlen)
                  {
                     extlerr = 1;
                     log_it(2, "Warning! Logical %d overlap extended borders\n", idx-1);
                  } else {
                     // search for start/end of USED space in extended partition
                     if (!drec->extstart || drec->extstart>pt_start)
                        drec->extstart = pt_start;
                     if (!drec->extend || (u64t)pt_start + pt_len>drec->extend)
                        drec->extend = (u64t)pt_start + pt_len;
                  }
               }
               // set index for hybrid partition
               if (drec->hybrid && drec->gpt_size) {
                  u32t  hidx;
                  for (hidx=0; hidx<drec->gpt_size; hidx++)
                     if (drec->ptg[hidx].PTG_FirstSec == pt_start) {
                        drec->gpt_index[hidx] = drec->index[ii];
                        break;
                     }
               }
            }
            // store start pos & size in single qword
            ptl->add(new_item(vlstart, vllen));
            // update first/last used sectors
            if (vllen) {
               if (vlstart+vllen>drec->usedlast || drec->usedlast==FFFF64)
                  drec->usedlast = vlstart+vllen-1;
               if (vlstart<drec->usedfirst) drec->usedfirst = vlstart;
            }
         } else
         if (ii<4) primfree = 1;
      }
      drec->pt_view = idx;
      // adjust known start by one track for extended partition entry
      if (drec->extstart) drec->extstart-=drec->lvm_spt;

      // build viewable index for GPT
      if (drec->gpt_size) {
         for (ii=0; ii<drec->gpt_size; ii++) {
            struct GPT_Record  *pte = drec->ptg + ii;
            // index for hybrid partition can be assigned above
            if (drec->gpt_index[ii]==FFFF && pte->PTG_FirstSec &&
               pte->PTG_LastSec > pte->PTG_FirstSec) drec->gpt_index[ii] = idx++;
            if (pte->PTG_FirstSec) {
#ifdef PRINT_ENTRIES
               char guidstr[40];
               dsk_guidtostr(pte->PTG_TypeGUID, guidstr);
               log_it(3, "%2d. %09LX..%09LX %s \"%S\" %LX\n", ii,
                  pte->PTG_FirstSec, pte->PTG_LastSec, guidstr, pte->PTG_Name,
                     pte->PTG_Attrs);
#endif
               // update first/last used sectors
               if (pte->PTG_LastSec>drec->usedlast || drec->usedlast==FFFF64)
                  drec->usedlast = pte->PTG_LastSec;
               if (pte->PTG_FirstSec<drec->usedfirst)
                  drec->usedfirst = pte->PTG_FirstSec;
            }
         }
      }
      // total viewable count
      drec->gpt_view = idx;
      if (drec->usedfirst!=FFFF64)
         log_it(3, "used start %09LX end %09LX\n", drec->usedfirst, drec->usedlast+1);

      if (extlerr && ptl->count()) {
         log_it(2, "Ignore free space because of extended`s size error\n");
      } else 
      if (drec->gpt_size && drec->hybrid) {
         log_it(2, "Ignore free space because of Hybrid GPT\n");
      } else {
         /* build free block list
            goes here in any case - to catch empty disk with single 
            free space entry */
         ptr_list fbl = NEW(ptr_list);
         u64t  lpnext = 0,
             userlast = 0;
         // free block list on GPT
         if (drec->gpt_size) {
            // free MBR data and replace it to list of GPT partitions
            ptl->freeitems(0, ptl->max());
            ptl->clear();
            for (ii=0; ii<drec->gpt_size; ii++) {
              struct GPT_Record  *pte = drec->ptg + ii;
              if (pte->PTG_FirstSec && pte->PTG_LastSec > pte->PTG_FirstSec)
                 ptl->add(new_item(pte->PTG_FirstSec, pte->PTG_LastSec-pte->PTG_FirstSec+1));
            }
            lpnext   = drec->ghead->GPT_UserFirst;
            userlast = drec->ghead->GPT_UserLast;
         } else { 
            log_it(3, "ext start %08X end %08LX\n", drec->extstart, drec->extend);

            lpnext   = drec->non_lvm ? 1 : drec->lvm_spt;
            userlast = drec->info.TotalSectors > _4GBLL ? FFFF : drec->info.TotalSectors-1;
         }
         // sort used blocks list
         qsort(ptl->array(), ptl->count(), sizeof(pvoid), dsk_pos_compare);

         for (ii=0; ii<ptl->count(); ii++) {
            dsk_pos *plv = (dsk_pos*)ptl->value(ii);
            u64t   start = plv->start,
                    size = plv->length;
            if (start<lpnext) {
               log_it(2, "Warning! Partition start at %09LX, but prev ends at %09LX\n",
                  start, lpnext);
            } else
            if (start - lpnext > drec->lvm_spt)
               fbl->add(new_item(lpnext, start - lpnext));
            lpnext = start + size;
         }
         // free block at the end of disk space
         if (userlast > lpnext && userlast-lpnext > drec->lvm_spt)
            fbl->add(new_item(lpnext, userlast - lpnext + 1));
        
         // free blocks founded?
         if ((drec->fsp_size=fbl->count())!=0) {
            u32t   heads = 0,
                sectin4k = 4096 / drec->info.SectorSize;
            // ugly way to get number of heads
            if (drec->lvm_snum) heads = drec->dlat[0].Heads_Per_Cylinder;
            if (!heads) {
               if (drec->info.TotalSectors < 1024*255*63) {
                  heads = drec->info.Heads;
                  if (maxhead>=heads) heads = maxhead+1;
               } else 
                  heads = 255;
            }
            // allocate free space array
            drec->fsp = (dsk_freeblock*)mem_allocz(memOwner, memPool,
               sizeof(dsk_freeblock) * drec->fsp_size);
            // and fill it
            for (ii=0; ii<drec->fsp_size; ii++) {
               dsk_freeblock *fbi = drec->fsp + ii;
               dsk_pos       *plv = (dsk_pos*)fbl->value(ii);

               fbi->Disk          = drec->disk;
               fbi->BlockIndex    = ii;
               fbi->StartSector   = plv->start;
               fbi->Length        = plv->length;
               fbi->Heads         = heads;
               fbi->SectPerTrack  = drec->lvm_spt;
#if 0
               log_it(3, "fb Disk %03X idx %i start %09LX len %09LX heads %d spt %d\n",
                  fbi->Disk, fbi->BlockIndex, fbi->StartSector, fbi->Length,
                     fbi->Heads, fbi->SectPerTrack);
#endif
               if (drec->gpt_size) {
                  // how easy it is ;)
                  fbi->Flags|=DFBA_PRIMARY;
               } else {
                  u64t extpe = (u64t)drec->extpos + drec->extlen;
                  // primary: if free space is outside of used extended space
                  if (primfree)
                     if (!drec->extstart || fbi->StartSector<drec->extstart ||
                        fbi->StartSector >= drec->extend) fbi->Flags|=DFBA_PRIMARY;
                  /* logical: if no extended at all or free space is inside
                     of extended or bounded to its border */
                  if (!drec->extpos && primfree || fbi->StartSector>=drec->extpos
                     && fbi->StartSector<extpe || fbi->StartSector==extpe ||
                       fbi->StartSector+fbi->Length==drec->extpos)
                          fbi->Flags|=DFBA_LOGICAL;
                  // CHS is good for logical partition
                  if (fbi->Flags&DFBA_LOGICAL)
                     if ((fbi->StartSector-fbi->SectPerTrack) %
                        (fbi->Heads*fbi->SectPerTrack) == 0)
                           fbi->Flags|=DFBA_CHSL;
                  // CHS is good for primary partition
                  if (fbi->Flags&DFBA_PRIMARY)
                     if (fbi->StartSector==fbi->SectPerTrack ||
                        fbi->StartSector % (fbi->Heads*fbi->SectPerTrack) == 0)
                           fbi->Flags|=DFBA_CHSP;
                  // CHS aligned end pos?
                  if ((fbi->StartSector+fbi->Length) % (fbi->Heads*fbi->SectPerTrack) == 0)
                     fbi->Flags|=DFBA_CHSEND;
               }
               if ((fbi->StartSector & sectin4k-1)==0) fbi->Flags|=DFBA_AF;

               log_it(3, "free %d: %09LX %09LX [%c%c%c%c%c]\n", ii, plv->start,
                  plv->length, fbi->Flags&DFBA_PRIMARY?'P':'-',
                     fbi->Flags&DFBA_CHSP?'+':'-', fbi->Flags&DFBA_LOGICAL?'L':'-',
                        fbi->Flags&DFBA_CHSL?'+':'-',fbi->Flags&DFBA_CHSEND?'E':'-');
            }
         }
         fbl->freeitems(0, fbl->max());
         DELETE(fbl);
      }
      ptl->freeitems(0, ptl->max());
      DELETE(ptl);
   }

   if (drec->lba_error)
      log_it(3, "%d pt lba pos errors on disk %X\n", drec->lba_error, drec->disk);
   return rc;
}

static hdd_info *hddi = 0;
static u32t      hddc = 0,
                 hddf = 0;
static int    hook_on = 0;    ///< hlp_diskremove hook installed?

void scan_init(void) {
   u32t ii;

   if (memOwner==-1) {
      mem_uniqueid(&memOwner,&memPool);
      hddc = hlp_diskcount(&hddf);
      // actually we support only 14 HDDs + 2 FDDs ;)
      if (hddc>MAX_QS_DISK) hddc = MAX_QS_DISK;

      if (hddc+hddf) {
         /* make constant ptrs to disk info (else some of dsk_ptrescan() calls
            will hang, i think :) */
         hddi = (hdd_info*)mem_allocz(memOwner, memPool, sizeof(hdd_info) * 
            (MAX_QS_DISK + hddf));
         for (ii=0; ii<hddf; ii++) {
            hdd_info *iptr = get_by_disk(QDSK_FLOPPY|ii);
            if (iptr) iptr->disk = QDSK_FLOPPY|ii;
         }
      }
   } else {
      // change number of hard disks (to support ram disk hot plug)
      u32t nhdd = hlp_diskcount(0);

      if (nhdd==hddc) return;
      hddc = nhdd;
   }
   if (hddc)
      for (ii=0; ii<hddc; ii++) {
         hdd_info *iptr = get_by_disk(ii);
         if (iptr) iptr->disk = ii;
      }
}

void scan_free(void) {
   if (memOwner!=-1) {
      mem_freepool(memOwner, memPool);
      hddi = 0; hddc = 0; hddf = 0;
   }
}

hdd_info *get_by_disk(u32t disk) {
   scan_init();
   if (!hddi) return 0;
   if (disk&QDSK_VOLUME) return 0;
   if ((disk&QDSK_DISKMASK)==disk) {
      if (hddc<=disk) return 0;
      return hddi + disk;
   }
   if (disk&QDSK_FLOPPY) {
      u32t idx = disk&QDSK_DISKMASK;
      if (idx>=hddf) return 0;
      return hddi + MAX_QS_DISK + idx;
   }
   return 0;
}

u32t _std dsk_ptrescan(u32t disk, int force) {
   hdd_info *hi;
   scan_init();
   hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (!hlp_disksize(disk,0,&hi->info)) return DPTE_ERRREAD;
#if 0
   log_it(3, "disk %X %d %LX %d x %d x %d\n", disk, hi->info.SectorSize,
      hi->info.TotalSectors, hi->info.Cylinders, hi->info.Heads,
         hi->info.SectOnTrack);
#endif
   // rescan on force, first time or floppy disk
   if (!hi->inited) hi->inited = 1; else
   if (!force && (disk&QDSK_FLOPPY)==0) return hi->scan_rc;

   return hi->scan_rc = scan_disk(hi);
}

/// entry hook for hlp_diskremove()
static int _std catch_diskremove(mod_chaininfo *info) {
   u32t disk = *(u32t*)(info->mc_regs->pa_esp+4);
#if 0
   log_it(2, "partmgr: hlp_diskremove(%02X)\n", disk);
#endif
   /* is it emulated? then drop known data for it!
      use direct structure addressing here to prevent any unwanted calls, 
      this is HOOK! */
   if ((hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)!=0)
      if (hddi && (disk&QDSK_DISKMASK)==disk && hddc>disk)
         hddi[disk].inited = 0;
   return 1;
}

/* catch hlp_diskremove() to drop all known disk info */
void set_hooks(void) {
   if (hook_on) return; else {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit)
         if (mod_apichain(qsinit, ORD_QSINIT_hlp_diskremove, APICN_ONENTRY, catch_diskremove))
            hook_on = 1; else log_it(2, "failed to catch!\n");
   }
}

void remove_hooks(void) {
   if (hook_on) {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit)
         mod_apiunchain(qsinit, ORD_QSINIT_hlp_diskremove, APICN_ONENTRY, catch_diskremove);
      hook_on = 0;
   }
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
u32t shl_dm_list(const char *cmd, str_list *args, u32t disk, u32t pos) {
   int  verbose = 0, force = 0;
   u32t      ii;
   if (args->count>=pos+1) {
      static char *argstr   = "force|/v";
      static short argval[] = {    1, 1};
      str_list *rcargs = str_parseargs(args, pos, 1, argstr, argval, &force, &verbose);
      ii = rcargs->count;
      free(rcargs);
      // we must get empty list, else unknown keys here
      if (ii) { cmd_shellerr(EMSG_CLIB, EINVAL, 0); return EINVAL; }
   }
   // do not init "pager" at nested calls
   if (cmd) cmd_printseq(0,1,0);

   if (disk==FFFF || disk==x7FFF) {
      int  pstate = vio_setansi(1);
      scan_init();
      if (!hddf) shellprn(" %d HDD%s in system\n",hddc,hddc<2?"":"s"); else
         shellprn(" %d HDD and %d floppy drives in system\n",hddc,hddf);

      for (ii=0; ii<hddf+hddc; ii++) {
         u32t dsk = ii<hddf?QDSK_FLOPPY|ii:ii-hddf;
         // hard disk if OFF, skip it
         if ((dsk&QDSK_FLOPPY)==0 && (hlp_diskmode(dsk,HDM_QUERY)&HDM_QUERY)==0)
            continue;

         if (disk==FFFF) { // dmgr list
            disk_geo_data  gdata;
            char    *stext, namebuf, sizebuf[32],
                  *dskname = dsk_disktostr(dsk,0);
            sizebuf[0] = 0;

            if (hlp_disksize(dsk,0,&gdata)) {
               stext = get_sizestr(gdata.SectorSize, gdata.TotalSectors);
               if (verbose) sprintf(sizebuf, "(%Lu sectors)", gdata.TotalSectors);
            } else {
               stext = "    no info";
            }
            if (shellprn(verbose?" %cDD %i =>%-4s: \x1B[1;37m%s\x1B[0m %s":
               " %cDD %i =>%-4s: %s", dsk&QDSK_FLOPPY?'F':'H', 
                  dsk&QDSK_DISKMASK, dskname, stext, sizebuf)) break;
            if (verbose) {
               disk_geo_data ldata;
               qs_extdisk     edsk;
               char        str[96];
               int           islvm = lvm_checkinfo(dsk)==0;
               if (islvm)
                  if (dsk_getptgeo(dsk, &ldata)) islvm = 0;
               sprintf(str, "         Sector %4d, CHS: %d x %d x %d",
                  gdata.SectorSize, gdata.Cylinders, gdata.Heads, 
                     gdata.SectOnTrack);
               if (islvm)
                  sprintf(str+strlen(str), ", LVM: %d x %d x %d",
                     ldata.Cylinders, ldata.Heads, ldata.SectOnTrack);
               if (shellprt(str)) break;
               edsk = hlp_diskclass(dsk, 0);
               if (edsk) {
                  char *einfo = edsk->getname();
                  if (einfo) {
                     int brk = shellprn("         %s",einfo);
                     free(einfo);
                     if (brk) break;
                  }
               }
            }
         } else {          // dmgr list all
            if (shl_dm_list(0,args,dsk,pos)==EINTR) break;
            shellprt("");
         }
      }
      vio_setansi(pstate);
   } else {
      char dname[10], lvmname[32];
      hdd_info *hi = get_by_disk(disk);
      disk_volume_data   vi[DISK_COUNT];
      lvm_disk_data    lvmi;
      char           *stext;

      if (!hi) { printf("There is no disk %X!\n", disk); return ENOMNT; }
      // rescan disk (forced or not)
      dsk_ptrescan(disk,force);
      // disk size string
      stext = dsk_formatsize(hi->info.SectorSize, hi->info.TotalSectors, 0, 0);
      // query lvm disk name
      lvmname[0]=0;
      if (lvm_diskinfo(disk,&lvmi)) {
         strcpy(lvmname,", LVM:<");
         strcat(lvmname,lvmi.Name);
         strcat(lvmname,">");
      }
      shellprc(VIO_COLOR_LWHITE, " %s %i: %s (%LX sectors), %d partition%s%s",
         disk&QDSK_FLOPPY?"Floppy disk":"HDD", disk&QDSK_DISKMASK, stext,
            hi->info.TotalSectors, hi->gpt_view, hi->gpt_view==1?"":"s",lvmname);

      dsk_disktostr(disk,dname);

      switch (hi->scan_rc) {
         case DPTE_ERRREAD:
         case DPTE_INVALID:
            shellprc(VIO_COLOR_LRED, hi->scan_rc==DPTE_ERRREAD?
               " Disk %s read error!":" Partition table on disk %s is invalid!",
                  dname);
            break;
         case DPTE_FLOPPY :
            shellprn(" Disk %s is floppy formatted!", dname);
            break;
         case DPTE_EMPTY  :
            shellprn(" Disk %s is empty!", dname);
            break;
         default :
            if (hi->scan_rc) common_errmsg("_DPTE%02d", "Scan error", hi->scan_rc);
            break;
      }
      if (hi->pt_view) {
         // query mounted volumes
         for (ii = 0; ii<DISK_COUNT; ii++) hlp_volinfo(ii, vi+ii);
         // inform about hybrid
         if (hi->gpt_present)
            if (cmd_printseq(" Hybrid partiton table (MBR + GPT)!", 0, VIO_COLOR_YELLOW))
               return EINTR;
         // print table
         if (shellprt(
            " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ \n"
            "  ## ³     type     ³ fs info ³   size   ³ mount ³   LBA    ³  LVM info       \n"
            " ÄÄÄÄÅÄÄÄÄÄÄÄÄÄÂÄÄÄÄÅÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄ "))
               return EINTR;
         for (ii = 0; ii<hi->pt_view; ii++) {
            char          pdesc[16], mounted[64], lvmpt[20];
            lvm_partition_data lvmd;
            u64t      psize, pstart;
            u32t          kk, flags;
            u8t               ptype = dsk_ptquery64(disk, ii, &pstart, &psize, pdesc, &flags);
            int               islvm = lvm_partinfo(disk, ii, &lvmd);
         
            mounted[0] = 0;
            for (kk = 0; kk<DISK_COUNT; kk++)
               if (vi[kk].StartSector && vi[kk].Disk==disk)
                  if (vi[kk].StartSector==pstart)
                     snprintf(mounted, 64, "%c:/%c:",'0'+kk,'A'+kk);
            if (islvm) {
               if (lvmd.Letter) snprintf(lvmpt,17,"%c: ³ %s", lvmd.Letter, lvmd.VolName);
                  else snprintf(lvmpt,17,"no ³ %s", lvmd.VolName);
            } else
               strcpy(lvmpt, "-- ³");
         
            if (shellprn("  %2d ³ %s ³ %02X ³ %-8s³%s ³ %s ³ %08LX ³ %s", ii,
               (flags&(DPTF_ACTIVE|DPTF_PRIMARY))==(DPTF_ACTIVE|DPTF_PRIMARY)?
                  "active ":(flags&DPTF_PRIMARY?"primary":"logical"), ptype,
                     pdesc, get_sizestr(hi->info.SectorSize, psize)+2,
                        *mounted?mounted:"     ", pstart, lvmpt)) return EINTR;
         }
      }
      if (hi->gpt_present) {
         // query mounted volumes if it was not done in 
         if (!hi->pt_view)
            for (ii = 0; ii<DISK_COUNT; ii++) hlp_volinfo(ii, vi+ii);
         else
            if (shellprt("")) return EINTR;

         if (shellprt(
            " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ \n"
            "  ## ³ fs info ³   size   ³ mount ³    LBA    ³ attr ³        type            \n"
            " ÄÄÄÄÅÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ "))
               return EINTR;

         for (ii = 0; ii<hi->gpt_view; ii++)
            // print both GPT and hybrid partitions here
            if (dsk_isgpt(disk, ii) > 0) {
               dsk_gptpartinfo  gpi;
               char       pdesc[16], mounted[64], guidstr[40], *ptstr,
                          eflags[8];
               u64t   psize, pstart;
               u32t              kk;

               static const s8t part_flags[] = { GPTATTR_SYSTEM, GPTATTR_IGNORE,
                  GPTATTR_BIOSBOOT, GPTATTR_MS_RO, GPTATTR_MS_HIDDEN, 
                     GPTATTR_MS_NOMNT, -1};
               static const char  *part_char = "SIBRHN";
               
               dsk_ptquery64(disk, ii, &pstart, &psize, pdesc, 0);
               // query printable partition type
               dsk_gptpinfo(disk, ii, &gpi);
               dsk_guidtostr(gpi.TypeGUID, guidstr);
               ptstr = guidstr[0] ? cmd_shellgetmsg(guidstr) : 0;

               mounted[0] = 0;
               for (kk = 0; kk<DISK_COUNT; kk++)
                  if (vi[kk].StartSector && vi[kk].Disk==disk)
                     if (vi[kk].StartSector==pstart)
                        snprintf(mounted, 64, "%c:/%c:",'0'+kk,'A'+kk);

               eflags[0] = 0;
               if (gpi.Attr) {
                  char *epf = eflags;
                  for (kk = 0; part_flags[kk]>=0; kk++)
                     if ((u64t)1<<part_flags[kk] & gpi.Attr) *epf++ = part_char[kk];
                  *epf = 0;
                  // align Attr string to center
                  kk = strlen(eflags);
                  if (kk) {
                     kk = 3 - (Round2(kk)>>1);
                     while (kk--) *epf++=' ';
                     *epf = 0;
                  }
               }
               if (shellprn("  %2d ³ %-8s³%s ³ %s ³%10.9LX ³%6s³ %s", ii, pdesc, 
                  get_sizestr(hi->info.SectorSize, psize)+2, 
                     *mounted?mounted:"     ", pstart, eflags, ptstr?ptstr:""))
                        return EINTR;
               if (ptstr) free(ptstr);
            }
      }
   }
   return 0;
}
