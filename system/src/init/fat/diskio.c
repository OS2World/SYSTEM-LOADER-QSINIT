#include "diskio.h"
#include "ioint13.h"
#include "qsint.h"
#include "qsinit.h"
#include "qsutil.h"
#include "clib.h"
#include "ffconf.h"
#include "qsconst.h"
#include "filetab.h"
#include "dskinfo.h"

extern struct   Disk_BPB BootBPB;
extern void           *Disk1Data;
extern u32t            Disk1Size;
extern u32t            DiskBufPM; // flat disk buffer address
extern
struct qs_diskinfo      qd_array[MAX_QS_DISK];
extern u8t      qd_fdds, qd_hdds;
extern int            qd_bootidx; // index of boot disk info in qd_array, -1 if no
extern u8t           dd_bootdisk,
                    dd_bootflags;
extern vol_data*          extvol;
cache_extptr        *cache_eproc;
// real mode thunk (this thunk pop parameters from stack)
// disk info parameter must be placed in DGROUP (16-bit offset used in RM)
extern u8t _std raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write);

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
         if (dd_bootflags&BF_NOMFSHVOLIO) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            if (!extvol||drv&&!vdta->flags) return RES_NOTRDY;
            if (hlp_diskread(vdta->disk, vdta->start+sector, count, buff) != count)
               return RES_ERROR;
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
         if (dd_bootflags&BF_NOMFSHVOLIO) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            if (!extvol||drv&&!vdta->flags) return RES_NOTRDY;
            if (ctrl==GET_SECTOR_SIZE ) *(WORD*) buff = vdta->sectorsize; else
            if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff = vdta->length;
            return RES_OK;
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
         if (dd_bootflags&BF_NOMFSHVOLIO) return RES_NOTRDY;
      default:
         if (drv<_VOLUMES) {
            vol_data *vdta = extvol + drv;
            if (!extvol||drv&&!vdta->flags) return RES_NOTRDY;
            if (hlp_diskwrite(vdta->disk, vdta->start+sector, count, (void*)buff) != count)
               return RES_ERROR;
         } else
            return RES_ERROR;
         break;
   }
   return RES_OK;
}

u32t _std hlp_diskcount(u32t *floppies) {
   u32t rc = 0, ii;
   /* number of floppies is not touched duaring QS work, but hdd number can
      be altered by hlp_diskadd() */
   for (ii=MAX_QS_FLOPPY; ii<MAX_QS_DISK; ii++)
      if (qd_array[ii].qd_flags&HDDF_PRESENT) rc++;
   if (floppies) *floppies = qd_fdds;
   return rc;
}

u32t _std hlp_disksize(u32t disk, u32t *sectsize, disk_geo_data *geo) {
   u32t            result = 0;
   u8t                idx = disk&QDSK_DISKMASK;
   struct qs_diskinfo *qe = 0;
   if (geo) memset(geo, 0, sizeof(disk_geo_data));

   if (disk & QDSK_VIRTUAL) { // virtual disk
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
               /* limit disk size to 24Tb now - larger values truncating to
                  low dword (because of older BIOS bugs - garbage in high dword) */
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
   }
   return result;
}

static u32t action(int write, u32t disk, u64t sector, u32t count, void *data) {
   static u8t nestlevel = 0;
   u32t   rc = 0;
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
                  if (sector + actsize > qe->qd_sectors) return 0;
                  // read/write by 32k via buffer in 1st Mb
                  if (raw_io(qe, sector, actsize, write)) return 0;
                  sector += actsize;
               }
         } while (count);
      }
   }
   return rc;
}

u32t _std hlp_diskread(u32t disk, u64t sector, u32t count, void *data) {
   if (disk & QDSK_VIRTUAL)
      return disk_read(disk&QDSK_DISKMASK,data,sector,count)==RES_OK?count:0;
   else
      return action(0,disk,sector,count,data);
}

u32t _std hlp_diskwrite(u32t disk, u64t sector, u32t count, void *data) {
   if (disk & QDSK_VIRTUAL)
      return disk_write(disk&QDSK_DISKMASK,data,sector,count)==RES_OK?count:0;
   else
      return action(1,disk,sector,count,data);
}

