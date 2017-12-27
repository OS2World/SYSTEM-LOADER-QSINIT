//
// QSINIT "start" module
// exportable interfaces
//
#include "qsclass.h"
#include "classes.hpp"
#include "qslog.h"
#include "syslocal.h"
#include "qsmodext.h"
#include "qsxcpt.h"

#define EXI_SIGN    MAKEID4('I','D','T','A')
#define THUNK_SIZE  32

#define INSTANCE_CHAINOFF  0x0000001    ///< no chaining for this instance
#define INSTANCE_MUTEX     0x0000002    ///< mutex available for instance
#define INSTANCE_GMUTEX    0x0000004    ///< mutex is global

static u8t thunkcode[THUNK_SIZE]= {0x68,0,0,0,0,      // push cl_header
                                   0xE8,0,0,0,0,      // call to mutex lock
                                   0x68,0,0,0,0,      // push data offset
                                   0xE8,0,0,0,0,      // call to method func
                                   0x68,0,0,0,0,      // push cl_header
                                   0xE9,0,0,0,0,      // jmp to mutex unlock
                                   0,0};

struct cl_header {
   u32t              sign;
   u32t           classid;
   u32t          datasize;
   u32t             flags;
   u32t               pid;
   qshandle           mux;
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
   u32t             flags;
   qshandle          gmux;
};

typedef Strings<cl_ref*> TRefList;

static TRefList    *refs = 0;
static TStrings *extlist = 0;

void exi_registerstd(void);
void exi_register_lists(void);
void exi_register_inifile(void);
void exi_register_filecc(void);
extern "C" void exi_register_bitmaps(void);
extern "C" void exi_muxunlocka(void);      // asm stub, should not be called directly

static void _std exi_muxlock(cl_header*hdr) {
   if ((hdr->flags&(INSTANCE_MUTEX|INSTANCE_GMUTEX)) && hdr->mux)
      mt_muxcapture(hdr->mux);
}

extern "C" void _std exi_muxunlock(cl_header*hdr) {
   if ((hdr->flags&(INSTANCE_MUTEX|INSTANCE_GMUTEX)) && hdr->mux)
      mt_muxrelease(hdr->mux);
}

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
      *(u32t*)(thc+ 1) = (u32t)hdr;
      *(u32t*)(thc+21) = (u32t)hdr;
      *(u32t*)(thc+ 6) = (u32t)&exi_muxlock - (u32t)(thc+10);
      *(u32t*)(thc+26) = (u32t)&exi_muxunlocka - (u32t)(thc+30);
      // store user data offset into push instruction (or 0 if no user data)
      *(u32t*)(thc+11) = ref->datasize ? dataptr : 0;
      // store jump to actual function into jmp rel32 instruction
      *(u32t*)(thc+16) = addr - (u32t)(thc+20);
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
    call thunks (32 * funcscount)
    ------------------------------------- */
void* _std exi_createid(u32t classid, u32t flags) {
   if (!classid) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref    *ref = refs->Objects(classid-1);
   if (!ref->fncount) return 0;
   // single instance per system?
   if ((ref->flags&EXIC_EXCLUSIVE) && ref->first) return 0;

   u32t  ftabsize = sizeof(void*) * ref->fncount,
           algn16 = Round16(ref->datasize + ftabsize);
   cl_header *hdr = (cl_header*)malloc(sizeof(cl_header) + algn16 + THUNK_SIZE*ref->fncount);
   u32t    *funcs = (u32t*)(hdr+1);

   mem_zero(hdr);
   hdr->sign      = EXI_SIGN;
   hdr->classid   = classid;
   hdr->datasize  = ref->datasize;
   hdr->pid       = flags&EXIF_SHARED?0:mod_getpid();
   // insert instance into list
   hdr->next      = ref->first;
   ref->first     = hdr;
   if (hdr->next) hdr->next->prev = hdr;
   // make code
   exi_makethunks(ref, hdr);
   // create mutex
   if (ref->flags&EXIC_GMUTEX) {
      hdr->mux    = ref->gmux; 
      hdr->flags |= INSTANCE_GMUTEX;
   } else
      if ((flags&EXIF_MTSAFE)) exi_mtsafe(funcs,1);
   // constructor
   if (ref->init) ref->init(funcs, ref->datasize?(u8t*)funcs+ftabsize:0);
   return funcs;
}

