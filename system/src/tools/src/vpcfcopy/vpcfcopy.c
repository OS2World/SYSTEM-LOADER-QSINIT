/*****************************************************************************
   copy file to virtual pc hard disk image with single FAT primary partition
 *****************************************************************************/

#include "fmkfs.h"
// turn LFN off, we are not QSINIT
#include "ffconf.h"
#undef  FF_USE_LFN
#define FF_USE_LFN  0
#undef  FF_FS_EXFAT
#define FF_FS_EXFAT 0
#define dbc_1st(src)   0
#define dbc_2nd(src)   0
#undef  _WIN32
#include "ff.h"
#include <sys/stat.h>
#include <io.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include "bsdata.h"
// include FAT code directly here, to let them catch _USE_LFN above ;)
#include "ff.c"
#include "msvhd.h"
#include "lvmdat.h"

const BYTE TBL_CT437[128] = {
   0x80,0x9A,0x45,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F,
   0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
   0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
   0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
   0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
   0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
   0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
   0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF};
const BYTE* __stdcall ExCvt = TBL_CT437;

#define swapdw(x) ((((x)&0xFF)<<8)|(((x)>>8)&(0xFF)))
u32t    swapdd(u32t value);
u64t    swapdq(u64t value);
u32t    rndint(u32t range);
u32t    _randseed = 0;

#pragma aux swapdd =  \
    "bswap   eax"     \
    parm [eax] value [eax] modify exact [eax];

#pragma aux swapdq =  \
    "bswap   eax"     \
    "bswap   edx"     \
    "xchg    eax,edx" \
    parm [edx eax] value [edx eax] modify exact [edx eax];

#pragma aux rndint =        \
    "mov     eax,8088405h"  \
    "mul     _randseed"     \
    "inc     eax"           \
    "mov     _randseed,eax" \
    "mul     ecx"           \
    "mov     eax,edx"       \
    parm [ecx]              \
    modify exact [eax edx];

#define CRC_POLYNOMIAL    0xEDB88320
u32t    CRC_Table[256];
FILE       *disk;
FATFS        fat;
FIL          dst;
u32t   Disk1Size;
int   was_inited = 0;

static const char *diskspath = "0:";

int format(DWORD vol_start, DWORD vol_size, DWORD unitsz, WORD heads, WORD spt, WORD n_rootdir);

typedef struct {
   u32t     p1;
   u16t     p2;
   u16t     p3;
   u64t     p4;
} _guidrec;

void makeguid(void *guid) {
   _guidrec *gi = (_guidrec*)guid;
   _randseed = time(0) + clock();
   gi->p1 = rndint(FFFF);
   gi->p2 = rndint(0xFFFF);
   gi->p3 = 0x4000 | rndint(0xFFF);
   gi->p4 = (u64t)rndint(FFFF) << 32 | rndint(FFFF);
}

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

u32t  fsize(FILE *fp) {
   u32t  size_of_file,
             save_pos = ftell(fp);
   fseek(fp, 0, SEEK_END);
   size_of_file = ftell(fp);
   fseek(fp, save_pos, SEEK_SET);
   return size_of_file;
}

// Initialize a Drive
DSTATUS disk_initialize(BYTE drv) {
   if (drv>=1) return STA_NOINIT;
   was_inited=1;
   return 0;
}

// Return Disk Status
DSTATUS disk_status(BYTE drv) {
   return !was_inited?STA_NOINIT:0;
}

// Read Sector(s)
DRESULT disk_read (
   BYTE drv,       /* Physical drive nmuber (0..) */
   BYTE *buff,     /* Data buffer to store read data */
   DWORD sector,   /* Sector address (LBA) */
   UINT count      /* Number of sectors to read (1..128) */
) {
   if (!count) return RES_OK;
   switch (drv) {
      case 0:
         if (fseek(disk,sector<<9,SEEK_SET)) return RES_ERROR;
         if (fread(buff,1,count<<9,disk)!=count<<9) return RES_ERROR;
         break;
      default:
         return RES_ERROR;
   }
   return RES_OK;
}

