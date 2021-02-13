//
// QSINIT
// partition management dll
//
#include "stdlib.h"
#include "qsdm.h"
#include "errno.h"
#include "vioext.h"
#include "pscan.h"
#include "qsint.h"
#include "qsio.h"
#include "direct.h"
#include "qcl/qsextdlg.h"

// is key present? (with argument or not)
#define key_present(kparm,key) str_findkey(kparm,key,0)

long get_disk_number(char *str) {
   u32t rc = dsk_strtodisk(str);
   return rc==FFFF?-EINVAL:rc;
}

int get_arg_index(char *arg, char **list) {
   int rc = 0;
   while (list[rc])
      if (stricmp(arg,list[rc])==0) return rc; else rc++;
   return -1;
}

// abort entering on -1 if ESC was pressed
static int _std esc_cb(u16t key, key_strcbinfo *inputdata) {
   char cch = toupper(key&0xFF);
   if (cch==27) return -1;
   return 0;
}

static int ask_yes(u8t color) {
   char *str;
   vio_setcolor(color?color:VIO_COLOR_LRED);
   vio_strout("Type \"YES\" here to continue: ");
   vio_setcolor(VIO_COLOR_RED);
   str = key_getstr(esc_cb);
   vio_setcolor(VIO_COLOR_RESET);
   vio_charout('\n');
   if (str) {
      int rc = strcmp(strupr(str),"YES")==0 ? 1 : 0;
      free(str);
      return rc;
   }
   return 0;
}

char *make_errmsg(qserr rc, const char *altmsg) {
   char *res = cmd_shellerrmsg(EMSG_QS, rc);
   if (!res)
      res = sprintf_dyn("%s, code = %d", altmsg, rc);
   return res;
}

void error_msgbox(qserr rc, const char *header) {
   char *err = cmd_shellerrmsg(EMSG_QS, rc);
   if (!err) err = sprintf_dyn("Error code %X", rc);
   vio_msgbox(header, err, MSG_LIGHTRED|MSG_OK, 0);
   free(err);
}

/// "mount" command
u32t _std shl_mount(const char *cmd, str_list *args) {
   int rc = -1;
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count>0) {
      static char *argstr   = "select|list|/v|/ro|/raw";
      static short argval[] = {     1,   1, 1,  1,   1};
      int       select = 0, list = 0, verbose = 0, ro = 0, raw = 0;
      u32t         ii, vt[DISK_COUNT];
      disk_volume_data vi[DISK_COUNT];
      for (ii=0; ii<DISK_COUNT; ii++) vt[ii] = hlp_volinfo(ii, vi+ii);

      args = str_parseargs(args, 0, SPA_RETLIST, argstr, argval, &select, &list,
                           &verbose, &ro, &raw);

      if (args->count>=1 && stricmp(args->item[0],"QUERY")==0) {
         if ((args->count==3 || args->count==4) && !ro && !raw) {
            char  prnname[8];
            u32t     disk = get_disk_number(args->item[2]), vol;
            long      idx = args->count>3?atoi(args->item[3]):-1;

            if ((long)disk<0 || (disk&QDSK_VOLUME)!=0) {
               printf("Invalid disk name specified!\n");
               rc = EINVAL;
            } else
            if (args->count>3 && (!isdigit(args->item[3][0]) || idx>=dsk_partcnt(disk))) {
               printf("Invalid partition index!\n");
               rc = EINVAL;
            } else {
               vol = dsk_ismounted(disk, idx);
               if (vol) dsk_disktostr(vol,prnname);
               setenv(args->item[1], vol?prnname:0, 1);
               rc = 0;
               if (verbose) {
                  printf("Disk %s", dsk_disktostr(disk,0));
                  if (idx>=0) printf(", partition %d ", idx);
                  if (!vol) printf("is not mounted\n"); else
                     printf("mounted as volume %s\n", prnname);
               }
            }
            // delete env. var in error case
            if (rc) setenv(args->item[1], 0, 1);
         }
      } else
      if (list && !ro && !raw && !select) {
         static char *fattype[5] = { "unknown", "FAT12", "FAT16", "FAT32", "exFAT" };
         cmd_printseq(0,1,0);
         rc = 0;

         for (ii=0; ii<DISK_COUNT; ii++)
            if (vi[ii].TotalSectors) {
               char  str[128], lvmi[32], dname[12];
               u32t  dsk = FFFF;
               long  idx = vol_index(ii,&dsk);
               int   vfs = vi[ii].InfoFlags&VIF_VFS?1:0;
               str [0] = 0;
               lvmi[0] = 0;

               if (idx>=0) {
                  int   lvm = lvm_checkinfo(dsk);
                  // LVM is ok or, at least, without fatal errors
                  if (!lvm || lvm==E_LVM_NOBLOCK || lvm==E_LVM_NOPART) {
                     lvm_partition_data pti;
                     if (lvm_partinfo(dsk, idx, &pti))
                        if (pti.Letter) sprintf(lvmi, "(LVM:%c)", pti.Letter);
                  }
               }
               if (vfs) dname[0] = 0; else dsk_disktostr(dsk, dname);
               if (verbose) {
                  char typestr[48];
                  if (vi[ii].InfoFlags&VIF_VFS) snprintf(str, 128,
                     "(%08X)       virtual fs", vi[ii].TotalSectors);
                  else {
                     if (!vi[ii].StartSector) strcpy(typestr, "floppy media"); else
                        if (idx>=0) snprintf(typestr, 48, "partition %d %s", idx, lvmi);
                           else snprintf(typestr, 48, "partition not matched (%09LX)",
                              vi[ii].StartSector);
                     snprintf(str, 128, "(%08X)  %-4s %s", vi[ii].TotalSectors, dname, typestr);
                  }
               } else {
                  if (idx>=0) sprintf(str," %s.%d  %s", dname, idx, lvmi); else
                     if (ii!=DISK_LDR && dname[0]) sprintf(str," %s  %s", dname, lvmi);
               }
               if (shellprn(" %c:/%c: %8s  %s %s %s", '0'+ii, 'A'+ii, 
                  vt[ii]<=FST_EXFAT?fattype[vt[ii]]:vi[ii].FsName,
                     get_sizestr(vi[ii].SectorSize, vi[ii].TotalSectors), 
                        vi[ii].InfoFlags&VIF_NOWRITE?"r/o":"   ", str))
                           { rc = EINTR; break; }
            }
      } else
      if (select && (args->count==0 || args->count==1 && stricmp(args->item[0],"ALL")==0)) {
         qserr      res = 0;
         u32t   freevol = 0;
         char    *vlist = 0;
         u32t      type = BTDT_ALL|BTDT_NOQSM;
         // no "ALL" arg, filter by known FS types
         if (args->count==0)
            type |= BTDT_FFAT|BTDT_FFAT32|BTDT_FexFAT|BTDT_FHPFS|BTDT_FJFS;

         for (ii=2; ii<DISK_COUNT; ii++)
            if (!vi[ii].TotalSectors) {
               char ltr[8];
               freevol++;
               sprintf(ltr, " %c:", ii+'A');
               vlist = strcat_dyn(vlist, ltr);
            }
         if (!freevol)
            vio_msgbox("Mount", "There is no free volume letter", MSG_OK|MSG_RED, 0);
         else {
            qs_extcmd  dlg = NEW(qs_extcmd);
            // no extcmd.dll?
            if (!dlg) res = E_SYS_UNSUPPORTED; else {
               u32t  rdsk = FFFF;
               long  ridx = -1;
               res = dlg->bootmgrdlg("Mount", 0, type, 0, &rdsk, &ridx);
               DELETE(dlg);
               if (!res) {
                  str_list   *lst = str_split(vlist, " ");
                  vio_listref *rf = malloc_th(sizeof(vio_listref));
                  u32t id;
                  rf->size  = sizeof(vio_listref);
                  rf->items = lst->count;
                  rf->text  = lst;
                  rf->id    = 0;
                  rf->subm  = 0;
                  rf->akey[0].key = 0;
                  
                  id = vio_showlist(0, rf, MSG_GRAY|MSG_POS_NW, 0);
                  free(rf);
                  if (id && id<=lst->count) {
                     u8t chr = lst->item[id-1][0]-'A';
                     res = vol_mount(&chr, rdsk, ridx, 0);
                  }
                  free(lst);
               } else
               if (res==E_SYS_UBREAK) res = 0;
            }
         }
         if (vlist) free(vlist);
         if (res) error_msgbox(res, "Mount");
         rc = 0;
      } else
      if (!list && !select && args->count>=2) {
         u32t   vol = get_disk_number(args->item[0]),
               disk = get_disk_number(args->item[1]);
         u64t start, size;
         long   idx = args->count>2?atoi(args->item[2]):-1,
                fdd = (disk&QDSK_FLOPPY)!=0;
         char prnname[8];
         dsk_disktostr(disk,prnname);

         if (args->count>2 && !isdigit(args->item[2][0])) {
            printf("Invalid partition index!\n");
            rc = EINVAL;
         } else
         if ((long)vol<0 || (vol&QDSK_VOLUME)==0) {
            printf("Invalid volume name specified!\n");
            rc = EINVAL;
         } else
         if (vol<=QDSK_VOLUME+DISK_LDR) {
            printf("Unable to re-mount A: and B: volumes!\n");
            rc = EINVAL;
         } else
         if ((long)disk<0 || (disk&QDSK_VOLUME)!=0) {
            printf("Invalid disk name specified!\n");
            rc = EINVAL;
         } else
         do {
            // check for LM_CACHE_ON_MOUNT and load cache if asked
            cache_envstart();
            // re-scan disk
            dsk_ptrescan(disk,1);
            // mount as a floppy
            if (idx<0) {
               u32t     st = dsk_sectortype(disk,0,0);
               int success = st==DSKST_BOOTFAT || st==DSKST_BOOTBPB || st==DSKST_BOOT
                  || st==DSKST_BOOTEXF || fdd && (st==DSKST_EMPTY || st==DSKST_DATA);

               if (!success) {
                   if (!fdd && (st==DSKST_EMPTY || st==DSKST_DATA)) {
                      vio_setcolor(VIO_COLOR_LRED);
                      printf("Do you really want to mount HARD DRIVE %s as big floppy?\n",
                         prnname);
                      if (ask_yes(VIO_COLOR_RESET)) success=1; else
                         { rc = EINTR; break; }
                   }
                   if (!success)
                      if (fdd && st==DSKST_ERROR) printf("No diskette in drive %s\n",
                         prnname);
                      else
                         printf("Disk %s is not floppy formatted!\n", prnname);
               }
               if (!success) { rc = ENOTBLK; break; }
               start = 0;
               size  = hlp_disksize64(disk,0);
            }
            // or hard disk partition
            if (idx>=0 && !dsk_ptquery64(disk,idx,&start,&size,0,0)) {
               printf("There is no partition %d in disk %s!\n", idx, prnname);
               rc = EINVAL;
            } else {
               qserr   merr;
               u32t  mflags = 0;
               for (ii=0; ii<DISK_COUNT; ii++)
                  if (vi[ii].StartSector && vi[ii].Disk==disk &&
                     vi[ii].StartSector==start)
                  {
                     printf("Already mounted as %c:\n", 'A'+ii);
                     rc = EBUSY; break;
                  }
               if (rc>0) break;
               if (raw) mflags|=IOM_RAW;
               if (ro) mflags|=IOM_READONLY;
               // mount
               merr = io_mount(vol&QDSK_DISKMASK, disk, start, size, mflags);
               if (!merr) {
                  printf("Mounted as %c:\n", 'A'+(vol&QDSK_DISKMASK));
                  rc = 0;
               } else {
                  cmd_shellerr(EMSG_QS, merr, "Failed to mount: ");
                  rc = qserr2errno(merr);
               }
            }
         } while (false);
      }
      free(args); args=0;
   }
   if (rc<0) cmd_shellerr(EMSG_CLIB,rc = EINVAL,0);
   return rc;
}

