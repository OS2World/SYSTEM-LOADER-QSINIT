#include "stdlib.h"
#include "errno.h"
#include "qsutil.h"
#include "qsvdisk.h"
#include "qsdm.h"

#define MIN_FILE_SIZE    (_1MB)            ///< min image size
#define COPY_BUF_SIZE   (_32KB)

static const char    *vhd_sign = "conectix";
static const u32t vhd_sign_len = 8;

#define IS_VHD_HEADER(sector) (memcmp((sector)+0,vhd_sign,vhd_sign_len)==0)

static u8t  *disk_data = 0;
static u32t  disk_size = 0;
static int       isVHD = 0;
static int      isMfsd = 0;

/* get image file size & type:
   * for stdio FILE & micro-FSD function checks image size and type
   * for PXE stream - it loads entire file - the only way to get size

   function checks for "conectix" in 0 pos of file (this must be "dynamic" VHD)
   and in zero pos of last sector (512 bytes) of file - this must be "fixed
   size" VHD - it suiltable for us.
*/
static int img_getsize(const char *path, read_callback cbprint) {
   u8t  sector[512];
   FILE    *fp = fopen(path,"rb");
   int      rc = ENOENT;

   disk_size = 0;
   if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }

   if (fp) {
      // QSINIT file check
      disk_size = fsize(fp);
      if ((disk_size&511) != 0) rc = ENOTBLK; else
      if (disk_size < MIN_FILE_SIZE) rc = EFBIG; else
      if (fread(sector, 1, 512, fp) != 512) rc = EIO; else
      if (IS_VHD_HEADER(sector)) rc = ENOTBLK; else // header at start
      if (fseek(fp,-512,SEEK_END)) rc = EIO; else
      if (fread(sector, 1, 512, fp) != 512) rc = EIO; else {
         isVHD  = IS_VHD_HEADER(sector);
         rc     = 0;
         isMfsd = 0;
      }
   } else 
   if (strchr(path,'/')==0 && strchr(path,'\\')==0) {
      if (hlp_boottype()==QSBT_PXE) {
         // PXE file pre-load
         disk_data = (u8t*)hlp_freadfull(path, &disk_size, cbprint);
         if (disk_data) {
            if ((disk_size&511) != 0) rc = ENOTBLK; else
            if (disk_size < MIN_FILE_SIZE) rc = EFBIG; else
            if (IS_VHD_HEADER(disk_data)) rc = ENOTBLK; else {
               isVHD = IS_VHD_HEADER(disk_data+disk_size-512);
               rc    = 0;
            }
         }
      } else {
         disk_size = hlp_fopen(path);
         // micro-FSD file check
         if (disk_size!=FFFF && disk_size>0) {
            if ((disk_size&511) != 0) rc = ENOTBLK; else
            if (disk_size < MIN_FILE_SIZE) rc = EFBIG; else
            if (hlp_fread(0, sector, 512) != 512) rc = EIO; else
            if (IS_VHD_HEADER(sector)) rc = ENOTBLK; else // header at start
            if (hlp_fread(disk_size-512, sector, 512) != 512) rc = EIO; else {
               isVHD  = IS_VHD_HEADER(sector);
               rc     = 0;
               isMfsd = 1;
            }
         }
         hlp_fclose();
      }
   }
   if (fp) fclose(fp);
   if (rc) {
      disk_size = 0;
      if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }
   } else
   if (isVHD) disk_size -= 512;

   return rc;
}


u32t _std sys_vdiskload(const char *path, u32t flags, u32t *disk, read_callback cbprint) {
   u32t imgsize = 0, dsk;
   int      res;
   // invalid args
   if (!path || flags&~(VFDF_NOHIGH|VFDF_NOLOW)) return EINVAL;
   // disk exists?
   if (sys_vdiskinfo(0, 0, 0)) return EEXIST;
   // trying to load
   res = img_getsize(path, cbprint);
   if (res) return res;
   /* round up to 1Mb,
      disk size is decremented by header page in sys_vdiskinit(), add it here */
   imgsize = disk_size+PAGESIZE>>20;
   if ((disk_size+PAGESIZE&_1MB-1) != 0) imgsize++;
   // create disk by fixing size to requested
   res = sys_vdiskinit(imgsize, imgsize, flags|VFDF_EMPTY, 0, 0, 0, &dsk);
   // load file data
   if (!res) {
      u32t scount = disk_size>>9;
      // this is PXE, file already in memory
      if (disk_data) {
         if (hlp_diskwrite(dsk, 0, scount, disk_data) != scount) res = EIO;
      } else {
         u8t  *buf = malloc(COPY_BUF_SIZE);
         u32t  spb = COPY_BUF_SIZE>>9;           // sectors per buffer
         FILE  *fp = 0;
         // opening...
         if (isMfsd) {
            if (hlp_fopen(path) != disk_size+(isVHD?512:0)) res = EIO;
         } else {
            fp = fopen(path,"rb");
            if (!fp || fsize(fp) != disk_size+(isVHD?512:0)) res = EIO;
         }
         if (!res) {
            u32t spos = 0, sprc = FFFF;
            while (spos<scount) {
               u32t scnt = scount-spos>spb ? spb : scount-spos;
               // read sectors
               if (fp) {
                  if (fread(buf,1,scnt<<9,fp) != scnt<<9) { res = EIO; break; }
               } else {
                  if (hlp_fread(spos<<9,buf,scnt<<9) != scnt<<9) { res = EIO; break; }
               }
               // write sectors
               if (hlp_diskwrite(dsk|QDSK_DIRECT,spos,scnt,buf) != scnt) { res = EIO; break; }
               spos += scnt;
               // print pos
               if (cbprint) {
                  u32t np = spos*100/scount;
                  if (np!=sprc) cbprint(sprc=np,0);
               }
            }
            res = 0;
            // force rescan
            dsk_ptrescan(dsk, 1);
         }
         // closing...
         if (fp) fclose(fp); else
         if (isMfsd) hlp_fclose();
         free(buf);
      }
      // delete disk on image load error
      if (res) sys_vdiskfree();
   }
   
   if (res) {
      disk_size = 0;
      if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }
   } else
   if (*disk) *disk = dsk;

   return res;
}
