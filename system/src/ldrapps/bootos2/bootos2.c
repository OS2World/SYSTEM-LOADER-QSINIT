//
// QSINIT "bootos2" module
// loading OS/2 kernel
//
#include "clib.h"
#include "vio.h"
#include "qsutil.h"
#include "doshlp.h"
#include "stdlib.h"
#include "qsshell.h"
#include "qsconst.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "filetab.h"
#include "qsint.h"
#include "qssys.h"
#include "qsstor.h"
#include "ldrparam.h"
#include "ioint13.h"
#include "kload.h"
#include "k_arena.h"
#include "vesainfo.h"
#include "pagedesc.h"
#include "qspage.h"
#include "qshm.h"
#include "qsdm.h"
#include "qslist.h"
#include "serial.h"
#include "dparm.h"

#undef  MAP_A0000                  // make linear address of A0000
#undef  MAP_LOGADDR                // make linear address of log
#define MAP_REMOVEHOLES            // mark memory holes as used blocks

#define MSG_POOL_LIMIT     4096    // limit every type of messages to 4k
#define VESAFONTBUFSIZE    2048    // font buffer, required to avoid intel adapters bios
#define STACK_SIZE         2048    // stack, actually it will be larger - rounded to page boundary
#define SELINCR               8    // some constants
#define SECTSIZE            512    //
#define REMOVEMEM_LIMIT     128    // removemem parameter count

#define SECTROUND(p)    (((p) + (SECTSIZE-1)) & ~(SECTSIZE-1))

const char *msgini_name="1:\\os2boot\\messages.ini";
const char *os2dmp_name="OS2DUMP";

struct ExportFixupData *efd = 0;    // doshlp export/import data
struct LoaderInfoData  *lid = 0;    // os4ldr info header
struct VesaInfoBuf     *vid = 0;    // vesa info data

void     *kernel = 0,               // kernel image
         *doshlp = 0,               // doshlp binary file
         *os2dmp = 0,               // os2dump binary file
         *os2sym = 0,               // os2krnl.sym binary file (OS/4)
        *mfsdsym = 0;               // micro-fsd sym binary file (OS/4)
u32t     dhfsize = 0,               // doshlp file size
         dmpsize = 0,               // os2dump file size
         symsize = 0,               // os2krnl.sym file size
        msymsize = 0,               // micro-fsd sym sym file size
        mfsdsize = 0,               // mini-fsd file file
        arenacnt = 0,               // number of filled arenas
       respart4k = 0,               // doshlp size, rounded to page
      laddrdelta = 0;               // paddr / vaddr offset
int     twodiskb = 0,               // two disk boot flag
       isos4krnl = 0,               // is OS/4 kernel?
       isdbgkrnl = 0;               // is debug kernel?
str_list  *kparm = 0;               // kernel parameters line
u32t  lndataaddr = MPDATAFLAT;      // free linear space below kernel code

char   *flat1000;                   // 100:0 flat address
char     *cfgext = 0;               // use alternative config.ext
char   *vfonttgt = 0;               // place to copy vesa font (in doshlp)
char    *pcidata = 0;               // prepared array with pci info
void    *logresv = 0;               // reserved address space for log buffer
u8t      bootsrc = 0;               // alternative mounted boot partition
u8t     bootdisk = 0;               // actual used boot disk
u8t    bootflags = 0;               // actual used boot flags

char    msgpool[2][MSG_POOL_LIMIT]; // temp buffer for parsed messages
int     msgsize[2];
k_arena   arena[ARENACOUNT];        // memory arenas

struct Disk_BPB         RepBPB;

extern boot_data     boot_info;
extern struct Disk_BPB BootBPB;
extern u16t            IODelay;
extern u32t     paeppd, swcode;

physmem_block         *physmem = 0;
u32t           physmem_entries = 0;
u32t             resblock_addr = 0, // reserved memory (log & misc)
                  resblock_len = 0;

// exit with error
void error_exit(int code, const char *message) {
   if (kernel)  { hlp_memfree(kernel); kernel=0; }
   if (doshlp)  { hlp_memfree(doshlp); doshlp=0; }
   if (os2dmp)  { hlp_memfree(os2dmp); os2dmp=0; }
   if (os2sym)  { hlp_memfree(os2sym); os2sym=0; }
   if (mfsdsym) { hlp_memfree(mfsdsym); mfsdsym=0; }
   if (logresv) { hlp_memfree(logresv); logresv=0; }
   if (pcidata) { free(pcidata); pcidata=0; }
   if (kparm)   { free(kparm); kparm=0; }
   // reinit physmem array
   hlp_setupmem(0,0,0,SETM_SPLIT16M);

   if (message) {
      log_printf(message);
      fprintf(stderr, "\n%s", message);
      key_wait(4);
   }
   exit(code);
}

void patch_kernel(module *mi,int isSMP);

typedef struct {
   u32t    fl;
   char    v0;
   char    v1;
} obj_flags_def;

obj_flags_def fprn[] = {
   {OBJREAD,   '-','r'}, {OBJWRITE,  '-','w'}, {OBJEXEC,   '-','x'},
   {OBJBIGDEF, '-','B'}, {OBJSHARED, 'P','s'}, {OBJNOFLAT, 'f','N'},
   {OBJTILE,   '-','T'}, {OBJHIMEM,  'L','h'}, {OBJALIAS16,'-','a'}, {0,0,0}};

char *getfprn(u32t flags) {
   static char rc[16];
   obj_flags_def *pa=fprn;
   char *pp=rc;
   while (pa->fl) { *pp++ = flags&pa->fl?pa->v1:pa->v0; pa++; }
   *pp=0;
   return rc;
}

// update physmem data pointers
void upd_physmem(void) {
   physmem = (physmem_block*)sto_data(STOKEY_PHYSMEM);
   physmem_entries = sto_size(STOKEY_PHYSMEM)/sizeof(physmem_block);
}

