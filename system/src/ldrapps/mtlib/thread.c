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

mt_thrdata* mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksize) {
   u32t   ii, tid = 0;

   if (pd->piSign!=PROCINFO_SIGN) return 0;

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
   /* limit to 4096 running threads per process now ;)
      this limitation used at least for heap blocks handling */
   if (tid && tid <= 1<<MAX_TID_BITS) {
      u32t          idx;
      // this will be the constant pointer while thread exists
      mt_thrdata    *rt = (mt_thrdata*)calloc(1,sizeof(mt_thrdata));

      pd->piList[tid-1] = rt;
      rt->tiSign        = THREADINFO_SIGN;
      rt->tiPID         = pd->piPID;
      rt->tiTID         = tid;
      rt->tiParent      = pd;
      rt->tiMiscFlags   = 0;
      rt->tiFiberIndex  = 0;
      rt->tiState       = THRD_SUSPENDED;
      // TLS ptr array is uninited by default
      rt->tiTLSArray    = 0;

      idx = mt_allocfiber(rt, FIBT_MAIN, stacksize, eip);
      if (idx) THROW_ERR_PD(pd);

      pd->piThreads++;
      return rt;
   }
   return 0;
}

void mt_freethread(mt_thrdata *th, int fini) {
   mt_prcdata *pd = th->tiParent;
   mt_fibdata *fd = th->tiList;
   u32t    ii, la = th->tiListAlloc,
               el = th->tiMiscFlags&TFLM_EMBLIST;
   // check it a bit
   if (!th || th->tiSign!=THREADINFO_SIGN || !pd || th->tiPID!=pd->piPID ||
      !th->tiTID || th!=pd->piList[th->tiTID-1] || !fd) THROW_ERR_PD(pd);

   for (ii=0; ii<th->tiListAlloc; ii++)
      if (th->tiList[ii].fiType!=FIBT_AVAIL) mt_freefiber(th,ii,fini);
   // free tls array (variables will be released by mem_freepool() below)
   if (th->tiTLSArray) free(th->tiTLSArray);

   if ((th->tiMiscFlags&TFLM_EMBLIST)==0) {
      free(th->tiList);
      th->tiListAlloc = 0;
      th->tiList = 0;
   }
   pd->piList[th->tiTID-1] = 0;
   /* free heap blocks, belonging to this thread and mt_thrdata itself.
      For main thread heap call is in the process freeing code and mt_thrdata
      is always embedded to process data block */
   if (th->tiTID>1) {
      mem_freepool(QSMEMOWNER_COTHREAD+th->tiTID-1, pd->piPID);
      memset(th, 0, sizeof(mt_thrdata));
      free(th);
   }
   pd->piThreads--;
}

u32t mt_allocfiber(mt_thrdata *th, u32t type, u32t stacksize, u32t eip) {
   u32t  rc=FFFF, ii;
   if (th->tiSign!=THREADINFO_SIGN) return FFFF;
   /* forbid creation of main stream for system threads, else
      it allows termination by any caller */
   if (type==FIBT_MAIN && (th->tiMiscFlags&TFLM_SYSTEM)) return FFFF;

   if (th->tiFibers<th->tiListAlloc) {
      for (ii=0; ii<th->tiListAlloc; ii++)
         if (th->tiList[ii].fiType==FIBT_AVAIL) { rc = ii; break; }
      if (rc==FFFF) {
         mt_prcdata *pd = th->tiParent;
         log_it(0, "thrdata err (%08X,%u,%s) alloc %u\n", th, pd->piPID,
            pd->piModule->name, th->tiListAlloc);
      }
   }
   // allocates new area
   if (rc==FFFF) {
      u32t        pa = th->tiListAlloc;
      mt_fibdata *nl = (mt_fibdata*)malloc_shared((th->tiListAlloc = (pa+5) * 2) * sizeof(mt_fibdata));
      mt_fibdata *pl = th->tiList;
      u32t   oldrpos = sizeof(mt_fibdata)*pa, ii,
             newrpos = sizeof(mt_fibdata)*th->tiListAlloc;
      // copy fiber data
      if (oldrpos) memcpy(nl, pl, oldrpos);
      // init new fiber data slots
      for (ii=pa; ii<th->tiListAlloc; ii++) {
         nl[ii].fiSign = FIBERSTATE_SIGN;
         nl[ii].fiType = FIBT_AVAIL;
         // zero registers data (to be safe a bit)
         memset(&nl[ii].fiRegisters, 0, sizeof(struct tss_s *));
      }
      // this is new list!
      th->tiList = nl;

      if (th->tiMiscFlags&TFLM_EMBLIST) th->tiMiscFlags&=~TFLM_EMBLIST; else
      if (pl) free(pl);

      rc = pa++;
   }
   if (rc!=FFFF) {
      struct tss_s rd;
      mt_fibdata  *fd = th->tiList + rc;
      void     *stack = 0;

      if (stacksize) stack = stacksize<_64KB ? malloc_shared(stacksize):
            hlp_memallocsig(stacksize, "MTst", QSMA_RETERR|QSMA_NOCLEAR);

      fd->fiSign      = FIBERSTATE_SIGN;
      fd->fiType      = type;
      fd->fiStackSize = stacksize;
      fd->fiStack     = stack;
      fd->fiXcptTop   = 0;

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
   }
   return rc;
}

