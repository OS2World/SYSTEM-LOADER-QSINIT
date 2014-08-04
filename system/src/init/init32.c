//
// QSINIT
// start 32bit code
//
#pragma code_seg ( CODE32, CODE )
#pragma data_seg ( DATA32, DATA )

#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#include "qsinit.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "filetab.h"
#include "vio.h"
#include "qsstor.h"
#include "ioint13.h"

extern u8t           *memtable;
extern u8t         *memheapres;
extern free_block  **memftable;
extern u32t          memblocks;
extern u32t          phmembase;
extern u32t           availmem;
extern u32t       int_mem_need; // internal memory needs (exit list, etc)
extern u16t          pmdataseg, // dpmi service data (GDT, callbacks, etc)
                     logbufseg; // real mode 4k log buffer
extern void       *minifsd_ptr; // temporary buffer for mini-fsd (1000:0) or 0
extern u8t           *page_buf; // 4k buffer for various needs
extern u8t        dd_bootflags; // boot flags
extern u8t           pminitres, // result of PM init
                      safeMode; // safe mode flag
u32t                 DiskBufPM, // flat disk buffer address
                     DiskBufRM, // RM far disk buffer address
                   ZeroAddress; // physical 0
void              *fatroot_buf; // buffer to save pre-readed FAT root
extern u16t      DiskBufRM_Seg; // and segment (exported)
extern struct Disk_BPB BootBPB; // boot disk BPB
extern u16t       puff_bufsize; // memory size need by puff.c
extern void          *puff_buf; // and pointer for it
extern u16t    physmem_entries; // number of entries in physmem
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];

extern u16t         LdrRstCode, // save/restore os2ldr entry data for restart
                      LdrRstCS,
           dd_bpbofs,dd_bpbseg,
         dd_rootofs,dd_rootseg,
                dd_firstsector;

#define CRC32_SIZE     (1024)

void make_disk1(void);
void hlp_basemem(void);
void get_ini_parm(void);
void exit_init(u8t *memory);
void make_crc_table(void *table);

extern struct filetable_s filetable;
extern mod_addfunc   *mod_secondary; // secondary function table, from "start" module
extern cache_extptr    *cache_eproc;