/// "umount" command
u32t _std shl_umount(const char *cmd, str_list *args) {
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count>0) {
      u32t ii, vt[DISK_COUNT];
      disk_volume_data vi[DISK_COUNT];
      for (ii=0; ii<DISK_COUNT; ii++) vt[ii] = hlp_volinfo(ii, vi+ii);

      if (stricmp(args->item[0],"select")==0) {
         qserr      res = 0;
         for (ii=2; ii<DISK_COUNT; ii++)
            if (vi[ii].TotalSectors) break;
         if (ii==DISK_COUNT)
            vio_msgbox("Unmount", "Nothing to unmount", MSG_OK|MSG_RED, 0);
         else {
            qs_extcmd  dlg = NEW(qs_extcmd);
            // no extcmd.dll?
            if (!dlg) res = E_SYS_UNSUPPORTED; else {
               u32t  rdsk = FFFF;
               long  ridx = -1;
               res = dlg->bootmgrdlg("Unmount", 0, BTDT_QSDRIVE|BTDT_ALL|
                                     BTDT_QSM|BTDT_FLTQS, FFFF&~3, &rdsk, &ridx);
               DELETE(dlg);
               if (!res) {
                  u32t dsk = dsk_ismounted(rdsk, ridx);
                  if (dsk) res = io_unmount(dsk&~QDSK_VOLUME,0);
               } else
               if (res==E_SYS_UBREAK) res = 0;
            }
         }
         if (res) error_msgbox(res, "Unmount");
         return 0;
      } else
      if (stricmp(args->item[0],"all")==0) {
         for (ii=DISK_LDR+1; ii<DISK_COUNT; ii++)
            if (vi[ii].TotalSectors) {
               if (hlp_unmountvol(ii)) printf("Volume %c: unmounted\n", 'A'+ii);
                  else printf("Failed to unmount %c: !\n", 'A'+ii);
            }
         if (io_curdisk()>DISK_LDR) io_setdisk(DISK_LDR);
         return 0;
      } else {
         u32t rc = 0;
         ii = 0;
         while (ii<args->count) {
            u32t  vol = get_disk_number(args->item[ii]);
            if ((long)vol<0 || (vol&QDSK_VOLUME)==0) {
               printf("Invalid volume name specified!\n");
               return EINVAL;
            } else {
               vol&=~QDSK_VOLUME;
               // current process only
               if (vol==io_curdisk()) io_setdisk(DISK_LDR);

               if (hlp_unmountvol(vol)) {
                  printf("Volume %c: unmounted\n", 'A'+vol);
               } else {
                  printf("Failed to unmount %c:\n", 'A'+vol);
                  rc = ENOMNT;
               }
            }
            ii++;
         }
         return rc;
      }

   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
   return EINVAL;
}


