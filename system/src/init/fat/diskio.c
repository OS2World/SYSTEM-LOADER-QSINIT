#include "qslocal.h"
#include "qssys.h"
#include "stdlib.h"
#include "qsconst.h"
#include "dskinfo.h"
#ifndef EFI_BUILD
#include "ioint13.h"
#include "filetab.h"
extern struct   Disk_BPB BootBPB;
#endif

#define QDSK_VALID    (QDSK_FLOPPY|QDSK_VOLUME|QDSK_DISKMASK|QDSK_DIRECT|\
                       QDSK_IAMCACHE|QDSK_IGNACCESS)

extern void           *Disk1Data;
extern u32t            Disk1Size;
extern
struct qs_diskinfo      qd_array[MAX_QS_DISK];
extern u8t      qd_fdds, qd_hdds;
extern int            qd_bootidx; // index of boot disk info in qd_array, -1 if no
extern u16t               cp_num;
extern u8t*                ExCvt; // FatFs OEM case conversion
cache_extptr        *cache_eproc;
// real mode thunk (this thunk pop parameters from stack)
// disk info parameter must be placed in DGROUP (16-bit offset used in RM)
u8t   _std raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write);
int   _std sys_intstate(int on);
qserr      format_vol1();

// cache control
void _std cache_ctrl(u32t action, u8t vol);

