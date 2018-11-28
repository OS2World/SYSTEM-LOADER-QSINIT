//
// QSINIT
// Virtual PC fixed size images
// ------------------------------------------------------------------
//
#include "vhdd.h"
#include "qsdm.h"
#include "sys/fs/msvhd.h"

/// "qs_vhddisk" class data
typedef struct {
   u32t           sign;
   qs_emudisk  selfptr;    // pointer to own instance
   s32t         qsdisk;
   io_handle        df;    // disk file
   char        *dfname;    // disk file name
   disk_geo_data  info;
   qs_extdisk    einfo;    // attached to this disk ext.info class
   vhd_footer     fhdr;
} diskdata;

static u32t vpc_checksum(vhd_footer* buf, u32t len) {
   u32t res = 0;
   int   ii;
   for (ii=0; ii<len; ii++) res+=((u8t*)buf)[ii];
   return ~res;
}

static qserr _exicc dsk_close(EXI_DATA);

/// create new image file
static qserr _exicc dsk_make(EXI_DATA, const char *fname, u32t sectorsize, u64t sectors) {
   qserr    rc;
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   // already opened?
   if (di->df) return E_SYS_INITED;
   if (sectorsize!=512) return E_DSK_SSIZE;
   // we have no support for such huge partitions
   if (sectors>=FFFF) return E_SYS_TOOLARGE;
   // create it (with "delete on close" flag initially), but disallow replacement
   rc = io_open(fname, IOFM_CREATE_NEW|IOFM_READ|IOFM_WRITE|IOFM_SHARE_REN|
                IOFM_CLOSE_DEL, &di->df, 0);
   if (!rc) {
      u32t  nsec = sectors,   // cast it to 32bit, thanks to limit above
            cyls = 1024, spt = 17, heads = 3, cyls_bios;
      u16t  heads_bios;

      while (cyls*spt*heads < nsec) {
         if (spt<63)    { spt   = (spt   + 1 & 0xF0) * 2 - 1; continue; }
         if (heads<255) { heads = (heads + 1) * 2 - 1; continue; }
         break;
      }
      cyls = nsec / (spt*heads);
      if (cyls<65535) nsec = cyls*spt*heads;
      // vhd wants dumb bios geometry in header, at least, as QEMU says
      if (heads>16) {
         heads_bios = 16;
         cyls_bios  = nsec / (spt*16);
         while (cyls_bios*heads_bios*spt < cyls*spt*heads) cyls_bios++;
      } else {
         heads_bios = heads;
         cyls_bios  = cyls;
      }
      rc = io_setsize(di->df, (u64t)nsec+1<<9);
      if (!rc) {
         // write VHD header (footer, actually)
         time_t     crtime = time(0) - VHD_TIME_BASE;
         
         memset(&di->fhdr, 0, sizeof(vhd_footer));
         memcpy(&di->fhdr.creator, "conectix", 8);
         memcpy(&di->fhdr.appname, "vpc ", 4);
         memcpy(&di->fhdr.osname , "Wi2k", 4);
         di->fhdr.features = bswap32(2);
         di->fhdr.version  = bswap32(0x10000);
         di->fhdr.nextdata = FFFF64;
         di->fhdr.crtime   = bswap32(crtime);
         di->fhdr.major    = bswap16(5);
         di->fhdr.minor    = bswap16(3);
         di->fhdr.org_size = bswap64(nsec << 9);
         di->fhdr.cur_size = di->fhdr.org_size;
         di->fhdr.cyls     = bswap16(cyls_bios);
         di->fhdr.heads    = heads_bios;
         di->fhdr.spt      = spt;
         di->fhdr.type     = bswap32(2);
         
         dsk_makeguid(&di->fhdr.uuid);
         
         di->fhdr.checksum = bswap32(vpc_checksum(&di->fhdr, sizeof(vhd_footer)));
         
         if (io_seek(di->df, -512, IO_SEEK_END)==FFFF64)
            rc = io_lasterror(di->df);
         else {
            // make sector zero-filled to be safe with all of VHD users
            void *sb = calloc_thread(512, 1);
            memcpy(sb, &di->fhdr, sizeof(vhd_footer));
            if (io_write(di->df, sb, 512)!=512) rc = E_DSK_ERRWRITE;
            free(sb);
         }
      }
      if (rc) dsk_close(data, 0); else {
         di->info.TotalSectors = nsec;
         di->info.SectorSize   = 512;
         di->info.Cylinders    = cyls_bios;
         di->info.Heads        = heads_bios;
         di->info.SectOnTrack  = spt; 
         // detach file to make it process-independent
         io_setstate (di->df, IOFS_DETACHED, 1);
         // reset "delete on close" because of success
         io_setstate(di->df, IOFS_DELONCLOSE, 0);
         // save full file name
         di->dfname = _fullpath(0, fname, 0);
         mem_modblock(di->dfname);
      }
   }
   return rc;
}

