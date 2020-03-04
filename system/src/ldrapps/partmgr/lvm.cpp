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
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
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
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t    start, size, ii, recidx, dlatidx;
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
      info->Installable  = de->Installable;
      strncpy(info->VolName, de->Volume_Name, 20);
      info->VolName[19] = 0;
      strncpy(info->PartName, de->Partition_Name, 20);
      info->PartName[19] = 0;
   }
   return 1;
}

/* no locks here, because no use of hdd_info.
   This function may be called by scan itself. */
u32t _std lvm_finddlat(u32t disk, u32t sector, u32t count) {
   u32t sectsz = 0; 
   if (!count || !sector) return 0;
   // ignore disk size
   hlp_disksize64(disk, &sectsz);
   if (sectsz) {
      u32t sper32k = _32KB/sectsz,
           bufsize = count<sper32k ? count : sper32k,
                rc = 0;
      // allocate buffer
      u8t    *zbuf = (u8t*)malloc_thread(bufsize * sectsz);
      // read by 32k
      while (count) {
         u32t toread = count<sper32k ? count : sper32k, ii;
         if (hlp_diskread(disk,sector,toread,zbuf) != toread) break;
         // scan sectors
         for (ii=0; ii<toread; ii++) {
            DLA_Table_Sector *dla = (DLA_Table_Sector*)(zbuf+ii*sectsz);
            if (dla->DLA_Signature1==DLA_TABLE_SIGNATURE1 &&
                dla->DLA_Signature2==DLA_TABLE_SIGNATURE2) { rc = sector+ii; break; }
         }
         count -= toread;
         sector+= toread;
      }
      free(zbuf);
      return rc;
   }
   return 0; 
}

qserr _std lvm_dlatpos(u32t disk, u32t index, u32t *recidx, u32t *dlatidx) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t    start, size, ii, *pidx, ridx, didx = FFFF;
   if (recidx)  *recidx  = 0;
   if (dlatidx) *dlatidx = 0;
   if (!hi) return E_DSK_DISKNUM;
   dsk_ptrescan(disk,0);
   // no serial number - no info at all
   if (!hi->lvm_snum) return E_LVM_NOINFO;
   // query position
   if (!dsk_ptquery(disk,index,&start,&size,0,0,0)) return E_PTE_PINDEX;
   // get mbr record
   pidx = memchrd(hi->index, index, hi->pt_size);
   if (!pidx) return 0;
   ridx = pidx - hi->index;
   // DLAT was found
   if (!hi->dlat[ridx>>2].DLA_Signature1) return E_LVM_NOBLOCK; else
   for (ii=0; ii<4; ii++) {
      DLA_Entry *de = &hi->dlat[ridx>>2].DLA_Array[ii];

      if (de->Partition_Size==size && de->Partition_Start==start) {
         didx = ii;
         break;
      }
   }
   if (recidx)  *recidx  = ridx;
   if (didx==FFFF) return E_LVM_NOPART;
   if (dlatidx) *dlatidx = didx;
   return 0;
}

int _std lvm_querybm(u32t *active) {
   FUNC_LOCK  lk;
   u32t     hdds = hlp_diskcount(0), ii;
   if (!active) return 0;
   for (ii=0; ii<hdds; ii++) {
      hdd_info *hi = get_by_disk(ii);
      u32t     idx;
      dsk_ptrescan(ii,0);
      active[ii] = 0;
      if (!hi) continue;
      if (!hi->lvm_snum) continue;

      for (idx=0; idx<hi->pt_view && idx<32; idx++) {
         u32t   recidx, dlatidx;

         if (lvm_dlatpos(ii, idx, &recidx, &dlatidx)) continue; else {
            DLA_Entry *de = &hi->dlat[recidx>>2].DLA_Array[dlatidx];

            if (de->On_Boot_Manager_Menu) active[ii] |= 1<<idx;
         }
      }
   }
   return 1;
}

qserr _std lvm_checkinfo(u32t disk) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t       ii, idx;
   if (!hi) return E_DSK_DISKNUM;
   dsk_ptrescan(disk,0);
   if (hi->scan_rc==E_PTE_FLOPPY || hi->scan_rc==E_PTE_EMPTY) return hi->scan_rc;
   if (hi->gpt_present) return E_PTE_GPTDISK;
   if (hi->non_lvm) return E_LVM_LOWPART;
   // no serial number - no info at all
   if (!hi->lvm_snum) return E_LVM_NOINFO;

   for (ii=0; ii<hi->pt_size; ii++) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      u8t pt = hi->pts[ii].PTE_Type;
      // whole block is missing
      if (!dls->DLA_Signature1) return E_LVM_NOBLOCK;
      // check partition entry presence in DLAT
      if (pt && !IS_EXTENDED(pt)) {
         for (idx=0; idx<4; idx++)
            if (dls->DLA_Array[idx].Partition_Size==hi->pts[ii].PTE_LBASize &&
                dls->DLA_Array[idx].Partition_Start==hi->pts[ii].PTE_LBAStart)
               break;
         if (idx==4) return E_LVM_NOPART;
      }
   }

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      // check serial number
      if (dls->Disk_Serial!=hi->dlat[0].Disk_Serial)
         return E_LVM_SERIAL;
      // check CHS
      if (dls->Cylinders!=hi->dlat[0].Cylinders ||
         dls->Heads_Per_Cylinder!=hi->dlat[0].Heads_Per_Cylinder ||
            dls->Sectors_Per_Track!=hi->dlat[0].Sectors_Per_Track)
               return E_LVM_GEOMETRY;
   }
   return 0;
}


