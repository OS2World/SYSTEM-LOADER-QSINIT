//
// QSINIT
// partition management: mbr & boot sector code
//
#include "dpmi.h"              // must be before qsutil.h
#include "stdlib.h"
#include "qsshell.h"
#include "errno.h"
#include "qsdm.h"
#include "vio.h"
#include "pscan.h"
#include "sys/fs/hpfs.h"         // for HPFSSEC_SUPERB only

#define MBR_LOAD_ADDR    0x7C00

extern char     psect[512],    ///< MBR code
              gptsect[512];
extern char   bsect16[512],    ///< VBR code
          bsect16_name[11],    ///< boot file name (for calculation)
              bsect32[512],
          bsect32_name[11],
              bsectdb[512];
extern char   hpfs_bsect[],    ///< HPFS partition boot sector + micro-FSD code
           hpfs_bsname[16];
extern char     bsectexf[],
         bsectexf_name[16];
extern u32t   hpfs_bscount;    ///< # of sectors in array above
extern u8t    exf_bsacount;    ///< # of addtional sectors in exFAT array

int _std dsk_newmbr(u32t disk, u32t flags) {
   char   mbr_buffer[MAX_SECTOR_SIZE];
   u32t   sectorsize;
   struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;
   // query sector size
   if (!hlp_disksize64(disk, &sectorsize)) return 0;
   if (sectorsize>MAX_SECTOR_SIZE) return 0;

   memset(mbr, 0, sectorsize);

   if ((flags&DSKBR_CLEARALL)==0) {
      memcpy(mbr, flags&DSKBR_GPTCODE ? &gptsect : &psect, 512);
      // put 55AA ti the end of sector too
      if (sectorsize>512) {
         mbr_buffer[sectorsize-2] = 0x55;
         mbr_buffer[sectorsize-1] = 0xAA;
      }
   }
   if (flags&DSKBR_GENDISKID) {
      u32t id = random(0x10000);
      mbr->MBR_DiskID = id<<16|id;
   }
   if (flags&DSKBR_GPTHEAD) {
      struct MBR_Record *rec0 = mbr->MBR_Table;
      u64t len = hlp_disksize64(disk, 0);
      if (!len) return 0;
      // single GPT stub record
      rec0->PTE_LBAStart = 1;
      rec0->PTE_LBASize  = len>=_4GBLL ? FFFF : len - 1;
      rec0->PTE_CSStart  = 2;
      rec0->PTE_HStart   = 0;
      rec0->PTE_CSEnd    = 0xFFFF;
      rec0->PTE_HEnd     = 0xFE;
      rec0->PTE_Type     = PTE_EE_UEFI;
   } else
   if ((flags&DSKBR_CLEARPT)==0) {
      char   oldmbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR *oldmbr = (struct Disk_MBR *)&oldmbr_buffer;
      if (!hlp_diskread(disk, 0, 1, oldmbr)) return 0;
      memcpy(&mbr->MBR_Table, &oldmbr->MBR_Table, sizeof(oldmbr->MBR_Table));
      mbr->MBR_DiskID   = oldmbr->MBR_DiskID;
      mbr->MBR_Reserved = oldmbr->MBR_Reserved;
   }
   // unmount all volumes from this disk if PT will be cleaned
   if ((flags&(DSKBR_CLEARALL|DSKBR_CLEARPT|DSKBR_GPTHEAD))!=0) hlp_unmountall(disk);
   if (flags&DSKBR_LVMINFO) lvm_wipeall(disk);

   // AND write it
   return hlp_diskwrite(disk, 0, 1, mbr);
}

int _std dsk_wipe55aa(u32t disk, u64t sector) {
   char   buffer[MAX_SECTOR_SIZE];
   u32t    ssize = 0;
   if (!hlp_diskread(disk, sector, 1, buffer)) return 0;
   hlp_disksize(disk, &ssize, 0);
   //  it is recommended to write 55AA to 510 AND last two bytes of other size
   if (ssize && ssize>512) {
      buffer[ssize-2] = 0;
      buffer[ssize-1] = 0;
   }
   buffer[510] = 0;
   buffer[511] = 0;
   return hlp_diskwrite(disk, sector, 1, buffer);
}