// Miscellaneous Functions
DRESULT disk_ioctl (
    BYTE drv,       /* Physical drive nmuber (0..) */
    BYTE ctrl,      /* Control code */
    void *buff      /* Buffer to send/receive control data */
) {
   //log_misc("disk_ioctl(%d,%d,%x)\n",(DWORD)drv,(DWORD)ctrl,buff);
   switch (drv) {
      case 0:
         if (!Disk1Size) return RES_NOTRDY;
         if (ctrl==GET_SECTOR_SIZE) *(WORD*)buff=512; else
         if (ctrl==GET_BLOCK_SIZE) *(DWORD*)buff=512; else
         if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff=Disk1Size>>9; else
         if (ctrl!=CTRL_SYNC) return RES_PARERR;
         return 0;
      case 2:
         break;
   }
   return RES_PARERR;
}

DRESULT disk_write (
   BYTE drv,          /* Physical drive number (0..) */
   const BYTE *buff,  /* Data buffer to write */
   DWORD sector,      /* Sector address (LBA) */
   UINT count         /* Number of sectors to read (1..128) */
) {
   //log_misc("disk_write(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   if (drv>0) return RES_WRPRT;
   if (fseek(disk,sector<<9,SEEK_SET)) return RES_ERROR;
   if (fwrite(buff,1,count<<9,disk)!=count<<9) return RES_ERROR;
   return 0;
}

DWORD fat_filetime = 0;
DWORD get_fattime (void) { return fat_filetime; }
void  set_fattime (time_t tmv) {
   struct tm ft;
   _localtime(&tmv,&ft);
   ft.tm_year  += 1900;
   fat_filetime = ft.tm_year<1980||ft.tm_year>2099?0:
      (DWORD)ft.tm_year-1980<<25|(DWORD)ft.tm_mon+1<<21|ft.tm_mday<<16|
         ft.tm_hour<<11|ft.tm_min<<5|ft.tm_sec>>1;
}

void *findstr(void *data, u32t dlen, const char *str) {
   int  len = strlen(str);
   char *cp = (char*)data;
   if (!len) return 0;
   while (dlen>=len) {
      if (*cp==*str)
         if (memcmp(cp,str,len)==0) return cp;
      cp++; dlen--;
   }
   return 0;
}

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

static u32t vpc_checksum(u8t* buf, u32t len) {
   u32t res = 0;
   int   ii;
   for (ii=0; ii<len; ii++) res+=buf[ii];
   return ~res;
}

