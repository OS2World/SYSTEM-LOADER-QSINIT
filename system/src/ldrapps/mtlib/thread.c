//
// QSINIT
// process/thread handling support
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//
#include "mtlib.h"
#include "qsmodext.h"

extern u32t   timer_cnt;

qserr mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksz, mt_thrdata **pth) {
   u32t   ii, tid = 0;

   if (pd->piSign!=PROCINFO_SIGN) return E_SYS_INVOBJECT;

   if (pd->piThreads<pd->piListAlloc) {
      for (ii=0; ii<pd->piListAlloc; ii++)
         if (pd->piList[ii]==0) { tid = ii+1; break; }
      if (!tid) {
         log_it(0, "prcdata err (%08X,%u,%s) alloc %u\n", pd, pd->piPID,
            pd->piModule->name, pd->piListAlloc);
      }
   }
   // expand thread ptr list
   if (!tid) {
      u32t         pa = pd->piListAlloc;
      mt_thrdata **nl = (mt_thrdata**)malloc((pd->piListAlloc = (pa+5) * 2) * sizeof(mt_thrdata*));
      mt_thrdata **pl = pd->piList;
      u32t    oldrpos = sizeof(mt_thrdata*)*pa, ii,
              newrpos = sizeof(mt_thrdata*)*pd->piListAlloc;
      // copy fiber data & zero added space
      memcpy(nl, pl, oldrpos);
      memset(nl+pa, 0, newrpos-oldrpos);
      // this is the new list!
      pd->piList = nl;
      if ((pd->piMiscFlags&PFLM_EMBLIST)==0) free(pl); else
         pd->piMiscFlags &= ~PFLM_EMBLIST;
      // first avail slot
      tid = pa;
   }
   // limit to 4096 running threads per process now ;)
   if (tid && tid <= 1<<MAX_TID_BITS) {
      qserr          rc;
      u32t          idx;
      /* this will be the constant pointer while thread exists, this thing
         assumed in many places and even used for direct comparation */
      mt_thrdata    *rt = (mt_thrdata*)calloc(1,sizeof(mt_thrdata));

      pd->piList[tid-1] = rt;
      rt->tiSign        = THREADINFO_SIGN;
      rt->tiPID         = pd->piPID;
      rt->tiTID         = tid;
      rt->tiParent      = pd;
      rt->tiSession     = pt_current->tiSession;
      rt->tiMiscFlags   = 0;
      rt->tiFiberIndex  = 0;
      rt->tiState       = THRD_SUSPENDED;
      // TLS ptr array is uninited by default
      rt->tiTLSArray    = 0;

      rc = mt_allocfiber(rt, FIBT_MAIN, stacksz, eip, &idx);
      if (rc) {
         log_it(0, "fiber alloc err %X (%08X, pid %u)\n", rc, pd, pd->piPID);
         pd->piList[tid-1] = 0;
         free(rt);
         return rc;
      } else
         if (idx) THROW_ERR_PD(pd);
      // we are ready here (except custom stack, caller should care of it)
      pd->piThreads++;
      *pth = rt;
      return 0;
   }
   return E_MT_THREADLIMIT;
}

void mt_checkth(mt_thrdata *th) {
   mt_prcdata *pd = th->tiParent;
   if (th->tiSign!=THREADINFO_SIGN || !pd || th->tiPID!=pd->piPID ||
      !th->tiTID || th!=pd->piList[th->tiTID-1] || !th->tiList) THROW_ERR_PD(pd);
}

void mt_freethread(mt_thrdata *th) {
   mt_prcdata *pd = th->tiParent;
   u32t        ii;
   // check it a bit
   mt_checkth(th);

   for (ii=0; ii<th->tiListAlloc; ii++)
      if (th->tiList[ii].fiType!=FIBT_AVAIL) mt_freefiber(th,ii);
   // free tls array (variables will be released by mem_freepool() below)
   if (th->tiTLSArray) free(th->tiTLSArray);

   if ((th->tiMiscFlags&TFLM_EMBLIST)==0) {
      free(th->tiList);
      th->tiListAlloc = 0;
      th->tiList = 0;
   }
   // call to START - it release thread owned memory and lost exit chain stacks
   mt_pexitcb(th, 1);
   // free thread info
   pd->piList[th->tiTID-1] = 0;
   memset(th, 0, sizeof(mt_thrdata));
   // main thread block is always embedded to the process data
   if (th->tiTID>1) free(th);

   pd->piThreads--;
}