/************************************************************************/
// load kernel and fill arena info
/************************************************************************/
void load_kernel(module  *mi) {
   k_arena  *pa = arena, *t16a;
#ifdef MAP_A0000
   k_arena *vma;
#endif
   u32t      ii,
        lopaddr = 0,                         // phys addr for 1st meg objects
        hipaddr = 0,                         // phys addr for high objects
        hiblock = 0,                         // block no# for the line above
        selbase = BOOTHLP_DATASEL,           // free selector base
          isSMP = 0,                         //
          pdiff = hlp_segtoflat(0),          // FLAT addr of 0 page now
         lonext, hinext,                     //
       rvdm_lin, svdm_lin,                   //
      laddrnext = 0,                         // linear addr assigning var
      lowobjcnt = 0;

   // mark all EXEC objects as SHARED
   for (ii=0;ii<mi->objects;ii++)
      if (mi->obj[ii].flags&OBJEXEC) mi->obj[ii].flags|=OBJSHARED;
   // 1st object in SMP kernel is one page MPDATA seg
   isSMP = mi->obj[0].size<=PAGESIZE;
   if (isSMP) lid->BootFlags|=BOOTFLAG_SMP;
   // starting address for low segments
   lopaddr = (DOSHLP_SEG<<PARASHIFT) + respart4k;

   // search block above 1Mb to place kernel
   for (hiblock=0;hiblock<physmem_entries;hiblock++) {
      u32t start = physmem[hiblock].startaddr;
      if (start>=_1MB && start<_16MB)
          if (physmem[hiblock].blocklen>=_1MB+_640KB) { hipaddr = start; break; }
   }
   if (!hipaddr) error_exit(7, "There is no free memory in first 16Mb.\n");

   // assigning physical address & future GDT selector values to kernel objects
   for (ii=0,lonext=lopaddr,hinext=hipaddr;ii<mi->objects;ii++) {
      u32t fl = mi->obj[ii].flags;

      if (fl & OBJALIAS16) {
         if (fl & OBJTILE) {
            if (isSMP && lonext>0x15080 && lonext<0x18000) {
               lonext = 0x18000;
               log_printf("skipping SMP space\n");
            }
            mi->obj[ii].sel = lonext>>PARASHIFT;
         } else
            mi->obj[ii].sel = (selbase += SELINCR);
      }
      if (fl & OBJHIMEM) {
          mi->obj[ii].resd1 = hinext;
          hinext = PAGEROUND(hinext + mi->obj[ii].size);
      } else {
          mi->obj[ii].resd1 = lonext;
          lonext = PAGEROUND(lonext + mi->obj[ii].size);
          lowobjcnt++;
      }
      // address in our`s current PM FLAT space
      mi->obj[ii].address = (void*)(mi->obj[ii].resd1 + pdiff);
   }
   // difference between the virtual and physical addresses of objects
   efd->LAddrDelta = laddrdelta = (MAXSYSPAGE << PAGESHIFT) - hinext;
   // try to detect debug kernel
   isdbgkrnl = lowobjcnt > 6;

   log_printf("laddrdelta=%08X\n",laddrdelta);
   log_printf("ob    flags    oi-flags     paddr/sel    fladdr      msz/vsz\n");

   // for multiple MVDM data segments (where we can find such kernel? ;))
   rvdm_lin = MVDM_INST_DATA_ADDR;
   svdm_lin = MVDM_SINST_DATA_ADDR;

   // assigning kernel linear space addresses
   for (ii=0;ii<mi->objects;ii++) {
      u32t fl = mi->obj[ii].flags;
      if (isSMP && ii==0)    // MPDATA object special address
         mi->obj[ii].fixaddr = MPDATAFLAT;
      else                   // else compute difference from physical location
         mi->obj[ii].fixaddr = laddrdelta + mi->obj[ii].resd1;
      // low end of high virtual address space
      if ((fl&OBJHIMEM) && laddrnext==0) laddrnext = mi->obj[ii].fixaddr;

      // MVDM instance data
      if ((fl&OBJSHARED)==0) {
         if (fl&OBJRESIDENT) {
            mi->obj[ii].fixaddr = rvdm_lin;
            rvdm_lin += PAGEROUND(mi->obj[ii].size);
         } else {
            mi->obj[ii].fixaddr = svdm_lin;
            svdm_lin += PAGEROUND(mi->obj[ii].size);
         }
      }
      // print object info dump
      log_printf("%2x  %s  %08X  %08X/%04X  %08X  %06X/%06X\n", ii+1,
         getfprn(fl), fl, mi->obj[ii].resd1, mi->obj[ii].sel, mi->obj[ii].fixaddr,
            PAGEROUND(mi->obj[ii].size), mi->obj[ii].size);

      if ((mi->flags&MOD_NOFIXUPS)!=0 && mi->obj[ii].orgbase)
         if (mi->obj[ii].orgbase!=mi->obj[ii].fixaddr) {
            log_printf("Kernel object %d misplacement: %08X instead of %08X\n",
               ii+1, mi->obj[ii].fixaddr, mi->obj[ii].orgbase);
            error_exit(6, "Incompatible Warp 3.0 kernel file.\n");
         }
   }

   lid->FlatDS = (selbase += SELINCR);
   lid->FlatCS = (selbase += SELINCR);
   log_printf("laddrnext=%08X, FLAT CS=%04X DS=%04X\n", laddrnext, lid->FlatCS, lid->FlatDS);

   memset(arena, 0, sizeof(arena));  // must be zeroed by hlp_allocmem

   // page 0 arena info.
   pa->a_oflags = OBJREAD|OBJWRITE;
   pa->a_size   = DOSHLP_SEG<<PARASHIFT;
   pa++;
   // doshlp segment
   pa->a_paddr  = DOSHLP_SEG<<PARASHIFT;
   pa->a_size   = respart4k;
   pa->a_laddr  = laddrnext -= respart4k;
   pa->a_sel    = DOSHLP_SEG;
   pa->a_aflags = AF_LADDR | AF_DOSHLP;
   pa->a_oflags = OBJEXEC | OBJALIAS16;
   pa->a_owner  = DOSHLPOWNER;
   pa++;

   lid->DosHlpFlatBase = laddrnext;
   if (vid) vid->FontCopy += laddrnext;
   /** low memory kernel objects.
       calculating laddr from paddr here because MVDM fixaddr field contain
       VDM linear address, not system global */
   for (ii=0;ii<mi->objects;ii++) {
      if ((pa->a_paddr = mi->obj[ii].resd1) < _1MB) {
         pa->a_size    = mi->obj[ii].size;
         pa->a_laddr   = isSMP && !ii? MPDATAFLAT : pa->a_paddr + laddrdelta;
         pa->a_sel     = mi->obj[ii].sel;
         pa->a_aflags  = AF_LADDR | AF_OS2KRNL | ii+1 << AF_OBJSHIFT;
         pa->a_oflags  = mi->obj[ii].flags;
         pa->a_owner   = OS2KRNLOWNER;
         pa++;
      }
   }
   /* we do not load OS2DBCS (sorry :)) and do not backup BIOS data, so
      those arenas are missing in our`s list.
      Beacase no BIOS data backup - no DOS<->OS/2 hot swap support on FAT in
      our loader, DOSHLP will panic on DHSetDosEnv call */

   /* adding arena for OS2DUMP (init located in init2)
      Create it in any way, because pginit code assume at least one
      invalid block in 1st Mb */
   if (pa) {
      u32t da_size = os2dmp ? dmpsize : PAGESIZE*4;
      pa->a_paddr  = PAGEROUND(pa[-1].a_paddr+pa[-1].a_size);
      pa->a_sel    = pa->a_paddr>>PARASHIFT;
      pa->a_aflags = AF_INVALID;
      /* allocate memory for pae disk boot in the same block,
         it must be single page aligned page */
      if (lid->BootFlags&BOOTFLAG_EDISK) {
         if (os2dmp) da_size = PAGEROUND(da_size); else da_size-=PAGESIZE;
         swcode   = pa->a_paddr + da_size;
         da_size += PAGESIZE;
      }
      pa->a_size  = da_size;

      if (os2dmp) {
         efd->DumpSeg = pa->a_sel;

         memcpy((void*)hlp_segtoflat(efd->DumpSeg), os2dmp, dmpsize);
         hlp_memfree(os2dmp); os2dmp = 0;
         log_printf("os2dump: %04X\n", efd->DumpSeg);
      }
      pa++;
   }
   // free low memory
   pa->a_paddr  = PAGEROUND(pa[-1].a_paddr+pa[-1].a_size);
   pa->a_size   = (efd->DisPartSeg << PARASHIFT) - pa->a_paddr;
   pa->a_aflags = AF_FREE;
   pa++;

   // arena record for discardable part of loader
   pa->a_paddr  = efd->DisPartSeg << PARASHIFT;
   pa->a_size   = efd->DisPartLen;
   pa->a_laddr  = laddrnext -= efd->DisPartLen;
   pa->a_aflags = AF_LADDR | AF_OS2LDR;
   pa->a_oflags = OBJREAD | OBJWRITE | OBJALIAS16;
   pa->a_owner  = OS2LDROWNER;
   pa++;
   // save linear addr into info structure
   lid->LdrHighFlatBase = laddrnext;
   // setup arena record for rom data
   pa->a_paddr = (u32t)efd->LowMem<<10;
   if (pa->a_paddr & PAGEMASK) {
      pa->a_paddr &=~PAGEMASK;
      pa->a_size   = PAGESIZE;
      pa->a_aflags = AF_LADDR;
      pa->a_laddr  = (laddrnext -= PAGESIZE) + (pa->a_paddr & PAGEMASK);
      pa->a_owner  = OS2ROMDATA;

      pa[1].a_paddr = pa->a_paddr + pa->a_size;
      pa++;
   }
   // create INVALID entry in case of <640k of memory (boot rom can make this)
   if (pa->a_paddr<_640KB) {
      pa->a_size   = _640KB-pa->a_paddr;
      pa->a_aflags = AF_INVALID;
      pa++;
   }
   // map entire A0000-FFFFF area as linear space
   // (it mapped as INVALID space in IBM loader)
   pa->a_paddr  = _640KB;
   pa->a_size   = _1MB - _640KB;
#ifdef MAP_A0000
   pa->a_aflags = AF_LADDR | AF_DOSHLP;
   pa->a_oflags = OBJREAD | OBJWRITE;
   pa->a_owner  = ALLOCPHYSOWNER;
   // do not touch lndataaddr until RIPL will be placed
   vma = pa++;
#else
   pa->a_aflags = AF_INVALID;
   // incorrect value, but it was used only in removed non-LFB mode
   lid->VMemFlatBase = 1;
   pa++;
#endif // MAP_A0000

   // find first block above or equal to 1meg
   for (ii=0; physmem[ii].startaddr<_1MB; ii++);
   // i hope, there is no such PCs now, but support still here ;)
   while (physmem[ii].startaddr<hipaddr) {
      u32t nextaddr;
      pa->a_paddr  = physmem[ii].startaddr;
      pa->a_aflags = AF_FREE;
      pa->a_size   = physmem[ii].blocklen;
      nextaddr = pa->a_paddr + pa->a_size;
      pa++; ii++;
      if (nextaddr < physmem[ii].startaddr) { // fill holes
         pa->a_paddr  = nextaddr;
         pa->a_aflags = AF_INVALID;
         pa->a_size   = physmem[ii].startaddr - nextaddr;
         pa++;
      }
   }
   // high memory kernel objects
   for (ii=0;ii<mi->objects;ii++) {
      if ((pa->a_paddr = mi->obj[ii].resd1) >= _1MB) {
         pa->a_size    = mi->obj[ii].size;
         pa->a_laddr   = pa->a_paddr + laddrdelta;
         pa->a_sel     = mi->obj[ii].sel;
         pa->a_aflags  = AF_LADDR | AF_OS2KRNL | ii+1 << AF_OBJSHIFT;
         pa->a_oflags  = mi->obj[ii].flags;
         pa->a_owner   = OS2KRNLOWNER;
         pa++;
      }
   }
   // free memory remaining in block with high kernel segments
   pa->a_paddr  = hinext;
   pa->a_size   = physmem[hiblock].startaddr + physmem[hiblock].blocklen - pa->a_paddr;
   pa->a_aflags = AF_FREE;
   pa++;
   // fill blocks between kernel and 16Mb boundary
   for (ii=hiblock+1;ii<physmem_entries;ii++) {
      physmem_block *cb = physmem+ii;
      if (cb->startaddr>hipaddr && cb->startaddr<_16MB) {
         pa->a_paddr = pa[-1].a_paddr + pa[-1].a_size;
         if (cb->startaddr != pa->a_paddr) { // fill holes
             pa->a_size   = cb->startaddr - pa->a_paddr;
             pa->a_aflags = AF_INVALID;
             pa++;
         }
         pa->a_paddr  = cb->startaddr;
         pa->a_size   = cb->blocklen;
         pa->a_aflags = AF_FREE;
         pa++;
      }
   }
   t16a = pa-1;
   // arenas for mini-fsd && ripl boot data
   /* fsd table was validated in start of QSINIT code and here we only setup
      arenas and copy data to the high memory */
   if ((bootflags&(BF_MINIFSD|BF_RIPL))) {
      struct filetable_s *ft = &boot_info.filetab;
      u32t   rreslen = 0,
            b16frame = 0,        // phys. addr for mini-FSD/SYMs
           riplframe = 0,        // phys. addr for RIPL block
          riplframe2 = 0,        // another one RIPL stuff (aurora addition)
             addsize = 0;

      // RIPL exists and have non-zero size data?
      if (ft->ft_cfiles >= 4 && (bootflags&BF_RIPL)!=0 &&
          ft->ft_ripllen)
      {
         // additional check
         if (ft->ft_cfiles==5 && (!ft->ft_resofs||!ft->ft_reslen))
            ft->ft_cfiles = 4;

         if (ft->ft_cfiles == 5) {
            rreslen    = PAGEROUND(ft->ft_reslen + (ft->ft_resofs & PAGEMASK));
            riplframe2 = ft->ft_resofs & ~PAGEMASK;
            riplframe  = riplframe2 - PAGEROUND(ft->ft_ripllen);
            // holes in first 16Mb???
            if (t16a->a_paddr>riplframe)
               error_exit(8, "Unable to handle memory for PXE boot.\n");
            // truncate free 16Mb block
            if (t16a->a_paddr+t16a->a_size > riplframe2+rreslen) {
               addsize = (t16a->a_paddr+t16a->a_size) - (riplframe2+rreslen);
               t16a->a_size -= addsize;
            }
         } else
            riplframe  = t16a->a_paddr + t16a->a_size - PAGEROUND(ft->ft_ripllen);

         log_printf("ripl: %08X\n", riplframe);
         memcpy((void*)(riplframe+pdiff), (void*)hlp_segtoflat(ft->ft_riplseg),
            ft->ft_ripllen);
         //  mfsd is below ripl data
         b16frame = riplframe;
      } else
         b16frame = t16a->a_paddr + t16a->a_size;
      // calculate used space
      if (mfsdsym && msymsize)   b16frame -= PAGEROUND(msymsize + 4);
      if (os2sym && symsize)     b16frame -= PAGEROUND(symsize + 4);
      if (boot_info.minifsd_ptr) b16frame -= PAGEROUND(mfsdsize);

      // copying mini-FSD to final location
      if (boot_info.minifsd_ptr) {
         log_printf("mFSD: %08X\n", b16frame);
         memcpy((void*)(b16frame+pdiff), boot_info.minifsd_ptr, mfsdsize);
         // setup mini-FSD arena
         pa->a_paddr  = b16frame;
         pa->a_size   = PAGEROUND(mfsdsize);
         pa->a_laddr  = laddrnext -= pa->a_size;
         pa->a_aflags = AF_LADDR | AF_FSD;
         pa->a_oflags = OBJREAD | OBJWRITE | OBJALIAS16;
         pa->a_owner  = FSMFSDOWNER;
         t16a->a_size-= pa->a_size;   // fix previous free block size
         b16frame    += pa->a_size;
         pa++;
      } else
         log_printf("no mFSD!\n");
      // copying kernel sym file
      if (os2sym && symsize) {
         u32t *tgt = (u32t*)(b16frame+pdiff);
         // save size and data
         tgt[0] = symsize;
         memcpy(tgt+1, os2sym, symsize);
         // setup arena
         pa->a_paddr  = b16frame;
         pa->a_size   = PAGEROUND(symsize + 4);
         pa->a_laddr  = laddrnext -= pa->a_size;
         pa->a_aflags = AF_LADDR | AF_SYM;
         pa->a_oflags = OBJREAD | OBJWRITE;
         pa->a_owner  = OS4SYMOWNER;
         t16a->a_size-= pa->a_size;   // fix previous free block size
         b16frame    += pa->a_size;
         pa++;
      }
      // copying mini-FSD sym file
      if (boot_info.minifsd_ptr && mfsdsym && msymsize) {
         u32t *tgt = (u32t*)(b16frame+pdiff);
         // save size and data
         tgt[0] = msymsize;
         memcpy(tgt+1, mfsdsym, msymsize);
         // setup arena
         pa->a_paddr  = b16frame;
         pa->a_size   = PAGEROUND(msymsize + 4);
         pa->a_laddr  = laddrnext -= pa->a_size;
         pa->a_aflags = AF_LADDR | AF_SYM;
         pa->a_oflags = OBJREAD | OBJWRITE;
         pa->a_owner  = MFSDSYMOWNER;
         t16a->a_size-= pa->a_size;   // fix previous free block size
         pa++;
      }

      // setup RIPL arena
      if ((bootflags & BF_RIPL) && riplframe) {
         pa->a_paddr  = riplframe;
         pa->a_size   = PAGEROUND(ft->ft_ripllen);
         pa->a_laddr  = laddrnext -= pa->a_size;
         pa->a_aflags = AF_LADDR | AF_RIPL;
         pa->a_oflags = OBJREAD | OBJWRITE | OBJALIAS16;
         pa->a_owner  = OS2RIPLOWNER;
         t16a->a_size-= pa->a_size;   // fix previous free block size
         pa++;

         if (ft->ft_cfiles == 5) {
            pa->a_paddr  = riplframe2;
            pa->a_size   = rreslen;
            pa->a_laddr  = lndataaddr -= pa->a_size;
            pa->a_aflags = AF_LADDR;
            pa->a_oflags = OBJREAD | OBJWRITE;
            pa->a_owner  = ALLOCPHYSOWNER;
            t16a->a_size-= pa->a_size;   // fix previous free block size
            t16a = pa++;
            // block between end of PXE data & 16Mb (if present)
            if (addsize) {
               pa->a_paddr  = t16a->a_paddr + t16a->a_size;
               pa->a_size   = addsize;
               pa->a_aflags = AF_FREE;
               pa++;
            }
         }
      }
   }
#ifdef MAP_A0000
   // assign linear address for A0000-FFFFF block
   vma->a_laddr = lndataaddr -= vma->a_size;
   lid->VMemFlatBase = vma->a_laddr;
#endif
   // invalid arena at the end of 16Mb (need for kernel?)
   pa->a_paddr  = pa[-1].a_paddr + pa[-1].a_size;
   if (pa->a_paddr == _16MB) {
      pa->a_aflags = AF_INVALID;
      pa++;
   }
   // setup arenas for memory above 16 Mb
   ii = 0;
   while (ii<physmem_entries) {
      physmem_block *pb = physmem+ii;
      if (pb->startaddr>=_16MB) {
         pa->a_paddr  = pb->startaddr;
         pa->a_size   = pb->blocklen;

         // DTOC PXE loader data grow above 16Mb - fix this block
         if (t16a->a_paddr + t16a->a_size > pa->a_paddr) {
            // block covered - skip it
            if (t16a->a_paddr + t16a->a_size >= pa->a_paddr + pa->a_size) {
               ii++;
               continue;
            }
            pa->a_paddr = t16a->a_paddr + t16a->a_size;
            pa->a_size -= pa->a_paddr - pb->startaddr;
         }
         pa->a_aflags = AF_FREE;
         pa++;
         pa->a_paddr  = pa[-1].a_paddr + pa[-1].a_size;
         pa->a_aflags = AF_INVALID;
         pa++;
      }
      /* create log arena.
         * previous variant allocate linear addr below FF800000 and make
           used phys mapped arena for it
         * now we just mark it as INVALID and call VMAlloc(phys) in OEMHLP
           init. This block will be the last in list & kernel removes it, but
           we still add, at least to see it in arena table */
      if (resblock_len && resblock_addr>pb->startaddr &&
        (ii==physmem_entries-1 || resblock_addr<physmem[ii+1].startaddr))
      {
         pa->a_paddr  = resblock_addr;
         pa->a_size   = resblock_len << 16;
#ifdef MAP_LOGADDR
         pa->a_aflags = AF_LADDR | AF_DOSHLP;
         pa->a_laddr  = lndataaddr -= pa->a_size;
         pa->a_oflags = OBJREAD | OBJWRITE;
         pa->a_owner  = ALLOCPHYSOWNER;
         efd->LogBufVAddr = pa->a_laddr;
         pa++;
         pa->a_paddr  = pa[-1].a_paddr + pa[-1].a_size;
         pa->a_aflags = AF_INVALID;
         pa++;
#else
         pa->a_aflags = AF_INVALID;
         pa++;
#endif // MAP_LOGADDR
      }
      ii++;
   }
   if (pa[-1].a_aflags==AF_INVALID && !pa[-1].a_size) pa--;
#ifdef MAP_REMOVEHOLES
   /* create mapped blocks instead of holes to make happy this stupid
      pginit.c in kernel ... 

      Trying thousand times - and this is a better variant.
      Kernel code doesn`t like holes is memory, kernel code doesn`t
      like preallocated log, kernel code doesn`t like anything, except
      plain single block of upper memory. */
   for (ii=0; arena+ii!=pa-1; ii++) {
      k_arena *ppa = arena + ii;
      if (!ppa->a_size) {
         ppa->a_size = arena[ii+1].a_paddr - ppa->a_paddr;
         if (ppa->a_size && ppa->a_aflags == AF_INVALID) {
            ppa->a_aflags = AF_LADDR | AF_DOSHLP;
            ppa->a_laddr  = lndataaddr -= ppa->a_size;
            ppa->a_oflags = OBJREAD | OBJWRITE;
            ppa->a_owner  = ALLOCPHYSOWNER;
         }
      }
   }
#endif
#ifdef MAP_LOGADDR
   /* align first free linear address to page dir entry border!
      to do this - we mark NON-present memory as used!

      THIS MUST BE THE LAST LINEAR ALLOCATION */
   if ((lndataaddr&~PD4M_ADDRMASK)!=0) {
      pa->a_aflags = AF_LADDR | AF_DOSHLP;
      pa->a_paddr  = pa[-1].a_paddr + pa[-1].a_size;
      pa->a_laddr  = lndataaddr&PD4M_ADDRMASK;
      pa->a_oflags = OBJREAD | OBJWRITE;
      pa->a_size   = lndataaddr - pa->a_laddr;
      pa->a_owner  = ALLOCPHYSOWNER;

      lndataaddr   = pa->a_laddr;
      pa++;
   }
#endif // MAP_LOGADDR
   pa->a_paddr  = pa[-1].a_paddr + pa[-1].a_size;
   pa->a_size   = 0;
   pa->a_aflags = AF_PEND;
   pa++;
   // the end
   pa->a_aflags = AF_VEND;
   pa++;

   arenacnt = pa - arena;
#ifndef MAP_REMOVEHOLES
   for (ii=0;ii<arenacnt-2;ii++)
      if (!arena[ii].a_size)
         arena[ii].a_size = arena[ii+1].a_paddr - arena[ii].a_paddr;
#endif
   /* mark 1Mb as AF_FAST - not required at all, but make the same view with
      IBM arena list - so add to be less confused */
   pa = arena;
   while (!(pa->a_aflags & AF_VEND)) {
      if (pa->a_paddr<0xA0000) pa->a_aflags|=AF_FAST;

      log_printf("pa=%08X sz=%08X va=%08X sel=%04X fl=%04X of=%08X ow=%04X\n",
         pa->a_paddr, pa->a_size, pa->a_laddr, pa->a_sel, pa->a_aflags,
            pa->a_oflags, pa->a_owner);
      pa++;
   }

   /* assign FLAT selectors for 32 bit ring 0 objects.
      this action required for fixup worker, but must be performed after
      arena list creation */
   for (ii=0;ii<mi->objects;ii++)
      if (!mi->obj[ii].sel) {
         u32t fl = mi->obj[ii].flags;
         if ((fl&(OBJSHARED|OBJBIGDEF))==(OBJSHARED|OBJBIGDEF))
            mi->obj[ii].sel = fl&OBJEXEC?lid->FlatCS:lid->FlatDS;
      }

   // actually load kernel ;)
   for (ii=0;ii<mi->objects;ii++) krnl_loadobj(ii);
   // query some data from loaded objects
   patch_kernel(mi,isSMP);
   // we are ready!
   efd->OS2Init = krnl_done();
   log_printf("OS2Init=%04X:%04X\n", efd->OS2Init>>16, efd->OS2Init&0xFFFF);
}