u32t _std hlp_diskcount(u32t *floppies) {
   u32t rc = 0, ii;
   mt_swlock();
   /* number of floppies is not touched during QS work, but hdd number can
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
      vol_data *vdta = extvol + idx;
      // wrong volume number
      if (idx>=DISK_COUNT) return 0;
      mt_swlock();
      if (vdta->flags&VDTA_ON) {
         if (sectsize) *sectsize = vdta->sectorsize;
         if (geo) geo->SectorSize = vdta->sectorsize;
         result = vdta->length;
         if (result && geo) geo->TotalSectors = result;
      }
      mt_swunlock();
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
   u32t   rc = 0;
   // lock it! this saves nestlevel usage too
   mt_swlock();
   // try to ask cache for action
   if (cache_eproc && (disk&QDSK_IAMCACHE)==0)
      if ((disk&QDSK_DIRECT)==0 || write)
         rc = (*(write?cache_eproc->cache_write:cache_eproc->cache_read))
            (disk,sector,count,data);
   // process i/o
   if (rc<count) {
      u8t     idx = disk&QDSK_DISKMASK;
      struct qs_diskinfo *qe = 0;
      
      if (disk & QDSK_FLOPPY) {  // floppy disk
         if (idx<MAX_QS_FLOPPY) qe = qd_array + idx;
      } else {                   // hdd
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
            u64t dsksize = qe->qd_sectors;
            int     step;
            count  -= actsize;
#ifndef EFI_BUILD
            /* special handling for imbecile HP BIOS writers : we have no
               int 13h Fn48h, but know at least boot volume location */
            if (!dsksize)
               if (qe->qd_biosdisk==dd_bootdisk)
                  dsksize = extvol[DISK_BOOT].start + extvol[DISK_BOOT].length;
               else
                  dsksize = 1;    // allow to read partition table
#endif         
            for (step=0; step<2; step++)
               if (step^write) {
                  // move from/to buffer
                  u32t szm = actsize << qe->qd_sectorshift;
                  if (data!=(void*)DiskBufPM)
                     memcpy(write?(void*)DiskBufPM:data, write?data:(void*)DiskBufPM, szm);
                  data = (u8t*)data + szm;
               } else {
                  int err = 0;

                  if (sector + actsize > dsksize) err = 1; else
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
   // check disk handle
   if (disk & ~QDSK_VALID) return 0;
   // check access
   if ((disk&(QDSK_IAMCACHE|QDSK_IGNACCESS))==0 && mod_secondary)
      if (mod_secondary->dsk_canread)
         if ((count = mod_secondary->dsk_canread(disk,sector,count))==0)
            return 0;
   // is it volume or direct disk handle?
   if (disk & QDSK_VOLUME) {
      u8t drv = disk&QDSK_DISKMASK;
      switch (drv) {
         case 1:
            memcpy(data, (char*)Disk1Data+(sector<<LDR_SSHIFT), count<<LDR_SSHIFT);
            return count;
         case 0: 
            if (!bootio_avail) return 0;
         default:
            if (drv<DISK_COUNT) {
               vol_data *vdta = extvol + drv;
               u32t       res = 0;
               mt_swlock();
               if (vdta->flags) res = hlp_diskread(vdta->disk|QDSK_IGNACCESS,
                  vdta->start+sector, count, data);
               mt_swunlock();
               return res;
            }
            break;
      }
      return 0;
   } else
      return action(0,disk,sector,count,data);
}

u32t _std hlp_diskwrite(u32t disk, u64t sector, u32t count, void *data) {
   // check disk handle
   if (disk & ~QDSK_VALID) return 0;
   // check access
   if ((disk&(QDSK_IAMCACHE|QDSK_IGNACCESS))==0 && mod_secondary)
      if (mod_secondary->dsk_canwrite)
         if ((count = mod_secondary->dsk_canwrite(disk,sector,count))==0)
            return 0;
   // is it volume or direct disk handle?
   if (disk & QDSK_VOLUME) {
      u8t drv = disk&QDSK_DISKMASK;
      switch (drv) {
         case 1:
            memcpy((char*)Disk1Data+(sector<<LDR_SSHIFT), data, count<<LDR_SSHIFT);
            return count;
         case 0: 
            if (!bootio_avail) return 0;
         default:
            if (drv<DISK_COUNT) {
               vol_data *vdta = extvol + drv;
               u32t       res = 0;
               mt_swlock();
               if (vdta->flags) res = hlp_diskwrite(vdta->disk|QDSK_IGNACCESS,
                  vdta->start+sector, count, data);
               mt_swunlock();
               return res;
            }
            break;
      }
      return 0;
   } else
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

s32t _std hlp_diskadd(struct qs_diskinfo *qdi, u32t flags) {
   u8t  bmused[16];
   u32t     ii;
   s32t     rc = -1, 
           rdn = -1;
   if (!qdi) return -1;

   /* makes a bitmap for 128 BIOS HDD numbers & try to find an empty pos in it */
   memset(&bmused, 0xFF, sizeof(bmused));

   mt_swlock();
   for (ii=MAX_QS_FLOPPY; ii<MAX_QS_DISK; ii++) {
      u8t   qfl = qd_array[ii].qd_flags;
      int empty = 0;
      if ((qfl&(HDDF_PRESENT|HDDF_HOTSWAP))==(HDDF_PRESENT|HDDF_HOTSWAP)) {
         if (qd_array[ii].qd_sectorsize==0) empty = 1;
      } else
      if ((qfl&HDDF_PRESENT)==0) empty = 1;

      if (!empty) {
         u8t biosnum = qd_array[ii].qd_biosdisk;
         if (biosnum>=0x80 && biosnum<0xFF && mod_secondary)
            mod_secondary->setbits(&bmused, biosnum-0x80, 1, 0);
      } else
      // get first of it
      if (rdn<0) rdn = ii;
   }
   
   if (rdn>=0 && rdn<MAX_QS_DISK && check_diskinfo(qdi, 0)) {
      struct qs_diskinfo *qe = qd_array + rdn;

      memcpy(qe, qdi, sizeof(struct qs_diskinfo));

      if (flags&HDDA_BIOSNUM) {
         u32t bn = mod_secondary->bitfind(&bmused,128,1,1,0);
         qe->qd_biosdisk = bn==FFFF ? 0xFF : bn+0x80;
      } else
         qe->qd_biosdisk = 0xFF;

      qe->qd_flags     = HDDF_LBAPRESENT|HDDF_PRESENT|HDDF_HOTSWAP|HDDF_LBAON;
      qe->qd_mediatype = 3;

      rc = rdn - MAX_QS_FLOPPY;
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
         qe->qd_biosdisk   = 0xFF;
      }
      rc = 1;
   }
   mt_swunlock();
   return rc;
}

#ifndef EFI_BUILD
// setup boot partition i/o
void init_vol0() {
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
         u32t secsize = BootBPB.BPB_BytesPerSec;
         // check sector size in BPB supplied by Moveton`s PXE loader
         if (pxemicro)
            if (bsr32(secsize)!=bsf32(secsize) || !secsize) {
               log_printf("Invalid BPB!\n");
               return;
            }
         vdta->length     = BootBPB.BPB_TotalSec;
         if (!vdta->length) vdta->length = BootBPB.BPB_TotalSecBig;
         vdta->start      = BootBPB.BPB_HiddenSec;
         vdta->sectorsize = secsize;
      }
      vdta->disk = hlp_diskbios(dd_bootdisk, 0);

      if (vdta->disk!=FFFF) {
         vdta->flags = VDTA_ON;
         // position of boot disk in qd_array - for easy access
         qd_bootidx  = vdta->disk&QDSK_FLOPPY?vdta->disk&~QDSK_FLOPPY :
                                              vdta->disk+MAX_QS_FLOPPY;
      }
      // log_printf("volume 0: %02X %08X %08X \n",vdta->disk,vdta->start,vdta->length);
   }
}
#endif // EFI_BUILD