u32t _std shl_dm_mbr(const char *cmd, str_list *args, u32t disk, u32t pos) {
   // no subcommand?
   if (args->count<=pos) return EINVAL;
   // check disk id
   if ((disk&QDSK_VOLUME)!=0) {
      printf("This is volume, not a physical disk! \n");
      return EINVAL;
   } else {
      char             prnname[8], sizestr[24];
      disk_geo_data      gdata;
      static char *subcommands[6] = {"WIPE", "CODE", "BOOT", "ACTIVE", "BOOTFILE", 0};
      int subcmd = get_arg_index(args->item[pos],subcommands);

      if (subcmd<0) return EINVAL;
      dsk_disktostr(disk,prnname);
      // allow boot from floppy
      if ((disk&QDSK_FLOPPY) && subcmd!=2 && subcmd!=4) {
         printf("This is a floppy disk! \n");
         return EINVAL;
      }
      if (subcmd<=1) {
         hlp_disksize(disk, 0, &gdata);
         if (gdata.TotalSectors)
            dsk_formatsize(gdata.SectorSize, gdata.TotalSectors, 0, sizestr);
      }
      if (subcmd==0) { // "WIPE"
         vio_setcolor(VIO_COLOR_LRED);
         printf("Do you really want to WIPE PARTITION TABLE on disk %s (%s)?\n",
            prnname, sizestr);
         vio_setcolor(VIO_COLOR_RESET);
         if (ask_yes(0)) {
            vio_setcolor(VIO_COLOR_LWHITE);
            printf("And again, please. Seriously.\n");
            vio_setcolor(VIO_COLOR_RESET);
            if (ask_yes(0)) {
               int rc = dsk_newmbr(disk,DSKBR_CLEARALL|DSKBR_LVMINFO);
               printf(rc?"crazy, but done!\n":"unable to write MBR!\n");
               return rc?0:EACCES;
            }
         }
      } else
      if (subcmd==1) { // "CODE"
         printf("Do you want to replace MBR code on disk %s  (%s)?\n",
            prnname, sizestr);
         if (ask_yes(VIO_COLOR_RESET)) {
            // use GPT MBR on pure GPT disks only
            int rc = dsk_newmbr(disk, dsk_isgpt(disk,-1)==1? DSKBR_GPTCODE : 0);
            printf(rc?"done!\n":"unable to write MBR!\n");
            return rc?0:EACCES;
         }
      } else
      if (subcmd==2||subcmd==4) { // "BOOT" & "BOOTFILE"
         u32t    rc, 
              force = 0,
             floppy = disk&QDSK_FLOPPY?1:0;
         pos++;
         // "FORCE" must be the last in command
         if (subcmd==2 && pos<args->count)
            if (stricmp(args->item[args->count-1], "FORCE")==0) {
               force = 1;
               args->count--;
            }
         if (subcmd==2 && args->count<=pos) {
            if (floppy) rc = exit_bootvbr(disk, -1, 0, 0);
               else rc = exit_bootmbr(disk, force?EMBR_NOACTIVE:0);
            switch (rc) {
               case ENOSYS:
                  printf("Disk boot is not possible on EFI host!\n");
                  break;
               case EINVAL:
                  printf("Invalid disk specified: %s!\n", prnname);
                  break;
               case EIO   :
                  printf("Unable to read from disk %s!\n", prnname);
                  break;
               case ENODEV:
                  printf("There is no active partition on disk %s!\n", prnname);
                  break;
               case ENOTBLK:
                  printf("Unable to boot from emulated disk %s!\n", prnname);
                  break;
               default:
                  printf("Error launching boot code from disk %s!\n", prnname);
                  break;
            }
         } else
         if (!floppy && (args->count<=pos || !isdigit(args->item[pos][0]))) {
            printf("Missing or invalid partition index!\n");
            return EINVAL;
         } else {
            long    index = floppy?-1:strtoul(args->item[pos++],0,0);
            u32t filesize = 0;
            char   letter = 0;
            void   *fdata = 0;

            if (force) printf("FORCE option is for MBR boot only!\n");

            if (subcmd==4)
               if (args->count<=pos) {
                  printf("Missing file name parameter!\n");
                  return EINVAL;
               } else {
                  char *np = args->item[args->count-1];
                  fdata = freadfull(np, &filesize);
                  if (!fdata && strchr(np,'/')==0 && strchr(np,'\\')==0) {
                     // trying to mount this volume and read file from the root
                     u32t rvol = dsk_ismounted(disk, index);
                     u8t  mvol = 0;
                     if (!rvol)
                        if (vol_mount(&mvol, disk, index, IOM_READONLY)) mvol = 0; else
                           rvol = QDSK_VOLUME|mvol;
                     if (rvol)
                        if (hlp_volinfo(rvol&QDSK_DISKMASK,0)!=FST_NOTMOUNTED) {
                           // this is FAT/HPFS
                           char *fp = sprintf_dyn("%c:\\", (rvol&QDSK_DISKMASK)+'0');
                           fp    = strcat_dyn(fp, np);
                           fdata = freadfull(fp, &filesize);
                           free(fp);
                        }
                     if (mvol) hlp_unmountvol(mvol);
                  }
                  if (!fdata) {
                     printf("There is no file \"%s\"!\n", args->item[args->count-1]);
                     return ENOENT;
                  } else {
                     u32t sectsize;
                     // get sector size
                     hlp_disksize(disk, &sectsize, 0);
                     if (sectsize!=filesize) {
                        printf("File size is not match to disk sector size!\n");
                        hlp_memfree(fdata);
                        return EBADF;
                     }
                     args->count--;
                  }
               }

            if (args->count>pos && isalpha(args->item[pos][0])) {
               if (args->item[pos][1]!=':' || args->item[pos][2]) {
                  printf("Invalid drive letter parameter!\n");
                  return EINVAL;
               }
               letter = toupper(args->item[pos][0]);
            }
            dsk_ptrescan(disk,0);
            rc = exit_bootvbr(disk, index, letter, fdata);
            if (fdata) hlp_memfree(fdata);

            switch (rc) {
               case ENOSYS:
                  printf("MBR boot mode is not possible on EFI host!\n");
                  break;
               case EINVAL:
                  printf("Invalid disk/partition specified: %s index %d!\n",
                     prnname, index);
                  break;
               case EIO   :
                  printf("Unable to read boot sector from disk %s partition %d!\n",
                     prnname, index);
                  break;
               case ENODEV:
                  printf("Boot sector is empty or regular data in disk %s partition %d!\n",
                     prnname, index);
                  break;
               case ENOTBLK:
                  printf("Unable to boot from emulated disk %s!\n", prnname);
                  break;
               case EFBIG:
                  printf("Partition or a part of it lies outside of 2TB!\n");
                  break;
               default:
                  printf("Error launching VBR code from disk %s partition %d!\n",
                     prnname, index);
                  break;
            }
         }

         return rc;
      } else
      if (subcmd==3) { // "ACTIVE"
         pos++;
         if (args->count<=pos || !isdigit(args->item[pos][0])) {
            printf("Missing or invalid partition index!\n");
            return EINVAL;
         } else {
            u32t   index = strtoul(args->item[pos],0,0), err;
            int  puregpt = 0;
            /* rescan on first access (and it will be rescanned again in
               dsk_setactive()) */
            err = dsk_ptrescan(disk,0);
            if (err) {
               cmd_shellerr(EMSG_QS, err, 0);
               return qserr2errno(err);
            }
            // use hybrid as MBR here!
            puregpt = dsk_isgpt(disk,index)==1;

            if (!puregpt) {
               u32t flags;
               u8t  ptype = dsk_ptquery(disk, index, 0, 0, 0, &flags, 0);

               if (!ptype) {
                  printf("Invalid partition index (%d)!\n", index); return EINVAL;
               }
               if ((flags&DPTF_PRIMARY)==0) {
                  printf("Partition is not primary!\n"); return EINVAL;
               }
               if (IS_EXTENDED(ptype)) {
                  printf("Unable to set extended partition as active!\n"); return EINVAL;
               }
            }
            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you want to set partition %d active on disk %s?\n",index,prnname);
            if (ask_yes(VIO_COLOR_RESET)) {
               qserr rc = puregpt ? dsk_gptactive(disk, index) : dsk_setactive(disk, index);
               if (rc) cmd_shellerr(EMSG_QS, rc, 0);
               return rc ? EACCES : 0;
            }
         }
      }
      return 0;
   }
}

u32t _std shl_dm_bootrec(const char *cmd, str_list *args, u32t disk, u32t pos) {
   char  fsname[10];
   u32t  sttype;
   // no subcommand?
   if (args->count<=pos) return EINVAL;
   // check it for VFS
   if (disk&QDSK_VOLUME) {
      disk_volume_data vi;
      hlp_volinfo(disk&~QDSK_VOLUME, &vi);
      if (vi.TotalSectors && (vi.InfoFlags&VIF_VFS)) {
         cmd_shellerr(EMSG_QS, E_DSK_NOTPHYS, 0);
         return ENOTBLK;
      }
   }
   // current sector type
   sttype = dsk_ptqueryfs(disk,0,fsname,0);
   // check sector type
   if (sttype==DSKST_ERROR) {
      printf("Unable to read boot sector! \n");
      return EIO;
   } else
   if (sttype==DSKST_PTABLE) {
      printf("This is a partitioned disk! \n");
      return EACCES;
   } else {
      char prnname[8];
      static char *subcommands[] = {"WIPE", "CODE", "DEBUG", "BPB", "DIRTY", 0};
      int subcmd = get_arg_index(args->item[pos++],subcommands);

      if (subcmd<0) return EINVAL;
      dsk_disktostr(disk,prnname);

      if (subcmd==0) { // "WIPE"
         if (sttype==DSKST_EMPTY) {
            printf("Boot sector on disk %s is empty\n",prnname);
            return 0;
         }
         if (disk==QDSK_VOLUME) {
            printf("This is your boot partition! \n");
            return EINVAL;
         }
         // now it should filtered by VIF_VFS above
         if (disk==QDSK_VOLUME+DISK_LDR) {
            printf("This is a service volume!\n");
            return EINVAL;
         }
         vio_setcolor(VIO_COLOR_LRED);
         printf("Do you really want to WIPE BOOTSECTOR on disk %s?\n",prnname);
         if (ask_yes(VIO_COLOR_RESET)) {
            int rc = dsk_newmbr(disk,DSKBR_CLEARALL|DSKBR_CLEARPT);
            printf(rc?"done!\n":"unable to write!\n");
            return rc?0:EACCES;
         }
      } else
      if (subcmd==1) { // "CODE"
         if (disk==QDSK_VOLUME) {
            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you really want to replace BOOT disk code?\n");
         } else {
            if (sttype!=DSKST_BOOTFAT && sttype!=DSKST_BOOTEXF && strcmp(fsname,"HPFS")) {
               printf("File system type is unknown!\n");
               return EACCES;
            }
            vio_setcolor(VIO_COLOR_LRED);
            printf("Replacing BOOT code on disk %s?\n",prnname);
         }
         vio_setcolor(VIO_COLOR_LWHITE);
         printf("Current file system is %s.\n",fsname);
         // saving...
         if (ask_yes(VIO_COLOR_RESET)) {
            qserr rc = dsk_newvbr(disk,0,DSKBS_AUTO,args->count==pos+1?args->item[pos]:0);
            if (rc) cmd_shellerr(EMSG_QS, rc, 0);
            return rc?EACCES:0;
         } else
            return EINTR;
      } else
      if (subcmd==2) { // "DEBUG"
         if (disk==QDSK_VOLUME) {
            printf("Unable to write debug boot sector to boot partition!\n");
            return EINVAL;
         }
         vio_setcolor(VIO_COLOR_LRED);
         printf("Replacing BOOT code on disk %s to DEBUG boot sector?\n",prnname);
         printf("DEBUG boot sector CANNOT be used for boot!\n");
         // saving...
         if (ask_yes(VIO_COLOR_RESET)) {
            int rc = dsk_debugvbr(disk,0);
            printf(rc?"done!\n":"unable to write!\n");
            return rc?0:EACCES;
         } else
            return EINTR;
      } else
      if (subcmd==3) { // "BPB"
         int rc = dsk_printbpb(disk, 0);
         if (rc && rc!=EINTR) cmd_shellerr(EMSG_CLIB,EIO,0);
         return rc;
      } else
      if (subcmd==4) { // "DIRTY"
         int nstate = -1,
                 rc = 0;

         if (sttype!=DSKST_BOOTFAT && sttype!=DSKST_BOOTEXF && strcmp(fsname,"HPFS")) {
            printf("File system type is unknown!\n");
            return EACCES;
         }
         if (args->count==pos+1) {
            if (stricmp(args->item[pos],"ON")==0) nstate = 1; else
            if (stricmp(args->item[pos],"OFF")==0) nstate = 0; else
               rc = EINVAL;

         } else
         if (args->count>pos+1) rc = EINVAL;

         if (!rc) {
            rc = vol_dirty(disk, nstate);

            if (nstate<0 && rc>=0) {
               vio_setcolor(VIO_COLOR_LWHITE);
               printf("File system is %s.\n",fsname);
               vio_setcolor(VIO_COLOR_RESET);
               printf("Dirty state is %s.\n",rc?"ON":"OFF");
               return 0;
            } else
            if (rc<0) {
               cmd_shellerr(EMSG_QS, -rc, 0);
               rc = EACCES;
            }
         } else
            cmd_shellerr(EMSG_CLIB,rc,0);
         return rc;
      }
      return 0;
   }
}