void create_disk(char *imgname, char *sizestr) {
   int    is_hdd = 0;
   u32t   unitsz = 0;
   u32t    heads = 0,
             spt = 0,
         rootdir = 512;
   if (strcmp(sizestr,"1.44")==0) {
      Disk1Size = 2880*512; unitsz = 1; heads = 2; spt = 18; rootdir = 224;
   } else
   if (stricmp(sizestr,"XDF")==0) {
      Disk1Size = 3680*512; unitsz = 1; heads = 2; spt = 23; rootdir = 224;
   } else
   if (strcmp(sizestr,"2.88")==0) {
      Disk1Size = 5760*512; unitsz = 2; heads = 2; spt = 36; rootdir = 240;
   } else {
      int len = atoi(sizestr);

      if (len<=0 || len>2047) {
         printf("invalid image size: %u\n", len);
         exit(1);
      }
      Disk1Size = (unsigned)len << 20;
      is_hdd    = 1;
   }
   disk = fopen(imgname,"w+b");
   if (!disk) {
      printf("unable to create disk file \"%s\"\n", imgname);
      exit(2);
   } else
   if (chsize(fileno(disk),Disk1Size)) {
      printf("unable to expand new disk file to specified size (%u kb)\n", Disk1Size>>10);
      fclose(disk);
      remove(imgname);
      exit(2);
   } else {
      u32t        vol_start, vol_size, cyls_bios, cyls;
      u16t       heads_bios;
      int             fmres;
      u32t           is_vhd = 0;
      char             *buf = calloc(1,512);
      struct Disk_MBR  *mbr = (struct Disk_MBR*)buf;
      struct MBR_Record *r0 = (struct MBR_Record*)(buf + MBR_Table);

      if (is_hdd) {
         u32t nsec = Disk1Size>>9;
         is_vhd = strlen(imgname)>4 && stricmp(imgname+strlen(imgname)-4, ".vhd")==0 ? 1 : 0;
         cyls = 1024; spt = 17; heads = 3;

         while (cyls*spt*heads < nsec) {
            if (spt<63) {
               spt   = (spt   + 1 & 0xF0) * 2 - 1; continue;
            }
            if (heads<255) {
               heads = (heads + 1) * 2 - 1; continue;
            }
            break;
         }
         cyls = nsec / (spt*heads);
         nsec = cyls*spt*heads;
         // vhd wants dumb bios geometry in header, at least, as QEMU says
         if (is_vhd && heads>16) {
            heads_bios = 16;
            cyls_bios  = nsec / (spt*16);
            while (cyls_bios*heads_bios*spt < cyls*spt*heads) cyls_bios++;
         } else {
            heads_bios = heads;
            cyls_bios  = cyls;
         }
         // adjust disk size a bit
         if (cyls_bios*heads_bios*spt+is_vhd != Disk1Size>>9) {
            Disk1Size  = cyls_bios*heads_bios*spt << 9;
            chsize(fileno(disk), Disk1Size + (is_vhd<<9));
         }
         memcpy(mbr, &bsdata, 512);

         r0->PTE_LBAStart = vol_start = spt;
         r0->PTE_LBASize  = vol_size  = nsec - spt;
         r0->PTE_Active   = 0x80;
         r0->PTE_Type     = PTE_06_FAT16;

         lba2chs(heads, spt, spt, &r0->PTE_HStart, &r0->PTE_CSStart);
         lba2chs(heads, spt, nsec-1, &r0->PTE_HEnd, &r0->PTE_CSEnd);
      } else {
         vol_start = 0;
         vol_size  = Disk1Size>>9;
         cyls = vol_size / (spt*heads);
      }
      rewind(disk);
      fwrite(buf, 1, 512, disk);
      // volume serial will use it
      set_fattime(time(0));

      fmres = format(vol_start, vol_size, unitsz, heads, spt, rootdir);
      if (fmres<0) {
         printf("mkfs error %i\n", -fmres);
         fclose(disk);
         remove(imgname);
         exit(4);
      } else
      if (is_hdd) {
         // update partition type
         if (vol_start+vol_size < 65536) {
            r0->PTE_Type = fmres==FST_FAT12 ? PTE_01_FAT12 : PTE_04_FAT16;
            rewind(disk);
            fwrite(buf, 1, 512, disk);
         }
         // write LVM information
         if (1) {
            DLA_Table_Sector *ld = (DLA_Table_Sector*)buf;
            DLA_Entry        *de = &ld->DLA_Array[0];
            time_t           now = time(0);
            u32t              ii;
            memset(buf, 0, 512);
            for (ii=1; ii<spt-1; ii++) disk_write(0, buf, ii, 1);
            // else disk serial may be 0
            _randseed = time(0) + clock();

            ld->DLA_Signature1     = DLA_TABLE_SIGNATURE1;
            ld->DLA_Signature2     = DLA_TABLE_SIGNATURE2;
            ld->Disk_Serial        = rndint(FFFF);
            ld->Boot_Disk_Serial   = ld->Disk_Serial;
            ld->Cylinders          = cyls;
            ld->Heads_Per_Cylinder = heads;
            ld->Sectors_Per_Track  = spt;
            strftime(ld->Disk_Name, LVM_NAME_SIZE, "vhd%y%m%d_%H%M", localtime(&now));

            de->Volume_Serial      = rndint(FFFF);
            de->Partition_Serial   = rndint(FFFF);
            de->Partition_Size     = vol_size;
            de->Partition_Start    = vol_start;

            strncpy(de->Volume_Name, "V1", LVM_NAME_SIZE);
            memcpy(&de->Partition_Name, &de->Volume_Name, LVM_NAME_SIZE);

            lvm_buildcrc();
            ld->DLA_CRC = lvm_crc32(LVM_INITCRC, buf, 512);

            disk_write(0, buf, spt-1, 1);
         }
         // write VHD header
         if (is_vhd) {
            vhd_footer  *vh = (vhd_footer*)buf;
            time_t   crtime = time(0) - VHD_TIME_BASE;

            memset(buf, 0, 512);
            memcpy(&vh->creator, "conectix", 8);
            memcpy(&vh->appname, "vpc ", 4);
            memcpy(&vh->osname , "Wi2k", 4);
            vh->features = swapdd(2);
            vh->version  = swapdd(0x10000);
            vh->nextdata = FFFF64;
            vh->crtime   = swapdd(crtime);
            vh->major    = swapdw(5);
            vh->minor    = swapdw(3);
            vh->org_size = swapdq(cyls_bios*heads_bios*spt << 9);
            vh->cur_size = vh->org_size;
            vh->cyls     = swapdw(cyls_bios);
            vh->heads    = heads_bios;
            vh->spt      = spt;
            vh->type     = swapdd(2);

            makeguid(&vh->uuid);

            vh->checksum = swapdd(vpc_checksum(buf, sizeof(vhd_footer)));

            fseek(disk, -512, SEEK_END);
            fwrite(buf, 1, 512, disk);
         }
      }
      printf("%s disk raw image created - %u kb.\n"
             "%s%5u cyls %3u heads %3u sectors per track.\n", is_hdd?"Hard":"Floppy",
             Disk1Size>>10, is_hdd&&is_vhd?"LVM geo: ":"chs:", cyls, heads, spt);
      if (is_vhd) printf("VHD geo: %5u cyls %3u heads %3u sectors per track.\n",
         cyls_bios, heads_bios, spt);
      fclose(disk);
      free(buf);
   }
}

