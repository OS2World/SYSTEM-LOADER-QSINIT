//
// QSINIT
// volume / micro-FSD api file i/o
//
#include "qslocal.h"
#include "qsconst.h"
#include "parttab.h"

#ifndef EFI_BUILD
#include "filetab.h"
extern char            mfsd_openname[];   ///< current file name
extern struct filetable_s  filetable;
extern u16t                mfs_table[];
void    hlp_fatsetup(void);
#else
char    mfsd_openname[MFSD_NAME_LEN];
#endif
extern u32t               mfsd_fsize;

static u32t                    micro;     ///< micro-fsd present?
static u32t              file_in_use;     ///< file opened
static u32t                 file_len;     ///< length of the current file
vol_data*                     extvol = 0;
u8t                         currdisk = 0;
u8t                           bootFT = 0; ///< boot partition FAT type (FST_*)
u8t                         pxemicro = 0; ///< PXE micro-fsd?
u8t                         streamio = 0; ///< use strm_read?

// micro-fsd call thunks
extern u16t _std    mfs_open (void);
extern u16t _std    strm_open(void);
extern u16t _std    fat_open (void);
/* call mfs_read/strm_read.
   buf must be placed in 1Mb for RM call and paragraph aligned, at least */
extern u32t __cdecl mfs_read (u32t offset, u32t  buf, u32t readsize);
extern u32t __cdecl fat_read (u32t offset, void *buf, u32t readsize);
extern u16t __cdecl strm_read(u32t buf, u16t readsize);
extern void __cdecl mfs_close(void);
extern void __cdecl fat_close(void);
extern void __cdecl mfs_term (void);
extern void __cdecl fat_term (void);

