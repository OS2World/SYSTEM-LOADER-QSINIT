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
#include "qsxcpt.h"
#include "qsmodext.h"
#include "cpudef.h"

#define TLS_GROW_SIZE   64    ///< TLS array one time grow size

mt_thrdata* mt_allocthread(mt_prcdata *pd, u32t eip, u32t stacksize) {
   mt_thrdata *rc = 0;
   u32t        ii;

   if (pd->piSign!=PROCINFO_SIGN) return 0;

   if (pd->piThreads<pd->piListAlloc) {
      for (ii=0; ii<pd->piListAlloc; ii++)
         if (pd->piList[ii].tiMiscFlags&TFLM_AVAIL) { rc = pd->piList+ii; break; }
      if (!rc) {
         log_it(0, "prcdata err (%08X,%u,%s) alloc %u\n", pd, pd->piPID,
            pd->piModule->name, pd->piListAlloc);
      }
   }
   // allocates new area
   if (!rc) {
      u32t        pa = pd->piListAlloc;
      mt_thrdata *nl = (mt_thrdata*)malloc_shared((pd->piListAlloc = (pa+5) * 2) * sizeof(mt_thrdata));
      mt_thrdata *pl = pd->piList;
      u32t   oldrpos = sizeof(mt_thrdata)*pa, ii,
             newrpos = sizeof(mt_thrdata)*pd->piListAlloc;
      // copy fiber data & zero added space
      memcpy(nl, pl, oldrpos);
      memset(nl+pa, 0, newrpos-oldrpos);
      // init new fiber data slots
      for (ii=pa; ii<pd->piListAlloc; ii++) {
         nl[ii].tiSign   = THREADINFO_SIGN;
         nl[ii].tiPID    = pd->piPID;
         nl[ii].tiTID    = FFFF;
         nl[ii].tiParent = pd;
         nl[ii].tiMiscFlags = TFLM_AVAIL;
      }
      // this is new list!
      pd->piList = nl;
      // update pt_current!!!
      if (pt_current->tiPID==pd->piPID) pt_current = pd->piList + (pt_current->tiTID - 1);

      if ((pd->piMiscFlags&PFLM_EMBLIST)==0) free(pl); else
         pd->piMiscFlags &= ~PFLM_EMBLIST;

      rc = pd->piList + pa;
   }
   if (rc) {
      u32t idx,
           tid = (rc-pd->piList) + 1;
      if (rc->tiPID!=pd->piPID || rc->tiParent!=pd || rc->tiFibers ||
         rc->tiSign!=THREADINFO_SIGN) THROW_ERR_PD(pd);
      /* limit to 4096 running threads per process now ;)
         this limitation is used for heap blocks handling */
      if (tid > 1<<MAX_TID_BITS) return 0;

      rc->tiFiberIndex = 0;
      rc->tiState      = THRD_SUSPENDED;
      rc->tiMiscFlags  = 0;
      rc->tiTID        = tid;
      // allocate TLS ptr array and storage for all existing variables
      rc->tiTLSArray   = (u64t**)calloc_shared(pd->piTLSSize,sizeof(u64t*));
      for (ii=0; ii<pd->piTLSSize; ii++)
         if (pd->piList->tiTLSArray[ii])
            rc->tiTLSArray[ii] = (u64t*)alloc_thread_memory(pd->piPID, tid, TLS_VARIABLE_SIZE);

      idx = mt_allocfiber(rc, FIBT_MAIN, stacksize, eip);
      if (idx) THROW_ERR_PD(pd);

      pd->piThreads++;
   }
   return rc;
}

void mt_freethread(mt_thrdata *th, int fini) {
   mt_prcdata *pd = th->tiParent;
   mt_fibdata *fd = th->tiList;
   u32t    ii, la = th->tiListAlloc,
               el = th->tiMiscFlags&TFLM_EMBLIST;
   // check it all: we must have active secondary thread here, at least
   if (th->tiSign!=THREADINFO_SIGN || (th->tiMiscFlags&TFLM_AVAIL) || !pd ||
      !fd || th->tiPID!=pd->piPID) THROW_ERR_PD(pd);

   for (ii=0; ii<th->tiListAlloc; ii++)
      if (th->tiList[ii].fiType!=FIBT_AVAIL) mt_freefiber(th,ii,fini);
   // free tls array
   if (th->tiTLSArray) free(th->tiTLSArray);
   /* free heap blocks, belonging to this thread,
      for the main thread this call is in the process data freeing code */
   if (th->tiTID>1) mem_freepool(QSMEMOWNER_COTHREAD+th->tiTID-1, pd->piPID);

   memset(th, 0, sizeof(mt_thrdata));

   pd->piThreads--;

   if (!el) free(fd); else
   if (!fini) {
      th->tiList      = fd;
      th->tiListAlloc = la;
   }
   // empty slot
   if (!fini) {
      th->tiSign      = THREADINFO_SIGN;
      th->tiMiscFlags = TFLM_AVAIL|el;
      th->tiPID       = pd->piPID;
      th->tiTID       = FFFF;
      th->tiParent    = pd;
   }
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

   if (th->tiSign!=THREADINFO_SIGN || (th->tiMiscFlags&TFLM_AVAIL) || !pd ||
      !fd || th->tiPID!=pd->piPID || index>=th->tiListAlloc ||
         fd->fiSign!=FIBERSTATE_SIGN || fd->fiType==FIBT_AVAIL)
            THROW_ERR_PD(pd);

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


/// allocate thread local storage index
u32t _std mt_tlsalloc(void) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   u32t    ii, idx;
   u64t      **pos;
   mt_swlock();
   th  = (mt_thrdata*)pt_current;
   pd  = th->tiParent;
   pos = (u64t**) memrchrd((u32t*)th->tiTLSArray, 0, pd->piTLSSize);
   // grow array in every thread
   if (!pos) {
      u32t nsz = pd->piTLSSize + TLS_GROW_SIZE;
      for (ii=0; ii<pd->piThreads; ii++) {
         mt_thrdata *tl = pd->piList+ii;
         tl->tiTLSArray = (u64t**)realloc(tl->tiTLSArray, sizeof(u64t**)*nsz);
         memset(tl->tiTLSArray+pd->piTLSSize, 0, TLS_GROW_SIZE*sizeof(u64t**));
      }
      pos = th->tiTLSArray + pd->piTLSSize;
      pd->piTLSSize += TLS_GROW_SIZE;
   }
   idx = pos - th->tiTLSArray;
   /// storage for every thread 
   for (ii=0; ii<pd->piThreads; ii++) {
      mt_thrdata *tl = pd->piList+ii;
      tl->tiTLSArray[idx] = (u64t*)alloc_thread_memory(pd->piPID, th->tiTID, TLS_VARIABLE_SIZE);
   }
   mt_swunlock();
   return idx;
}