u32t shl_dm_mode(const char *cmd, str_list *args, u32t disk, u32t pos) {
   // working disk
   if (disk!=x7FFF && (disk==QDSK_DISKMASK)!=0) {
      printf("Only HDD mode can be switched! \n");
      return EINVAL;
   }
   // do not init "pager" at nested calls
   if (args) cmd_printseq(0,1,0);

   if (disk==FFFF || disk==x7FFF) {
      u32t hddc = hlp_diskcount(0), ii;

      shellprn(" %d HDD%s in system\n",hddc,hddc<2?"":"s");

      for (ii=0;ii<hddc;ii++) {
         disk_geo_data  gdata;
         char          *stext, mbuf[32];
         u32t  mode = hlp_diskmode(ii,HDM_QUERY);

         if (mode) {
            if (mode&HDM_EMULATED) strcpy(mbuf,"emulated"); else
               strcpy(mbuf,mode&HDM_USELBA?"LBA":"CHS");
         } else strcpy(mbuf,"query error");

         hlp_disksize(ii, 0, &gdata);
         if (gdata.TotalSectors) {
            stext = get_sizestr(gdata.SectorSize, gdata.TotalSectors);
         } else {
            stext = "    no info";
         }
         shellprn(" %HDD %i =>%-4s: %s   %s", ii, dsk_disktostr(ii,0), stext, mbuf);
      }
   } else {
      char buf[32];
      u32t prevmode = hlp_diskmode(disk,HDM_QUERY), mode = prevmode;
      int   sizecmd = args->count>pos && stricmp(args->item[pos],"SIZE")==0;
      dsk_disktostr(disk,buf);

      // allow to force VHDD disk size (for a test reason)
      if ((prevmode&HDM_EMULATED) && !sizecmd)
         shellprn("Disk %s is emulated and has no modificable settings", buf);
      else
      if (prevmode) {
         if (args->count>pos) {
            int success = 1;
            if (sizecmd) {
               char     cv;
               u64t   size = 0;
               u32t sector = 0;
               qserr   res;
               if (args->count==++pos) {
                  printf("Missing disk size value\n");
                  return EINVAL;
               }
               cv = args->item[pos][0];
               if (!isdigit(cv) && cv!='*') {
                  printf("Invalid disk size value\n");
                  return EINVAL;
               }
               if (cv!='*') size = str2uint64(args->item[pos]);

               if (args->count==++pos+1) sector = str2ulong(args->item[pos]); else
               if (args->count>pos+1) {
                  cmd_shellerr(EMSG_CLIB, EINVAL, 0);
                  return EINVAL;
               }
               res = dsk_setsize(disk, size, sector);
               if (res) cmd_shellerr(EMSG_QS, res, 0);
               return 0;
            } else
            if (stricmp(args->item[pos],"LBA")==0) mode = HDM_USELBA; else
            if (stricmp(args->item[pos],"CHS")==0) {
               // check disk size
               if (hlp_disksize64(disk,0)>=256*63*1024) {
                  vio_setcolor(VIO_COLOR_LRED);
                  printf("Do you really want to switch disk >8 Gb to CHS mode?\n");
                  if (!ask_yes(VIO_COLOR_RESET)) success=0;
               }
               if (success) mode = HDM_USECHS;
            } else {
               printf("Wrong access mode name!\n");
               return EINVAL;
            }
            if (success) {
               mode = hlp_diskmode(disk,mode);
               shellprn("Previous mode for %s is %s",buf,prevmode&HDM_USELBA?"LBA":"CHS");
            }
         }
         shellprn(" Current mode for %s is %s",buf,mode&HDM_USELBA?"LBA":"CHS");
      } else
         shellprn("Unable to query access mode for disk %s", buf);
   }
   return 0;
}

static void _std clonevol_cb(u32t percent, u32t readed) {
   printf("\rCopying %02d%%",percent);
}

static u32t dm_clone_np = 0,
            dm_clone_ip = 0;

static void _std cloneall_cb(u32t percent, u32t readed) {
   printf("\rCopying partition %d of %d - %02d%%", dm_clone_ip, dm_clone_np, percent);
}

u32t _std shl_dm_clone(const char *cmd, str_list *args, u32t disk, u32t pos) {
   // no subcommand?
   if (args->count<=pos) return EINVAL; else {
      char dstname[8], srcname[8];
      long srcdisk;
      static char *subcommands[] = {"STRUCT", "VOLUME", "ALL", 0};
      int   subcmd = get_arg_index(args->item[pos++],subcommands);
      u32t  dpterr = 0;

      if (subcmd<0 || args->count<pos+1) {
         cmd_shellerr(EMSG_CLIB, EINVAL, 0);
         return EINVAL;
      }
      srcdisk = get_disk_number(args->item[pos++]);
      if (srcdisk<0) {
         printf("Invalid source disk name specified!\n");
         return EINVAL;
      }
      if ((disk&QDSK_VOLUME) || (srcdisk&QDSK_VOLUME)) {
         printf("Both source and destination must be a physical disk, not a volume!\n");
         return ENOTBLK;
      }
      dsk_disktostr(disk,dstname);
      dsk_disktostr(srcdisk,srcname);

      if (subcmd==1) { // "VOLUME"
         //dmgr clone hd1 volume hd2 pt [hd1pt]
         char  fsname[10], sdname[8], ddname[8], slen[9], dlen[9];
         u64t    dstsize, srcsize;
         u32t   srcindex, dstindex, s_sectsz, d_sectsz;

         if (args->count<pos+1 || args->count>pos+2) {
            cmd_shellerr(EMSG_CLIB, EINVAL, 0);
            return EINVAL;
         }
         // use str2long to prevent octal conversion
         srcindex = str2long(args->item[pos++]);
         if (args->count==pos+1) dstindex = str2long(args->item[pos++]);
            else dstindex = srcindex;

         if (!dsk_ptquery64(disk, dstindex, 0, &dstsize, fsname, 0)) {
            printf("Destination partition index is invalid\n");
            return EINVAL;
         }
         if (!dsk_ptquery64(srcdisk, srcindex, 0, &srcsize, 0, 0)) {
            printf("Source partition index is invalid\n");
            return EINVAL;
         }
         dsk_disktostr(srcdisk,sdname);  hlp_disksize(srcdisk, &s_sectsz, 0);
         dsk_disktostr(disk   ,ddname);  hlp_disksize(disk   , &d_sectsz, 0);
         dsk_formatsize(s_sectsz, srcsize, 0, slen);
         dsk_formatsize(d_sectsz, dstsize, 0, dlen);

         vio_setcolor(VIO_COLOR_LRED);
         printf("Do you really want to copy disk %s partition %d (%s)\n"
            "to disk %s partition %d (%s)?\n", sdname, srcindex, slen,
               ddname, dstindex, dlen);
         if (fsname[0]) {
            vio_setcolor(VIO_COLOR_LWHITE);
            printf("Current file system on target is %s. It will be lost!\n",
               fsname);
         }
         if (ask_yes(VIO_COLOR_RESET)) {
            dpterr = dsk_clonedata(disk, dstindex, srcdisk, srcindex, clonevol_cb, 0);
            // clear %% printing
            printf("\r                       \r");
         } else
            return EINTR;
      } else
      if (subcmd==0 || subcmd==2) { // "STRUCT" && "ALL"
         static char *argstr   = "ident|sameid|ignspt|nowipe";
         static short argval[] = { 1,   1};
         int    ident = 0, sameid = 0, ignspt = 0, nowipe = 0, rcnt;
         u32t   flags = DCLN_MBRCODE;
         args = str_parseargs(args, pos, SPA_RETLIST, argstr, argval, &ident,
                              &sameid, &ignspt, &nowipe);
         rcnt = args->count;
         free(args);
         // we must get empty list, else unknown keys here
         if (rcnt) {
            cmd_shellerr(EMSG_CLIB, EINVAL, 0);
            return EINVAL;
         }
         if (ident ) flags|=DCLN_IDENT;
         if (sameid) flags|=DCLN_SAMEIDS;
         if (ignspt) flags|=DCLN_IGNSPT;
         if (nowipe) flags|=DCLN_NOWIPE;

         dpterr = dsk_clonestruct(disk, srcdisk, flags);
         if (subcmd==2 && dpterr==0) {
            long np = dsk_partcnt(disk);
            if (np<0 || np!=dsk_partcnt(srcdisk)) {
               printf("Fatal error while cloning partition structure.\n");
               return EACCES;
            } else {
               u32t ii;
               // copy partitiions
               for (ii=0; ii<np; ii++) {
                  dm_clone_np = np;
                  dm_clone_ip = ii+1;
                  dpterr = dsk_clonedata(disk, ii, srcdisk, ii, cloneall_cb, 0);
                  // clear %% printing
                  printf("\r                                                  \r");
                  if (dpterr) {
                     log_it(2, "clone err %d on partition %d (%s->%s)\n", dpterr,
                        ii, srcname, dstname);
                     break;
                  }
               }
            }
         }
      }
      if (dpterr) {
         cmd_shellerr(EMSG_QS, dpterr, 0);
         return EACCES;
      }
      return 0;
   }
}

