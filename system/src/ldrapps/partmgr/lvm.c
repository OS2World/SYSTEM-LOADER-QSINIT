#include "pscan.h"
#include "stdlib.h"
#include "qsdm.h"

#define CRC_POLYNOMIAL    0xEDB88320
static u32t CRC_Table[256];

void lvm_buildcrc(void) {
   u32t ii, jj, crc;
   for (ii=0; ii<=255; ii++) {
      crc = ii;
      for (jj=8; jj>0; jj--)
         if (crc&1) crc = crc>>1^CRC_POLYNOMIAL; else crc>>=1;
      CRC_Table[ii] = crc;
   }
}

u32t _std lvm_crc32(u32t crc, void *Buffer, u32t BufferSize) {
   u8t *Current_Byte = (u8t*) Buffer;
   u32t Temp1, Temp2, ii;
   for (ii=0; ii<BufferSize; ii++) {
     Temp1 = crc >> 8 & 0x00FFFFFF;
     Temp2 = CRC_Table[( crc ^ (u32t) *Current_Byte ) & (u32t)0xFF];
     Current_Byte++;
     crc = Temp1 ^ Temp2;
   }
   return crc;
}

int _std lvm_diskinfo(u32t disk, lvm_disk_data *info) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return 0;
   dsk_ptrescan(disk,0);

   if (!hi->lvm_snum) return 0;
   if (!hi->dlat[0].DLA_Signature1) return 0;
   if (!info) return 1;
   info->DiskSerial   = hi->lvm_snum;
   info->BootSerial   = hi->lvm_bsnum;
   info->Cylinders    = hi->dlat[0].Cylinders;
   info->Heads        = hi->dlat[0].Heads_Per_Cylinder;
   info->SectPerTrack = hi->dlat[0].Sectors_Per_Track;
   strncpy(info->Name, hi->dlat[0].Disk_Name, 20);
   info->Name[19] = 0;
   return 1;
}

int _std lvm_partinfo(u32t disk, u32t index, lvm_partition_data *info) {
   hdd_info *hi = get_by_disk(disk);
   u32t   start, size, ii, recidx, dlatidx;
   if (!hi) return 0;
   if (lvm_dlatpos(disk, index, &recidx, &dlatidx)) return 0;

   if (info) {
      DLA_Entry *de = &hi->dlat[recidx>>2].DLA_Array[dlatidx];

      info->DiskSerial   = hi->lvm_snum;
      info->BootSerial   = hi->lvm_bsnum;
      info->VolSerial    = de->Volume_Serial;
      info->PartSerial   = de->Partition_Serial;
      info->Start        = de->Partition_Start;
      info->Size         = de->Partition_Size;
      info->BootMenu     = de->On_Boot_Manager_Menu?1:0;
      info->Letter       = de->Drive_Letter;
      strncpy(info->VolName, de->Volume_Name, 20);
      info->VolName[19] = 0;
      strncpy(info->PartName, de->Partition_Name, 20);
      info->PartName[19] = 0;
   }
   return 1;
}

int lvm_dlatpos(u32t disk, u32t index, u32t *recidx, u32t *dlatidx) {
   hdd_info *hi = get_by_disk(disk);
   u32t   start, size, ii, *pidx, ridx, didx = FFFF;
   if (recidx)  *recidx  = 0;
   if (dlatidx) *dlatidx = 0;
   if (!hi) return LVME_DISKNUM;
   dsk_ptrescan(disk,0);
   // no serial number - no info at all
   if (!hi->lvm_snum) return LVME_NOINFO;
   // query position
   if (!dsk_ptquery(disk,index,&start,&size,0,0,0)) return LVME_INDEX;
   // get mbr record
   pidx = memchrd(hi->index, index, hi->pt_size);
   if (!pidx) return 0;
   ridx = pidx - hi->index;
   // DLAT was found
   if (!hi->dlat[ridx>>2].DLA_Signature1) return LVME_NOBLOCK; else
   for (ii=0; ii<4; ii++) {
      DLA_Entry *de = &hi->dlat[ridx>>2].DLA_Array[ii];

      if (de->Partition_Size==size && de->Partition_Start==start) {
         didx = ii;
         break;
      }
   }
   if (recidx)  *recidx  = ridx;
   if (didx==FFFF) return LVME_NOPART;
   if (dlatidx) *dlatidx = didx;
   return 0;
}

