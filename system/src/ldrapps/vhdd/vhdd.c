//
// QSINIT
// virtual disk management module
//
#include "vhdd.h"
#include "qsmod.h"
#include "qstask.h"
#include "qsdm.h"

// interface array for every possible disk number
qs_emudisk mounted[QDSK_DISKMASK+1];
// disk info static buffer
static disk_geo_data di_info;
static char         di_fpath[_MAX_PATH+1];
static u32t         di_total, di_used;
static qshandle         cmux = 0;
u32t                 cid_ext = 0,
                    cid_vhdd = 0,
                    cid_vhdf = 0;
/* list of class id-s to create/open (in order of mount) */
static u32t*      cid_list[] = { &cid_vhdd, &cid_vhdf, 0 };

u32t _std diskio_read (u32t disk, u64t sector, u32t count, void *data) {
   disk &= ~(QDSK_DIRECT|QDSK_IAMCACHE|QDSK_IGNACCESS);
   if (disk>QDSK_DISKMASK) return 0;
   if (!mounted[disk]) return 0;
   return mounted[disk]->read(sector, count, data);
}

u32t _std diskio_write(u32t disk, u64t sector, u32t count, void *data) {
   disk &= ~(QDSK_DIRECT|QDSK_IAMCACHE|QDSK_IGNACCESS);
   if (disk>QDSK_DISKMASK) return 0;
   if (!mounted[disk]) return 0;
   return mounted[disk]->write(sector, count, data);
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

static qserr _exicc de_getgeo(EXI_DATA, disk_geo_data *geo) {
   instance_ret(de_data, de, E_SYS_INVOBJECT);
   if (!geo) return E_SYS_ZEROPTR;
   if (!de->self) return E_SYS_NONINITOBJ;
   // call parent, it shoud be safe in its mutex
   return de->self->query(geo, 0, 0, 0);
}

static u32t _exicc de_setgeo(EXI_DATA, disk_geo_data *geo) {
   instance_ret(de_data, de, E_SYS_INVOBJECT);
   if (!geo) return E_SYS_ZEROPTR;
   if (!de->self) return E_SYS_NONINITOBJ;
   // call parent, it shoud be safe in its mutex
   return de->self->setgeo(geo);
}

static char* _exicc de_getname(EXI_DATA) {
   char *rc, fname[QS_MAXPATH+1];
   instance_ret(de_data, de, 0);
   if (!de->self) return 0;
   if (de->self->query(0, fname, 0, 0)) return 0;
   rc = strdup("VHDD disk. File ");
   rc = strcat_dyn(rc, fname);
   return rc;
}

static u32t _exicc de_state(EXI_DATA, u32t state) { return EDSTATE_NOCACHE; }

static void *qs_de_list[] = { de_getgeo, de_setgeo, de_getname, de_state };

static void _std de_init(void *instance, void *data) {
   de_data *de = (de_data*)data;
   de->sign    = VHDD_SIGN;
}

static void _std de_done(void *instance, void *data) {
   instance_void(de_data,de);
   memset(de, 0, sizeof(de_data));
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

// grab vhdd command execution mutex
static void shell_lock(void) { if (cmux) mt_muxcapture(cmux); }
// release vhdd command mutex
static void shell_unlock(void) { if (cmux) mt_muxrelease(cmux); }

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

static void print_diskinfo(void) {
   printf("Size: %s (%LX sectors, %d bytes per sector)\n",
      dsk_formatsize(di_info.SectorSize, di_info.TotalSectors, 0, 0),
         di_info.TotalSectors, di_info.SectorSize);
   printf("File: %s (%d sectors total, %d used (%2d%%))\n", di_fpath,
      di_total, di_used, di_used*100/di_total);
}

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
         || m_and_m) && args->count>=3 && args->count<=5)
      {
         u64t  sectors = 0;
         u32t     acnt = args->count,
                sector = 0,
                   cid = cid_vhdd;

         if (acnt>3) {
            sector = str2long(args->item[acnt>4?4:3]);
            if (sector==512 || sector==1024 || sector==2048 || sector==4096) acnt--;
               else sector = 0;
         }
         if (!sector) sector = 512;
         /* just put format key as lowercased subst into "qs_???disk" string 
            and then use it as a class name. But check that it is really listed
            in cid_list[] first */
         if (acnt==4) {
            char cname[32];
            u32t    cidalt;
            snprintf(cname, 32, "qs_%sdisk", strlwr(args->item[3]));

            cidalt = exi_queryid(cname);
            if (!cidalt) {
               printf("Unknown format string (%s)\n", args->item[3]);
               showerr = 0; sector = 0; // force error
            } else {
               u32t **plcid = cid_list;
               while (*plcid)
                  if (**plcid++==cidalt) { cid = cidalt; break; }
               if (cid!=cidalt) {
                  printf("Format class is unknown (%s)\n", cname);
                  showerr = 0; sector = 0; // force error
               }
            }
         }

         if (sector)
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
            qs_emudisk dsk = (qs_emudisk)exi_createid(cid, EXIF_SHARED);
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
         u32t   **plcid = cid_list;
         qs_emudisk dsk = 0;
         /* just try it as VHDD first, VHD then, etc */
         while (*plcid) {
            if (dsk) DELETE(dsk);
            dsk = (qs_emudisk)exi_createid(**plcid, EXIF_SHARED);
            if (dsk) rc = dsk->open(args->item[1]); else rc = E_SYS_BADFMT;
            if (!rc) break; else plcid++;
         }
         if (rc==0) mount_action(dsk, args->item[1]); else {
            if (rc==E_SYS_BADFMT) {
               printf("File \"%s\" format is unknown!\n", args->item[1]);
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
static void _std on_mtmode(sys_eventinfo *info) {
   if (mt_muxcreate(0, "__vhdd_mux__", &cmux) || io_setstate(cmux, IOFS_DETACHED, 1))
      log_it(0, "mutex init error!\n");
}

static int release_all(void) {
   u32t ii;
   // call delete, it calls umount & close disk file for every mounted disk
   for (ii=0; ii<=QDSK_DISKMASK; ii++)
      if (mounted[ii]) exi_free(mounted[ii]);

   memset(mounted, 0, sizeof(mounted));

   if (cid_vhdd)
      if (exi_unregister(cid_vhdd)) cid_vhdd = 0;
   if (cid_vhdf)
      if (exi_unregister(cid_vhdf)) cid_vhdf = 0;
   // if one of main classes still exists, then ext will deny unreg too
   if (cid_ext && !cid_vhdd && !cid_vhdf)
      if (exi_unregister(cid_ext)) cid_ext = 0;

   return !cid_ext && !cid_vhdd && !cid_vhdf;
}

// shutdown handler - delete all disks
static void _std on_exit(sys_eventinfo *info) {
   release_all();
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      if (mod_query(selfname, MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      memset(mounted, 0, sizeof(mounted));
      /* register all */
      cid_vhdd = init_rwdisk();
      cid_vhdf = init_vhdisk();
      cid_ext  = exi_register(VHDD_VHDD "_ext", qs_de_list,
                    sizeof(qs_de_list)/sizeof(void*), sizeof(de_data), 0,
                       de_init, de_done, 0);
      log_it(3, "vhdd cid: %u vhd=%u ext=%u\n", cid_vhdd, cid_vhdf, cid_ext);

      if (!cid_vhdd || !cid_vhdf || !cid_ext) {
         log_printf("vhdd class reg error!\n");
         release_all();
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
      if (!release_all()) return 0;
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