/* Disc manager command.
   cmdline: dm subcommand disk operation options */
u32t _std shl_dmgr(const char *cmd, str_list *args) {
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count==1 && stricmp(args->item[0],"LIST")==0) {
      return shl_dm_list(cmd, args, FFFF, 1);
   } else
   while (args->count>=2) {
      static char *subcommands[] = {"MBR", "BOOT", "LIST", "MODE", "BS", "PM",
                                    "CLONE", 0};
      int  subcmd = get_arg_index(args->item[0],subcommands);
      long   disk;
      if (subcmd<0) break;

      // help for subcommand?
      if (strcmp(args->item[1],"/?")==0) {
         char hname[128];
         strcpy(hname,cmd);
         strcat(hname,".");
         strcat(hname,args->item[0]);
         cmd_shellhelp(hname,CLR_HELP);
         return 0;
      }

      if (subcmd==5 && stricmp(args->item[1],"FREE")==0) disk=x7FFF; else
      if ((subcmd==2||subcmd==3) && stricmp(args->item[1],"ALL")==0) disk=x7FFF;
      else {
         disk = get_disk_number(args->item[1]);
         if (disk<0)
            if (subcmd==2) disk=FFFF; else {
               printf("Invalid disk name specified!\n");
               return EINVAL;
            }
      }

      switch (subcmd) {
         case 0:
            return shl_dm_mbr(cmd, args, disk, 2);
         case 1: case 4:
            return shl_dm_bootrec(cmd, args, disk, 2);
         case 2:
            return shl_dm_list(cmd, args, disk, disk==FFFF?1:2);
         case 3:
            return shl_dm_mode(cmd, args, disk, 2);
         case 5:
            return shl_pm(cmd, args, disk, 2);
         case 6:
            return shl_dm_clone(cmd, args, disk, 2);
      }
      break;
   }
   cmd_shellerr(EMSG_CLIB, EINVAL,0);
   return EINVAL;
}

static void _std fmt_callback(u32t percent, u32t readed) {
   printf("\rFormatting %02d%%",percent);
}

u32t _std shl_format(const char *cmd, str_list *args) {
   u32t rc = 0, fstype = 0;
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   while (args->count>0) {
      int  ii, quick = 0, wipe = 0, noaf = 0, asz = 0, fatcpn = 2, quiet = 0, force = 0;
      u32t   disk = 0;
      char *szstr = 0;
      disk_volume_data vi;

      for (ii=0; ii<args->count && !rc; ii++) {
         if (args->item[ii][0]=='/') {
            switch (toupper(args->item[ii][1])) {
               case 'A':if (args->item[ii][2]==':') {
                           char *ep = 0;
                           asz = strtoul(args->item[ii]+3, &ep, 0);
                           if (toupper(*ep)=='K') asz*=1024; else 
                              if (toupper(*ep)=='M') asz*=1024*1024; else
                                 if (*ep) rc=EINVAL;
                           // check for power of 2
                           if (!rc) {
                              int bit = bsf64(asz);
                              if (bit<0 || 1<<bit!=asz) rc=EINVAL;
                           }
                        } else rc=EINVAL;
                        break;
               case 'F':if (args->item[ii][2]==':') {
                           fatcpn = atoi(args->item[ii]+3);
                           if (fatcpn!=1 && fatcpn!=2) rc=EINVAL;
                        } else
                        if (toupper(args->item[ii][2])=='S' &&
                           args->item[ii][3]==':')
                        {
                           if (stricmp(args->item[ii]+4, "FAT")==0) fstype = 0;
                              else
                           if (stricmp(args->item[ii]+4, "HPFS")==0) fstype = 1;
                              else
                           if (stricmp(args->item[ii]+4, "EXFAT")==0) {
                              fstype = 2; fatcpn = 1;
                           } else rc=EINVAL;
                        } else 
                        if (stricmp(args->item[ii]+1,"FORCE")==0) force = 1;
                          else rc=EINVAL;
                        break;
               case 'N':if (stricmp(args->item[ii]+1,"NOAF")==0) noaf = 1;
                           else rc=EINVAL;
                        break;
               case 'Q':if (stricmp(args->item[ii]+1,"QUICK")==0) quick = 1;
                           else
                        if (stricmp(args->item[ii]+1,"Q")==0) quiet = 1;
                           else rc=EINVAL;
                        break;
               case 'W':if (stricmp(args->item[ii]+1,"WIPE")==0) wipe = 1;
                           else rc=EINVAL;
                        break;
               default: rc=EINVAL;
            }
         } else
         if (disk) rc=EINVAL; else {
            long dsk = get_disk_number(args->item[ii]);
            if (dsk<0) rc = -dsk; else disk = dsk;
         }
      }
      if (rc) break;
      if (quick && wipe) {
         printf("Select one of WIPE and QUICK, please!\n");
         return EINVAL;
      }
      if ((disk&QDSK_VOLUME)==0) {
         printf("Invalid volume name specified!\n");
         return EINVAL;
      }
      if (fstype==2 && fatcpn>1) {
         printf("Only single FAT copy is supported on EXFAT now!\n");
         return EINVAL;
      }
      disk &= QDSK_DISKMASK;

      if (disk<=DISK_LDR) {
         printf("Unable to format 0: and 1: volumes!\n");
         return EINVAL;
      }
      hlp_volinfo(disk, &vi);
      if (vi.InfoFlags&VIF_VFS) {
         printf("Format is not possible for non-physical disk\n");
         return EINVAL;
      }
      if (vi.InfoFlags&VIF_NOWRITE) {
         printf("Volume is mounted as read-only!\n");
         return EINVAL;
      }
      // only emulated hdds can be formatted quietly
      if (quiet && (hlp_diskmode(vi.Disk,HDM_QUERY)&HDM_EMULATED)==0) {
         printf("Quiet format is not possible for real disks!\n");
         return EINVAL;
      }
      // buffer is static!
      szstr = dsk_formatsize(vi.SectorSize, vi.TotalSectors, 0, 0);

      if (!quiet) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("Do you really want to format volume %s (%s)?\n"
            "All data will be lost!\n", dsk_disktostr(disk|QDSK_VOLUME,0), szstr);
      }
      if (quiet || ask_yes(VIO_COLOR_RESET)) {
         u32t flags = DFMT_BREAK;
         if (quick) flags|=DFMT_QUICK;
         if (fatcpn==1) flags|=DFMT_ONEFAT;
         if (wipe) flags|=DFMT_WIPE;
         if (noaf) flags|=DFMT_NOALIGN;
         if (force || quiet) flags|=DFMT_FORCE;

         switch (fstype) {
            case 0: rc = vol_format (disk, flags, asz, quiet?0:fmt_callback); break;
            case 1: rc = hpfs_format(disk, flags, quiet?0:fmt_callback); break;
            case 2: rc = exf_format(disk, flags, asz, quiet?0:fmt_callback); break;
            default: rc = EINVAL;
         }

         if (rc) {
            cmd_shellerr(EMSG_QS, rc, 0);
            rc = ENOMNT;
         } else
         if (!quiet) {
            static const char *ftstr[5] = {"unrecognized", "FAT12", "FAT16", "FAT32", "exFAT"};
            u32t  fstype = hlp_volinfo(disk, &vi),
                  clsize = vi.ClSize * vi.SectorSize;
            int   msgidx = !fstype || fstype>=FST_OTHER ? (vi.FsName[0]?-1:0) : fstype;

            printf("\rFormat complete.     \n");
            printf("\nThe type of file system is %s.\n\n", msgidx<0?vi.FsName:ftstr[msgidx]);
            printf("  %s total disk space\n", get_sizestr(vi.SectorSize, vi.TotalSectors));

            printf("  %s are available\n", get_sizestr(clsize, vi.ClAvail));
            if (vi.ClBad)
               printf("\n  %s in bad sectors\n", get_sizestr(clsize, vi.ClBad));

            printf("\n  %8d bytes in each allocation unit.\n", clsize);
            printf("  %8d total allocation units on disk.\n", vi.ClTotal);
            printf("  %8d allocation units available on disk.\n", vi.ClAvail);
         }
      }
      return rc;
   }
   cmd_shellerr(EMSG_CLIB, !rc?EINVAL:rc, 0);
   return !rc?EINVAL:rc;
}

