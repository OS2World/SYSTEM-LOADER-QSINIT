//
// QSINIT
// disk/partition cloning
//
#include "pscan.h"
#include "stdlib.h"
#include "direct.h"
#include "errno.h"
#include "lvm.h"
#include "qcl/qsedinfo.h"

// try to change CHS geo of "virtual disk"
int dsk_clonechs(u32t dstdisk, disk_geo_data *srcgeo) {
   disk_geo_data geo1, geo2;
   qs_extdisk   extp = hlp_diskclass(dstdisk, 0);
   // disk have no extended control class?
   if (!extp) return 0;
   if (extp->getgeo(&geo1)) return 0;

   geo2.SectOnTrack  = srcgeo->SectOnTrack;
   geo2.Heads        = srcgeo->Heads;
   geo2.Cylinders    = geo1.TotalSectors/geo2.Heads/geo2.SectOnTrack;
   geo2.SectorSize   = geo1.SectorSize;
   geo2.TotalSectors = geo1.TotalSectors;
   if (geo2.Cylinders>65520) geo2.Cylinders = 65520;

   log_it(2,"changing %s chs from %d x %d x %d to %d x %d x %d\n",
      dsk_disktostr(dstdisk,0), geo1.Cylinders, geo1.Heads, geo1.SectOnTrack, 
         geo2.Cylinders, geo2.Heads, geo2.SectOnTrack);
   if (extp->setgeo(&geo2)) return 0;
   return 1;
}

