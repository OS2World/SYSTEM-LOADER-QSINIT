#include "diskio.h"
#include "qsint.h"
#include "qsinit.h"
#include "qsutil.h"
#include "qssys.h"
#include "clib.h"
#include "ffconf.h"
#include "qsconst.h"
#include "dskinfo.h"
#include "qsmodint.h"
#ifndef EFI_BUILD
#include "ioint13.h"
#include "filetab.h"
extern struct   Disk_BPB BootBPB;
#endif
extern void           *Disk1Data;
extern u32t            Disk1Size;
extern u32t            DiskBufPM; // flat disk buffer address
extern
struct qs_diskinfo      qd_array[MAX_QS_DISK];
extern u8t      qd_fdds, qd_hdds;
extern int            qd_bootidx; // index of boot disk info in qd_array, -1 if no
extern u8t          dd_bootflags, // boot flags
                     dd_bootdisk,
                    bootio_avail;
extern u16t               cp_num;
extern vol_data*          extvol;
extern u8t*                ExCvt; // FatFs OEM case conversion
cache_extptr        *cache_eproc;
extern
mod_addfunc       *mod_secondary; // secondary function table, from "start" module
// real mode thunk (this thunk pop parameters from stack)
// disk info parameter must be placed in DGROUP (16-bit offset used in RM)
u8t _std raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write);
int _std sys_intstate(int on);

// cache control
void _std cache_ctrl(u32t action, u8t vol);
// bss is zeroed
static char was_inited[_VOLUMES];
// Initialize a Drive
DSTATUS disk_initialize(BYTE drv) {
   if (drv>=_VOLUMES) return STA_NOINIT;
#if 0 //def INITDEBUG
   log_misc(2,"disk_init(%d)\n",(DWORD)drv);
#endif
   cache_ctrl(CC_RESET, drv);
   was_inited[drv]=1;
   return 0;
}

// Return Disk Status
DSTATUS disk_status(BYTE drv) {
   return !was_inited[drv]?STA_NOINIT:0;
}

DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, UINT count) {
   if (!count) return RES_OK;
   if (!buff) return RES_PARERR;
   //log_misc(2,"disk_read(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   switch (drv) {
      case 1:
         memcpy(buff,(char*)Disk1Data+(sector<<LDR_SSHIFT),count<<LDR_SSHIFT);
         break;
      case 0: 
         if (!bootio_avail) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            DRESULT    res = RES_OK;
            mt_swlock();
            if (drv&&!vdta->flags) res = RES_NOTRDY; else
            if (hlp_diskread(vdta->disk, vdta->start+sector, count, buff) != count)
               res = RES_ERROR;
            mt_swunlock();
            return res;
         } else
            return RES_ERROR;
         break;
   }
   return RES_OK;
}

DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff) {
   //log_misc(2,"disk_ioctl(%d,%d,%X)\n",(DWORD)drv,(DWORD)ctrl,buff);
   if (ctrl==CTRL_SYNC) {
      cache_ctrl(CC_SYNC, drv);
      return RES_OK;
   }
   if (ctrl==GET_BLOCK_SIZE) { *(DWORD*)buff=1; return RES_OK; }

   if (ctrl==GET_SECTOR_SIZE || ctrl==GET_SECTOR_COUNT)
   switch (drv) {
      case 1:
         if (!Disk1Size) return RES_NOTRDY;
         if (ctrl==GET_SECTOR_SIZE ) *(WORD*) buff = LDR_SSIZE; else
         if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff = Disk1Size>>LDR_SSHIFT;
         return RES_OK;
      case 0:
         if (!bootio_avail) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            DRESULT    res = RES_OK;
            mt_swlock();
            if (drv&&!vdta->flags) res = RES_NOTRDY; else {
               if (ctrl==GET_SECTOR_SIZE ) *(WORD*) buff = vdta->sectorsize; else
               if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff = vdta->length;
            }
            mt_swunlock();
            return res;
         } else
            return RES_ERROR;
         break;
   }
   return RES_PARERR;
}

DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, UINT count) {
   if (!count) return RES_OK;
   if (!buff) return RES_PARERR;
   //log_misc(2,"disk_write(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   switch (drv) {
      case 1:
         memcpy((char*)Disk1Data+(sector<<LDR_SSHIFT),(void*)buff,count<<LDR_SSHIFT);
         break;
      case 0: 
         if (!bootio_avail) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            DRESULT    res = RES_OK;
            mt_swlock();
            if (drv&&!vdta->flags) res = RES_NOTRDY; else
            if (hlp_diskwrite(vdta->disk, vdta->start+sector, count, (void*)buff) != count)
               res = RES_ERROR;
            mt_swunlock();
            return res;
         } else
            return RES_ERROR;
         break;
   }
   return RES_OK;
}

u32t _std hlp_diskcount(u32t *floppies) {
   u32t rc = 0, ii;
   mt_swlock();
   /* number of floppies is not touched duaring QS work, but hdd number can
      be altered by hlp_diskadd() */
   for (ii=MAX_QS_FLOPPY; ii<MAX_QS_DISK; ii++)
      if (qd_array[ii].qd_flags&HDDF_PRESENT) rc = ii+1-MAX_QS_FLOPPY;
   if (floppies) *floppies = qd_fdds;
   mt_swunlock();
   return rc;
}

u32t _std hlp_disksize(u32t disk, u32t *sectsize, disk_geo_data *geo) {
   u32t            result = 0;
   u8t                idx = disk&QDSK_DISKMASK;
   struct qs_diskinfo *qe = 0;
   if (geo) memset(geo, 0, sizeof(disk_geo_data));

   if (disk & QDSK_VOLUME) { // mounted volume
      if (sectsize||geo) {
         u32t dsize = 0;
         disk_ioctl(idx,GET_SECTOR_SIZE,&dsize);
         if (sectsize) *sectsize = dsize;
         if (geo) geo->SectorSize = dsize;
      }
      if (disk_ioctl(idx,GET_SECTOR_COUNT,&result)!=RES_OK)
         result = 0;
      if (result && geo) geo->TotalSectors = result;
   } else  {
      mt_swlock();
      if (disk & QDSK_FLOPPY) {  // floppy disk
         if (idx<MAX_QS_FLOPPY) {
            qe     = qd_array + idx;
            result = (u32t)qe->qd_sectors;
            if (geo) geo->TotalSectors = result;
         }
      } else 
      if ((disk & ~QDSK_DISKMASK)==0) { // hdd
         if (idx<MAX_QS_DISK-MAX_QS_FLOPPY && (qd_array[idx+MAX_QS_FLOPPY].qd_flags&HDDF_PRESENT)!=0) {
            qe     = qd_array + idx + MAX_QS_FLOPPY;
            result = (u32t)qe->qd_sectors;
            if (geo) {
               geo->TotalSectors = qe->qd_sectors;
               /* limit REAL disks to 24Tb now - larger values truncating to
                  low dword (because of older BIOS bugs - garbage in high dword) */
               if ((qe->qd_flags&HDDF_HOTSWAP)==0)
                  if (geo->TotalSectors>_4GBLL * 12) geo->TotalSectors = result;
            }
         }
      }
      if (qe) {
         if (sectsize) *sectsize = qe->qd_sectorsize;
         if (geo && qe) {
            geo->SectorSize   = qe->qd_sectorsize;
            geo->Cylinders    = qe->qd_cyls;
            geo->Heads        = qe->qd_heads;
            geo->SectOnTrack  = qe->qd_spt;
         }
      }
      mt_swunlock();
   }
   return result;
}

u64t _std hlp_disksize64(u32t disk, u32t *sectsize) {
   disk_geo_data geo;
   hlp_disksize(disk, sectsize, &geo);
   return geo.TotalSectors;
}

