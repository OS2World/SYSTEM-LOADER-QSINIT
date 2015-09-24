//
// QSINIT "start" module
// exportable interfaces
//
#include "qsclass.h"
#include "classes.hpp"
#include "qslog.h"
#include "internal.h"
#include "qsmodext.h"
#include "qsxcpt.h"

#define EXI_SIGN    MAKEID4('I','D','T','A')
#define THUNK_SIZE  16

#define INSTANCE_CHAINOFF  0x0000001    ///< no chaining for this instance

static u8t thunkcode[THUNK_SIZE]= {0x58,              // pop  eax
                                   0x68,0,0,0,0,      // push data offset
                                   0x50,              // push eax
                                   0xE9,0,0,0,0,      // jmp to real function
                                   0,0,0,0};

struct cl_header {
   u32t              sign;
   u32t           classid;
   u32t          datasize;
   u32t             flags;
   u32t       reserved[2];
   cl_header        *prev;
   cl_header        *next;
};

struct cl_ref {
   u32t           fncount;
   u32t          datasize;
   exi_pinitdone     init;
   exi_pinitdone     done;
   cl_header       *first;
   u32t            module;
   pvoid          *thunks;
};

typedef Strings<cl_ref*> TRefList;

static TRefList    *refs = 0;
static TStrings *extlist = 0;

void exi_registerstd(void);
void register_lists(void);
extern "C" void register_bitmaps(void);

/// create push/jmp exi thunks for instance
static void exi_makethunks(cl_ref *ref, cl_header *hdr) {
   if (!ref->fncount) return;
   u32t      sz16 = Round16(ref->datasize + sizeof(void*)*ref->fncount), ii;
   u8t      *code = (u8t*)(hdr+1) + sz16;
   u32t    *funcs = (u32t*)(hdr+1),
          dataptr = (u32t)(funcs + ref->fncount);
   int    chainon = ref->thunks && (hdr->flags&INSTANCE_CHAINOFF)==0;
   for (ii=0; ii<ref->fncount; ii++) {
      u8t    *thc = code + ii*THUNK_SIZE;
      // jump to chaining thunks or directly to function
      u32t   addr = chainon ? (u32t)ref->thunks[ii] : ((u32t*)(ref+1))[ii];
      memcpy(thc, thunkcode, THUNK_SIZE);
      // store user data offset into push instruction
      *(u32t*)(thc+2) = dataptr;
      // store jump to actual function into jmp rel32 instruction
      *(u32t*)(thc+8) = addr - (u32t)(thc+12);
      // store thunk ptr to public struct
      funcs[ii] = (u32t)thc;
   }
}

/*  create struct:
    ------------ private part -----------
    signature (4 bytes)
    class id (4 bytes)
    user data size (4 bytes)
    internal data (20 bytes)
    ------------ public part ------------
    public function list (4 * funcscount)
    ------------ private part -----------
    user data
    space for 16 byte align
    call thunks (16 * funcscount)
    ------------------------------------- */
void* _std exi_createid(u32t classid) {
   if (!classid) return 0;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref    *ref = refs->Objects(classid-1);
   if (!ref->fncount) return 0;

   u32t  ftabsize = sizeof(void*) * ref->fncount,
           algn16 = Round16(ref->datasize + ftabsize);
   cl_header *hdr = (cl_header*)malloc(sizeof(cl_header) + algn16 + THUNK_SIZE*ref->fncount);
   u32t    *funcs = (u32t*)(hdr+1);

   memZero(hdr);
   hdr->sign      = EXI_SIGN;
   hdr->classid   = classid;
   hdr->datasize  = ref->datasize;
   // insert instance into list
   hdr->next      = ref->first;
   ref->first     = hdr;
   if (hdr->next) hdr->next->prev = hdr;
   // make code
   exi_makethunks(ref, hdr);
   // constructor
   if (ref->init) ref->init(funcs,(u8t*)funcs+ftabsize);
   return funcs;
}

static int exi_loadext(const char *classname) {
   if (!extlist) {
      extlist = new TStrings;
      ecmd_readsec("class",*extlist);
      if (!extlist->Count()) {
         delete extlist; extlist = 0;
         return -1;
      }
      extlist->TrimEmptyLines();
   }
   int idx = extlist->IndexOfName(classname);
   if (idx<0) return -1;

   spstr mdfname = ecmd_readstr("MODULES", extlist->Value(idx)());
   // module loaded, but no class?
   if (mod_query(mdfname(),MODQ_NOINCR)) return -1;
   // exi_register() must been called from module init
   u32t error = 0;
   if (load_module(mdfname, &error)) return refs->IndexOf(classname); else
      log_printf("Error %d on loading module \"%s\" for class \"%s\"\n",
         error, mdfname(), classname);
   return -1;
}