u32t _std dsk_clonestruct(u32t dstdisk, u32t srcdisk, u32t flags) {
   hdd_info  *hS, *hD;
   dsk_mapblock  *map, *mp;
   u32t        rc, ii;
   u64t        reqlen, 
            firstused;
   // same disk?
   if (dstdisk==srcdisk) return DPTE_INVARGS;
   hS = get_by_disk(srcdisk);
   hD = get_by_disk(dstdisk);
   if (!hS || !hD) return DPTE_INVDISK;
   // force rescan of both disks
   rc = dsk_ptrescan(dstdisk, 1);
   if (rc && rc!=DPTE_EMPTY) return rc;
   if (!rc) return DPTE_CSPACE;
   rc = dsk_ptrescan(srcdisk, 1);
   if (rc) return rc;
   // get sorted disk map 
   map = dsk_getmap(srcdisk);
   if (!map) return DPTE_INVDISK;
   // hybrid, too crazy to clone ;)
   if (hS->hybrid) return DPTE_HYBRID;
   // sector size mismatch, fatal.
   if (hS->info.SectorSize!=hD->info.SectorSize) return DPTE_INCOMPAT;
   // BIOS sectors per track is not equal!
   if (hS->info.SectOnTrack!=hD->info.SectOnTrack)
      if (dsk_clonechs(dstdisk, &hS->info)) dsk_ptrescan(dstdisk, 1);
   if (hS->info.SectOnTrack!=hD->info.SectOnTrack && (flags&DCLN_IGNSPT)==0)
      return DPTE_INCOMPAT;
   /* get last used sector, but without extended partition size, it will be
      adjusting below, for DCLN_IDENT option only */
   dsk_usedspace(srcdisk, &firstused, &reqlen);
   if (reqlen==FFFF64) reqlen = 0; else reqlen++;

   if (hS->gpt_size) {
      /* if disk is empty we need space only for 2 headers, else 1st header
         must be below user partitions */
      reqlen += (hS->gpt_sectors + 1) * (reqlen?1:2);
   } else {
      if (!reqlen) reqlen = hS->info.SectOnTrack;
      // if we use direct copying - size must include entire extended`s size
      if ((flags&DCLN_IDENT) && hS->extpos)
         if (reqlen < hS->extpos+hS->extlen) reqlen = hS->extpos+hS->extlen;
   }
   do {
      // no space
      if (reqlen>hD->info.TotalSectors) { rc = DPTE_CSPACE; break; }

      if (hS->gpt_size) {
         // remove IDENT in GPT mode to not confuse code below
         flags &= ~DCLN_IDENT;
         // init GPT
         rc = dsk_gptinit(dstdisk);
         if (rc) break;
         /* clone GPT partitions.
            Actually, GPT cloning is wrong - it just creating partitions one
            by one on the same pos, but places headers to default position,
            which can be not the equal on source disk */
         mp = map;
         do {
            if (mp->Type) {
               dsk_gptpartinfo sinfo, dinfo;
               // create partition
               rc = dsk_gptcreate(dstdisk, mp->StartSector, mp->Length,
                  DFBA_PRIMARY, 0);
               if (rc) break;
               // combine source & destin partition info
               rc = dsk_gptpinfo(srcdisk, mp->Index, &sinfo);
               if (rc) break;
               rc = dsk_gptpinfo(dstdisk, mp->Index, &dinfo);
               if (rc) break;
               memcpy(&dinfo.TypeGUID, &sinfo.TypeGUID, 16);
               if (sinfo.Name[0])
                  memcpy(&dinfo.Name, &sinfo.Name, sizeof(sinfo.Name));
               dinfo.Attr = sinfo.Attr;
               if ((flags&DCLN_SAMEIDS)) memcpy(&dinfo.GUID, &sinfo.GUID, 16);
               // update partition info
               rc = dsk_gptpset(dstdisk, mp->Index, &dinfo);
            }
         } while (!rc && (mp++->Flags&DMAP_LEND)==0);
         if (rc) break;
         // clone source disk GUID if same IDs requested.
         if ((flags&DCLN_SAMEIDS)) {
            dsk_gptdiskinfo dinfo, sinfo;
            rc = dsk_gptdinfo(srcdisk, &sinfo);
            if (!rc) rc = dsk_gptdinfo(dstdisk, &dinfo);
            if (!rc) {
               memcpy(&dinfo.GUID, &sinfo.GUID, sizeof(sinfo.GUID));
               if (!rc) rc = dsk_gptdset(dstdisk, &dinfo);
            }
            // ignore this!
            if (rc) rc = 0;
         }
      } else
      /* MBR code: DCLN_IDENT will force copying of all MBR/LVM sectors and
         adjusting data in it, without this flag - partitions created one by
         one - at original positions, but as result - disk structure can be
         different */
      if ((flags&DCLN_IDENT)) {
         // scan for old LVM info
         lvm_wipeall(dstdisk);
         // copy MBR areas
         if (dsk_copysector(dstdisk, 0, srcdisk, 0, hS->lvm_spt, 0, 0, 0, &rc) <
            hS->lvm_spt) break;
         for (ii=1; ii<hS->pt_size>>2; ii++)
           if (dsk_copysector(dstdisk, hS->ptspos[ii], srcdisk, hS->ptspos[ii],
              hS->lvm_spt, 0, 0, 0, &rc) < hS->lvm_spt) break;
         // rescan disk after MBR/LVM creation
         rc = dsk_ptrescan(dstdisk, 1);
         if (rc) break;
         // reinit LVM if detected one - with fixing CHS values
         if (hD->lvm_snum) {
            disk_geo_data  geo;
            // query current LVM geo & adjust number of cylinders
            if (dsk_getptgeo(dstdisk,&geo)==0) {
               int    lvmrc;
               /* leave heads & spt the same as it was - else partition`s
                  cylinder alignment will be broken */
               geo.Cylinders = geo.TotalSectors/geo.Heads/geo.SectOnTrack;
               if (geo.Cylinders>65535) geo.Cylinders = 65535;
               // init with custom geo (used only here)
               lvmrc = lvm_initdisk(dstdisk, &geo, hD->lvm_snum==hD->lvm_bsnum?1:0);
               // ignore LVM errors!
               if (!lvmrc && (flags&DCLN_SAMEIDS)==0)
                  lvmrc = lvm_newserials(dstdisk, 0, 0, 1);
               if (lvmrc) log_printf("lvm upd rc = %d\n",lvmrc);
               
               rc = dsk_ptrescan(dstdisk, 1);
            }
         }
         // change disk id in partition table if IDs is not the same
         if ((flags&DCLN_SAMEIDS)==0) {
            struct Disk_MBR *mbr = (struct Disk_MBR*)malloc(hD->info.SectorSize);

            if (dsk_sectortype(dstdisk,0,(u8t*)mbr)==DSKST_PTABLE) {
               mbr->MBR_DiskID = random(0x10000)<<16|random(0x10000);
               if (!hlp_diskwrite(dstdisk, 0, 1, mbr)) rc = DPTE_ERRWRITE;
            }
            free(mbr);
         }
      } else {
         // create new MBR
         if (!dsk_newmbr(dstdisk,DSKBR_CLEARPT|DSKBR_GENDISKID))
            { rc = DPTE_ERRWRITE; break; }
         /* copy MBR area sectors (1..63), if no partitions below sector #63,
            but ignore error code - its not critical */
         if (firstused>=(u64t)hS->lvm_spt)
            dsk_copysector(dstdisk, 1, srcdisk, 1, hS->lvm_spt-1, 0, 0, 0, 0);
         /* wipe any founded LVM sectors (we can get it from source at line
            above or from original disk data */
         lvm_wipeall(dstdisk);
         /* init LVM in common way (to calc valid CHS), then copy some fields
            and check sectors per track match - it is required for LVM mode */
         if (hS->lvm_snum) {
            int lvmrc = lvm_initdisk(dstdisk, 0, 1);
            if (lvmrc) {
               log_printf("lvm create rc = %d\n",lvmrc);
               rc = DPTE_ERRWRITE;
               break;
            }
            rc = dsk_ptrescan(dstdisk, 1);
            if (rc) break;
            if (hD->dlat) {
               // we must have equal spts!
               if (hS->lvm_spt!=hD->lvm_spt) { 
                  rc = DPTE_INCOMPAT; 
                  break; 
               } else {
                  /* here we just updating first (and single) DLAT sector - LVM
                     guarantee using it as base for later ops after forced
                     rescan below */
                  DLA_Table_Sector *dls = hS->dlat+0,
                                   *dld = hD->dlat+0;
                  // we have difference in number of heads, this is BAD
                  if (dls->Heads_Per_Cylinder!=dld->Heads_Per_Cylinder) {
                     u32t cyls = hD->info.TotalSectors/hD->lvm_spt/dls->Heads_Per_Cylinder;
                     if (cyls<=65535) {
                        dld->Heads_Per_Cylinder = dls->Heads_Per_Cylinder;
                        dld->Cylinders          = cyls;
                     }
                  }
                  // copy IDs
                  if ((flags&DCLN_SAMEIDS)) {
                     dld->Disk_Serial      = dls->Disk_Serial;
                     dld->Boot_Disk_Serial = dls->Boot_Disk_Serial;
                  }
                  dld->Install_Flags = dls->Install_Flags;
                  dld->Reboot        = dls->Reboot;
                  memcpy(&dld->Disk_Name, &dls->Disk_Name, sizeof(dls->Disk_Name));
                  // hope it will succeed, but ignore non-critical rc
                  lvm_flushdlat(dstdisk, 0);
               }
            }
         }
         // rescan disk after MBR/LVM creation
         rc = dsk_ptrescan(dstdisk, 1);
         if (!rc) {
            // clone MBR partitions
            u32t  pend = hD->lvm_spt;
            u8t  ptype = 0xFF, l1st = 1;
            mp   = map;
            do {
               if (mp->Type) {
                  u32t start = mp->StartSector,
                        size = mp->Length;
                  /* fix one trouble in logical creation - dsk_ptcreate()
                     shrinks partition size to get space for extended MBR
                     record when it creates it.
                     So - for 1st logical partition and logical partition after
                     free space - we must step back by SPT to make identical
                     sector values */
                  if ((mp->Flags&DMAP_PRIMARY)==0 && (l1st || !ptype)) {
                     if (start>hD->lvm_spt && pend<=start-hD->lvm_spt) {
                        start -= hD->lvm_spt;
                        size  += hD->lvm_spt;
                     }
                     l1st = 0;
                  }
                  rc = dsk_ptcreate(dstdisk, start, size, mp->Flags&DMAP_PRIMARY ?
                     DFBA_PRIMARY:DFBA_LOGICAL, mp->Type);
                  if (rc) break;
                  // make it active, but errors ignored (not critical)
                  if (mp->Flags&DMAP_ACTIVE) dsk_setactive(dstdisk, mp->Index);
                  // end of used at this moment
                  pend = start + size;
               }
               ptype = mp->Type;
            } while (!rc && (mp++->Flags&DMAP_LEND)==0);
         }
         if (rc) break;
         // partitions ready - copy LVM serials & names
         if (hD->lvm_snum) {
            int lvmrc;
            for (ii=0; ii< hD->pt_view; ii++) {
               u32t sridx, sidx, dridx, didx;
               if (lvm_dlatpos(dstdisk, ii, &dridx, &didx)==0 &&
                   lvm_dlatpos(srcdisk, ii, &sridx, &sidx)==0)
               {
                   DLA_Entry *se = &hS->dlat[sridx>>2].DLA_Array[sidx],
                             *de = &hD->dlat[dridx>>2].DLA_Array[didx];
                   if ((flags&DCLN_SAMEIDS)) {
                      de->Volume_Serial    = se->Volume_Serial;
                      de->Partition_Serial = se->Partition_Serial;
                   }
                   memcpy(&de->On_Boot_Manager_Menu, &se->On_Boot_Manager_Menu,
                      sizeof(DLA_Entry) - FIELDOFFSET(DLA_Entry,On_Boot_Manager_Menu));
               }
            }
            lvmrc = lvm_flushall(dstdisk, 1);
            if (lvmrc) {
               log_printf("lvm upd rc = %d\n",lvmrc);
               rc = DPTE_ERRWRITE;
            }
         }
      }
      if (rc) break;
      // common for GPT & MBR: update MBR code/IDs
      if ((flags&DCLN_IDENT)==0 && (flags&(DCLN_SAMEIDS|DCLN_MBRCODE))) {
         struct Disk_MBR *smbr = (struct Disk_MBR*)malloc(hS->info.SectorSize),
                         *dmbr = (struct Disk_MBR*)malloc(hD->info.SectorSize);
         if (!hlp_diskread(srcdisk, 0, 1, smbr)) rc = DPTE_ERRREAD;
         if (!hlp_diskread(dstdisk, 0, 1, dmbr)) rc = DPTE_ERRREAD;

         if (!rc) {
            if ((flags&DCLN_MBRCODE))
               memcpy(&dmbr->MBR_Code, &smbr->MBR_Code, sizeof(smbr->MBR_Code));
            if ((flags&DCLN_SAMEIDS)) {
               dmbr->MBR_DiskID   = smbr->MBR_DiskID;
               dmbr->MBR_Reserved = smbr->MBR_Reserved;
            }
            if (!hlp_diskwrite(dstdisk, 0, 1, dmbr)) rc = DPTE_ERRWRITE;
         }
         free(dmbr); free(smbr);
      }
      if (rc) break;
      /* enum partitions & zeroing boot sector in every one - to wipe possible
         remains of previous target disk`s structures */
      if ((flags&DCLN_NOWIPE)==0)
         for (ii=0; ii< hD->pt_view; ii++) {
            u64t  pos, len;
            if (dsk_ptquery64(dstdisk, ii, &pos, &len, 0, 0))
               if (len>0) dsk_emptysector(dstdisk, pos, 1);
         }
   } while (false);
   free(map);

   return rc;
}

