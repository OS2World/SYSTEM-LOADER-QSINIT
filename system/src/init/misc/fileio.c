//
// QSINIT
// volume / micro-FSD api file i/o
//
#include "clib.h"
#include "qsint.h"
#include "qsinit.h"
#include "qsutil.h"
#include "ff.h"
#include "qsconst.h"
#include "qsstor.h"
#include "parttab.h"

#ifndef EFI_BUILD
#include "filetab.h"
extern u8t              dd_bootflags;
extern char          mfsd_openname[]; // current file name
extern struct filetable_s  filetable;
#else
char    mfsd_openname[MFSD_NAME_LEN];
#endif
extern u32t                DiskBufPM; // flat disk buffer address
extern u8t               dd_bootdisk;
extern u32t               mfsd_fsize;

static struct _io {
   u32t        micro;           // micro-fsd present?
   u32t      file_ok;           // file opened
   FATFS*     fat_fs;           // FAT disk struct
   FIL*       fat_fl;           // FAT file struct
   u32t     file_len;           // length of the file
} *io=0;

FATFS*     extdrv[_VOLUMES];
FIL*       extfl [_VOLUMES];
vol_data*  extvol = 0;
char*     currdir[_VOLUMES];
u8t      currdisk = 0;
u8t    *mount_buf = 0;

extern void    *Disk1Data; // own data virtual disk
extern u32t     Disk1Size; // and it`s size

// remove intensive usage of 32bit offsets in compiler generated code
#define IOPTR auto struct _io *iop=io
#define fs_ok    (iop->fs_ok)
#define file_ok  (iop->file_ok)
#define fat_fs   (iop->fat_fs)
#define fat_fl   (iop->fat_fl)
#define micro    (iop->micro)
#define file_len (iop->file_len)

// micro-fsd call thunks
extern u16t _std    mfs_open (void);
extern u16t _std    strm_open(void);
/* call mfs_read/strm_read.
   buf must be placed in 1Mb for RM call and paragraph aligned, at least */
extern u32t __cdecl mfs_read (u32t offset, u32t buf, u32t readsize);
extern u16t __cdecl strm_read(u32t buf, u16t readsize);
extern u32t __cdecl mfs_close(void);
extern u32t __cdecl mfs_term (void);

// cache control
void _std cache_ctrl(u32t action, u8t vol);
// setup boot partition i/o
void bootdisk_setup();

int __stdcall toupper(int cc) {
   return ((cc)>0x60&&(cc)<=0x7A?(cc)-0x20:(cc));
}

int __stdcall tolower(int cc) {
   return ((cc)>0x40&&(cc)<=0x5A?(cc)+0x20:(cc));
}

static void nametocname(const char *name) {
   int ii;
   strncpy(mfsd_openname, name, MFSD_NAME_LEN-1);
   mfsd_openname[MFSD_NAME_LEN-1]=0;
   // micro-FSD does not uppercase requested names
   for (ii=0; ii<MFSD_NAME_LEN-1; ii++) {
      char ch=toupper(mfsd_openname[ii]);
      mfsd_openname[ii]=ch;
      if (!ch) { // remove trailing dot
         if (ii--)
            if (mfsd_openname[ii]=='.') mfsd_openname[ii]=0;
         break;
      }
   }
}

