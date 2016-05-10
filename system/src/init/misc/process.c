//
// QSINIT
// process initial setup
//

#define MODULE_INTERNAL
#include "clib.h"
#include "qsint.h"
#include "qsmod.h"
#include "qsutil.h"
#include "qspdata.h"
#include "seldesc.h"

extern mt_proc_cb    mt_exechooks;
extern u16t         state_rec_len;
extern mod_addfunc *mod_secondary;

mt_prcdata* _std mt_new(process_context *pq) {
   u32t   alloc_len = sizeof(mt_prcdata) + sizeof(mt_thrdata) + sizeof(mt_fibdata);
   mt_prcdata   *pc = mod_secondary?mod_secondary->mem_alloc(QSMEMOWNER_MODLDR,
                      (u32t)pq->self, alloc_len): hlp_memallocsig(alloc_len,
                      "MDta", QSMA_READONLY);
   mt_thrdata   *td = (mt_thrdata*)(pc+1);
   mt_fibdata   *fd = (mt_fibdata*)(td+1);
   // guarantee zero-fill
   if (mod_secondary) memset(pc, 0, alloc_len);

   pc->piSign       = PROCINFO_SIGN;
   pc->piPID        = pq->pid;
   pc->piParentPID  = pq->parent?pq->pctx->pid:0;
   pc->piParentTID  = 1;
   pc->piThreads    = 1;
   pc->piExitCode   = FFFF;
   pc->piContext    = pq;
   pc->piModule     = pq->self;
   pc->piList       = td;
   pc->piListAlloc  = 1;
   pc->piMiscFlags  = (pc->piModule->flags&MOD_SYSTEM?PFLM_SYSTEM:0)|PFLM_EMBLIST;
   // b:\ is initial start dir
   pc->piCurDrive   = DISK_LDR;
   // and we can try to get it from current process
   if (mod_secondary) {
      char *cdir = mod_secondary->getcurdir();
      if (cdir && cdir[0]) {
         pc->piCurDrive = cdir[0]-'A';
         pc->piCurDir[pc->piCurDrive] = cdir;
      }
   }

   td->tiSign       = THREADINFO_SIGN;
   td->tiPID        = pc->piPID;
   td->tiTID        = 1;
   td->tiParent     = pc;
   td->tiFibers     = 1;
   td->tiExitCode   = FFFF;

   td->tiState      = THRD_RUNNING;
   td->tiList       = fd;
   td->tiListAlloc  = 1;

   td->tiMiscFlags  = TFLM_MAIN|TFLM_EMBLIST;

   fd->fiSign       = FIBERSTATE_SIGN;
   fd->fiType       = FIBT_MAIN;
   return pc;
}

u32t _std mt_exit(process_context *pq) {
   mt_prcdata *pc = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   char       ltr;
   // trap in any way ;)
   if (!pc || pc->piSign!=PROCINFO_SIGN)
      mod_secondary->sys_throw(0xFFFA, pq->self->name, pq->pid);
   // just return on system
   if (pc->piMiscFlags&MOD_SYSTEM) return 0;
   // free current dir buffers if was allocated
   for (ltr='A'; ltr<='Z'; ltr++)
      if (pc->piCurDir[ltr-'A']) mod_secondary->mem_free(pc->piCurDir[ltr-'A']);
   // free process & main thread memory
   mod_secondary->mem_freepool(QSMEMOWNER_COPROCESS, (u32t)pc);
   mod_secondary->mem_freepool(QSMEMOWNER_COTHREAD, pq->pid);
   // it will panic on bad pointer too ;)
   mod_secondary->mem_free((void*)pc);

   pq->rtbuf[RTBUF_PROCDAT] = 0;
   return 1;
}
