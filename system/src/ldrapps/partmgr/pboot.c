//
// QSINIT
// partition management: mbr & boot sector code
//
#include "stdlib.h"
#include "qslog.h"
#include "qsshell.h"
#include "errno.h"
#include "qsdm.h"
#include "vio.h"
#include "dpmi.h"
#include "pscan.h"

#define MBR_LOAD_ADDR    0x7C00

extern char    psect[512],    ///< MBR code
             gptsect[512];
extern char  bsect16[512],    ///< VBR code
         bsect16_name[11],    ///< boot file name (for calculation)
             bsect32[512],
         bsect32_name[11],
             bsectdb[512];

int _std dsk_newmbr(u32t disk, u32t flags) {
   char   mbr_buffer[MAX_SECTOR_SIZE];
   struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;

   if ((flags&DSKBR_CLEARALL)==0) 
      memcpy(mbr, flags&DSKBR_GPTCODE ? &gptsect : &psect, 512);
   else 
      memset(mbr, 0, 512);
               

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
      rec0->PTE_CSStart  = 1;
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
   if ((flags&(DSKBR_CLEARALL|DSKBR_CLEARPT|DSKBR_GPTHEAD))!=0) dsk_unmountall(disk);
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
   struct Boot_Record *br = (struct Boot_Record*)malloc(MAX_SECTOR_SIZE);
   u32t st = dsk_sectortype(disk, sector, (u8t*)br);
   u32t ii, ssize;
   hlp_disksize(disk, &ssize, 0);

   if (st==DSKST_ERROR) return 0;
   if (st==DSKST_EMPTY) return 1;
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
   // write it back
   ii = hlp_diskwrite(disk, sector, 1, br);
   free(br);
   return ii;
}

void runmbr(struct rmcallregs_s *regs);
#ifdef __WATCOMC__
#pragma aux runmbr = \
   "mov   ax,301h"   \
   "xor   bx,bx"     \
   "xor   ecx,ecx"   \
   "int   31h"       \
   parm  [edi]       \
   modify [eax ebx ecx];
#endif // __WATCOMC__

int _std exit_bootmbr(u32t disk, u32t flags) {
   if (disk&(QDSK_FLOPPY|QDSK_VIRTUAL)) return EINVAL; else {
      char   mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR *mbr = (struct Disk_MBR *)&mbr_buffer;
      struct rmcallregs_s regs;
      int ii;
      // is it emulated?
      if ((hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)!=0) return ENOTBLK;
      // read mbr
      if (!hlp_diskread(disk, 0, 1, mbr)) return EIO;
      log_printf("MBR boot: disk %02X, flags %04X\n", disk, flags);
      // check for active bit presence
      if (dsk_isgpt(disk,-1)<=0)
         if ((flags&EMBR_NOACTIVE)==0) {
            for (ii=0;ii<4;ii++)
               if (mbr->MBR_Table[ii].PTE_Active>=0x80) break;
            if (ii==4) {
               log_printf("empty partition table!\n");
               return ENODEV;
            }
         }
      // copy own code
      if (flags&EMBR_OWNCODE) memcpy(&mbr->MBR_Code, dsk_isgpt(disk,-1)>0 ?
         &gptsect : &psect, sizeof(mbr->MBR_Code));

      // place to 0:07C00
      memcpy((char*)hlp_segtoflat(0)+MBR_LOAD_ADDR, mbr, 512);
      // setup DPMI
      memset(&regs, 0, sizeof(regs));
      regs.r_ip  = MBR_LOAD_ADDR;
      regs.r_edx = disk + 0x80;
      // shutdown all and go boot
      vio_clearscr();
      exit_prepare();
      hlp_fdone();
      runmbr(&regs);
   }
   return 0;
}

