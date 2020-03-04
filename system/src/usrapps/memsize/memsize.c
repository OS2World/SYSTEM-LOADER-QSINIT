#define INCL_BASE
#define INCL_DOSDEVIOCTL
#include <os2.h>
#include "oemhlpex.h"
#include <stdio.h>

void main(void) {
   OEMHLP_MEMINFO    mi;
   OEMHLP_MEMINFOEX xmi;
   HFILE           drvh = 0;
   ULONG         action = 0, psize, ii;
   APIRET            rc;

   rc = DosOpen("\\DEV\\OEMHLP$", &drvh, &action, 0, FILE_NORMAL,
                OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE, NULL);
   if (rc) {
      printf("Unable to open OEMHLP!\n");
      return;
   }
   psize = sizeof(mi);
   rc    = DosDevIOCtl(drvh, IOCTL_OEMHLP, OEMHLP_GETMEMINFO, 0, 0, 0, &mi,
                       psize, &psize);
   if (rc) {
      printf("OEMHLP call failed!\n");
   } else {
      printf("Memory in 1st Mb : %7u kb\n", (ULONG)mi.LowMemKB);
      printf("Memory available : %7u kb\n", mi.HighMemKB);

      psize = sizeof(xmi);
      rc    = DosDevIOCtl(drvh, IOCTL_OEMHLP, OEMHLP_GETMEMINFOEX, 0, 0, 0,
                          &xmi, psize, &psize);
      if (rc || psize!=sizeof(xmi)) {
         printf("No extended memory information!\n");
      } else
      if (mi.HighMemKB>>2 != xmi.AvailPages) {
         printf("Extended memory information is incorrect!\n");
      } else {
         printf("Memory below 4Gb : %7u kb (%u pages)\n", xmi.LoPages<<2, xmi.LoPages);
         printf("Memory above 4Gb : %7u mb (%u pages)\n", xmi.HiPages>>8, xmi.HiPages);
      }
   }

   for (ii=OEMHLP_TAB_ACPI; ii<=OEMHLP_TAB_MP; ii++) {
      OEMHLP_BIOSTABLE  bt;
      ULONG          asize = 2;
      static const char *title[] = {"ACPI root", "SMBIOS", "SMBIOS v.3",
         "Legacy DMI", "PNP BIOS", "BIOS32", "PCI Int.Routing", "Intel MP"};

      psize = sizeof(bt);
      rc    = DosDevIOCtl(drvh, IOCTL_OEMHLP, OEMHLP_GETBIOSTABLE, &ii, asize,
                          &asize, &bt, psize, &psize);

      if (!rc) printf("%-16s : %08X %5u bytes\n", title[ii], bt.Addr, bt.Length);
   }

   DosClose(drvh);
}

