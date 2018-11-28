//
// QSINIT
// session & signal specific code
// ------------------------------------------------------------
//
#include "mtlib.h"

#define SIGNAL_STACK     12288         ///< signal thread/fiber stack size

// warning! called from interrupt!
u32t key_available(u32t sno) {
   se_sysdata *sd = se_dataptr(sno);
   return sd ? sd->vs_keybuf[sd->vs_keyrp] : 0;
}

qserr _std mod_execse(u32t mod, const char *env, const char *params,
   u32t flags, u32t vdev, mt_pid *ppid, const char *title)
{
   module *mi = (module*)mod;
   qserr  res = 0;
   if (!mt_on) {
      res = mt_initialize();
      if (res) return res;
   }
   if (!mod || (flags&~(QEXS_DETACH|QEXS_WAITFOR|QEXS_BOOTENV|QEXS_BACKGROUND))
      || (flags&QEXS_DETACH) && vdev) return E_SYS_INVPARM;
   // check device bits
   if (vdev && (vdev&se_devicemask())!=vdev) return E_CON_NODEVICE;

   mt_swlock();
   if (mi->sign!=MOD_SIGN) res = E_SYS_INVOBJECT; else
   if (mi->flags&MOD_EXECPROC) res = E_MOD_EXECINPROC; else
   if (mi->flags&MOD_LIBRARY) res = E_MOD_LIBEXEC; else {
      int         eres;
      char    *envdata = 0;
      se_start_data ld;
      // here we safe again
      mt_swunlock();
      // create default environment
      if (!env) envdata = env_copy(flags&QEXS_BOOTENV?pq_qsinit:mod_context(), 0);
      // and launch it
      ld.sign  = SEDAT_SIGN;
      ld.flags = flags;
      ld.pid   = 0;
      ld.vdev  = vdev;
      ld.title = 0;
      if (title) {
         ld.title = strdup(title);
         // string will be released by mt_launch()
         mem_modblock(ld.title);
      }
      eres     = mod_exec(mod, env?env:envdata, params, &ld);
      res      = eres<0?E_SYS_SOFTFAULT:0;
      if (!res && ppid) *ppid = ld.pid;
      if (envdata) free(envdata);
   }
   return res;
}

/** set signal processing callback for this thread.
   Callback is one per thread.
   @param  cb               New callback function, 0 to return default.
   @return previous installed callback function or 0 if no one. */
mt_sigcb _std mt_setsignal(mt_sigcb cb) {
   mt_sigcb res;
   mt_swlock();
   res = (mt_sigcb)pt_current->tiSigFunc;
   pt_current->tiSigFunc = cb;
   mt_swunlock();
   return res;
}

static u32t _std signal_fiber(void *arg) {
   mt_thrdata *th = (mt_thrdata*)pt_current;
   /* pop all available signals in queue and call it one by one */
   while (1) {
      mt_sigcb   cb = 0;
      u32t    signo = 0;
      int        rc = 0;
      /* lock critical part. this place must be in sync with
         th->tiSigFiber setup in mt_sigfiberstart() below */
      mt_swlock();
      if (th->tiSigFunc) cb = (mt_sigcb)th->tiSigFunc;
      if (th->tiSigQueue) {
         u32t alloc = mem_blocksize(th->tiSigQueue)>>2;
         signo = *th->tiSigQueue;
         // delete empty queue
         if (th->tiSigQueue[1]==0) {
            free(th->tiSigQueue);
            th->tiSigQueue = 0;
         } else {
            memmove(th->tiSigQueue, th->tiSigQueue+1, alloc-1);
            th->tiSigQueue[alloc-1] = 0;
         }
      }
      if (!signo) th->tiSigFiber = 0;
      mt_swunlock();
      // no more signals?
      if (!signo) return 0;

      if (dump_lvl>0) log_it(3, "signal %u for pid %u tid %u (%08X)\n", signo,
         th->tiPID, th->tiTID, cb);
      // process can stop self here or exit from this fiber via siglongjmp
      if (cb) rc = cb(signo);

      if (!rc) {
         switch (signo) {
            case QMSV_KILL  :
               // terminate self, no success here, because no return on success
               if (th->tiTID==1) mod_stop(th->tiPID,0,EXIT_FAILURE);
                  else mt_termthread(0, th->tiTID, EXIT_FAILURE);
               break;
            case QMSV_BREAK :
               if (th->tiTID==1) _exit(EXIT_FAILURE); else
                  mt_exitthread(EXIT_FAILURE);
               break;
         }
      }
      // log_it(3, "signal %u rc %u\n", signo, rc);
   }
   return 0;
}