// open file. return -1 on no file, else file size
u32t hlp_fopen(const char *name) {
   u32t retsize = 0;
   IOPTR;
   if (!iop||file_ok) return FFFF;
#if defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2,"hlp_fopen(%s)\n",name);
#endif
   if (micro) {
      nametocname(name);
      if (mfs_open()) return FFFF;
      retsize = mfsd_fsize;
   } else {
      FRESULT rc;
      mfsd_openname[0]='0'; mfsd_openname[1]=':'; mfsd_openname[2]='\\';
      strncpy(mfsd_openname+3, name, MFSD_NAME_LEN-4);
      mfsd_openname[MFSD_NAME_LEN-1]=0;
      rc = f_open(fat_fl, mfsd_openname, FA_READ);
      if (rc==FR_NO_FILE||rc==FR_INVALID_NAME||rc==FR_NO_PATH) return FFFF; else
      if (rc) {
#if 0 // defined(INITDEBUG)
         log_printf("opn(%s): %d\n", mfsd_openname, rc);
#endif
         exit_pm32(QERR_DISKERROR);
      }
      retsize = fat_fl->fsize;
   }
#if 0 // defined(INITDEBUG)
   log_printf("%s size: %d\n", mfsd_openname, retsize);
#endif
   file_ok=1;
   return file_len=retsize;
}

// read file
u32t hlp_fread2(u32t offset, void *buf, u32t readsize, read_callback cbprint) {
   u32t res=0, offsb = offset, sizeb = readsize>>10, ppr = 0;
   FRESULT fatrc = 0;
   IOPTR;
   //log_misc(2,"hlp_fread(%d,%08X,%d,%08x)\n",offset,buf,readsize,cbprint);
   if (!iop||!file_ok) return 0;
   if (!readsize||offset>=file_len) return 0;
   if (offset+readsize>file_len) readsize=file_len-offset;
   if (micro||cbprint) {
      u8t* dstpos=(u8t*)buf;
      res=readsize;
      do {
         u32t rds,actual;
         if (micro) {
            rds    = readsize>DISKBUF_SIZE?DISKBUF_SIZE:readsize;
            actual = mfs_read(offset, DiskBufPM, rds);
            // copy from disk i/o buffer in low memory to user buffer
            memcpy(dstpos, (void*)DiskBufPM, rds);
         } else {
            rds   =readsize>_1MB?_1MB:readsize;
            fatrc =f_read(fat_fl, dstpos, rds, (UINT*)&actual);
            if (fatrc) break;
         }
         readsize-=actual;
         // read error, exiting...
         if (actual!=rds) break;
         dstpos+=rds;
         offset+=rds;
         if (cbprint) {
            u32t pr = (offset-offsb>>10)*100/sizeb;
            if (pr!=ppr) (*cbprint)(ppr=pr,offset-offsb);
         }
      } while (readsize);
      res-=readsize;
   } else
      fatrc=f_read(fat_fl, buf, readsize, (UINT*)&res);
#ifdef INITDEBUG
   if (fatrc) log_printf("rd(%s): %d\n", mfsd_openname, fatrc);
#endif
   return res;
}

u32t hlp_fread(u32t offset, void *buf, u32t readsize) {
   return hlp_fread2(offset,buf,readsize,0);
}

// close file
void hlp_fclose() {
   IOPTR;
   if (!iop||!file_ok) return;
   if (micro) mfs_close(); else f_close(fat_fl);
   file_ok=0;
}

// open, read, close file & return buffer with it
void* hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint) {
   void *res=0;
   u32t rcsize=0;
   IOPTR;
   if (!iop||file_ok||!bufsize) return 0;
   *bufsize=0;
#ifndef EFI_BUILD
   if (micro && filetable.ft_cfiles>=5) {
      u32t  allocsize;
      char  mvpxe = filetable.ft_cfiles==6;
      nametocname(name);
#if 0 //def INITDEBUG
      log_misc(2,"pxe open(%s)\n", mfsd_openname);
#endif
      if ((mvpxe?strm_open():mfs_open())!=0) return 0;
      // alloc 256kb first
      res = hlp_memallocsig(allocsize=_256KB, "file", 0);
      while (true) {
         u16t readed = mvpxe?strm_read(DiskBufPM,DISKBUF_SIZE):
                       mfs_read(rcsize,DiskBufPM,DISKBUF_SIZE);
         if (!readed||readed==0xFFFF) break;
         if (rcsize+readed>allocsize)
            res=hlp_memrealloc(res,allocsize+=_256KB);
         memcpy((u8t*)res+rcsize,(void*)DiskBufPM,readed);
         rcsize+=readed;
         if (cbprint) (*cbprint)(0,rcsize);
         /* PXE read is slow, so if we call idle no one will sense
            the difference ;) */
         cache_ctrl(CC_IDLE,DISK_LDR);
      }
      mfs_close();
      // trim used memory
      res=hlp_memrealloc(res,rcsize);
      *bufsize=rcsize;
   } else 
#endif // EFI_BUILD
   {
      rcsize=hlp_fopen(name);
      if (rcsize==FFFF) return 0;
      if (rcsize) {
         res = hlp_memallocsig(rcsize, "file", 0);
         *bufsize=hlp_fread2(0,res,rcsize,cbprint);
      }
      hlp_fclose();
   }
   return res;
}

