//
// QSINIT "start" module
// volume functions
// -------------------------------------
//
// moved here from QSINIT binary

#include "qsint.h"
#include "fsfat.h"
#include "qsutil.h"
#include "diskio.h"

// Initialize a Drive
DSTATUS disk_initialize(BYTE drv) {
   if (drv>=DISK_COUNT) return STA_NOINIT;
#if 0 //def INITDEBUG
   log_misc(2,"disk_init(%d)\n",(DWORD)drv);
#endif
   return 0;
}

// Return Disk Status
DSTATUS disk_status(BYTE drv) {
   return drv>=DISK_COUNT?STA_NOINIT:0;
}

DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff) {
   //log_misc(2,"disk_ioctl(%d,%d,%X)\n",(DWORD)drv,(DWORD)ctrl,buff);
   if (ctrl==CTRL_SYNC) {
      hlp_cachenotify(drv, CC_FLUSH);
      return RES_OK;
   }
   if (ctrl==GET_BLOCK_SIZE) { *(DWORD*)buff=1; return RES_OK; }

   if (!_extvol) return RES_NOTRDY;

   if (ctrl==GET_SECTOR_SIZE || ctrl==GET_SECTOR_COUNT) {
      vol_data *vdta = _extvol + drv;
      DRESULT    res = RES_OK;
      mt_swlock();
      if (vdta->flags&VDTA_ON) {
         if (ctrl==GET_SECTOR_SIZE ) *(WORD*) buff = vdta->sectorsize; else
         if (ctrl==GET_SECTOR_COUNT) *(DWORD*)buff = vdta->length;
      } else
         res = RES_NOTRDY; 
      mt_swunlock();
      return res;
   }
   return RES_PARERR;
}

DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, UINT count) {
   if (!count  ) return RES_OK;
   if (!buff   ) return RES_PARERR;
   if (!_extvol) return RES_NOTRDY;
   //log_it(2,"disk_read(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   if (drv==DISK_LDR) {
      return hlp_diskread(drv|QDSK_VOLUME, sector, count, (void*)buff)==count?
         RES_OK:RES_ERROR;
   } else {
      if (drv<DISK_COUNT) {
         vol_data *vdta = _extvol + drv;
         DRESULT    res = RES_OK;
         mt_swlock();
         if (drv&&!vdta->flags) res = RES_NOTRDY; else
         if (hlp_diskread(vdta->disk|QDSK_IGNACCESS, vdta->start+sector,
            count, (void*)buff) != count) res = RES_ERROR;
         mt_swunlock();
         return res;
      }
   }
   return RES_ERROR;
}

DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, UINT count) {
   if (!count  ) return RES_OK;
   if (!buff   ) return RES_PARERR;
   if (!_extvol) return RES_NOTRDY;
   //log_it(2,"disk_write(%d,%x,%d,%d)\n",(DWORD)drv,buff,sector,(DWORD)count);
   if (drv==DISK_LDR) {
      return hlp_diskwrite(drv|QDSK_VOLUME, sector, count, (void*)buff)==count?
         RES_OK:RES_ERROR;
   } else {
      if (drv<DISK_COUNT) {
         vol_data *vdta = _extvol + drv;
         DRESULT    res = RES_OK;
         mt_swlock();
         if (drv&&!vdta->flags) res = RES_NOTRDY; else
         if (hlp_diskwrite(vdta->disk|QDSK_IGNACCESS, vdta->start+sector,
            count, (void*)buff) != count) res = RES_ERROR;
         mt_swunlock();
         return res;
      }
   }
   return RES_ERROR;
}

u32t get_fattime(void) { return tm_getdate(); }