/** collect all child processes into the list.
    Function uses walk_start() / walk_next() to enum childs.

    @param  pd        process
    @param  self      include self into the list
    @return list or 0 if no one found. I.e. if returning value is non-zero,
            then list has at least ONE entry in it. */
dd_list mt_gettree(mt_prcdata *pd, int self) {
   dd_list     pl = 0;
   mt_prcdata *pp;
   mt_swlock();
   if (self) {
      pl = NEW(dd_list);
      pl->add(pd->piPID);
   }
   pp = pd->piFirstChild;
   if (pp) {
      pp = walk_start(pp);
      do {
         if (!pl) pl = NEW(dd_list);
         pl->add(pp->piPID);
         pp = walk_next(pp);
      } while (pp);
   }
   mt_swunlock();
   return pl;
}

/** query number of signals in the thread signal queue.
    Must be called inside MT lock only!
    Function just throws trap screen on error in thread data ;)
    @param  th           thread
    @param  [out] bsize  size of tiSigQueue buffer (can be 0)
    @return # of available signals */
u32t mt_sigavail(mt_thrdata *th, u32t *bsize) {
   u32t alloc = 0, used = 0;
   // check it a bit
   mt_checkth(th);
   // ask memmgr about heap block size
   if (th->tiSigQueue) {
      alloc = mem_blocksize(th->tiSigQueue)>>2;
      if (alloc) {
         u32t *pos = memchrd(th->tiSigQueue, 0, alloc);
         used = pos ? pos - th->tiSigQueue : alloc;
      }
   }
   if (bsize) *bsize = alloc;
   return used;
}

/** push signal into the thread`s signal queue.
    Must be called inside MT lock only!
    @param  th         thread
    @param  sig        signal number. sig==0 just check for errors
    @return error code */
qserr mt_sigpush(mt_thrdata *th, u32t sig) {
   u32t alloc = 0,
         used = mt_sigavail(th, &alloc);
   /* check thread state: ignore break if thread waits for a child and
      ignore any signal for exiting(ed) thread */
   if (th->tiState==THRD_WAITING && (th->tiWaitReason==THWR_CHILDEXEC &&
      sig==QMSV_BREAK || th->tiWaitReason==THWR_TIDMAIN))
         return th->tiWaitReason==THWR_TIDMAIN?E_MT_GONE:E_MT_BUSY;
   if (th->tiState==THRD_FINISHED) return E_MT_GONE;
   // check success!
   if (!sig) return 0;
   // do not duplicate signals!
   if (used)
      if (memchrd(th->tiSigQueue, sig, used)) return 0;
   // push signal
   if (used==alloc) {
      u32t nsize = alloc + 32;
      th->tiSigQueue = th->tiSigQueue ? realloc(th->tiSigQueue, nsize<<2) : malloc(nsize<<2);
      mem_threadblockex(th->tiSigQueue, th);
      memsetd(th->tiSigQueue+alloc, 0, nsize-alloc);
      alloc = nsize;
   }
   th->tiSigQueue[used] = sig;
   return 0;
}

/// must be called in MT lock
qserr mt_sigfiberstart(mt_thrdata *th) {
   if (th->tiSigFiber) return 0; else {
      mt_cfdata ops;
      ops.size       = sizeof(mt_cfdata);
      ops.stack      = 0;
      ops.stacksize  = SIGNAL_STACK;
      /* create signal fiber, but just leave it.
         scheduler will swap fiber to th->tiSigFiber when discover existing
         signal queue before thread will receive time slice next time. */
      th->tiSigFiber = mt_newfiber(th, signal_fiber, MTCF_APC, &ops, 0);
      return th->tiSigFiber ? 0 : ops.errorcode;
   }
}