int _std exit_bootvbr(u32t disk, u32t index, char letter) {
   if (disk&(QDSK_FLOPPY|QDSK_VIRTUAL)) return EINVAL; else {
      u8t    vbr_buffer[MAX_SECTOR_SIZE];
      struct Boot_Record *br = (struct Boot_Record *)&vbr_buffer;
      struct rmcallregs_s regs;
      u32t   bstype, dmode;
      u64t   start, size;
      int ii;
      u8t type = dsk_ptquery64(disk, index, &start, &size, 0, 0);
      if (!type || IS_EXTENDED(type)) return EINVAL;
      if (start + size > _4GBLL) return EFBIG;

      // query current disk access mode
      dmode = hlp_diskmode(disk,HDM_QUERY);
      if ((dmode&HDM_EMULATED)!=0) return ENOTBLK;

      bstype = dsk_sectortype(disk, start, vbr_buffer);
      if (bstype==DSKST_ERROR) return EIO;
      if (bstype==DSKST_EMPTY) return ENODEV;
      // query volume letter from DLAT data
      if (!letter) {
         lvm_partition_data info;
         if (lvm_partinfo(disk, index, &info))
            if (info.Letter) letter = info.Letter;
      }
      if (letter) letter = toupper(letter);

      if (bstype==DSKST_BOOTFAT || bstype==DSKST_BOOTBPB) {
         /* update phys.disk and hidden sectors field.
            values updated only if BPB presence detected */
         br->BR_BPB.BPB_HiddenSec = start;

         // FAT32?
         if (bstype==DSKST_BOOTFAT && br->BR_BPB.BPB_RootEntries==0)
            ((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_PhysDisk = disk + 0x80;
         else {
            u8t sign = br->BR_EBPB.EBPB_Sign;
            // check signature
            if (sign==0x28 || sign==0x29) {
               br->BR_EBPB.EBPB_PhysDisk = disk + 0x80;
               if (letter) br->BR_EBPB.EBPB_Dirty = (u32t)letter - 'C' + 0x80;
                  else br->BR_EBPB.EBPB_Dirty = 0;
            }
         }
      }
      log_printf("VBR boot: disk %02X, sector %08LX, letter %c, %s, %02X\n", disk,
         start, letter?letter:'-', dmode&HDM_USELBA?"lba":"chs",
            br->BR_EBPB.EBPB_Dirty);

      // write I13X for IBM boot sectors
      if (dmode&HDM_USELBA) {
         u32t *p_i13x = (u32t*)hlp_segtoflat(0x3000);
         *p_i13x = 0x58333149;
      }
      // place to 0:07C00
      memcpy((char*)hlp_segtoflat(0)+MBR_LOAD_ADDR, br, 512);
      // setup DPMI
      memset(&regs, 0, sizeof(regs));
      regs.r_ip  = MBR_LOAD_ADDR;
      regs.r_edx = disk + 0x80;
      // shutdown all and go boot
      vio_clearscr();
      exit_prepare();
      hlp_fdone();
      runmbr(&regs);
   }
   return 0;
}

u32t _std dsk_sectortype(u32t disk, u64t sector, u8t *optbuf) {
   u8t   *sbuf = optbuf?optbuf:malloc(MAX_SECTOR_SIZE);
   struct Disk_MBR       *mbr = (struct Disk_MBR *)sbuf;
   struct Boot_Record    *btw = (struct Boot_Record *)sbuf;
   struct Boot_RecordF32 *btd = (struct Boot_RecordF32 *)sbuf;
   u32t rc;
   memset(sbuf, 0, sizeof(MAX_SECTOR_SIZE));

   do {
      if (!hlp_diskread(disk, sector, 1, sbuf)) { rc=DSKST_ERROR; break; }

      if (mbr->MBR_Signature==0xAA55) {
         u16t bps  = btw->BR_BPB.BPB_BytePerSect;
         int  boot = 0, pt = 0, fat = 0, ii, bpb = 0;
         // byte per sector valid?
         if (bps == 512 || bps == 1024 || bps == 2048 || bps == 4096) { boot++; bpb++; }
         // is FAT boot record
         if (strncmp(btw->BR_BPB.BPB_RootEntries==0?btd->BR_F32_EBPB.EBPB_FSType:
            btw->BR_EBPB.EBPB_FSType,"FAT",3)==0) { boot+=2; fat++; }
         if (strnicmp(btw->BR_OEM,"NTFS",4)==0 || strnicmp(btw->BR_OEM,"EXFAT",5)==0)
            boot+=100;
         // HDD sector 0
         if (!sector && (disk&(QDSK_FLOPPY|QDSK_VIRTUAL))==0) pt++;
         // boot partition
         if (!sector && (disk&QDSK_VIRTUAL)!=0) boot+=100;
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
         rc = bpb?DSKST_BOOTBPB:DSKST_BOOT;
      } else {
         struct Disk_GPT *pt = (struct Disk_GPT *)sbuf;

         if (pt->GPT_Sign==GPT_SIGNMAIN && pt->GPT_PtEntrySize==GPT_RECSIZE && 
            pt->GPT_HdrSize>=sizeof(struct Disk_GPT)) rc = DSKST_GPTHEAD;
         else
            rc = memchrnb(sbuf,0,MAX_SECTOR_SIZE)?DSKST_DATA:DSKST_EMPTY;
      }
   } while (false);
   if (sbuf!=optbuf) free(sbuf);
   return rc;
}

int _std dsk_newvbr(u32t disk, u64t sector, u32t type, const char *name) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record *btw = (struct Boot_Record *)&sbuf;
   char   bfname[12];

   // minimal check: do not wipe FSD boot disks
   if (type==FST_FAT12) type=FST_FAT16;
   if ((disk&QDSK_VIRTUAL)!=0) {
      u32t vi = hlp_volinfo(disk&~QDSK_VIRTUAL,0);
      if (vi==FST_FAT12) vi=FST_FAT16;
      if (vi==FST_NOTMOUNTED) return 0;
      // check type match or use auto-detect if was asked
      if (type==FST_NOTMOUNTED) type=vi; else
         if (vi!=type) return 0;
   }
   memset(sbuf, 0, sizeof(sbuf));
   if (!hlp_diskread(disk, sector, 1, sbuf)) return 0;

   if (type==FST_NOTMOUNTED)
      type=btw->BR_BPB.BPB_RootEntries==0?FST_FAT32:FST_FAT16;

   bfname[0]=0;
   if (name&&*name) {
      int ii;
      for (ii=0;name[ii]&&ii<11;ii++) bfname[ii]=toupper(name[ii]);
      for (;ii<11;ii++) bfname[ii]=' ';
   }

   log_printf("Write VBR: disk %02X, FAT%s %s\n", disk,
      type==FST_FAT16?"16":"32", bfname);

   if (type==FST_FAT16) {
      struct Boot_Record *sbtw = (struct Boot_Record *)&bsect16;

      memcpy(btw,bsect16,11);
      memcpy(btw+1,sbtw+1,512-sizeof(struct Boot_Record));
      // copy custom boot name
      if (bfname[0])
         memcpy(sbuf+((u32t)&bsect16_name - (u32t)&bsect16), bfname, 11);

      return hlp_diskwrite(disk, sector, 1, sbuf);
   } else
   if (type==FST_FAT32) {
      struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsect32,
                             *btd = (struct Boot_RecordF32 *)&sbuf;

      memcpy(btd,bsect32,11);
      memcpy(btd+1,sbtd+1,512-sizeof(struct Boot_RecordF32));
      // copy custom boot name
      if (bfname[0])
         memcpy(sbuf+((u32t)&bsect32_name - (u32t)&bsect32), bfname, 11);

      return hlp_diskwrite(disk, sector, 1, sbuf);
   }
   return 0;
}

/** write debug boot record code.
    This code does not load anything, but print DL, BPB.PhysDisk and i13x
    presence. Code can be written to any type of FAT boot sector and to
    MBR (function preserve partition table space in sector).

    @param disk     disk number
    @param sector   sector number
    @return boolean (success flag) */
int _std dsk_debugvbr(u32t disk, u32t sector) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record *btw = (struct Boot_Record *)&sbuf;
   char   bfname[12];
   // minimal check
   if (disk==QDSK_VIRTUAL) return 0;

   memset(sbuf, 0, sizeof(sbuf));
   if (hlp_diskread(disk, sector, 1, sbuf)) {
      struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsectdb,
                             *btd = (struct Boot_RecordF32 *)&sbuf;
      struct Disk_MBR       *mbrd = (struct Disk_MBR *)&sbuf;

      log_printf("Write debug VBR: disk %02X\n", disk);

      memcpy(btd,bsectdb,11);
      memcpy(btd+1,sbtd+1,sizeof(mbrd->MBR_Code)-sizeof(struct Boot_RecordF32));

      return hlp_diskwrite(disk, sector, 1, sbuf);
   }

   return 0;
}