u32t _std exi_methods(u32t classid) {
   if (!classid) return 0;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref *ref = refs->Objects(classid-1);
   return ref->fncount;
}

u32t _std exi_queryid(const char *classname) {
   if (!refs) exi_registerstd();
   int idx = refs->IndexOf(classname);
   if (idx<0) idx = exi_loadext(classname);
   if (idx<0) return 0;
   return idx+1;
}

void* _std exi_create(const char *classname) {
   u32t classid = exi_queryid(classname);
   return classid ? exi_createid(classid) : 0;
}

void _std exi_free(void *instance) {
   if (!refs||!instance) return;
   cl_header *ch = (cl_header*)instance-1;
   if (ch->sign!=EXI_SIGN) return;
   cl_ref   *ref = refs->Objects(ch->classid-1);
   // unlink from list
   if (ch==ref->first) ref->first = ch->next; else ch->prev->next = ch->next;
   if (ch->next) ch->next->prev = ch->prev;
   // clear sign to prevent recursive call from destructor
   ch->sign = 0;  ch->next = 0;  ch->prev = 0;
   // call destructor
   if (ref->done) ref->done(instance,(u8t*)(instance)+sizeof(void*)*ref->fncount);
   // free object
   free(ch);
}

static int exi_countinst(const char *clname, cl_ref *ref, int trap) {
   cl_header *ptr = ref->first;
   u32t        rc = 0;
   while (ptr) {
      if (ptr->sign!=EXI_SIGN) {
         log_printf("Class \"%s\, instance %d (%08X) - header broken\n",
            clname, rc, ptr);
         log_printf("%08X: %16b\n", (u8t*)ptr-16, (u8t*)ptr-16);
         log_printf("%08X: %8lb\n", ptr, ptr);
         log_printf("%08X: %16b\n", ptr+1, ptr+1);
         log_printf("%08X: %16b\n", (u8t*)(ptr+1)+16, (u8t*)(ptr+1)+16);
         if (trap) _throw_(xcpt_exierr); else return -1;
      }
      rc++; ptr = ptr->next;
   }
   return rc;
}

int _std exi_printclass(u32t classid) {
   if (!refs || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);

   if (!ref->fncount) log_it(2,"%2d. class \"%s\" is unregistered\n", classid,
      (*refs)[classid-1]());
   else {
      log_it(2,"%2d. class \"%s\", %d methods, %d bytes data\n", classid,
         (*refs)[classid-1](), ref->fncount, ref->datasize);
      int cnt = exi_countinst((*refs)[classid-1](), ref, 1);
      log_it(2,"    %i instance(s), source module \"%s\"\n", cnt,
         mod_getname(ref->module,0));
   }
   return 1;
}

void _std exi_dumpall(void) {
   log_it(2,"<====== Class list ======>\n");
   if (!refs || !refs->Count()) return;
   log_it(2,"%d classes:\n", refs->Count());
   for (u32t ii=0; ii<refs->Count(); ii++) exi_printclass(ii+1);
}

int _std exi_checkstate(void) {
   if (!refs) return 1;
   u32t ii;
   for (ii=0; ii<refs->Count(); ii++) {
      cl_ref *ref = refs->Objects(ii);
      if (!ref->fncount) continue;
      /// walk over instance list and check signatures
      if (exi_countinst((*refs)[ii](), ref, 0) < 0) return 0;
   }
   return 1;
}

u32t _std exi_classid(void *instance) {
   if (!refs||!instance) return 0;
   volatile cl_header *ch = (cl_header*)instance-1;
   // call was made safe to allow querying of any pointer
   _try_ {
      if (ch->sign!=EXI_SIGN) _ret_in_try_(0);
      if (ch->classid>refs->Count()) _ret_in_try_(0);
      _ret_in_try_(ch->classid);
   }
   _catch_(xcpt_all) {
   }
   _endcatch_
   return 0;
}

void *_std exi_instdata(void *instance) {
   if (!refs||!instance) return 0;
   volatile cl_header *ch = (cl_header*)instance-1;
   _try_ {
      if (ch->sign!=EXI_SIGN) _ret_in_try_(0);
      if (ch->classid>refs->Count()) _ret_in_try_(0);

      cl_ref  *ref = refs->Objects(ch->classid-1);
      _ret_in_try_((u32t*)instance + ref->fncount);
   }
   _catch_(xcpt_all) {
   }
   _endcatch_
   return 0;
}

