//
// QSINIT "start" module
// volume functions
// -------------------------------------
//
// moved here from QSINIT binary

#include "qsutil.h"
#include "qsint.h"
#include "qsstor.h"
#include "fat/ff.h"
#include "stdlib.h"
#include "qsinit_ord.h"
#include "seldesc.h"
#include "qsmodext.h"

static cache_extptr *cache_eproc = 0;

static int _std catch_cacheptr(mod_chaininfo *info) {
   cache_extptr *fptr = *(cache_extptr **)(info->mc_regs->pa_esp+4);
   if (fptr && fptr->entries!=3) {
      log_it(2, "Invalid runcache() parameter (%08X)\n", fptr);
      return 1;
   }
   log_it(2, "hlp_runcache(%08X)\n", fptr);
   cache_eproc = fptr;
   return 1;
}

// catch hlp_runcache() call to get pointer to new cache_extptr
void setup_cache(void) {
   u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   if (qsinit)
      if (mod_apichain(qsinit, ORD_QSINIT_hlp_runcache, APICN_ONENTRY, 
         catch_cacheptr)) return;
   log_it(2, "failed to catch runcache!\n");
}

static void _std cache_ioctl(u8t vol, u32t action) {
   if (cache_eproc) (*cache_eproc->cache_ioctl)(vol,action);
}

u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   FATFS   **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);
   // no curcular refences allowed!!! ;)
   if (disk==QDSK_VIRTUAL+(u32t)drive) return 0;
   // no 2Tb partitions!
   if (count>=_4GBLL) return 0;
   // mount it
   if (extvol && drive>DISK_LDR && drive<_VOLUMES) {
      u32t     sectsz;
      u64t        tsz = hlp_disksize64(disk, &sectsz);
      vol_data  *vdta = extvol+drive;
      char      dp[8];
      FRESULT    mres;
      // check for init, size and sector count overflow
      if (!tsz||sector>=tsz||sector+count>tsz||sector+count<sector) return 0;
      // unmount current
      hlp_unmountvol(drive);
      // disk parameters for fatfs i/o
      vdta->disk         = disk;
      vdta->start        = sector;
      vdta->length       = count;
      vdta->sectorsize   = sectsz;
      vdta->serial       = 0;
      vdta->badclus      = 0;
      sprintf(dp,"%d:\\",drive);
      // mark as mounted else disk i/o deny disk access
      vdta->flags = 1;
      // trying to mount
      mres = f_mount(extdrv[drive],dp,1);
      // allow ok and no-filesystem error codes
      if (mres==FR_OK || mres==FR_NO_FILESYSTEM) {
         /* prior to FatFs R0.10 this call was needed to force FatFs call 
            to chk_mounted, now this can be done by f_mount parameter.
            In addition, function update current drive in QSINIT module. */
         if (mres==FR_OK) hlp_chdir(dp);
         cache_ioctl(drive,CC_MOUNT);
         return 1;
      }
      // clear state
      vdta->flags = 0;
   }
   return 0;
}

u32t _std hlp_unmountvol(u8t drive) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   char       dp[8];
   if (!extvol || drive<=DISK_LDR || drive>=_VOLUMES) return 0;
   if ((extvol[drive].flags&1)==0) return 0;
   cache_ioctl(drive,CC_UMOUNT);
   extvol[drive].flags = 0;
   sprintf(dp,"%d:",drive);
   f_mount(0,dp,0);
   return 1;
}

u32t _std hlp_volinfo(u8t drive, disk_volume_data *info) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   FATFS   **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);

   if (extvol && drive<_VOLUMES) {
      vol_data  *vdta = extvol+drive;
      FATFS     *fdta = extdrv[drive];
      // mounted?
      if (drive<=DISK_LDR || (vdta->flags&1)!=0) {
         if (info) {
            info->StartSector  = vdta->start;
            info->TotalSectors = vdta->length;
            info->Disk         = vdta->disk;
            info->ClSize       = fdta->csize;
            info->RootDirSize  = fdta->n_rootdir;
            info->SectorSize   = vdta->sectorsize;
            info->FatCopies    = fdta->n_fats;
            info->DataSector   = fdta->database;
            info->BadClusters  = vdta->badclus;

            if (!vdta->serial) {
                char cp[3];
                cp[0]='0'+drive; cp[1]=':'; cp[2]=0;
                f_getlabel(cp,vdta->label,&vdta->serial);
            }
            info->SerialNum    = vdta->serial;
            strcpy(info->Label,vdta->label);
         }
         return fdta->fs_type;
      }
   }
   if (info) memset(info,0,sizeof(disk_volume_data));
   return FST_NOTMOUNTED;
}

u32t _std hlp_vollabel(u8t drive, const char *label) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   FATFS   **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);

   if (extvol && drive<_VOLUMES) {
      vol_data  *vdta = extvol+drive;
      FATFS     *fdta = extdrv[drive];
      // mounted?
      if (drive<=DISK_LDR || (vdta->flags&1)!=0) {
         char lb[14];
         lb[0] = drive+'0';
         lb[1] = ':';
         if (label) {
            strncpy(lb+2,label,12);
            lb[13] = 0;
         } else
            lb[2] = 0;
         if (f_setlabel(lb)==FR_OK) {
            // drop file label cache to make read it again
            vdta->serial = 0;
            return 1;
         }
      }
   }
   return 0;
}

u32t _std dsk_strtodisk(const char *str) {
   if (str && *str) {
      u32t disk, floppies, 
          disks = hlp_diskcount(&floppies);
      char   c1 = toupper(str[0]),
             c2 = toupper(str[1]);
      
      do {
         if ((c1=='H'||c1=='F') && isalnum(c2)) { // hd0 h0 hdd0 fd0 f0 fdd0
            int idx = isalpha(c2)?1:0;
            if (idx && c2!='D') break; else
            if (toupper(str[2])=='D') idx++;
            // check for 0..9
            if (!isdigit(str[1+idx])) break;
            disk = str2long(str+1+idx);
            if (disk>=(c1=='F'?floppies:disks)) break;
            if (c1=='F') disk|=QDSK_FLOPPY;
         } else
         if (c2==':' && isalnum(c1)) {            // 0: 1: A: B:
            if (isalpha(c1)) c1 = c1-'A'+'0';
            disk = c1-'0';
            if (disk>=DISK_COUNT) break;
            disk|=QDSK_VIRTUAL;
         } else break;
      
         return disk;
      } while (0);
   }
   return FFFF;
}

u64t _std hlp_disksize64(u32t disk, u32t *sectsize) {
   disk_geo_data geo;
   hlp_disksize(disk, sectsize, &geo);
   return geo.TotalSectors;
}