/* load external class.
   @param  classname     class name
   @param  query         query only mode
   @retval class index (id-1) in normal mode or 0 in query mode
           if class exists and was/can be loaded; -1 - on error/unknown
           class name */
static int exi_loadext(const char *classname, int query = 0) {
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

   spstr mdname = extlist->Value(idx),
        mdfname = ecmd_readstr("MODULES", mdname());
   // module loaded, but no class?
   if (mod_query(mdname(),MODQ_NOINCR)) return -1;
   // query only - we know enough
   if (query) return 0;
   // exi_register() must been called from module init
   u32t error = 0;
   if (load_module(mdfname, &error)) return refs->IndexOf(classname); else
      log_printf("Error %d on loading module \"%s\" for class \"%s\"\n",
         error, mdfname(), classname);
   return -1;
}

u32t _std exi_methods(u32t classid) {
   MTLOCK_THIS_FUNC lk;
   if (!classid) return 0;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref *ref = refs->Objects(classid-1);
   return ref->fncount;
}

u32t exi_queryid_int(const char *classname, int noprivate = 1) {
   MTLOCK_THIS_FUNC lk;
   if (!refs) exi_registerstd();
   int idx = refs->IndexOf(classname);
   if (idx<0) idx = exi_loadext(classname);
   if (idx<0) return 0;
   // private class?
   if (noprivate)
      if (refs->Objects(idx)->flags&EXIC_PRIVATE) return 0;

   return idx+1;
}

u32t _std exi_queryid(const char *classname) {
   return exi_queryid_int(classname);
}

u32t _std exi_query(const char *classname) {
   MTLOCK_THIS_FUNC lk;
   if (!refs) exi_registerstd();
   int idx = refs->IndexOf(classname);
   if (idx>=0) return refs->Objects(idx)->flags&EXIC_PRIVATE?EXIQ_PRIVATE:EXIQ_AVAILABLE;
   if (exi_loadext(classname,1)==0) return EXIQ_KNOWN;
   return EXIQ_UNKNOWN;
}

void* _std exi_create(const char *classname, u32t flags) {
   MTLOCK_THIS_FUNC lk;
   // will call also cause EXIC_PRIVATE to be failed
   u32t classid = exi_queryid_int(classname);
   return classid ? exi_createid(classid, flags) : 0;
}

void _std exi_free(void *instance) {
   if (!instance) return;
   MTLOCK_THIS_FUNC lk;
   if (!refs) return;
   cl_header *ch = (cl_header*)instance-1;
   if (ch->sign!=EXI_SIGN) return;
   cl_ref   *ref = refs->Objects(ch->classid-1);
   // unlink from list
   if (ch==ref->first) ref->first = ch->next; else ch->prev->next = ch->next;
   if (ch->next) ch->next->prev = ch->prev;
   // clear sign to prevent recursive call from destructor
   ch->sign = 0;  ch->next = 0;  ch->prev = 0;
   // call destructor
   if (ref->done)
      ref->done(instance, ref->datasize?(u8t*)(instance)+sizeof(void*)*ref->fncount:0);
   if (ch->mux) { 
      if (ch->mux!=ref->gmux) mt_closehandle(ch->mux); 
      ch->mux = 0; 
   }
   // free object
   free(ch);
}

static int exi_countinst(const char *clname, cl_ref *ref, int trap, u32t free_pid) {
   cl_header *ptr = ref->first, *prev;
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
      prev = ptr;
      ptr  = ptr->next;
      if (free_pid && prev->pid==free_pid) exi_free(prev+1); else rc++;
   }
   return rc;
}

