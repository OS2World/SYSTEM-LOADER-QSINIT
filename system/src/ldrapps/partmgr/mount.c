//
// QSINIT
// partition management dll
//
#include "stdlib.h"
#include "qslog.h"
#include "qsdm.h"
#include "errno.h"
#include "vioext.h"
#include "pscan.h"
#include "qsint.h"
#include "direct.h"

// is key present? (with argument or not)
#define key_present(kparm,key) key_present_pos(kparm,key,0)

// is key present? for subsequent search of the same parameter
char *key_present_pos(str_list *kparm, const char *key, u32t *pos) {
   u32t ii=pos?*pos:0, len = strlen(key);
   if (!kparm||ii>=kparm->count) return 0;
   for (;ii<kparm->count;ii++) {
      u32t plen = strlen(kparm->item[ii]);
      if (plen>len+1 && kparm->item[ii][len]=='=' || plen==len)
         if (strnicmp(kparm->item[ii],key,len)==0) {
            char *rc=kparm->item[ii]+len;
            if (*rc=='=') rc++;
            if (pos) *pos=ii;
            return rc;
         }
   }
   return 0;
}

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

void common_errmsg(const char *mask, const char *altmsg, u32t rc) {
   char hkey[16], *hmsg;
   sprintf(hkey, mask, rc);
   printf("\r");
   hmsg = cmd_shellgetmsg(hkey);
   if (hmsg) cmd_printtext(hmsg,0,0,0); else
      printf("%s, code = %d\n", altmsg, rc);
   if (hmsg) free(hmsg);
}