int _std lvm_querybm(u32t *active) {
   u32t hdds = hlp_diskcount(0), ii;
   if (!active) return 0;
   for (ii=0; ii<hdds; ii++) {
      hdd_info *hi = get_by_disk(ii);
      u32t     idx;
      dsk_ptrescan(ii,0);
      active[ii] = 0;
      if (!hi) continue;
      if (!hi->lvm_snum) continue;

      for (idx=0; idx<hi->pt_size; idx++)
         if (hi->dlat[idx>>2].DLA_Signature1) {
            DLA_Entry *de = &hi->dlat[idx>>2].DLA_Array[idx&3];
            if (de->On_Boot_Manager_Menu && hi->index[idx]<32)
               active[ii] |= 1<<hi->index[idx];
         }
   }
   return 1;
}

int _std lvm_checkinfo(u32t disk) {
   hdd_info *hi = get_by_disk(disk);
   u32t      ii, idx;
   if (!hi) return LVME_DISKNUM;
   dsk_ptrescan(disk,0);
   if (hi->scan_rc==DPTE_FLOPPY) return LVME_FLOPPY;
   if (hi->scan_rc==DPTE_EMPTY) return LVME_EMPTY;
   if (hi->gpt_present) return LVME_GPTDISK;
   if (hi->non_lvm) return LVME_LOWPART;
   // no serial number - no info at all
   if (!hi->lvm_snum) return LVME_NOINFO;

   for (ii=0; ii<hi->pt_size; ii++) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      u8t pt = hi->pts[ii].PTE_Type;
      // whole block is missing
      if (!dls->DLA_Signature1) return LVME_NOBLOCK;
      // check partition entry presence in DLAT
      if (pt && !IS_EXTENDED(pt)) {
         for (idx=0; idx<4; idx++)
            if (dls->DLA_Array[idx].Partition_Size==hi->pts[ii].PTE_LBASize &&
                dls->DLA_Array[idx].Partition_Start==hi->pts[ii].PTE_LBAStart)
               break;
         if (idx==4) return LVME_NOPART;
      }
   }

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      // check serial number
      if (dls->Disk_Serial!=hi->dlat[0].Disk_Serial)
         return LVME_SERIAL;
      // check CHS
      if (dls->Cylinders!=hi->dlat[0].Cylinders ||
         dls->Heads_Per_Cylinder!=hi->dlat[0].Heads_Per_Cylinder ||
            dls->Sectors_Per_Track!=hi->dlat[0].Sectors_Per_Track)
               return LVME_GEOMETRY;
   }
   return 0;
}


u32t _std lvm_usedletters(void) {
   u32t hdds = hlp_diskcount(0), ii, rc = 0;

   for (ii=0; ii<hdds; ii++) {
      hdd_info *hi = get_by_disk(ii);
      u32t     idx;
      dsk_ptrescan(ii,0);
      if (!hi) continue;
      if (!hi->lvm_snum) continue;

      for (idx=0; idx<hi->pt_size; idx++)
         if (hi->dlat[idx>>2].DLA_Signature1) {
            DLA_Entry *de = &hi->dlat[idx>>2].DLA_Array[idx&3];
            if (de->Drive_Letter) {
               char ltr = toupper(de->Drive_Letter);
               if (ltr>='A' && ltr<='Z') rc |= 1<<ltr-'A';
            }
         }
   }
   return rc;
}

