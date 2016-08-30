//
// QSINIT "bootos2" module
// kernel module handling
//
#include "stdlib.h"
#include "kload.h"
#include "doshlp.h"
#include "qsutil.h"
#include "qserr.h"

#define ORD_DOSIODELAYCNT    427           // one allowed import

//extern struct ExportFixupData *efd;
extern u16t   IODelay;

static module     *km = 0;
static lx_exe_t   *ke = 0;
static u32t *fixtable = 0;
static u16t *fixcount = 0;

static mod_export iodelay_exp = {0, ORD_DOSIODELAYCNT, 1, 0, 0};

void error_exit(int code, const char *message);

module *krnl_readhdr(void *kernel, u32t size) {
   u32t rc = 1;
   /* error codes: 1 - not LE/LX, 2 - bad flags, 3 - empty module,
                   4 - unsupported feature, 5 - broken file,
                   6 - no free selector, 7 - obj load error,
                   8 - invalid page table, 9 - additional code not loaded
                   10 - too many imported modules, 11 - invalid fixup,
                   12 - no ordinal, 13 - decompress error */
   do {
      mz_exe_t          *mz = (mz_exe_t*)kernel;
      u32t      oldhdr_size = mz->e_magic!=EMAGIC?0:mz->e_lfanew;
      register lx_exe_t *eh = (lx_exe_t*)((u8t*)kernel+oldhdr_size);
      lx_obj_t          *ot = (lx_obj_t*)((u8t*)eh+eh->e32_objtab);
      u32t               ii = 0;
      u8t*             tptr;

      ke = eh;
      // invalid signature
      if (eh->e32_magic!=LXMAGIC && eh->e32_magic!=LEMAGIC) break;
      // invalid constant fields
      if (eh->e32_border||eh->e32_worder||eh->e32_level||
          eh->e32_pagesize!=PAGESIZE) break;
      rc++;
      // module with errors or pdd/vdd
      if ((eh->e32_mflags&E32NOLOAD)!=0||(eh->e32_mflags&E32MODMASK)>E32MODDLL)
         break;
      rc++;
      // module without executable code
      if (!eh->e32_mpages||!eh->e32_startobj||!eh->e32_objmap||!eh->e32_restab||
          !eh->e32_objcnt||!eh->e32_objtab||eh->e32_startobj>eh->e32_objcnt)
              break;
      rc++;
      // unsupported feature
      if (eh->e32_instpreload) break;
      // too many used modules
      if (eh->e32_impmodcnt>1) { rc = E_MOD_MODLIMIT; break; }
      rc++;
      // broken file
      if (size-oldhdr_size-eh->e32_objcnt*sizeof(lx_obj_t)<eh->e32_objtab) // broken object table
         break;
      if (size-oldhdr_size-(eh->e32_magic==LXMAGIC?sizeof(lx_map_t):       // broken page table
         sizeof(le_map_t))*eh->e32_mpages<eh->e32_objmap) break;

      rc=0;

      // allocate physical storage for all loadable objects
      ii  = sizeof(module)+sizeof(mod_object)*(eh->e32_objcnt-1);
      km = (module*)malloc(ii);
      memset(km,0,ii);
      km->mod_path = &km->name;
      km->sign     = MOD_SIGN;
      km->usage    = 1;
      km->objects  = eh->e32_objcnt;
      km->baseaddr = 0;
      km->flags    = MOD_LIBRARY;
      // flag pre-applied fixups (Warp 3 kernel)
      if ((eh->e32_mflags&E32NOINTFIX)!=0) km->flags|=MOD_NOFIXUPS;
      // copy module name
      tptr = (u8t*)eh+eh->e32_restab;
      ii   = *tptr&0x7F;
      memcpy(km->name, tptr+1, 127);
      // modify offsets to LX header start
      eh->e32_datapage-=oldhdr_size;
      if (rc) break;
      // check import table
      if (eh->e32_impmodcnt) {
         u8t *table = (u8t*)eh+eh->e32_impmod;
         u8t    len =*table++;
         if (strnicmp("DOSCALLS",table,len)) rc = E_MOD_MODLIMIT;
      }
      if (rc) break;
      // copying object data
      for (ii=0;ii<eh->e32_objcnt;ii++) {
         km->obj[ii].size    = ot[ii].o32_size;
         km->obj[ii].orgbase = ot[ii].o32_base;
         km->obj[ii].flags   = ot[ii].o32_flags;
         if (!ot[ii].o32_base && (km->flags&MOD_NOFIXUPS)!=0 &&
           (ot[ii].o32_flags&OBJBIGDEF)!=0) {
              char err[80];
              sprintf(err,"Bad image: zero base for 32bit object %d!\n",ii+1);
              error_exit(6,err);
           }
      }
      // save start obj/eip
      km->start_ptr=eh->e32_startobj;
      km->stack_ptr=eh->e32_eip;
      // fake DOSCALLS ptr to trick mod_unpackobj
      km->impmod[0]=km;
      // IODelay fixup init
      iodelay_exp.address = IODelay;
   } while (false);

   if (rc) {
      char err[64];
      sprintf(err,"Kernel module loading error %X\n",rc);
      error_exit(6,err);
   }

   return km;
}