// init file i/o
void hlp_finit() {
   IOPTR;
   int   ii;
   char *ptr;
#if defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2,"hlp_finit()\n");
#endif
   if (iop) return;
   // allocating constant blocks for FAT i/o, no need to free it
   iop=io = (struct _io *)hlp_memallocsig(sizeof(struct _io), "iop", QSMA_READONLY);
#ifdef EFI_BUILD
   micro  = 1;
#else
   micro  = dd_bootflags&BF_MICROFSD?1:0;
#endif
   // allocate memory for FATs in one call (because of 64k memalloc step)
   fat_fs = (FATFS*)hlp_memallocsig((sizeof(FATFS)+sizeof(FIL)+sizeof(vol_data))*_VOLUMES+
            Round16(QS_MAXPATH+1)*_VOLUMES, "vol", QSMA_READONLY);
   fat_fl = (FIL*)((u8t*)fat_fs+sizeof(FATFS));
   // initing arrays
   ptr=(char*)fat_fs;
   for (ii=0;ii<_VOLUMES;ii++) {
      extdrv[ii]=(FATFS*)ptr; ptr+=sizeof(FATFS);
      extfl[ii] =(FIL*)ptr;   ptr+=sizeof(FIL);
   }
   sto_save(STOKEY_FATDATA, &extdrv, _VOLUMES*sizeof(FATFS*), 0);
   // additional volumes (2-9) mount data
   extvol=(vol_data*)ptr;
   ptr  +=sizeof(vol_data)*_VOLUMES;
   sto_save(STOKEY_VOLDATA, extvol, sizeof(vol_data)*_VOLUMES, 0);
   // current directory for all drives
   for (ii=0;ii<_VOLUMES;ii++) {
      currdir[ii]=ptr;
      ptr[0]='0'+ii; ptr[1]=':'; ptr[2]='\\'; ptr[3]=0;
      ptr+=Round16(QS_MAXPATH+1);
   }
   // setup volume/disk i/o for boot partition (drive 0)
   bootdisk_setup();
   // mount boot FAT (drive 0)
   if (!micro) {
      FRESULT rc;
      rc=f_mount(fat_fs, currdir[0], 1);
      if (rc) {
#ifdef INITDEBUG
         log_printf("mnt: %d\n",rc);
#endif
         exit_pm32(QERR_FATERROR);
      } else
         extvol[0].flags|=VDTA_FAT;
   }
#if defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2,"hlp_finit() done\n");
#endif
}

// fini file i/o
void hlp_fdone() {
   IOPTR;
   log_misc(2,"hlp_fdone()\n");
   if (!iop) return;
   if (file_ok) hlp_fclose();
   if (micro) mfs_term();
   cache_ctrl(CC_SHUTDOWN,0);
   // empty virtual disk
   if (Disk1Data) memset(Disk1Data,0,Disk1Size);
}