int lvm_flushdlat(u32t disk, u32t quadidx) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return LVME_DISKNUM;
   if (quadidx>=hi->pt_size>>2) return LVME_INDEX; else {
      u8t  sbuf[MAX_SECTOR_SIZE];
      u32t tsec;
      DLA_Table_Sector *dls = hi->dlat + quadidx;

      dls->DLA_CRC = 0;
      memset(sbuf, 0, hi->info.SectorSize);
      memcpy(sbuf, dls, sizeof(DLA_Table_Sector));

      ((DLA_Table_Sector*)&sbuf)->DLA_CRC = lvm_crc32(LVM_INITCRC, 
         sbuf, hi->info.SectorSize);

      tsec = hi->ptspos[quadidx]+hi->lvm_spt-1;
      log_it(2,"flush dlat: %X, quad %d, sector %08X\n", disk, quadidx, tsec);

      if (!hlp_diskwrite(disk, tsec, 1, sbuf)) return LVME_IOERROR;
   }
   return 0;
}

/// flush all modified DLATs (with CRC=FFFF). Function doesn`t checks anything!
int lvm_flushall(u32t disk, int force) {
   u32t ii;
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return LVME_DISKNUM;

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);

      if (force || dls->DLA_CRC == FFFF) {
         u32t rc = lvm_flushdlat(disk,ii>>2);
         if (rc) return rc;
      }
   }
   return 0;
}

u32t _std lvm_assignletter(u32t disk, u32t index, char letter, int force) {
   u32t ridx, didx;
   char  ltr = toupper(letter);
   int    rc;
   // check letter
   if (ltr && (ltr<'A' || ltr>'Z') && ltr!='*') return LVME_LETTER;
   if (!force)
      if (ltr && ltr!='*')
         if ((lvm_usedletters()&1<<ltr-'A')!=0) return LVME_LETTER;
   // query dlat with error code
   rc = lvm_dlatpos(disk, index, &ridx, &didx);
   if (!rc) {
      hdd_info *hi = get_by_disk(disk);
      // save letter
      DLA_Table_Sector *dls = hi->dlat + (ridx>>2);
      dls->DLA_Array[didx].Drive_Letter = ltr;
      // and flush it
      rc = lvm_flushdlat(disk,ridx>>2);
   }
   return rc;
}

