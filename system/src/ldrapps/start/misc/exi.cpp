//
// QSINIT "start" module
// exportable interfaces
//
#include "qsclass.h"
#include "classes.hpp"
#include "qslog.h"

#define EXI_SIGN    MAKEID4('I','D','T','A')
#define THUNK_SIZE  16

static u8t thunkcode[THUNK_SIZE]= {0x58,              // pop  eax
                                   0x68,0,0,0,0,      // push data offset
                                   0x50,              // push eax
                                   0xE9,0,0,0,0,      // jmp to real function
                                   0,0,0,0};

struct cl_ref {
   u32t           fncount;
   u32t          datasize;
   exi_pinitdone     init;
   exi_pinitdone     done;
};
typedef Strings<cl_ref*> TRefList;

static TRefList *refs = 0;

struct cl_header {
   u32t              sign;
   u32t           classid;
   u32t          datasize;
   u32t          reserved;
};

void exi_registerstd(void);
void register_lists(void);
extern "C" void register_bitmaps(void);

/*  create struct:
    ------------ private part -----------
    signature (4 bytes)
    class id (4 bytes)
    user data size (4 bytes)
    reserved (4 bytes)
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

   u32t    algn16 = Round16(ref->datasize + sizeof(void*)*ref->fncount);
   cl_header *hdr = (cl_header*)malloc(sizeof(cl_header) + algn16 + THUNK_SIZE*ref->fncount);
                    
   memZero(hdr);
   hdr->sign      = EXI_SIGN;
   hdr->classid   = classid;
   hdr->datasize  = ref->datasize;

   u32t ii;
   u8t *thunks = (u8t*)(hdr+1) + algn16;
   u32t *funcs = (u32t*)(hdr+1),
       dataptr = (u32t)(funcs + ref->fncount);
   // creating thunks
   for (ii=0;ii<ref->fncount;ii++) {
      u8t *thc = thunks+ii*THUNK_SIZE;
      memcpy(thc, thunkcode, THUNK_SIZE);
      // store user data offset into push instruction
      *(u32t*)(thc+2) = dataptr;
      // store jump to actual function into jmp rel32 instruction
      *(u32t*)(thc+8) = ((u32t*)(ref+1))[ii] - (u32t)(thc+12);
      // store thunk ptr to public struct
      funcs[ii] = (u32t)thc;
   }
   // constructor
   if (ref->init) ref->init(funcs,(void*)dataptr);
   return funcs;
}

void* _std exi_create(const char *classname) {
   if (!refs) exi_registerstd();
   int idx = refs->IndexOf(classname);
   if (idx<0) return 0;
   return exi_createid(idx+1);
}

void _std exi_free(void *instance) {
   if (!refs||!instance) return;

   cl_header *ch = (cl_header*)instance-1;
   if (ch->sign!=EXI_SIGN) return;
   // clear sign to prevent recursive call from destructor
   ch->sign = 0;
   // call destructor
   cl_ref *ref = refs->Objects(ch->classid-1);
   if (ref->done) ref->done(instance,(u8t*)(instance)+sizeof(void*)*ref->fncount);
   // free object
   free(ch);
}

u32t _std exi_classid(void *instance) {
   if (!refs||!instance) return 0;

   cl_header *ch=(cl_header*)instance-1;
   if (ch->sign!=EXI_SIGN) return 0;
   if (ch->classid>refs->Count()) return 0;
   return ch->classid;
}

char* _std exi_classname(void *instance) {
   u32t id = exi_classid(instance);
   if (!id) return 0;
   return (char*)((*refs)[id-1]());
}

u32t _std exi_register(const char *classname, void **funcs, u32t funccount, 
   u32t datasize, exi_pinitdone constructor, exi_pinitdone destructor)
{
   if (!refs) exi_registerstd();
   if (!funcs||!funccount||!classname) return 0;
   int idx = refs->IndexOf(classname);
   cl_ref *ref = 0;
   // class unregistered?
   if (idx>=0) 
      if ((ref=refs->Objects(idx))->fncount) return 0;
   // alloc/realloc ref data
   ref = (cl_ref*)realloc(ref,sizeof(cl_ref)+funccount*sizeof(void*));
   memZero(ref);
   // copy values
   ref->fncount  = funccount;
   ref->datasize = datasize;
   ref->init     = constructor;
   ref->done     = destructor;

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
