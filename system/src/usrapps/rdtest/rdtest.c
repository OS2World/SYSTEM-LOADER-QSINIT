#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hmio.h"

typedef unsigned long long u64t;

ULONG getmsuptime(void) {
   ULONG res;
   DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &res, sizeof(res));
   return res;
}

void errexit(const char *str, APIRET code) {
   char msgbuf[256];
   ULONG   len = 0;
   msgbuf[0] = 0;
   DosGetMessage(0, 0, msgbuf, 256, code, "OSO001.MSG", &len);
   msgbuf[len] = 0;
   printf("%s (%d): %s\n", str, code, msgbuf);
   exit(1);
}

void main(int argc, char *argv[]) {
   ULONG   handle;
   APIRET      rc;

   rc = hm_open(1, &handle);
   if (rc==ERROR_WRITE_PROTECT) {
      printf("Opening in non-exclusive mode.\n");
      rc = hm_open(0, &handle);
   } else
   if (!rc)
      printf("Opening in exclusive mode.\n");

   if (rc) errexit("Driver open error",rc);

   do {
      ULONG   mbsize = hm_getsize(handle),
             disk_on = 0;
      ULONG     pass, sz;

      if (!mbsize) {
         printf("Unable to query memory size\n");
         break;
      }
      hm_chsgeo(handle, &disk_on, 0, 0, 0);

      printf("Memory size: %dkb %s\n", mbsize>>1, disk_on?"":"(IOCTL i/o only)");
#if 1
      // query disk location (will fail in non-exclusive mode)
      sz = hm_getinfo(handle, 0, 0);
      if (sz) {
         HM_LOCATION_INFO *li = malloc(sizeof(HM_LOCATION_INFO) * sz);
         if (li) {
            sz = hm_getinfo(handle, li, sz);
            if (sz) {
               printf("Used memory ranges:\n");
               for (pass=0; pass<sz; pass++)
                  printf("%2d. %08X000 %10d kb\n", pass+1, li[pass].physpage,
                     li[pass].sectors>>1);
            }
            free(li);
         }
      }
#endif
      /* make READ pass in any case and WRITE pass if no ram disk,
         i.e. "basedev = HD4DISK.ADD /i" used */
      for (pass = 0; pass < (disk_on?1:2); pass++) {
         // step down from 4Mb to 1Kb i/o block
         for (sz = 1024*1024*4; sz>=1024; sz/=4) {
            void     *mem = malloc(sz + 4096),
                    *amem = (void*)((ULONG)mem + 0xFFF & ~0xFFF); //align to page start
            ULONG tmstart = getmsuptime(),
                     step = sz/512,
                    steps = mbsize/step, ii, actual;
            APIRET    res = 0;

            for (ii=0; ii<steps; ii++) {
               res = pass ? hm_write (handle, ii*step, step, amem, &actual) 
                          : hm_read  (handle, ii*step, step, amem, &actual);
               if (res || actual!=step) {
                  printf("Error %d on pos %X, %d sectors instead of %d\n",
                     res, ii*step, actual, step);
                  break;
               }
            }
            if (!res) {
               tmstart = getmsuptime() - tmstart;
               if (tmstart) {
                  u64t bytes_per_sec = (u64t)mbsize * 512 / tmstart * 1000;
                  printf("%s by %4d kb blocks - speed %8d kb per second.\n", 
                     pass?"Write":"Read ", sz>>10, (ULONG)(bytes_per_sec>>10));
               }
            }
            free(mem);
         }
      }
   } while (0);

   rc = hm_close(handle);
   if (rc) errexit("Driver close error",rc);
}