static u32t action(int write, u32t disk, u64t sector, u32t count, void *data) {
   static u8t nestlevel = 0;
   u32t   rc = 0;
   // lock it! this saves nestlevel usage too
   mt_swlock();
   // try to ask cache for action
   if (cache_eproc && !nestlevel) {
      nestlevel = 1;
      if ((disk&QDSK_DIRECT)==0 || write)
         rc = (*(write?cache_eproc->cache_write:cache_eproc->cache_read))
            (disk,sector,count,data);
      nestlevel = 0;
   }
   // process i/o
   if (rc<count) {
      u8t     idx = disk&QDSK_DISKMASK;
      struct qs_diskinfo *qe = 0;
      
      if (disk & QDSK_FLOPPY) {  // floppy disk
         if (idx<MAX_QS_FLOPPY) qe = qd_array + idx;
      } else
      if ((disk & ~(QDSK_DISKMASK|QDSK_DIRECT))==0) { // hdd
         u8t flags = qd_array[idx+MAX_QS_FLOPPY].qd_flags;
         if (idx<MAX_QS_DISK-MAX_QS_FLOPPY && (flags&HDDF_PRESENT)!=0) {
            qe = qd_array + idx + MAX_QS_FLOPPY;
            if (flags&HDDF_HOTSWAP) {
               diskio_func fp = write?qe->qd_extwrite:qe->qd_extread;
               rc = fp ? fp(disk,sector,count,data) : 0;
               qe = 0;
            }
         }
      }
      if (qe) {
         u32t s32k = DISKBUF_SIZE >> qe->qd_sectorshift;
         if (rc) {
            data = (u8t*)data + (rc << qe->qd_sectorshift);
            sector += rc;
            count  -= rc;
         }
         rc = count;
         do {
            u32t actsize = count>s32k?s32k:count;
            int     step;
            count  -= actsize;
         
            for (step=0; step<2; step++)
               if (step^write) {
                  // move from/to buffer
                  u32t szm = actsize << qe->qd_sectorshift;
                  memcpy(write?(void*)DiskBufPM:data, write?data:(void*)DiskBufPM, szm);
                  data = (u8t*)data + szm;
               } else {
                  int err = 0;
                  if (sector + actsize > qe->qd_sectors) err = 1; else
                  // read/write by 32k via buffer in 1st Mb
                  if (raw_io(qe, sector, actsize, write)) err = 1;
                  // unlock and exit
                  if (err) { mt_swunlock(); return 0; }
                  sector += actsize;
               }
         } while (count);
      }
   }
   mt_swunlock();
   return rc;
}

u32t _std hlp_diskread(u32t disk, u64t sector, u32t count, void *data) {
   if (disk & QDSK_VOLUME)
      return disk_read(disk&QDSK_DISKMASK,data,sector,count)==RES_OK?count:0;
   else
      return action(0,disk,sector,count,data);
}

u32t _std hlp_diskwrite(u32t disk, u64t sector, u32t count, void *data) {
   if (disk & QDSK_VOLUME)
      return disk_write(disk&QDSK_DISKMASK,data,sector,count)==RES_OK?count:0;
   else
      return action(1,disk,sector,count,data);
}

u32t _std hlp_diskmode(u32t disk, u32t flags) {
   struct qs_diskinfo *qe;
   u8t               fldc;
   register u32t      rcv;
   if ((disk & ~QDSK_DISKMASK)!=0) return 0;
   if (disk >= MAX_QS_DISK-MAX_QS_FLOPPY) return 0;
   qe = qd_array + disk + MAX_QS_FLOPPY;

   mt_swlock();
   fldc = qe->qd_flags;
   // disk present?
   if ((fldc&HDDF_PRESENT)==0) rcv = 0; else 
   // emulated disk
   if ((fldc&HDDF_HOTSWAP)!=0) rcv = HDM_QUERY|HDM_EMULATED; else
   // no i13x for this disk
   if ((flags&HDM_USELBA)!=0 && (fldc&HDDF_LBAPRESENT)==0) rcv = 0; else {
      // change flags if requested
      if ((flags&HDM_QUERY)==0)
         if (fldc&HDM_USELBA^flags&HDM_USELBA)
            if (flags&HDM_USELBA) qe->qd_flags|=HDM_USELBA;
               else qe->qd_flags&=~HDM_USELBA;
      rcv = HDM_QUERY|qe->qd_flags&HDM_USELBA;
   }   
   mt_swunlock();
   // return current value
   return rcv;
}

