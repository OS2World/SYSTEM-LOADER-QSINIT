//
// QSINIT
// virtual disk init code
//
#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#include "unzip.h"
#include "fat/ff.h"
#include "qsconst.h"
#include "qsstor.h"

void *Disk1Data;      // own data virtual disk
u32t  Disk1Size = 0;  // and it`s size

extern FATFS*    extdrv[_VOLUMES];
extern DWORD     FileCreationTime;
extern u8t      *page_buf,
                 mod_delay;

void    init_vol1data(void);
FRESULT f_mkfs2(void);

int unpack_zip(int disk, ZIP *zz, u32t totalsize) {
   FIL        fh;   // we have at least 16k stack, so 4k fits good.
   char *getpath;
   u32t  getsize=0;
   int   errors =0, errprev;
   u32t  ready  =0;

   if (disk<0) return 1;
   if (!zz) return 1;

   while (zip_nextfile(zz,&getpath,&getsize)) {
      // allocate at least MAXPATH bytes ;)))
      void *filemem = 0;
      int   skip    = 0;
      // print process
#if 0
      log_printf("Extracting... %d%%",(ready>>10)*100/(totalsize>>10));
#endif
      ready+=getsize;
      // ignore directories and/or empty files here
      if (!getsize) continue;

      if (mod_delay && disk==DISK_LDR) {
         char *ep = strchr(getpath,'.');
         // delayed unpack
         if (ep && (!strnicmp(ep,".EXE",4)||!strnicmp(ep,".DLL",4))) {
            filemem = hlp_memalloc(getsize,0);
            // save delayed cound, will stop on 256 overflow ;)
            sto_save(STOKEY_DELAYCNT,&mod_delay,4,1);
            mod_delay++;
            skip = 1;
         }
      }
      // unpack & save
      if (!filemem) filemem = zip_readfile(zz);

      if (!filemem) errors++; else {
         FRESULT rc;
         // create full path (x:\name)
         char *path=(char*)page_buf;
         path[0]='0'+disk;
         path[1]=':';
         path[2]='\\';
         strcpy(path+3,getpath);
         // report zip file time to FAT engine
         FileCreationTime=zz->dostime;
         // create file
         rc = f_open(&fh, path, FA_WRITE|FA_CREATE_ALWAYS);
         // no such path? trying to create it
         if (rc==FR_NO_PATH) {
            char *cc=path;
            do {
               cc=strchr(cc,'/');
               if (cc) *cc='\\';
            } while (cc);
            cc=path+3;

            do {
               cc=strchr(cc,'\\');
               if (cc) {
                  *cc=0;
                  rc = f_mkdir(path);
                  *cc++='\\';
                  if (rc!=FR_EXIST && rc!=FR_OK) break;
               }
            } while (cc);
            // open file again
            rc = f_open(&fh, path, FA_WRITE|FA_CREATE_ALWAYS);
         }
         // these messages scroll up from screen actual info in verbose build
#ifndef INITDEBUG
         log_printf("%s %s, %d bytes\n",skip?"skip ":"unzip",path,getsize);
#endif
         errprev=errors;
         if (rc==FR_OK) {
            UINT rsize;
            if (getsize)
               if (f_write(&fh, filemem, getsize, &rsize)!=FR_OK) errors++;
            if (f_close(&fh)!=FR_OK) errors++;
         } else errors++;
         if (errprev!=errors) log_printf("error %d\n",rc);
      }
      if (filemem) hlp_memfree(filemem);
   }
   // reset FAT engine to use current time
   FileCreationTime=0;
#if 0
   log_printf("Extracting... done\n");
#endif
   return errors;
}

static void *try_alloc_disk(u32t size)  {
   void *rc;
   size = Round64k(size);
#ifdef INITDEBUG
   log_printf("dsk1 %d kb\n",size>>10);
#endif
   rc = hlp_memallocsig(size, "disk", QSMA_RETERR);
   if (rc) Disk1Size = size;
   return rc;
}

void make_disk1(void) {
   static const char  vp[3] = { DISK_LDR+'0', ':', 0 };
   u32t   size, unpsize, files;
   FRESULT  rc;
   void   *zip, *disk = 0;
   ZIP      zz; // 300 bytes!!

   zip     = hlp_freadfull("QSINIT.LDI",&size,0);
   if (!zip) exit_pm32(QERR_NOEXTDATA);
   if (!zip_open(&zz, zip, size)) exit_pm32(QERR_NOEXTDATA);
   unpsize = zip_unpacked(&zz,&files);
   if (!unpsize) exit_pm32(QERR_NOEXTDATA);
   // +4k for every file + reserved 384k
   files   = unpsize + (files<<12) + _256KB + _128KB;
   if (files<_1MB+_256KB) files = _1MB+_256KB;
   // trying to allocate size from ini first
   if (Disk1Size) disk = try_alloc_disk(Disk1Size + files);
   // and required size if requested was failed
   if (!disk) disk = try_alloc_disk(files);
   // panic
   if (!disk) exit_pm32(QERR_NODISKMEM);
   Disk1Data = disk;
   // re-open zip
   zip_close(&zz);
   zip_open(&zz, zip, size);
   // mount & make internal disk with common loader code
   rc = f_mount(extdrv[DISK_LDR], vp, 0);
   if (!rc) rc = f_mkfs2();
   if (rc) {
#ifdef INITDEBUG
      log_printf("mkfs: %d\n",rc);
#endif
      exit_pm32(QERR_RUNEXTDATA);
   }
   // unpack QSINIT.LDI here
   rc=unpack_zip(DISK_LDR,&zz,unpsize);
   if (rc) exit_pm32(QERR_RUNEXTDATA);
   // free memory
   zip_close(&zz);
   // free zip if delay was turned off, else save it for future unpacks
   if (!mod_delay) hlp_memfree(zip); else 
      sto_save(STOKEY_ZIPDATA,zip,size,0);
   // hlp_chdisk(DISK_LDR);
   // for mount list command
   init_vol1data();
}