/// free thread local storage index
qserr _std mt_tlsfree(u32t index) {
   mt_thrdata  *th;
   mt_prcdata  *pd;
   u32t     ii, rc = 0;
   mt_swlock();
   th = (mt_thrdata*)pt_current;
   pd = th->tiParent;
   if (index>=pd->piTLSSize) rc = E_MT_TLSINDEX; else
   if (index<tls_prealloc) rc = E_SYS_ACCESS; else
   if (th->tiTLSArray[index]==0) rc = E_MT_TLSINDEX; else {
      for (ii=0; ii<pd->piThreads; ii++) {
         mt_thrdata *tl = pd->piList+ii;
         free(tl->tiTLSArray[index]);
         tl->tiTLSArray[index] = 0;
      }
   }
   mt_swunlock();
   return rc;
}

void _std mt_dumptree(void) {
   mt_prcdata *pd;
   mt_swlock();

   log_it(2,"== Process Tree ==\n");
   pd = walk_start();
   while (pd) {
      process_context* pq = pd->piContext;
      mt_thrdata      *th = pd->piList;
      u32t             ii;

      log_dumppctx(pq);

      //log_printf("pid %3d, %s\n", pd->piPID, pd->piModule->name);
      log_printf("  parent pid %d, tid %d\n", pd->piParentPID, pd->piParentTID);
      log_printf("  threads: %d (alloc %d)\n", pd->piThreads, pd->piListAlloc);

      for (ii=0; ii<pd->piListAlloc; ii++, th++) {
         if ((th->tiMiscFlags&TFLM_AVAIL)==0) {
            static const char *thstate[THRD_STATEMAX+1] = { "RUNNING", "SUSPENDED",
               "FINISHED", "WAITING"};
            char    modinfo[144];
            u32t     object, offset, ti;
            module      *mi;
            mt_fibdata *cfb = th->tiList + th->tiFiberIndex;

            log_printf("%stid %d : %s\n", th==pt_current?">>":"  ", th->tiTID,
               thstate[th->tiState]);

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
               }
            }
            mi = mod_by_eip(cfb->fiRegisters.tss_eip, &object, &offset, cfb->fiRegisters.tss_cs);
            if (mi)
               snprintf(modinfo, 64, "(\"%s\" %d:%08X)", mi->name, object+1, offset);
            else
               modinfo[0] = 0;

            if (th->tiMiscFlags)
               log_printf("    %s%s%s%s\n", th->tiMiscFlags&TFLM_MAIN?"main ":"",
                  th->tiMiscFlags&TFLM_EMBLIST?"emb ":"", th->tiMiscFlags&TFLM_SYSTEM?"sys ":"",
                     th->tiMiscFlags&TFLM_AVAIL?"avail ":"");
            log_printf("    fibers %d, active %d, th %08X, stack %08X..%08X, eip %08X %s\n",
               th->tiFibers, th->tiFiberIndex, th, cfb->fiStack,
                  (u32t)cfb->fiStack+cfb->fiStackSize-1, cfb->fiRegisters.tss_eip, modinfo);
            // modinfo has 144 bytes, this should be enough
            sprintf(modinfo, "    lead tls: ");
            for (ti=0; ti<6; ti++) {
               u64t *ptv = th->tiTLSArray[ti];
               if (ptv) {
                  sprintf(modinfo+strlen(modinfo), "%LX %LX", ptv[0], ptv[1]);
               } else strcat(modinfo, "x");
               if (ti<5) strcat(modinfo, ", ");
            }
            strcat(modinfo, "\n");
            log_printf(modinfo);
         }
      }

      pd = walk_next(pd);
      if (pd) log_it(2,"------------------\n");
   }
   log_it(2,"==================\n");
   mt_swunlock();
}
