//
// QSINIT
// plain binary file images
// ------------------------------------------------------------------
//
#include "vhdd.h"
#include "sys/fs/msvhd.h"

/// "qs_vhddisk" class data
typedef struct {
   u32t           sign;
   qs_dyndisk  selfptr;    // pointer to own instance
   s32t         qsdisk;
   io_handle        df;    // disk file
   u64t       startpos;
   char        *dfname;    // disk file name
   int              ro;
   disk_geo_data  info;
   qs_extdisk    einfo;    // attached to this disk ext.info class
   u8t        secshift;    // bit shift for the sector size (9..12)
} diskdata;

static qserr _exicc dsk_close(EXI_DATA);

/// create new image file
static qserr _exicc dsk_make(EXI_DATA, const char *fname, u32t sectorsize, u64t sectors) {
   qserr     rc;
   int   sshift = bsf32(sectorsize);
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   // already opened?
   if (di->df) return E_SYS_INITED;
   // non-zero single bit in range 512..4096
   if (sshift!=bsr32(sectorsize) || sshift<9 || sshift>12) return E_DSK_SSIZE;
   // we have no support for such a huge partition
   if (sectors>=FFFF) return E_SYS_TOOLARGE;
   // create it (with "delete on close" flag initially), but disallow replacement
   rc = io_open(fname, IOFM_CREATE_NEW|IOFM_READ|IOFM_WRITE|IOFM_SHARE_REN|
                IOFM_CLOSE_DEL, &di->df, 0);
   if (!rc) {
      di->info.TotalSectors = sectors;
      di->info.SectorSize   = sectorsize;
      di->info.Heads        = 0;
      di->info.SectOnTrack  = 0;
      di->info.Cylinders    = 0;
      // fill missing chs values
      rc = dsk_fakechs(&di->info);

      if (!rc) rc = io_setsize(di->df, sectors<<sshift);

      if (rc) dsk_close(data, 0); else {
         di->startpos = 0;
         di->secshift = sshift;
         // detach file to make it process-independent
         io_setstate (di->df, IOFS_DETACHED, 1);
         // reset "delete on close" because of success
         io_setstate(di->df, IOFS_DELONCLOSE, 0);
         // save full file name
         di->dfname = _fullpath(0, fname, 0);
         mem_modblock(di->dfname);
         di->ro = 0;
      }
   }
   return rc;
}

static qserr _exicc dsk_open(EXI_DATA, const char *fname, const char *options) {
   qserr         rc;
   str_list   *opts = 0;
   u32t    sectorsz = 512,
             imglen = 0,     // length in sectors
                spt = 0,
              heads = 0;
   u64t      offset = 0;
   int       sshift = 9,
                 ro = 0;

   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   // already opened?
   if (di->df) return E_SYS_INITED;

   if (options) {
      char *key;
      opts = str_split(options,";");
      key  = str_findkey(opts, "sector", 0);
      if (key) {
         sectorsz = str2ulong(key);
         sshift   = bsf32(sectorsz);
         if (sshift!=bsr32(sectorsz) || sshift<9 || sshift>12) return E_DSK_SSIZE;
      }
      key = str_findkey(opts, "offset", 0);
      if (key) offset = str2uint64(key);
      key = str_findkey(opts, "size", 0);
      if (key) imglen = str2ulong(key);
      key = str_findkey(opts, "spt", 0);
      if (key) spt = str2ulong(key);
      key = str_findkey(opts, "heads", 0);
      if (key) heads = str2ulong(key);
      if (spt>255 || heads>255) return E_DSK_INVCHS;
      // forces read-only mode
      if (str_findentry(opts,"ro",0,1)>=0 || str_findentry(opts,"readonly",0,1)>=0)
        ro = 1;
   }
   // open file
   rc = ro ? E_SYS_ACCESS : io_open(fname, IOFM_OPEN_EXISTING|IOFM_READ|
                                    IOFM_WRITE|IOFM_SHARE_REN, &di->df, 0);
   // deny read prevents second open even in r/o mode
   if (rc==E_SYS_ACCESS) {
      rc = io_open(fname, IOFM_OPEN_EXISTING|IOFM_READ, &di->df, 0);
      if (!rc) ro = 1;
   }

   if (!rc) {
      u64t sz;
      rc = io_size(di->df, &sz);

      if (!rc) {
         // at least one sector after offset
         if (imglen && offset+((u64t)imglen<<sshift) > sz ||
            offset+sectorsz > sz) rc = E_SYS_EOF;
         else {
            if (!imglen) imglen = sz - offset >> sshift;

            di->info.TotalSectors = imglen;
            di->info.SectorSize   = sectorsz;
            di->info.Heads        = heads;
            di->info.SectOnTrack  = spt;
            di->info.Cylinders    = 0;
            // fill missing chs values
            rc = dsk_fakechs(&di->info);

            if (!rc) {
               di->startpos = offset;
               di->secshift = sshift;
               di->ro       = ro;
               // save full file name
               di->dfname   = _fullpath(0, fname, 0);
               mem_modblock(di->dfname);
               // detach file to make it process-independent
               io_setstate (di->df, IOFS_DETACHED, 1);
            }
         }
      }
   }
   if (rc) dsk_close(data, 0);
   return rc;
}