void update_boot(char *imgname, char *bsfile, char *bootname) {
   char   bfname[16];
   u8t    bscode[512];
   u32t    bslen = 0;
   FRESULT    rc;

   if (bsfile) {
      FILE *bs = fopen(bsfile,"rb");
      if (!bs) {
         printf("unable to open boot sector file \"%s\"\n", bsfile);
         exit(2);
      }
      bslen = fsize(bs);
      if (bslen!=512) {
         printf("wrong boot sector file size (only 512 bytes accepted)\n");
         exit(2);
      }
      if (fread(&bscode,1,bslen,bs)!=bslen) {
         printf("error reading of boot sector file\n");
         exit(4);
      }
      fclose(bs);
   } else {
      if (strlen(bootname)>8 || strchr(bootname,'.')) {
         printf("Boot file name must have up to 8 chars without dot\n");
         exit(1);
      } else {
         int ii;
         for (ii=0; bootname[ii]&&ii<8; ii++) bfname[ii] = toupper(bootname[ii]);
         for (; ii<11; ii++) bfname[ii]=' '; bfname[12]=0;
      }
   }
   disk = fopen(imgname,"r+b");
   if (!disk) {
      printf("unable to open disk file \"%s\"\n", imgname);
      exit(2);
   } else {
      char  *buf = malloc(512);
      struct Boot_Record    *brw = (struct Boot_Record*)buf;
      struct Boot_RecordF32 *brd = (struct Boot_RecordF32*)buf;
      Disk1Size = fsize(disk);
      do {
         u32t  vpos;
         u16t   bps;
         rc = f_mount(&fat, diskspath, 1);
         if (rc!=FR_OK) { printf("FAT mount error %d\n", rc); break; }
         vpos = fat.volbase;
         rc   = disk_read(0, buf, vpos, 1);
         if (rc!=FR_OK) { printf("Boot sector read error!\n"); break; }

         bps  = brw->BR_BPB.BPB_BytePerSect;
         // byte per sector valid?
         if (bps!=512) { printf("Unsupported sector size (%u)!\n", bps); break; }
         // boot sector copy location
         bps  = 0;
         /* even if header says about missing FAT32 support, we still write
            external sector here, but 512 bytes only! This makes this option
            void for common boot code - like Windows one. BUT two and more
            sectors require smart logic of boot area updating, with additional
            parameters to setup */
         if (fat.fs_type==FS_FAT32) {
            struct Boot_RecordF32 *sbtd = bslen?(struct Boot_RecordF32 *)&bscode:
                                                (struct Boot_RecordF32 *)&bsdata[1024];
            memcpy(brd, sbtd, 11);
            memcpy(brd+1, sbtd+1, 512-sizeof(struct Boot_RecordF32));
            /* looks like every one happy without this string, but still
               make the image more general */
            if (!bslen) memcpy(&brw->BR_OEM, "MSWIN4.1", 8);
            // zero value will be ignored too
            if (brd->BR_F32_BPB.FBPB_BootCopy!=0xFFFF)
               bps = brd->BR_F32_BPB.FBPB_BootCopy;
         } else {
            struct Boot_Record *sbtw = bslen?(struct Boot_Record *)&bscode:
                                             (struct Boot_Record *)&bsdata[512];
            memcpy(brw, sbtw, 11);
            memcpy(brw+1, sbtw+1, 512-sizeof(struct Boot_Record));
            // the same as above
            if (!bslen) memcpy(&brw->BR_OEM, "MSDOS5.0", 8);
         }
         // update boot file name in self-provided code
         if (!bslen) {
            void *ps = findstr(buf, 512, "QSINIT");
            if (ps) memcpy(ps, &bfname, 11);
         }
         rc = disk_write(0, buf, vpos, 1);
         if (rc!=FR_OK) { printf("Boot sector write error %u!\n", rc); break; }
         // a copy of boot sector on FAT32 (save it, but ignore write error)
         if (bps)
            if (disk_write(0, buf, vpos+bps, 1) != FR_OK)
               printf("Warning! Boot sector backup write error\n");
      } while(0);

      fclose(disk);
      free(buf);
      if (rc!=FR_OK) exit(4);
   }
}

