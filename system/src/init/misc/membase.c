/*
   common memory allocating functions:
    - any allocation rounded to 64k internally
    - allocated size is zero filled
    - incorrect free/damaged block structure will cause immediate panic
*/
#include "qstypes.h"
#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#define MODULE_INTERNAL
#include "qsmod.h"      // for mod_secondary only

#define CALLER(x) (((u32t*)(&x))[-1])

// memtable & memftable - arrays with 64k step
u32t         *memtable; // size of block if used block starts here
                        // zero if this 64k is free
                        // -1   if it is in the middle of the used block
u8t        *memheapres; // bit array - r/o blocks, available to heap mgr
free_block **memftable; // pointers to first block in list of free blocks with same size
extern u32t  memblocks; // number of 64k blocks
extern u32t   availmem; // total avail memory for as (above 16M, including those arrays)
extern u32t  phmembase; // physical address of used memory (16Mb or larger on PXE)

extern mod_addfunc 
        *mod_secondary; // secondary function table, from "start" module

/*  here: 'a' - alloc, 'r' - realloc, 'f' - free, 'i' - isfree, 'm' - avail, 
          'b' - blocksize */
static void mterr(void *blk,u32t ii,char here,u32t caller) {
   log_printf("used/free: %08X mt:%08X %c %08X\n",blk,memtable[ii],here,caller);
   exit_pm32(QERR_MCBERROR);
}

static void szerr(free_block *fb,u32t mustbe,char here,u32t caller) {
   log_printf("free size: %08X %16b sz:%08X %c %08X\n",fb,fb,mustbe,here,caller);
   exit_pm32(QERR_MCBERROR);
}

// trying to free or realloc read-only block
static int hrchk(u32t pos,char here,u32t caller) {
   if (memheapres[pos>>3]&1<<(pos&7)) {
      log_printf("r/o: %08X %c %08X\n",(u32t)memtable+(pos<<16),here,caller);
      if (here!='f') exit_pm32(QERR_MCBERROR);
      return 1;
   }
   return 0;
}

static void addrcheck(void *addr,u32t caller) {
   if (addr<memtable||(u32t)addr>=(u32t)memtable+availmem||
      ((u32t)addr-(u32t)memtable&0xFFFF)!=0)
   {
      log_printf("bad addr: %08X (%08X)\n",addr,caller);
      exit_pm32(QERR_MCBERROR);
   }
}

static void fbcheck(free_block *fb,char here,u32t caller) {
   if (fb->sign!=FREE_SIGN) {
      log_printf("free sign: %08X %16b %c %08X\n",fb,fb,here,caller);
      exit_pm32(QERR_MCBERROR);
   }
}

// add block to list of free blocks with the same size
static void fb_add(free_block *fb,u32t size64k,char here,u32t caller) {
   auto free_block **mft=memftable;
   fb->prev=0; fb->next=mft[size64k];
   if (mft[size64k]) {
      fbcheck(mft[size64k],here,caller);
      mft[size64k]->prev=fb;
   }
   fb->sign=FREE_SIGN; fb->size=size64k;
   mft[size64k]=fb;
}

// unlink block from list of free blocks
static void fb_del(free_block *fb,u32t size64k,char here,u32t caller) {
   free_block **mft=memftable,
               *pfb=fb->prev,
               *nfb=fb->next;
   // if no size specified - no check
   if (!size64k) size64k=fb->size; else
   if (fb->size!=size64k) szerr(fb,size64k,here,caller);
   // check, clean and unlink header
   fbcheck(fb,here,caller); fb->sign=0;
   if (pfb) pfb->next=nfb; else mft[size64k]=nfb;
   if (nfb) nfb->prev=pfb;
}

static void fb_chkhdr(free_block *fb,char here,u32t caller,int first) {
   if (first&&fb->prev||(u32t)fb->next>=(u32t)memtable+availmem) {
      log_printf("bad hdr: %08X<-%08X->%08X %c %08X\n",fb->prev,
         fb,fb->next,here,caller);
      exit_pm32(QERR_MCBERROR);
   }
}

// allocate memory
static void *alloc_worker(u32t size,u32t caller,int ro) {
   auto u32t*         mt= memtable; // to avoid many 32bit data segment offsets
   auto free_block **mft=memftable;
   void          *result=0;
   u32t         sz,ii,ps;

   ii=sz=Round64k(size)>>16;
   while (ii<memblocks) {
      if (mft[ii]) {
         free_block *fb=(free_block*)(result=mft[ii]);
         u8t      *hptr, mask;
         // check for damaged free block header
         fbcheck(fb,'a',caller);
         fb_chkhdr(fb,'a',caller,1);
         // unlink block from block list
         fb_del(fb,ii,'a',caller);
         ps=(u32t)fb-(u32t)memtable>>16;
         if (mt[ps]) mterr(result,ps,'a',caller);
         // save user size
         mt[ps]=size;
         // update const ptr flag
         hptr=memheapres+(ps>>3);
         if (ro) *hptr|=1<<(ps&7); else *hptr&=~(1<<(ps&7));
         // create free block with smaller size
         if (ii!=sz) fb_add((free_block*)((u32t)result+Round64k(size)),ii-sz,'a',caller);
         // mark blocks in size array
         while (--sz)
            if (mt[++ps]) mterr(result,ps,'a',caller); else mt[ps]=FFFF;
         // zero-fill requested part only
         return result;
      }
      ii++;
   }
   return 0;
}

