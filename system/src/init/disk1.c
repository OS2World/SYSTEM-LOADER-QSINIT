//
// QSINIT
// virtual disk init code
//
#include "qslocal.h"
#include "qsconst.h"
#include "unzip.h"

void      *Disk1Data;               ///< virtual disk
u32t       Disk1Size = 0;           ///< virtual disk`s size
void       *sec_data;
u32t        sec_size = 0;

void init_vol1(void);

static int unpack_start(ZIP *zz) {
   char  *zfpath;
   u32t   zfsize = 0;

   if (zz)
      while (zip_nextfile(zz, &zfpath, &zfsize)==0)
         // ignore directories and/or empty files here
         if (zfsize && strnicmp(zfpath, MODNAME_START, strlen(zfpath))==0)
            if (zip_readfile(zz, &sec_data)==0) {
               sec_size = zfsize;
               return 1;
            }
   return 0;
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
   u32t   size, unpsize, files;
   void   *zip, *disk = 0;
   ZIP      zz; // 300 bytes!!

   zip     = hlp_freadfull("QSINIT.LDI",&size,0);
   if (!zip) exit_pm32(QERR_NOEXTDATA);
   if (zip_open(&zz, zip, size)) exit_pm32(QERR_NOEXTDATA);
   unpsize = zip_unpacked(&zz,&files);
   if (!unpsize) exit_pm32(QERR_NOEXTDATA);
   // +4k for every file + reserved 384k
   files   = unpsize + (files<<12) + _512KB + _128KB;
   if (files < _1MB+_512KB) files = _1MB+_512KB;
   // trying to allocate size from ini first
   if (Disk1Size) disk = try_alloc_disk(Disk1Size + files);
   // and required size if requested was failed
   if (!disk) disk = try_alloc_disk(files);
   // panic
   if (!disk) exit_pm32(QERR_NODISKMEM);
   Disk1Data = disk;
   // re-open zip (our`s ugly code can`t rewind the stream)
   zip_close(&zz);
   zip_open(&zz, zip, size);
   // unpack START module from QSINIT.LDI to memory block
   if (!unpack_start(&zz)) exit_pm32(QERR_RUNEXTDATA);
   zip_close(&zz);

   sto_save(STOKEY_ZIPDATA, zip, size, 0);
   // mount/format volume
   init_vol1();
}
