//
// QSINIT
// volume / micro-FSD api file i/o
//
#include "clib.h"
#include "qsint.h"
#include "qsinit.h"
#include "qsmodint.h"
#include "qsutil.h"
#include "ff.h"
#include "qsconst.h"
#include "qsstor.h"
#include "parttab.h"

#ifndef EFI_BUILD
#include "filetab.h"
extern u8t              dd_bootflags;
extern char            mfsd_openname[];   ///< current file name
extern struct filetable_s  filetable;
#else
char    mfsd_openname[MFSD_NAME_LEN];
#endif
extern u32t                DiskBufPM;     ///< flat disk buffer address
extern u8t               dd_bootdisk;
extern u32t               mfsd_fsize;

static u32t                    micro;     ///< micro-fsd present?
static u32t                 pxemicro;     ///< PXE micro-fsd?
static u32t              file_in_use;     ///< file opened
static FATFS*                 fat_fs = 0; ///< FAT disk struct
static FIL*                   fat_fl;     ///< FAT file struct
static u32t                 file_len;     ///< length of the current file
extern
mod_addfunc           *mod_secondary;     ///< START exports for us

FATFS*                        extdrv[_VOLUMES];
vol_data*                     extvol = 0;
char*                        currdir[_VOLUMES];
u8t                         currdisk = 0;

extern void               *Disk1Data;     ///< own data virtual disk
extern u32t                Disk1Size;     ///< and it`s size

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

// grab fsd file mutex
static void mfs_lock(void) {
   if (mod_secondary && mod_secondary->in_mtmode)
      mod_secondary->muxcapture(mod_secondary->mfs_mutex);
}

/** release fsd file mutex.
    Note, that exit code in START will call hlp_fclose() if this semaphore
    owned by exiting thread */
static void mfs_unlock(void) {
   if (mod_secondary && mod_secondary->in_mtmode)
      mod_secondary->muxrelease(mod_secondary->mfs_mutex);
}

// open file. return -1 on no file, else file size
u32t hlp_fopen(const char *name) {
   u32t  result = FFFF;
   /* mutex blocks us in MT mode & file_in_use deny second open in the same
      thread. Mutex will block other threads until hlp_fclose()! */
   mfs_lock();
   if (fat_fs && !file_in_use) {
#if defined(EFI_BUILD) // || defined(INITDEBUG)
      log_misc(2,"hlp_fopen(%s)\n",name);
#endif
      if (micro) {
         // only disk FSD, not PXE
         if (!pxemicro) {
            nametocname(name);
            if (mfs_open()==0) result = mfsd_fsize;
         }
      } else {
         FRESULT rc;
         mfsd_openname[0]='0'; mfsd_openname[1]=':'; mfsd_openname[2]='\\';
         strncpy(mfsd_openname+3, name, MFSD_NAME_LEN-4);
         mfsd_openname[MFSD_NAME_LEN-1]=0;
         // lock FatFs call
         mt_swlock();
         rc = f_open(fat_fl, mfsd_openname, FA_READ);
         if (!rc) result = f_size(fat_fl); else {
#if 0 // defined(INITDEBUG)
            log_printf("opn(%s): %d\n", mfsd_openname, rc);
#endif
         }
         mt_swunlock();
      }
#if 0 // defined(INITDEBUG)
      log_printf("%s size: %d\n", mfsd_openname, retsize);
#endif
   }
   if (result==FFFF) mfs_unlock(); else {
      file_in_use = 1;
      file_len    = result;
   }
   return result;
}

// read file
u32t hlp_fread2(u32t offset, void *buf, u32t readsize, read_callback cbprint) {
   u32t      res = 0;
   FRESULT fatrc = 0;
   /* grab mutex even to check flags, this should cause no troubles for owner,
      but stops other threads */
   mfs_lock();
   //log_misc(2,"hlp_fread(%d,%08X,%d,%08x)\n",offset,buf,readsize,cbprint);
   if (fat_fs && file_in_use && readsize && offset<file_len) {
      if (offset+readsize>file_len) readsize = file_len-offset;
      if (micro || cbprint) {
         u32t  offsb = offset, sizeb = readsize>>10, ppr = 0;
         u8t *dstpos = (u8t*)buf;
         res = readsize;
         do {
            u32t  rds, actual;
            if (micro) {
               rds    = readsize>DISKBUF_SIZE?DISKBUF_SIZE:readsize;
               actual = mfs_read(offset, DiskBufPM, rds);
               // copy from disk i/o buffer in low memory to user buffer
               memcpy(dstpos, (void*)DiskBufPM, rds);
            } else {
               // lock FatFs call
               mt_swlock();
               rds   = readsize>_1MB?_1MB:readsize;
               fatrc = f_read(fat_fl, dstpos, rds, (UINT*)&actual);
               mt_swunlock();
               if (fatrc) break;
            }
            readsize-=actual;
            // read error, exiting...
            if (actual!=rds) break;
            dstpos+=rds; offset+=rds;
            if (cbprint) {
               u32t pr = (offset-offsb>>10)*100/sizeb;
               if (pr!=ppr) (*cbprint)(ppr=pr, offset-offsb);
            }
         } while (readsize);
         res -= readsize;
      } else {
         mt_swlock();
         fatrc = f_read(fat_fl, buf, readsize, (UINT*)&res);
         mt_swunlock();
      }
   }
   // dec mutex usage
   mfs_unlock();
#ifdef INITDEBUG
   if (fatrc) log_printf("rd(%s): %d\n", mfsd_openname, fatrc);
#endif
   return res;
}