int dsk_printbpb(u32t disk, u64t sector) {
   u8t    sbuf[MAX_SECTOR_SIZE];
   struct Boot_Record    *br   = (struct Boot_Record*)   &sbuf;
   struct Boot_RecordF32 *br32 = (struct Boot_RecordF32*)&sbuf;
   int           IsFAT32 = 0;
   u32t       SectPerFat;
   char           fstype[9];

   memset(sbuf, 0, sizeof(sbuf));
   if (!hlp_diskread(disk, sector, 1, sbuf)) return EIO;

   if (br->BR_BPB.BPB_RootEntries==0 && br->BR_BPB.BPB_FATCopies) {
      IsFAT32 = 1;
      strncpy(fstype,(char*)&br32->BR_F32_EBPB.EBPB_FSType,8);
      SectPerFat = br32->BR_F32_BPB.FBPB_SecPerFAT;
   } else {
      strncpy(fstype,(char*)&br->BR_EBPB.EBPB_FSType,8);
      if (!*fstype)
         if (strncmp(br->BR_OEM,"NTFS",4)==0) strcpy(fstype,"NTFS");
      SectPerFat = br->BR_BPB.BPB_SecPerFAT;
   }
   fstype[8] = 0;
   // init pager
   cmd_printseq(0,1,0);
   // print it!
   if (shellprn("Volume type: %s", fstype) || shellprt("Volume BPB:") ||
      shellprn("    Bytes Per Sector      %5d         Sect Per Cluster      %5d",
         br->BR_BPB.BPB_BytePerSect, br->BR_BPB.BPB_SecPerClus) ||
      shellprn("    Res Sectors           %5d         FAT Copies            %5d",
         br->BR_BPB.BPB_ResSectors, br->BR_BPB.BPB_FATCopies) ||
      shellprn("    Root Entries          %5d         Total Sectors         %5d",
         br->BR_BPB.BPB_RootEntries, br->BR_BPB.BPB_TotalSec) ||
      shellprn("    Media Byte               %02X         Sect Per FAT       %8d",
         br->BR_BPB.BPB_MediaByte, SectPerFat) ||
      shellprn("    Sect Per Track        %5d         Heads                 %5d",
         br->BR_BPB.BPB_SecPerTrack, br->BR_BPB.BPB_Heads) ||
      shellprn("    Hidden Sectors     %08X         Total Sectors (L)  %08X",
         br->BR_BPB.BPB_HiddenSec, br->BR_BPB.BPB_TotalSecBig))
      return EINTR;
   return 0;
}