static int _std break_copy(void *info) {
   if (confirm_dlg("Break copying process?^Target partition will be unusable!"))
      return 1;
   return 0;
}

u32t  _std dsk_clonedata(u32t dstdisk, u32t dstindex, u32t srcdisk, 
   u32t srcindex, read_callback cbprint, u32t flags)
{
   hdd_info          *hS, *hD;
   u32t            rc, dflags, 
             hidden_upd, svol;
   u64t      spos, slen, dpos,
                         dlen;
   u8t           stype, dtype;
   struct Boot_Record     *br;

   if (dstdisk==srcdisk && dstindex==srcindex) return DPTE_INVARGS;
   hS = get_by_disk(srcdisk);
   hD = get_by_disk(dstdisk);
   if (!hS || !hD) return DPTE_INVDISK;
   // force rescan of both disks
   rc = dsk_ptrescan(dstdisk, 1);
   if (rc) return rc;
   rc = dsk_ptrescan(srcdisk, 1);
   if (rc) {
      // ignore source disk scan errors (this allow to copy founded partitions)
      log_printf("warning! source disk scan = %d\n", rc);
      rc = 0;
   }
   // sector size mismatch, fatal.
   if (hS->info.SectorSize!=hD->info.SectorSize) return DPTE_INCOMPAT;
   // query size/pos
   stype = dsk_ptquery64(srcdisk, srcindex, &spos, &slen, 0, 0);
   if (stype==0) return DPTE_PINDEX;
   dtype = dsk_ptquery64(dstdisk, dstindex, &dpos, &dlen, 0, &dflags);
   if (dtype==0) return DPTE_PINDEX;
   if (slen>dlen) return DPTE_CSPACE;
   // may be sometimes later ;)
   if (slen>=_4GBLL) return DPTE_LARGE;

   /* check destination - is it mounted?
      deny boot partition and unmount any other */
   svol = dsk_ismounted(dstdisk, dpos);
   if (svol==QDSK_VOLUME) return DPTE_BOOTPT;
   if (svol&QDSK_VOLUME) 
      if (!hlp_unmountvol(svol&QDSK_DISKMASK)) return DPTE_UMOUNT;

   dlen = dsk_copysector(dstdisk, dpos, srcdisk, spos, slen, cbprint, 
                         flags&DCLD_NOBREAK?0:break_copy, 0, &rc);
   if (dlen<slen) return rc;

   rc = 0; hidden_upd = 0; 
   br = 0;
   // update target partition`s BPB
   if ((flags&DCLD_SKIPBPB)==0) {
      u32t  bstype;
      br     = (struct Boot_Record*)malloc(MAX_SECTOR_SIZE);
      bstype = dsk_sectortype(dstdisk, dpos, (u8t*)br);
      /* NOT touching total number of sectors in BPB here because of two
         reasons:
         * NTFS/ExFat have own 8 byte field at offset 72 and half-compatible
           BPB format.
         * some hypotetic code can depends on this value (i.e. check some
           FS structures and so on) 
         So, updating HiddenSectors & CHS values only (if it possible). */
      if (bstype==DSKST_BOOTFAT || bstype==DSKST_BOOTBPB) {
         disk_geo_data  geo;
         u32t   wrt = 0, nv = dpos>=_4GBLL ? FFFF : dpos;
         /* destination is logical partition - we must query hidden sectors
            value in special way */
         if ((dflags&DPTF_PRIMARY)==0)
            dsk_ptquery(dstdisk, dstindex, 0, 0, 0, 0, &nv);
         // CHS changed?
         if (dsk_getptgeo(dstdisk, &geo)==0) {
            if (br->BR_BPB.BPB_SecPerTrack != geo.SectOnTrack) {
               br->BR_BPB.BPB_SecPerTrack = geo.SectOnTrack;
               wrt++;
            }
            if (br->BR_BPB.BPB_Heads != geo.Heads) {
               br->BR_BPB.BPB_Heads = geo.Heads;
               wrt++;
            }
         }
         if (nv!=br->BR_BPB.BPB_HiddenSec) {
            br->BR_BPB.BPB_HiddenSec = nv;
            wrt++; 
            // flag for boot.ini update below (it must be non-zero)
            hidden_upd = bstype==DSKST_BOOTFAT ? nv : 0;
         }
         // update BPB
         if (wrt)
            if (!hlp_diskwrite(dstdisk, dpos, 1, br)) rc = DPTE_ERRWRITE;
      }
   }
   // change partition type on MBR disks (and ignore result of this action)
   if (dsk_isgpt(dstdisk,-1)==0 && stype!=dtype)
      dsk_setpttype(dstdisk, dstindex, stype);

   // update boot sectors, listed in boot.ini on FAT/FAT32 ;)
   if (hidden_upd) {
      u8t vol = 0;
      // mounts destination temporary and checks boot.ini, but ignore any errors
      if (vol_mount(&vol, dstdisk, dstindex)==0) {
         u32t vt = hlp_volinfo(vol, 0);
         if (vt==FST_FAT12 || vt==FST_FAT16 || vt==FST_FAT32) {
            char    path[12];
            str_list  *oslst;
            sprintf(path, "%c:\\BOOT.INI", vol+'A');

            oslst = str_keylist(path, "operating systems", 0);
            if (oslst) {
               u32t ii;
               for (ii=0; ii<oslst->count; ii++) {
                  if (strnicmp(oslst->item[ii], "C:\\", 3)==0) {
                     char *bsp = sprintf_dyn("%c:\\%s", vol+'A', oslst->item[ii][3]
                                             ? oslst->item[ii]+3 : "BOOTSECT.DOS");
                     dir_t  fi;
                     u16t  err = _dos_stat(bsp, &fi);
                     if (!err) {
                        if (fi.d_size == hD->info.SectorSize) {
                           FILE *fb;
                           // drop read-only for a while
                           if (fi.d_attr&_A_RDONLY) _dos_setfileattr(bsp,_A_ARCH);
                           // update sector
                           fb = fopen(bsp, "r+b");
                           if (!fb) err = errno; else {
                              if (fread(br,1,fi.d_size,fb)==fi.d_size) {
                                 br->BR_BPB.BPB_HiddenSec = hidden_upd;
                                 rewind(fb);
                                 if (fwrite(br,1,fi.d_size,fb)!=fi.d_size) err = errno;
                              } else err = errno;
                              fclose(fb);
                              if (!err) log_printf("\"%s\" updated\n", bsp);
                           }
                           // restore attributes
                           if (fi.d_attr&_A_RDONLY) _dos_setfileattr(bsp,fi.d_attr);
                        } else
                           log_printf("\"%s\" size mismatch (%u bytes)\n", bsp,
                              hD->info.SectorSize);
                     }
                     if (err) log_printf("error %h on updating \"%s\"\n", err, bsp);
                     free(bsp);
                  }
               }
               free(oslst);
            }
         }
         hlp_unmountvol(vol);
      }
   }
   // free sector buffer
   if (br) free(br);

   return rc;
}