qserr mt_allocfiber(mt_thrdata *th, u32t type, u32t stacksize, u32t eip, u32t *fibid) {
   u32t  ii, index = FFFF;
   if (th->tiSign!=THREADINFO_SIGN) return E_SYS_INVOBJECT;
   /* forbid creation of main stream for system threads, else
      it allows termination by any caller */
   if (type==FIBT_MAIN && (th->tiMiscFlags&TFLM_SYSTEM)) return E_MT_ACCESS;

   if (th->tiFibers<th->tiListAlloc) {
      for (ii=0; ii<th->tiListAlloc; ii++)
         if (th->tiList[ii].fiType==FIBT_AVAIL) { index = ii; break; }
      if (index==FFFF) {
         mt_prcdata *pd = th->tiParent;
         log_it(0, "thrdata err (%08X,%u,%s) alloc %u\n", th, pd->piPID,
            pd->piModule->name, th->tiListAlloc);
      }
   }
   // allocates new area (tiListAlloc is 0 here during seconary threads creation)
   if (index==FFFF) {
      u32t        pa = th->tiListAlloc;
      mt_fibdata *nl = (mt_fibdata*)malloc_shared((th->tiListAlloc = (pa+5) * 2) * sizeof(mt_fibdata));
      mt_fibdata *pl = th->tiList;
      u32t   oldrpos = sizeof(mt_fibdata)*pa,
             newrpos = sizeof(mt_fibdata)*th->tiListAlloc;
      // copy fiber data
      if (oldrpos) memcpy(nl, pl, oldrpos);
      // init new fiber data slots
      for (ii=pa; ii<th->tiListAlloc; ii++) {
         nl[ii].fiSign    = FIBERSTATE_SIGN;
         nl[ii].fiType    = FIBT_AVAIL;
         // zero registers data (to be safe a bit)
         memset(&nl[ii].fiRegs, 0, sizeof(struct tss_s *));
      }
      // this is new list!
      th->tiList = nl;

      if (th->tiMiscFlags&TFLM_EMBLIST) th->tiMiscFlags&=~TFLM_EMBLIST; else
         if (pl) free(pl);
      index = pa++;
   }
   {
      struct tss_s rd;
      mt_fibdata  *fd = th->tiList + index;
      void     *stack = 0;
      // stacksize == 0 for user allocated stack
      if (stacksize) {
         stack = stacksize<_64KB ? malloc_shared(stacksize):
                 hlp_memallocsig(stacksize, "MTst", QSMA_RETERR|QSMA_NOCLEAR);
         if (!stack) return E_SYS_NOMEM;
      }
      fd->fiSign      = FIBERSTATE_SIGN;
      fd->fiType      = type;
      fd->fiStackSize = stacksize;
      fd->fiStack     = stack;
      fd->fiXcptTop   = 0;
      fd->fiFPUMode   = FIBF_EMPTY;
      fd->fiFPURegs   = 0;

      memset(&rd, 0, sizeof(rd));
      rd.tss_eip      = eip;
      rd.tss_cs       = get_flatcs();
      rd.tss_ds       = rd.tss_cs + 8;
      rd.tss_es       = rd.tss_ds;
      rd.tss_ss       = force_ss?force_ss:get_flatss();
      rd.tss_eflags   = CPU_EFLAGS_IOPLMASK|CPU_EFLAGS_IF;
      // caller should setup it in own way if stacksize is 0
      if (stacksize) rd.tss_esp = (u32t)fd->fiStack + stacksize;

      mt_setregs(fd, &rd);

      fd->fiUserData  = 0;
      th->tiFibers++;
      if (fibid) *fibid = index;
      return 0;
   }
}

