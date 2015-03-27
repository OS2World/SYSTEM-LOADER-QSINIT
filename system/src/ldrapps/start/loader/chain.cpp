#define MODULE_INTERNAL
#include "qsmod.h"
#include "seldesc.h"
#include "qschain.h"
#include "qsmodext.h"
#include "qcl/qslist.h"
#include "qslog.h"
#include "stdlib.h"

static ptr_list chlist = 0;
// asm router
extern "C" void _std chain_entry();

static void build_thunk(ordinal_data *od, int disable = 0) {
   // reset thunk to original state
   if (disable || od->od_entry==0 && od->od_exit==0 && od->od_replace==0) {
      *od->od_thunk = 0xE9;
      *(u32t*)(od->od_thunk+1) = od->od_address - ((u32t)od->od_thunk+5);
   } else {
      od->od_thunk[0] = 0x50;  // push eax
      od->od_thunk[1] = 0xB8;  // mov  eax, od
      *(u32t*)(od->od_thunk+2) = (u32t)od;
      od->od_thunk[6] = 0xE9;  // jmp  chain_entry
      *(u32t*)(od->od_thunk+7) = (u32t)&chain_entry - ((u32t)od->od_thunk+11);
   }
}

// called from chain_entry()
extern "C"
void _std thunk_call(mod_chaininfo *ci) {
   ordinal_data *od = ci->mc_orddata;
   ptr_list   funcs = ci->mc_entry?od->od_entry:od->od_exit;
   if (funcs) {
      u32t ii;
      for (ii=0; ii<funcs->count(); ii++)
         if (!( (mod_chainfunc)(funcs->value(ii)) )(ci)) break;
   }
}

extern "C"
void _std thunk_panic(ordinal_data *od, int msgnum) {
   switch (msgnum) {
      case 0:
         log_it(2,"Hook error: module %s, ordinal %d - exit stack is over!\n",
            od->od_handle->name, od->od_ordinal);
         break;
      case 1:
         log_it(0,"Hook error: invalid ebp\n");
         break;
   }
}

static ordinal_data *find_entry(module *mh, u32t ordinal) {
   u32t ii, cnt = chlist?chlist->count():0;
   if (!cnt) return 0;
   for (ii=0;ii<cnt;ii++) {
      ordinal_data *od = (ordinal_data*)chlist->value(ii);
      if (od->od_handle==mh && od->od_ordinal==ordinal) return od;
   }
   return 0;
}

int  _std mod_apichain  (u32t mh, u32t ordinal, u32t chaintype, void *handler)
{
   module *md=(module*)mh;
   if (!mh || md->sign!=MOD_SIGN || !ordinal || !chaintype ||
      (chaintype&~APICN_FIRSTPOS)>APICN_REPLACE || !handler) return 0;
   ordinal_data *od = find_entry(md, ordinal);
   if (!od) {
      u32t ii;
      mod_export *me;
      // check ordinal & thunk presence
      for (ii=0; ii<md->exports; ii++) {
         me = md->exps+ii;
         if (me->ordinal==ordinal) {
            // 16bit or forward entry - no chain
            if (me->is16 || me->forward) return 0;
            // data object entry, no thunk was created
            if (me->address == me->direct) return 0;
            break;
         }
      }
      // no ordinal
      if (ii>=md->exports) return 0;

      if (!chlist) chlist = NEW(ptr_list);
      od = (ordinal_data*)malloc(sizeof(ordinal_data));
      memZero(od);
      od->od_handle  = md;
      od->od_ordinal = ordinal;
      od->od_thunk   = (u8t*)me->address;
      od->od_address = me->direct;

      chlist->add(od);
   } else // disable thunk until end of changes
      build_thunk(od,1);

   int f1stpos = chaintype&APICN_FIRSTPOS;

   switch (chaintype&~APICN_FIRSTPOS) {
      case APICN_ONENTRY:
         if (!od->od_entry) od->od_entry = NEW(ptr_list);
         if (f1stpos) od->od_entry->insert(0,handler,1); 
            else od->od_entry->add(handler);
         break;
      case APICN_ONEXIT :
         if (!od->od_exit)  od->od_exit  = NEW(ptr_list);
         if (f1stpos) od->od_exit->insert(0,handler,1); 
            else od->od_exit->add(handler);
         break;
      case APICN_REPLACE:
         od->od_replace = handler;
         break;
   }
   build_thunk(od);
   return 1;
}

u32t  _std mod_apiunchain(u32t mh, u32t ordinal, u32t chaintype, void *handler) {
   module *md=(module*)mh;
   if (!mh || md->sign!=MOD_SIGN || chaintype>APICN_REPLACE) return 0;
   if (!chlist) return 0;
   u32t rc = 0;
   if (!ordinal) {
      u32t ii = 0;
      while (ii<chlist->count()) {
         ordinal_data *od = (ordinal_data*)chlist->value(ii);
         if (od->od_handle!=md) ii++; else {
            rc+=mod_apiunchain(mh, od->od_ordinal, chaintype, handler);
            if (!rc) ii++;
         }
      }
      if (rc) log_it(2,"module %s, unchain %d entries\n", md->name, rc);
   } else {
      ordinal_data *od = find_entry(md, ordinal);
      if (!od) return 0;
      // disable thunk until end of changes
      build_thunk(od,1);

      if (!handler) {
         if ((!chaintype || chaintype==APICN_ONENTRY) && od->od_entry) {
            rc+=od->od_entry->count();
            od->od_entry->clear();
         }
         if ((!chaintype || chaintype==APICN_ONEXIT) && od->od_exit) {
            rc+=od->od_exit->count();
            od->od_exit->clear();
         }
         if ((!chaintype || chaintype==APICN_REPLACE) && od->od_replace) {
            od->od_replace = 0;
            rc++;
         }
      } else {
         if ((!chaintype || chaintype==APICN_ONENTRY) && od->od_entry) {
            rc+=od->od_entry->count();
            od->od_entry->delvalue(handler);
            rc-=od->od_entry->count();
         }
         if ((!chaintype || chaintype==APICN_ONEXIT) && od->od_exit) {
            rc+=od->od_exit->count();
            od->od_exit->delvalue(handler);
            rc-=od->od_exit->count();
         }
         if ((!chaintype || chaintype==APICN_REPLACE) && od->od_replace==handler) {
            od->od_replace = 0;
            rc++;
         }
      }
      if (od->od_entry && od->od_entry->count()==0)
         { DELETE(od->od_entry); od->od_entry = 0; }
      if (od->od_exit  && od->od_exit->count()==0 )
         { DELETE(od->od_exit);  od->od_exit  = 0; }
      build_thunk(od);

      // delete empty ordinal entry
      if (od->od_entry==0 && od->od_exit==0 && od->od_replace==0) {
         chlist->delvalue(od);
         free(od);
      }
   }
   // free empty list
   if (chlist->count()==0) { DELETE(chlist); chlist=0; }
   return rc;
}

// warning! called from trap screen!
extern "C"
void mod_resetchunk(u32t mh, u32t ordinal) {
   module *md=(module*)mh;
   if (!mh || md->sign!=MOD_SIGN || !ordinal) return;
   ordinal_data *od = find_entry(md, ordinal);
   if (od) build_thunk(od,1);
}