static int check_diskinfo(struct qs_diskinfo *qdi, int bioschs) {
   /* check sector shift, non-zero sectors & non-zero chs values (someone
      can fatally divide by it, especially in real-mode code) */
   if (1<<qdi->qd_sectorshift!=qdi->qd_sectorsize || !qdi->qd_sectors ||
      !qdi->qd_heads || !qdi->qd_spt) return 0;
   if (bioschs && (!qdi->qd_chsheads || !qdi->qd_chsspt)) return 0;
   return 1;
}

s32t _std hlp_diskadd(struct qs_diskinfo *qdi) {
   u32t  ii;
   s32t  rc = -1;
   if (!qdi) return -1;

   mt_swlock();
   for (ii=MAX_QS_FLOPPY; ii<MAX_QS_DISK; ii++) {
      u8t qfl = qd_array[ii].qd_flags;
      if ((qfl&(HDDF_PRESENT|HDDF_HOTSWAP))==(HDDF_PRESENT|HDDF_HOTSWAP)) {
         if (qd_array[ii].qd_sectorsize==0) break;
      } else
      if ((qfl&HDDF_PRESENT)==0) break;
   }
   if (ii<MAX_QS_DISK && check_diskinfo(qdi, 0)) {
      struct qs_diskinfo *qe = qd_array + ii;

      memcpy(qe, qdi, sizeof(struct qs_diskinfo));
      qe->qd_biosdisk  = 0xFF;
      qe->qd_flags     = HDDF_LBAPRESENT|HDDF_PRESENT|HDDF_HOTSWAP|HDDF_LBAON;
      qe->qd_mediatype = 3;

      rc = ii - MAX_QS_FLOPPY;
   }
   // "disk added" notification
   if (rc>=0 && mod_secondary) mod_secondary->sys_notifyexec(SECB_DISKADD, rc);
   mt_swunlock();
   return rc;
}

/* since we returning disk data here, function is unsafe in any way, so
   let caller care about this */
struct qs_diskinfo *_std hlp_diskstruct(u32t disk, struct qs_diskinfo *qdi) {
   struct qs_diskinfo *qe = 0;
   u8t  idx = disk&QDSK_DISKMASK;
   if (disk & QDSK_FLOPPY) {  // floppy disk
      if (idx<MAX_QS_FLOPPY) qe = qd_array + idx;
   } else
   if (disk<MAX_QS_DISK-MAX_QS_FLOPPY) qe = qd_array + disk + MAX_QS_FLOPPY;
   if (!qe) return 0;
   if ((qe->qd_flags&HDDF_PRESENT)==0 || qe->qd_sectorsize==0) return 0; else 
   if (qdi) {
      u32t fv = qe->qd_flags;
      if (check_diskinfo(qdi, (fv&HDDF_HOTSWAP)==0 && (fv&(HDDF_LBAPRESENT|
         HDDF_LBAON))!=(HDDF_LBAPRESENT|HDDF_LBAON))==0)
      {
         memcpy(qe, qdi, sizeof(struct qs_diskinfo));
         qe->qd_flags = fv;
      } else
         return 0;
   }
   return qe;
}