int dsk_wipevbs(u32t disk, u64t sector) {
   struct Boot_Record *br = (struct Boot_Record*)malloc_thread(MAX_SECTOR_SIZE);
   u32t st = dsk_sectortype(disk, sector, (u8t*)br);
   u32t ssize, rc;
   hlp_disksize(disk, &ssize, 0);

   do {
      if (st==DSKST_ERROR) { rc = 0; break; }
      if (st==DSKST_EMPTY) { rc = 1; break; }
      // wipe 55AA as in function above
      if (ssize && ssize>512) {
         ((u8t*)br)[ssize-2] = 0;
         ((u8t*)br)[ssize-1] = 0;
      }
      ((u8t*)br)[510] = 0;
      ((u8t*)br)[511] = 0;
      // wipe FS name
      if (st==DSKST_BOOTFAT && br->BR_BPB.BPB_RootEntries==0)
         memset(&((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_FSType, 0, 8);
      else
      if (st==DSKST_BOOTFAT || st==DSKST_BOOTBPB)
         memset(&br->BR_EBPB.EBPB_FSType, 0, 8);
      // clear OEM (NTFS, EXFAT strings)
      memset(&br->BR_OEM, 0, 8);
      // write it back
      rc = hlp_diskwrite(disk, sector, 1, br);
   } while (false);

   free(br);
   return rc;
}

int _std exit_bootmbr(u32t disk, u32t flags) {
   if (hlp_hosttype()==QSHT_EFI) return ENOSYS; else
   if (disk&(QDSK_FLOPPY|QDSK_VOLUME)) return EINVAL; else {
      char   mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;
      struct rmcallregs_s regs;
      int        ii, isgpt;
      u8t         biosdisk = hlp_diskbios(disk,1);
      // is it emulated?
      if ((hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)!=0) return ENOTBLK;
      // read mbr
      if (!hlp_diskread(disk, 0, 1, mbr)) return EIO;
      isgpt = dsk_isgpt(disk,-1)>0?1:0;
      log_printf("MBR boot: disk %02X(%02X), flags %04X, gpt %i\n", disk,
         biosdisk, flags, isgpt);
      // check for active bit presence
      if (!isgpt && (flags&EMBR_NOACTIVE)==0) {
         for (ii=0;ii<4;ii++)
            if (mbr->MBR_Table[ii].PTE_Active>=0x80) break;
         if (ii==4) {
            log_printf("empty partition table!\n");
            return ENODEV;
         }
      }
      // copy own code
      if (flags&EMBR_OWNCODE)
         memcpy(&mbr->MBR_Code, isgpt?&gptsect:&psect, sizeof(mbr->MBR_Code));
      // setup DPMI
      memset(&regs, 0, sizeof(regs));
      regs.r_ip  = MBR_LOAD_ADDR;
      regs.r_edx = hlp_diskbios(disk,1);
      // shutdown all and go boot
      vio_clearscr();
      exit_prepare();
      // place to 0:07C00
      memcpy((char*)hlp_segtoflat(0)+MBR_LOAD_ADDR, mbr, 512);

      hlp_rmcallreg(-1, &regs, RMC_EXITCALL);
   }
   return 0;
}

int _std exit_bootvbr(u32t disk, long index, char letter, void *sector) {
   if (hlp_hosttype()==QSHT_EFI) return ENOSYS; else
   if (disk&QDSK_VOLUME) return EINVAL; else {
      u8t    vbr_buffer[MAX_SECTOR_SIZE];
      struct Boot_Record *br = (struct Boot_Record *)&vbr_buffer;
      struct rmcallregs_s regs;
      u32t    bstype, dmode, sectsize;
      u64t     start;
      int         ii,
              floppy = disk&QDSK_FLOPPY?1:0;
      u8t   biosdisk;

      if (!floppy) {
         u8t   type;
         if (index<0) return EINVAL;
         type = dsk_ptquery64(disk, index, &start, 0, 0, 0);
         if (!type || IS_EXTENDED(type)) return EINVAL;
      } else
         start = 0;
      // query current disk access mode
      dmode = hlp_diskmode(disk,HDM_QUERY);
      if ((dmode&HDM_EMULATED)!=0) return ENOTBLK;
      // get sector size
      hlp_disksize(disk, &sectsize, 0);

      if (sector) {
         // use safe memcpy
         if (!hlp_memcpy(vbr_buffer, sector, sectsize, 0)) return EFAULT;
         bstype = dsk_datatype(vbr_buffer, sectsize, disk, start==0);
      } else
         bstype = dsk_sectortype(disk, start, vbr_buffer);

      if (bstype==DSKST_ERROR) return EIO;
      if (bstype==DSKST_EMPTY) return ENODEV;
      /* allow boot on 2Tb border, only 1st sector must be below.
         At least our`s FAT32 & HPFS boot code can do this.
         For exFAT - allow to boot everywhere, it has 64-bit volume
         position field in own BPB */
      if (start>=_4GBLL && bstype!=DSKST_BOOTEXF) return EFBIG;
      // query volume letter from DLAT/GPT data
      if (!letter && !floppy) letter = lvm_ismounted(disk, index);
      if (letter) letter = toupper(letter);
      if (letter<'C') letter = 0;

      biosdisk = hlp_diskbios(disk,1);

      if (bstype==DSKST_BOOTFAT || bstype==DSKST_BOOTBPB) {
         /* update phys.disk and hidden sectors field.
            values updated only if BPB presence detected */
         br->BR_BPB.BPB_HiddenSec = start;

         // FAT32?
         if (bstype==DSKST_BOOTFAT && br->BR_BPB.BPB_RootEntries==0)
            ((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_PhysDisk = biosdisk;
         else {
            u8t sign = br->BR_EBPB.EBPB_Sign;
            // check signature
            if (sign==0x28 || sign==0x29) {
               br->BR_EBPB.EBPB_PhysDisk = biosdisk;
               if (letter) br->BR_EBPB.EBPB_Dirty = (u32t)letter - 'C' + 0x80;
                  else br->BR_EBPB.EBPB_Dirty = 0;
            }
         }
      } else
      if (bstype==DSKST_BOOTEXF) {
         struct Boot_RecExFAT *bre = (struct Boot_RecExFAT *)&vbr_buffer;
         bre->BR_ExF_VolStart = start;
         bre->BR_ExF_PhysDisk = biosdisk;
      }
      log_printf("VBR boot: disk %02X(%02X), sector %08LX, letter %c, %s\n",
         disk, biosdisk, start, letter?letter:'-', dmode&HDM_USELBA?"lba":"chs");

      // write I13X for IBM boot sectors
      if (dmode&HDM_USELBA) {
         u32t *p_i13x = (u32t*)hlp_segtoflat(0x3000);
         *p_i13x = 0x58333149;
      }
      /* makes Boot Manager happy:
         this signature (in 7C00+200) forces it to use disk number from dl */
      {
         struct Disk_MBR *mbr = (struct Disk_MBR*)hlp_segtoflat(MBR_LOAD_ADDR+sectsize >> 4);
         mbr->MBR_Reserved    = 0xCC33;
      }
      // setup DPMI
      memset(&regs, 0, sizeof(regs));
      regs.r_ip  = MBR_LOAD_ADDR;
      regs.r_edx = biosdisk;
      // shutdown all and go boot
      vio_clearscr();
      exit_prepare();
      // place to 0:07C00
      memcpy((char*)hlp_segtoflat(0)+MBR_LOAD_ADDR, br, sectsize);
      // one way call!
      hlp_rmcallreg(-1, &regs, RMC_EXITCALL);
   }
   return 0;
}

u32t _std dsk_datatype(u8t *sectordata, u32t sectorsize, u32t disk, int is0) {
   if (!sectordata || sectorsize!=512 && sectorsize!=1024 && sectorsize!=2048
      && sectorsize!=4096) return DSKST_ERROR;
   else {
      struct Disk_MBR       *mbr = (struct Disk_MBR *)sectordata;
      struct Boot_Record    *btw = (struct Boot_Record *)sectordata;
      struct Boot_RecordF32 *btd = (struct Boot_RecordF32 *)sectordata;
      u32t rc;

      do {
         if (mbr->MBR_Signature==0xAA55) {
            u16t bps  = btw->BR_BPB.BPB_BytePerSect;
            int  boot = 0, pt = 0, fat = 0, ii, bpb = 0, exf = 0;
            // byte per sector valid?
            if (bps == 512 || bps == 1024 || bps == 2048 || bps == 4096) { boot++; bpb++; }
            // is FAT boot record
            if (strncmp(btw->BR_BPB.BPB_RootEntries==0?btd->BR_F32_EBPB.EBPB_FSType:
               btw->BR_EBPB.EBPB_FSType,"FAT",3)==0) { boot+=2; fat++; }
            // NTFS?
            if (strnicmp(btw->BR_OEM,"NTFS",4)==0) boot+=100; else
            // exFAT?
            if (strnicmp(btw->BR_OEM,"EXFAT",5)==0) { boot+=100; exf = 1; }
            // HDD sector 0
            if (is0 && (disk&(QDSK_FLOPPY|QDSK_VOLUME))==0) pt++;
            // boot partition
            if (is0 && (disk&QDSK_VOLUME)!=0) boot+=100;
            // check MBR
            for (ii=0;ii<4;ii++) {
               struct MBR_Record *ptr = mbr->MBR_Table+ii;
               if (ptr->PTE_Active<0x80 && ptr->PTE_Active!=0) break;
               if (ptr->PTE_Type && (!ptr->PTE_LBAStart || !ptr->PTE_LBASize)) break;
               // filled by FF
               if (ptr->PTE_Type==0xFF && ptr->PTE_LBAStart==FFFF) break;
               if (ii==3) pt++;
            }
            if (pt>boot) { rc = DSKST_PTABLE; break; }
            if (fat)     { rc = DSKST_BOOTFAT; break; }
            if (exf)     { rc = DSKST_BOOTEXF; break; }
            rc = bpb?DSKST_BOOTBPB:DSKST_BOOT;
         } else {
            struct Disk_GPT *pt = (struct Disk_GPT *)sectordata;

            if (pt->GPT_Sign==GPT_SIGNMAIN && pt->GPT_PtEntrySize==GPT_RECSIZE &&
               pt->GPT_HdrSize>=sizeof(struct Disk_GPT)) rc = DSKST_GPTHEAD;
            else {
               rc = memchrnb(sectordata,0,sectorsize)?DSKST_DATA:DSKST_EMPTY;
               if (rc==DSKST_DATA)
                  if (memcmp(sectordata+1,"CD001",5)==0) rc = DSKST_CDFSVD;
            }
         }
      } while (false);
      return rc;
   }
}

// used by scan itself, so should not use scan_lock()
u32t _std dsk_sectortype(u32t disk, u64t sector, u8t *optbuf) {
   u8t   *sbuf = optbuf?optbuf:malloc_thread(MAX_SECTOR_SIZE);
   struct Disk_MBR       *mbr = (struct Disk_MBR *)sbuf;
   struct Boot_Record    *btw = (struct Boot_Record *)sbuf;
   struct Boot_RecordF32 *btd = (struct Boot_RecordF32 *)sbuf;
   u32t          rc, sectsize;
   memset(sbuf, 0, sizeof(MAX_SECTOR_SIZE));
   // get sector size
   hlp_disksize(disk, &sectsize, 0);

   if (!hlp_diskread(disk, sector, 1, sbuf)) rc = DSKST_ERROR; else
      rc = dsk_datatype(sbuf, sectsize, disk, sector==0);

   if (sbuf!=optbuf) free(sbuf);
   return rc;
}

qserr _std dsk_newvbr(u32t disk, u64t sector, u32t type, const char *name) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record *btw = (struct Boot_Record *)&sbuf;
   char         lvm_letter = 0;
   char         bfname[16], fsname[10];
   u32t     current, curst;

   if (type>DSKBS_EXFAT) return E_SYS_INVPARM;
   if (type==DSKBS_DEBUG) return dsk_debugvbr(disk,sector)?0:E_DSK_ERRWRITE;
   // try to discover LVM drive letter for this sector
   if (disk&QDSK_VOLUME) {
      u32t  phdisk;
      long  volidx = vol_index(disk&~QDSK_VOLUME, &phdisk);
      if (volidx>0) {
         lvm_letter = toupper(lvm_ismounted(phdisk, volidx));
         if (lvm_letter<'C' || lvm_letter>'Z') lvm_letter = 0;
      }
   }

   memset(sbuf, 0, sizeof(sbuf));
   curst = dsk_ptqueryfs(disk, sector, fsname, sbuf);
   // i/o error?
   if (curst==DSKST_ERROR) return E_DSK_ERRREAD;
   // check for BPB presence
   if (curst!=DSKST_BOOTFAT && curst!=DSKST_BOOTEXF && curst!=DSKST_BOOTBPB)
      return E_DSK_UNKFS;
   // discover current file system type
   if ((disk&QDSK_VOLUME)!=0) {
      switch (hlp_volinfo(disk&~QDSK_VOLUME,0)) {
         case FST_FAT12:
         case FST_FAT16: current = DSKBS_FAT16; break;
         case FST_FAT32: current = DSKBS_FAT32; break;
         case FST_EXFAT: current = DSKBS_EXFAT; break;
         case FST_OTHER:
         case FST_NOTMOUNTED:
            if (strcmp(fsname,"HPFS")) return E_DSK_UNKFS;
            current = DSKBS_HPFS;
            break;
         default:
            return E_DSK_UNKFS;
      }
   } else {
      if (curst==DSKST_BOOTFAT)
         current = btw->BR_BPB.BPB_RootEntries==0?DSKBS_FAT32:DSKBS_FAT16; else
      if (curst==DSKST_BOOTEXF) current = DSKBS_EXFAT; else
      if (strcmp(fsname,"HPFS")==0) current = DSKBS_HPFS;
         else return E_DSK_UNKFS;
      // get sector size
   }

   if (type==DSKBS_AUTO) type = current; else
   if (type!=current) return E_DSK_FSMISMATCH;

   bfname[0]=0;
   if (name&&*name) {
      int ii;
      if (type==DSKBS_HPFS || type==DSKBS_EXFAT) {
         if (strlen(name)>15) return E_DSK_CNAMELEN;
         for (ii=0; name[ii]&&ii<15; ii++) bfname[ii] = toupper(name[ii]);
         for (; ii<16; ii++) bfname[ii]=0;
      } else {
         if (strlen(name)>8 || strchr(name,'.')) return E_DSK_CNAMELEN;  // 8.3
         for (ii=0; name[ii]&&ii<8; ii++) bfname[ii] = toupper(name[ii]);
         for (; ii<11; ii++) bfname[ii]=' '; bfname[12]=0;
      }
   }
   log_printf("Write VBR: disk %02X, %s %s\n", disk, fsname, bfname);

   if (type==DSKBS_FAT16) {
      struct Boot_Record *sbtw = (struct Boot_Record *)&bsect16;

      memcpy(btw,bsect16,11);
      memcpy(btw+1,sbtw+1,512-sizeof(struct Boot_Record));
      // copy custom boot name
      if (bfname[0])
         memcpy(sbuf+((u32t)&bsect16_name - (u32t)&bsect16), bfname, 11);

      return hlp_diskwrite(disk, sector, 1, sbuf)?0:E_DSK_ERRWRITE;
   } else
   if (type==DSKBS_FAT32) {
      struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsect32,
                             *btd = (struct Boot_RecordF32 *)&sbuf;

      memcpy(btd,bsect32,11);
      memcpy(btd+1,sbtd+1,512-sizeof(struct Boot_RecordF32));
      // copy custom boot name
      if (bfname[0])
         memcpy(sbuf+((u32t)&bsect32_name - (u32t)&bsect32), bfname, 11);

      return hlp_diskwrite(disk, sector, 1, sbuf)?0:E_DSK_ERRWRITE;
   } else
   if (type==DSKBS_HPFS) {
      struct Boot_Record *sbth = (struct Boot_Record *)&hpfs_bsect;

      /* Boot Manager saves or checks OEM signature and unable to find volume
         if it was changed, so leave it untouched */
      memcpy(btw, hpfs_bsect, 3);
      memcpy(btw+1, sbth+1, 512-sizeof(struct Boot_Record));
      // copy custom boot name
      if (bfname[0])
         memcpy(sbuf+((u32t)&hpfs_bsname - (u32t)&hpfs_bsect), bfname, 15);
      if (lvm_letter)
         btw->BR_EBPB.EBPB_Dirty = 0x80+(lvm_letter-'C');

      if (hlp_diskwrite(disk, sector, 1, sbuf)==0) return E_DSK_ERRWRITE;
      if (hpfs_bscount>1)
         if (hlp_diskwrite(disk, sector+1, hpfs_bscount-1, hpfs_bsect+512)!=hpfs_bscount-1)
            return E_DSK_ERRWRITE;
      // zero sectors between micro-FSD & superblock
      if (HPFSSEC_SUPERB-hpfs_bscount>0)
         dsk_emptysector(disk, sector+hpfs_bscount, HPFSSEC_SUPERB-hpfs_bscount);
      return 0;
   } else
   if (type==DSKBS_EXFAT) {
      u32t sectsize = dsk_sectorsize(disk), ii;
      u8t   *exfbuf = (u8t*)calloc(exf_bsacount+1, sectsize);
      struct Boot_RecExFAT *sbte = (struct Boot_RecExFAT *)&bsectexf,
                            *bte = (struct Boot_RecExFAT *)exfbuf;
      if (!exfbuf) return E_SYS_NOMEM;
      memcpy(bte, btw, sizeof(struct Boot_RecExFAT));
      memcpy(bte, sbte, 3);
      memcpy(bte+1, sbte+1, 512-sizeof(struct Boot_RecExFAT));

      if (exf_bsacount)
         for (ii=1; ii<=exf_bsacount; ii++)
            memcpy(exfbuf+sectsize*ii, (u8t*)sbte+512*ii, 512);
      // copy custom boot name
      if (bfname[0]) {
         u32t nameofs = (u32t)&bsectexf_name - (u32t)&bsectexf;
         // fix offset in target buffer
         if (nameofs>512 && sectsize>512)
            nameofs = (nameofs>>9)*sectsize + (nameofs&511);
         memcpy(exfbuf+nameofs, bfname, 15);
      }
      // update boot record & check sums
      ii = exf_updatevbr(disk, sector, exfbuf, 1+exf_bsacount, 1);
      free(exfbuf);
      return ii?0:E_DSK_ERRWRITE;
   }

   return E_DSK_UNKFS;
}


int _std vol_dirty(u8t vol, int on) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record *btw = (struct Boot_Record *)&sbuf;
   char   fsname[10];
   u32t   bs;
   int    state = -E_DSK_UNKFS;

   memset(sbuf, 0, sizeof(sbuf));
   bs = dsk_ptqueryfs(QDSK_VOLUME|vol, 0, fsname, sbuf);
   // i/o error?
   if (bs==DSKST_ERROR) return -E_DSK_ERRREAD;
   // check for BPB presence
   if (bs!=DSKST_BOOTFAT && bs!=DSKST_BOOTBPB && bs!=DSKST_BOOTEXF)
      return -E_DSK_UNKFS;
   // discover current file system type
   switch (hlp_volinfo(vol,0)) {
      case FST_FAT12:
      case FST_FAT16:
         state = btw->BR_EBPB.EBPB_Dirty & 1;
         if (on>=0)
            if (on>0) btw->BR_EBPB.EBPB_Dirty|=1; else
               btw->BR_EBPB.EBPB_Dirty&=~1;
         break;
      case FST_FAT32: {
         struct Boot_RecordF32 *btd = (struct Boot_RecordF32 *)&sbuf;
         state = btd->BR_F32_EBPB.EBPB_Dirty & 1;
         if (on>=0)
            if (on>0) btd->BR_F32_EBPB.EBPB_Dirty|=1; else
               btd->BR_F32_EBPB.EBPB_Dirty&=~1;
         break;
      }
      case FST_EXFAT: {
         struct Boot_RecExFAT *bre = (struct Boot_RecExFAT *)&sbuf;
         state = bre->BR_ExF_State&2?1:0;
         if (on>=0)
            if (on>0) bre->BR_ExF_State|=2; else bre->BR_ExF_State&=~2;
         break;
      }
      case FST_OTHER:
      case FST_NOTMOUNTED:
         if (strcmp(fsname,"HPFS")) return -E_DSK_UNKFS;
         return hpfs_dirty(vol, on);
         break;
      default:
         return -E_DSK_UNKFS;
   }
   if (on>=0)
      return hlp_diskwrite(QDSK_VOLUME|vol, 0, 1, sbuf)?state:-E_DSK_ERRWRITE;

   return state;
}


/** write debug boot record code.
    This code does not load anything, but print DL, BPB.PhysDisk and i13x
    presence. Code can be written to any type of FAT boot sector and to
    MBR (function preserve partition table space in sector).

    @param disk     disk number
    @param sector   sector number
    @return boolean (success flag) */
int _std dsk_debugvbr(u32t disk, u64t sector) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record *btw = (struct Boot_Record *)&sbuf;
   char   bfname[12];
   // minimal check
   if (disk==QDSK_VOLUME) return 0;

   memset(sbuf, 0, sizeof(sbuf));
   if (hlp_diskread(disk, sector, 1, sbuf)) {
      struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsectdb,
                             *btd = (struct Boot_RecordF32 *)&sbuf;
      struct Disk_MBR       *mbrd = (struct Disk_MBR *)&sbuf;
      u32t               sectsize;
      // get sector size
      hlp_disksize(disk, &sectsize, 0);
      // deny exFAT! (we will overwrite it superblock)
      if (dsk_datatype(sbuf, sectsize, disk, sector==0)==DSKST_BOOTEXF) {
         log_printf("exFAT can`t fit debug VBR!\n");
         return 0;
      }
      log_printf("Write debug VBR: disk %02X\n", disk);

      memcpy(btd, bsectdb, 11);
      memcpy(btd+1, sbtd+1, sizeof(mbrd->MBR_Code)-sizeof(struct Boot_RecordF32));

      return hlp_diskwrite(disk, sector, 1, sbuf);
   }

   return 0;
}

int exf_updatevbr(u32t disk, u64t sector, void *data, u32t nsec, u32t zerorem) {
   u32t sectsize = dsk_sectorsize(disk),
        secshift = bsf32(sectsize), ii, sum;
   u8t       *sb = (u8t*)calloc(12,sectsize);

   if (!nsec || nsec>11) return 0;
   /* force 55AA at the end of sectors 0..8. This required for secondary sectors,
      but with sector > 512b can be useful for boot record too */
   for (ii=0; ii<nsec; ii++) {
      memcpy(sb + (ii<<secshift), (u8t*)data + (ii<<secshift), sectsize);
      *(u16t*)(sb+(ii+1<<secshift)-2) = 0xAA55;
   }
   // get/create remain sectors
   if (nsec<11) {
      u32t fbofs = nsec<<secshift;
      if (zerorem) {
         // set 55AA to bootstrap sectors
         if (nsec<9)
            for (ii=0; ii<9-nsec; ii++)
               *(u16t*)(sb+(ii+1<<secshift)-2+fbofs) = 0xAA55;
      } else
         if (hlp_diskread(disk, sector+nsec, 11-nsec, sb+fbofs) != 11-nsec)
            { free(sb); return 0; }
   }
   for (ii = 0, sum = 0; ii<sectsize; ii++)
      if (ii!=offsetof(struct Boot_RecExFAT,BR_ExF_State) &&
         ii!=offsetof(struct Boot_RecExFAT,BR_ExF_State)+1 &&
            ii!=offsetof(struct Boot_RecExFAT,BR_ExF_UsedSpc))
               sum = exf_sum(sb[ii], sum);
   for (; ii<11<<secshift; ii++) sum = exf_sum(sb[ii], sum);
   // fill crc sector
   memsetd((u32t*)(sb+(11<<secshift)), sum, sectsize>>2);

   ii = hlp_diskwrite(disk, sector, 12, sb)==12 && 
        hlp_diskwrite(disk, sector+12, 12, sb)==12 ? 1 : 0;
   free(sb);
   return ii;
}


int dsk_printbpb(u32t disk, u64t sector) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record    *br   = (struct Boot_Record*)   &sbuf;
   struct Boot_RecordF32 *br32 = (struct Boot_RecordF32*)&sbuf;
   struct Boot_RecExFAT  *bre  = (struct Boot_RecExFAT*) &sbuf;
   int           IsFAT32 = 0, IsNTFS = 0, IsExF = 0;
   u32t       SectPerFat, n_res = 0;
   char           fstype[9];
   u16t            v_bps = 0, v_spc = 0, n_fat = 0;

   memset(sbuf, 0, sizeof(sbuf));
   if (!hlp_diskread(disk, sector, 1, sbuf)) return EIO;

   if (br->BR_BPB.BPB_RootEntries==0 && br->BR_BPB.BPB_FATCopies) {
      IsFAT32 = 1;
      strncpy(fstype,(char*)&br32->BR_F32_EBPB.EBPB_FSType,8);
      SectPerFat = br32->BR_F32_BPB.FBPB_SecPerFAT;
   } else {
      strncpy(fstype,(char*)&br->BR_EBPB.EBPB_FSType,8);
      if (!*fstype) {
         if (strncmp(br->BR_OEM,"NTFS",4)==0)
            { strcpy(fstype,"NTFS"); IsNTFS = 1; } else
         if (strnicmp(br->BR_OEM,"EXFAT",5)==0) {
            strcpy(fstype,"exFAT");
            IsExF = 1;
            v_bps = 1<<bre->BR_ExF_SecSize;
            v_spc = 1<<bre->BR_ExF_ClusSize;
            n_fat = bre->BR_ExF_FATCnt;
            n_res = bre->BR_ExF_FATPos<bre->BR_ExF_DataPos?bre->BR_ExF_FATPos:bre->BR_ExF_DataPos;
         }
      }
      SectPerFat = br->BR_BPB.BPB_SecPerFAT;
   }
   if (!v_bps) v_bps = br->BR_BPB.BPB_BytePerSect;
   if (!v_spc) v_spc = br->BR_BPB.BPB_SecPerClus;
   if (!n_fat) n_fat = br->BR_BPB.BPB_FATCopies;
   if (!n_res) n_res = br->BR_BPB.BPB_ResSectors;
   fstype[8] = 0;
   // init pager
   cmd_printseq(0,1,0);
   // print it!
   if (shellprn("Volume type: %s", fstype) || shellprt("Volume BPB:") ||
      shellprn("    Bytes Per Sector      %5d         Sect Per Cluster      %5d",
         v_bps, v_spc) ||
      shellprn("    Res Sectors           %5d         FAT Copies            %5d",
         n_res, n_fat)) return EINTR;
   if (IsExF) {
      if (shellprn("    Total Clusters     %08X         Sect Per FAT      %9d",
            bre->BR_ExF_NumClus, bre->BR_ExF_FATSize) ||
         shellprn("    First Sector   %012LX", bre->BR_ExF_VolStart) ||
         shellprn("    Total Sectors  %012LX", bre->BR_ExF_VolSize))
            return EINTR;
   } else
   if (shellprn("    Root Entries          %5d         Total Sectors         %5d",
         br->BR_BPB.BPB_RootEntries, br->BR_BPB.BPB_TotalSec) ||
      shellprn("    Media Byte               %02X         Sect Per FAT       %8d",
         br->BR_BPB.BPB_MediaByte, SectPerFat) ||
      shellprn("    Sect Per Track        %5d         Heads                 %5d",
         br->BR_BPB.BPB_SecPerTrack, br->BR_BPB.BPB_Heads) ||
      shellprn("    Hidden Sectors     %08X         Total Sectors (L)  %08X",
         br->BR_BPB.BPB_HiddenSec, br->BR_BPB.BPB_TotalSecBig) ||
      IsNTFS && shellprn("    Total Sectors (Q)  %012LX",
         ((struct Boot_RecordNT*)br)->BR_NT_TotalSec))
      return EINTR;
   return 0;
}