void krnl_fixuptable(u32t *fixtab, u16t *fixcnt) {
   fixtable  = fixtab;
   fixcount  = fixcnt;
   *fixcount = 0;
}

static mod_export* _std findexport(module *mh, u16t ordinal) {
   if (ordinal!=ORD_DOSIODELAYCNT) return 0;
   return &iodelay_exp;
}

static u32t _std applyfixup(module *mh, void *addr, u8t type, u16t sel, u32t offset) {
   u32t obj, obj_base = 0;
   // we already checked module name in init
   if (!fixcount || *fixcount>=FIXUP_STORE_TABLE_SIZE)
      error_exit(6,"Too many i/o delay fixups in this kernel module!\n");
   // searching object there fixup located and get it linear address
   for (obj=0;obj<km->objects;obj++) {
      u32t base = (u32t)km->obj[obj].address;
      if ((u32t)addr>=base && (u32t)addr<base+km->obj[obj].size) {
         obj_base = km->obj[obj].fixaddr;
         break;
      }
   }
   // assume object 1 is not MVDM :) and use it for MVDM objects
   if ((km->obj[obj].flags&OBJSHARED)==0) obj_base = km->obj[obj=1].fixaddr;

   fixtable[*fixcount] = (u32t)addr - (u32t)km->obj[obj].address + obj_base;
   //log_printf("Fixup obj %d, %8x %8x\n",obj+1,addr,fixtable[*fixcount]);
   *fixcount+=1;
   return 1;
}

static void _std objloaded(module *mh, u32t object) {
#if 0 // just debug dump ;)
   FILE *ff=fopen("0:\\dump.bin", object?"ab":"wb");
   u8t  buf=0;
   u32t pos=0;
   if (!ff) return;

   while (ftell(ff)&0x0F)
      if (!fwrite(&buf,1,1,ff)) return;
   pos = ftell(ff);
   if (fwrite(mh->obj[object].address,1,mh->obj[object].size,ff)!=
      mh->obj[object].size) return;

   log_printf("object %d dumped to file offset %8.8x size %8.8x\n",
      object+1,pos,mh->obj[object].size);
   fclose(ff);
#endif
}


void krnl_loadobj(u32t obj) {
   if (obj<ke->e32_objcnt) {
      modobj_extreq  extr;
      lx_obj_t *ot = (lx_obj_t*)((u8t*)ke+ke->e32_objtab);
      u32t  ii, rc;

      extr.flags = MODUNP_ALT_FIXADDR|MODUNP_FINDEXPORT|MODUNP_FINDEXPORTFX|
         MODUNP_OBJLOADED;
      extr.findexport = findexport;
      extr.applyfixup = applyfixup;
      extr.objloaded  = objloaded;

      rc = mod_unpackobj(km, ke, ot, obj, (u32t)km->obj[obj].address, &extr);
      if (rc) {
         char err[128];
         sprintf(err,"Kernel module loading error %d (object %d)\n",rc,obj+1);
         error_exit(6,err);
      }
   }
}

u32t  krnl_done(void) {
   u32t startaddr=MAKEFAR16((u32t)km->obj[km->start_ptr-1].sel,km->stack_ptr);
   if (km) { free(km); km=0; }
   ke=0;
   return startaddr;
}