u32t _std lvm_usedletters(void) {
   FUNC_LOCK  lk;
   u32t     hdds = hlp_diskcount(0), ii, rc = 0;

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

qserr _std lvm_flushdlat(u32t disk, u32t quadidx) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   if (!hi) return E_DSK_DISKNUM;
   if (quadidx>=hi->pt_size>>2) return E_PTE_PINDEX; else {
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

      if (!hlp_diskwrite(disk, tsec, 1, sbuf)) return E_DSK_ERRWRITE;
   }
   return 0;
}

/// flush all modified DLATs (with CRC=FFFF). Function doesn`t checks anything!
qserr _std lvm_flushall(u32t disk, int force) {
   FUNC_LOCK  lk;
   u32t       ii;
   hdd_info  *hi = get_by_disk(disk);
   if (!hi) return E_DSK_DISKNUM;

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);

      if (force || dls->DLA_CRC == FFFF) {
         u32t rc = lvm_flushdlat(disk,ii>>2);
         if (rc) return rc;
      }
   }
   return 0;
}

qserr _std lvm_assignletter(u32t disk, u32t index, char letter, int force) {
   FUNC_LOCK  lk;
   u32t     ridx, didx;
   char      ltr = toupper(letter);
   qserr      rc;
   // check letter
   if (ltr && (ltr<'A' || ltr>'Z') && ltr!='*') return E_LVM_LETTER;
   if (!force)
      if (ltr && ltr!='*')
         if ((lvm_usedletters()&1<<ltr-'A')!=0) return E_LVM_LETTER;
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

qserr _std lvm_setvalue(u32t disk, u32t index, u32t vtype, u32t value) {
   u32t chk = vtype&~(LVMV_DISKSER|LVMV_BOOTSER|LVMV_VOLSER|LVMV_PARTSER) |
              (vtype&(LVMV_DISKSER|LVMV_BOOTSER)?LVMV_DISKSER:0) |
              (vtype&(LVMV_VOLSER|LVMV_PARTSER)?LVMV_VOLSER:0);
   // check drive letter
   if ((chk&LVMV_LETTER) && value && (value<'A' || value>'Z') && value!='*')
      return E_LVM_LETTER;
   // deny zero serial number
   if ((chk&(LVMV_DISKSER|LVMV_VOLSER)) && !value) return E_LVM_BADSERIAL;
   // should be only one bit of unique type
   if (!chk || bsf32(chk)!=bsr32(chk)) return E_SYS_INVPARM; else {
      FUNC_LOCK  lk;
      qserr      rc = lvm_checkinfo(disk), ii;

      if (rc) return rc;

      if (chk&LVMV_DISKSER) {
         hdd_info *hi = get_by_disk(disk);
         if (!hi) return E_DSK_DISKNUM;

         for (ii=0; ii<hi->pt_size; ii+=4) {
            DLA_Table_Sector *dls = hi->dlat + (ii>>2);
         
            if (vtype&LVMV_DISKSER) dls->Disk_Serial = value;
            if (vtype&LVMV_BOOTSER) dls->Boot_Disk_Serial = value;
         }
         rc = lvm_flushall(disk, 1);
      } else {
         u32t  ridx, didx;
         // query dlat with error code
         rc = lvm_dlatpos(disk, index, &ridx, &didx);
         if (!rc) {
            hdd_info *hi = get_by_disk(disk);
            // save data
            DLA_Table_Sector *dls = hi->dlat + (ridx>>2);
            DLA_Entry         *de = &dls->DLA_Array[didx];

            if (vtype&LVMV_VOLSER ) de->Volume_Serial = value;
            if (vtype&LVMV_PARTSER) de->Partition_Serial = value;
            if (vtype&LVMV_INBM   ) de->On_Boot_Manager_Menu = (u8t)value;
            if (vtype&LVMV_INSTALL) de->Installable = (u8t)value;
            if (vtype&LVMV_LETTER ) de->Drive_Letter = (u8t)value;
            // and flush it
            rc = lvm_flushdlat(disk, ridx>>2);
         }
      }
      return rc;
   }
}

qserr _std lvm_initdisk(u32t disk, disk_geo_data *vgeo, int separate) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t  ii, idx, pti, heads, cyls, writeflag = 0;
   int        rc, same_ser = 0, snum_commit = 0;
   if (!hi) return E_DSK_DISKNUM;
   dsk_ptrescan(disk,1);
   // check LVM info
   rc = lvm_checkinfo(disk);
   // init again on good info only if separate key is not "AUTO"
   if (rc==E_LVM_SERIAL || rc==E_LVM_GEOMETRY || rc==E_PTE_EMPTY ||
      rc==E_PTE_FLOPPY|| rc==E_PTE_GPTDISK || rc==E_LVM_LOWPART ||
         !rc && separate<0) return rc;
   // check supplied geometry
   if (vgeo)
      if (!vgeo->Cylinders || vgeo->Cylinders>65535 || !vgeo->Heads || 
         vgeo->Heads>255 || !vgeo->SectOnTrack || vgeo->SectOnTrack>255)
            return E_LVM_GEOMETRY;
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
      if (!heads || !cyls) return E_LVM_GEOMETRY;
   }

   log_it(2,"init dlat: %X, %d x %d x %d, current state %X, ser %08X, boot %08X\n", 
      disk, cyls, heads, hi->lvm_spt, rc, hi->lvm_snum, hi->lvm_bsnum);

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      // whole block is missing -> write signs here and other in statement below
      if (!dls->DLA_Signature1) {
         int  vhdd = 0;
         dls->DLA_Signature1     = DLA_TABLE_SIGNATURE1;
         dls->DLA_Signature2     = DLA_TABLE_SIGNATURE2;
         // mark this DLAT as created (in present sectors CRC is 0 after scan)
         dls->DLA_CRC            = FFFF;
         // set a special name for VHDD disks
         sprintf(dls->Disk_Name, dsk_vhddmade(disk)?"VHDD_%s":"[ %s ]",
            dsk_disktostr(disk,0));
         // signal write pass
         writeflag++;
      }
      // info fields mismatch - fix it & write
      if (dls->Sectors_Per_Track!=hi->lvm_spt || dls->Cylinders!=cyls ||
         dls->Heads_Per_Cylinder!=heads || dls->Disk_Serial!=hi->lvm_snum ||
            dls->Boot_Disk_Serial!=hi->lvm_bsnum)
      {
         dls->Disk_Serial        = hi->lvm_snum;
         dls->Boot_Disk_Serial   = hi->lvm_bsnum;
         dls->Sectors_Per_Track  = hi->lvm_spt;
         dls->Cylinders          = cyls;
         dls->Heads_Per_Cylinder = heads;
         dls->DLA_CRC            = FFFF;
         // signal write pass
         writeflag++;
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
               writeflag++;
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
                     writeflag++;
                     dls->DLA_CRC = FFFF;
                     break;
                  }
               }
         }
      }
   }
   // some of DLATs was changed, write it
   if (writeflag||snum_commit) return lvm_flushall(disk, snum_commit);

   return 0;
}