int _std exi_printclass(u32t classid) {
   MTLOCK_THIS_FUNC lk;
   if (!refs || !classid || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);

   if (!ref->fncount) log_it(2,"%2d. class \"%s\" is unregistered\n", classid,
      (*refs)[classid-1]());
   else {
      log_it(2,"%2d. class \"%s\", %d methods, %d bytes data%s%s%s\n", classid,
         (*refs)[classid-1](), ref->fncount, ref->datasize, ref->flags&EXIC_GMUTEX?
            ", gmutex":"", ref->flags&EXIC_PRIVATE?", private":"",
               ref->flags&EXIC_EXCLUSIVE?", exclusive":"");
      int cnt = exi_countinst((*refs)[classid-1](), ref, 1, 0);
      log_it(2,"    %i instance(s), source module \"%s\"\n", cnt,
         mod_getname(ref->module,0));
   }
   return 1;
}

void _std exi_dumpall(void) {
   MTLOCK_THIS_FUNC lk;
   log_it(2,"<====== Class list ======>\n");
   if (!refs || !refs->Count()) return;
   log_it(2,"%d classes:\n", refs->Count());
   for (u32t ii=0; ii<refs->Count(); ii++) exi_printclass(ii+1);
}

void exi_free_as(u32t pid) {
   MTLOCK_THIS_FUNC lk;
   if (!pid || !refs || !refs->Count()) return;
   // enum classes/instances & call exi_free for this pid
   for (u32t ii=0; ii<refs->Count(); ii++) {
      cl_ref *ref = refs->Objects(ii);
      exi_countinst((*refs)[ii](), ref, 1, pid);
   }
}

int _std exi_checkstate(void) {
   MTLOCK_THIS_FUNC lk;
   if (!refs) return 1;
   u32t ii;
   for (ii=0; ii<refs->Count(); ii++) {
      cl_ref *ref = refs->Objects(ii);
      if (!ref->fncount) continue;
      /// walk over instance list and check signatures
      if (exi_countinst((*refs)[ii](), ref, 0, 0) < 0) return 0;
   }
   return 1;
}

u32t _std exi_classid(void *instance) {
   if (!instance) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs) return 0;
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
   if (!instance) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs) return 0;
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

int _std exi_share(void *instance, int global) {
   if (!instance) return -1;
   MTLOCK_THIS_FUNC lk;
   if (!refs) return -1;
   volatile cl_header *ch = (cl_header*)instance-1;
   // call was made safe to allow querying of any pointer
   _try_ {
      if (ch->sign!=EXI_SIGN) _ret_in_try_(-1);
      if (ch->classid>refs->Count()) _ret_in_try_(-1);
      int pstate = ch->pid?0:1;
      if (global>=0) ch->pid = global?0:mod_getpid();
      _ret_in_try_(pstate);
   }
   _catch_(xcpt_all) {
   }
   _endcatch_
   return -1;
}

static qserr exi_createmutex(volatile cl_header *ch) {
   qserr rc = 0;
   if (!ch->mux) {
      char mxname[16];
      snprintf(mxname, 16, "EXI_%08X", ch+1);
      rc = mt_muxcreate(0, mxname, (qshandle*)&ch->mux);
      // detach it to be sync with any kind of instance
      if (rc) log_it(1, "mux for inst %08X creation err %X\n", ch+1, rc); else
         io_setstate(ch->mux, IOFS_DETACHED, 1);
   }
   return rc;
}

static qserr exi_creategmux(u32t classid) {
   qserr    rc = 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->gmux) {
      char mxname[64];
      snprintf(mxname, 64, "EXI_G_%s", (*refs)[classid-1]());
      rc = mt_muxcreate(0, mxname, (qshandle*)&ref->gmux);
      // detach it to be sync with any kind of instance
      if (rc) log_it(1, "mux %s creation err %X\n", mxname, rc); else
         io_setstate(ref->gmux, IOFS_DETACHED, 1);
   }
   return rc;
}

