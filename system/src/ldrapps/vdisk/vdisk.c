#include "errno.h"
#include "stdlib.h"
#include "qsshell.h"
#include "stdarg.h"
#include "qsvdisk.h"
#include "qsutil.h"
#include "qsmod.h"
#include "qsdm.h"
#include "vioext.h"
#include "qsmodext.h"

const char   *CMDNAME = "RAMDISK";
static int lopt_quiet = 0;  // last RAMDISK command has /q option on

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
      stricmp(args->item[0],"INFO")==0)) 
   {
      int   info = toupper(args->item[0][0])=='I';
      u32t    dh, sectors = 0;

      if (!sys_vdiskinfo(&dh,&sectors,0)) {
         printf("There is no RAM Disk available\n");
      } else {
         vio_setcolor(VIO_COLOR_LWHITE);
         printf(info?"Disk %s is available - %s\n":"Delete RAM Disk %s - %s?\n",
            dsk_disktostr(dh,0), getsizestr(512,sectors,1));
         // delete action
         if (!info)
            if (ask_yes())
               if (!sys_vdiskfree()) printf("Unspecified error occured!\n");
      }
      return 0;
   } else {
      u32t  minsize=0, maxsize=0, nolow=0, nohigh=0, nofmt=0, nopt=0, nofat32=0,
             divpos=0, fat32=0, ii, dh = 0;
      char     ltr1=0, ltr2=0;
      int   divpcnt=0;   // divide pos in percents

      if (args->count>0) {
         for (ii=0; ii<args->count; ii++) {
            if (strnicmp(args->item[ii],"min=",4)==0)
               minsize = strtoul(args->item[ii]+4,0,10); else
            if (strnicmp(args->item[ii],"max=",4)==0)
               maxsize = strtoul(args->item[ii]+4,0,10); else
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
            if (stricmp(args->item[ii],"nopt")==0) nopt = 1;
               else
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
            if (nopt && divpos) { errmsg("Both NOPT and DIV keys used!\n"); break; }
            if (nolow && nohigh) { errmsg("Both NOLOW and NOHIGH keys used!\n"); break; }
            if (nofat32 && fat32) { errmsg("Both NOFAT32 and FAT32 keys used!\n"); break; }
            if (maxsize && maxsize<minsize) { errmsg("MAX can`t be smaller than MIN!\n"); break; }
      
            if (divpos && !divpcnt && maxsize && divpos>=maxsize) {
               errmsg("Invalid values: DIV > MAX!\n");
               break;
            }
            // ramdisk max=700 nolow z: q: div=30% nofmt
            if (ii==args->count-1) rc = 0;
         }
         // error was shown
         if (rc) return rc;
      }
      ii = 0;
      if (divpos) { 
         ii|=VFDF_SPLIT;
         if (divpcnt) ii|=VFDF_PERCENT;
      }
      if (nolow) ii|=VFDF_NOLOW; else
         if (nohigh) ii|=VFDF_NOHIGH;
      if (nopt )   ii|=VFDF_EMPTY; else
      if (nofmt)   ii|=VFDF_NOFMT; else
      if (nofat32) ii|=VFDF_NOFAT32; else
      if (fat32)   ii|=VFDF_FAT32;
      // create disk
      rc = sys_vdiskinit(minsize, maxsize, ii, divpos, ltr1, ltr2, &dh);
      if (rc==0) {
         if (!lopt_quiet) {
            u32t sectors = 0;
            sys_vdiskinfo(0,&sectors,0);
      
            printf("Disk %s created - %s\n", dsk_disktostr(dh,0),
               getsizestr(512,sectors,1));
         }
      } else
      if (!lopt_quiet)
         if (rc==ENODEV) printf("No PAE on this CPU!\n");
            else
         if (rc==EEXIST) printf("Disk already exist!\n");
            else
         if (rc==ENOMEM) printf("There is no memory for disk with specified parameters!\n");
            else cmd_shellerr(rc, 0);
      return rc;
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("Module \"%s\" already loaded!\n", mod_getname(hmod,0));
         vio_setcolor(VIO_COLOR_RESET);
         return 0;
      }
      // install shell command
      cmd_shelladd(CMDNAME, shl_ramdisk);
   } else {
      // remove shell command
      cmd_shellrmv(CMDNAME, shl_ramdisk);
      /* remove disk and clean memory, but only on module unloading,
         QSINIT shutdown will leave disk memory as it is */
      sys_vdiskfree();
   }
   return 1;
}