/// "mount" command
u32t _std shl_mount(const char *cmd, str_list *args) {
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   if (args->count>0) {
      static char *argstr   = "list|/v";
      static short argval[] = { 1,   1};
      int       list = 0, verbose = 0;
      u32t         ii, vt[DISK_COUNT];
      disk_volume_data vi[DISK_COUNT];
      for (ii=0; ii<DISK_COUNT; ii++) vt[ii] = hlp_volinfo(ii, vi+ii);

      str_parseargs(args, argstr, argval, &list, &verbose);

      if (list) {
         static char *fattype[4] = { "uncknown", "  FAT12 ", "  FAT16 ", "  FAT32 " };

         cmd_printseq(0,1,0);

         for (ii=0; ii<DISK_COUNT; ii++)
            if (vi[ii].TotalSectors) {
               char  str[128];
               u32t  dsk = FFFF;
               long  idx = dsk_volindex(ii,&dsk);
               str[0] = 0; 

               if (verbose) {
                  char *dn = dsk_disktostr(vi[ii].Disk,0), idxstr[8];
                  itoa(idx, idxstr, 10);

                  snprintf(str, 128, "(%08X) %-4s at %09LX, partition %s",
                     vi[ii].TotalSectors, dn, vi[ii].StartSector,
                        idx<0 ? "not matched" : idxstr);
               } else {
                  if (idx>=0) {
                     char *dn = dsk_disktostr(dsk,0);
                     if (dn) sprintf(str," at %s.%d ",dn,idx);
                  }
               }
               if (shellprn(" %c:/%c: %s %s %s",'0'+ii,'A'+ii, fattype[vt[ii]],
                  get_sizestr(vi[ii].SectorSize, vi[ii].TotalSectors), str))
                     return EINTR;
            }
         return 0;
      }
      if (args->count>=2) {
         u32t   vol = get_disk_number(args->item[0]),
               disk = get_disk_number(args->item[1]);
         u64t start, size;
         long   idx = args->count>2?atoi(args->item[2]):-1,
                fdd = (disk&QDSK_FLOPPY)!=0;
         char prnname[8];
         dsk_disktostr(disk,prnname);

         if (args->count>2 && !isdigit(args->item[2][0])) {
            printf("Invalid partition index!\n");
            return EINVAL;
         }
         if ((long)vol<0 || (vol&QDSK_VIRTUAL)==0) {
            printf("Invalid volume name specified!\n");
            return EINVAL;
         } else
         if (vol<=QDSK_VIRTUAL+DISK_LDR) {
            printf("Unable to re-mount 0: and 1: volumes!\n");
            return EINVAL;
         }
         if ((long)disk<0 || (disk&QDSK_VIRTUAL)!=0) {
            printf("Invalid disk name specified!\n");
            return EINVAL;
         }
         // check for LM_CACHE_ON_MOUNT and load cache if asked
         cache_envstart();
         // re-scan disk
         dsk_ptrescan(disk,1);
         // mount as floppy
         if (idx<0) {
            u32t     st = dsk_sectortype(disk,0,0);
            int success = st==DSKST_BOOTFAT || st==DSKST_BOOTBPB || st==DSKST_BOOT
               || fdd && (st==DSKST_EMPTY || st==DSKST_DATA);

            if (!success) {
                if (!fdd && (st==DSKST_EMPTY || st==DSKST_DATA)) {
                   vio_setcolor(VIO_COLOR_LRED);
                   printf("Do you really want to mount HARD DRIVE %s as big floppy?\n",
                      prnname);
                   if (ask_yes(VIO_COLOR_RESET)) success=1; else
                      return EINTR;
                }
                if (!success)
                   if (fdd && st==DSKST_ERROR) printf("No diskette in drive %s\n",
                      prnname);
                   else
                      printf("Disk %s is not floppy formatted!\n", prnname);
            }
            if (!success) return EACCES;
            start = 0;
            size  = hlp_disksize64(disk,0);
         }
         // or hard disk partition
         if (idx>=0 && !dsk_ptquery64(disk,idx,&start,&size,0,0)) {
            printf("There is no partition %d in disk %s!\n", idx, prnname);
            return EINVAL;
         } else {
            for (ii=0; ii<DISK_COUNT; ii++)
               if (vi[ii].StartSector && vi[ii].Disk==disk &&
                  vi[ii].StartSector==start)
               {
                  printf("Already mounted as %c:\n", '0'+ii);
                  return 0;
               }
            // mount
            if (hlp_mountvol(vol&QDSK_DISKMASK, disk, start, size)) {
               printf("Mounted as %c:\n", '0'+(vol&QDSK_DISKMASK));
            } else {
               printf("Failed to mount!");
            }
            return 0;
         }
      }
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
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

      if (stricmp(args->item[0],"all")==0) {
         for (ii=DISK_LDR+1; ii<DISK_COUNT; ii++)
            if (vt[ii]) {
               if (hlp_unmountvol(ii)) {
                  printf("Volume %c: unmounted\n", '0'+ii);
               } else {
                  printf("Failed to unmount %c: !\n", '0'+ii);
               }
            }
         if (hlp_curdisk()>DISK_LDR) hlp_chdisk(DISK_LDR);
         return 0;
      } else {
         u32t rc = 0;
         ii = 0;
         while (ii<args->count) {
            u32t  vol = get_disk_number(args->item[ii]);
            if ((long)vol<0 || (vol&QDSK_VIRTUAL)==0) {
               printf("Invalid volume name specified!\n");
               return EINVAL;
            } else {
               vol&=~QDSK_VIRTUAL;

               if (vol==hlp_curdisk()) hlp_chdisk(DISK_LDR);

               if (hlp_unmountvol(vol)) {
                  printf("Volume %c: unmounted\n", '0'+vol);
               } else {
                  printf("Failed to unmount %c:\n", '0'+vol);
                  rc = ENOMNT;
               }
            }
            ii++;
         }
         return rc;
      }

   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}


u32t _std shl_dm_mbr(const char *cmd, str_list *args, u32t disk, u32t pos) {
   // no subcommand?
   if (args->count<=pos) return EINVAL;
   // check disk id
   if (disk&QDSK_FLOPPY) {
      printf("This is a floppy disk! \n");
      return EINVAL;
   } else
   if ((disk&QDSK_VIRTUAL)!=0) {
      printf("This is volume, not a physical disk! \n");
      return EINVAL;
   } else {
      char             prnname[8], *sizestr = "";
      disk_geo_data      gdata;
      static char *subcommands[5] = {"WIPE", "CODE", "BOOT", "ACTIVE", 0};
      int subcmd = get_arg_index(args->item[pos],subcommands);

      if (subcmd<0) return EINVAL;
      dsk_disktostr(disk,prnname);

      if (subcmd<=1)
         if (hlp_disksize(disk,0,&gdata)) {
            sizestr = get_sizestr(gdata.SectorSize, gdata.TotalSectors);
            while (*sizestr==' ') sizestr++;
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
      if (subcmd==2) { // "BOOT"
         u32t rc, force = 0;
         // "FORCE" must be the last in command
         if (++pos<args->count)
            if (stricmp(args->item[args->count-1], "FORCE")==0) {
               force = 1;
               args->count--;
            }
         if (args->count<=pos || !isdigit(args->item[pos][0])) {
            rc = exit_bootmbr(disk, force?EMBR_NOACTIVE:0);
            switch (rc) {
               case EINVAL:
                  printf("Invalid disk specified: %s!\n",prnname);
                  break;
               case EIO   :
                  printf("Unable to read MBR from disk %s!\n",prnname);
                  break;
               case ENODEV:
                  printf("There is no active partition on disk %s!\n",prnname);
                  break;
               case ENOTBLK:
                  printf("Unable to boot from emulated disk %s!\n",prnname);
                  break;
               default:
                  printf("Error launching MBR code from disk %s!\n",prnname);
                  break;
            }
         } else {
            u32t  index = strtoul(args->item[pos++],0,0);
            char letter = 0;

            if (force) printf("FORCE option is for MBR boot only!\n");

            if (args->count>pos && isalpha(args->item[pos][0])) {
               if (args->item[pos][1]!=':' || args->item[pos][2]) {
                  printf("Invalid drive letter parameter!\n");
                  return EINVAL;
               }
               letter = toupper(args->item[pos][0]);
            }
            dsk_ptrescan(disk,0);
            rc = exit_bootvbr(disk, index, letter);
            switch (rc) {
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
                  printf("Partition or a part of it lies outsize of 2TB!\n");
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
            hdd_info *hi = get_by_disk(disk);
            u32t   index = strtoul(args->item[pos],0,0);
            int  puregpt = 0;

            if (!hi) {
               printf("There is no disk %s!\n", prnname); return ENOMNT;
            }
            /* rescan on first access (and it will be rescanned again in
               dsk_setactive()) */
            dsk_ptrescan(disk,0);
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
               u32t rc = puregpt ? dsk_gptactive(disk, index) : dsk_setactive(disk, index);

               if (rc) common_errmsg("_DPTE%02d", "Set active error", rc);
               return rc ? EACCES : 0;
            }
         }
      }
      return 0;
   }
}

u32t _std shl_dm_bootrec(const char *cmd, str_list *args, u32t disk, u32t pos) {
   u32t  sttype;
   // no subcommand?
   if (args->count<=pos) return EINVAL;
   // current sector type
   sttype = dsk_sectortype(disk,0,0);
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
      static char *subcommands[6] = {"WIPE", "CODE", "DEBUG", "BPB", 0};
      int subcmd = get_arg_index(args->item[pos++],subcommands);

      if (subcmd<0) return EINVAL;
      dsk_disktostr(disk,prnname);

      if (subcmd==0) { // "WIPE"
         if (sttype==DSKST_EMPTY) {
            printf("Boot sector on disk %s is empty\n",prnname);
            return 0;
         }
         if (disk==QDSK_VIRTUAL) {
            printf("This is your boot partition! \n");
            return EINVAL;
         }
         if (disk==QDSK_VIRTUAL+DISK_LDR) {
            printf("This is a virtual disk! \n");
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
         if (disk==QDSK_VIRTUAL) {
            if (hlp_volinfo(0,0)==FST_NOTMOUNTED) {
               printf("This is non-FAT boot partition! \n");
               return EINVAL;
            }
            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you really want to replace BOOT disk code?\n");
         } else {
            if (sttype!=DSKST_BOOTFAT) {
               printf("This is not a FAT boot sector! \n");
               return EACCES;
            }
            vio_setcolor(VIO_COLOR_LRED);
            printf("Replacing BOOT code on disk %s?\n",prnname);
         }
         // saving...
         if (ask_yes(VIO_COLOR_RESET)) {
            int rc = dsk_newvbr(disk,0,FST_NOTMOUNTED,
               args->count==pos+1?args->item[pos]:0);
            printf(rc?"done!\n":"unable to write!\n");
            return rc?0:EACCES;
         } else
            return EINTR;
      } else
      if (subcmd==2) { // "DEBUG"
         if (disk==QDSK_VIRTUAL) {
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
         if (rc && rc!=EINTR) cmd_shellerr(EIO,0);
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

         if (hlp_disksize(ii,0,&gdata)) {
            stext = get_sizestr(gdata.SectorSize, gdata.TotalSectors);
         } else {
            stext = "    no info";
         }
         shellprn(" %HDD %i =>%-4s: %s   %s", ii, dsk_disktostr(ii,0), stext, mbuf);
      }
   } else {
      char buf[32];
      u32t prevmode = hlp_diskmode(disk,HDM_QUERY), mode = prevmode;
      dsk_disktostr(disk,buf);

      if (prevmode&HDM_EMULATED) 
         shellprn("Disk %s is emulated and have no access mode", buf);
      else
      if (prevmode) {
         if (args->count>pos) {
            int success = 1;
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
      static char *subcommands[] = {"MBR", "BOOT", "LIST", "MODE", "BS", "PM", 0};
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
         // use str2long to prevent octal conversion
         disk = get_disk_number(args->item[1]);
         if (disk<0) {
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
            return shl_dm_list(cmd, args, disk, 2);
         case 3:
            return shl_dm_mode(cmd, args, disk, 2);
         case 5:
            return shl_pm(cmd, args, disk, 2);
      }
      break;
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}

static void _std fmt_callback(u32t percent, u32t readed) {
   printf("\rFormatting %02d%%",percent);
}

u32t _std shl_format(const char *cmd, str_list *args) {
   u32t rc = 0;
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   while (args->count>0) {
      int  ii, quick = 0, wipe = 0, noaf = 0, asz = 0, fatcpn = 2, quiet = 0;
      u32t   disk = 0;
      char *szstr = 0;
      disk_volume_data vi;

      for (ii=0; ii<args->count && !rc; ii++) {
         if (args->item[ii][0]=='/') {
            switch (toupper(args->item[ii][1])) {
               case 'A':if (args->item[ii][2]==':') {
                           char *ep = 0;
                           asz = strtoul(args->item[ii]+3, &ep, 0);
                           if (asz<512)
                              if (toupper(*ep)=='K') asz*=1024; else rc=EINVAL;
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
                        } else rc=EINVAL;
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
      if ((disk&QDSK_VIRTUAL)==0) {
         printf("Invalid volume name specified!\n");
         return EINVAL;
      }
      disk &= QDSK_DISKMASK;

      if (disk<=DISK_LDR) {
         printf("Unable to format 0: and 1: volumes!\n");
         return EINVAL;
      }
      hlp_volinfo(disk, &vi);
      // only emulated hdds can be formatted quietly
      if (quiet && (hlp_diskmode(vi.Disk,HDM_QUERY)&HDM_EMULATED)==0) {
         printf("Quiet format not possible for real disks!\n");
         return EINVAL;
      }
      // buffer is static!
      szstr = get_sizestr(vi.SectorSize, vi.TotalSectors);
      while (*szstr==' ') szstr++;

      if (!quiet) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("Do you really want to format volume %s (%s)?\n"
            "All data will be lost!\n", dsk_disktostr(disk|QDSK_VIRTUAL,0), szstr);
      }
      if (quiet || ask_yes(VIO_COLOR_RESET)) {
         u32t flags = DFMT_BREAK;
         if (quick) flags|=DFMT_QUICK;
         if (fatcpn==1) flags|=DFMT_ONEFAT;
         if (wipe) flags|=DFMT_WIPE;
         if (noaf) flags|=DFMT_NOALIGN;

         rc = dsk_format(disk, flags, asz, quiet?0:fmt_callback);
         if (rc) {
            common_errmsg("_FMT%02d", "Format error", rc);
            rc = ENOMNT;
         } else
         if (!quiet) {
            static const char *ftstr[4] = {"unrecognized", "FAT12", "FAT16", "FAT32"};
            u32t   fstype, clsize;
            diskfree_t df;

            if (_dos_getdiskfree(disk+1,&df)) df.avail_clusters = 0;
            fstype = hlp_volinfo(disk, &vi);
            clsize = vi.ClSize * vi.SectorSize;

            printf("\rFormat complete.     \n");
            printf("\nThe type of file system is %s.\n\n", ftstr[fstype]);
            printf("  %s total disk space\n", get_sizestr(vi.SectorSize, vi.TotalSectors));
            printf("  %s are available\n", get_sizestr(clsize, df.avail_clusters));
            if (vi.BadClusters)
               printf("\n  %s in bad sectors\n", get_sizestr(clsize, vi.BadClusters));

            printf("\n  %8d bytes in each allocation unit.\n", clsize);
            printf("  %8d total allocation units on disk.\n", df.total_clusters);
            printf("  %8d allocation units available on disk.\n", df.avail_clusters);
         }
      }
      return rc;
   }
   cmd_shellerr(!rc?EINVAL:rc,0);
   return !rc?EINVAL:rc;
}

u32t _std shl_lvm(const char *cmd, str_list *args) {
   if (key_present(args,"/?")) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }

   cmd_printseq(0,1,0);

   while (args->count>=2) {
      static char *subcommands[6] = {"WRITE", "ASSIGN", "RENAME", "INFO", "QUERY", 0};
      int  subcmd = get_arg_index(args->item[0],subcommands),
               rc = EINVAL;
      long   disk = FFFF;
      if (subcmd<0) break;

      if (subcmd!=4 || args->count>4) {
         disk = get_disk_number(args->item[subcmd==4?4:1]);
         if (disk<0) {
            printf("Invalid disk name specified!\n");
            return EINVAL;
         }
         if ((disk&QDSK_VIRTUAL)!=0) {
            printf("This is volume, not a physical disk! \n");
            return EINVAL;
         }
      }
      if (subcmd==0) {
         int master_boot = -1;
         rc = lvm_checkinfo(disk);
         // check for non-LVMed huge disk
         if (rc==LVME_NOINFO) {
            u64t size = hlp_disksize64(disk,0);
            if (size>63*255*65535 && dsk_partcnt(disk)>0) {
               printf("Writing LVM info to non-OS/2 formatted huge "
                      "disk (>500Gb) will affect nothing.\n");
               if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
            }
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
            u32t   ssize;
            u64t dsksize = hlp_disksize64(disk,&ssize);

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
               if (dsk_ptquery(disk,index,0,0,0,0,0)) rc = LVME_NOPART; else
                  rc = LVME_INDEX;
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
      }

      if (rc) {
         common_errmsg("_LVME%02d", "LVM error", rc);
         return rc==LVME_IOERROR?EIO:ENODEV;
      }
      return 0;
   }
   cmd_shellerr(EINVAL,0);
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
      dsk_freeblock *fi = (dsk_freeblock*)malloc(sz*sizeof(dsk_freeblock));
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
      if (rc) rc=rc==DPTE_ERRREAD||rc==DPTE_ERRWRITE?EIO:ENODEV; else
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

      if ((disk&QDSK_VIRTUAL)!=0) {
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
                  char *dsstr = get_sizestr(ssize, dsize);
                  while (*dsstr==' ') dsstr++;
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
         if (rc==DPTE_EMPTY) {
            printf("Disk is not empty!\n");
            return ENODEV;
         }
      } else
      if (subcmd==1) { // "FREE"
         dmgr_pm_disk_free(disk);
      } else
      if (subcmd==2) { // "CREATE"
         // dmgr pm hd0 create index type [size [place]]
         // dmgr pm hd0 create 0 p 48000 start
         // dmgr pm hd0 create 0 p 48% start
         u32t fb_index, flags = 0, ptsize = 0;
         u64t   pt_pos, pt_len;
         char  se_char = 'S', *pt;
         int   percent = 0;

         if (args->count<pos+2 || args->count>pos+4) break;
         if (!isdigit(args->item[pos][0])) break;

         fb_index = str2long(args->item[pos]);
         // partition type
         pt = args->item[pos+1];
         if (stricmp(pt,"PRIMARY")==0 || stricmp(pt,"P")==0) flags = DFBA_PRIMARY;
            else
         if (stricmp(pt,"LOGICAL")==0 || stricmp(pt,"L")==0) flags = DFBA_LOGICAL;
            else break;
         // partition size
         if (args->count>=pos+3) {
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
            if (args->count==pos+4) {
               se_char = toupper(args->item[pos+3][0]);
               if (se_char!='S' && se_char!='E') {
                  printf("Position string must be \"s[tart]\" or \"e[nd]\"!\n");
                  return EINVAL;
               }
            }
         }
         rc = dsk_ptalign(disk, fb_index, ptsize, (percent?DPAL_PERCENT:0)|
            DPAL_CHSSTART|DPAL_CHSEND|(se_char=='E'?DPAL_ESPC:0), &pt_pos, &pt_len);
         if (!rc) 
            if (dsk_isgpt(disk,-1) > 0)
               rc = dsk_gptcreate(disk, pt_pos, pt_len, flags|DPAL_AF, 0);
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
            dscp = get_sizestr(ssize, ptsize);
            while (*dscp==' ') dscp++;

            vio_setcolor(VIO_COLOR_LRED);
            printf("Do you really want to delete partition %d on disk %s?\n"
                   "Partition size is %s %s\n", pt_index, prnname, dscp, desc);
            if (!ask_yes(VIO_COLOR_RESET)) return EINTR;
            rc = dsk_ptdel(disk, pt_index, start);
         }
      }
      if (rc) {
         common_errmsg("_DPTE%02d", "PMGR error", rc);
         return rc==DPTE_ERRREAD||rc==DPTE_ERRWRITE?EIO:ENODEV;
      }
      return 0;
   }
   cmd_shellerr(EINVAL,0);
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
         if ((disk&QDSK_VIRTUAL)!=0) {
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
               str_parseargs(args, argstr, argval, &af[0], &af[0], &af[1],
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
         /* searching continue if command have form "GPT QUERY guid", i.e.
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
         common_errmsg("_DPTE%02d", "GPT error", rc);
         return rc==DPTE_ERRREAD||rc==DPTE_ERRWRITE?EIO:ENODEV;
      }
      if (!rc) return 0;
      break;
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}