u32t _std lvm_initdisk(u32t disk, disk_geo_data *vgeo, int separate) {
   hdd_info *hi = get_by_disk(disk);
   u32t      ii, idx, pti, heads, cyls, writecnt = 0;
   int       rc, same_ser = 0, snum_commit = 0;
   if (!hi) return LVME_DISKNUM;
   dsk_ptrescan(disk,1);
   // check LVM info
   rc = lvm_checkinfo(disk);
   // init again on good info only if separate key is not "AUTO"
   if (rc==LVME_SERIAL||rc==LVME_GEOMETRY||rc==LVME_EMPTY||rc==LVME_FLOPPY||
      rc==LVME_GPTDISK||rc==LVME_LOWPART||!rc&&separate<0) return rc;
   // check supplied geometry
   if (vgeo)
      if (!vgeo->Cylinders || vgeo->Cylinders>65535 || !vgeo->Heads || 
         vgeo->Heads>255 || !vgeo->SectOnTrack || vgeo->SectOnTrack>255)
            return LVME_GEOMETRY;
   // disk was a master bootable?
   if (hi->lvm_snum && hi->lvm_bsnum == hi->lvm_snum) same_ser = 1;
   // generate serial number
   while (!hi->lvm_snum) {
      hi->lvm_snum = random(65356) | random(65356)<<16;
      // disk is not hd0 - get "Boot Disk Serial"
      if (!separate && disk!=0) {
         hdd_info *hd0 = get_by_disk(0);
         dsk_ptrescan(0,0);
         if (hd0->lvm_snum) hi->lvm_bsnum = hd0->lvm_snum;
      }
      // still no boot serial? use own for it...
      if (!hi->lvm_bsnum) hi->lvm_bsnum = hi->lvm_snum; 
      snum_commit = 1;
   }
   // use own serial as boot serial
   if (separate>0 || same_ser && separate<0) 
      if (hi->lvm_bsnum != hi->lvm_snum) {
         hi->lvm_bsnum = hi->lvm_snum;
         snum_commit   = 1;
      }
   // searching for cylinder/heads value
   if (vgeo) {
      heads = vgeo->Heads;
      cyls  = vgeo->Cylinders;
      // !!!!
      hi->lvm_spt = vgeo->SectOnTrack;
   } else {
      heads = 0;
      cyls  = 0;
      for (ii=0; ii<hi->pt_size; ii+=4) {
         DLA_Table_Sector *dls = hi->dlat + (ii>>2);
         if (!dls->DLA_Signature1) continue;
         heads = dls->Heads_Per_Cylinder;
         cyls  = dls->Cylinders;
         break;
      }
      if (!heads && !cyls) {
         disk_geo_data geo;
         if (dsk_getptgeo(disk,&geo)==0 && geo.TotalSectors < 1024*255*63) {
            heads = geo.Heads;
            cyls  = geo.Cylinders;
         } else {
            heads = 255;
            /* partition table is empty? 
               * if yes, we can create our`s ugly LVM geometry here ;) 
               * if not - disk will be truncated at 512Gb */
            if (hi->pt_size==4 && dsk_isquadempty(hi,0,0)) {
               u32t spt = 63;
               do {
                  cyls = hi->info.TotalSectors / (heads * spt);
                  if (cyls>>16)
                     if (spt<128) spt = spt<<1|1; else cyls = 65535;
               } while (cyls>>16);
               // !!!!
               hi->lvm_spt = spt;
            } else {
               cyls = hi->info.TotalSectors / (heads * hi->lvm_spt);
               if (cyls>65535) cyls = 65535;
            }
         }
      } else
      if (!heads || !cyls) return LVME_GEOMETRY;
   }

   log_it(2,"init dlat: %X, %d x %d x %d, current state %d, ser %08X, boot %08X\n", 
      disk, cyls, heads, hi->lvm_spt, rc, hi->lvm_snum, hi->lvm_bsnum);

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      // whole block is missing
      if (!dls->DLA_Signature1) {
         dls->DLA_Signature1     = DLA_TABLE_SIGNATURE1;
         dls->DLA_Signature2     = DLA_TABLE_SIGNATURE2;
         dls->Disk_Serial        = hi->lvm_snum;
         dls->Boot_Disk_Serial   = hi->lvm_bsnum;
         dls->Sectors_Per_Track  = hi->lvm_spt;
         dls->Cylinders          = cyls;
         dls->Heads_Per_Cylinder = heads;
         // mark this DLAT as created (in present sectors CRC is 0 after scan)
         dls->DLA_CRC            = FFFF;
         sprintf(dls->Disk_Name,"[ %s ]",dsk_disktostr(disk,0));
         // signal write pass
         writecnt++;
      }
      // check DLAT entries for missing partitions
      for (idx=0; idx<4; idx++) {
         u32t lpsize = dls->DLA_Array[idx].Partition_Size,
               lppos = dls->DLA_Array[idx].Partition_Start;
         if (lpsize && lppos) {
            for (pti=0; pti<4; pti++) {
               u8t pt = hi->pts[ii+pti].PTE_Type;
               if (pt && !IS_EXTENDED(pt))
                  if (lpsize==hi->pts[ii+pti].PTE_LBASize &&
                     lppos==hi->pts[ii+pti].PTE_LBAStart)
                        break;
            }
            // no partition for this entry, remove it
            if (pti==4) {
               if (idx<3) 
                  memcpy(&dls->DLA_Array[idx],&dls->DLA_Array[idx+1],sizeof(DLA_Entry));
               memset(&dls->DLA_Array[3],0,sizeof(DLA_Entry));
               // signal write pass
               writecnt++;
               dls->DLA_CRC = FFFF;
            }
         }
      }
      // check partitions for missing DLAT entries
      for (idx=0; idx<4; idx++) {
         u8t pt = hi->pts[ii+idx].PTE_Type;

         if (pt && !IS_EXTENDED(pt)) {
            u32t lpsize = hi->pts[ii+idx].PTE_LBASize,
                  lppos = hi->pts[ii+idx].PTE_LBAStart;
            for (pti=0; pti<4; pti++)
               if (dls->DLA_Array[pti].Partition_Size==lpsize &&
                  dls->DLA_Array[pti].Partition_Start==lppos)
                     break;
            // no DLAT entry for this partition, add it
            if (pti==4)
               for (pti=0; pti<4; pti++) {
                  DLA_Entry *de = &dls->DLA_Array[pti];

                  if (!de->Partition_Size && !de->Partition_Start) {
                     u32t index = hi->index[ii+idx];
                     memset(de,0,sizeof(DLA_Entry));

                     if (index!=FFFF) {
                        sprintf(de->Partition_Name,"[ Vol.%02d ]",index);
                        if (pt!=PTE_0A_BOOTMGR)
                           strcpy(de->Volume_Name,de->Partition_Name);
                     }
                     de->Volume_Serial    = random(65356) | random(65356)<<16;
                     de->Partition_Serial = random(65356) | random(65356)<<16;
                     de->Partition_Size   = lpsize;
                     de->Partition_Start  = lppos;
                     // signal write pass
                     writecnt++;
                     dls->DLA_CRC = FFFF;
                     break;
                  }
               }
         }
      }
   }
   // some of DLATs was changed, write it
   if (writecnt||snum_commit) return lvm_flushall(disk, snum_commit);

   return 0;
}