/// query "disk" info
static qserr _exicc dsk_query(EXI_DATA, disk_geo_data *info, char *fname,
                              u32t *sectors, u32t *used, u32t *state)
{
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   if (info)
      if (di->df) memcpy(info, &di->info, sizeof(disk_geo_data));
         else memset(info, 0, sizeof(disk_geo_data));
   if (fname)
      if (di->dfname) strcpy(fname, di->dfname); else *fname = 0;
   if (sectors) *sectors = di->info.TotalSectors;
   if (used)    *used    = di->info.TotalSectors;
   if (state)   *state   = EDSTATE_NOCACHE | (di->ro?EDSTATE_RO:0);
   return 0;
}

/* common read/write function.
   Since total # of sectors limited in mount, use dword for sector. */
static u32t io_common(diskdata *di, u32t sector, u32t count, void *buf,
                      u32t (*io)(io_handle,const void *,u32t))
{
   // no file?
   if (!di->df) return 0;
   // check limits
   if (sector>=di->info.TotalSectors) return 0;
   if (sector+count>di->info.TotalSectors) count = di->info.TotalSectors - sector;

   if (count) {
      if (io_seek(di->df, di->startpos + ((u64t)sector<<di->secshift),
        IO_SEEK_SET)==FFFF64) return 0;
      return io(di->df, buf, count<<di->secshift) >> di->secshift;
   }
   return 0;
}

static u32t _exicc dsk_read(EXI_DATA, u64t sector, u32t count, void *usrdata) {
   instance_ret(diskdata,di,0);
   return io_common(di, sector, count, usrdata, io_read);
}

static u32t _exicc dsk_write(EXI_DATA, u64t sector, u32t count, void *usrdata) {
   instance_ret(diskdata,di,0);
   if (di->ro) return 0;
   return io_common(di, sector, count, usrdata, io_write);
}

/** mount as QSINIT disk.
    @return disk number or negative value on error */
static s32t _exicc dsk_mount(EXI_DATA) {
   struct qs_diskinfo dp;
   instance_ret(diskdata,di,-1);

   memset(&dp, 0, sizeof(dp));
   dp.qd_sectors     = di->info.TotalSectors;
   dp.qd_cyls        = di->info.Cylinders;
   dp.qd_heads       = di->info.Heads;
   dp.qd_spt         = di->info.SectOnTrack;
   dp.qd_extread     = diskio_read;
   dp.qd_extwrite    = diskio_write;
   dp.qd_sectorsize  = di->info.SectorSize;
   dp.qd_sectorshift = di->secshift;

   if (cid_ext) {
      qs_extdisk  eptr = (qs_extdisk)exi_createid(cid_ext, EXIF_SHARED);
      if (eptr) {
         de_data *de  = (de_data*)exi_instdata(eptr);
         de->self     = di->selfptr;
         dp.qd_extptr = eptr;
         di->einfo    = eptr;
      }
   }
   // install new "hdd"
   di->qsdisk = hlp_diskadd(&dp, 0);
   if (di->qsdisk<0 || di->qsdisk>QDSK_DISKMASK) log_printf("disk add error!\n");
      else mounted[di->qsdisk] = di->selfptr;

   return di->qsdisk;
}

/** query QSINIT disk number.
    @return disk number or negative value if not mounted */
static s32t _exicc dsk_disk(EXI_DATA) {
   instance_ret(diskdata,di,-1);
   return di->qsdisk;
}

