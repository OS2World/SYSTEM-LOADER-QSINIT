#include "sp_defs.h"
#include "volio.h"
#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <stdarg.h>
#include "parttab.h"
#include "bsdata.h"
#include "classes.hpp"

#define MAX_SECTOR_SIZE 4096

static volh volume = 0;

void error(const char *fmt, ...) {
   va_list arglist;
   va_start(arglist,fmt);
   vprintf(fmt,arglist);
   va_end(arglist);

   if (volume) volume_close(volume);

   exit(2);
}

void *findstr(void *data, u32 dlen, const char *str) {
   int  len = strlen(str);
   char *cp = (char*)data;
   if (!len) return 0;
   while (dlen>=len) {
      if (*cp==*str)
         if (memcmp(cp,str,len)==0) return cp;
      cp++; dlen--;
   }
   return 0;
}

static void about(void) {
   printf(" bootset: install QSINIT boot sector to FAT/FAT32 volume\n\n"
          " bootset [options] drive: [options]\n"
          " options:\n"
          "   -w          write sector (by default app only print BPB)\n"
          "   -y          skip confirmation prompt\n"
          "   -q          be quiet\n"
          "   -bf=str     boot file name (QSINIT by default)\n");
   exit(1);
}

int main(int argc, char *argv[]) {
   if (argc<2) about();
   TStrings args;
   spstr    bootfile;
   int ii, write = 0, noask = 0, quiet = 0;
   for (ii=1; ii<argc; ii++) args.Add(argv[ii]);

   ii=args.IndexOfName("-w");
   if (ii>=0) { write=1; args.Delete(ii); }
   ii=args.IndexOfName("-q");
   if (ii>=0) { quiet=1; args.Delete(ii); }
   ii=args.IndexOfName("-y");
   if (ii>=0) { noask=1; args.Delete(ii); }
   ii=args.IndexOfName("-bf");
   if (ii>=0) {
      bootfile = args.Value(ii).trim().upper();
      if (bootfile.length()>11) {
      printf("Invalid volume name: %s\n", args[0]());
      }
      args.Delete(ii);
   }
   if (args.Count()<1) about();

   char dl = toupper(args[0][0]);
   if (!isalpha(dl) || args[0][1]!=':' || args[0].length()>2) {
      printf("Invalid volume name: %s\n", args[0]());
      return 1;
   }

   if (!volume_open(dl,&volume))
      error("Unable to open volume %c: for writing!\n", dl);
   else {
      u8  s0data[MAX_SECTOR_SIZE];
      u32 vs = volume_size (volume, 0);
      if (!vs) error("Unable to access volume %c: data!\n", dl);
      if (!quiet) printf("Volume %c: size: %u sectors\n", dl, vs);

      memset(s0data, 0, MAX_SECTOR_SIZE);

      if (!volume_read(volume,0,1,s0data))
         error("Unable to read sector 0 on volume %c:!\n", dl);
      else
      if (memchrnb(s0data,0,MAX_SECTOR_SIZE)==0) {
         error("Sector 0 on volume %c: is empty!\n", dl);
      } else {
         Boot_Record     &br   = *(Boot_Record*)   &s0data;
         Boot_RecordF32  &br32 = *(Boot_RecordF32*)&s0data;
         int           IsFAT32 = False;
         char           fstype[9];

         if (br.BR_BPB.BPB_RootEntries==0 && br.BR_BPB.BPB_FATCopies) {
            IsFAT32 = True;
            strncpy(fstype,(char*)&br32.BR_F32_EBPB.EBPB_FSType,8);
         } else {
            strncpy(fstype,(char*)&br.BR_EBPB.EBPB_FSType,8);
         }
         fstype[8] = 0;
         if (!quiet) {
            printf("Volume %c: type: %s\n", dl, fstype);

            printf("Volume %c: BPB:\n", dl);
            printf("    Bytes Per Sector      %5d     ", br.BR_BPB.BPB_BytePerSect );
            printf("    Sect Per Cluster      %5d\n"   , br.BR_BPB.BPB_SecPerClus  );
            printf("    Res Sectors           %5d     ", br.BR_BPB.BPB_ResSectors  );
            printf("    FAT Copies            %5d\n"   , br.BR_BPB.BPB_FATCopies   );
            printf("    Root Entries          %5d     ", br.BR_BPB.BPB_RootEntries );
            printf("    Total Sectors         %5d\n"   , br.BR_BPB.BPB_TotalSec    );
            printf("    Media Byte               %02X     ",br.BR_BPB.BPB_MediaByte);
            printf("    Sect Per FAT          %5d\n"   , br.BR_BPB.BPB_SecPerFAT   );
            printf("    Sect Per Track        %5d     ", br.BR_BPB.BPB_SecPerTrack );
            printf("    Heads                 %5d\n"   , br.BR_BPB.BPB_Heads       );
            printf("    Hidden Sectors     %08X     "  , br.BR_BPB.BPB_HiddenSec   );
            printf("    Total Sectors (L)  %08X\n"     , br.BR_BPB.BPB_TotalSecBig );
         }

         u16 bps = br.BR_BPB.BPB_BytePerSect;
         // byte per sector valid?
         if (bps!=512&&bps!=1024&&bps!=2048&&bps!=4096)
            error("Invalid BPB, unable to process!\n");
         if (br.BR_BPB.BPB_FATCopies==0)
            error("Not a FAT partition type!\n");

         if (IsFAT32) {
            struct Boot_RecordF32 *sbtd = (struct Boot_RecordF32 *)&bsdata[512];
            memcpy(&br32,sbtd,11);
            memcpy(&br32+1,sbtd+1,512-sizeof(struct Boot_RecordF32));
         } else {
            struct Boot_Record *sbtw = (struct Boot_Record *)&bsdata;
            memcpy(&br,sbtw,11);
            memcpy(&br+1,sbtw+1,512-sizeof(struct Boot_Record));
         }
         if (bootfile.length()) {
            void *ps = findstr(&br,512,"QSINIT     ");
            if (!ps) error("Unable to locate position for boot file name!\n");
            memcpy(ps,bootfile(),bootfile.length());
         }

         if (write && !noask) {
            char str[20], *lp;
            str[0]=16; str[1]=0;
            printf("Type YES here to write boot sector: "); fflush(stdout);
            lp = cgets(str);
            lp[str[1]]=0;
            printf("\n");
            if (stricmp(lp,"YES")) {
               printf("Cancelled.\n");
               write = 0;
            }
         }
         if (write) {
            if (!volume_write(volume,0,1,s0data))
               error("Unable to write sector 0 on volume %c:!\n", dl);
            printf("Bootsector written.\n");
         }
      }
      volume_close(volume);
   }
   return 0;
}