u32t _std lvm_setname(u32t disk, u32t index, u32t nametype, const char *name) {
   hdd_info *hi = get_by_disk(disk);
   u32t      ii, rc;
   if (!hi) return LVME_DISKNUM;
   // check LVM info (rescan called here)
   rc = lvm_checkinfo(disk);
   if (rc) return rc;
   if (nametype && index>=hi->pt_view) return LVME_INDEX;
   // disk name
   if (!nametype) {

      for (ii=0; ii<hi->pt_size; ii+=4) {
         DLA_Table_Sector *dls = hi->dlat + (ii>>2);

         strncpy(dls->Disk_Name, name, LVM_NAME_SIZE);
         dls->Disk_Name[LVM_NAME_SIZE-1] = 0;
         dls->DLA_CRC = FFFF;
      }

   } else 
   if ((nametype&(LVMN_PARTITION|LVMN_VOLUME))!=0) {
      u32t recidx, dlatidx;
      rc = lvm_dlatpos(disk, index, &recidx, &dlatidx);

      if (rc) return rc; else {
         DLA_Table_Sector *dls = hi->dlat + (recidx>>2);
         DLA_Entry         *de = &dls->DLA_Array[dlatidx];

         if (nametype&LVMN_VOLUME) {
            strncpy(de->Volume_Name, name, LVM_NAME_SIZE);
            de->Volume_Name[19] = 0;
         }
         if (nametype&LVMN_PARTITION) {
            strncpy(de->Partition_Name, name, LVM_NAME_SIZE);
            de->Partition_Name[19] = 0;
         }
         dls->DLA_CRC = FFFF;
      }
   }
   return lvm_flushall(disk,0);
}

