//
// QSINIT "start" module
// C library signal functions
//
#include "stdlib.h"
#include "signal.h"
#include "syslocal.h"
#include "sysio.h"
#include "qsxcpt.h"
#include "errno.h"

#define SIGN_INFO   0x47495354   // TSIG

typedef struct {
   u32t         sign;            // check signature
   __sig_function fn[SIGUSR3+1]; // signal() functions
} sig_state;

static int mtcb_on = 0;          // callback on MT start is installed

// QS <-> clib signal value match
static const u32t cvt_matrix[SIGUSR3+1] = { 0, 0, QMSV_BREAK, 0, 0, 0,
   QMSV_CLIB, 0, 0, QMSV_KILL, 0, 0, 0, 0, QMSV_ALARM, 0, QMSV_CLIB+1,
   QMSV_CLIB+2, QMSV_CLIB+3 };

static u32t sig_convert(int sig) {
   return sig>0 && sig<=SIGUSR3 ? cvt_matrix[sig] : 0;
}

static int  sig_cvtback(u32t sig) {
   u32t *pos = memchrd(cvt_matrix, sig, SIGUSR3+1);
   return pos ? pos - cvt_matrix : 0;
}

static int seg_send(int pid, u32t tid, int sig) {
   qserr  res = 0;
   u32t  nsig = 0;

   if (sig)
      if ((nsig=sig_convert(sig))==0) res = E_SYS_INVPARM;

   if (!res) {
      qs_mtlib mt = get_mtlib();
      // this will make ENOSYS errno
      if (!mt) res = E_MT_DISABLED; else
         if (pid>=0) res = mt->sendsignal(0, tid, nsig, pid==0?QMSF_TREE:0); else
            if (pid==-1) res = mt->sendsignal(1, tid, nsig, QMSF_TREEWOPARENT);
               else res = mt->sendsignal(-pid, tid, nsig, QMSF_TREE);
   }
   
   if (res) set_errno(qserr2errno(res));
   return res?-1:0;
}

int _std tkill(u32t tid, int sig) {
   return seg_send(0, tid, sig);
}

int _std kill(int pid, int sig) {
   return seg_send(pid, 0, sig);
}

int _std getpid(void) { return mod_getpid(); }

static void exit_common(int status, int fini) {
   process_context       *pq = mod_context();
   void _std (*finifn)(void) = (void _std (*)(void))pq->rtbuf[RTBUF_RTFINI];
   void _std (*exitfn)(int ) = (void _std (*)(int ))pq->rtbuf[RTBUF_RTEXIT];

   if (pq->pid==1) _throw_(xcpt_interr);

   if (fini && finifn) finifn();
   if (!in_mtmode && exitfn) exitfn(status);
   // finish self in any way
   get_mtlib()->stop(0, 0, status);
   // no MT mode?
   _throw_(xcpt_interr);
}

void _std exit (int errorcode) { exit_common(errorcode, 1); }
void _std _exit(int errorcode) { exit_common(errorcode, 0); }

void _std abort(void) { 
   if (!in_mtmode) _exit(EXIT_FAILURE); else raise(SIGABRT);
}

unsigned int _std alarm(unsigned int seconds) {
   u32t     res = 0;
   qe_eid  evno;

   if (!in_mtmode)
      if (mt_initialize()) return 0;

   mt_swlock();
   // unschedule current event
   evno = mt_tlsget(QTLS_ALARMQH);
   if (evno) {
      qe_event *ev = qe_unschedule(evno);
      if (ev) {
         s64t   tdiff = ev->evtime - sys_clock();
         if (tdiff>0)
            if ((tdiff/=CLOCKS_PER_SEC)>=_4GBLL) res = FFFF; else res = tdiff;
         free(ev);
      }
   }
   if (seconds) {
      mt_thrdata *th;
      mt_getdata(0, &th);
      evno = qe_schedule(sys_q, sys_clock()+(u64t)seconds*CLOCKS_PER_SEC,
                         SYSQ_ALARM, th->tiPID, th->tiTID, 0);
   } else
      evno = 0;
   mt_tlsset(QTLS_ALARMQH, evno);
   mt_swunlock();
   return res;
}

/// common process signal handler
static int _std common_handler(u32t signal) {
   sig_state *ti = (sig_state*)mt_tlsget(QTLS_SIGINFO);
   if (ti) {
      int sig = sig_cvtback(signal);
      if (sig) {
         __sig_function fn = ti->fn[sig];

         // log_it(3, "sig %u->%u (%08X)\n", signal, sig, fn);

         // drop schedule information
         if (sig==SIGALRM) mt_tlsset(QTLS_ALARMQH,0);

         if (fn==SIG_IGN) return 1;
         if (fn!=SIG_DFL) {
            fn(sig);
            return 1;
         } else
         if (sig==SIGABRT) get_mtlib()->stop(0,0,EXIT_FAILURE);
      }
   }
   return 0;
}

// called before main() - in new process context
void init_signals(void) {
   mt_prcdata *pd;
   mt_swlock();
   mt_getdata(&pd, 0);
   pd->piList[0]->tiSigFunc = common_handler;
   mt_swunlock();
}

__sig_function _std signal(int sig, __sig_function sf) {
   u32t signum = sig_convert(sig);
   if (!signum || sf==SIG_ERR) {
      set_errno(EINVAL);
      return SIG_ERR;
   } else {
      sig_state     *ti = (sig_state*)mt_tlsget(QTLS_SIGINFO);
      __sig_function pv;

      if (!ti) {
         // return it without allocation
         if (sf==SIG_GET || sf==SIG_DFL) return SIG_DFL;

         ti = (sig_state*)calloc_thread(1,sizeof(sig_state));
         ti->sign = SIGN_INFO;
         mt_tlsset(QTLS_SIGINFO,(u32t)ti);
      } else 
      if (ti->sign!=SIGN_INFO) {
         static const char *panic = "signal() record broken!";
         // just hang to catch it
         log_printf("%08X: %16b\n", ti, ti);
         mem_dumplog(panic);
         _throwmsg_(panic);
      }
      pv = ti->fn[sig];

      if (sf!=SIG_GET)
         if (!in_mtmode) ti->fn[sig] = sf; else {
            mt_thrdata *th;
         
            mt_swlock();
            mt_getdata(0, &th);
            // ugly, but set it directly instead of mt_setsignal() call
            th->tiSigFunc = common_handler;
            ti->fn[sig]   = sf;
            mt_swunlock();
         }
      return pv;
   }
}
