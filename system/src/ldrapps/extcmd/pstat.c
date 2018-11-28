#include "vio.h"
#include "qsshell.h"
#include "qstask.h"
#include "qsmodext.h"
#include "qsint.h"
#include "qspdata.h"
#include "stdlib.h"
#include "errno.h"

u32t _std shl_ps(const char *cmd, str_list *args) {
   int  rc=-1, nopause=0, l_thr=0, l_mod=0, l_all=0;
   static char *argstr   = "/t|/m|/a|/np";
   static short argval[] = { 1, 1, 1, 1  };
   // help overrides any other keys
   if (str_findkey(args, "/?", 0)) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   args = str_parseargs(args, 0, 1, argstr, argval, &l_thr, &l_mod, &l_all, &nopause);
   // process command
   if (args->count==0) {
      u32t *pidlist = mod_pidlist(), *pidp = pidlist;

      if (l_all) l_mod=l_thr=1;

      cmd_printseq(0, PRNSEQ_INIT, 0);

      cmd_printseq(" pid    sid    module%s\n"
                   "------ ------ ------------%s", nopause?-1:0, 0,
                   l_thr?"       tid   state    cputime   name":"",
                   l_thr?     " ----- -------- --------- --------":"");

      while (pidp) {
         process_context *pq;
         u32t             pv = *pidp++;
         if (!pv) break;
         pq = mod_pidctx(pv);
         /* function above turns on MT lock in MT mode if result is non-zero.
            so we prints into the string below and flush it after unlock.
            This makes command safe, because it operates with *system* data here */
         if (pq) {
            mt_prcdata *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
            char  sbuf[32], *ln;
            u32t       sno = pd->piList[0]->tiSession;

            sbuf[0] = 0;
            /* print DET in any case, because session 0 in the main thread
               cannot be chacnged, i.e. process fixed to detached */
            if (!sno) strcpy(sbuf,"    DET"); else
               if (!l_thr) snprintf(sbuf, sizeof(sbuf), "%7u", sno);

            ln = sprintf_dyn("%5u%s   %s", pd->piPID, sbuf, pq->self->mod_path);

            if (l_mod) {
               module **ml = &pd->piModule->impmod;
               while (*ml) {
                  char *mn = sprintf_dyn("\n                 %s", (*ml)->mod_path);
                  ln = strcat_dyn(ln, mn);
                  free(mn);
                  ml++;
               }
            }
            if (l_thr) {
               u32t ii;
               for (ii=0; ii<pd->piListAlloc; ii++) {
                  mt_thrdata *th = pd->piList[ii];
                  if (th) {
                     static const char *state[THRD_STATEMAX+1] =
                        { "running", "suspend", "finished", "waiting" };
                     char  thname[20], *ti, thtime[24];
                     thname[0] = 0;
                     thname[1] = 0;
                     if (mt_getthname(th->tiPID, th->tiTID, thname+1)==0)
                        if (thname[1]) {
                           thname[0] = '\"';
                           strcat(thname, "\"");
                        }
                     // uptime conversion
                     if (th->tiTime==0) strcpy(thtime,"no data"); else
                        if (th->tiTime<10) strcpy(thtime,"<1 ms"); else {
                           u64t ms = th->tiTime/10;
                           if (ms<100000) sprintf(thtime, "%Lu ms", ms); else
                              if ((ms/=1000)<10000) sprintf(thtime, "%Lu sec", ms); else
                                 if ((ms/=60)<10000) sprintf(thtime, "%Lu min", ms); else
                                    if ((ms/=60)<10000) sprintf(thtime, ">%Lu hr", ms); else {
                                       ms/=24*365;  // :)
                                       sprintf(thtime, ">%Lu yr", ms);
                                    }
                        }
                     ti = sprintf_dyn("\n     %7u               %4u  %-8s %8s  %s",
                        th->tiSession, th->tiTID, state[th->tiState], thtime, thname);

                     ln = strcat_dyn(ln, ti);
                     free(ti);
                  }
               }
            }
            // unlock MT, locked by mod_pidctx() above
            if (mt_active()) mt_swunlock();

            cmd_printseq(ln, nopause?-1:0, 0);
            free(ln);
         }
      }
      if (pidlist) free(pidlist);

      rc = 0;
   }
   // free args clone, returned by str_parseargs() above
   free(args);
   
   if (rc<0) rc = EINVAL;
   if (rc && rc!=EINTR) cmd_shellerr(EMSG_CLIB,rc,0);
   return 0;
}