int init32(void) {
   free_block      *ib;
   int           ii,sz;
   u32t    module, err;
   u8t*      crc_table;
   // hello, world!
   if (pminitres) vio_strout("\nA20 error!\n"); 
      else vio_charout('\n');
   // addr of physical 0
   ZeroAddress = hlp_segtoflat(0);
   /* calculate disk i/o buffer address (below micro-FSD & GDT data).
      logbufseg is always page aligned, so our address is aligned to.
      This allow to remove "half-sector DMA code" in disk.asm */
   DiskBufPM = hlp_segtoflat(logbufseg)-DISKBUF_SIZE;
   DiskBufRM = (DiskBufRM_Seg = (u32t)logbufseg - (DISKBUF_SIZE>>PARASHIFT))<<16;
   //log_printf("disk buf: %4.4X %4.4X\n",DiskBufRM,DiskBufPM);
   hlp_basemem();
#if 0
   printf("\nphysmem:\n");
   for (ii=0;ii<physmem_entries;ii++)
      printf("Base:%08X Size:%08X\n",physmem[ii].startaddr,
         physmem[ii].blocklen);
#endif
   // use 16Mb as low own memory border
   sz = _16MB;
   // and PXE cache can grow above it :(
   if (filetable.ft_cfiles==5) {
      u32t resend = filetable.ft_reslen + filetable.ft_resofs;
      // log_misc(2, "PXE data: %08X %08X\n", filetable.ft_resofs, filetable.ft_reslen);
      if (resend > sz) sz = Round64k(resend);
   }
   // searching for whole 8Mb
   for (ii=0;ii<physmem_entries;ii++) {
      register physmem_block *pmb = physmem + ii;
      if (pmb->startaddr>=_16MB && pmb->startaddr+pmb->blocklen>sz) {
         availmem = pmb->startaddr+pmb->blocklen-sz;
         if (availmem>=_16MB/2) break;
      }
   }
   // no - error!
   if (ii>=physmem_entries) return QERR_NOMEMORY;
   // truncate to nearest 64k
   phmembase = sz;
   availmem &= 0xFFFF0000;
   memblocks = availmem>>16;
#if 0
   printf("\nAvailable memory above 16Mb: %d kb\n",availmem>>10);
#endif
   /* memory size for memory management (we are giving memory by 64kb blocks)
      allocating array of pointers to first free block & array of allocated
      blocks */
   ii = memblocks * 4;
   memtable = (u8t*)(phmembase - ((u32t)rm16code<<PARASHIFT));
   memftable = (free_block**)(memtable + ii);
   memheapres = (u8t*)memftable + ii;
   ii=(ii<<2)+(memblocks>>3);
   // init memory tables
   memset(memtable,0,ii);
   // save tables itself as used block
   ((u32t*)memtable)[0]=ii;
   sz=(ii=Round64k(ii))>>16;
   while (--sz) ((u32t*)memtable)[sz]=FFFF;
   // create one free memory entry
   ib=(free_block*)((u32t)memtable+ii);
   ib->sign=FREE_SIGN; ib->prev=0; ib->next=0;
   memftable[ib->size=availmem-ii>>16]=ib; *memheapres=1;
   // memmgr is ready, allocating memory for unzip tables and other
   crc_table   = (u8t*)hlp_memalloc(CRC32_SIZE + PAGESIZE + int_mem_need +
                  puff_bufsize, QSMA_READONLY);
   page_buf    = (void*)(crc_table + CRC32_SIZE);
   puff_buf    = (void*)(page_buf + PAGESIZE);
   exit_init((u8t*)puff_buf + puff_bufsize);
   make_crc_table(crc_table);
   // save some storage keys for start module
   sto_save(STOKEY_BASEMEM,&memtable,4,1);
   if (safeMode) {
      ii = safeMode;
      sto_save(STOKEY_SAFEMODE,&ii,4,1);
   }
   // saving FAT root (it can be destroyd by bootos2.exe)
   if (!dd_bootflags) {
      ii = BootBPB.BPB_RootEntries*32;
      fatroot_buf = hlp_memalloc(ii, QSMA_READONLY);
      memcpy(fatroot_buf, (char*)hlp_segtoflat(dd_rootseg)+dd_rootofs,ii);
#if 0
      log_misc(2,"BPB %04X:%04X, data %04X, root %04X:%04X\n", dd_bpbseg,
         dd_bpbofs, dd_firstsector, dd_rootseg, dd_rootofs);
#endif
   } else // or copying mini-fsd from 1000:0 to upper memory
   if (minifsd_ptr) {
      void *minifsd_new = hlp_memalloc(filetable.ft_mfsdlen, QSMA_READONLY);
      memcpy(minifsd_new, (char*)hlp_segtoflat((u32t)minifsd_ptr>>16),
         filetable.ft_mfsdlen);
      minifsd_ptr = minifsd_new;
   }
   // init file i/o
   hlp_finit();
   // read some data from .ini
   get_ini_parm();
   // read and unpack QSINIT.LDI (zip with common code)
   make_disk1();

   module=mod_load("start", 0, &err, 0);
   if (module) {
      s32t rc=mod_exec(module, 0, 0);
      log_printf("start rc=%d!\n",rc);
      // we do not free start module at all
   } else {
      char msg[64];
      snprintf(msg,64,"start error %d...\n",err);
      vio_strout(msg);
   }

   hlp_fdone();
   return 0;
}

// exit and restart another os2ldr
void exit_restart(char *loader) {
   u32t  ldrsize=0;
   // loading from boot drive, if failed - from virtual disk
   void *ldrdata=hlp_freadfull(loader,&ldrsize,0);
   if (!ldrdata&&mod_secondary)
      ldrdata=(*mod_secondary->freadfull)(loader,&ldrsize);
   if (!ldrdata||!ldrsize) return;
   // copying mini-fsd back to original location
   if (minifsd_ptr) memcpy((void*)hlp_segtoflat(filetable.ft_mfsdseg),
      minifsd_ptr, filetable.ft_mfsdlen);
   else // or restore FAT root dir on FAT boot
   if (fatroot_buf) memcpy((char*)hlp_segtoflat(dd_rootseg)+dd_rootofs,
      fatroot_buf,BootBPB.BPB_RootEntries*32);

   // copy BPB back to initial location (if available)
   if ((dd_bootflags&BF_NOMFSHVOLIO)==0)
      memcpy((char*)hlp_segtoflat(dd_bpbseg)+dd_bpbofs,&BootBPB,sizeof (BootBPB));

   // copy new loader to the right place
   memcpy((void*)hlp_segtoflat(LdrRstCS),ldrdata,ldrsize);
   hlp_memfree(ldrdata);
   // clear screen
   vio_clearscr();
   // and call restart code
   exit_prepare();
   rmcall(LdrRstCode,0);
}

u32t hlp_segtoflat(u16t Segment) {
   return _4GB-((u32t)rm16code-Segment<<PARASHIFT);
}

/// call installed cache callback with supplied action code
void _std cache_ctrl(u32t action, u8t vol) {
   if (cache_eproc) (*cache_eproc->cache_ioctl)(vol, action);
}

/// install cache callback functions
u32t hlp_runcache(cache_extptr *fptr) {
   if (fptr && fptr->entries!=3) return 0;
   cache_eproc = fptr;
   return 1;
}