/************************************************************************/
// form resident/discardable message data
/************************************************************************/
u32t create_msg_pool(char *pool,const char *section) {
   str_list* keys=str_keylist(msgini_name,section,0);
   int res=MSG_POOL_LIMIT, count=0, ii;
   char *ps=pool;

   if (keys) {
      char *lpool=0;
      count=keys->count;
      ii   =0;
      while (ii<count) {
         char msg[384], *ch;
         u16t msg_id;
         int  ln;
         msg[0] = 0;
         ini_getstr(section,keys->item[ii],"",msg,384,msgini_name);
         // count EOL substs and add one more char for every one
         ln = strccnt(msg,'^');
         // full result length
         ln+= strlen(msg);
         // buffer overflow?
         if (res-ln-1-4<0) break;
         msg_id = atoi(keys->item[ii]);
         if (msg_id) {
            lpool = ps;
            *(u16t*)ps = msg_id;  ps+=2;
            *(u16t*)ps = ln;      ps+=2;
            strcpy(ps,msg);
            ch=strchr(ps,'^');
            while (ch) {
               memmove(ch+1,ch,strlen(ch)+1);
               *ch++='\n';
               *ch++='\r';
               ch=strchr(ch,'^');
            }
            ps+=ln+1;
            res-=ps-lpool;
         }
         ii++;
      }
      // there is no space for ending zero? remove last message.
      if (lpool && res-4<0) {
         count--;
         ps = lpool;
      }
   }
   *(u32t*)ps = 0; ps+=4;
   ii = ps-pool;
   log_printf("%s messages: %d bytes, %d entries\n",section,ii,count);
   return ii;
}