qserr _std mt_sendsignal(u32t pid, u32t tid, u32t signal, u32t flags) {
   mt_prcdata   *where;
   qserr           res = 0;
   int         asktree = 0;
   mt_thrdata  *thdest = 0;
   // signal==0 can be used to check as in libc
   if (tid==0) tid = 1;
   // log_it(3, "#01: signal %u for pid %u tid %u\n", signal, pid, tid);

   if (flags&~(QMSF_TREE|QMSF_TREEWOPARENT)) return E_SYS_INVPARM;
   asktree = flags&(QMSF_TREE|QMSF_TREEWOPARENT) ? 1 : 0;
   // accepts only 0 or 1 (main thread) for a tree
   if (asktree && tid!=1) return E_SYS_INVPARM;
   // check MT mode
   if (!mt_on) {
      res = mt_initialize();
      if (res) return res;
   }
   mt_swlock();
   where = pid?get_by_pid(pid):pt_current->tiParent;
   // check args a bit
   if (!where) res = E_MT_BADPID; else
      if (!asktree) {
         thdest = get_by_tid(where, tid);
         if (!thdest) res = E_MT_BADTID;
      }
   // and here we go
   if (!res) {
      if (thdest) {
         // only system thread able to send signal to a system thread
         if ((thdest->tiMiscFlags&TFLM_SYSTEM) && (pt_current->tiMiscFlags&
            TFLM_SYSTEM)==0) res = E_SYS_ACCESS;
         else {
            res = mt_sigpush(thdest, signal);
            if (!res && signal) res = mt_sigfiberstart(thdest);
         }
      } else {
         // collect process tree into the list
         dd_list       tree = mt_gettree(where, 0);
         mt_prcdata *target;
         u32t      succ_cnt = 0;
         // create fibers for each enumerated process and then for the root pid
         do {
            target = 0;
            if (tree) {
               target = (mt_prcdata*)get_by_pid(tree->value(tree->max()));
               if (tree->count()==1) {
                  DELETE(tree);
                  tree = 0;
               } else
                  tree->del(tree->max(),1);
            }
            if (!target && (flags&QMSF_TREEWOPARENT)==0) {
               target = where;
               where  = 0;
            }
            // we got a target to signal (tid 1 only here)
            if (target) {
               mt_thrdata *th = target->piList[0];
               res = mt_sigpush(th, signal);
               if (!res && signal) res = mt_sigfiberstart(th);
               if (!res) succ_cnt++;
            }
         } while (target);
         /* return success if it was ok at least once,
            and error if no one found */
         if (res && succ_cnt) res = 0; else
            if (!res && !succ_cnt) res = E_SYS_NOTFOUND;
      }
   }
   mt_swunlock();
   return res;
}

qserr _std mod_stop(mt_pid pid, int tree, u32t rval) {
   mt_prcdata *pd;
   qserr      res = 0;
   if (!mt_on) {
      res = mt_initialize();
      if (res) return res;
   }
   mt_swlock();
   pd = pid?get_by_pid(pid):pt_current->tiParent;
   if (!pd) res = E_MT_BADPID;

   if (!res) {
      if (tree) {
         /* because PID value grows linear, we can collect tree into the list
            here and terminate it one by one without possibility to catch
            someone else */
         dd_list   pl = mt_gettree(pd,1);
         int     self = pl->delvalue(pt_current->tiPID);
         // stop them, but release lock between because of too long timeouts
         if (pl)
            while (pl->count()) {
               u32t lres = mod_stop(pl->value(pl->max()), 0, rval);
               /* return to the user last non-zero error code, but ignore
                  any errors */
               if (lres) res = lres;

               pl->del(pl->max(),1);
               mt_swunlock();
               // yield some time for other threads
               mt_swlock();
            }
         if (pl) DELETE(pl);
         // some kind of sin, i think ;)
         if (self) mod_stop(0, 0, rval);
      } else
      if (pd->piPID==1 || (pd->piMiscFlags&PFLM_SYSTEM)) res = E_MT_ACCESS; else {
         u32t ii, self;
         /* try to suspend them first - to be safe a bit more.

            use "pid" arg here and in mt_termthread() below to save zero value
            in it. mod_stop(0) can be called by chained module, which cannot
            be enlisted by get_by_pid(), but still accessible via pt_current */
         for (ii=0; ii<pd->piListAlloc; ii++) {
            mt_thrdata *th = pd->piList[ii];
            if (th && th!=pt_current) mt_suspendthread(pid, th->tiTID, 0);
         }
         /* stop threads, but current must be the last, because no return
            from mt_termthread() in this case */
         for (ii=0, self=0; ii<pd->piListAlloc+self; ii++) {
            mt_thrdata *th = ii==pd->piListAlloc?(mt_thrdata*)pt_current:pd->piList[ii];
            if (th) {
               if (th==pt_current && !self) { self=1; continue; }
               res = mt_termthread(pid, th->tiTID, rval);
               if (res==E_MT_GONE) res = 0;
               if (res) break;
            }
         }
      }
   }
   mt_swunlock();
   return res;
}