u32t hlp_fread(u32t offset, void *buf, u32t readsize) {
   return hlp_fread2(offset, buf, readsize, 0);
}

// close file
void hlp_fclose() {
   mfs_lock();
   if (fat_fs && file_in_use) {
      if (micro) mfs_close(); else {
         mt_swlock();
         f_close(fat_fl);
         mt_swunlock();
      }
      file_in_use = 0;
      file_len    = 0;
      /* dec mutex usage & finally release it by second call. If thread forgot to
         do that - special code in START will check for this mutex on and call
         hlp_fclose() */
      mfs_unlock();
   }
   mfs_unlock();
}

// open, read, close file & return buffer with it
void* hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint) {
   void   *res = 0;
   u32t rcsize = 0;

   mfs_lock();
   if (fat_fs && !file_in_use && bufsize) {
      *bufsize=0;
#ifndef EFI_BUILD
      if (pxemicro) {
         u32t  asize = _256KB;
         char   mpxe = filetable.ft_cfiles==6;
         nametocname(name);
         if ((mpxe?strm_open():mfs_open())==0) {
            // alloc 256kb first
            res = hlp_memallocsig(asize, "file", 0);
            while (true) {
               u16t readed = mpxe?strm_read(DiskBufPM, DISKBUF_SIZE):
                                   mfs_read(rcsize, DiskBufPM, DISKBUF_SIZE);
               if (!readed || readed==0xFFFF) break;
               if (rcsize+readed>asize)
                  res = hlp_memrealloc(res, asize+=_256KB);
               memcpy((u8t*)res+rcsize, (void*)DiskBufPM, readed);
               rcsize += readed;
               if (cbprint) (*cbprint)(0,rcsize);
            }
            mfs_close();
            // trim used memory
            res = hlp_memrealloc(res, rcsize);
            *bufsize = rcsize;
         }
      } else 
#endif // EFI_BUILD
      {
         rcsize = hlp_fopen(name);
         if (rcsize!=FFFF) {
            if (rcsize) {
               res = hlp_memallocsig(rcsize, "file", 0);
               *bufsize = hlp_fread2(0, res, rcsize, cbprint);
            }
            hlp_fclose();
         }
      }
   }
   mfs_unlock();
   return res;
}

// init file i/o
void hlp_finit(void) {
   int    ii;
   char *ptr;
#if 0 //defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2,"hlp_finit()\n");
#endif
   if (fat_fs) return;
#ifdef EFI_BUILD
   micro    = 1;
   pxemicro = 0;
#else
   micro    = dd_bootflags&BF_MICROFSD?1:0;
   pxemicro = micro && filetable.ft_cfiles>=5;
#endif
   fat_fs = (FATFS*)hlp_memallocsig((sizeof(FATFS)+sizeof(vol_data))*_VOLUMES+
            Round16(QS_MAXPATH+1)*_VOLUMES, "vol", QSMA_READONLY);
   fat_fl = (FIL*)((u8t*)fat_fs+sizeof(FATFS));
   // initing arrays
   ptr    = (char*)fat_fs;
   for (ii=0; ii<_VOLUMES; ii++) { extdrv[ii]=(FATFS*)ptr; ptr+=sizeof(FATFS); }
   sto_save(STOKEY_FATDATA, &extdrv, _VOLUMES*sizeof(FATFS*), 0);
   // additional volumes (2-9) mount data
   extvol = (vol_data*)ptr;
   ptr   += sizeof(vol_data)*_VOLUMES;
   sto_save(STOKEY_VOLDATA, extvol, sizeof(vol_data)*_VOLUMES, 0);
   // current directory for all drives
   for (ii=0; ii<_VOLUMES; ii++) {
      currdir[ii]=ptr;
      ptr[0]='0'+ii; ptr[1]=':'; ptr[2]='\\'; ptr[3]=0;
      ptr+=Round16(QS_MAXPATH+1);
   }
   // setup volume/disk i/o for boot partition (drive 0)
   bootdisk_setup();
   // mount boot FAT (drive 0)
   if (!micro) {
      FRESULT rc;
      rc = f_mount(fat_fs, currdir[0], 1);
      if (rc) {
#ifdef INITDEBUG
         log_misc(2, "mnt: %d\n", rc);
#endif
         exit_pm32(QERR_FATERROR);
      } else
         extvol[0].flags|=VDTA_FAT;
   }
#if 0 //defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2, "hlp_finit() done\n");
#endif
}

// fini file i/o
void hlp_fdone(void) {
   static int once = 0;
   log_misc(2,"hlp_fdone()\n");
   if (once++) return;
   if (file_in_use) hlp_fclose();
   if (micro) mfs_term();
   cache_ctrl(CC_SHUTDOWN,0);
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
   /* EFI forced to be FSD because of danger in simultaneous access to EFI
      boot partition by BIOS & QSINIT */
   return QSBT_FSD;
#else
   static u8t boottype = 0xFF;

   if (boottype==0xFF) {
      if ((dd_bootflags&BF_MICROFSD)==0) {
         // check OS2BOOT presence in the root of FAT12/16
         if (extdrv[DISK_BOOT]->fs_type>=FST_FAT32 || hlp_fopen("OS2BOOT")==FFFF) 
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