void mt_freefiber(mt_thrdata *th, u32t index) {
   mt_fibdata *fd = th->tiList + index;
   mt_prcdata *pd = th->tiParent;

   if (!th || th->tiSign!=THREADINFO_SIGN || !pd || th->tiPID!=pd->piPID ||
      index>=th->tiListAlloc || !fd || fd->fiSign!=FIBERSTATE_SIGN ||
         fd->fiType==FIBT_AVAIL) THROW_ERR_PD(pd);
   // it was a signal fiber?
   if (th->tiSigFiber==index) th->tiSigFiber = 0;

   if (fd->fiStack) {
      u32t  esp = get_esp(),
           base = (u32t)fd->fiStack;
      /* we cannot free stack on which we executing just NOW, but this occurs
         if we`re called from the switch_context() during deletion of current
         thread/fiber.
         Ask system thread to release it later - this is safe, because this
         stack will be swapped out forever at the end of switch_context(). */
      if (base<=esp && base+fd->fiStackSize>esp)
         qe_postevent(sys_q, SYSQ_FREE, base, fd->fiStackSize<_64KB, (u32t)&mt_freefiber);
      else
      if (fd->fiStackSize<_64KB) free(fd->fiStack); else hlp_memfree(fd->fiStack);
   }
   // zero current FPU context
   if (mt_exechooks.mtcb_cfpt==th && mt_exechooks.mtcb_cfpf==index)
      mt_exechooks.mtcb_cfpt = 0;

   memset(fd, 0, sizeof(mt_fibdata));

   th->tiFibers--;

   // empty slot
   fd->fiSign = FIBERSTATE_SIGN;
   fd->fiType = FIBT_AVAIL;
   // cut too large fiber list if it possible
   if (th->tiListAlloc>=32) {
      u32t ii = th->tiListAlloc;
      while (ii-->2)
         if (th->tiList[ii].fiType!=FIBT_AVAIL) break; else
         if (ii<th->tiListAlloc/2) {
            th->tiListAlloc/=2;
            th->tiList = (mt_fibdata*)realloc_shared(th->tiList,
                         th->tiListAlloc * sizeof(mt_fibdata));
            break;
         }
   }
}

qserr _std mt_switchfiber(u32t fb, int free) {
   mt_thrdata  *th;
   u32t         rc = 0;

   mt_swlock();
   th = (mt_thrdata*)pt_current;
   // print it
   if (dump_lvl>1)
      log_it(3, "pid %u tid %u fiber %u(%u)->%u %s\n", th->tiPID, th->tiTID,
         th->tiFiberIndex, th->tiList[th->tiFiberIndex].fiType, fb, free?"(free)":"");
   // check fiber index
   if (th->tiListAlloc<=fb || th->tiList[fb].fiType==FIBT_AVAIL) rc = E_MT_BADFIB; else
      if (th->tiFiberIndex==fb) rc = E_SYS_DONE; else
         if (free && th->tiFiberIndex==0) rc = E_MT_BADFIB;

   if (!rc) {
      th->tiWaitReason = fb;
      // switch context here (and optionally delete current fiber).
      switch_context(0, free?SWITCH_FIBEXIT:SWITCH_FIBER);
      return 0;
   } else
   if (free && rc!=E_SYS_DONE) {
      /* we called with free==on and invalid target fiber - this mean, that no
         code to return to and no code to switch to.
         just print error into the log and terminate current process */
      log_printf("Incorrect switch fiber (no fiber %u) -> no path to return!\n", fb);
      mod_stop(0, 0, EXIT_FAILURE);
   }
   mt_swunlock();
   return rc;
}

qserr mt_setfiber(mt_tid tid, u32t fiber) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   mt_thrdata *dth;
   qserr        rc = 0;

   mt_swlock();
   th  = (mt_thrdata*)pt_current;
   pd  = th->tiParent;
   dth = tid ? get_by_tid(pd, tid) : th;
   // no tid
   if (!dth) rc = E_MT_BADTID; else
   // thread is gone or this is main, waiting for secondaries
   if (dth->tiState==THRD_FINISHED || dth->tiState==THRD_WAITING &&
      dth->tiWaitReason==THWR_TIDMAIN) rc = E_MT_GONE; else
   // waiting for anything?
   if (dth->tiState==THRD_WAITING) rc = E_MT_BUSY; else
   // check fiber index
   if (dth->tiListAlloc>=fiber || dth->tiList[fiber].fiType==FIBT_AVAIL)
      rc = E_MT_BADFIB; else
   if (dth->tiFiberIndex==fiber) rc = E_SYS_DONE;

   if (!rc)
      if (th==dth) {
         // switch fiber in current active thread
         dth->tiWaitReason = fiber;
         switch_context(0, SWITCH_FIBER);
         return 0;
      } else
         dth->tiFiberIndex = fiber;
   mt_swunlock();
   return rc;
}

static void getmodinfo(char *modinfo, u32t r_cs, u32t r_eip) {
   u32t  object, offset;
   module   *mi = mod_by_eip(r_eip, &object, &offset, r_cs);
   if (mi)
      snprintf(modinfo, 64, "(\"%s\" %d:%08X)", mi->name, object+1, offset);
   else
      modinfo[0] = 0;
}