u32t _std hlp_chdir(const char *path) {
   if (!path) return 0; else {
      int drive = path[1]==':'?path[0]-'0':currdisk;
      FRESULT rc = f_chdir(path);
      if (rc==FR_OK) {
         char *cc = currdir[drive], vn[3];
         *cc=0;
         if (drive!=currdisk) {
            vn[0] = '0'+drive; vn[1] = ':'; vn[2] = 0;
            f_chdrive(vn);
         }
         f_getcwd(cc, QS_MAXPATH+1);

         if (drive!=currdisk) {
            vn[0] = '0'+currdisk;
            f_chdrive(vn);
         }
         do {
            cc = strchr(cc,'/');
            if (cc) *cc='\\';
         } while (cc);
      } else
         log_misc(2,"f_chdir(%s)=%d\n",path,rc);
      return rc==FR_OK;
   }
}

u32t _std hlp_chdisk(u8t drive) {
   char    vn[3];
   FRESULT rc;
   vn[0] = '0'+drive; vn[1] = ':'; vn[2] = 0;
   rc = f_chdrive(vn);
   if (rc==FR_OK) currdisk = drive;
   return rc==FR_OK;
}

u8t  _std hlp_curdisk(void) {
   return currdisk;
}

char* _std hlp_curdir(u8t drive) {
   return drive>=_VOLUMES?0:currdir[drive];
}

void init_vol1data(void) {
   vol_data *vdta = extvol+DISK_LDR;
   if (!vdta->length) {
      vdta->length = Disk1Size>>LDR_SSHIFT;
      vdta->start  = 0;
      vdta->disk   = QDSK_VOLUME + DISK_LDR;
      vdta->sectorsize = LDR_SSIZE;
      vdta->flags |= VDTA_FAT;
   }
}

u32t _std hlp_boottype(void) {
#ifdef EFI_BUILD
   return QSBT_FSD;
#else
   static u8t boottype = 0xFF;

   if (boottype==0xFF) {
      if ((dd_bootflags&BF_MICROFSD)==0) {
         // check OS2BOOT presence in the root of FAT12/16
         if (extdrv[DISK_BOOT]->fs_type==FST_FAT32 || hlp_fopen("OS2BOOT")==FFFF) 
            boottype = QSBT_SINGLE;
         else {
            boottype = QSBT_FAT;
            hlp_fclose();
         }
      } else 
         boottype = filetable.ft_cfiles>=5?QSBT_PXE:QSBT_FSD;
   } 
   return boottype;
#endif // EFI_BUILD
}