qserr _std lvm_setname(u32t disk, u32t index, u32t nametype, const char *name) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t       ii, rc;
   if (!hi) return E_DSK_DISKNUM;
   // check LVM info (rescan called here)
   rc = lvm_checkinfo(disk);
   if (rc) return rc;
   if (nametype && index>=hi->pt_view) return E_PTE_PINDEX;
   // wrong action key
   if (nametype&~(LVMN_PARTITION|LVMN_VOLUME)) return E_SYS_INVPARM;
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

qserr _std lvm_delptrec(u32t disk, u32t index) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t     ridx, dlidx;
   int        rc;
   if (!hi) return E_DSK_DISKNUM;
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

qserr _std lvm_wipedlat(u32t disk, u32t quadidx) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   if (!hi) return E_DSK_DISKNUM;
   if (quadidx>=hi->pt_size>>2) return E_PTE_PINDEX;
   if (!hi->dlat[quadidx].DLA_Signature1) return E_LVM_NOBLOCK;

   hi->dlat[quadidx].DLA_Signature1 = 0;
   hi->dlat[quadidx].DLA_Signature2 = 0;
   return lvm_flushdlat(disk,quadidx);
}

qserr _std lvm_wipeall(u32t disk) {
   FUNC_LOCK  lk;
   hdd_info  *hi = get_by_disk(disk);
   u32t   rc, ii;
   if (!hi) return E_DSK_DISKNUM;

   for (ii=0; ii<hi->pt_size; ii+=4) {
      DLA_Table_Sector *dls = hi->dlat + (ii>>2);
      memset(dls, 0, sizeof(DLA_Table_Sector));

      rc = lvm_flushdlat(disk,ii>>2);
      if (rc) return rc;
   }
   // wipe ALL missing DLATs in first 255 sectors
   ii = 0;
   do {
      ii = lvm_finddlat(disk, ii+1, 256-ii);
      if (ii) dsk_emptysector(disk, ii, 1);
   } while (ii && ii<256);
   /* force scan on next use (we can`t call it here because this is
      low level function */
   hi->inited = 0;
   return 0;
}

