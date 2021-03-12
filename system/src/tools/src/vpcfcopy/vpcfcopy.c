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
#ifdef __LINUX__
#include <unistd.h>
#else
#include <io.h>
#endif
#include <time.h>
#ifndef SP_LINUX
#include <dos.h>         // _dos_setftime
#else
#include <utime.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include "bsdata.h"      // MBR boot code
#include "gptboot.h"     // GPT boot code
// include FAT code directly here, to let them catch _USE_LFN above ;)
#include "ff.c"
#include "lvmdat.h"

// dos time conversion for linux host
#ifdef SP_LINUX
#define CONST_DATE_D0     1461
#define CONST_DATE_D1   146097
#define CONST_DATE_D2  1721119
#define CONST_SECINDAY   86400

static int GregorianToJulian(u16t Year,u16t Month,u16t Day) {
   int Century, XYear;
   if (Month<=2) { Year--; Month+=12; }
   Month  -= 3;
   Century = Year/100;
   XYear   = Year%100;
   Century = Century*CONST_DATE_D1>>2;
   XYear   = XYear  *CONST_DATE_D0>>2;
   return (Month*153+2)/5+Day+CONST_DATE_D2+XYear+Century;
}

static void JulianToGregorian(int Julian,int *Year,int *Month,int *Day) {
   int Temp = ((Julian-CONST_DATE_D2<<2)-1),
      XYear = Temp%CONST_DATE_D1|3,YYear,YMonth,YDay;
   Julian = Temp/CONST_DATE_D1;
   YYear  = XYear/CONST_DATE_D0;
   Temp   = (XYear%CONST_DATE_D0+4>>2)*5-3;
   YMonth = Temp/153;
   if (YMonth>=10) { YYear++; YMonth-=12; }
   YMonth+= 3;
   YDay   = Temp%153;
   YDay   = (YDay+5)/5;
   *Year  = YYear+Julian*100;
   *Month = YMonth;
   *Day   = YDay;
}

time_t dostimetotime(u32t dostime) {
   int j1970 = GregorianToJulian(1970,1,1),
        jNow = GregorianToJulian((dostime>>25)+1980,dostime>>21&0xF,dostime>>16&0x1F),
         uts = (jNow-j1970)*CONST_SECINDAY,
        temp = (dostime>>11&0x1F)*3600+(dostime>>5&0x3F)*60+((dostime&0x1F)<<1);
   return uts+temp;
}
#endif // SP_LINUX

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

u32t    rndint(u32t range);
u32t    _randseed = 0;

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
FILE       *disk = 0;
FATFS        fat;
FIL          dst;
u32t   disk_size = 0, // in sectors
      disk_start = 0; // in sectors !!
u8t   disk_shift = 9; // sector size shift
int   was_inited = 0,
         phys_io = 0;
u32t   phys_disk = 0;

static const char *diskspath = "0:";

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

u32t fsize(FILE *fp) {
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
#ifdef PHYS_IO
         if (phys_io)
            return dsk_read(phys_disk, disk_start+sector, count, buff)==count?
               RES_OK:RES_ERROR;
#endif
         if (fseek(disk,disk_start+sector<<disk_shift,SEEK_SET)) return RES_ERROR;
         if (fread(buff,1,count<<disk_shift,disk)!=count<<disk_shift) return RES_ERROR;
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
         if (!disk_size) return RES_NOTRDY;
         if (ctrl==GET_SECTOR_SIZE) *(WORD*)buff = 1<<disk_shift; else
         if (ctrl==GET_BLOCK_SIZE) *(DWORD*)buff = 1<<disk_shift; else
         if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff=disk_size; else
         if (ctrl!=CTRL_SYNC) return RES_PARERR;
         return 0;
      case 2:
         break;
   }
   return RES_PARERR;
}

#ifdef PHYS_IO
void disk_write_error(u32t sector) {
   printf("Disk %u write error at sector %u\n", phys_disk, sector);
   exit(2);
}
#endif