u32t _std hlp_diskmode(u32t disk, u32t flags) {
   struct qs_diskinfo *qe;
   u8t               fldc;
   if ((disk & ~QDSK_DISKMASK)!=0) return 0;
   if (disk >= MAX_QS_DISK-MAX_QS_FLOPPY) return 0;
   // disk present?
   qe   = qd_array + disk + MAX_QS_FLOPPY;
   fldc = qe->qd_flags;
   if ((fldc&HDDF_PRESENT)==0) return 0;
   // emulated disk
   if ((fldc&HDDF_HOTSWAP)!=0) return HDM_QUERY|HDM_EMULATED;
   // no i13x for this disk
   if ((flags&HDM_USELBA)!=0 && (fldc&HDDF_LBAPRESENT)==0) return 0;
   // change flags if requested
   if ((flags&HDM_QUERY)==0)
      if (fldc&HDM_USELBA^flags&HDM_USELBA)
         if (flags&HDM_USELBA) qe->qd_flags|=HDM_USELBA;
            else qe->qd_flags&=~HDM_USELBA;
   // return current value
   return HDM_QUERY|qe->qd_flags&HDM_USELBA;
}

s32t _std hlp_diskadd(struct qs_diskinfo *qdi) {
   u32t ii;
   if (!qdi) return -1;

   for (ii=MAX_QS_FLOPPY; ii<MAX_QS_DISK; ii++) {
      u8t qfl = qd_array[ii].qd_flags;
      if ((qfl&(HDDF_PRESENT|HDDF_HOTSWAP))==(HDDF_PRESENT|HDDF_HOTSWAP)) {
         if (qd_array[ii].qd_sectorsize==0) break;
      } else
      if ((qfl&HDDF_PRESENT)==0) break;
   }
   if (ii<MAX_QS_DISK) {
      struct qs_diskinfo *qe = qd_array + ii;
      memcpy(qe, qdi, sizeof(struct qs_diskinfo));
      qe->qd_biosdisk = 0xFF;
      qe->qd_flags    = HDDF_LBAPRESENT|HDDF_PRESENT|HDDF_HOTSWAP|HDDF_LBAON;

      return ii - MAX_QS_FLOPPY;
   }
   return -1;
}

int  _std hlp_diskremove(u32t disk) {
   struct qs_diskinfo *qe;
   if (disk >= MAX_QS_DISK-MAX_QS_FLOPPY) return 0;
   qe = qd_array + disk + MAX_QS_FLOPPY;

   if ((qe->qd_flags&(HDDF_PRESENT|HDDF_HOTSWAP))==(HDDF_PRESENT|HDDF_HOTSWAP)) {
      // drop used flags if next disk is not used
      if (disk+1>=MAX_QS_DISK || (qe[1].qd_flags&HDDF_PRESENT)==0) {
         memset(qe, 0, sizeof(struct qs_diskinfo));
      } else {   
         qe->qd_sectorsize = 0;
         qe->qd_extwrite   = 0;
         qe->qd_extread    = 0;
      }
      return 1;
   }
   return 0;
}

/* OEM-Unicode bidirectional conversion */
WCHAR ff_convert (WCHAR src, UINT to_unicode) {
   return src>=0x80?0:src;
}

/* Unicode upper-case conversion */
WCHAR ff_wtoupper (WCHAR src) {
   return toupper(src);
}

// setup boot partition i/o
void bootdisk_setup() {
   // BPB available? Mount it to drive 0:
   if ((dd_bootflags&BF_NOMFSHVOLIO)==0) {
      int ii;
      vol_data *vdta   = extvol;
      vdta->length     = BootBPB.BPB_TotalSec;
      if (!vdta->length) vdta->length = BootBPB.BPB_TotalSecBig;
      vdta->start      = BootBPB.BPB_HiddenSec;
      // cannot use BootBPB here - it can be FAT32 and it can contain 0x80
      // from boot sector instead of actual value
      vdta->disk       = dd_bootdisk^QDSK_FLOPPY;
      vdta->flags      = 
      vdta->sectorsize = BootBPB.BPB_BytesPerSec;
      // locate boot disk in qd_array for easy access
      for (ii=0; ii<MAX_QS_DISK; ii++) {
         struct qs_diskinfo *qe = qd_array + ii;
         if (qe->qd_flags)
            if (qe->qd_biosdisk==dd_bootdisk) { qd_bootidx=ii; break; }
      }
      // log_printf("volume 0: %02X %08X %08X \n",vdta->disk,vdta->start,vdta->length);
   }
}