void* hlp_memalloc(u32t size, u32t flags) {
   void* rc=0;
   if (!size) return 0;
   if (size<availmem) 
      rc=alloc_worker(size,CALLER(size),flags&QSMA_READONLY?1:0);
   if (!rc && (flags&QSMA_RETERR)==0) exit_pm32(QERR_NOMEMORY);
   // zero-fill requested part only
   if (rc && (flags&QSMA_NOCLEAR)==0) memset(rc,0,size);
   return rc;
}

/// free memory. addr must point to the begin of allocated block
void  hlp_memfree(void *addr) {
   auto u32t*         mt= memtable; // to avoid many 32bit data segment offsets
   auto free_block **mft=memftable;
   u32t    sz,ii,stp,pos,caller=CALLER(addr);
   free_block       *sfb;
   // invalid address?
   addrcheck(addr,caller);
   // position
   pos=(u32t)addr-(u32t)memtable>>16;
   if (!mt[pos]||mt[pos]==FFFF) mterr(addr,pos,'f',caller);
   // ignore free of r/o block
   if (hrchk(pos,'f',caller)) return;
   // size in blocks
   sz=Round64k(mt[pos])>>16;
   // begin of feature free block
   stp=pos;
   while (stp--)
      if (mt[stp]) break;
   sfb=(free_block*)((u32t)mt+(++stp<<16));
   if (stp!=pos) fb_del(sfb,pos-stp,'f',caller);
   // check and clear size array
   if (sz>1)
      if (memchrnd(mt+pos+1,FFFF,sz-1)) mterr(addr,pos,'f',caller);
   memsetd(mt+pos,0,sz);
   if (pos+sz<memblocks)
      if (!mt[pos+sz]) {
         free_block *fb=(free_block*)((u32t)mt+(pos+sz<<16));
         sz+=fb->size;
         fb_del(fb,0,'f',caller);
      }
   // add block to list
   fb_add(sfb,pos-stp+sz,'f',caller);
}

// query block size by pointer
u32t hlp_memgetsize(void *addr) {
   auto u32t*    mt = memtable; // avoid 32bit offsets
   u32t  pos,caller = CALLER(addr);
   // invalid address?
   addrcheck(addr,caller);
   // position
   pos=(u32t)addr-(u32t)memtable>>16;
   hrchk(pos,'b',caller);
   return mt[pos];
}

// realloc region of memory. return new/old address.
void* hlp_memrealloc(void *addr, u32t newsize) {
   auto u32t*         mt= memtable; // to avoid many 32bit data segment offsets
   auto free_block **mft=memftable;
   u32t    sz,nsz,pos,ii,caller=CALLER(addr);
   // invalid address?
   addrcheck(addr,caller);
   if (!newsize) newsize++;
   // position
   pos=(u32t)addr-(u32t)memtable>>16;
   hrchk(pos,'r',caller);
   // size in blocks
   sz =Round64k(mt[pos])>>16;
   nsz=Round64k(newsize)>>16;
   if (sz==nsz) { // realloc does not require table changes
      if (newsize>mt[pos]) memset((u8t*)addr+mt[pos],0,newsize-mt[pos]);
      mt[pos]=newsize;
      return addr;
   }
   if (nsz<sz) { // new size is smaller
      mt[pos]=newsize;
      memsetd(mt+pos+nsz,0,sz-nsz);
      if (pos+sz<memblocks)
         if (!mt[pos+sz]) {
            free_block *fb=(free_block*)((u32t)mt+(pos+sz<<16));
            sz+=fb->size;
            fb_del(fb,0,'r',caller);
         }
      // add block to list
      fb_add((free_block*)((u32t)mt+(pos+nsz<<16)),sz-nsz,'r',caller);
      return addr;
   }
   if (pos+sz<memblocks)
      if (!mt[pos+sz]) {
         free_block *fb=(free_block*)((u32t)mt+(pos+sz<<16));
         if (sz+fb->size>=nsz) { // use free block behind processed
            fb_del(fb,0,'r',caller);
            if (sz+fb->size>nsz) // save remainder
               fb_add((free_block*)((u32t)mt+(pos+nsz<<16)),sz+fb->size-nsz,'r',caller);
            // mark blocks as used
            memsetd(mt+pos+sz,FFFF,nsz-sz);
            // zero fill new bytes
            memset((u8t*)addr+mt[pos],0,newsize-mt[pos]);
            mt[pos]=newsize;
            return addr;
         }
      }
   // common allocating via alloc/free
   {
      void *rc=hlp_memalloc(newsize,QSMA_RETERR);
      if (!rc) return 0;
      memcpy(rc,addr,mt[pos]);
      hlp_memfree(addr);
      return rc;
   }

   return 0;
}

