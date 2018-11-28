//
// QSINIT
// virtual disk init code
//
#include "qslocal.h"
#include "qsconst.h"
#include "unzip.h"

void       *sec_data;
u32t        sec_size = 0;

static int unpack(ZIP *zz) {
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

void unpack_start_module(void) {
   u32t  zsize;
   ZIP      zz; // 300 bytes!!
   void   *zip = hlp_freadfull("QSINIT.LDI", &zsize, 0);
   if (!zip) exit_pm32(QERR_NOEXTDATA);
   if (zip_open(&zz, zip, zsize)) exit_pm32(QERR_NOEXTDATA);
   // unpack START module from QSINIT.LDI to memory block
   if (!unpack(&zz)) exit_pm32(QERR_RUNEXTDATA);
   zip_close(&zz);
   sto_save(STOKEY_ZIPDATA, zip, zsize, 0);
}
