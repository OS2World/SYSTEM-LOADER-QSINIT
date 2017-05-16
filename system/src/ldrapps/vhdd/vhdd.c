//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
//
#include "stdlib.h"
#include "qsbase.h"
#include "qsmod.h"
#include "qstask.h"
#include "qcl/qsvdimg.h"
#include "qsdm.h"

int  init_rwdisk(void);
int  done_rwdisk(void);

extern qs_emudisk    mounted[];
// disk info static buffer
static disk_geo_data di_info;
static char         di_fpath[_MAX_PATH+1];
static u32t         di_total, di_used;
static qshandle         cmux = 0;

static void print_diskinfo(void) {
   printf("Size: %s (%LX sectors, %d bytes per sector)\n",
      dsk_formatsize(di_info.SectorSize, di_info.TotalSectors, 0, 0),
         di_info.TotalSectors, di_info.SectorSize);
   printf("File: %s (%d sectors total, %d used (%2d%%))\n", di_fpath,
      di_total, di_used, di_used*100/di_total);
}

// grab vhdd command execution mutex
static void shell_lock(void) { if (cmux) mt_muxcapture(cmux); }
// release vhdd command mutex
static void shell_unlock(void) { if (cmux) mt_muxrelease(cmux); }

static void mount_action(qs_emudisk dsk, char *prnname) {
   s32t  disk;
   shell_lock();
   disk = dsk->mount();
   if (disk<0) {
      printf("Error mouting disk!\n");
      DELETE(dsk);
   } else {
      printf("File \"%s\" mounted as disk %s\n", prnname, dsk_disktostr(disk,0));
      if (dsk->query(&di_info, di_fpath, &di_total, &di_used)==0) print_diskinfo();
   }
   shell_unlock();
}

u32t _std shl_vhdd(const char *cmd, str_list *args) {
   static char fpath[_MAX_PATH+1];
   qserr          rc = E_SYS_INVPARM;
   int       showerr = 1;

   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   if (args->count>0) {
      int  m_and_m;
      strupr(args->item[0]);
      // make and mount
      m_and_m = strcmp(args->item[0],"MM")==0;

      if ((strcmp(args->item[0],"CREATE")==0 || strcmp(args->item[0],"MAKE")==0
         || m_and_m) && args->count>=3)
      {
         u64t  sectors = 0;
         u32t   sector = args->count>3?str2long(args->item[3]):512;

         if (args->item[2][0]=='#') {
            char *endptr = 0;
            sectors = strtoull(args->item[2]+1, &endptr, 0);
            // force EINVAL error if garbage at the end
            if (*endptr) sectors = 0;
         } else {
            u32t size = str2long(args->item[2]);
            sectors   = (u64t)size * _1GB / sector;
         }

         if (sector && sectors) {
            qs_emudisk dsk = NEW_G(qs_emudisk);
            rc = dsk->make(args->item[1], sector, sectors);
            switch (rc) {
               case E_SYS_TOOLARGE:
                  printf("Disk too large!\n"); showerr = 0;
                  break;
               case E_SYS_EXIST:
                  printf("File \"%s\" already exists!\n", args->item[1]);
                  showerr = 0;
                  break;
               case 0:
                  if (!m_and_m) printf("File ready!\n");
                  break;
            }
            // it it make & mount - do it, else release
            if (!rc && m_and_m) mount_action(dsk, args->item[1]);
               else DELETE(dsk);
         }
      } else
      if (strcmp(args->item[0],"MOUNT")==0 && args->count==2) {
         qs_emudisk dsk = NEW_G(qs_emudisk);
         rc = dsk->open(args->item[1]);
         // compact it on mount
         //if (rc==0) dsk->compact(1);
         if (rc==0) mount_action(dsk, args->item[1]); else {
            if (rc==E_DSK_UNKFS) {
               printf("File \"%s\" is not an disk image!\n", args->item[1]);
               showerr = 0;
            }
            DELETE(dsk);
         }
      } else
      if (strcmp(args->item[0],"LIST")==0 && args->count==1) {
         u32t ii, any;
         shell_lock();
         for (ii=0, any=0; ii<=QDSK_DISKMASK; ii++)
            if (mounted[ii])
               if (mounted[ii]->query(&di_info, di_fpath, 0, 0)==0) {
                  printf("%s: %8s  %s\n", dsk_disktostr(ii,0), dsk_formatsize(di_info.SectorSize,
                     di_info.TotalSectors,0,0), di_fpath);
                  any++;
               }
         shell_unlock();
         if (!any) printf("There is no VHDD disks mounted.\n");
         rc = 0;
      } else
      if ((strcmp(args->item[0],"INFO")==0 || strcmp(args->item[0],"UMOUNT")==0
         || strcmp(args->item[0],"TRACE")==0) && args->count==2)
      {
         u32t  disk = dsk_strtodisk(args->item[1]);
         if (disk==FFFF) rc = E_DSK_NOTMOUNTED; else {
            rc = 0;
            // lock it for mounted[] array safeness & also allows static vars usage
            shell_lock();

            if (disk>=QDSK_FLOPPY) printf("Invalid disk type!\n"); else
            if (!mounted[disk]) printf("Not a VHDD disk!\n"); else

            // "TRACE disk" command (enable internal bitmaps tracing)
            if (args->item[0][0]=='T') {
               mounted[disk]->trace(-1,1);
            } else
            if (args->item[0][0]=='I') {
               rc = mounted[disk]->query(&di_info, di_fpath, &di_total, &di_used);
               if (rc==0) print_diskinfo();
            } else
            if (args->item[0][0]=='U') {
               qs_emudisk  dinst = mounted[disk];
               // umount will zero mounted[disk] value, so save it and free after
               rc = mounted[disk]->umount();
               if (!rc) printf("Disk %s unmounted!\n", dsk_disktostr(disk,0));
               DELETE(dinst);
            }
            shell_unlock();
         }
      }
   }

   if (rc) {
      if (showerr) cmd_shellerr(EMSG_QS,rc,"VHDD: ");
      rc = qserr2errno(rc);
   }
   return rc;
}

const char *selfname = "VHDD",
            *cmdname = "VHDD";

// mutes creation
void _std on_mtmode(sys_eventinfo *info) {
   if (mt_muxcreate(0, "__vhdd_mux__", &cmux) || io_setstate(cmux, IOFS_DETACHED, 1))
      log_it(0, "mutex init error!\n");
}

// shutdown handler - delete all disks
void _std on_exit(sys_eventinfo *info) {
   done_rwdisk();
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
      sys_notifyevent(SECB_QSEXIT|SECB_GLOBAL, on_exit);
      // catch MT mode
      if (!mt_active()) sys_notifyevent(SECB_MTMODE|SECB_GLOBAL, on_mtmode);
         else on_mtmode(0);
      // add shell command
      cmd_shelladd(cmdname, shl_vhdd);
      log_printf("%s is loaded!\n",selfname);
   } else {
      // DENY unload if class was not unregistered
      if (!done_rwdisk()) return 0;
      // mutex should be free, because no more disk instances
      if (!cmux) sys_notifyevent(0, on_mtmode); else
         if (mt_closehandle(cmux)) log_it(2, "mutex fini error!\n");
      // remove shutdown handler
      sys_notifyevent(0, on_exit);
      // remove shell command
      cmd_shellrmv(cmdname, shl_vhdd);
      log_printf("%s unloaded!\n",selfname);
   }
   return 1;
}