int _std lvm_delptrec(u32t disk, u32t index) {
   hdd_info *hi = get_by_disk(disk);
   u32t    ridx, dlidx;
   int       rc;
   if (!hi) return LVME_DISKNUM;
   rc = lvm_dlatpos(disk, index, &ridx, &dlidx);
   if (rc) return rc;
   // move other entries to start
   if (dlidx<3) {
      DLA_Entry *de = &hi->dlat[ridx>>2].DLA_Array[dlidx];
      memmove(de, de+1, sizeof(DLA_Entry)*(3-dlidx));
   }
   // wipe last
   memset(&hi->dlat[ridx>>2].DLA_Array[3], 0, sizeof(DLA_Entry));
   // mark this DLAT sector as modified
   hi->dlat[ridx>>2].DLA_CRC = FFFF;
   // and flush it
   return lvm_flushall(disk,0);
}

int _std lvm_wipedlat(u32t disk, u32t quadidx) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return LVME_DISKNUM;
   if (quadidx>=hi->pt_size>>2) return LVME_INDEX;
   if (!hi->dlat[quadidx].DLA_Signature1) return LVME_NOBLOCK;
   hi->dlat[quadidx].DLA_Signature1 = 0;
   hi->dlat[quadidx].DLA_Signature2 = 0;
   return lvm_flushdlat(disk,quadidx);
}

int _std lvm_wipeall(u32t disk) {
   hdd_info  *hi = get_by_disk(disk);
   u32t   rc, ii;
   if (!hi) return LVME_DISKNUM;

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      memset(dls, 0, sizeof(DLA_Table_Sector));

      rc = lvm_flushdlat(disk,ii>>2);
      if (rc) return rc;
   }
   return 0;
}

u32t _std lvm_findname(const char *name, u32t nametype, u32t *disk, u32t *index) {
   if (!disk||!name||!*name||(nametype&~(LVMN_PARTITION|LVMN_VOLUME)))
      return LVME_INVARGS;
   if (nametype!=LVMN_DISK && !index) return LVME_INVARGS;
   if (strlen(name)>LVM_NAME_SIZE) return nametype==LVMN_DISK?LVME_DSKNAME:LVME_PTNAME;

   if (nametype==LVMN_DISK || *disk==FFFF) {
      u32t hdds = hlp_diskcount(0), ii;
      
      for (ii=0; ii<hdds; ii++) {
         hdd_info *hi = get_by_disk(ii);
         if (!hi) continue;
         dsk_ptrescan(ii,0);
         if (!hi->lvm_snum) continue;

         if (nametype==LVMN_DISK) {
            if (strnicmp(name,hi->dlat[0].Disk_Name,LVM_NAME_SIZE)==0) {
               *disk = ii;
               return 0;
            }
         } else
         if (!lvm_findname(name, nametype, &ii, index)) {
            *disk = ii;
            return 0;
         }
      }
      return nametype==LVMN_DISK?LVME_DSKNAME:LVME_PTNAME;
   } else {
      hdd_info *hi = get_by_disk(*disk);
      u32t     idx;
      if (!hi) return LVME_DISKNUM;
      *index = FFFF;
      dsk_ptrescan(*disk,0);
      if (!hi->lvm_snum) return LVME_NOINFO;

      for (idx=0; idx<hi->pt_size; idx++)
         if (hi->dlat[idx>>2].DLA_Signature1) {
            DLA_Entry *de = &hi->dlat[idx>>2].DLA_Array[idx&3];
            char  *ptname = nametype&LVMN_PARTITION ? de->Partition_Name :
                            de->Volume_Name;

            if (ptname[0] && strnicmp(name,ptname,LVM_NAME_SIZE)==0) {
               u32t ii;
               // search partition entry for this DLAT entry
               for (ii=0; ii<4; ii++) {
                  u32t            pt4idx = (idx&~3)+ii;
                  struct MBR_Record *rec = hi->pts + pt4idx;

                  if (rec->PTE_Type && rec->PTE_LBAStart==de->Partition_Start
                     && rec->PTE_LBASize==de->Partition_Size) 
                  {
                     // this is an indexed partition?
                     if (hi->index[pt4idx]!=FFFF) {
                        *index = hi->index[pt4idx];
                        return 0;
                     }
                  }
               }
            }
         }
      return LVME_PTNAME;
   }
}