int  _std hlp_diskremove(u32t disk) {
   struct qs_diskinfo *qe;
   int  rc = 0;
   if (disk >= MAX_QS_DISK-MAX_QS_FLOPPY) return 0;
   qe = qd_array + disk + MAX_QS_FLOPPY;

   mt_swlock();
   if ((qe->qd_flags&(HDDF_PRESENT|HDDF_HOTSWAP))==(HDDF_PRESENT|HDDF_HOTSWAP)) {
      // "disk removing" notification
      if (rc>=0 && mod_secondary) mod_secondary->sys_notifyexec(SECB_DISKREM, disk);

      memset(qe, 0, sizeof(struct qs_diskinfo));
      // drop used flags if next disk is not used
      if (disk+1>=MAX_QS_DISK || (qe[1].qd_flags&HDDF_PRESENT)==0) {
         memset(qe, 0, sizeof(struct qs_diskinfo));
      } else {   
         qe->qd_sectorsize = 0;
         qe->qd_extwrite   = 0;
         qe->qd_extread    = 0;
         qe->qd_sectors    = 0;
      }
      rc = 1;
   }
   mt_swunlock();
   return rc;
}

#ifndef EFI_BUILD
// setup boot partition i/o
void bootdisk_setup() {
   // BPB available? Mount it to drive 0:
   if (bootio_avail) {
      int ii;
      vol_data *vdta   = extvol;

      if (dd_bootflags&BF_NEWBPB) {
         struct Disk_NewPB *npb = (struct Disk_NewPB*)&BootBPB;
         // > 2Tb exFAT should be denied by boot sector installer
         vdta->length     = (u32t)npb->NPB_VolSize;
         vdta->start      = npb->NPB_VolStart;
         vdta->sectorsize = npb->NPB_BytesPerSec;
      } else {
         vdta->length     = BootBPB.BPB_TotalSec;
         if (!vdta->length) vdta->length = BootBPB.BPB_TotalSecBig;
         vdta->start      = BootBPB.BPB_HiddenSec;
         vdta->sectorsize = BootBPB.BPB_BytesPerSec;
      }
      // cannot use BootBPB here - it can be FAT32 and it can contain 0x80
      // from boot sector instead of actual value
      vdta->disk       = dd_bootdisk^QDSK_FLOPPY;
      vdta->flags      = VDTA_ON;
      // locate boot disk in qd_array for easy access
      for (ii=0; ii<MAX_QS_DISK; ii++) {
         struct qs_diskinfo *qe = qd_array + ii;
         if (qe->qd_flags)
            if (qe->qd_biosdisk==dd_bootdisk) { qd_bootidx=ii; break; }
      }
      // log_printf("volume 0: %02X %08X %08X \n",vdta->disk,vdta->start,vdta->length);
   }
}
#endif // EFI_BUILD

static u16t _std (*ext_convert)(u16t src, int to_unicode) = 0;
static u16t _std (*ext_wtoupper)(u16t src) = 0;

/* OEM-Unicode bidirectional conversion */
WCHAR ff_convert (WCHAR src, UINT to_unicode) {
   if (ext_convert) {
      int state = sys_intstate(0);
      src = ext_convert(src, to_unicode);
      sys_intstate(state);
      return src;
   }
   return src>=0x80?0:src;
}

/* Unicode upper-case conversion */
WCHAR ff_wtoupper (WCHAR src) {
   if (ext_wtoupper) {
      int state = sys_intstate(0);
      src = ext_wtoupper(src);
      sys_intstate(state);
      return src;
   }
   return toupper(src);
}

/** setup FatFs i/o to specified codepage.
    setup itself and function usage above is covered by cli to guarantee
    thread-safeness duaring convertion/setup calls */
void _std hlp_setcpinfo(codepage_info *info) {
   int state = sys_intstate(0);
   if (!info) {
      ext_convert  = 0;
      ext_wtoupper = 0;
      cp_num       = 0;
   } else {
      memcpy(ExCvt, info->oemupr, 128);
      ext_convert  = info->convert;
      ext_wtoupper = info->wtoupper;
      cp_num       = info->cpnum;
   }
   sys_intstate(state);
}
