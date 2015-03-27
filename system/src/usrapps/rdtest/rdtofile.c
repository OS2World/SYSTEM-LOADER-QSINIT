/*
    another one IOCTL example: copy entire PAE disk to single file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hmio.h"

typedef unsigned long long u64t;

FILE *fout = 0;

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
   if (fout) fclose(fout);
   exit(1);
}

void main(int argc, char *argv[]) {
   ULONG   handle;
   APIRET      rc;

   if (argc!=2) {
      printf("This app copies entire PAE disk to single file on disk!\n");
      printf("usage: rdtofile <dump_file>\n");
      exit(2);
   }

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
      ULONG   sz, ii;
      UCHAR   sector[512];

      if (!mbsize) {
         printf("Unable to query memory size\n");
         break;
      }
      hm_chsgeo(handle, &disk_on, 0, 0, 0);

      printf("Disk size: %dkb %s\n", mbsize>>1, disk_on?"":"(IOCTL i/o only)");

      fout = fopen(argv[1], "wb");
      if (!fout) {
         printf("Unable to create \"%s\"\n", argv[1]);
         break;
      }
      for (ii=0; ii<mbsize; ii++) {
         ULONG actual;
         if (hm_read(handle, ii, 1, &sector, &actual)) {
            // there is no chance to reach this point
            printf("Error reading sector %08X\n", ii);
            memset(&sector, 0, 512);
         }
         if (fwrite(&sector,1,512,fout)!=512) {
            printf("Error writing file (disk full?)\n");
            fclose(fout);
            exit(2);
         }
      }
      fclose(fout);
   } while (0);

   rc = hm_close(handle);
   if (rc) errexit("Driver close error",rc);
}