/************************************************************************/
// ini key search
/************************************************************************/

// is key present? (with argument or not)
#define key_present(key) key_present_pos(key,0)

// is key present? for subsequent search of the same parameter
char *key_present_pos(const char *key, u32t *pos) {
   u32t ii=pos?*pos:0, len = strlen(key);
   if (!kparm||ii>=kparm->count) return 0;
   for (;ii<kparm->count;ii++) {
      u32t plen = strlen(kparm->item[ii]);
      if (plen>len+1 && kparm->item[ii][len]=='=' || plen==len)
         if (strnicmp(kparm->item[ii],key,len)==0) {
            char *rc=kparm->item[ii]+len;
            if (*rc=='=') rc++;
            if (pos) *pos=ii;
            return rc;
         }
   }
   return 0;
}

/************************************************************************/
// init memory related parameters
/************************************************************************/

void memparm_init(void) {
   char *parmptr = key_present("MEMLIMIT");
   u32t  limit = 0, ps, rmvmem[REMOVEMEM_LIMIT], ii, hsize, exrsize = 0;
   if (parmptr)
      if ((limit = strtoul(parmptr,0,0))<16) limit = 0;
   // check memlimit from VMTRR
   ps = sto_dword(STOKEY_MEMLIM);
   if (ps)
      if ((!limit || ps<limit) && ps>=16) {
         limit = ps;
         log_printf("VMTRR limit us to %uMb\n", ps);

      }
   // limit warp kernel memory to 2Gb
   if (lid->BootFlags&BOOTFLAG_WARPSYS)
      if (!limit || limit>2048) limit=2048;
   // additional requirements
   if (lid->BootFlags&BOOTFLAG_EDISK) exrsize = PAEIO_MEMREQ>>16;
   // fill removemem array
   ps = ii = 0;
   do {
      parmptr = key_present_pos("REMOVEMEM",&ps);
      if (parmptr)
         if ((rmvmem[ii] = strtoul(parmptr,0,0))!=0) ii++;
      ps++;
   } while (parmptr && ii<REMOVEMEM_LIMIT-1);
   rmvmem[ii] = 0;
   // query log size in 64k blocks
   parmptr = key_present("LOGSIZE");
   if (parmptr)
      if ((resblock_len = strtoul(parmptr,0,0))<64) resblock_len = 0; else {
         if (resblock_len>32768) resblock_len = 32768;
         resblock_len>>=6;
      }
   resblock_len    += exrsize;
   resblock_addr    = hlp_setupmem(limit, &resblock_len, rmvmem, SETM_SPLIT16M);
   efd->LogBufPAddr = resblock_addr;

   if (resblock_len<exrsize)
      error_exit(12,"Unable to reserve memory for RAM disk boot!\n");
   if (exrsize) {
      // get part of allocated log space for ram disk boot i/o
      efd->LogBufSize = resblock_len  - exrsize;
      paeppd = resblock_addr + (efd->LogBufSize<<16);
      // no log at all?
      if (!efd->LogBufSize) efd->LogBufPAddr = 0;
   } else
      efd->LogBufSize = resblock_len;
   upd_physmem();

   log_printf("physmem after split/resetup:\n");
   for (ii=0;ii<physmem_entries;ii++)
      log_printf("%2d. Base:%08X Size:%08X\n",ii+1,physmem[ii].startaddr,
         physmem[ii].blocklen);

   // calculate high/extend mem values. lowmem was filled in doshlp init
   ii = 0;
   hsize = 0;
   efd->ExtendMem = 0;
   while (physmem[ii].startaddr<_1MB) ii++;
   while (physmem[ii].startaddr<_16MB && ii<physmem_entries) {
      u32t bend = physmem[ii].startaddr + physmem[ii].blocklen;

      if (bend < _16MB) hsize += physmem[ii].blocklen; else {
         hsize += _16MB - physmem[ii].startaddr;
         efd->ExtendMem = bend - _16MB;
      }
      ii++;
   }
   efd->HighMem = hsize>>10;

   while (ii<physmem_entries) efd->ExtendMem+=physmem[ii++].blocklen;
   efd->ExtendMem>>=10;

   if (efd->LogBufPAddr) {
      lid->BootFlags|=BOOTFLAG_LOG;
      log_printf("log: phys %08X, size %dkb\n", efd->LogBufPAddr, efd->LogBufSize<<6);
   }
   // try to reserve allocated log address in memory manager
   if (resblock_addr)
      logresv = hlp_memreserve(resblock_addr, resblock_len<<16);
}

