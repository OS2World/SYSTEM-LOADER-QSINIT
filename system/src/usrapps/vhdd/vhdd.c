//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
//
#include "stdlib.h"
#include "qsutil.h"
#include "qsshell.h"
#include "qsmod.h"
#include "qslog.h"
#include "errno.h"
#include "vio.h"
#include "rwdisk.h"
#include "qsdm.h"

static const char *help_text = "Creates dinamically expanded virtual HDD:^^"
   "VHDD MAKE filename size [sector]^"
   "VHDD MOUNT filename^"
   "VHDD INFO diskname^"
   "VHDD UMOUNT diskname^"
   "\xdd\tMAKE\t\tcreate new disk image^"
   "\xdd\tMOUNT\t\tmount disk image^"
   "\xdd\tINFO\t\tshows info about disk image^"
   "\xdd\tUMOUNT\t\tumount disk image^"
   "\xdd\tsize\t\tdisk size in gigabytes^"
   "\xdd\tfilename\tname of image file on mounted partition to create/use^"
   "\xdd\tsector\t\tsector size (512, 1024, 2048 or 4096, default is 512)^"
   "\xdd\tdiskname\tQSINIT disk name";

int  init_rwdisk(void);
void done_rwdisk(void);

extern emudisk mounted[];
// disk info static buffer
static disk_geo_data di_info;
static char          di_fpath[_MAX_PATH+1];
static u32t          di_total, di_used;


static void print_diskinfo(void) {
   printf("Size: %s (%LX sectors, %d bytes per sector)\n",
      dsk_formatsize(di_info.SectorSize, di_info.TotalSectors, 0, 0), 
         di_info.TotalSectors, di_info.SectorSize);
   printf("File: %s (%d sectors total, %d used (%2d%%))\n", di_fpath,
      di_total, di_used, di_used*100/di_total);
}

u32t _std shl_vhdd(const char *cmd, str_list *args) {
   static char fpath[_MAX_PATH+1];
   int rc = EINVAL;

   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   if (args->count>0) {
      strupr(args->item[0]);

      if ((strcmp(args->item[0],"CREATE")==0 || strcmp(args->item[0],"MAKE")==0)
         && args->count>=3) 
      {
         u32t    size = str2long(args->item[2]),
               sector = args->count>3?str2long(args->item[3]):512;
         u64t sectors = (u64t)size * _1GB / sector;

         if (size && sector) {
            emudisk dsk = NEW(emudisk);
            rc = dsk->make(args->item[1], sector, sectors);
            switch (rc) {
               case EFBIG :printf("Disk too large!\n"); rc=0; 
                           break;
               case EEXIST:printf("File \"%s\" already exists!\n", args->item[1]); rc=0; 
                           break;
               case 0     :printf("File ready!\n");
                           break;
            }
            DELETE(dsk);
         }
      } else
      if (strcmp(args->item[0],"MOUNT")==0 && args->count==2) {
         emudisk dsk = NEW(emudisk);
         rc = dsk->open(args->item[1]);
         // compact it on mount
         if (rc==0) dsk->compact(1);
         if (rc==0) {
            s32t disk = dsk->mount();
            if (disk<0) {
               printf("Error mouting disk!\n"); 
               DELETE(dsk);
            } else {
               printf("File \"%s\" mounted as disk %s\n", args->item[1], dsk_disktostr(disk,0));
               if (dsk->query(&di_info, di_fpath, &di_total, &di_used)==0) print_diskinfo();
            }
         } else {
            if (rc==EBADF) {
               printf("File \"%s\" is not an disk image!\n", args->item[1]);
               rc=0;
            }
            DELETE(dsk);
         }
      } else
      if ((strcmp(args->item[0],"INFO")==0 || strcmp(args->item[0],"UMOUNT")==0)
         && args->count==2) 
      {
         u32t  disk = dsk_strtodisk(args->item[1]);
         if (disk==FFFF) rc = ENODEV; else {
            rc = 0;
            if (disk>=QDSK_FLOPPY) printf("Invalid disk type!\n"); else
            if (!mounted[disk]) printf("Not a VHDD disk!\n"); else
            if (args->item[0][0]=='I') {
               rc = mounted[disk]->query(&di_info, di_fpath, &di_total, &di_used);
               if (rc==0) print_diskinfo();
            } else
            if (args->item[0][0]=='U') {
               emudisk dinst = mounted[disk];
               // umount will zero mounted[disk] value, so save it and free after
               rc = mounted[disk]->umount();
               if (!rc) printf("Disk %s unmounted!\n", dsk_disktostr(disk,0));
               DELETE(dinst);
            }
         }
      }
   }

   if (rc) cmd_shellerr(rc,"VHDD: ");
   return rc;
}

int        lib_ready = 0; // lib is ready
const char *selfname = "VHDD",
            *cmdname = "VHDD";

void on_exit(void) {
   if (!lib_ready) return;
   cmd_shellrmv(cmdname, shl_vhdd);
   done_rwdisk();
   lib_ready = 0;
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      if (mod_query(selfname, MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      if (!init_rwdisk()) {
         log_printf("unable to register class!\n");
         return 0;
      }
      // install shutdown handler
      exit_handler(&on_exit,1);
      // add shell command
      cmd_shelladd(cmdname, shl_vhdd);
      // add shell help message
      cmd_shellsetmsg(cmdname, help_text);
      lib_ready = 1;
      log_printf("%s is loaded!\n",selfname);
   } else {
      exit_handler(&on_exit,0);
      on_exit();
      log_printf("%s unloaded!\n",selfname);
   }
   return 1;
}