/** query list of constant blocks (internal use only).
    reporting only blocks with, at least, 4k of free size, any other still
    marked as r/o (to prevent free), but not visible to memmgr */
u32t hlp_memqconst(u32t *array, u32t pairsize) {
   auto u32t* mt=memtable;
   u32t ii, cnt=0;
   for (ii=0;ii<memblocks&&cnt<pairsize;ii++)
      if ((memheapres[ii>>3]&1<<(ii&7)) && mt[ii] && mt[ii]!=FFFF) 
         if (mt[ii]<_64KB-_4KB) {
            *array++=(u32t)memtable+(ii<<16);
            *array++=mt[ii];
            cnt++;
         }
   return cnt;
}

void* _std hlp_memreserve(u32t physaddr, u32t length) {
   if (physaddr<phmembase || physaddr>=phmembase+availmem) return 0; 
   else {
      auto u32t*         mt= memtable; // avoid 32bit offsets
      auto free_block **mft=memftable;
      u32t    sz,pos,caller=CALLER(physaddr);
      // truncate length (must never occur)
      if (physaddr+length>phmembase+availmem) length=phmembase+availmem-physaddr;
      // convert physical to qsinit flat
      physaddr+=hlp_segtoflat(0);
      // and round down to allocator step
      sz = (u32t)physaddr-(u32t)memtable&0xFFFF;
      length  +=sz;
      physaddr-=sz;
      // invalid address?
      addrcheck((void*)physaddr,caller);

      pos=physaddr-(u32t)memtable>>16;
      sz =Round64k(length)>>16;
      // check entire length is free
      if (memchrnd(mt+pos,0,sz)) return 0; else {
         // mark this block as allocated
         free_block  *sfb;
         u32t  ii=pos, fbsz;
         while (ii--) // search for start of free block
            if (mt[ii]) break;
         sfb =(free_block*)((u32t)mt+(++ii<<16));
         fbsz=sfb->size;
         fb_del(sfb,0,'i',caller);
         // free block before reserved space
         if (pos-ii) fb_add(sfb,pos-ii,'i',caller);
         // free block after reserved space
         if (pos+sz<ii+fbsz) fb_add((free_block*)((u32t)mt+(pos+sz<<16)),
            ii+fbsz-(pos+sz),'i',caller);
         // used memory
         mt[pos]=length;
         while (--sz)
            if (mt[++pos]) mterr((void*)physaddr,pos,'i',caller); else mt[pos]=FFFF;
      }
#ifdef INITDEBUG
      log_printf("resrvd: %08X %d\n",physaddr,length);
#endif
      return (void*)physaddr;
   }
}

// print memory contol table contents (debug)
void hlp_memprint(void) {
   if (mod_secondary) mod_secondary->memprint(memtable,memftable,memblocks);
#if 0 // moved to start
   auto u32t*         mt= memtable;
   auto free_block **mft=memftable;
   u32t ii,cnt;
   log_printf("<=====Memory dump=====>\n");
   log_printf("Total : %d kb\n",memblocks<<6);
   for (ii=0,cnt=0;ii<memblocks;ii++,cnt++) {
      free_block *fb=(free_block*)((u32t)mt+(ii<<16));
      if (mt[ii]&&mt[ii]<FFFF) {
         log_printf("%5d. %08X - %d kb (%d)\n",cnt,fb,Round64k(mt[ii])>>10,mt[ii]);
      } else
      if (!mt[ii]&&ii&&mt[ii-1]) {
         if (fb->sign!=FREE_SIGN) log_printf("free header destroyd!!!\n");
            else log_printf("%5d. %08X - free %d kb\n",cnt,fb,fb->size<<6);
      }
   }
   log_printf("%5d. %08X - end\n",cnt,(u32t)mt+(memblocks<<16));

   log_printf("Free table:\n");
   for (ii=0;ii<memblocks;ii++)
      if (mft[ii]) {
         free_block *fb=mft[ii];
         log_printf("%d kb (%d):",ii<<6,ii);
         while (fb) {
            log_printf(" %08X",fb);
            if (fb->size!=ii) log_printf("(%d!)",fb->size);
            if (fb->sign!=FREE_SIGN) log_printf("(S:%08X!)",fb->sign);
            fb=fb->next;
         }
         log_printf("\n");
      }
#endif
}

u32t _std hlp_memavail(u32t *maxblock, u32t *total) {
   auto free_block **mft=memftable;
   u32t rc=0,ii=memblocks,caller=CALLER(maxblock),mb=0;

   while (ii--)
      if (mft[ii]) {
         free_block *fb=mft[ii];
         u32t   size = ii<<16;
         if (!mb) mb = size;
         while (fb) {
            // check for damaged free block header
            fbcheck(fb,'a',caller);
            fb_chkhdr(fb,'m',caller,fb==mft[ii]);
            rc+=size;
            fb =fb->next;
         }
      }
   if (maxblock) *maxblock = mb;
   if (total) *total = availmem;
   return rc;
}