u32t _std shl_lvm(const char *cmd, str_list *args) {
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   while (args->count>=2) {
      static char *subcommands[8] = {"WRITE", "ASSIGN", "RENAME", "INFO",
                                     "QUERY", "WIPE"  , "SELECT", 0};
      int    subcmd = get_arg_index(args->item[0],subcommands),
                 rc = E_SYS_INVPARM;
      long     disk = FFFF;
      u32t    ssize = 0;
      u64t  dsksize = 0;
      if (subcmd<0) break;
      // read disk parameter
      if (subcmd<4 || subcmd==5 || subcmd==4 && args->count>4) {
         disk = get_disk_number(args->item[subcmd==4?4:1]);
         if (disk<0) {
            printf("Invalid disk name specified!\n");
            return EINVAL;
         }
         if ((disk&QDSK_VOLUME)!=0) {
            printf("This is volume, not a physical disk! \n");
            return EINVAL;
         }
      }
      if (disk!=FFFF) dsksize = hlp_disksize64(disk, &ssize);

      if (subcmd==0) {
         int master_boot = -1;
         rc = lvm_checkinfo(disk);
         // check for non-LVMed huge disk
         if (rc==E_LVM_NOINFO)
            if (dsksize>63*255*65535 && dsk_partcnt(disk)>0) {
               printf("Writing LVM info to non-OS/2 formatted huge "
                      "disk (>500Gb) will affect nothing.\n");
               if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
            }
         if (args->count>=3)
            if (stricmp(args->item[2],"MASTER")==0) master_boot = 1; else break;
         rc = lvm_initdisk(disk,0,master_boot);
      } else
      if (subcmd==1 && args->count>=3) {
         char ltr = args->count>=4?args->item[3][0]:0;
         u32t index, force = 0;

         if (ltr) ltr = toupper(ltr);
         // force letter parameter
         if (args->count>=5 && ltr)
            if (stricmp(args->item[4],"FORCE")==0) force = 1; else break;
         // invalid index (no digit at 1st pos)
         if (!isdigit(args->item[2][0])) break;
         // use str2long to prevent octal conversion
         index = str2long(args->item[2]);
         rc = lvm_assignletter(disk,index,ltr,force);
      } else
      if (subcmd==2 && (args->count==3 || args->count==4)) {
         u32t index, flags = 0;
         if (args->count==4) {
            flags = LVMN_VOLUME|LVMN_PARTITION;
            // invalid index (no digit at 1st pos)
            if (!isdigit(args->item[2][0])) break;
            // use str2long to prevent octal conversion
            index = str2long(args->item[2]);
         }
         rc = lvm_setname(disk, index, flags, args->item[flags?3:2]);
      } else
      if (subcmd==3) {
         rc = lvm_checkinfo(disk);
         if (!rc) {
            if (args->count>2) {
               lvm_partition_data info;
               u32t index = str2long(args->item[2]);

               if (lvm_partinfo(disk, index, &info)) {
                  shellprc(VIO_COLOR_LWHITE, " %s %i, partition %u: %s (%X sectors)",
                     disk&QDSK_FLOPPY?"Floppy disk":"HDD", disk&QDSK_DISKMASK,
                        index, get_sizestr(ssize,info.Size), info.Size);

                  shellprn(" Volume serial   : %04X-%04X\n"
                           " Partition serial: %04X-%04X\n"
                           " Partition start : %08X\n"
                           " In Boot menu    : %s\n"
                           " Drive letter    : %c%c\n"
                           " Volume name     : %s\n"
                           " Partition name  : %s",
                           info.VolSerial>>16,  info.VolSerial&0xFFFF,
                           info.PartSerial>>16, info.PartSerial&0xFFFF,
                           info.Start, info.BootMenu?"Yes":"No",
                           info.Letter?info.Letter:'-', info.Letter?':':' ',
                           info.VolName, info.PartName);
               } else
               if (dsk_ptquery(disk,index,0,0,0,0,0)) rc = E_LVM_NOPART; else
                  rc = E_PTE_PINDEX;
            } else {
               lvm_disk_data lvd;
               lvm_diskinfo(disk,&lvd);

               shellprc(VIO_COLOR_LWHITE, " %s %i: %s (%LX sectors)",
                  disk&QDSK_FLOPPY?"Floppy disk":"HDD", disk&QDSK_DISKMASK,
                     get_sizestr(ssize,dsksize), dsksize);

               shellprn(" Serial number   : %04X-%04X\n"
                        " Boot disk serial: %04X-%04X\n"
                        " CHS geometry    : %d x %d x %d\n"
                        " Name            : %s",
                        lvd.DiskSerial>>16, lvd.DiskSerial&0xFFFF,
                        lvd.BootSerial>>16, lvd.BootSerial&0xFFFF,
                        lvd.Cylinders, lvd.Heads, lvd.SectPerTrack, lvd.Name);
            }
         }
      } else
      if (subcmd==4 && args->count>=2) {
         // LVM QUERY "name" [diskname [partname [disk]]]^
         u32t     idx;
         int  dskmode = args->count<=3;
         if (dskmode)
            rc = lvm_findname(args->item[1], LVMN_DISK, (u32t*)&disk, 0);
         if (rc && args->count!=3) {
            rc = lvm_findname(args->item[1], LVMN_PARTITION, (u32t*)&disk, &idx);
            dskmode = 0;
         }
         if (!rc) {
            char buf[16];
            dsk_disktostr(disk,buf);

            if (args->count>2) {
               setenv(args->item[2], buf, 1);
               if (args->count>3) {
                  utoa(idx,buf,10);
                  setenv(args->item[3], buf, 1);
               }
            } else {
               if (dskmode)
                  printf("LVM name \"%s\" is disk %s\n", args->item[1], buf);
               else
                  printf("LVM name \"%s\" is disk %s partition index %d.\n",
                     args->item[1], buf, idx);
            }
         } else {
            // delete variables if command failed
            if (args->count>2) setenv(args->item[2], 0, 1);
            if (args->count>3) setenv(args->item[3], 0, 1);
         }
      } else
      if (subcmd==5 && args->count==2) {
         disk_geo_data geo;

         vio_setcolor(VIO_COLOR_LRED);
         printf("Wipe LVM info on disk %s (%s)?\n", dsk_disktostr(disk,0),
            dsk_formatsize(ssize,dsksize,0,0));
         if (!ask_yes(VIO_COLOR_RESET)) return EINTR;

         vio_setcolor(VIO_COLOR_LRED);
         if (!dsk_getptgeo(disk,&geo))
            if (geo.SectOnTrack>63 && lvm_checkinfo(disk)!=E_LVM_NOINFO) {
               printf("It is highly not recommended to remove LVM info from huge disk (>500Gb)\n"
                      "because it required for disk format detection!\n");
               if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
            }
         vio_setcolor(VIO_COLOR_RESET);
         rc = lvm_wipeall(disk);
      } else
      if (subcmd==6 && args->count>=2) {
         /* it should have at least BOOT or disk name parameter:
            LVM SELECT [ALL|DISK] {BOOT|[partname] disk} */
         static char *argstr   = "ALL|DISK|BOOT|E+|E-";
         static short argval[] = { BTDT_DISKTREE, BTDT_HDD, 1, 0, BTDT_NOEMU};
         u32t     rdsk = FFFF;
         long     ridx = -1;
         u32t       dt = BTDT_LVM, emu = FFFF;
         int      boot = 0;
         char  *dskvar = 0,
               *ptivar = 0;
         str_list *arr = str_parseargs(args, 1, SPA_RETLIST, argstr, argval,
                                       &dt, &dt, &boot, &emu, &emu);
         if (emu==FFFF) emu = boot?BTDT_NOEMU:0;
         // filter variants without error
         if (boot && arr->count==0) rc = 0; else
         if (!boot)
            if (dt==BTDT_HDD && arr->count==1) { rc = 0; dskvar = arr->item[0]; } else
               if (arr->count==2) { rc = 0; ptivar = arr->item[0]; dskvar = arr->item[1]; }
         // no error in args
         if (!rc) {
            qs_extcmd dlg = NEW(qs_extcmd);
            // no extcmd.dll?
            if (!dlg) rc = E_SYS_UNSUPPORTED; else {
               rc = dlg->bootmgrdlg(dt==BTDT_LVM?"Select volume":(dt==BTDT_HDD?
                  "Select disk":"Select partition"), 0, dt|emu, 0, &rdsk, &ridx);
               DELETE(dlg);
               if (!rc) {
                  if (dskvar) setenv(dskvar, dsk_disktostr(rdsk,0), 1);
                  if (ptivar) env_setvar(ptivar, ridx);
                  // call "DMGR MBR disk BOOT" command just to simplify code
                  if (boot) {
                     char     *cmd = sprintf_dyn(dt==BTDT_HDD||ridx<0?"BOOT":"BOOT %u", ridx);
                     str_list *cma = str_split(cmd, " ");
                     free(cmd);
                     shl_dm_mbr(0, cma, rdsk, 0);
                     free(cma);
                 }
               } else {
                  // clear vars in case of error
                  if (dskvar) setenv(dskvar, 0, 1);
                  if (ptivar) setenv(ptivar, 0, 1);
               
                  if (rc==E_SYS_UBREAK) rc = 0;
               }
            }
         }
         if (arr) free(arr);
      }
      if (rc) {
         cmd_shellerr(EMSG_QS, rc, 0);
         return qserr2errno(rc);
      }
      return 0;
   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
   return EINVAL;
}

static u32t dmgr_pm_disk_free(u32t disk) {
   char prnname[8];
   u32t      sz = dsk_ptgetfree(disk,0,0), ssize, ii, rc=0;
   u64t dsksize = hlp_disksize64(disk,&ssize);

   dsk_disktostr(disk,prnname);

   shellprc(VIO_COLOR_LWHITE, " %s %i: %s (%LX sectors)",
      disk&QDSK_FLOPPY?"Floppy disk":"HDD", disk&QDSK_DISKMASK,
         get_sizestr(ssize,dsksize), dsksize);

   if (sz) {
      dsk_freeblock *fi = (dsk_freeblock*)malloc_thread(sz*sizeof(dsk_freeblock));
      sz = dsk_ptgetfree(disk,fi,sz);

      if (sz) {
         if (shellprt(
            " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄ \n"
            "  ## ³ position ³    size    ³ primary ³ logical  \n"
            " ÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄ "))
               rc=EINTR;
         else
         for (ii=0; ii<sz; ii++)
            if (shellprn(" %3d ³ %08X ³%s ³    %c    ³    %c", ii,
               (u32t)fi[ii].StartSector, get_sizestr(ssize,fi[ii].Length),
                  fi[ii].Flags&DFBA_PRIMARY?'+':'-',
                     fi[ii].Flags&DFBA_LOGICAL?'+':'-')) { rc=EINTR; break; }
      }
      free(fi);
      if (rc) return rc;
   }
   if (!sz) {
      rc = dsk_ptrescan(disk,0);
      if (rc) rc=rc==E_DSK_ERRREAD||rc==E_DSK_ERRWRITE?EIO:ENODEV; else
         shellprn(" There is no free space on disk %s!",prnname);
   }
   return rc;
}

u32t shl_pm(const char *cmd, str_list *args, u32t disk, u32t pos) {
   u32t  ii, rc=0;

   cmd_printseq(0,1,0);
   // dmgr pm free
   if (disk==x7FFF) {
      u32t hdds = hlp_diskcount(0);
      for (ii=0;ii<hdds;ii++)
         if ((rc=dmgr_pm_disk_free(ii))!=0) break;
            else shellprt("");
      return rc;
   }
   // no subcommand?
   if (args->count<=pos) return EINVAL;

   while (1) {
      static char *subcommands[6] = {"INIT", "FREE", "CREATE", "DELETE", "DEL", 0};
      char prnname[8];
      int   subcmd = get_arg_index(args->item[pos++],subcommands);
      dsk_disktostr(disk,prnname);

      if ((disk&QDSK_VOLUME)!=0) {
         printf("This is volume, not a physical disk!\n");
         return EINVAL;
      }
      if (subcmd<0) break;

      if (subcmd==0) { // "INIT"
         int lvmmode = 0;
         if (args->count>pos+1) break;
         if (args->count==pos+1)
            if (stricmp(args->item[pos],"LVM")==0) lvmmode = 1; else
               if (stricmp(args->item[pos],"GPT")==0) lvmmode = -1; else break;
         // additional check for floppy
         if ((disk&QDSK_FLOPPY)!=0) {
            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you really want to init FLOPPY %s as partitioned disk?\n",
               prnname);
            if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
         }
         if (lvmmode<0) {
            // ask about GPT for real (non-emulated) disks < 2TB
            if ((hlp_diskmode(disk, HDM_QUERY)&HDM_EMULATED)==0) {
               u32t  ssize;
               u64t  dsize = hlp_disksize64(disk, &ssize);

               if (dsize < _4GBLL) {
                  char *dsstr = dsk_formatsize(ssize, dsize, 0, 0);
                  vio_setcolor(VIO_COLOR_LRED);
                  printf("Do you want to init disk %s, which is smaller than 2Tb (%s) as GPT?\n",
                     prnname, dsstr);
                  if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
               }
            }
            rc = dsk_gptinit(disk);
         } else
            rc = dsk_ptinit(disk, lvmmode);
         // special error code
         if (rc==E_PTE_EMPTY) {
            printf("Disk is not empty!\n");
            return ENODEV;
         }
      } else
      if (subcmd==1) { // "FREE"
         dmgr_pm_disk_free(disk);
      } else
      if (subcmd==2) { // "CREATE"
         // dmgr pm hd0 create index type [size [place]] [AF+/-]
         // dmgr pm hd0 create 0 p 48000 start
         // dmgr pm hd0 create 0 p 48% start
         u32t fb_index, flags = 0, ptsize = 0, aflags;
         u64t   pt_pos, pt_len;
         char  se_char = 'S', *pt;
         int   percent = 0,
                 argsc = args->count,
               forceaf = -1, isgpt,
               forceac = -1;

         if (argsc<pos+2 || argsc>pos+6) break;
         if (!isdigit(args->item[pos][0])) break;

         fb_index = str2long(args->item[pos]);
         // partition type
         pt = args->item[pos+1];
         if (stricmp(pt,"PRIMARY")==0 || stricmp(pt,"P")==0) flags = DFBA_PRIMARY;
            else
         if (stricmp(pt,"LOGICAL")==0 || stricmp(pt,"L")==0) flags = DFBA_LOGICAL;
            else break;
         // check for af+/-
         if (argsc>=pos+3) {
            int argsp;
            do {
               char *str = args->item[argsc-1];
               argsp = argsc;
               if (stricmp(str,"AF+")==0) { forceaf=1; argsc--; } else
                  if (stricmp(str,"AF-")==0) { forceaf=0; argsc--; } else
                     if (stricmp(str,"AC+")==0) { forceac=1; argsc--; } else
                        if (stricmp(str,"AC-")==0) { forceac=0; argsc--; }
            } while (argsc!=argsp && argsc>=pos+2);
         }
         // partition size
         if (argsc>=pos+3) {
            pt = args->item[pos+2];
            ptsize = str2long(pt);
            if (!ptsize) {
               printf("Type partition size in megabytes or percents!\n");
               return EINVAL;
            }
            if (pt[strlen(pt)-1]=='%') {
               if (!ptsize || ptsize>100) break;
               percent = 1;
            }
            // start/end position in free space
            if (argsc==pos+4) {
               se_char = toupper(args->item[pos+3][0]);
               if (se_char!='S' && se_char!='E') {
                  printf("Position string must be \"s[tart]\" or \"e[nd]\"!\n");
                  return EINVAL;
               }
            } else
            if (argsc>pos+4) break;
         }
         isgpt  = dsk_isgpt(disk,-1) > 0;
         aflags = forceaf<0 ? (isgpt?DPAL_AF:0) : (forceaf>0?DPAL_AF:0);

         if (isgpt && forceac>0) aflags |= DPAL_CYLSIZE;

         if (percent)            aflags |= DPAL_PERCENT;
         if (!isgpt)             aflags |= DPAL_CHSSTART|DPAL_CHSEND;
         if (se_char=='E')       aflags |= DPAL_ESPC;
         if (flags&DFBA_LOGICAL) aflags |= DPAL_LOGICAL;

         rc = dsk_ptalign(disk, fb_index, ptsize, aflags, &pt_pos, &pt_len);
         if (!rc)
            if (isgpt)
               rc = dsk_gptcreate(disk, pt_pos, pt_len, flags, 0);
            else
               rc = dsk_ptcreate(disk, pt_pos, pt_len, flags, PTE_0C_FAT32);
      } else
      if (subcmd==3 || subcmd==4) { // "DELETE"
         // dmgr pm hd0 delete index
         u64t         ptsize, start = 0;
         u32t       pt_index;
         char       desc[20];
         disk_volume_data vi;

         if (args->count!=pos+1) break;
         if (!isdigit(args->item[pos][0])) break;
         pt_index = str2long(args->item[pos]);
         // scan if not done before
         dsk_ptrescan(disk,0);
         // query partition
         desc[0]  = '(';
         desc[1]  = 0;
         dsk_ptquery64(disk, pt_index, &start, &ptsize, desc+1, 0);
         if (desc[1]) strcat(desc,")"); else desc[0] = 0;

         hlp_volinfo(0,&vi);
         // is this a boot partition?
         if (vi.StartSector==start && vi.Disk==disk) {
            printf("Unable to delete boot partition!\n");
            return EACCES;
         } else {
            u32t    ssize;
            char    *dscp;

            hlp_disksize(disk, &ssize, 0);
            dscp = dsk_formatsize(ssize, ptsize, 0, 0);

            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you really want to delete partition %d on disk %s?\n"
                   "Partition size is %s %s\n", pt_index, prnname, dscp, desc);
            if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
            rc = dsk_ptdel(disk, pt_index, start);
         }
      }
      if (rc) {
         cmd_shellerr(EMSG_QS, rc, 0);
         return rc==E_DSK_ERRREAD||rc==E_DSK_ERRWRITE?EIO:ENODEV;
      }
      return 0;
   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
   return EINVAL;
}