static void exi_freethunks(cl_ref *ref) {
   if (ref->thunks && ref->fncount) {
      for (u32t ii=0; ii<ref->fncount; ii++)
         if (ref->thunks[ii]) mod_freethunk(ref->module, ref->thunks[ii]);
      free(ref->thunks);
      ref->thunks = 0;
   }
}

int _std exi_chainon(u32t classid) {
   if (!refs || !classid || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->fncount) return 0;
   if (ref->thunks) return 1;
   // trap here on damaged list
   u32t   icnt = exi_countinst((*refs)[classid-1](), ref, 1), ii;
   // zeroed buffer
   ref->thunks = (pvoid*)calloc(ref->fncount,sizeof(void*));
   // alloc thunks, but with success for all
   for (ii=0; ii<ref->fncount; ii++)
      if (!(ref->thunks[ii] = mod_buildthunk(ref->module, ((void**)(ref+1))[ii]))) {
         exi_freethunks(ref);
         return 0;
      }
   // rebuild exi thunks in all instances! ;)
   if (icnt) {
      cl_header *ihdr = ref->first;
      while (ihdr) {
         exi_makethunks(ref, ihdr);
         ihdr = ihdr->next;
      }
   }
   return 1;
}

int _std exi_chainset(void *instance, int enable) {
   if (!refs || !instance) return -1;
   volatile cl_header *ch = (cl_header*)instance-1;
   volatile int prevstate = 0;
   _try_ {
      if (ch->sign!=EXI_SIGN) _ret_in_try_(-1);
      if (ch->classid>refs->Count()) _ret_in_try_(-1);

      prevstate  = ch->flags&INSTANCE_CHAINOFF?0:1;
      // state actually changed -> rebuild table
      if (enable>=0 && Xor(prevstate,enable)) {
         ch->flags = ch->flags&~INSTANCE_CHAINOFF|(enable?0:INSTANCE_CHAINOFF);
         exi_makethunks(refs->Objects(ch->classid-1), (cl_header*)ch);
      }
   }
   _catch_(xcpt_all) {
      prevstate = -1;
   }
   _endcatch_
   return prevstate;
}


pvoid* _std exi_thunklist(u32t classid) {
   if (!refs || !classid || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->thunks) exi_chainon(classid);
   return ref->thunks;
}

char* _std exi_classname(void *instance) {
   u32t id = exi_classid(instance);
   if (!id) return 0;
   return (char*)((*refs)[id-1]());
}

u32t _std exi_register(const char *classname, void **funcs, u32t funccount,
   u32t datasize, exi_pinitdone constructor, exi_pinitdone destructor, u32t mh)
{
   if (!refs) exi_registerstd();
   if (!funcs||!funccount||!classname) return 0;
   int idx = refs->IndexOf(classname), ii;
   cl_ref *ref = 0;
   // class unregistered?
   if (idx>=0)
      if ((ref=refs->Objects(idx))->fncount) return 0;
   // get module
   if (!mh)
      for (ii=0; ii<funccount; ii++) {
         module *md = mod_by_eip((u32t)funcs[ii],0,0,0);
         if (!mh) mh = (u32t)md; else
            if (mh!=(u32t)md) {
               log_printf("module mismatch in class %s\n", classname);
               return 0;
            }
      }
   // alloc/realloc ref data
   ref = (cl_ref*)realloc(ref,sizeof(cl_ref)+funccount*sizeof(void*));
   memZero(ref);
   // copy values
   ref->fncount  = funccount;
   ref->datasize = datasize;
   ref->init     = constructor;
   ref->done     = destructor;
   ref->module   = mh;

   memcpy(ref+1,funcs,funccount*sizeof(void*));
   // update or add ref data
   if (idx>=0) refs->Objects(idx)=ref; else idx=refs->AddObject(classname,ref);
   return idx+1;
}

int _std exi_unregister(u32t classid) {
   if (!classid) return 0;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref *ref  = refs->Objects(classid-1);
   // trap here on damaged list
   u32t   icnt  = exi_countinst((*refs)[classid-1](), ref, 1);
   // deny if it have instances
   if (icnt) {
      log_printf("unable to unregister %s, %d active instances\n",
         (*refs)[classid-1](), icnt);
      return 0;
   }

   if (ref->thunks) exi_freethunks(ref);
   ref->fncount = 0;
   // prevent destructor calling for lost objects of this type
   ref->done = 0;
   return 1;
}

void exi_registerstd(void) {
   if (refs) return;
   refs = new TRefList;
   register_lists();
   register_bitmaps();

   spstr prereg = refs->GetTextToStr(",");
   if (prereg.lastchar()==',') prereg.dellast();
   log_printf("lists inited: %s\n",prereg());
}