/************************************************************************/
// patch IBM kernel binary data
/************************************************************************/

void patch_kernel(module *mi,int isSMP) {
   char *symname = key_present("SYM"), sym[12];
   int   dosdata = -1,
          gdtseg = -1;
   u32t  ii;
   for (ii=0;ii<mi->objects;ii++) {
      // search for OS/4 in xxx:4 of low code segment (DOSCODE now)
      if ((mi->obj[ii].flags & (OBJEXEC|OBJHIMEM)) == OBJEXEC)
         if (((u32t*)mi->obj[ii].address)[1] == OS4MAGIC) isos4krnl = 1;
      // check for 'SAS ' string in low data segment
      if ((mi->obj[ii].flags & (OBJEXEC|OBJHIMEM)) == 0)
         if (mi->obj[ii].size>0x1000 && *(u32t*)mi->obj[ii].address==0x20534153)
            dosdata = ii;
      // locate GDT seg object
      if ((mi->obj[ii].flags & (OBJNOFLAT|OBJINVALID)) == OBJNOFLAT)
         gdtseg = ii;
   }
   // get kernel version from InfoSeg
   if (gdtseg>0 && dosdata>=0)
      if (mi->obj[gdtseg].size>INFOSEG_SEL+8) {
         u16t  ofs = *(u16t*)((u8t*)mi->obj[gdtseg].address + INFOSEG_SEL + 2);
         // is offset in DOSDATA correct?
         if (mi->obj[dosdata].size>=ofs+INFOSEG_SIZE) {
            u8t *objaddr = (u8t*)mi->obj[dosdata].address,
                   major = objaddr[ofs+INFOSEG_VERSION],
                   minor = objaddr[ofs+INFOSEG_VERSION+1];
            if (major==20) {
               efd->KernelVersion = minor;
               log_printf("Kernel ver.: %d.%d %s\n", minor/10, minor%10,
                  isSMP?"SMP":"");
            }
         }
      }
   // uppercase SYM file name
   if (symname) {
      strncpy(sym,symname,12); sym[11]=0;
      strupr(sym);
   }
   // patch various data in DOSINITDATA segment
   if ((lid->BootFlags&(BOOTFLAG_NOLOGO|BOOTFLAG_CFGEXT)) || symname) {
      // search for last low DATA object - this must be DOSINITDATA both on UNI & SMP
      for (ii=0;ii<mi->objects;ii++)
         if ((mi->obj[ii].flags&(OBJWRITE|OBJHIMEM))==OBJWRITE && ii<mi->objects-1
            && (mi->obj[ii+1].flags&(OBJEXEC|OBJHIMEM))==(OBJEXEC|OBJHIMEM))
         {
            u8t  *objaddr = (u8t*)mi->obj[ii].address;
            u32t  objsize = mi->obj[ii].size;
            log_printf("DOSINITDATA: %d %04X %d\n", ii+1, mi->obj[ii].sel, objsize);

            // change config.sys ext in IBM kernels
            if (!isos4krnl && (lid->BootFlags&BOOTFLAG_CFGEXT)) {
               int err = patch_binary(objaddr,objsize,4,"CONFIG.SYS",0,7,cfgext,4,0);
               if (err) log_printf("config.sys ext patch err: %d\n",err);
            }
            // rename "OS2LOGO" to "OS2NONE" ;)
            if (lid->BootFlags&BOOTFLAG_NOLOGO)
               patch_binary(objaddr,objsize,1,"OS2LOGO",0,3,"NONE",4,0);
            // replace .SYM file name to specified one
            if (symname&&sym[0]) {
               int err = patch_binary(objaddr,objsize,1,"OS2KRNL.SYM",0,0,sym,11,0);
               if (err) log_printf("sym name patch err: %d\n",err);
            }
            break;
         }
   }
   // print kernel revision from DOSDATA
   if (mi->objects>2) {
      ii = isSMP?1:0;
      if ((mi->obj[ii].flags & (OBJWRITE|OBJHIMEM))==OBJWRITE) {
          u8t *objaddr = (u8t*)mi->obj[ii].address;
          u32t  revpos = 0;
          int err = patch_binary(objaddr,mi->obj[ii].size,1,"Internal re",0,12,0,0,&revpos);

          if (!err) log_printf("Kernel name: %s\n",objaddr+mi->obj[ii].size-revpos-1); else
             log_printf("unable to find revision string (%d)\n",err);
      }
   }
}