u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count) {
   // no curcular refences allowed!!! ;)
   if (disk==QDSK_VOLUME+(u32t)drive || !mount_buf) return 0;
   // no 2Tb partitions!
   if (count>=_4GBLL) return 0;
   // mount it
   if (extvol && drive>DISK_LDR && drive<_VOLUMES) {
      struct Boot_Record *br = (struct Boot_Record *)mount_buf;
      u32t     sectsz,
              *fsname = 0;
      u64t        tsz = hlp_disksize64(disk, &sectsz);
      vol_data  *vdta = extvol+drive;
      FATFS     *fdta = extdrv[drive];
      char      dp[8];
      FRESULT    mres;
      // check for init, size and sector count overflow
      if (!tsz||sector>=tsz||sector+count>tsz||sector+count<sector) return 0;
      // unmount current
      hlp_unmountvol(drive);
      // read boot sector
      if (!hlp_diskread(disk|QDSK_DIRECT, sector, 1, mount_buf)) return 0;
      // disk parameters for fatfs i/o
      vdta->disk       = disk;
      vdta->start      = sector;
      vdta->length     = count;
      vdta->sectorsize = sectsz;
      vdta->serial     = 0;
      vdta->badclus    = 0;
      vdta->clsize     = 1;
      vdta->clfree     = 0;
      vdta->cltotal    = 0;

      vdta->fsname[0]  = 0;
      snprintf(dp,8,"%d:\\",drive);
      // mark as mounted else disk i/o deny disk access
      vdta->flags      = VDTA_ON;

      if (br->BR_BPB.BPB_SecPerFAT)
         if (br->BR_BPB.BPB_RootEntries) fsname = (u32t*)br->BR_EBPB.EBPB_FSType;
            else fsname = (u32t*)((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_FSType;
      // call mount to setup FatFs vars
      mres = f_mount(fdta,dp,1);
      // if it is not FAT really - fix error code to accptable
      if (mres!=FR_OK && (!fsname || (*fsname&0xFFFFFF)!=0x544146))
         mres = FR_NO_FILESYSTEM;
      // allow ok and no-filesystem error codes
      if (mres==FR_OK || mres==FR_NO_FILESYSTEM) {
         /* prior to FatFs R0.10 this call was required to force FatFs
            chk_mounted(), now this can be done by f_mount parameter.
            In addition, function update current drive in QSINIT module. */
         if (mres==FR_OK) {
            hlp_chdir(dp);
            strcpy(vdta->fsname,fdta->fs_type<=FS_FAT16?"FAT":"FAT32");
            vdta->clsize  = fdta->csize;
            vdta->cltotal = (vdta->length - fdta->database)/fdta->csize;
            vdta->flags  |= VDTA_FAT;
         }
         cache_ctrl(CC_MOUNT,drive);
         return 1;
      } else
         log_printf("mount err %d\n", mres);
      // clear state
      vdta->flags = 0;
   }
   return 0;
}

u32t _std hlp_unmountvol(u8t drive) {
   u32t     flags;
   if (!extvol || drive<=DISK_LDR || drive>=_VOLUMES) return 0;
   flags = extvol[drive].flags;
   if ((flags&VDTA_ON)==0) return 0;
   // change current disk to 1:
   if (hlp_curdisk()==drive) hlp_chdisk(DISK_LDR);

   cache_ctrl(CC_UMOUNT,drive);
   extvol[drive].flags = 0;
   
   if ((flags&VDTA_FAT)!=0) {
      char  dp[8];
      snprintf(dp,8,"%d:",drive);
      f_mount(0,dp,0);
   }
   return 1;
}

u32t _std hlp_volinfo(u8t drive, disk_volume_data *info) {
   if (extvol && drive<_VOLUMES) {
      vol_data  *vdta = extvol+drive;
      FATFS     *fdta = extdrv[drive];
      // mounted?
      if (drive<=DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
         if (info) {
            info->StartSector  = vdta->start;
            info->TotalSectors = vdta->length;
            info->Disk         = vdta->disk;
            info->SectorSize   = vdta->sectorsize;
            // was it really mounted?
            if ((vdta->flags&VDTA_FAT)!=0) {
               FATFS *pfat = 0;
               DWORD nclst = 0;
               char     cp[3];
               cp[0]='0'+drive; cp[1]=':'; cp[2]=0;

               info->ClSize      = fdta->csize;
               info->RootDirSize = fdta->n_rootdir;
               info->FatCopies   = fdta->n_fats;
               info->DataSector  = fdta->database;
               // update info to actual one
               if (f_getfree(cp,&nclst,&pfat)==FR_OK) vdta->clfree = nclst;
               if (!vdta->serial) f_getlabel(cp,vdta->label,&vdta->serial);
            } else {
               /* Not a FAT.
                  HPFS format still can fill vdta in way, compatible with
                  "Format" command printing and this function will return
                  it, because nobody touch it until unmount */
               info->RootDirSize = 0;
               info->FatCopies   = 0;
               info->DataSector  = 0;
            }
            info->ClBad     = vdta->badclus;
            info->SerialNum = vdta->serial;
            info->ClSize    = vdta->clsize;
            info->ClAvail   = vdta->clfree;
            info->ClTotal   = vdta->cltotal;
            strcpy(info->Label,vdta->label);
            strcpy(info->FsName,vdta->fsname);
         }
         return fdta->fs_type;
      }
   }
   if (info) memset(info,0,sizeof(disk_volume_data));
   return FST_NOTMOUNTED;
}
