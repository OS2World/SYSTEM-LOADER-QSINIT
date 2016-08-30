#include "vdproc.h"
#include "qsutil.h"
#include "qsio.h"
#include "qcl/qsvdmem.h"

#define MIN_FILE_SIZE    (_1MB)            ///< min image size
#define COPY_BUF_SIZE   (_32KB)

static const char    *vhd_sign = "conectix";
static const u32t vhd_sign_len = 8;

#define IS_VHD_HEADER(sector) (memcmp((sector)+0,vhd_sign,vhd_sign_len)==0)

static u8t    *disk_data = 0;
static u64t    disk_size = 0;
static int         isVHD = 0;
static int        isMfsd = 0;
static io_handle disk_fh = 0;

/* get image file size & type:
   * for common i/o & micro-FSD function checks image size and type
   * for PXE stream - it loads entire file - the only way to get size

   function checks for "conectix" in 0 pos of file (this must be "dynamic" VHD)
   and in zero pos of file`s last sector (512 bytes) - this must be "fixed
   size" VHD - it suiltable for us.
*/
static qserr img_getsize(const char *path, read_callback cbprint) {
   u8t  sector[512];
   qserr   res;
   if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }
   disk_size = 0;
   disk_fh   = 0;
   res = io_open(path, IOFM_OPEN_EXISTING|IOFM_READ|IOFM_SHARE_READ, &disk_fh, 0);

   if (!res) {
      // QSINIT file check
      disk_size = 0;
      res = io_size(disk_fh, &disk_size);
      if (!res) {
         if (disk_size&511) res = E_SYS_BADFMT; else
         if (disk_size < MIN_FILE_SIZE) res = E_SYS_TOOSMALL; else
         if (io_read(disk_fh, sector, 512) != 512) res = E_DSK_IO; else
         if (IS_VHD_HEADER(sector)) res = E_SYS_BADFMT; else // header at start
         if (io_seek(disk_fh,-512,IO_SEEK_END)==FFFF64) res = E_DSK_IO; else
         if (io_read(disk_fh, sector, 512) != 512) res = E_DSK_IO; else {
            isVHD  = IS_VHD_HEADER(sector);
            isMfsd = 0;
         }
      }
   } else
   if (strchr(path,'/')==0 && strchr(path,'\\')==0) {
      if (hlp_boottype()==QSBT_PXE) {
         u32t sized;
         // PXE file pre-load
         disk_data = (u8t*)hlp_freadfull(path, &sized, cbprint);
         disk_size = sized;
         if (disk_data) {
            if (disk_size&511) res = E_SYS_BADFMT; else
            if (disk_size < MIN_FILE_SIZE) res = E_SYS_TOOSMALL; else
            if (IS_VHD_HEADER(disk_data)) res = E_SYS_BADFMT; else {
               isVHD = IS_VHD_HEADER(disk_data+disk_size-512);
               res   = FFFF;
            }
         }
      } else {
         disk_size = hlp_fopen(path);
         // micro-FSD file check
         if (disk_size!=FFFF && disk_size>0) {
            if (disk_size&511) res = E_SYS_BADFMT; else
            if (disk_size < MIN_FILE_SIZE) res = E_SYS_TOOSMALL; else
            if (hlp_fread(0, sector, 512) != 512) res = E_DSK_IO; else
            if (IS_VHD_HEADER(sector)) res = E_SYS_BADFMT; else // header at start
            if (hlp_fread(disk_size-512, sector, 512) != 512) res = E_DSK_IO; else {
               isVHD  = IS_VHD_HEADER(sector);
               isMfsd = 1;
               res    = FFFF;
            }
         }
         hlp_fclose();
      }
   }
   if (res) {
      if (disk_fh) { io_close(disk_fh); disk_fh = 0; }
      if (res==FFFF) res = 0; else {
         if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }
         disk_size = 0;
      }
   }
   if (!res)
      if (isVHD) disk_size -= 512;
   return res;
}


qserr _exicc vdisk_load(EXI_DATA, const char *path, u32t flags, u32t *disk,
                       read_callback cbprint) 
{
   u32t imgsize = 0, dsk;
   qserr    res;
   // invalid args
   if (!path || flags&~(VFDF_NOHIGH|VFDF_NOLOW)) return E_SYS_INVPARM;
   // disk exists?
   if (vdisk_query(0,0,0,0,0,0,0)==0) return E_SYS_INITED;
   // trying to load
   res = img_getsize(path, cbprint);
   if (res) return res;
   /* round up to 1Mb,
      disk size is decremented by header page in sys_vdiskinit(), add it here */
   imgsize = disk_size+PAGESIZE>>20;
   if ((disk_size+PAGESIZE&_1MB-1) != 0) imgsize++;
   /* create disk by fixing size to requested.
      can call vdisk functions directly because we`re here in mutex already */
   res = vdisk_init(0, 0, imgsize, imgsize, flags|VFDF_EMPTY, 0, 0, 0, &dsk);
   // load file data
   if (!res) {
      u32t scount = disk_size>>9;
      // this is PXE, file already in memory
      if (disk_data) {
         if (hlp_diskwrite(dsk, 0, scount, disk_data) != scount) res = E_DSK_IO;
      } else {
         u8t  *buf = malloc(COPY_BUF_SIZE);
         u32t  spb = COPY_BUF_SIZE>>9;           // sectors per buffer
         // opening...
         if (isMfsd) {
            if (hlp_fopen(path) != disk_size+(isVHD?512:0)) res = E_DSK_IO;
         } else {
            if (!disk_fh) res = E_DSK_IO; else
               if (io_seek(disk_fh,0,IO_SEEK_SET)) res = E_DSK_IO;
         }
         if (!res) {
            u32t spos = 0, sprc = FFFF;
            while (spos<scount) {
               u32t scnt = scount-spos>spb ? spb : scount-spos;
               // read sectors
               if (disk_fh) {
                  if (io_read(disk_fh,buf,scnt<<9) != scnt<<9) { res = E_DSK_IO; break; }
               } else {
                  if (hlp_fread(spos<<9,buf,scnt<<9) != scnt<<9) { res = E_DSK_IO; break; }
               }
               // write sectors
               if (hlp_diskwrite(dsk|QDSK_DIRECT,spos,scnt,buf) != scnt) 
                  { res = E_DSK_IO; break; }
               spos += scnt;
               // print pos
               if (cbprint) {
                  u32t np = spos*100/scount;
                  if (np!=sprc) cbprint(sprc=np,0);
               }
            }
            // force rescan
            if (!res) dsk_ptrescan(dsk, 1);
         }
         if (isMfsd) hlp_fclose();
         free(buf);
      }
      // delete disk on image load error
      if (res) vdisk_free(0,0);
   }
   if (disk_fh) { io_close(disk_fh); disk_fh = 0; }

   if (res) {
      disk_size = 0;
      if (disk_data) { hlp_memfree(disk_data); disk_data = 0; }
   } else
   if (*disk) *disk = dsk;

   return res;
}