/************************************************************************/
// load optional files to put it into memory
/************************************************************************/

void check_size(void **ptr, u32t *size, u32t limit, const char *info) {
   if (!*ptr || !*size) return;
   if (*size > limit) {
      log_printf("%s file too large (%d bytes), disable it\n", info, *size);
      hlp_memfree(*ptr);
      *ptr  = 0;
      *size = 0;
   }
}

void *read_file(const char *path, u32t *size, int kernel) {
   void *rc = 0;
   *size = 0;
   if (path[1]!=':') {
      if (bootsrc) {
         char *cp = strdup("A:\\");
         cp[0] += bootsrc;
         cp     = strcat_dyn(cp,path);
         rc     = freadfull(cp,size);
         free(cp);
      }
      if (!rc) rc = hlp_freadfull(path,size,0);
      if (!rc && kernel && strnicmp(path,"OS2KRNL",8)==0) {
         rc = hlp_freadfull("OS2KRNLI",size,0);
         if (rc) twodiskb=1;
      }
   }
   // no? trying to read from our`s virtual disk
   if (!rc) rc = freadfull(path,size);
   return rc;
}

void load_miscfiles(const char *kernel) {
   char *parmptr;
   // load os2dump
   os2dmp = read_file(os2dmp_name, &dmpsize, 0);
   check_size(&os2dmp, &dmpsize, _64KB, "Dump");
   // load sym to separate arena
   if (key_present("LOADSYM")) {
      char sym[64];
      parmptr = key_present("SYM");
      if (parmptr) strcpy(sym,parmptr); else
      if (strlen(kernel)<=60) {
         strcpy(sym,kernel);
         strcat(sym,".SYM");
      } else
         *sym=0;
      if (sym[0]) {
         os2sym = read_file(sym, &symsize, 0);
         // check file size
         check_size(&os2sym, &symsize, _512KB, "Sym");
      }
   }
   // mini-FSD present, check SYM key
   if (boot_info.minifsd_ptr) {
      parmptr = key_present("MFSDSYM");
      if (parmptr) {
         mfsdsym = read_file(parmptr, &msymsize, 0);
         // check file size
         check_size(&mfsdsym, &msymsize, _256KB, "Mfsd sym");
      }
   }
}

