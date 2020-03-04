//
// QSINIT "start" module
// abstract "system data cache" processing
// -------------------------------------
//
#include "qsint.h"
#include "syslocal.h"
#include "qsxcpt.h"
#include "time.h"

#define DCF_DELETED     0x00010           ///< entry is deleted
#define DCF_CBACTIVE    0x00020           ///< callback in progress

struct dce {
   u32t       size;
   u32t      flags;
   void       *usr;
   dc_notify    cb;
   qsclock     exp;    // expiration time
   dce       *prev,
             *next;
};

static dce    *first = 0;
static u32t  delwait = 0;
static int notify_on = 0;

void _std notifycation(sys_eventinfo *info) {
   switch (info->eventtype) {
      case SECB_QSEXIT: sys_dccommit(DCN_COMMIT); break;
      case SECB_LOWMEM: sys_dccommit(DCN_MEM);    break;
   }
}

qserr _std sys_dcenew(void *usr, u32t flags, u32t maxtime, dc_notify cb) {
   if (!usr || !cb) return E_SYS_ZEROPTR;
   if (flags&~(DCF_UNSAFE|DCF_NOTIMER)) return E_SYS_INVPARM;
   
   dce *fc   = new dce;
   fc->size  = sizeof(dce);
   fc->flags = flags;
   fc->usr   = usr;
   fc->cb    = cb;
   fc->exp   = sys_clock() + (u64t)maxtime * CLOCKS_PER_SEC;

   /* add function is safe to be called from sys_dccommit() callback,
      because it modify only "first" variable */
   hlp_blistadd(fc, FIELDOFFSET(dce,prev), (void**)&first, 0);

   if (!notify_on) {
      notify_on = 1;
      sys_notifyevent(SECB_QSEXIT|SECB_GLOBAL, notifycation);
      sys_notifyevent(SECB_LOWMEM|SECB_GLOBAL, notifycation);
   }
   return 0;
}

static qserr del_mark(dce *ce) {
   if (!ce) return E_SYS_ZEROPTR;
   if (ce->size!=sizeof(dce)) return E_SYS_INVOBJECT;

   ce->flags|=DCF_DELETED;

   mt_safedadd(&delwait,1);
   return 0;
}


static void del_process() {
   mt_swlock();
   if (delwait) {
      delwait = 0;

      dce *ce = first;
      while (ce) {
         dce *nce = ce->next;
         if (ce->size!=sizeof(dce))
            _throwmsg_("dce: memory violation");
         // delete all, except current callback`s caller entry
         if (ce->flags&DCF_DELETED)
            if (ce->flags&DCF_CBACTIVE) delwait++; else {
               hlp_blistdel(ce, FIELDOFFSET(dce,prev), (void**)&first, 0);
               ce->size = 0; ce->prev = 0; ce->next = 0;
               delete ce;
            }
         ce = nce;        
      }
   }
   mt_swunlock();
}


qserr _std sys_dcedel(void *usr) {
   if (!usr) return E_SYS_ZEROPTR;
   mt_swlock();
   dce *ce = first;
   while (ce) {
      if (ce->usr==usr) { del_mark(ce); break; }
      ce = ce->next;
   }
   if (ce) del_process();
   mt_swunlock();

   return ce?0:E_SYS_NOTFOUND;
}

void _std sys_dccommit(u32t code) {
   if (code>DCN_CPCHANGE) return;

   /* note, that function should not use any kind of memory allocation,
      because of DCN_MEM code, which called from hlp_memalloc() during
      out of memory processing.
      AND function must take in mind, that user may modify processing list
      during callback */

   mt_swlock();
   dce     *ce = first;
   qsclock now = 0;

   while (ce) {
      int call = 1;

      switch (code) {
         case DCN_COMMIT:
            if ((ce->flags&DCF_UNSAFE)==0) call = 0;
            break;
         case DCN_TIMER :
            if (ce->flags&DCF_NOTIMER) call = 0; else {
               if (!now) now = sys_clock();
               if (now<ce->exp) call = 0;
            }
            break;
      }
      if (call) {
         ce->flags|= DCF_CBACTIVE;
         u32t   rc = ce->cb(code, ce->usr);
         ce->flags&=~DCF_CBACTIVE;
         // update flags from callback answer
         switch (rc&0xFF) {
            case DCNR_GONE  : del_mark(ce); break;
            case DCNR_SAFE  : ce->flags&=~DCF_UNSAFE; break;
            case DCNR_UNSAFE: ce->flags|=DCF_UNSAFE; break;
         }
         if (rc&DCNR_TMSTART) ce->flags&=~DCF_NOTIMER; else
            if (rc&DCNR_TMSTOP) ce->flags|=DCF_NOTIMER;
         // zero clock cache because callback execution time is unknown
         now = 0;
      }
      ce = ce->next;
   }
   if (delwait) del_process();
   mt_swunlock();
}
