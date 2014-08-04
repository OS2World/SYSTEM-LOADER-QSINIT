//
// QSINIT
// screen support dll
//
#include "stdlib.h"
#include "qsutil.h"
#include "qslog.h"
#include "qsshell.h"
#include "errno.h"
#include "qsmod.h"
#include "qsint.h"
#include "qsdm.h"
#include "vio.h"
#include "qscache.h"

const char *selfname = MODNAME_CACHE;

#define shellprc(col,x,...)  cmd_printseq(x,0,col,__VA_ARGS__)
#define shellprn(x,...)      cmd_printseq(x,0,0,__VA_ARGS__)
#define shellprt(x)          cmd_printseq(x,0,0)

int lib_ready =  0; // lib is ready

extern cache_extptr exptable;
extern u32t     blocks_total,
                  free_total;
extern u64t       cache_hits,
                 cache_total;
extern int           dblevel;

void unload_all(void);

u32t _std shl_cache(const char *cmd, str_list *args) {
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   if (args->count>0) {
      char *fp = args->item[0];
      if (stricmp(fp,"OFF")==0) { hlp_cachesize(0); return 0; }
         else
      if (stricmp(fp,"DBLEVEL")==0 && args->count==2) {
         dblevel = atoi(args->item[1]);
         return 0;
      } else
      if (stricmp(fp,"INFO")==0) {
         u32t hdds, fdds, ii;
         static const char *msg_str[3] = {"not possible", "off", "on"};

         printf(" Cache state: ");
         shellprc(blocks_total?VIO_COLOR_LGREEN:VIO_COLOR_LRED,"%s",
            blocks_total?"ON":"OFF");
         if (blocks_total) {
            shellprn("Memory total: %d Mb, used: %d%%, hits %d.%02d%%",
               blocks_total>>5, (blocks_total-free_total)*100/blocks_total,
                  cache_total?(u32t)(cache_hits*100/cache_total):0,
                     cache_total?(u32t)(cache_hits*10000/cache_total%100):0);
         }
         shellprt("");

         hdds = hlp_diskcount(&fdds);
         for (ii=0; ii<fdds+hdds; ii++) {
            u32t disk = ii<fdds ? ii|QDSK_FLOPPY : ii-fdds;
            int    on = hlp_cacheon(disk, -1);
            char pbuf[80];
            if (on>0) {
               u32t  bcntp, bcnt = hlp_cachestat(disk, &bcntp);

               sprintf(pbuf, "(used/with priority: %u/%u)", bcnt, bcntp);
            } else pbuf[0] = 0;

            shellprn("%s %-3d: %s  %s", ii<fdds?"fdd":"hdd", disk&QDSK_DISKMASK,
               msg_str[on+1], pbuf);
         }
         return 0;
      } else
      if (fp[0]=='-' || fp[0]=='+') {
         u32t ii;
         for (ii=0; ii<args->count; ii++) {
            fp = args->item[ii];
            if (*fp=='-' || *fp=='+') {
               u32t dsk = dsk_strtodisk(fp+1);
               if (dsk==FFFF) break;
               hlp_cacheon(dsk, *fp=='+');
               continue;
            }
            fp=0; break;
         }
         if (fp) return 0;
      } else
      if (isdigit(fp[0])) {
         char *errptr = 0;
         u32t value = strtoul(fp, &errptr, 0);
         if (*errptr=='%') {
            u32t availmax;
            if (value>50) value = 50;
            hlp_memavail(&availmax,0);
            hlp_cachesize((availmax>>20) * value /100);
         } else
            hlp_cachesize(value);
         // do not make noise to screen, because can be called from partmgr init
         return 0;
      }
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}

void on_exit(void) {
   if (!lib_ready) return;
   lib_ready = 0;
   unload_all();
   cmd_shellrmv("CACHE",shl_cache);
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("%s already loaded!\n",selfname);
         vio_setcolor(VIO_COLOR_RESET);
         return 0;
      }
      exit_handler(&on_exit,1);
      cmd_shelladd("CACHE",shl_cache);
      lib_ready = 1;
   } else {
      exit_handler(&on_exit,0);
      on_exit();
   }
   return 1;
}