u32t _std hlp_diskbios(u32t disk, int qs2bios) {
   u32t rc = FFFF;
#ifndef EFI_BUILD
   mt_swlock();
   if (qs2bios) {
      struct qs_diskinfo *qe = hlp_diskstruct(disk, 0);
      if (qe && qe->qd_biosdisk!=0xFF) rc = qe->qd_biosdisk;
   } else
   // 0xFF is used as invalid in QS code
   if (disk<0xFF) {
      u32t ii;
      for (ii=0; ii<MAX_QS_DISK; ii++) {
         struct qs_diskinfo *qe = qd_array + ii;
         if (qe->qd_flags)
            if (qe->qd_biosdisk==disk) {
               rc = ii<MAX_QS_FLOPPY ? ii|QDSK_FLOPPY : ii-MAX_QS_FLOPPY;
               break;
            }
      }
   }
   mt_swunlock();
#endif
   return rc;
}


void init_vol1(void) {
   vol_data *vdta = extvol+DISK_LDR;
   if (!vdta->length) {
      qserr    res;
      vdta->length = Disk1Size>>LDR_SSHIFT;
      vdta->start  = 0;
      vdta->disk   = QDSK_VOLUME + DISK_LDR;
      vdta->sectorsize = LDR_SSIZE;
      vdta->flags  = VDTA_ON;

      res = format_vol1();
      if (res) {
         log_printf("fmt err %X\n", res);
         exit_pm32(QERR_FATERROR);
      }
   }
}

void check_disks(void) {
   u32t ii;
   // locate boot disk in qd_array for easy access
   for (ii=0; ii<MAX_QS_DISK; ii++) {
      struct qs_diskinfo *qe = qd_array + ii;
      if (qe->qd_flags)
         if (!qe->qd_sectors) log_printf("disk %02X - no size!\n", qe->qd_biosdisk);
   }
}

static u16t _std (*ext_convert)(u16t src, int to_unicode) = 0;
static u16t _std (*ext_wtoupper)(u16t src) = 0;

/* OEM-Unicode bidirectional conversion */
u16t _std ff_convert(u16t src, unsigned int to_unicode) {
   if (ext_convert) {
      int state = sys_intstate(0);
      src = ext_convert(src, to_unicode);
      sys_intstate(state);
      return src;
   }
   return src>=0x80?0:src;
}

/* Unicode upper-case conversion */
u16t _std ff_wtoupper(u16t src) {
   if (ext_wtoupper) {
      int state = sys_intstate(0);
      src = ext_wtoupper(src);
      sys_intstate(state);
      return src;
   }
   return toupper(src);
}

/** codepage changing callback.
    All notifications are called in MT lock, so it should be safe. */
void _std cb_codepage(sys_eventinfo *cbinfo) {
   codepage_info *info = (codepage_info*)cbinfo->data;
   if (!info) {
      ext_convert  = 0;
      cp_num       = 0; 
   } else {
      memcpy(ExCvt, info->oemupr, 128);
      ext_convert  = info->convert;
      cp_num       = info->cpnum;
   }
}
