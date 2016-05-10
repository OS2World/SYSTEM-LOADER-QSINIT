//
// QSINIT
// sector-oriented i/o cache module
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
#include "qcl/qscache.h"
#include "cache.h"

const char *selfname = MODNAME_CACHE;

#define shellprc(col,x,...)  cmd_printseq(x,0,col,__VA_ARGS__)
#define shellprn(x,...)      cmd_printseq(x,0,0,__VA_ARGS__)
#define shellprt(x)          cmd_printseq(x,0,0)

int                lib_ready = 0; // lib is ready
u32t                 classid = 0; // class id

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
      if (stricmp(fp,"DBLEVEL")==0 && args->count==2) {
         dblevel = atoi(args->item[1]);
         return 0;
      } else
      if (stricmp(fp,"INFO")==0) {
         u32t hdds, fdds, ii;
         static const char *msg_str[3] = {"impossible", "off", "on"};

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
            int    on = cc_enable(0, disk, -1);
            char pbuf[80];
            if (on>0) {
               u32t  bcntp, bcnt = cc_stat(0, disk, &bcntp);

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
               cc_enable(0, dsk, *fp=='+');
               continue;
            }
            fp=0; break;
         }
         if (fp) return 0;
      } else
      if (cc_setsize_str(0,fp)) return 0;
   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
   return EINVAL;
}

static void *methods_list[] = { cc_setsize, cc_setsize_str, cc_setprio,
   cc_enable, cc_invalidate, cc_invalidate_vol, cc_stat};

// data is not used and no any signature checks in this class
typedef struct {
   u32t     reserved;
} cc_data;

u32t _std cc_setsize_str(void *data, const char *size) {
   if (stricmp(size,"OFF")==0) { cc_setsize(data, 0); return 1; }
      else
   if (isdigit(size[0])) {
      char *errptr = 0;
      u32t value = strtoul(size, &errptr, 0);
      if (*errptr=='%') {
         u32t availmax;
         if (value>50) value = 50;
         hlp_memavail(&availmax,0);
         cc_setsize(data, (availmax>>20) * value /100);
      } else
         cc_setsize(data, value);
      // do not make noise to screen, because can be called from partmgr init
      return 1;
   }
   return 0;
}

static void _std cc_init(void *instance, void *data) {
   cc_data *cpd  = (cc_data*)data;
   cpd->reserved = 0xCC;
}

static void _std cc_done(void *instance, void *data) {
   cc_data *cpd  = (cc_data*)data;
   cpd->reserved = 0;
}

// shutdown handler
void on_exit(void) {
   if (!lib_ready) return;
   lib_ready = 0;
   unload_all();
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(selfname, MODQ_LOADED|MODQ_NOINCR)) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("%s already loaded!\n", selfname);
         vio_setcolor(VIO_COLOR_RESET);
         return 0;
      }
      classid = exi_register("qs_cachectrl", methods_list,
         sizeof(methods_list)/sizeof(void*), sizeof(cc_data),
            cc_init, cc_done, 0);
      if (!classid) {
         log_printf("unable to register class!\n");
         return 0;
      }
      exit_handler(&on_exit, 1);
      cmd_shelladd("CACHE", shl_cache);

      lib_ready = 1;
   } else {
      if (classid)
         if (exi_unregister(classid)) classid = 0;
      // DENY unload if class was not unregistered
      if (classid) return 0;

      cmd_shellrmv("CACHE",shl_cache);
      exit_handler(&on_exit,0);
      on_exit();
   }
   return 1;
}