int main(int argc, char *argv[]) {
   if (argc<4) {
      printf("usage: vpcfcopy disk_image dst_pathname src_pathname\n"
             "       only short names accepted in \"dst_pathname\"\n\n"
             "   or: vpcfcopy /boot disk_image boot_file_name\n"
             "       update boot sector, example of \"boot_file_name\" is OS2LDR\n"
             "       FAT and FAT32 supported\n\n"
             "   or: vpcfcopy /bootext disk_image boot_sector_file\n"
             "       replace boot sector code to another one from a file\n"
             "       FAT only is supported\n\n"
             "   or: vpcfcopy /create disk_image 1.44|XDF|2.88|size\n"
             "       where \"size\" 1..2047 (mb) - for a RAW hard disk image,\n"
             "       use disk_image.VHD to create fixed size VHD image\n"
             "       hdd image will have single FAT partition and valid LVM information\n");
      return 1;
   }
   if (stricmp(argv[1],"/create")==0) create_disk(argv[2], argv[3]); else
   if (stricmp(argv[1],"/boot")==0) update_boot(argv[2], 0, argv[3]); else
   if (stricmp(argv[1],"/bootext")==0) update_boot(argv[2], argv[3], 0);
   else {
      disk = fopen(argv[1],"r+b");
      if (!disk) {
         printf("unable to open disk file \"%s\"\n", argv[1]);
         return 2;
      } else {
         struct stat si;
         FILE   *src = 0;
         u32t   size;
         UINT    act;
         FRESULT  rc = FR_OK;
         void  *data = 0;
         // get file time
         if (!stat(argv[3],&si)) set_fattime(si.st_mtime);
         // read file
         src = fopen(argv[3],"rb");
         do {
            if (!src) {
               printf("unable to open source file \"%s\"\n",argv[3]);
               break;
            }
            size = fsize(src);
            if (size) {
               data = malloc(size);
               act  = fread(data,1,size,src);
               if (act!=size) { fclose(src); src=0; break; }
            }
            Disk1Size = fsize(disk);
            rc = f_mount(&fat, diskspath, 1);
            if (rc!=FR_OK) { printf("FAT mount error %d\n",rc); break; }
            // drop r/o attr if this file is present on disk
            f_chmod(argv[2], 0, AM_RDO|AM_HID|AM_SYS);
            // and re-write it
            rc = f_open(&dst, argv[2], FA_WRITE|FA_CREATE_ALWAYS);
            // no such path? trying to create it
            if (rc==FR_NO_PATH) {
               char *fncopy = strdup(argv[2]),
                        *cc = fncopy;
               do {
                  cc = strchr(cc,'/');
                  if (cc) *cc='\\';
               } while (cc);
               cc = fncopy;
               if (*cc=='\\') cc++;

               do {
                  cc = strchr(cc,'\\');
                  if (cc) {
                     *cc = 0;
                     rc  = f_mkdir(fncopy);
                     *cc++='\\';
                     if (rc!=FR_EXIST || rc!=FR_OK) break;
                  }
               } while (cc);
               free(fncopy);
               // open file again
               rc = f_open(&dst, argv[2], FA_WRITE|FA_CREATE_ALWAYS);
            }
            if (rc!=FR_OK) {
               printf("Invalid destination file name (\"%s\")\n", argv[2]);
               break;
            }
            if (size) {
               rc = f_write(&dst, data, size, &act);
               if (rc!=FR_OK || size!=act) {
                  printf("File \"%s\" write error!\n", argv[2]);
                  break;
               }
               f_close(&dst);
            }
         } while(0);

         fclose(disk);
         if (!src) return 3; else fclose(src);
         if (rc!=FR_OK) return 4;
      }
   }
   return 0;
}