qserr _std lvm_findname(const char *name, u32t nametype, u32t *disk, u32t *index) {
   int  is_vol = nametype&(LVMN_PARTITION|LVMN_VOLUME) ? 1 : 0,
       is_char = nametype&LVMN_LETTER ? 1 : 0;
   // check args
   if (!disk||!name||!*name) return E_SYS_INVPARM;
   if (nametype&~(LVMN_PARTITION|LVMN_VOLUME|LVMN_LETTER)) return E_SYS_INVPARM;
   if (is_vol+is_char>1) return E_SYS_INVPARM;
   if (is_char && !isalpha(name[0])) return E_SYS_INVPARM;
      
   if (nametype!=LVMN_DISK && !index) return E_SYS_INVPARM;
   if (strlen(name)>LVM_NAME_SIZE)
      return nametype==LVMN_DISK?E_LVM_DSKNAME:E_LVM_PTNAME;

   FUNC_LOCK  lk;

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
      return nametype==LVMN_DISK?E_LVM_DSKNAME:E_LVM_PTNAME;
   } else {
      hdd_info *hi = get_by_disk(*disk);
      u32t     idx;
      char    vltr = is_char ? toupper(name[0]) : 0;
      if (!hi) return E_DSK_DISKNUM;
      *index = FFFF;
      dsk_ptrescan(*disk,0);
      if (!hi->lvm_snum) return E_LVM_NOINFO;


      for (idx=0; idx<hi->pt_size; idx++)
         if (hi->dlat[idx>>2].DLA_Signature1) {
            DLA_Entry *de = &hi->dlat[idx>>2].DLA_Array[idx&3];
            int     match = 0;

            if (vltr) {
               if (de->Drive_Letter)
                  match = toupper(de->Drive_Letter)==vltr;
            } else {
               char  *ptname = nametype&LVMN_PARTITION ? de->Partition_Name :
                               de->Volume_Name;
               match = ptname[0] && strnicmp(name,ptname,LVM_NAME_SIZE)==0;
            }
            if (match) {
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
      return E_LVM_PTNAME;
   }
}

u32t _std lvm_present(int physonly) {
   u32t dsk=FFFF, rc=0;

   FUNC_LOCK  lk;

   while (true) {
      hdd_info *hi = get_by_disk(++dsk);
      if (!hi) break;

      if (physonly && (hlp_diskmode(dsk,HDM_QUERY)&HDM_EMULATED)!=0) continue;

      dsk_ptrescan(dsk,0);
      if (hi->scan_rc==E_PTE_FLOPPY || hi->scan_rc==E_PTE_EMPTY) continue;
      if (hi->gpt_present || hi->non_lvm || !hi->lvm_snum) continue;
      // accept any major LVM error if we have serial number after scan code
      if (dsk>=31) { rc|=0x80000000; break; } else rc|=1<<dsk;
   }
   return rc;
}

qserr _std lvm_newserials(u32t disk, u32t disk_serial, u32t boot_serial, int genpt) {
   FUNC_LOCK lk;
   int       rc = lvm_checkinfo(disk);
   // deny any errors here
   if (!rc) {
      u32t ii, pti;
      hdd_info *hi = get_by_disk(disk);
      if (!hi) return E_DSK_DISKNUM;

      if (!disk_serial) disk_serial = random(65356) | random(65356)<<16;
      if (!boot_serial) 
         boot_serial = hi->lvm_snum==hi->lvm_bsnum ? disk_serial : hi->lvm_bsnum;
      // nothing to do?
      if (disk_serial==hi->lvm_snum && boot_serial==hi->lvm_bsnum && !genpt)
         return 0;
      // enum records
      for (ii=0; ii<hi->pt_size; ii+=4) {
         DLA_Table_Sector *dls = hi->dlat + (ii>>2);

         if (genpt) {
            for (pti=0; pti<4; pti++) {
               DLA_Entry *de = &dls->DLA_Array[pti];
            
               if (de->Partition_Size && de->Partition_Start) {
                  de->Volume_Serial    = random(65356) | random(65356)<<16;
                  de->Partition_Serial = random(65356) | random(65356)<<16;
               }
            }
         }
         dls->Disk_Serial      = disk_serial;
         dls->Boot_Disk_Serial = boot_serial;
         rc = lvm_flushdlat(disk,ii>>2);
         if (rc) return rc;
      }
      rc = 0;
   }
   return rc; 
}