/// open existing image file
static qserr _exicc dsk_open(EXI_DATA, const char *fname) {
   qserr  rc;
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   // already opened?
   if (di->df) return E_SYS_INITED;
   // open file
   rc = io_open(fname, IOFM_OPEN_EXISTING|IOFM_READ|IOFM_WRITE|IOFM_SHARE_REN, &di->df, 0);
   if (!rc)
      if (io_seek(di->df, -512, IO_SEEK_END)==FFFF64) rc = io_lasterror(di->df); else
      if (io_read(di->df, &di->fhdr, sizeof(vhd_footer))!=sizeof(vhd_footer))
         rc = E_DSK_ERRREAD;
      else
      if (memcmp(&di->fhdr.creator, "conectix", 8) || bswap32(di->fhdr.type)!=2)
         rc = E_SYS_BADFMT;
      else {
         u64t sz;
         rc = io_size(di->df, &sz);

         if (!rc) {
            di->info.TotalSectors = bswap64(di->fhdr.cur_size)>>9;
            di->info.SectorSize   = 512;
            di->info.Cylinders    = bswap16(di->fhdr.cyls);
            di->info.Heads        = di->fhdr.heads;
            di->info.SectOnTrack  = di->fhdr.spt;
            // even if this case is impossible - just check it here
            if (di->info.TotalSectors>=FFFF) {
               log_it(3, "VHD is too large!\n");
               rc = E_SYS_BADFMT;
            } else
            if (sz>>9 < di->info.TotalSectors+1) {
               log_it(3, "VHD size is invalid (%s) -> %Lu<%Lu!\n", fname, (u64t)sz>>9, di->info.TotalSectors+1);
               rc = E_SYS_BADFMT;
            }
            if (!rc) {
               // save full file name
               di->dfname = _fullpath(0, fname, 0);
               mem_modblock(di->dfname);
               // detach file to make it process-independent
               io_setstate (di->df, IOFS_DETACHED, 1);
            }
         }
      }
   if (rc) dsk_close(data, 0);
   return rc;
}

/// query "disk" info
static qserr _exicc dsk_query(EXI_DATA, disk_geo_data *info, char *fname, u32t *sectors, u32t *used) {
   instance_ret(diskdata,di,E_SYS_INVOBJECT);
   if (info)
      if (di->df) memcpy(info, &di->info, sizeof(disk_geo_data));
         else memset(info, 0, sizeof(disk_geo_data));
   if (fname)
      if (di->dfname) strcpy(fname, di->dfname); else *fname = 0;
   if (sectors) *sectors = di->info.TotalSectors;
   if (used)    *used    = di->info.TotalSectors;
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
      if (io_seek(di->df, (u64t)sector<<9, IO_SEEK_SET)==FFFF64) return 0;
      return io(di->df, buf, count<<9) >> 9;
   }
   return 0;
}

static u32t _exicc dsk_read(EXI_DATA, u64t sector, u32t count, void *usrdata) {
   instance_ret(diskdata,di,0);
   return io_common(di, sector, count, usrdata, io_read);
}

static u32t _exicc dsk_write(EXI_DATA, u64t sector, u32t count, void *usrdata) {
   instance_ret(diskdata,di,0);
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
   dp.qd_sectorsize  = 512;
   dp.qd_sectorshift = 9;

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
   if (di->info.TotalSectors != geo->TotalSectors) return E_SYS_INVPARM;
   if (di->info.SectorSize != geo->SectorSize) return E_SYS_INVPARM;

   // check CHS (VHD has limits for it)
   if (!geo->Heads || geo->Heads>16 || !geo->SectOnTrack ||
      geo->SectOnTrack>255 || !geo->Cylinders) return E_SYS_INVPARM;
   else {
      // 0xFFFF looks dangerous, so limit it to 65564
      u16t cyls = geo->Cylinders>=0xFFFF ? 0xFFFE : geo->Cylinders;

      if (io_seek(di->df, -512, IO_SEEK_END)==FFFF64)
         return io_lasterror(di->df);

      di->fhdr.cyls     = bswap16(cyls);
      di->fhdr.heads    = geo->Heads;
      di->fhdr.spt      = geo->SectOnTrack;
      di->fhdr.checksum = 0;
      di->fhdr.checksum = bswap32(vpc_checksum(&di->fhdr, sizeof(vhd_footer)));

      if (io_write(di->df, &di->fhdr, sizeof(vhd_footer))!=sizeof(vhd_footer))
         return E_DSK_ERRWRITE;
      /* even if we wrote truncated value into the footer, we store
         it full in own data until unmount.
         nothing special depends on it, because setgeo exists for changing
         "sectors per track" value basically (by dmgr clone command)
       */
      memcpy(&di->info, geo, sizeof(disk_geo_data));
      // update CHS value in system info if file is mounted as disk now
      if (di->qsdisk>=0) {
         struct qs_diskinfo *qdi = hlp_diskstruct(di->qsdisk, 0);
         // this is WRONG! patching it in system struct directly!
         if (qdi) {
            qdi->qd_cyls  = geo->Cylinders;
            qdi->qd_heads = geo->Heads;
            qdi->qd_spt   = geo->SectOnTrack;
         }
      }
   }
   return 0;
}

static void *qs_emudisk_list[] = { dsk_make, dsk_open, dsk_query, dsk_read,
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

u32t init_vhdisk(void) {
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_emudisk)!=sizeof(qs_emudisk_list)) {
      log_printf("function list mismatch!\n");
      return 0;
   }
   return exi_register(VHDD_VHD_F, qs_emudisk_list, sizeof(qs_emudisk_list)/
      sizeof(void*), sizeof(diskdata), 0, dsk_init, dsk_done, 0);
}