int _std exi_mtsafe(void *instance, int enable) {
   volatile cl_header *ch;
   if (!instance) return -1; else {
      MTLOCK_THIS_FUNC lk;

      if (!refs) return -1;
      ch = (cl_header*)instance-1;
      // call was made safe to allow querying of any pointer
      _try_ {
         if (ch->sign!=EXI_SIGN) _ret_in_try_(-1);
         if (ch->classid>refs->Count()) _ret_in_try_(-1);
      }
      _catch_(xcpt_all) {
         ch = 0;
      }
      _endcatch_
      if (!ch) return -1;
      // continue lock until end of function
      mt_swlock();
   }
   cl_ref     *ref = refs->Objects(ch->classid-1);
   // we have global mutex, this function is void
   if (ref->flags&EXIC_GMUTEX) {
      mt_swunlock();
      return 2;
   }
   int      pstate = ch->flags&INSTANCE_MUTEX ? 1 : 0;
   qshandle free_h = 0;
   if (enable>=0) {
      if (Xor(enable,pstate)) {
         qserr rc = 0;
         if (in_mtmode)
            if (enable) rc = exi_createmutex(ch); else
            // release it only if it is free now
            if (ch->mux)
               if (mt_muxstate(ch->mux, 0)==E_MT_SEMFREE) {
                  free_h  = ch->mux;
                  ch->mux = 0;
               } else
                  rc = E_MT_BUSY;

         if (rc) pstate = -1; else
         if (enable) ch->flags|=INSTANCE_MUTEX; else
            ch->flags&=~INSTANCE_MUTEX;
      }
   }
   mt_swunlock();
   // free it outside of MT lock
   if (free_h) mt_closehandle(free_h);
   return pstate;
}

static int _std mtstart_enum(u32t classid, void *instance, void *userdata) {
   cl_header *ch = (cl_header*)instance-1;
   cl_ref   *ref = refs->Objects(classid-1);
   // copy global mutex handle else create mutex if flag was set before MT mode
   if (!ch->mux)
      if (ref->gmux) ch->mux = ref->gmux; else
         if (ch->flags&INSTANCE_MUTEX) exi_createmutex(ch);
   return 1;
}

extern "C" void setup_exi_mt(void) {
   MTLOCK_THIS_FUNC lk;
   if (!refs) return;
   for (u32t ii=0; ii<refs->Count(); ii++) {
      cl_ref *ref = refs->Objects(ii);
      if (!ref->fncount) continue;
      if (ref->flags&EXIC_GMUTEX) exi_creategmux(ii+1);
      // enum all instances and create mutexes if requested so earlier.
      exi_instenum(ii+1, mtstart_enum, EXIE_ALL);
   }
}

static void exi_freethunks(cl_ref *ref) {
   if (ref->thunks && ref->fncount) {
      for (u32t ii=0; ii<ref->fncount; ii++)
         if (ref->thunks[ii]) mod_freethunk(ref->module, ref->thunks[ii]);
      free(ref->thunks);
      ref->thunks = 0;
   }
}

int _std exi_instenum(u32t classid, exi_pinstenumcb efunc, u32t etype) {
   if (!efunc || (etype&EXIE_ALL)==0 || !classid) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->fncount) return 0;
   // check list & trap on error
   u32t   icnt = exi_countinst((*refs)[classid-1](), ref, 1, 0);
   // walk over instance list
   if (icnt) {
      cl_header *ihdr = ref->first;
      u32t       cpid = mod_getpid();
      while (ihdr) {
         if (!ihdr->pid && (etype&EXIE_GLOBAL) || ihdr->pid &&
            ((etype&EXIE_CURRENT) && ihdr->pid==cpid || (etype&EXIE_LOCAL) &&
               ihdr->pid!=cpid))
                  if (!efunc(classid, ihdr+1, (u32t*)(ihdr+1) + ref->fncount))
                     break;
         ihdr = ihdr->next;
      }
   }
   return 1;
}