DRESULT disk_write (
   BYTE drv,          /* Physical drive number (0..) */
   const BYTE *buff,  /* Data buffer to write */
   DWORD sector,      /* Sector address (LBA) */
   UINT count         /* Number of sectors to read (1..128) */
) {
   //log_misc("disk_write(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   if (drv>0) return RES_WRPRT;
#ifdef PHYS_IO
   if (phys_io)
      return dsk_write(phys_disk, disk_start+sector, count, (void*)buff)==count?
         RES_OK:RES_ERROR;
#endif
   if (fseek(disk,disk_start+sector<<disk_shift,SEEK_SET)) return RES_ERROR;
   if (fwrite(buff,1,count<<disk_shift,disk)!=count<<disk_shift) return RES_ERROR;
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

void check_phys_io(const char *name) {
#ifdef PHYS_IO
   if (name[0]==':' && name[1]==':' && isdigit(name[2])) {
      phys_disk = strtoul(name+2,0,0);
      phys_io   = 1;
   }
#endif
}

void update_mbr(char *mbrdata) {
   if (!phys_io) {
      rewind(disk);
      fwrite(mbrdata, 1, 512, disk);
   } else {
#ifdef PHYS_IO
      if (!dsk_write(phys_disk, 0, 1, mbrdata)) disk_write_error(0);
#endif
   }
}

typedef enum { f_mbr, f_mbref, f_gpt, f_fdd } disk_fmt;

void create_disk(char *imgname, char *sizestr, disk_fmt fmt) {
   int    is_hdd = 0;
   u32t   unitsz = 0;
   u32t    heads = 0,
             spt = 0,
         rootdir = 512,
      altvolsize = 0;
   char     *buf;

   check_phys_io(imgname);
   if (fmt==f_fdd && phys_io) {
      printf("Floppy creation is not supported for the physical device\n");
      exit(1);
   }

   // disk_start should be always zero in this function
   if (fmt!=f_gpt && !phys_io && strcmp(sizestr,"1.44")==0) {
      disk_size = 2880; unitsz = 1; heads = 2; spt = 18; rootdir = 224;
   } else
   if (fmt!=f_gpt && !phys_io && stricmp(sizestr,"XDF")==0) {
      disk_size = 3680; unitsz = 1; heads = 2; spt = 23; rootdir = 224;
   } else
   if (fmt!=f_gpt && !phys_io && strcmp(sizestr,"2.88")==0) {
      disk_size = 5760; unitsz = 2; heads = 2; spt = 36; rootdir = 240;
   } else {
      int len = atoi(sizestr);

      if (len<=0 || len>2047) {
         printf("invalid %s size: %u\n", phys_io?"FAT volume":"image", len);
         exit(1);
      }
      disk_size = (unsigned)len << 20 - 9;
      is_hdd    = fmt!=f_fdd;
   }
   buf = calloc(1,512);
#ifdef PHYS_IO
   if (phys_io) {
      struct Disk_MBR  *mbr = (struct Disk_MBR*)buf;
      if (!dsk_read(phys_disk, 0, 1, buf)) {
         printf("unable to read disk %u MBR sector\n", phys_disk);
         exit(2);
      }
      if (mbr->MBR_Signature==0xAA55) {
         printf("disk %u is not empty!\n", phys_disk);
         exit(2);
      }

   } else 
#endif
   {
      disk = fopen(imgname,"w+b");
      if (!disk) {
         printf("unable to create disk file \"%s\"\n", imgname);
         exit(2);
      } else
      if (chsize(fileno(disk),disk_size<<9)) {
         printf("unable to expand disk file to specified size (%u kb)\n", disk_size>>1);
         fclose(disk);
         remove(imgname);
         exit(2);
      }
   }
   if (1) {
      u32t        vol_start, vol_size, cyls;
      int             fmres;
      u32t           is_vhd = 0;
      struct Disk_MBR  *mbr = (struct Disk_MBR*)buf;
      struct MBR_Record *r0 = (struct MBR_Record*)(buf + MBR_Table);

      // custom image format, calc chs
      if (!heads) {
         u32t nsec;

         if (!phys_io) {
            nsec   = disk_size;
            is_vhd = strlen(imgname)>4 && stricmp(imgname+strlen(imgname)-4, ".vhd")==0 ? 1 : 0;
            cyls   = 1024; spt = 17; heads = 3;
            
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
            nsec = cyls*heads*spt;
            // vhd wants dumb bios geometry in header, at least, as QEMU says
            if (is_vhd && heads>16) {
               u32t  heads_bios = 16,
                      cyls_bios = nsec / (spt*16);
               while (cyls_bios*heads_bios*spt < cyls*spt*heads) cyls_bios++;
               heads = heads_bios;
               cyls  = cyls_bios;
               nsec  = cyls*heads*spt;
            }
            // adjust disk size
            if (nsec + is_vhd != disk_size) {
               disk_size = nsec;
               chsize(fileno(disk), disk_size+is_vhd << 9);
            }
         } else {
#ifdef PHYS_IO
            dsk_geo_data geo;
            nsec  = dsk_size(phys_disk, 0, &geo);
            cyls  = geo.Cylinders;
            spt   = geo.SectOnTrack;
            heads = geo.Heads;
            altvolsize = disk_size;
            disk_size  = nsec;
#endif
         }
         if (is_hdd) {
            if (fmt==f_gpt) {
               vol_size = altvolsize;
               disk_init(nsec, &vol_start, &vol_size);
            
               memcpy(mbr, &gptboot, 512);
            
               r0->PTE_LBAStart = 1;
               r0->PTE_LBASize  = nsec - 1;
               r0->PTE_Type     = PTE_EE_UEFI;
            
               if (altvolsize && altvolsize<vol_size) vol_size = altvolsize;
            } else {
               memcpy(mbr, &bsdata, 512);
               vol_start = spt;
               vol_size  = nsec - spt; 
               // alt volume size
               if (altvolsize && altvolsize<vol_size) {
                  u32t ep = vol_start + altvolsize;
                  ep = ep / (heads*spt) * (heads*spt);
                  if (ep>vol_start+vol_size) vol_size = ep - vol_start;
               }
               r0->PTE_LBAStart = vol_start;
               r0->PTE_LBASize  = vol_size;
               r0->PTE_Active   = 0x80;
               r0->PTE_Type     = PTE_06_FAT16;
            }
            lba2chs(heads, spt, r0->PTE_LBAStart, &r0->PTE_HStart, &r0->PTE_CSStart);
            lba2chs(heads, spt, nsec-1, &r0->PTE_HEnd, &r0->PTE_CSEnd);
            
            update_mbr(buf);
         }
      }
      if (!is_hdd) {
         vol_start = 0;
         vol_size  = disk_size;
         cyls = vol_size / (spt*heads);
      }
      // volume serial will use it
      set_fattime(time(0));
#if 0
      printf("formatting to FAT: start sector %X, %X sectors\n", vol_start, vol_size);
#endif      
      fmres = format(vol_start, vol_size, unitsz, heads, spt, rootdir, 0);
      if (fmres<0) {
         printf("mkfs error %i\n", -fmres);
         if (!phys_io) {
            fclose(disk);
            remove(imgname);
         }
         exit(4);
      } else
      if (is_hdd) {
         // update partition type
         if (fmt==f_mbr && vol_start+vol_size < 65536) {
            r0->PTE_Type = fmres==FST_FAT12 ? PTE_01_FAT12 : PTE_04_FAT16;
            update_mbr(buf);
         } else
         if (fmt==f_mbref) {
            r0->PTE_Type = PTE_EF_UEFI;
            update_mbr(buf);
         }
         // write LVM information
         if (fmt==f_mbr || fmt==f_mbref) {
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
         vh->org_size = swapdq(cyls*heads*spt << 9);
         vh->cur_size = vh->org_size;
         vh->cyls     = swapdw(cyls);
         vh->heads    = heads;
         vh->spt      = spt;
         vh->type     = swapdd(2);
      
         makeguid(&vh->uuid);
      
         vh->checksum = swapdd(vpc_checksum(buf, sizeof(vhd_footer)));
      
         fseek(disk, -512, SEEK_END);
         fwrite(buf, 1, 512, disk);
      }
      if (!phys_io) {
         printf("%s disk %s image created - %u kb.\n"
                "chs:%5u cyls %3u heads %3u sectors per track.\n",
                is_hdd?"Hard":"Floppy", is_vhd?"VHD":"raw", disk_size>>1,
                cyls, heads, spt);
         fclose(disk);
      } else {
         printf("Disk initialized - %u mb (FAT volume - %u kb).\n"
                "chs:%5u cyls %3u heads %3u sectors per track.\n",
                disk_size>>11, vol_size>>1, cyls, heads, spt);
      }
   }
   free(buf);
}

void update_boot(char *imgname, char *bsfile, char *bootname) {
   char   bfname[16];
   u8t    bscode[512];
   u32t    bslen = 0;
   FRESULT    rc;
   char     *buf = 0;
   media     dst;

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
   check_phys_io(imgname);
   // target media
   dst.phys = phys_io;
   if (phys_io) dst.dsk = phys_disk; else {
      disk = fopen(imgname,"r+b");
      if (!disk) {
         printf("unable to open disk file \"%s\"\n", imgname);
         exit(2);
      }
      dst.df = disk;
   }

   disk_check(&dst, &disk_start, &disk_size, &disk_shift);
   if (disk_shift!=9) {
      printf("Unsupported sector size (%u)!\n", 1<<disk_shift);
      exit(4);
   }
   do {
      struct Boot_Record    *brw;
      struct Boot_RecordF32 *brd;
      u32t   vpos;
      u16t    bps;

      buf = malloc(512);
      brw = (struct Boot_Record*)buf;
      brd = (struct Boot_RecordF32*)buf;

      rc = f_mount(&fat, diskspath, 1);
      if (rc!=FR_OK) { printf("FAT mount error %d\n", rc); break; }
      vpos = fat.volbase;
      rc   = disk_read(0, buf, vpos, 1);
      if (rc!=RES_OK) { printf("Boot sector read error!\n"); break; }

      bps  = brw->BR_BPB.BPB_BytePerSect;
      // byte per sector valid?
      if (bps!=512) { printf("Wrong sector size value (%u)!\n", bps); break; }
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
      if (rc!=RES_OK) { printf("Boot sector write error %u!\n", rc); break; }
      // a copy of boot sector on FAT32 (save it, but ignore write error)
      if (bps)
         if (disk_write(0, buf, vpos+bps, 1) != RES_OK)
            printf("Warning! Boot sector backup write error\n");
   } while(0);
   if (!phys_io) fclose(disk);
   if (buf) free(buf);
   if (rc!=FR_OK) exit(4);
}

static void fmtsize(u64t srcsize, int width, char *buf) {
   static char *suffix[] = { "kb", "mb", "gb", "tb", "pb", "eb", "zb" }; // ;)
   char  fmt[16];
   u64t size = srcsize<<disk_shift;
   int   idx = 0;
   size>>=10;
   while (size>=100000) { size>>=10; idx++; }
   if (width>4) sprintf(fmt, "%%%uu %%s", width-3);
   sprintf(buf, width>4?fmt:"%u %s", (u32t)size, suffix[idx]);
}


static void open_image(const char *imgname, media *dst) {
   check_phys_io(imgname);
   // target media
   dst->phys = phys_io;
   if (phys_io) dst->dsk = phys_disk; else {
      disk = fopen(imgname,"r+b");
      if (!disk) {
         printf("unable to open disk file \"%s\"\n", imgname);
         exit(2);
      }
      dst->df = disk;
   }
   disk_check(dst, &disk_start, &disk_size, &disk_shift);
}

void print_info(char *imgname) {
   FRESULT    rc;
   media     dst;
   char   *dpath = 0, *cp;

   open_image(imgname, &dst);

   do {
      char  label[16];
      u32t serial, ii;
      static const char *mfmt[MTYPE_TYPEMASK+1] = {"MBR disk", "GPT disk",
                               "Floppy", 0,0,0,0,0 };
      static const char *fs[] = {"Unknown", "FAT12", "FAT16", "FAT32", "exFAT" };

      rc = f_mount(&fat, diskspath, 1);
      if (rc!=FR_OK) {
         printf("Image format is unknown or no FAT found\n");
         break;
      }

      printf("Source type   : %s\n", dst.phys?"Physical disk":"Disk image");
      if (!dst.phys)
         printf("Image type    : %s\n", dst.type&MTYPE_VHD?"VHD":"Raw image");
      printf("Media format  : %s\n", mfmt[dst.type&MTYPE_TYPEMASK]);

      if ((dst.type&MTYPE_TYPEMASK)==MTYPE_MBR)
         for (ii=0; ii<4; ii++)
            if (dst.mbrpos[ii]==disk_start+fat.volbase) {
               printf("MBR type      : %02X\n", dst.mbrtype[ii]);
               break;
            }
      printf("\nFirst sector  : %08X\n", disk_start+fat.volbase);
      printf("Sector size   : %u\n", 1<<disk_shift);

      fmtsize((fat.n_fatent-2)*fat.csize, 4, label);
      printf("User space    : %s\n", label);

      printf("File system   : %s\n\n", fs[fat.fs_type]);
      if (fat.fs_type>0) {
         if (fat.csize==1) sprintf(label, "%u", 1<<disk_shift); else
            fmtsize(fat.csize, 4, label);
         printf("Cluster size  : %s\n", label);
         printf("Clusters      : %u\n", fat.n_fatent-2);
         printf("FAT copies    : %u\n", fat.n_fats);

         printf("Data area     : %08X\n", disk_start+fat.database);
      }
      if (fat.fs_type==FS_FAT12 || fat.fs_type==FS_FAT16)
         printf("Root entries  : %u\n", fat.n_rootdir);

      rc = f_getlabel(diskspath, label, &serial);
      if (rc==FR_OK && label[0])
         printf("Volume label  : %s\n", label);
      printf("Serial number : %04X-%04X\n", serial>>16, serial&0xFFFF);
   } while(0);

   if (!phys_io) fclose(disk);
   if (rc!=FR_OK) exit(4);
}

void print_dir(char *imgname, const char *dirname) {
   FRESULT    rc;
   media     dst;
   char   *dpath = 0, *cp;

   open_image(imgname, &dst);
   do {
      DIR      dp;
      rc = f_mount(&fat, diskspath, 1);
      if (rc!=FR_OK) { printf("FAT mount error %d\n", rc); break; }

      while (*dirname=='/' || *dirname=='\\') dirname++;
      dpath = (char*)malloc(2+strlen(dirname)+strlen(diskspath));
      strcpy(dpath, diskspath);
      strcat(dpath, "\\");
      strcat(dpath, dirname);

      while ((cp = strchr(dpath,'/'))) *cp = '\\';

      rc = f_opendir(&dp, dpath);
      if (rc) { 
         printf("Unable to open directory \"%s\"\n", dpath+3);
         break;
      } else {
         DWORD vsn;
         if (f_getlabel(diskspath,0,&vsn)==FR_OK)
            printf("Volume serial number is %04X-%04X\n", vsn>>16, vsn&0xFFFF);
         printf("Directory of %s\n", dpath+2);
      }

      do {
         u32t     tm;
         u8t    attr;
         FILINFO  fi;
         char   size[12];
         fi.fname[0] = 0;

         rc = f_readdir(&dp, &fi);

         if (!fi.fname[0]) break;
         tm   = (u32t)fi.fdate<<16|fi.ftime;
         attr = fi.fattrib;

         if (attr&AM_DIR) strcpy(size, "  <DIR>  ");
            else ultoa(fi.fsize, size, 10);

         printf("%-12s %10s %02u.%02u.%04u %02u:%02u:%02u %c%c%c%c\n",
            fi.fname, size, tm>>16&0x1F, tm>>21&0xF,
               (tm>>25) + 1980, tm>>11&0x1F, tm>>5&0x3F, (tm&0x1F)<<1,
                  attr&AM_RDO?'R':'-', attr&AM_HID?'H':'-',
                     attr&AM_SYS?'S':'-', attr&AM_ARC?'A':'-');

      } while (rc==0);

      f_closedir(&dp);
   } while(0);
   if (!phys_io) fclose(disk);
   if (dpath) free(dpath);
   if (rc!=FR_OK) exit(4);
}

void read_file(char *imgname, const char *dst_path, const char *src_path) {
   FRESULT    rc;
   media     dst;
   FIL      file;
   char      *sp = strdup(src_path), *cp;
   FILE      *df = 0;
   int        nc = 0;

   open_image(imgname, &dst);
   do {
      struct stat si;
      FILINFO     fi;
      BYTE      *mem;
      DWORD     step, pos;
      rc = f_mount(&fat, diskspath, 1);
      if (rc!=FR_OK) { printf("FAT mount error %d\n", rc); break; }

      while ((cp = strchr(sp,'/'))) *cp = '\\';

      if (!stat(dst_path,&si)) {
         printf("File \"%s\" already exists!\n", dst_path);
         break;
      }
      // get size and time
      rc = f_stat(sp, &fi);
      if (rc) { 
         printf("Unable to find file \"%s\"\n", sp);
         break;
      }
      rc = f_open(&file, sp, FA_READ|FA_OPEN_EXISTING);
      if (rc) { 
         printf("Unable to open file \"%s\"\n", sp);
         break;
      } else
         nc = 1;
      df = fopen(dst_path, "wb");
      if (!df) {
         printf("Unable to create \"%s\"!\n", dst_path);
         break;
      }
      step = fi.fsize>1024*1024 ? 1024*1024 : fi.fsize;
      mem  = (BYTE*)malloc(step);
      pos  = 0;
      while (pos<fi.fsize) {
         UINT  br;
         rc = f_read(&file, mem, step, &br);
         if (rc) {
            printf("Error reading \"%s\"!\n", sp);
            break;
         }
         br = fwrite(mem, 1, step, df);
         if (br!=step) {
            printf("Error writing \"%s\"!\n", dst_path);
            break;
         }
         pos += step;
         if (fi.fsize - pos < step) step = fi.fsize - pos;
      }
      fclose(df);

      if (pos==fi.fsize) {
#ifndef SP_LINUX
         int handle;
         // ugly way to set time
         if (_dos_open(dst_path, O_RDWR, &handle)==0) {
            _dos_setftime(handle, fi.fdate, fi.ftime);
            _dos_close(handle);
         }
#else // even watcom is not fully cross-compatible
         struct utimbuf ft;
         ft.actime  = dostimetotime((u32t)fi.fdate<<16|fi.ftime);
         ft.modtime = ft.actime;
         utime(dst_path, &ft);
#endif
      } else 
         unlink(dst_path);
      free(mem);
   } while(0);
   free(sp);
   if (nc) f_close(&file);
   if (!phys_io) fclose(disk);
   if (rc!=FR_OK) exit(4);
}

void format_EF(char *imgname) {
   media    dst;
   int   fmtres = -1;

   open_image(imgname, &dst);
   if ((dst.type&MTYPE_TYPEMASK)!=MTYPE_MBR || (dst.type&MTYPE_EF)==0) {
      printf("Only primary MBR partition type EF can be accepted\n");
   } else {
      FRESULT rc = f_mount(&fat, diskspath, 1);
      if (rc==FR_OK) {
         printf("Partition type EF is FAT already\n");
      } else {
         // for volume serial number
         set_fattime(time(0));

         fmtres = format(0, disk_size, 0, dst.heads, dst.spt, 512, disk_start);
         if (fmtres<0) printf("mkfs error %i\n", -fmtres);
      }
   }
   if (!phys_io) fclose(disk);
   if (fmtres<0) exit(4);
}

int main(int argc, char *argv[]) {
   int nargs = 4;
   if (argc>1)
      if (!stricmp(argv[1],"/read")) nargs=5; else
         if (!stricmp(argv[1],"/info") || !stricmp(argv[1],"/format")) nargs=3;

   if (argc<nargs) {

      printf("  vpcfcopy disk_image dst_pathname src_pathname\n"
             "           copy file to a path on FAT/FAT32 volume in the target disk image\n"
             "           - only short names accepted as \"dst_pathname\"\n"
             "           - disk image can be a floppy, HDD with FAT/FAT32 primary MBR\n"
             "             partition or HDD with EFI boot GPT partition\n"
             "           - MBR type EF has a priority, else 1st found primary FAT is used\n"
             "           - for the RAW floppy image 1/2/4k sector is accepted from BPB\n"
             "           - [OS/2 build] disk_image can be a physical disk, in form ::?\n"
             "             where ? - zero based physical disk number\n"
             "  vpcfcopy /dir disk_image directory\n"
             "           print directory contents (short names only!)\n"
             "  vpcfcopy /read disk_image dst_pathname src_pathname\n"
             "           read a file from the image and save it to dst_pathname \n"
             "  vpcfcopy /info disk_image\n"
             "           print disk image detection results\n"
             "  vpcfcopy /boot disk_image boot_file_name\n"
             "           update boot sector, example of \"boot_file_name\" is OS2LDR\n"
             "           - FAT and FAT32 supported\n"
             "  vpcfcopy /bootext disk_image boot_sector_file\n"
             "           replace boot sector code to another one from a file\n"
             "           - FAT only\n"
             "  vpcfcopy /create[EF] disk_image 1.44|XDF|2.88|size\n"
             "           where \"size\" 1..2047 (mb) - creates a RAW hard disk image,\n"
             "           - use disk_image.VHD to create a fixed size VHD image\n"
             "           - hdd image will have single FAT partition and valid LVM data\n"
             "           - /createEF creates the same, but partition type is EF\n"
             "  vpcfcopy /floppy disk_image 1.44|XDF|2.88|size\n"
             "           same as \"/create\" but forced (big) floppy format instead\n"
             "  vpcfcopy /creategpt disk_image size\n"
             "           same as \"/create\" but HDD only, GPT partition table with\n"
             "           single EFI FAT boot volume on it.\n"
             "  vpcfcopy /format disk_image\n"
             "           format type EF (only!) primary partition to FAT\n"
             "  note:  for the physical disk \"create/createEF/creategpt\" command gets\n"
             "         only an empty disk and \"size\" is a FAT volume size in this case\n");
      return 1;
   }
   if (stricmp(argv[1],"/create")==0) create_disk(argv[2], argv[3], f_mbr); else
   if (stricmp(argv[1],"/createef")==0) create_disk(argv[2], argv[3], f_mbref); else
   if (stricmp(argv[1],"/creategpt")==0) create_disk(argv[2], argv[3], f_gpt); else
   if (stricmp(argv[1],"/floppy")==0) create_disk(argv[2], argv[3], f_fdd); else
   if (stricmp(argv[1],"/boot")==0) update_boot(argv[2], 0, argv[3]); else
   if (stricmp(argv[1],"/bootext")==0) update_boot(argv[2], argv[3], 0); else
   if (stricmp(argv[1],"/read")==0) read_file(argv[2], argv[3], argv[4]); else
   if (stricmp(argv[1],"/dir")==0) print_dir(argv[2], argv[3]); else
   if (stricmp(argv[1],"/info")==0) print_info(argv[2]); else
   if (stricmp(argv[1],"/format")==0) format_EF(argv[2]);
   else {
      struct stat si;
      FILE      *src = 0;
      u32t      size;
      UINT       act;
      FRESULT     rc = FR_OK;
      void     *data = 0;

      check_phys_io(argv[1]);

      if (!phys_io) {
         disk = fopen(argv[1],"r+b");
         if (!disk) {
            printf("unable to open disk file \"%s\"\n", argv[1]);
            return 2;
         }
      }
      // get file time
      if (!stat(argv[3],&si)) set_fattime(si.st_mtime);
      // read file
      src = fopen(argv[3],"rb");
      do {
         media mdst;

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
         // target media
         mdst.phys = phys_io;
         if (phys_io) mdst.dsk = phys_disk; else mdst.df = disk;
         // get partition
         disk_check(&mdst, &disk_start, &disk_size, &disk_shift);

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
                  if (rc!=FR_EXIST && rc!=FR_OK) break;
               }
            } while (cc);
            free(fncopy);
            // open file again
            rc = f_open(&dst, argv[2], FA_WRITE|FA_CREATE_ALWAYS);
         }
         if (rc!=FR_OK) {
            printf("Invalid destination file name (\"%s\") (error %u)\n", argv[2], rc);
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

      if (!phys_io) fclose(disk);
      if (!src) return 3; else fclose(src);
      if (rc!=FR_OK) return 4;
   }
   return 0;
}