void _std mt_dumptree(printf_function pfn) {
   mt_prcdata *pd;
   mt_swlock();
   if (pfn==0) pfn = log_printf;

   pfn("== Process Tree ==\n");
   pd = walk_start(0);
   while (pd) {
      process_context* pq = pd->piContext;
      u32t             ii;

      log_dumppctx(pq,pfn);

      //pfn("pid %3d, %s\n", pd->piPID, pd->piModule->name);
      pfn("  parent pid %d, tid %d\n", pd->piParentPID, pd->piParentTID);
      pfn("  threads: %d (alloc %d)\n", pd->piThreads, pd->piListAlloc);

      if (pd->piMiscFlags&~PFLM_EMBLIST)
         pfn("    ( %s%s%s%s)\n", pd->piMiscFlags&PFLM_SYSTEM ?"sys ":"",
            pd->piMiscFlags&PFLM_CHAINEXIT?"chainexit ":"",
               pd->piMiscFlags&PFLM_NOPWAIT?"ses ":"",
                  pd->piMiscFlags&PFLM_LWAIT?"lwait ":"");

      for (ii=0; ii<pd->piListAlloc; ii++) {
         mt_thrdata *th = pd->piList[ii];
         if (th) {
            static const char *thstate[THRD_STATEMAX+1] = { "RUNNING", "SUSPENDED",
               "FINISHED", "WAITING"};
            char    modinfo[144], thname[20], *th_np;
            u32t         ti;
            mutex_data  *md;
            mt_fibdata *cfb = th->tiList + th->tiFiberIndex;
            // thread comment string
            th_np = th->tiTLSArray?(char*)th->tiTLSArray[QTLS_COMMENT]:0;
            if (th_np && th_np!=(char*)TLS_FAKE_PTR && th_np[0]) {
               memcpy(&thname[1], th->tiTLSArray[QTLS_COMMENT], 16); thname[17] = 0;
               thname[0] = '\"'; strcat(thname, "\"");
            } else
               thname[0] = 0;

            pfn("%stid %d [%10LX] [%08X] [%3u]: %s   %s\n", th==pt_current?">>":"  ",
               th->tiTID, th->tiTime, th, th->tiSession, thstate[th->tiState], thname);

            if (th->tiState==THRD_WAITING) {
               switch (th->tiWaitReason) {
                  case THWR_CHILDEXEC:
                     pfn("    waiting for child pid %d\n", th->tiWaitHandle);
                     break;
                  case THWR_TIDMAIN  :
                     pfn("    main thread exited\n");
                     break;
                  case THWR_TIDEXIT  :
                     pfn("    waiting for tid %d\n", th->tiWaitHandle);
                     break;
                  case THWR_WAITLNCH :
                     pfn("    suspended until launcher`s (pid %d) mt_waitobject\n",
                        th->tiWaitHandle);
                     break;
                  case THWR_WAITOBJ  : {
                     we_list_entry *we = (we_list_entry*)th->tiWaitHandle;
                     u32t          idx;
                     pfn("    wait object, %08X, %u entries, lmask %08X\n",
                        we, we->ecnt, we->clogic);
                     for (idx=0; idx<we->ecnt; idx++) {
                        static const char *wt = "?TPCMKQSE?";
                        mt_waitentry     *pwe = we->we + idx;
                        u32t            htype = pwe->htype;
                        char             estr[48];

                        switch (pwe->htype) {
                           case QWHT_TID  :
                           case QWHT_PID  :
                              snprintf(estr, 48, "id %u pRes:%08X", pwe->pid, pwe->resaddr);
                              break;
                           case QWHT_EVENT:
                           case QWHT_MUTEX:
                              snprintf(estr, 48, "%X %LX", pwe->wh, pwe->reserved);
                              break;
                           case QWHT_CLOCK:
                              snprintf(estr, 48, "%LX %LX", pwe->tme, pwe->reserved);
                              break;
                           case QWHT_QUEUE: {
                              u32t sft_no = 0;
                              // can be called here because we`re in lock
                              int   evnum = hlp_qavail(pwe->wh, 0, &sft_no);
                              snprintf(estr, 48, "%X sft:%u, %i events",
                                 pwe->wh, sft_no, evnum);
                              break;
                           }
                           default:
                              estr[0] = 0;
                        }
                        pfn("      %3u. [%u] %08X %c %s\n", idx+1,
                           we->sigf[idx], pwe->group, wt[htype], estr);
                     }
                     break;
                  }
               }
            }
            md = (mutex_data*)th->tiFirstMutex;
            while (md) {
               pfn("    owned mutex %u (%08X), cnt %u, wait %u\n", md->sft_no,
                  md, md->lockcnt, md->waitcnt);
               md = md->next;
            }
            if (th->tiMiscFlags) {
               u32t mf = th->tiMiscFlags;
               pfn("    %s%s%s%s%s%s\n", mf&TFLM_MAIN?"main ":"",
                  mf&TFLM_EMBLIST?"emb ":"", mf&TFLM_SYSTEM?"sys ":"",
                     mf&TFLM_NOSCHED?"idle ":"", mf&TFLM_NOFPU?"nofpu ":"",
                        mf&TFLM_NOTRACE?"notrace ":"");
            }
            if (th->tiFibers==1 && dump_lvl==0) {
               getmodinfo(modinfo, cfb->fiRegs.tss_cs, cfb->fiRegs.tss_eip);
               pfn("    fibers %u, active %u, fpu %u %08X, stack %08X..%08X, eip %08X %s\n",
                  th->tiFibers, th->tiFiberIndex, cfb->fiFPUMode, cfb->fiFPURegs,
                     cfb->fiStack, (u32t)cfb->fiStack+cfb->fiStackSize-1,
                        cfb->fiRegs.tss_eip, modinfo);
            } else {
               pfn("    fibers %u (alloc %u), active %u:\n", th->tiFibers,
                  th->tiListAlloc, th->tiFiberIndex);

               for (ti=0; ti<th->tiListAlloc; ti++) {
                  static const char *fibstate[FIBT_AVAIL] = {"main", "apc"};
                  mt_fibdata *pfb = th->tiList + ti;

                  if (pfb->fiType<FIBT_AVAIL)
                     getmodinfo(modinfo, pfb->fiRegs.tss_cs, pfb->fiRegs.tss_eip);
                  else
                     modinfo[0] = 0;
                  if (pfb->fiType>FIBT_AVAIL)
                     pfn("      %2u. ????: %7lb\n", ti, pfb->fiType, pfb);
                  else
                  if (pfb->fiType<FIBT_AVAIL)
                     pfn("    %s%2u. %-4s, fpu %u %08X, stack %08X..%08X, eip %08X %s\n",
                        ti==th->tiFiberIndex?">>":"  ", ti, fibstate[pfb->fiType],
                           pfb->fiFPUMode, pfb->fiFPURegs, pfb->fiStack, (u32t)pfb->fiStack+
                              pfb->fiStackSize-1, pfb->fiRegs.tss_eip, modinfo);
               }
            }
            // modinfo has 144 bytes, this should be enough
            sprintf(modinfo, "    lead tls: ");
            if (!th->tiTLSArray) strcat(modinfo, "unallocated"); else
            for (ti=0; ti<6; ti++) {
               u64t *ptv = th->tiTLSArray[ti];
               if (ptv==(u64t*)TLS_FAKE_PTR) strcat(modinfo, "z"); else
                  if (ptv) sprintf(modinfo+strlen(modinfo), "%LX %LX", ptv[0], ptv[1]);
                     else strcat(modinfo, "x");
               if (ti<5) strcat(modinfo, ", ");
            }
            strcat(modinfo, "\n");
            pfn(modinfo);
         }
      }

      pd = walk_next(pd);
      pfn("------------------\n");
   }
   pfn("%u timer interrupts since last dump\n", timer_cnt);
   timer_cnt = 0;
   // use "set MTLIB = ON, DUMPLVL=1" to open access here
   if (dump_lvl>0 && apic) {
      u32t  tmrdiv;
      pfn("APIC: %X (%08X %08X %08X %08X %08X %08X)\n", apic[APIC_APICVER]&0xFF,
         apic[APIC_LVT_TMR], apic[APIC_SPURIOUS], apic[APIC_LVT_PERF],
            apic[APIC_LVT_LINT0], apic[APIC_LVT_LINT1], apic[APIC_LVT_ERR]);

      tmrdiv = apic[APIC_TMRDIV];
      tmrdiv = (tmrdiv&3 | tmrdiv>>1&4) + 1 & 7;
      apic[APIC_ESR] = 0;
      pfn("APIC timer: %u %08X %08X isr0 %08X, esr %08X\n", 1<<tmrdiv,
         apic[APIC_TMRINITCNT], apic[APIC_TMRCURRCNT], apic[APIC_ISR_TAB],
            apic[APIC_ESR]);
   }

   pfn("==================\n");
   mt_swunlock();
}