int _std exi_chainon(u32t classid) {
   MTLOCK_THIS_FUNC lk;
   if (!refs || !classid || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->fncount) return 0;
   if (ref->thunks) return 1;
   // trap here on damaged list
   u32t   icnt = exi_countinst((*refs)[classid-1](), ref, 1, 0), ii;
   // zeroed buffer
   ref->thunks = (pvoid*)calloc_shared(ref->fncount,sizeof(void*));
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
   if (!instance) return -1;
   MTLOCK_THIS_FUNC lk;
   if (!refs) return -1;
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
   MTLOCK_THIS_FUNC lk;
   if (!refs || !classid || classid>refs->Count()) return 0;
   cl_ref *ref = refs->Objects(classid-1);
   if (!ref->thunks) exi_chainon(classid);
   return ref->thunks;
}

char* _std exi_classname(void *instance) {
   MTLOCK_THIS_FUNC lk;
   u32t id = exi_classid(instance);
   if (!id) return 0;
   return strdup((*refs)[id-1]());
}

u32t _std exi_register(const char *classname, void **funcs, u32t funccount,
   u32t datasize, u32t flags, exi_pinitdone constructor, exi_pinitdone destructor,
   u32t mh)
{
   char rname[64];
   if (!funcs || !funccount || (flags&~(EXIC_GMUTEX|EXIC_PRIVATE|EXIC_EXCLUSIVE)))
      return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs) exi_registerstd();
   int     idx, ii;
   cl_ref *ref = 0;

   if (classname) {
      idx = refs->IndexOf(classname);
      // is it unregistered?
      if (idx>=0)
         if ((ref=refs->Objects(idx))->fncount) return 0;
   }
   // get module
   if (!mh)
      for (ii=0; ii<funccount; ii++) {
         module *md = mod_by_eip((u32t)funcs[ii],0,0,0);
         if (!mh) mh = (u32t)md; else
            if (mh!=(u32t)md) {
               // printf can handle NULL ;)
               log_printf("module mismatch in class %s\n", classname);
               return 0;
            }
      }
   // generate random unique name
   if (!classname) {
      do {
         snprintf(rname, 64, "PRIVATE_CLASS_%05X_%05X_%s", random(0x100000),
            random(0x100000), ((module*)mh)->name);
         idx = refs->IndexOf(rname);
      } while (idx>=0);
      classname = rname;
   }
   // alloc/realloc ref data
   ref = (cl_ref*)realloc(ref,sizeof(cl_ref)+funccount*sizeof(void*));
   mem_zero(ref);
   // copy values
   ref->fncount  = funccount;
   ref->datasize = datasize;
   ref->init     = constructor;
   ref->done     = destructor;
   ref->module   = mh;
   ref->flags    = flags;

   memcpy(ref+1,funcs,funccount*sizeof(void*));
   // update or add ref data
   if (idx>=0) refs->Objects(idx)=ref; else idx=refs->AddObject(classname,ref);
   // create global mutex
   if (in_mtmode && (ref->flags&EXIC_GMUTEX)) exi_creategmux(idx+1);
   // class id
   return idx+1;
}

int _std exi_unregister(u32t classid) {
   if (!classid) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!refs) exi_registerstd();
   if (classid>refs->Count()) return 0;

   cl_ref *ref  = refs->Objects(classid-1);
   // trap here on damaged list
   u32t   icnt  = exi_countinst((*refs)[classid-1](), ref, 1, 0);
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
   exi_register_lists();
   exi_register_bitmaps();
   exi_register_inifile();
   exi_register_filecc();
#if 0
   spstr prereg = refs->GetTextToStr(",");
   if (prereg.lastchar()==',') prereg.dellast();
   log_printf("lists inited: %s\n",prereg());
#endif
}