void mt_freefiber(mt_thrdata *th, u32t index, int fini) {
   mt_fibdata *fd = th->tiList + index;
   mt_prcdata *pd = th->tiParent;

   if (!th || th->tiSign!=THREADINFO_SIGN || !pd || th->tiPID!=pd->piPID ||
      index>=th->tiListAlloc || !fd || fd->fiSign!=FIBERSTATE_SIGN ||
         fd->fiType==FIBT_AVAIL) THROW_ERR_PD(pd);

   if (fd->fiStack)
      if (fd->fiStackSize<_64KB) free(fd->fiStack); else
          hlp_memfree(fd->fiStack);

   memset(fd, 0, sizeof(mt_fibdata));

   th->tiFibers--;

   // empty slot
   if (!fini) {
      fd->fiSign = THREADINFO_SIGN;
      fd->fiType = FIBT_AVAIL;

      // cut too large fiber list if it possible
      if (th->tiListAlloc>=32) {
         u32t ii = th->tiListAlloc;

         while (ii--)
            if (th->tiList[ii].fiType!=FIBT_AVAIL) break; else
            if (ii<th->tiListAlloc/2) {
               th->tiListAlloc/=2;
               th->tiList = (mt_fibdata*)realloc_shared(th->tiList,
                            th->tiListAlloc * sizeof(mt_fibdata));
               break;
            }
      }
   }
}

u32t _std mt_setfiber(mt_tid tid, u32t fiber) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   mt_thrdata *dth;
   u32t         rc = 0;

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

qserr _std mt_threadname(const char *str) {
   char *va;
   qserr rc = mt_tlsaddr(QTLS_COMMENT, (u64t**)&va);
   if (!rc) {
      // no lock here, because partial name is safe ;)
      memset(va, 0, TLS_VARIABLE_SIZE);
      if (str && *str) strncpy(va, str, TLS_VARIABLE_SIZE);
   }
   return rc;
}

