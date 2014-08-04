/*****************************************************************************
   copy file to virtual pc hard disk image with single FAT primary partition
   warning! it does not create path for file!
 *****************************************************************************/

// turn LFN off, we are not QSINIT
#include "ffconf.h"
#undef  _USE_LFN
#define _USE_LFN 0
#include "ff.h"
#include "diskio.h"
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
// include FAT code directly here, to let them catch _USE_LFN above ;) 
#include "ff.c"

FILE *disk;
FATFS  fat;
FIL    dst;

DWORD  Disk1Size;
int was_inited=0;

DWORD fsize(FILE *fp) {
  DWORD save_pos, size_of_file;
  save_pos=ftell(fp);
  fseek(fp,0,SEEK_END);
  size_of_file=ftell(fp);
  fseek(fp,save_pos,SEEK_SET);
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
)
{
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
)
{
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
)
{
   //log_misc("disk_write(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   if (drv>0) return RES_WRPRT;
   if (fseek(disk,sector<<9,SEEK_SET)) return RES_ERROR;
   if (fwrite(buff,1,count<<9,disk)!=count<<9) return RES_ERROR;
   return 0;
}

DWORD fat_filetime = 0;
DWORD get_fattime (void) { return fat_filetime; }


int main(int argc, char *argv[]) {
   if (argc<4) {
      printf("usage: vpcfcopy disk_image dst_pathname src_pathname\n");
      return 1;
   }
   disk=fopen(argv[1],"r+b");
   if (!disk) {
      printf("unable to open disk file \"%s\"\n",argv[1]);
      return 2;
   } else {
      static const char *dp = "0:";
      struct stat si;
      FILE  *src=0;
      DWORD size;
      UINT   act;
      FRESULT rc=FR_OK;
      void *data=0;
      // get file time
      if (!stat(argv[3],&si)) {
         struct tm ft;
         _localtime(&si.st_mtime,&ft);
         ft.tm_year   += 1900;
         fat_filetime = ft.tm_year<1980||ft.tm_year>2099?0:
            (DWORD)ft.tm_year-1980<<25|(DWORD)ft.tm_mon+1<<21|ft.tm_mday<<16|
               ft.tm_hour<<11|ft.tm_min<<5|ft.tm_sec>>1;
      }
      // read file
      src=fopen(argv[3],"r+b");
      do {

         if (!src) {
            printf("unable to open source file \"%s\"\n",argv[3]);
            break;
         }
         size = fsize(src);
         if (size) {
            data=malloc(size);
            act =fread(data,1,size,src);
            if (act!=size) { fclose(src); src=0; break; }
         }
         Disk1Size=fsize(disk);
         rc=f_mount(&fat,dp,0);
         if (rc!=FR_OK) { printf("FAT mount error %d\n",rc); break; }
         rc=f_open(&dst, argv[2], FA_WRITE|FA_CREATE_ALWAYS);
         if (rc!=FR_OK) {
            printf("Invalid destination file name\"%s\"\n",argv[2]);
            break;
         }
         if (size) {
            rc=f_write(&dst,data,size,&act);
            if (rc!=FR_OK) {
               printf("File write error!\n");
               break;
            }
            f_close(&dst);
         }
      } while(0);

      fclose(disk);
      if (!src) return 3; else fclose(src);
      if (rc!=FR_OK) return 4;
   }
   return 0;
}