/************************************************************************/
// main :)
/************************************************************************/
void main(int argc,char *argv[]) {
   char    *parmptr;
   int     testmode = 0,
            memview = 0;
   u32t    kernsize = 0,        // kernel image size
         pcidatalen = 0;
   module       *mi = 0;        // this block will lost on failure exit.
   struct Disk_BPB *dstbpb;

   if (argc<2 || !strcmp(argv[1],"/?")) {
      cmd_shellhelp("BOOTOS2",CLR_HELP);
      exit(1);
   }
#if 0
   // check module size field
   if (_Module->own_size!=sizeof(module)+(_Module->objects-1)*sizeof(mod_object))
      error_exit(2,"This bootos2 compiled with different version of qsinit!\n");
#endif
   // kernel boot parameters
   kparm = str_split(argc>2?argv[2]:"",",");
   log_printf("loading %s,\"%s\"\n",argv[1],argv[2]);
   // default boot disk/flags values
   bootdisk  = boot_info.boot_disk;
   bootflags = boot_info.boot_flags;
   // switch boot source to another partition / ram disk
   parmptr = key_present("SOURCE");
   if (parmptr) {
      int serr = 1;
      if (!parmptr[1] || parmptr[1]==':' && !parmptr[2]) {
         char dsk = toupper(parmptr[0]);
         if (dsk>='C' && dsk<='A'+9) dsk = '0'+ (dsk-'A');
         /* check volume is mounted FAT partition, make fake BPB for it
            and switch flags to FAT boot type */
         if (dsk>='2' && dsk<='9')
            if (replace_bpb(dsk - '0', &RepBPB)) {
               serr      = 0;
               bootsrc   = dsk - '0';
               bootdisk  = RepBPB.BPB_BootDisk;
               bootflags = 0;
            }
      }
      if (serr) error_exit(10,"Invalid volume in \"SOURCE\" parameter!\n");
   }
   // mini-fsd present
   if (boot_info.minifsd_ptr) {
      mfsdsize = boot_info.filetab.ft_mfsdlen;
      log_printf("mini-fsd len: %d\n",mfsdsize);
   } else
   if ((bootflags&BF_MICROFSD)!=0)
      error_exit(9,"Mini-FSD is absent in FSD boot mode!\n");
   /* read kernel from "boot replacement" partition if present,
      then from boot FS */
   kernel = read_file(argv[1], &kernsize, 1);
   if (!kernel) {
      char mbuf[128];
      snprintf(mbuf,128,"Unable to load kernel file \"%s\"!\n",argv[1]);
      error_exit(3,mbuf);
   }
   /* some kind of safeness ;) see comment in start\loader\lxmisc.c */
   kernel = hlp_memrealloc(kernel, kernsize+4);

   doshlp = freadfull("1:\\os2boot\\doshlp",&dhfsize);
   if (!doshlp || *(u16t*)doshlp!=DOSHLP_SIGN)
      error_exit(4,"Valid \"os2boot\\doshlp\" missing in loader data!\n");
   else {
      char bslist[256];
      int sz=ini_getstr(0,0,0,bslist,256,msgini_name), succ=0;
      if (sz) {
         char *cp=bslist;
         while (*cp) {
            if (stricmp("Resident",cp)==0) succ|=1; else
            if (stricmp("Discardable",cp)==0) succ|=2;
            cp+=strlen(cp)+1;
         }
         if (succ!=3) sz=0;  // both sections must exists
      }
      if (!sz)
         error_exit(5,"Valid \"os2boot\\messages.ini\" is missing in loader data!\n");
   }
   // convert messages from ini to binary data for doshlp
   msgsize[0] = create_msg_pool(msgpool[0],"Resident");
   msgsize[1] = create_msg_pool(msgpool[1],"Discardable");
   // reading physmem data
   //upd_physmem();
   // copying binary doshlp to 100:0
   flat1000 = (char*)hlp_segtoflat(DOSHLP_SEG);
   memcpy(flat1000, doshlp, dhfsize);
   hlp_memfree(doshlp); doshlp=0;
   /* read os2dump, and sym files here because i/o can be closed before
      arenas fill on PXE boot */
   load_miscfiles(argv[1]);
   // doshlp interfaces
   lid = (struct LoaderInfoData*) (flat1000 + LDRINFO_OFS);
   efd = (struct ExportFixupData*)(flat1000 + ((u16t*)flat1000)[1]);
   log_printf("doshlp file=%d, size=%d\n",dhfsize,efd->DosHlpSize);
   // zero doshlp BSS
   if (dhfsize<efd->DosHlpSize)
      memset(flat1000+dhfsize, 0, efd->DosHlpSize-dhfsize);

   strncpy(lid->BootKernel,twodiskb?"OS2KRNLI":argv[1],14);
   lid->BootKernel[13]=0;
   // write OS/4 signature to 100:0
   ((u32t*)lid)[-1] = OS4MAGIC;
   // initial flags value
   efd->Flags    = twodiskb?EXPF_TWODSKBOOT:0;
   // signature (for hd4disk - it allow him check loader type)
   efd->InfoSign = EXPDATA_SIGN;
   // pae ram disk physical page or 0 if not present
   efd->HD4Page  = sto_dword(STOKEY_VDPAGE);

   // check keys
   cfgext  = key_present("CFGEXT");
   if (cfgext) strncpy((char*)&lid->ConfigExt,cfgext,3);
   parmptr = key_present("DBPORT");
   if (parmptr) {
      u32t baud = 0;
      u16t port = hlp_seroutinfo(&baud);
      lid->DebugPort = strtoul(parmptr,0,0);
      if (port!=lid->DebugPort)
         log_printf("warning! dbport mismatch %04X %04X\n",port,lid->DebugPort);
      if (lid->DebugPort) {
         // COM port debug
         lid->DebugTarget = DEBUG_TARGET_COM;
         if (key_present("CTRLC"))   lid->DebugTarget|=DEBUG_CTRLC_ON;
         if (key_present("FULLCABLE")) efd->Flags|=EXPF_FULLCABLE;
         // baud rate parameter exist
         parmptr = key_present("BAUDRATE");
         if (parmptr) efd->BaudRate = strtoul(parmptr,0,0); else
            efd->BaudRate = baud?baud:BD_115200;
         /* set another baud rate for the same port - change it for QSINIT too
            (doshlp binary use own COM port code, so rate must be equal) */
         if (port==lid->DebugPort && efd->BaudRate!=baud)
            if (hlp_seroutset(port,efd->BaudRate))
               log_printf("baud rate changed to %d\n",efd->BaudRate);
      } else {
         u32t lines=0;
         // VIO screen output
         lid->DebugTarget = DEBUG_TARGET_VIO;
         efd->BaudRate    = BD_19200;
         // set mode to 80x50
         if (!vio_getmode(0,&lines)||lines!=50) vio_setmode(50);
      }
      // debug present
      if (lid->DebugTarget) {
         if (key_present("VERBOSE")) lid->DebugTarget|=DEBUG_VERBOSE_LOG;
         log_printf("dbport=%04X, flags=%04X\n",lid->DebugPort,lid->DebugTarget);
      }
   }
   // test mode?
   if (key_present("TEST")) testmode = 1;
   if (key_present("VIEWMEM")) memview = 1;
   /* PXE boot? call mfs_term, but only if this is not test & we`re not
      planning to call SYSVIEW later */
   if (boot_info.filetab.ft_cfiles==6 && !testmode && !memview) hlp_fdone();
   // read 1Mb memory size
   efd->LowMem = int12mem();
   // setup flags
   lid->BootFlags = (key_present("NOLOGO")?BOOTFLAG_NOLOGO:0)|
                    (key_present("NOREV")?BOOTFLAG_NOREV:0)|
#ifdef MAP_A0000
                    (key_present("NOLFB")?0:BOOTFLAG_LFB)|
#else
                    BOOTFLAG_LFB|
#endif
                    (key_present("PRELOAD")?BOOTFLAG_PRELOAD:0)|
                    (key_present("CHSONLY")?BOOTFLAG_CHS:0)|
                    (key_present("NOAF")?BOOTFLAG_NOAF:0)|
                    (cfgext?BOOTFLAG_CFGEXT:0);
   // open kernel file
   mi = krnl_readhdr(kernel,kernsize);
   // this is warp kernel (pre-applied fixups)
   if (mi->flags&MOD_NOFIXUPS) lid->BootFlags|=BOOTFLAG_WARPSYS;
   // store MTRR setup for secondary CPUs or reset all changes
   if (hlp_mtrrchanged(1,1,1)) {
      parmptr = key_present("NOMTRR");
      if (!parmptr) {
         efd->MsrTableCnt = hlp_mtrrbatch(flat1000 + efd->MsrTableOfs);
         log_printf("%d msr entries saved\n",(u32t)efd->MsrTableCnt);
      } else
      if (!testmode) hlp_mtrrbios();
   }
   // disk access mode / type
   if ((bootflags&(BF_NOMFSHVOLIO|BF_RIPL))==0) {
      u32t btdsk = bootdisk^QDSK_FLOPPY,
            mode = hlp_diskmode(btdsk,HDM_QUERY);
      // mode is valid?
      if ((mode&HDM_QUERY)!=0) {
         /* boot from emulated ram disk?
            check - is it VDISK made or something else? */
         if (mode&HDM_EMULATED) {
            if (efd->HD4Page) {
               u32t dsk = dsk_strtodisk((char*)sto_data(STOKEY_VDNAME));
               if (dsk==btdsk) lid->BootFlags|=BOOTFLAG_EDISK;
                  else log_printf("emu disk mismatch: %X %X\n", dsk, btdsk);
            }
            if ((lid->BootFlags&BOOTFLAG_EDISK)==0)
               error_exit(11,"Uncknown subst disk type!\n");
         } else
         // is CHS access is not specified manually, query it in disk i/o
         if ((lid->BootFlags&BOOTFLAG_CHS)==0 && (mode&HDM_USELBA)==HDM_USECHS)
            lid->BootFlags|=BOOTFLAG_CHS;
      } else
      if ((btdsk&QDSK_FLOPPY)==0) error_exit(11,"Uncknown disk type!\n");
   }
   // init some memory and log parameters
   memparm_init();

   // i/o delay (used in init1)
   efd->IODelay = IODelay;
   log_printf("i/o delay value: %d\n",IODelay);
   // buffer (used in vesa detect)
   efd->Buf32k  = boot_info.diskbuf_seg;
   log_printf("buf32k: %04X\n",efd->Buf32k);
   // copying Boot BPB (or fake BPB created for "replacement partition")
   dstbpb = (struct Disk_BPB*)(flat1000+efd->BootBPBOfs);
   memcpy(dstbpb, bootsrc?&RepBPB:&BootBPB, sizeof(struct Disk_BPB));
   // drive letter specified
   parmptr = key_present("LETTER");
   if (parmptr) {
      char drv = toupper(parmptr[0]);
      if (drv>='C'&&drv<='Z') {
         dstbpb->BPB_BootLetter = 0x80+(drv-'C');
         log_printf("Forcing %c: boot drive letter\n",drv);
      }
   }
   if ((bootflags&(BF_NOMFSHVOLIO|BF_RIPL))==0) print_bpb(dstbpb);
   // boot disk & os2ldr flags (used in disk init)
   *(u16t*)(flat1000+efd->BootDisk) = bootdisk|bootflags<<8;
   log_printf("boot disk %02X flags %02X!\n", bootdisk, bootflags);

   // saving logo mask to first dw of buffer for use by VesaDetect
   if ((lid->BootFlags&BOOTFLAG_NOLOGO)==0) {
      u16t mask = M640x480x8|M640x480x15|M640x480x16|M640x480x24|M640x480x32, *bptr;
      parmptr = key_present("LOGOMASK");
      if (parmptr) mask = strtoul(parmptr,0,0);
      bptr    = (u16t*)hlp_segtoflat(efd->Buf32k);
      *bptr++ = mask;
      *bptr++ = lid->BootFlags&BOOTFLAG_LFB?0:1;
   }
   // calling init1() method
   log_printf("init1\n");
   hlp_rmcall(MAKEFAR16(DOSHLP_SEG,efd->Init1Ofs),0);
   log_printf("init1 done!\n");

   // -------------------------------------------------------------
   // doshlp size
   respart4k = efd->DisPartOfs + msgsize[0];
   // build PCI arrays
   if (efd->Flags&EXPF_PCI) {
      pci_location dev;
      dd_list a_bus=NEW(dd_list), a_vid=NEW(dd_list), a_cls=NEW(dd_list);
      // enum PCI
      int ok = hlp_pcigetnext(&dev,1,0), ii, devcnt;
      while (ok) {
         a_bus->add(dev.bus|(u32t)(dev.slot<<3&0xF8|dev.func&0x7)<<8);
         a_vid->add((u32t)dev.vendorid<<16|dev.deviceid);
         a_cls->add((u32t)dev.classcode<<8|dev.progface);
         ok = hlp_pcigetnext(&dev,0,0);
      }
      devcnt = a_bus->count();
      // drop PCI flag if unable to find devices
      if (!devcnt) efd->Flags&=~EXPF_PCI; else {
         pcidatalen    = devcnt*10;
         pcidata       = (char*)malloc(pcidatalen);
         efd->PCICount = devcnt;
         memcpy(pcidata, a_cls->array(), devcnt*4);
         memcpy(pcidata+devcnt*4, a_vid->array(), devcnt*4);
         for (ii=0; ii<devcnt; ii++)
           ((u16t*)(pcidata+devcnt*8))[ii] = a_bus->value(ii);
         // allocate aligned place in resident part
         efd->PCIClassList  = Round4(respart4k);
         efd->PCIVendorList = efd->PCIClassList + devcnt*4;
         efd->PCIBusList    = efd->PCIVendorList + devcnt*4;
         respart4k          = efd->PCIBusList + devcnt*2;
         log_printf("%d pci devices saved\n",devcnt);
      }
      // free lists
      DELETE(a_cls); DELETE(a_vid); DELETE(a_bus);
   }
   // logo present? add space for saving BIOS font in doshlp
   if ((lid->BootFlags&BOOTFLAG_NOLOGO)==0) {
      vid = (struct VesaInfoBuf*)(flat1000 + lid->VesaInfoOffset);
#ifndef MAP_A0000
      if (!vid->LFBAddr) { // no LFB - drop logo
         lid->BootFlags |= BOOTFLAG_NOLOGO;
      } else
#endif
      {  // future address
         vfonttgt = flat1000 + (vid->FontCopy = respart4k);
         lid->BootFlags |= BOOTFLAG_VESAFNT;
         respart4k      += VESAFONTBUFSIZE;
      }
   }
   // doshlp size, rounded to nearest page
   respart4k = PAGEROUND(respart4k);
   // -------------------------------------------------------------
   /* discardable part (boothlp) include: resident part (as a garbage), self,
      512 bytes disk buffer, stack and arena list for kernel (one page) */
   efd->DisMsgOfs  = SECTROUND(efd->DosHlpSize) + SECTSIZE;
   efd->DisPartLen = PAGEROUND(efd->DisMsgOfs + msgsize[1] + STACK_SIZE) + PAGESIZE;
   // both len & Lowmem aligned to page, so segment will (and must) be aligned too
   efd->DisPartSeg = (((u32t)efd->LowMem<<10) & ~PAGEMASK) - efd->DisPartLen >> PARASHIFT;
   efd->ResMsgOfs  = efd->DisPartOfs;
   log_printf("boothlp seg: %04X!\n",efd->DisPartSeg);
   log_printf("Low/High/Ext: %d/%d/%d\n",efd->LowMem,efd->HighMem,efd->ExtendMem);
   log_printf("flags: %08X, dis msgs: %04X\n",lid->BootFlags,efd->DisMsgOfs);

   /* we need to split doshlp data processing here, because load_kernel()
      can setup resident arena size smaller, than DosHlpSize, and some
      of flat1000 data will be destroyd */
   doshlp = hlp_memalloc(efd->DosHlpSize, 0);
   memcpy(doshlp, flat1000, efd->DosHlpSize);
   // fixup table to fill in
   parmptr = efd->Io32FixupOfs<efd->DisPartOfs?flat1000:(char*)doshlp;
   krnl_fixuptable((u32t*)(parmptr+efd->Io32FixupOfs), &efd->Io32FixupCnt);
   // update vid addr
   parmptr = lid->VesaInfoOffset<efd->DisPartOfs?flat1000:(char*)doshlp;
   vid = (struct VesaInfoBuf*)(parmptr + lid->VesaInfoOffset);

   load_kernel(mi);

   // setup ram disk boot configuration
   if (lid->BootFlags&BOOTFLAG_EDISK)
      setup_ramdisk(bootdisk^QDSK_FLOPPY, efd, (char*)doshlp);
   /* turn on COM2 port for IBM debug kernel without OS2LDR.INI 
      (actually, without parameters) */
   if (isdbgkrnl && !isos4krnl && kparm->count==0 && (lid->DebugTarget&DEBUG_TARGET_MASK)==0) {
      lid->DebugPort   = COM2_PORT;
      efd->BaudRate    = BD_19200;
      lid->DebugTarget = DEBUG_TARGET_COM;
   }
   // target to temporary place discardable part
   parmptr = (char*)hlp_segtoflat(boot_info.diskbuf_seg);
   // copying discardable part to disk buffer (temporary)
   memcpy(parmptr, doshlp, efd->DosHlpSize);
   hlp_memfree(doshlp); doshlp=0;
   // copying discardable messages
   memcpy(parmptr + efd->DisMsgOfs, msgpool[1], msgsize[1]);
   // copying filled arena array
   memcpy(parmptr + efd->DisPartLen - PAGESIZE, arena, PAGESIZE);
   // copying vesa font to doshlp (destroying discardable part)
   if (vfonttgt)
      memcpy(vfonttgt, (char*)hlp_segtoflat(0xA000)+vid->FontAddr, VESAFONTBUFSIZE);
   vid = 0;
   if (pcidata) {
      memcpy(flat1000 + efd->PCIClassList, pcidata, pcidatalen);
      free(pcidata);
      pcidata = 0;
   }
   // copying resident messages
   memcpy(flat1000 + efd->ResMsgOfs, msgpool[0], msgsize[0]);

   // copying qsinit log if present and asked
   if (efd->LogBufPAddr && (lid->BootFlags&BOOTFLAG_LOG)!=0) {
      parmptr = key_present("LOGLEVEL");
      if (parmptr) {
         u32t  level = strtoul(parmptr,0,0);
         char *qslog;
         if (level>LOG_GARBAGE) level=LOG_GARBAGE;
         qslog = log_gettext(level|LOGTF_LEVEL|LOGTF_TIME);
         if (qslog) {
            u32t  len = strlen(qslog);
            char *log = pag_physmap(efd->LogBufPAddr, efd->LogBufSize<<16, 0);

            if (log) {
               log_printf("flushing QS log to kernel, max level %d, size %d\n",level,len);
               // log is too large?
               if (len>=(efd->LogBufSize<<16)-efd->LogBufWrite) {
                  u32t diff = len - (efd->LogBufSize<<16) + 1;
                  len-=diff; qslog+=diff;
               }
               // copying data
               memcpy(log+efd->LogBufWrite, qslog, len);
               pag_physunmap(log);

               efd->LogBufWrite+=len;
            }
            free(qslog);
         }
      }
   }
   log_printf("Flags: %04X\n",lid->BootFlags);

   // push key press
   parmptr = key_present("PKEY");
   if (parmptr) {
      u16t key = strtoul(parmptr,0,0);
      log_printf("pkey = %s %04X\n",parmptr,key);
      if (key) key_push(key);
   }
   // call sysview /mem for direct memory viewing/editing
   if (memview) {
      cmd_state cst = cmd_init("sysview /mem",0);
      cmd_run(cst,CMDR_ECHOOFF);
      cmd_close(cst);
   }
   // loading process is non-destructive until this point
   if (testmode) error_exit(0, "test finished!\n");
   // shutdown QSINIT & micro-FSD
   exit_prepare();
   hlp_fdone();
   // boot OS/2 :)
   log_printf("init2\n");
   hlp_rmcall(MAKEFAR16(boot_info.diskbuf_seg,efd->Init2Ofs),0);
   // we must never reach this point!
}