void _std mt_dumptree(void) {
   mt_prcdata *pd;
   mt_swlock();

   log_it(2,"== Process Tree ==\n");
   pd = walk_start();
   while (pd) {
      process_context* pq = pd->piContext;
      u32t             ii;

      log_dumppctx(pq);

      //log_printf("pid %3d, %s\n", pd->piPID, pd->piModule->name);
      log_printf("  parent pid %d, tid %d\n", pd->piParentPID, pd->piParentTID);
      log_printf("  threads: %d (alloc %d)\n", pd->piThreads, pd->piListAlloc);

      for (ii=0; ii<pd->piListAlloc; ii++) {
         mt_thrdata *th = pd->piList[ii];
         if (th) {
            static const char *thstate[THRD_STATEMAX+1] = { "RUNNING", "SUSPENDED",
               "FINISHED", "WAITING"};
            char    modinfo[144], thname[20], *th_np;
            u32t     object, offset, ti;
            module      *mi;
            mutex_data  *md;
            mt_fibdata *cfb = th->tiList + th->tiFiberIndex;
            // thread comment string
            th_np = th->tiTLSArray?(char*)th->tiTLSArray[QTLS_COMMENT]:0;
            if (th_np && th_np!=(char*)TLS_FAKE_PTR && th_np[0]) {
               memcpy(&thname[1], th->tiTLSArray[QTLS_COMMENT], 16); thname[17] = 0;
               thname[0] = '\"'; strcat(thname, "\"");
            } else
               thname[0] = 0;

            log_printf("%stid %d [%10LX] [%08X]: %s   %s\n", th==pt_current?">>":"  ",
               th->tiTID, th->tiTime, th, thstate[th->tiState], thname);

            if (th->tiState==THRD_WAITING) {
               switch (th->tiWaitReason) {
                  case THWR_CHILDEXEC:
                     log_printf("    waiting for child pid %d\n", th->tiWaitHandle);
                     break;
                  case THWR_TIDMAIN  :
                     log_printf("    main thread exited\n");
                     break;
                  case THWR_TIDEXIT  :
                     log_printf("    waiting for tid %d\n", th->tiWaitHandle);
                     break;
                  case THWR_WAITOBJ  : {
                     we_list_entry *we = (we_list_entry*)th->tiWaitHandle;
                     u32t          idx;
                     log_printf("    wait object, %08X, %u entries, lmask %08X\n",
                        we, we->ecnt, we->clogic);
                     for (idx=0; idx<we->ecnt; idx++) {
                        static const char *wt = "?TPCM";
                        mt_waitentry     *pwe = we->we + idx;
                        u32t            htype = pwe->htype;
                        log_printf("      %3u. [%u] %08X %c %LX %LX\n", idx+1,
                           we->sigf[idx], pwe->group, wt[htype], pwe->htype==QWHT_CLOCK?
                              pwe->tme:pwe->pid, pwe->htype==QWHT_CLOCK?pwe->reserved:0);
                     }
                     break;
                  }
               }
            }
            md = (mutex_data*)th->tiFirstMutex;
            while (md) {
               log_printf("    owned mutex %u (%08X), cnt %u, wait %u\n", md->sft_no,
                  md, md->lockcnt, md->waitcnt);
               md = md->next;
            }
            mi = mod_by_eip(cfb->fiRegisters.tss_eip, &object, &offset, cfb->fiRegisters.tss_cs);
            if (mi)
               snprintf(modinfo, 64, "(\"%s\" %d:%08X)", mi->name, object+1, offset);
            else
               modinfo[0] = 0;

            if (th->tiMiscFlags)
               log_printf("    %s%s%s%s\n", th->tiMiscFlags&TFLM_MAIN?"main ":"",
                  th->tiMiscFlags&TFLM_EMBLIST?"emb ":"", th->tiMiscFlags&TFLM_SYSTEM?"sys ":"",
                     th->tiMiscFlags&TFLM_NOSCHED?"idle ":"");
            log_printf("    fibers %d, active %d, th %08X, stack %08X..%08X, eip %08X %s\n",
               th->tiFibers, th->tiFiberIndex, th, cfb->fiStack,
                  (u32t)cfb->fiStack+cfb->fiStackSize-1, cfb->fiRegisters.tss_eip, modinfo);
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
            log_printf(modinfo);
         }
      }

      pd = walk_next(pd);
      log_it(2,"------------------\n");
   }
   log_it(2,"%u timer interrupts since last dump\n", timer_cnt);
   timer_cnt = 0;
   // use "set MTLIB = ON, DUMPLEVEL=1" to het access here
   if (dump_lvl>0) {
      u32t  tmrdiv;
      log_it(3, "APIC: %X (%08X %08X %08X %08X %08X %08X)\n", apic[APIC_APICVER]&0xFF,
         apic[APIC_LVT_TMR], apic[APIC_SPURIOUS], apic[APIC_LVT_PERF],
            apic[APIC_LVT_LINT0], apic[APIC_LVT_LINT1], apic[APIC_LVT_ERR]);

      tmrdiv = apic[APIC_TMRDIV];
      tmrdiv = (tmrdiv&3 | tmrdiv>>1&4) + 1 & 7;
      apic[APIC_ESR] = 0;
      log_it(3, "APIC timer: %u %08X %08X isr0 %08X, esr %08X\n", 1<<tmrdiv,
         apic[APIC_TMRINITCNT], apic[APIC_TMRCURRCNT], apic[APIC_ISR_TAB],
            apic[APIC_ESR]);
   }

   log_it(2,"==================\n");
   mt_swunlock();
}