// cache control
void _std cache_ctrl(u32t action, u8t vol);
// setup boot partition i/o
void init_vol0();

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
   /* at least HPFS micro-FSD does not uppercase requested names, so do it
      here, but leave original case for the Moveton`s PXE stream i/o */
   for (ii=0; ii<MFSD_NAME_LEN-1; ii++) {
      char ch = streamio?mfsd_openname[ii]:toupper(mfsd_openname[ii]);
      mfsd_openname[ii] = ch;
      if (!ch) { // remove trailing dot
         if (ii--)
            if (mfsd_openname[ii]=='.') mfsd_openname[ii] = 0;
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
u32t hlp_fopen2(const char *name, int allow_pxe) {
   u32t  result = FFFF;
   /* mutex blocks us in MT mode & file_in_use deny second open in the same
      thread. Mutex will block other threads until hlp_fclose()! */
   mfs_lock();
   if (extvol && !file_in_use && name) {
#if defined(EFI_BUILD) // || defined(INITDEBUG)
      log_misc(2,"hlp_fopen(%s)\n",name);
#endif
      // only disk FSD, not PXE
      if (!pxemicro || allow_pxe) {
         u16t    rc;
         nametocname(name);
         if (mod_secondary && mod_secondary->mfs_replace)
            rc = mod_secondary->mfs_open(mfsd_openname, &mfsd_fsize); else
               rc = bootFT?fat_open():(streamio?strm_open():mfs_open());
#ifdef INITDEBUG
         log_misc(2,"hlp_fopen(%s) = %X, size %u\n", mfsd_openname, rc, mfsd_fsize);
#endif
         if (!rc) result = pxemicro?0:mfsd_fsize;
      }
   }
   if (result==FFFF) mfs_unlock(); else {
      file_in_use = 1;
      file_len    = result;
   }
   return result;
}

// open file. return -1 on no file, else file size
u32t hlp_fopen(const char *name) {
   return hlp_fopen2(name, 0);
}

/* read file.
   PXE accepted too, but offset is ignored. PXE hlp_fopen can only be done
   from unzip code in this module and does not exported outside. */
u32t hlp_fread2(u32t offset, void *buf, u32t readsize, read_callback cbprint) {
   u32t      res = 0;
   /* grab mutex even to check flags, this should cause no troubles for owner,
      but stops other threads */
   mfs_lock();
   //log_misc(2,"hlp_fread(%d,%08X,%d,%08x)\n",offset,buf,readsize,cbprint);
   if (extvol && file_in_use && readsize && (pxemicro || offset<file_len)) {
      u32t   offsb = offset, sizeb, ppr = 0;
      u8t  *dstpos = (u8t*)buf;

      if (!pxemicro && offset+readsize>file_len) readsize = file_len-offset;
      sizeb = readsize>>10;
      res   = readsize;
      do {
         u32t  rds = readsize>DISKBUF_SIZE?DISKBUF_SIZE:readsize, actual;

         if (mod_secondary && mod_secondary->mfs_replace)
            actual = mod_secondary->mfs_read(offset, dstpos, rds);
         else
         if (bootFT) actual = fat_read(offset, dstpos, rds); else {
            actual = streamio?strm_read(DiskBufPM, rds):
                              mfs_read(offset, DiskBufPM, rds);
            // copy from disk i/o buffer in low memory to user buffer
            memcpy(dstpos, (void*)DiskBufPM, rds);
         }
         readsize -= actual;
         // read error, exiting...
         if (actual!=rds) break;
         dstpos+=rds; offset+=rds;
         if (cbprint) {
            u32t pr = (offset-offsb>>10)*100/sizeb;
            if (pr!=ppr) (*cbprint)(ppr=pr, offset-offsb);
         }
      } while (readsize);
      res -= readsize;
   }
   // dec mutex usage
   mfs_unlock();
   return res;
}

u32t hlp_fread(u32t offset, void *buf, u32t readsize) {
   return hlp_fread2(offset, buf, readsize, 0);
}

// close file
void hlp_fclose() {
   mfs_lock();
   if (extvol && file_in_use) {
      if (mod_secondary && mod_secondary->mfs_replace) mod_secondary->mfs_close(); else
         if (bootFT) fat_close(); else mfs_close();
      file_in_use = 0;
      file_len    = 0;
      /* dec mutex usage & finally release it by second call. If thread forgot to
         do that - special code in START will check for this mutex on and call
         hlp_fclose() */
      mfs_unlock();
   }
   mfs_unlock();
}

u32t _std hlp_fexist(const char *name) {
   u32t res = 0;
   mfs_lock();

   if (extvol && !file_in_use && name) {
#ifndef EFI_BUILD
      if (pxemicro) {
         nametocname(name);
         if ((streamio?strm_open():mfs_open())==0) {
            res = FFFF;
            mfs_close();
         }
      } else
#endif // EFI_BUILD
      {
         res = hlp_fopen(name);
         if (res==FFFF) res = 0; else hlp_fclose();
      }
   }
   mfs_unlock();
   return res;
}

// open, read, close file & return buffer with it
void* hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint) {
   void   *res = 0;
   u32t rcsize = 0;

   mfs_lock();
   if (extvol && !file_in_use && bufsize && name) {
      *bufsize=0;
#ifndef EFI_BUILD
      if (pxemicro) {
         u32t  asize = _256KB;
         nametocname(name);
         if ((streamio?strm_open():mfs_open())==0) {
            // alloc 256kb first
            res = hlp_memallocsig(asize, "file", QSMA_RETERR|QSMA_LOCAL);
            while (res) {
               u16t readed = streamio?strm_read(DiskBufPM, DISKBUF_SIZE):
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
            if (res) res = hlp_memrealloc(res, rcsize);
            *bufsize = rcsize;
         }
      } else
#endif // EFI_BUILD
      {
         rcsize = hlp_fopen(name);
         if (rcsize!=FFFF) {
            if (rcsize) {
               res = hlp_memallocsig(rcsize, "file", QSMA_RETERR|QSMA_LOCAL);
               *bufsize = res ? hlp_fread2(0, res, rcsize, cbprint) : 0;
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
#if 0 //defined(EFI_BUILD) // || defined(INITDEBUG)
   log_misc(2,"hlp_finit()\n");
#endif
   if (extvol) return;
#ifdef EFI_BUILD
   micro    = 1;
   pxemicro = 0;
   streamio = 0;
#else
   micro    = dd_bootflags&BF_MICROFSD?1:0;
   pxemicro = micro && filetable.ft_cfiles>=5;
   streamio = micro && filetable.ft_cfiles==6;
#endif
   // additional volumes (2-9) mount data
   extvol = (vol_data*)hlp_memallocsig(sizeof(vol_data)*DISK_COUNT, "vol", QSMA_READONLY);
   sto_save(STOKEY_VOLDATA, extvol, sizeof(vol_data)*DISK_COUNT, 0);
   // setup volume/disk i/o for boot partition (drive 0)
   init_vol0();
#ifndef EFI_BUILD
   if (!micro && bootio_avail) hlp_fatsetup();
#endif
}

// fini file i/o
void hlp_fdone(void) {
   static int once = 0;
   log_misc(2,"hlp_fdone()\n");
   if (once++) return;
   if (file_in_use) hlp_fclose();
   if (bootFT) fat_term(); else
      if (micro) mfs_term();
   cache_ctrl(CC_SHUTDOWN,0);
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
         if (bootFT>=FST_FAT32 || hlp_fopen("OS2BOOT")==FFFF)
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