// make attrs str: empty or "(name1 name2)"
static void fill_gptattr(char *buf, u64t attr) {
   static const s8t attr_flags[] = { GPTATTR_SYSTEM, GPTATTR_IGNORE,
      GPTATTR_BIOSBOOT, GPTATTR_MS_RO, GPTATTR_MS_HIDDEN,
         GPTATTR_MS_NOMNT, -1};
   static const char *attr_str[] = {"System", "Ignore", "BiosBoot",
      "ReadOnly", "Hidden", "NoMount"};
   u32t kk;

   *buf = 0;
   if (attr)
      for (kk=0; attr_flags[kk]>=0; kk++)
         if (1LL<<attr_flags[kk] & attr) {
            if (*buf==' ') buf++; else *buf++='(';
            strcpy(buf, attr_str[kk]);
            buf += strlen(buf);
            *buf = ' ';
         }
   if (*buf==' ') { *buf++=')'; *buf=0; }
}


u32t _std shl_gpt(const char *cmd, str_list *args) {
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }

   cmd_printseq(0,1,0);

   while (args->count>=2) {
      char prnname[8];
      static char *subcommands[] = {"TYPE", "ATTR", "RENAME", "INFO", "QUERY",
                                    "ID", "QTYPE", 0};
      int   subcmd = get_arg_index(args->item[0],subcommands),
                rc = -1,
             query = subcmd==4 || subcmd==6;
      long    disk = FFFF;
      u32t   index = FFFF;
      u32t   ssize = 0;
      u64t dsksize = 0;

      if (subcmd<0) break;

      if (!query || args->count>4) {
         disk = get_disk_number(args->item[query?4:1]);
         if (disk<0) {
            printf("Invalid disk name specified!\n");
            return EINVAL;
         }
         if ((disk&QDSK_VOLUME)!=0) {
            printf("This is volume, not a physical disk!\n");
            return EINVAL;
         }
         dsksize = hlp_disksize64(disk,&ssize);
         dsk_disktostr(disk,prnname);
      }
      // partition index for all commands except QUERY/QTYPE
      if (!query && args->count>2 && !(subcmd==5 && args->count==3)) {
         // invalid index (no digit at 1st pos)
         if (!isdigit(args->item[2][0])) break;
         // use str2long to prevent octal conversion
         index = str2long(args->item[2]);
      }

      if (subcmd==0 || subcmd==5) { // "TYPE" and "ID"
         if (args->count==4 || subcmd==5 && args->count==3) {
            u8t         gbuf[16];
            if (!dsk_strtoguid(args->item[args->count-1], gbuf)) {
               printf("Incorrect GUID string format!\n");
               return EINVAL;
            }
            if (args->count==3) { // set disk GUID
               dsk_gptdiskinfo info;
               rc = dsk_gptdinfo(disk, &info);
               if (!rc) {
                  memcpy(info.GUID, gbuf, 16);
                  rc = dsk_gptdset(disk, &info);
               }
            } else {  // set partition (type) GUID
               dsk_gptpartinfo info;
               rc = dsk_gptpinfo(disk, index, &info);
               if (!rc) {
                  memcpy(subcmd==0 ? info.TypeGUID : info.GUID, gbuf, 16);
                  rc = dsk_gptpset(disk, index, &info);
               }
            }
         }
      } else
      if (subcmd==1 && args->count>=3) { // "ATTR"
         dsk_gptpartinfo info;
         rc = dsk_gptpinfo(disk, index, &info);
         if (!rc) {
            if (args->count>3) {
               static char *argstr   = "+r|-r|+s|-s|+h|-h|+i|-i|+b|-b|+n|-n";
               static short argval[] = { 1,-1, 1,-1, 1,-1, 1,-1, 1,-1, 1,-1};
               static u8t     bitv[] = { GPTATTR_MS_RO, GPTATTR_SYSTEM,
                                         GPTATTR_MS_HIDDEN, GPTATTR_IGNORE,
                                         GPTATTR_BIOSBOOT, GPTATTR_MS_NOMNT };
               int             af[6], ii;
               u64t            nattr = info.Attr;
               memset(af, 0, sizeof(af));
               // zero main args to exclude it from search
               args->item[0][0] = 0;
               args->item[1][0] = 0;
               args->item[2][0] = 0;
               str_parseargs(args, 0, 0, argstr, argval, &af[0], &af[0], &af[1],
                  &af[1], &af[2], &af[2], &af[3], &af[3], &af[4], &af[4],
                     &af[5], &af[5]);
               for (ii=0; ii<6; ii++)
                  if (af[ii]<0) nattr&=~(1LL<<bitv[ii]); else
                     if (af[ii]>0) nattr|=1LL<<bitv[ii];
               // attrs changed -> update
               if (nattr!=info.Attr) {
                  info.Attr = nattr;
                  rc = dsk_gptpset(disk, index, &info);
               }
            }
            if (!rc) {
               char attrs[128];
               fill_gptattr(attrs, info.Attr);

               shellprc(VIO_COLOR_LWHITE, " Disk %s, partition %u: %s (%LX sectors)",
                  prnname, index, get_sizestr(ssize,info.Length), info.Length);
               shellprn(" Attributes : %LX %s", info.Attr, attrs);
            }
         }
      } else
      if (subcmd==2 && args->count==4 && args->item[3][0]) { // "RENAME"
         dsk_gptpartinfo info;

         rc = dsk_gptpinfo(disk, index, &info);
         if (!rc) {
            u32t ii;
            memset(info.Name, 0, sizeof(info.Name));
            for (ii=0; ii<36; ii++)
               if ((info.Name[ii] = args->item[3][ii])==0) break;
            rc = dsk_gptpset(disk, index, &info);
         }
      } else
      if (subcmd==3) { // "INFO"
         if (args->count>2) {
            dsk_gptpartinfo info;
            rc = dsk_gptpinfo(disk, index, &info);
            if (!rc) {
               char gpstr[40], gtstr[40], *ptstr, attrs[128];

               shellprc(VIO_COLOR_LWHITE, " Disk %s, partition %u: %s (%LX sectors)",
                  prnname, index, get_sizestr(ssize,info.Length), info.Length);

               dsk_guidtostr(info.TypeGUID, gtstr);
               dsk_guidtostr(info.GUID, gpstr);

               ptstr = gtstr[0] ? cmd_shellgetmsg(gtstr) : 0;
               fill_gptattr(attrs, info.Attr);

               shellprn(" Partition GUID : %s\n"
                        " Type GUID      : %s\n"
                        " Type GUID info : %s\n"
                        " Partition name : %S\n"
                        " Attributes     : %LX %s",
                        gpstr, gtstr, ptstr, info.Name, info.Attr, attrs);
               if (ptstr) free(ptstr);
            }
         } else {
            dsk_gptdiskinfo info;
            rc = dsk_gptdinfo(disk, &info);
            if (!rc) {
               char guid[40];
               dsk_guidtostr(info.GUID, guid);

               shellprc(VIO_COLOR_LWHITE, " Disk %s: %s (%LX sectors)", prnname,
                  get_sizestr(ssize,dsksize), dsksize);
               shellprn(" Disk GUID      : %s\n"
                        " User space     : %09LX-%09LX\n"
                        " Main header    : %09LX\n"
                        " Backup header  : %09LX",
                        guid, info.UserFirst, info.UserLast, info.Hdr1Pos,
                        info.Hdr2Pos);
            }
         }
      } else
      if (query && args->count>=2) { // "QUERY" and "QTYPE"
         // GPT QUERY/QTYPE "name" [diskname [partname [disk]]]^
         u8t     gbuf[16];
         u32t     idx;
         int  dskmode = subcmd==4 && args->count<=3;

         if (!dsk_strtoguid(args->item[1], gbuf)) {
            printf("Incorrect GUID string format!\n");
            return EINVAL;
         }
         if (dskmode)
            rc = dsk_gptfind(gbuf, GPTN_DISK, (u32t*)&disk, 0);
         /* continue searching if command is "GPT QUERY guid", i.e.
            query OR disk OR partition by GUID */
         if (rc && (!dskmode || args->count!=3)) {
            rc = dsk_gptfind(gbuf, subcmd==6?GPTN_PARTTYPE:GPTN_PARTITION,
               (u32t*)&disk, &idx);
            dskmode = 0;
         }

         if (!rc) {
            char buf[16];
            dsk_disktostr(disk,buf);

            if (args->count>2) {
               setenv(args->item[2], buf, 1);
               if (args->count>3) {
                  utoa(idx,buf,10);
                  setenv(args->item[3], buf, 1);
               }
            } else {
               if (dskmode)
                  printf("GUID \"%s\" is disk %s\n", args->item[1], buf);
               else
                  printf("GUID \"%s\" is disk %s partition index %d.\n",
                     args->item[1], buf, idx);
            }
         } else {
            // delete variables if command failed
            if (args->count>2) setenv(args->item[2], 0, 1);
            if (args->count>3) setenv(args->item[3], 0, 1);
         }
      }

      if (rc>0) {
         cmd_shellerr(EMSG_QS, rc, 0);
         return rc==E_DSK_ERRREAD||rc==E_DSK_ERRWRITE?EIO:ENODEV;
      }
      if (!rc) return 0;
      break;
   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
   return EINVAL;
}

