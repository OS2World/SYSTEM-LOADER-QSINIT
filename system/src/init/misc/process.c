//
// QSINIT
// process initial setup
//

#define MODULE_INTERNAL
#include "qsmod.h"
#include "qsutil.h"
#include "qspdata.h"

static mt_prcdata* _std mt_new (process_context *pq);
static void        _std mt_exit(process_context *pq);

extern u16t         state_rec_len;
extern mod_addfunc *mod_secondary;
// alloc/free functions
pmt_new   mt_init = mt_new;
pmt_exec  mt_exec = 0;
pmt_exit  mt_fini = mt_exit;

static mt_prcdata* _std mt_new(process_context *pq) {
   u32t   alloc_len = sizeof(mt_prcdata) + sizeof(mt_thrdata) +
                      sizeof(mt_fibdata) + state_rec_len;
   mt_prcdata   *pc = mod_secondary?mod_secondary->memAlloc(1, (u32t)pq->self,
                      alloc_len): hlp_memallocsig(alloc_len, "MDta", QSMA_READONLY);
   mt_thrdata   *td = (mt_thrdata*)(pc+1);
   mt_fibdata   *fd = (mt_fibdata*)(td+1);

   pc->piSign       = PROCINFO_SIGN;
   pc->piPID        = pq->pid;
   pc->piParentPID  = pq->parent?pq->pctx->pid:0;
   pc->piThreads    = 1;   
   pc->piExitCode   = FFFF;
   pc->piPrio       = 0;   
   pc->piModule     = pq->self;
   pc->piList       = td;
   pc->piListAlloc  = 1;  
   pc->piMiscFlags  = (pc->piModule->flags&MOD_SYSTEM?PFLM_SYSTEM:0)|PFLM_EMBLIST;
   pc->piNext       = 0;
   pc->piFirstChild = 0;

   td->tiSign       = THREADINFO_SIGN;
   td->tiPID        = pc->piPID;
   td->tiTID        = 1;
   td->tiParent     = pc;
   td->tiFibers     = 1;
   td->tiExitCode   = FFFF;   

   td->tiState      = THRD_RUNNING;
   td->tiWaitReason = 0;
   td->tiUserData   = 0;
   td->tiList       = fd;
   td->tiListAlloc  = 1;

   td->tiMiscFlags  = TFLM_MAIN|TFLM_EMBLIST;   

   fd->fiSign       = FIBERSTATE_SIGN;
   fd->fiType       = FIBT_MAIN;
   fd->fiRegisters  = fd+1;
   fd->fiUserData   = 0;
   return pc;
}

static void _std mt_exit(process_context *pq) {
   mt_prcdata *pc = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   if (!pc) return;
   // trap in any way ;)
   if (pc->piSign!=PROCINFO_SIGN)
      mod_secondary->sys_throw(0xFFFA, pq->self->name, pq->pid);
   // just return on system
   if (pc->piMiscFlags&MOD_SYSTEM) return;
   // it will panic on bad pointer too ;)
   mod_secondary->memFree((void*)pc);

   pq->rtbuf[RTBUF_PROCDAT] = 0;
}

void _std mt_setextcb(mt_proc_cbext *ecb) {
   if (ecb) {
      mt_init = ecb->init;
      mt_exec = ecb->exec;
      mt_fini = ecb->fini;
   } else {
      mt_init = mt_new; 
      mt_exec = 0;
      mt_fini = mt_exit;
   }
}