/// unmount "disk" from QSINIT
static qserr _exicc dsk_umount(EXI_DATA) {
   instance_ret(diskdata,di,E_SYS_INVOBJECT);

   if (di->qsdisk>=0) {
      if (di->qsdisk<=QDSK_DISKMASK) mounted[di->qsdisk] = 0;
      hlp_diskremove(di->qsdisk);
      di->qsdisk = -1;
      if (di->einfo) { DELETE(di->einfo); di->einfo = 0; }
      return 0;
   }
   return E_DSK_NOTMOUNTED;
}

static u32t _exicc dsk_compact(EXI_DATA, int freeF6) { return 0; }

/// close image file
static qserr _exicc dsk_close(EXI_DATA) {
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   if (!di->df) return E_SYS_NONINITOBJ;
   // unmount it first
   if (di->qsdisk>=0) dsk_umount(data, 0);
   // and close file
   io_close(di->df);
   if (di->dfname) free(di->dfname);
   di->df     = 0;
   di->dfname = 0;
   di->ro     = 0;
   return 0;
}

static void _exicc dsk_trace(EXI_DATA, int self, int bitmaps) {
   instance_void(diskdata,di);
   // self - query it, because user able to change it by exi_chainset() too ;)
   if (self>=0) {
      int cstate = exi_chainset(di->selfptr,-1);
      if (cstate>=0)
         if (Xor(self,cstate)) exi_chainset(di->selfptr, self);
   }
}

static qserr _exicc dsk_setgeo(EXI_DATA, disk_geo_data *geo) {
   instance_ret(diskdata, di, E_SYS_INVOBJECT);
   if (!geo) return E_SYS_ZEROPTR;
   if (!di->df) return E_DSK_NOTMOUNTED;
   if (di->ro) return E_DSK_WP;
   if (di->info.TotalSectors != geo->TotalSectors) return E_SYS_INVPARM;
   if (di->info.SectorSize != geo->SectorSize) return E_SYS_INVPARM;

   // check CHS (at least minimal)
   if (!geo->Heads || geo->Heads>255 || !geo->SectOnTrack ||
      geo->SectOnTrack>255 || !geo->Cylinders || (u64t)geo->Cylinders *
         geo->Heads * geo->SectOnTrack > geo->TotalSectors) return E_SYS_INVPARM;

   memcpy(&di->info, geo, sizeof(disk_geo_data));
   // if disk is mounted - update CHS value in system info
   if (di->qsdisk>=0) {
      struct qs_diskinfo *qdi = hlp_diskstruct(di->qsdisk, 0);
      // this is WRONG! patching it in the system struct directly!
      if (qdi) {
         qdi->qd_cyls  = geo->Cylinders;
         qdi->qd_heads = geo->Heads;
         qdi->qd_spt   = geo->SectOnTrack;
      }
   }
   return 0;
}

static void *qs_binimg_list[] = { dsk_make, dsk_open, dsk_query, dsk_read,
   dsk_write, dsk_compact, dsk_mount, dsk_disk, dsk_umount, dsk_close,
   dsk_trace, dsk_setgeo };

static void _std dsk_init(void *instance, void *data) {
   diskdata  *di = (diskdata*)data;
   di->sign      = VHDD_SIGN;
   di->qsdisk    = -1;
   di->df        = 0;
   di->dfname    = 0;
   di->selfptr   = instance;
   di->einfo     = 0;
   di->startpos  = 0;
   di->secshift  = 0;
   /// force mutex for every instance!
   exi_mtsafe(instance, 1);
}

static void _std dsk_done(void *instance, void *data) {
   instance_void(diskdata,di);

   if (di->qsdisk>=0) dsk_umount(data, 0);
   if (di->einfo) DELETE(di->einfo);
   if (di->df) dsk_close(data, 0);

   memset(di, 0, sizeof(diskdata));
}

u32t init_bfdisk(void) {
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_dyndisk)!=sizeof(qs_binimg_list)) {
      log_printf(VHDD_VHD_B ": function list mismatch!\n");
      return 0;
   }
   return exi_register(VHDD_VHD_B, qs_binimg_list, sizeof(qs_binimg_list)/
      sizeof(void*), sizeof(diskdata), 0, dsk_init, dsk_done, 0);
}
