//
// QSINIT
// partition scan
//
#include "stdlib.h"
#include "qsdm.h"
#include "qsbase.h"
#include "memmgr.h"
#include "pscan.h"
#include "qcl/qslist.h"
#include "qsint.h"
#include "qsinit.h"
#include "vioext.h"
#include "errno.h"
#include "qcl/sys/qsedinfo.h"
#include "qcl/qsmt.h"
#include "seldesc.h"          // next 3 is for hook only
#include "qsmodext.h"
#include "qsinit_ord.h"

#define PRINT_ENTRIES

long memOwner=-1,
      memPool=-1;

typedef struct {
   u64t      start;
   u64t     length;
} dsk_pos;

u32t      os2guid[4] = {0x90B6FF38, 0x4358B98F, 0xF3481FA2, 0xD38A4A5B},
      windataguid[4] = {0xEBD0A0A2, 0x4433B9E5, 0xB668C087, 0xC79926B7},
      lindataguid[4] = {0x0FC63DAF, 0x47728483, 0x693D798E, 0xE47D47D8},
          efiguid[4] = {0xC12A7328, 0x11D2F81F, 0xA0004BBA, 0x3BC93EC9};

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
   u32t          stype, recinsec, crc;
   int        table_ok = 0;
   // buffers for both headers
   drec->ghead  = (struct Disk_GPT *)mem_allocz(memOwner, memPool, MAX_SECTOR_SIZE);
   drec->ghead2 = (struct Disk_GPT *)mem_allocz(memOwner, memPool, MAX_SECTOR_SIZE);
   // read header #1
   pt    = drec->ghead;
   stype = dsk_sectortype(drec->disk, drec->gpthead, (u8t*)drec->ghead);
   if (stype != DSKST_GPTHEAD) return E_PTE_GPTHDR;
   if (pt->GPT_Hdr1Pos != drec->gpthead) return E_PTE_GPTHDR;
   // too large partition table size (who can made it?)
   if (pt->GPT_PtCout > _1MB) return E_PTE_GPTLARGE;

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
   // viewable index for GPTs 
   drec->gpt_index = (u32t*)mem_alloc(memOwner, memPool, drec->gpt_size*4);
   memset(drec->gpt_index, 0xFF, drec->gpt_size*4);
   // allocate and read partition data
   drec->ptg = (struct GPT_Record*)mem_alloc(memOwner, memPool,
      sizeof(struct GPT_Record) * pt->GPT_PtCout);
   if (!drec->ptg) return E_PTE_GPTLARGE; else {
      mem_zero(drec->ptg);

      table_ok = hlp_diskread(drec->disk, pt->GPT_PtInfo, drec->gpt_sectors,
                              drec->ptg) == drec->gpt_sectors;
      if (!table_ok) {
         log_it(0, "GPT records read error!\n");
         mem_zero(drec->ptg);
      }
   }
   // read header #2
   if (pt->GPT_Hdr2Pos && pt->GPT_Hdr2Pos!=FFFF64) {
      if (pt->GPT_Hdr2Pos >= drec->info.TotalSectors) {
         // check that BIOS just cuts high dword of disk size, but i/o works well
         mt_swlock();
         if ((pt->GPT_Hdr2Pos+1&FFFF) == drec->info.TotalSectors) {
            if (!_setdisksize(drec->disk, pt->GPT_Hdr2Pos+1, drec->info.SectorSize)) {
               stype = dsk_sectortype(drec->disk, pt->GPT_Hdr2Pos, (u8t*)drec->ghead2);

               if (stype==DSKST_GPTHEAD && drec->ghead2->GPT_Hdr2Pos==pt->GPT_Hdr1Pos) {
                  // it works! ask scan_disk() to launch full scan again
                  mt_swunlock();
                  log_it(0, "!!disk size changed from %LX to %LX!!\n",
                     drec->info.TotalSectors, pt->GPT_Hdr2Pos+1);
                  return E_PTE_RESCAN;
               }
               // disk i/o is broken too - return size back and show error
               _setdisksize(drec->disk, drec->info.TotalSectors, drec->info.SectorSize);
            }
         }
         mt_swunlock();

         log_it(0, "GPT header #2 position error (%LX>%LX)!\n", pt->GPT_Hdr2Pos, drec->info.TotalSectors);
         return E_PTE_GPTHDR;
      }
      stype = dsk_sectortype(drec->disk, pt->GPT_Hdr2Pos, (u8t*)drec->ghead2);
      if (stype != DSKST_GPTHEAD) {
         log_it(0, "GPT header #2 error (%u)!\n", stype);
         // error on access above 2TB?
         if (pt->GPT_Hdr2Pos >= _4GBLL) return E_DSK_2TBERR;
         /* no header #2 and no free space to write it with partition table
            data - so no GPT updates possible - exit */
         if (pt->GPT_Hdr2Pos - drec->gpt_sectors <= pt->GPT_UserLast)
             return E_PTE_GPTHDR;
         // flag error in header #2
         free(drec->ghead2); drec->ghead2 = 0;
      } else
         if (drec->ghead2->GPT_PtInfo <= pt->GPT_UserLast) return E_PTE_GPTHDR2;
   }
   if (!table_ok) {
      // header #2 available?
      if (!drec->ghead2) return E_DSK_ERRREAD;
      // unable to read partition records?
      if (hlp_diskread(drec->disk, drec->ghead2->GPT_PtInfo, drec->gpt_sectors,
         drec->ptg) != drec->gpt_sectors)
      {   
         log_it(0, "Error readind table 2 (sector %LX)!\n", drec->ghead2->GPT_PtInfo);
         return E_DSK_ERRREAD;
      }
   }
   // check crc32, but ignore it
   crc = crc32(0,0,0); 
   crc = crc32(crc, (u8t*)drec->ptg, sizeof(struct GPT_Record) * pt->GPT_PtCout);

   if (pt->GPT_PtCRC != crc)
      log_it(2, "GPT records CRC mismatch: %08X instead of %08X!\n", crc,
         pt->GPT_PtCRC);
   
   return 0;      
}

