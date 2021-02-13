#include "vdproc.h"
#include "errno.h"
#include "stdarg.h"
#include "vioext.h"
#include "qsmodext.h"

const char     *CMDNAME = "RAMDISK";
static int   lopt_quiet = 0;  ///< last RAMDISK command has /q option on

static void errmsg(const char *msg,...) {
   va_list al;
   va_start(al,msg);
   if (!lopt_quiet) vprintf(msg, al);
   log_vprintf(msg,al);
   va_end(al);
}

// abort entering on -1 if ESC was pressed
static int _std esc_cb(u16t key, key_strcbinfo *inputdata) {
   char cch = toupper(key&0xFF);
   if (cch==27) return -1;
   return 0;
}

static int ask_yes(void) {
   char *str;
   vio_setcolor(VIO_COLOR_LRED);
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

char *getsizestr(u32t sectsize, u32t disksize, int trim) {
   static char buffer[64];
   static char *suffix[] = { "kb", "mb", "gb", "tb" };

   u64t size = (u64t)disksize * (sectsize?sectsize:1);
   int   idx = 0;
   size    >>= 10;
   while (size>=100000) { size>>=10; idx++; }
   sprintf(buffer, trim?"%d %s":"%8d %s", (u32t)size, suffix[idx]);
   return buffer;
}

void _std percent_prn(u32t percent,u32t size) {
   if (percent) 
      printf("\rLoading ... %02d%% ",percent); else
   if ((size & _1MB-1)==0)
      printf("\rLoading ... %d Mb ",size>>20);
}

u32t _std shl_ramdisk(const char *cmd, str_list *args) {
   u32t rc = EINVAL;
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);
   // drop previous quiet flag
   lopt_quiet = 0;

   if (args->count==1 && (stricmp(args->item[0],"DELETE")==0 ||
      stricmp(args->item[0],"INFO")==0 || stricmp(args->item[0],"CLEAN")==0)) 
   {
      u32t      dh, sectors = 0;
      char     key = toupper(args->item[0][0]);
      qs_vdisk vdi = exi_createid(classid_vd, 0);

      if (key=='C') {
         u32t res = vdi->clean();
         switch (res) {
            case E_SYS_INITED      : printf("Unable to perform while RAM disk is in use!\n"); break;
            case E_SYS_UNSUPPORTED : printf("No PAE on this CPU!\n"); break;
            case E_SYS_NOMEM       : printf("No memory above 4Gb!\n"); break;
            case E_SYS_NONINITOBJ  : printf("No ram disk found!\n"); break;
            case 0: break;
            default:
               cmd_shellerr(EMSG_QS, res, 0);
         }
      } else
      if (vdi->query(0,&dh,&sectors,0,0)) {
         printf("There is no RAM Disk available\n");
      } else {
         vio_setcolor(VIO_COLOR_LWHITE);
         printf(key=='I'?"Disk %s is available - %s\n":"Delete RAM Disk %s - %s?\n",
            dsk_disktostr(dh,0), getsizestr(512,sectors,1));
         // delete action
         if (key=='D')
            if (ask_yes())
               if (!vdi->free()) printf("Unspecified error occured!\n");
      }
      DELETE(vdi);
      return 0;
   } else {
      u32t  minsize=0, maxsize=0, nolow=0, nohigh=0, nofmt=0, nopt=0, nofat32=0,
             divpos=0, fat32=0, ii, dh = 0, hpfs=0, load = 0, persist = 0,
             exact = 0, gpt = 0;
      char     ltr1=0, ltr2=0, *vhdname = 0;
      int   divpcnt=0;   // divide pos in percents

      if (args->count>0) {
         for (ii=0; ii<args->count; ii++) {
            if (strnicmp(args->item[ii],"min=",4)==0 || strnicmp(args->item[ii],"size=",5)==0) {
               char *vpos = strchr(args->item[ii],'=')+1;
               exact = toupper(args->item[ii][0])=='S';
               if (*vpos!='*' || !exact) {
                  minsize = strtoul(vpos, &vpos, 10);
                  if (*vpos) {
                     errmsg("Invalid %s value!\n", exact?"SIZE":"MIN");
                     break;
                  }
               }
            } else
            if (strnicmp(args->item[ii],"max=",4)==0 || strnicmp(args->item[ii],"maxhigh=",8)==0) {
               char *vpos = strchr(args->item[ii],'=')+1;
               maxsize = strtoul(vpos, &vpos, 10);
               // assume max=0 as nohigh
               if (!maxsize && *vpos==0) nohigh = 1; else
               if (*vpos) {
                  errmsg("Invalid MAXHIGH value!\n");
                  break;
               }
            } else
            if (stricmp(args->item[ii],"nolow")==0) nolow = 1;
               else
            if (stricmp(args->item[ii],"nohigh")==0) nohigh = 1;
               else
            if (stricmp(args->item[ii],"/q")==0) lopt_quiet = 1;
               else
            if (stricmp(args->item[ii],"nofmt")==0) nofmt = 1;
               else
            if (stricmp(args->item[ii],"nofat32")==0) nofat32 = 1;
               else
            if (stricmp(args->item[ii],"fat32")==0) fat32 = 1;
               else
            if (stricmp(args->item[ii],"hpfs")==0) hpfs = 1;
               else
            if (stricmp(args->item[ii],"nopt")==0) nopt = 1;
               else
            if (stricmp(args->item[ii],"gpt")==0) gpt = 1;
               else
            if (stricmp(args->item[ii],"persist")==0) persist = 1;
               else
            if (stricmp(args->item[ii],"load")==0) {
               if (ii==args->count-1) {
                  errmsg("Mising LOAD file name!\n");
                  break;
               }
               vhdname = args->item[++ii];
               load    = 1;
            } else
            if (strnicmp(args->item[ii],"div=",4)==0) {
               char *cp=0;
               divpos = strtoul(args->item[ii]+4,&cp,0);
               if (cp)
                  if (cp[0]=='%') divpcnt = 1; else 
                  if (cp[0]) {
                     errmsg("Invalid DIV value!\n");
                     break;
                  }
            } else
            if (ltr1 && ltr2) {
               errmsg("Too many drive letters!\n");
               break;
            } else {
               char ltr = toupper(args->item[ii][0]);
               if (args->item[ii][2]!=0 || args->item[ii][1]!=':' || ltr!='*' && !isalpha(ltr)) {
                  errmsg("Invalid drive letter (%s)\n", args->item[ii]);
                  break;
               }
               if (!ltr1) ltr1 = ltr; else ltr2 = ltr;
            }
            if (nopt && gpt) { errmsg("Both NOPT and GPT keys are used!\n"); break; }
            if (nopt && divpos) { errmsg("Both NOPT and DIV keys are used!\n"); break; }
            if (nolow && nohigh) { errmsg("Both NOLOW and NOHIGH keys are used!\n"); break; }
            if (nofat32 && fat32) { errmsg("Both NOFAT32 and FAT32 keys are used!\n"); break; }
            if (hpfs && fat32) { errmsg("Both HPFS and FAT32 keys are used!\n"); break; }
            if (load && (minsize || maxsize || fat32 || hpfs || divpos || ltr1)) {
               errmsg("Image LOAD can be combined with NOLOW/NOHIGH only!\n");
               break;
            }
            if (divpos && !divpcnt && minsize && divpos>=minsize) {
               errmsg("Invalid values: DIV > %s!\n", exact?"SIZE":"MIN");
               break;
            }
            // ramdisk max=700 nolow z: q: div=30% nofmt
            if (ii==args->count-1) rc = 0;
         }
         // error was shown
         if (rc) return rc;
      }
      if (1) {
         qs_vdisk    vdi = 0;
         qserr       res = 0;
         int  persist_ok = 0;
      
         ii = 0;
         if (divpos) { 
            ii|=VFDF_SPLIT;
            if (divpcnt) ii|=VFDF_PERCENT;
         }
         if (nolow) ii|=VFDF_NOLOW; else
            if (nohigh) { ii|=VFDF_NOHIGH; maxsize=0; }
         
         vdi = exi_createid(classid_vd, 0);
         
         if (load)    {
            res = vdi->load(vhdname, ii, &dh, lopt_quiet?0:percent_prn);
            // empty line
            if (!lopt_quiet) printf("\r                     \r");
         } else {
            if (minsize && exact) ii|=VFDF_EXACTSIZE;
            if (persist) ii|=VFDF_PERSIST;
            if (gpt)     ii|=VFDF_GPT;

            if (nopt )   ii|=VFDF_EMPTY; else
            if (nofmt)   ii|=VFDF_NOFMT; else
            if (hpfs)    ii|=VFDF_HPFS; else
            if (nofat32) ii|=VFDF_NOFAT32; else
            if (fat32)   ii|=VFDF_FAT32;
            // create disk
            res = vdi->init(minsize, maxsize, ii, divpos, ltr1, ltr2, &dh);
            if (res==E_SYS_EXIST) { res = 0; persist_ok = 1; }
         }
         
         if (res==0) {
            if (!lopt_quiet) {
               u32t sectors = 0;
               vdi->query(0,0,&sectors,0,0);
         
               printf("Disk %s %s - %s\n", dsk_disktostr(dh,0),
                  persist_ok?"found":"created", getsizestr(512,sectors,1));
            }
         } else
         if (!lopt_quiet) {
            switch (res) {
               case E_SYS_UNSUPPORTED : printf("No PAE on this CPU!\n"); break;
               case E_SYS_INITED      : printf("Disk already exist!\n"); break;
               case E_SYS_NOMEM       :
                  printf("There is no memory for disk with specified parameters!\n");
                  break;
               case E_SYS_NOFILE      : printf("LOAD image file is not exist!\n"); break;
               case E_SYS_BADFMT      : printf("Disk image is not suitable!\n"); break;
               case E_SYS_TOOSMALL    : printf("Disk image too small!\n"); break;
               case E_DSK_ERRREAD     : printf("I/O error while reading disk image!\n"); break;
               default:
                  cmd_shellerr(EMSG_QS, res, 0);
            }
         }
         DELETE(vdi);
         rc = qserr2errno(res);
      }
      return rc;
   }
   cmd_shellerr(EMSG_CLIB, EINVAL, 0);
   return EINVAL;
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         printf("Module \"%s\" already loaded!\n", mod_getname(hmod,0));
         return 0;
      }
      if (hlp_hosttype()==QSHT_EFI) {
         log_printf("Not supported EFI host!\n");
         return 0;
      }
      // register classes
      if (!register_class()) {
         log_printf("Unable to register class!\n");
         return 0;
      }
      // install shell command
      cmd_shelladd(CMDNAME, shl_ramdisk);
   } else {
      // unregister class(es), cancel unloading if failed
      if (!unregister_class()) return 0;
      // remove shell command
      cmd_shellrmv(CMDNAME, shl_ramdisk);
      /* remove disk and clean memory, but only on module unloading,
         QSINIT shutdown will leave disk memory as it is */
      vdisk_free(0,0);
   }
   return 1;
}