/* note, that scan code should not use any FPU.
   if it want so, MTCT_NOFPU flag in the thread below must be removed.
   
   Also, here is a huge disadvantage of scanning in a separate thread:
   scan mutex is catched by caller and we must avoid any calls to every one,
   who uses it.
   
   And also scan_disk_call() below should be used, because this function may
   ask to call it again (if E_PTE_RESCAN has returned).
*/
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
   //log_it(3, "lvm_spt = %u\n", drec->lvm_spt);

   do {
      u8t pt_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR *pt = (struct Disk_MBR *)&pt_buffer;
      u32t stype = dsk_sectortype(drec->disk, chsread?chssec:sector, pt_buffer);

      if (stype==DSKST_ERROR) rc = E_DSK_ERRREAD; else
      if (stype==DSKST_BOOTFAT || stype==DSKST_BOOTBPB || stype==DSKST_BOOTEXF ||
         stype==DSKST_BOOT) rc = E_PTE_FLOPPY; else
      if (stype==DSKST_EMPTY || stype==DSKST_DATA) rc = E_PTE_EMPTY;

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
                  if (rec->PTE_Type==PTE_EE_UEFI || rec->PTE_Type==PTE_EF_UEFI &&
                     rec->PTE_LBAStart==1)
                  {
                     if (!drec->gpthead || rec->PTE_LBAStart<drec->gpthead)
                        drec->gpthead = rec->PTE_LBAStart;
                     drec->gpt_present++;
                  }
                  // too low partition?
                  if (rec->PTE_LBAStart<16) drec->non_lvm = 1;
                  // remember lowest LBA value for DLAT searching
                  if (lowestLBA && rec->PTE_LBAStart<lowestLBA || !lowestLBA)
                     lowestLBA = rec->PTE_LBAStart;
               }
            }
         }
         // too low partition - this should be GPT
         if (lowestLBA && lowestLBA<16) drec->non_lvm = 1;
            else
         /* Searching for DLAT in "Sectors_Per_Track - 1" and in sector 126/254
            (for 127/255 sector disks, made by danis + LVM combination).
            In last case ALL CHS values in partition table is broken and
            most of boot managers go crazy on boot.

            Also, including case when Sectors_Per_Track is wrong and take lowest
            availabe LBA value instead or just search for DLAT as a last chance. */
         for (ii=0; ii<(offset?1:3); ii++) {
            u32t ofs, crcorg, crc;
            // normal geometry on first pass, 127/255 on second and search on third
            switch (ii) {
               case 0:
                  if (!offset && lowestLBA && lowestLBA<drec->lvm_spt) ofs = lowestLBA;
                     else ofs = drec->lvm_spt;
                  break;
               case 1:
                  ofs = disksize < 127*255*65535?127:255;
                  break;
               case 2:
                  ofs = lvm_finddlat(drec->disk, 1, lowestLBA ? lowestLBA : 254);
                  if (ofs) ofs++;
                  break;
            }
            if (!ofs || ii && lowestLBA && lowestLBA<ofs) continue;

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

                  if (drec->lvm_spt!=ofs) {
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
            if (!rc) rc = E_PTE_INVALID;
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
   // disk size changed by scan_gpt()!
   if (rc==E_PTE_RESCAN) return rc;

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
            /* non-GPT partition found, flag hybrid type, set it for EF too,
               because scheme is strange and better to be r/o */
            if (drec->gpt_present && pt!=PTE_EE_UEFI) drec->hybrid = 1;
            // trying to search for max. head value
            if (drec->pts[ii].PTE_HEnd > maxhead) maxhead = drec->pts[ii].PTE_HEnd;
            /* get any of secondary or hidden extended partitions as one big partition
               (and hope what length is valid) */
            if (IS_EXTENDED(pt) && (ii>=4 || drec->extpos==pt_start)) {
               /* include extended header into used space only if it belongs to
                  existing partition */
               if (!dsk_isquadempty(drec,(ii>>2)+1,1)) vllen += drec->lvm_spt;
            } else
            if (!drec->gpt_present || drec->gpt_present && pt!=PTE_EE_UEFI && 
               (pt!=PTE_EF_UEFI || pt_start>1))
            {
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
            if (drec->extstart || drec->extend)
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
        
         // free blocks found?
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

   if (drec->gpt_present) gptcfg_load();

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
   /* lock array creation to prevent mod_stop() at this time */
   mt_swlock();

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

      if (nhdd==hddc) { mt_swunlock(); return; }
      hddc = nhdd;
   }
   if (hddc)
      for (ii=0; ii<hddc; ii++) {
         hdd_info *iptr = get_by_disk(ii);
         if (iptr) iptr->disk = ii;
      }
   mt_swunlock();
}

void scan_free(void) {
   mt_swlock();
   if (memOwner!=-1) {
      mem_freepool(memOwner, memPool);
      hddi = 0; hddc = 0; hddf = 0;
   }
   mt_swunlock();
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

static mt_tid   service_tid = 0;
static qshandle service_que = 0;
// use dynamic MTLIB calls to prevent dependence on its presence
static qs_mtlib       mtlib = 0;

#define SCAN_THREAD_WAIT  45000   ///< scan thread life time (45s)

static u32t scan_disk_call(hdd_info *drec) {
   u32t res = scan_disk(drec);
   /* GPT read code can found wrong int 13h ah=48 disk size, but working
      disk i/o. In this case it forces a real disk size and returns
      E_PTE_RESCAN to scan again  */
   if (res==E_PTE_RESCAN) {
      hlp_disksize(drec->disk, 0, &drec->info);
      res = scan_disk(drec);
   }
   return res;
}

static u32t _std scan_thread(void *arg) {
   mt_threadname("dmgr scan");

   while (service_que) {
      int    avail;
      qe_event *ev = qe_waitevent(service_que, SCAN_THREAD_WAIT);

      if (ev) {
         hdd_info *hi = (hdd_info*)ev->a;
         hi->scan_rc  = scan_disk_call(hi);
         hi->inited   = 1;
         // wake up caller thread by unused signal number
         mtlib->sendsignal(ev->b, ev->c, FFFF-0xFFF, 0);
         free(ev);
      } else {
         // no event for 45 sec - exit
         mt_swlock();
         service_tid = 0;
         mt_swunlock();
         break;
      }
   }
   return 0;
}

/* we have a trouble in MT mode: process kill can stop us in the middle of
   disk scan and broke disk information for all.
   SO - we create a thread in PID 1 context - to use it as a scan processor
   and ask to scan disks for us via queue */
static void scan_disk_mt(hdd_info *drec) {
   mt_swlock();
   // first time init
   if (!mtlib) {
      mtlib = NEW_G(qs_mtlib);
      qe_create(0, &service_que);
      if (service_que)
         if (io_setstate(service_que, IOFS_DETACHED, 1)) service_que = 0;
      // just panic, should never occurs
      if (!service_que) _throwmsg_("partmgr: scan queue error!");
   }
   if (!service_tid) {
      mt_ctdata  ctd;
      memset(&ctd, 0, sizeof(ctd));
      ctd.size      = sizeof(mt_ctdata);
      ctd.stacksize = _64KB;
      ctd.pid       = 1;
      /* create detached thread else it will have the caller`s session!!!
         such a case is very bad, because "lost session" may remain on the
         screen until this thread exit (45 seconds!) */
      service_tid = mtlib->thcreate(scan_thread, MTCT_NOFPU|MTCT_DETACHED, &ctd, 0);
      // just panic, should never occurs
      if (!service_tid) _throwmsg_("partmgr: scan thread start error!");
   }
   mt_swunlock();
   drec->inited = 0;
   qe_postevent(service_que, 0, (long)drec, mod_getpid(), mt_getthread());

   // wait for any signal and check drec->inited on it
   while (!drec->inited) {
      mt_waitentry we;
      we.htype = QWHT_SIGNAL;
      we.group = 0;
      mtlib->waitobject(&we, 1, 0, 0);
   }
}

qserr _std dsk_ptrescan(u32t disk, int force) {
   hdd_info *hi;
   u32t     res;

   scan_lock();
   scan_init();

   hi = get_by_disk(disk);
   if (!hi) res = E_DSK_DISKNUM; else {
      hlp_disksize(disk, 0, &hi->info);
      if (!hi->info.TotalSectors) res = E_DSK_ERRREAD; else {
#if 0
         log_it(3, "disk %X %d %LX %d x %d x %d\n", disk, hi->info.SectorSize,
            hi->info.TotalSectors, hi->info.Cylinders, hi->info.Heads,
               hi->info.SectOnTrack);
#endif
         // rescan on force, first time or floppy disk
         if (hi->inited && !force && (disk&QDSK_FLOPPY)==0) res = hi->scan_rc; else {
            if (mt_active()) {
               scan_disk_mt(hi);
               res = hi->scan_rc;
            } else {
               res = scan_disk_call(hi);
               hi->scan_rc = res;
               hi->inited  = 1;
            }
         }
      }
   }
   scan_unlock();
   return res;
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
   int  verbose = 0, force = 0,
        ioredir = !isatty(fileno(stdout));
   u32t      ii;
   if (args->count>=pos+1) {
      static char *argstr   = "force|/v";
      static short argval[] = {    1, 1};
      str_list *rcargs = str_parseargs(args, pos, SPA_RETLIST, argstr, argval,
                                       &force, &verbose);
      ii = rcargs->count;
      free(rcargs);
      // we must get empty list, else unknown keys here
      if (ii) { cmd_shellerr(EMSG_CLIB, EINVAL, 0); return EINVAL; }
   }
   // do not init "pager" at nested calls
   if (cmd) cmd_printseq(0,1,0);

   if (disk==FFFF || disk==x7FFF) {
      int  pstate = vio_setansi(1);
      u32t  n_hdd, n_fdd;
      scan_lock();
      scan_init();
      if (!hddf) shellprn(" %d HDD%s in system\n", hddc, hddc<2?"":"s"); else
         shellprn(" %d HDD and %d floppy drives in system\n", hddc, hddf);
      // save # of disks while we`re in lock
      n_hdd = hddc;
      n_fdd = hddf;
      scan_unlock();

      for (ii=0; ii<n_hdd+n_fdd; ii++) {
         u32t dsk = ii<n_fdd ? QDSK_FLOPPY|ii : ii-n_fdd;
         // hard disk if OFF, skip it
         if ((dsk&QDSK_FLOPPY)==0 && (hlp_diskmode(dsk,HDM_QUERY)&HDM_QUERY)==0)
            continue;

         if (disk==FFFF) { // dmgr list
            disk_geo_data  gdata;
            char          *stext, namebuf, sizebuf[32], robuf[16],
                        *dskname = dsk_disktostr(dsk,0);
            qs_extdisk      edsk = hlp_diskclass(dsk, 0);
            int               ro = 0;
            sizebuf[0] = 0;
            robuf  [0] = 0;
            // at least LVM info should be updated
            if (force) dsk_ptrescan(dsk,1);
            if (edsk)
               if (edsk->state(EDSTATE_QUERY) & EDSTATE_RO) ro = 1;

            hlp_disksize(dsk, 0, &gdata);
            if (gdata.TotalSectors) {
               stext = get_sizestr(gdata.SectorSize, gdata.TotalSectors);
               if (verbose) sprintf(sizebuf, " (%Lu sectors)", gdata.TotalSectors);
               if (ro) strcpy(robuf, " (readonly)");
            } else {
               stext = "    no info";
            }

            if (shellprn(verbose?" %cDD %i =>%-4s: %s%s%s%s%s":" %cDD %i =>%-4s: %s%s%s%s%s",
               dsk&QDSK_FLOPPY?'F':'H', dsk&QDSK_DISKMASK, dskname, ioredir?"":ANSI_WHITE,
                  stext, ioredir?"":ANSI_RESET, sizebuf, robuf)) break;
            if (verbose) {
               disk_geo_data ldata;
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
      char      *out = 0;
      u32t        rc = 0;
      hdd_info   *hi;
      // Make all printing inside mutex, then flush it to console.
      scan_lock();
      hi = get_by_disk(disk);
      if (!hi) { 
         printf("There is no disk %X!\n", disk); 
         rc = ENOMNT; 
      } else {
         disk_volume_data  vi[DISK_COUNT];
         lvm_disk_data   lvmi;
         char         lvmname[32], *stext;
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
         out = sprintf_dyn("%s %s %i: %s (%LX sectors), %d partition%s%s%s\n",
            ioredir?"":ANSI_WHITE, disk&QDSK_FLOPPY?"Floppy disk":"HDD",
               disk&QDSK_DISKMASK, stext, hi->info.TotalSectors, hi->gpt_view,
                  hi->gpt_view==1?"":"s", lvmname, ioredir?"":ANSI_RESET);

         switch (hi->scan_rc) {
            case E_DSK_ERRREAD:
               out = strcat_dyn(out, ANSI_LRED " Disk read error!" ANSI_RESET "\n");
               break;
            case E_PTE_INVALID:
               out = strcat_dyn(out, ANSI_LRED " Partition table is invalid!" ANSI_RESET "\n");
               break;
            case E_PTE_FLOPPY :
               out = strcat_dyn(out, " Disk is floppy formatted!\n");
               break;
            case E_PTE_EMPTY  :
               out = strcat_dyn(out, " Disk is empty!\n");
               break;
            default :
               if (hi->scan_rc) {
                  char *err = make_errmsg(hi->scan_rc, "Scan error");
                  out = strcat_dyn(out, ANSI_LRED " ");
                  out = strcat_dyn(out, err);
                  free(err);
                  out = strcat_dyn(out, ANSI_RESET "\n");
               }
               break;
         }
         if (hi->pt_view) {
            // query mounted volumes
            for (ii = 0; ii<DISK_COUNT; ii++) hlp_volinfo(ii, vi+ii);
            // inform about hybrid
            if (hi->gpt_present)
               out = strcat_dyn(out, ANSI_YELLOW " Hybrid partition table (MBR + GPT)!"
                  ANSI_RESET "\n");
            // print table
            out = strcat_dyn(out,
               " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ \n"
               "  ## ³     type     ³ fs info ³   size   ³ mount ³   LBA    ³  LVM info       \n"
               " ÄÄÄÄÅÄÄÄÄÄÄÄÄÄÂÄÄÄÄÅÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄ \n");
            for (ii = 0; ii<hi->pt_view; ii++) {
               char          pdesc[16], mounted[64], lvmpt[20], *pline;
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

               pline = sprintf_dyn("  %2d ³ %s ³ %02X ³ %-8s³%s ³ %s ³ %08LX ³ %s\n",
                  ii, (flags&(DPTF_ACTIVE|DPTF_PRIMARY))==(DPTF_ACTIVE|DPTF_PRIMARY)
                     ?"active ":(flags&DPTF_PRIMARY?"primary":"logical"), ptype,
                        pdesc, get_sizestr(hi->info.SectorSize, psize)+2,
                           *mounted?mounted:"     ", pstart, lvmpt);
               out = strcat_dyn(out, pline);
               free(pline);
            }
         }
         if (hi->gpt_present) {
            u32t wide = ioredir ? 1000 : 0;

            if (!wide) vio_getmode(&wide, 0);

            // query mounted volumes if it was not done in 
            if (!hi->pt_view)
               for (ii = 0; ii<DISK_COUNT; ii++) hlp_volinfo(ii, vi+ii);
            else
               out = strcat_dyn(out, "\n");
         
            out = strcat_dyn(out,
               " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ \n"
               "  ## ³ fs info ³   size   ³ mount ³    LBA    ³ LVM ³ attr ³      type        \n"
               " ÄÄÄÄÅÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ \n");
         
            for (ii = 0; ii<hi->gpt_view; ii++)
               // print both GPT and hybrid partitions here
               if (dsk_isgpt(disk, ii) > 0) {
                  dsk_gptpartinfo  gpi;
                  char       pdesc[16], mounted[64], guidstr[40], *ptstr, *pline,
                             eflags[8], lvmvol[8];
                  u64t   psize, pstart;
                  u32t              kk;
                  char          letter;
         
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
                  lvmvol[0] = 0;
                  if (!dsk_gptletter(&gpi, -1, &letter))
                     if (letter)
                        snprintf(lvmvol, sizeof(lvmvol), "  %c: ", letter);

                  if (gpi.Attr) {
                     char *epf = eflags;
                     u64t attr = gpi.Attr;
                     // use only valid bits for a non-windows volume
                     if (memcmp(&windataguid, &gpi.TypeGUID, 16)) attr &= 7;

                     for (kk = 0; part_flags[kk]>=0; kk++)
                        if ((u64t)1<<part_flags[kk] & attr) *epf++ = part_char[kk];
                     *epf = 0;
                     // align Attr string to center
                     kk = strlen(eflags);
                     if (kk) {
                        kk = 3 - (Round2(kk)>>1);
                        while (kk--) *epf++=' ';
                        *epf = 0;
                     }
                  }

                  pline = sprintf_dyn("  %2d ³ %-8s³%s ³ %s ³%10.9LX ³%5s³%6s³ ",
                     ii, pdesc, get_sizestr(hi->info.SectorSize, psize)+2,
                        *mounted?mounted:"     ", pstart, lvmvol, eflags);
                  if (ptstr) pline = strcat_dyn(pline, ptstr);
                  // cut it!
                  if (strlen(ptstr) > wide-2) ptstr[wide-2] = 0;
                  out = strcat_dyn(out, pline);
                  out = strcat_dyn(out, "\n");
                  if (ptstr) free(ptstr);
                  free(pline);
               }
         }

      }
      scan_unlock();

      if (!rc) {
         u32t svstate = vio_setansi(1);
         // del trailing \n
         if (strclast(out)=='\n') out[strlen(out)-1] = 0;
         if (shellprt(out)) rc = EINTR;

         vio_setansi(svstate);
      }
      if (out) free(out);
   }
   return 0;
}
