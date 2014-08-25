//
// QSINIT
// start 32bit code
//
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
extern void       *minifsd_ptr; // buffer with mini-fsd or 0
extern u8t           *page_buf; // 4k buffer for various needs
extern u8t        dd_bootflags; // boot flags
extern u8t           pminitres, // result of PM init
                      safeMode; // safe mode flag
extern u32t          DiskBufPM, // flat disk buffer address
                     DiskBufRM; // RM far disk buffer address
void              *fatroot_buf; // buffer to save pre-readed FAT root
extern u16t      DiskBufRM_Seg; // and segment (exported)
extern struct Disk_BPB BootBPB; // boot disk BPB
extern u16t       puff_bufsize; // memory size need by puff.c
extern void          *puff_buf; // and pointer for it
extern u16t    physmem_entries; // number of entries in physmem
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];
extern u16t           rm16code;
extern u32t          stacksize; // 32-bit part stack size

extern u16t         LdrRstCode, // save/restore os2ldr entry data for restart
                      LdrRstCS,
           dd_bpbofs,dd_bpbseg,
         dd_rootofs,dd_rootseg,
                dd_firstsector;

u32t                  qs16base; // base addr of 16-bit part

#define CRC32_SIZE     (1024)

void make_disk1(void);
void hlp_basemem(void);
void get_ini_parm(void);
void exit_init(u8t *memory);
void make_crc_table(void *table);
void getfullSMAP(void);

extern struct filetable_s filetable;
extern mod_addfunc   *mod_secondary; // secondary function table, from "start" module
extern cache_extptr    *cache_eproc;

int _std init32(u32t rmstack) {
   free_block      *ib;
   int           ii,sz;
   u32t    module, err;
   u8t*      crc_table;

   qs16base = (u32t)rm16code<<PARASHIFT;
   // hello, world!
   if (pminitres) vio_strout("\nA20 error!\n"); 
      else vio_charout('\n');
   //log_printf("disk buf: %4.4X %4.4X\n",DiskBufRM,DiskBufPM);

   // save storage key with array`s pointer and size
   sto_save(STOKEY_PHYSMEM, &physmem, physmem_entries * sizeof(physmem_block), 0);
#if 0
   printf("\nphysmem:\n");
   for (ii=0;ii<physmem_entries;ii++)
      printf("Base:%08X Size:%08X\n",physmem[ii].startaddr,
         physmem[ii].blocklen);
#endif

#if 0
   printf("\nAvailable memory above 16Mb: %d kb\n",availmem>>10);
#endif
   /* memory management (we are giving memory by 64kb blocks):
      allocating array of pts to first free block & array of allocated blocks */
   ii = memblocks * 4;
   memtable   = (u8t*)phmembase;
   memftable  = (free_block**)(memtable + ii);
   memheapres = (u8t*)memftable + ii;
   ii = (ii<<2)+(memblocks>>3);
   // init memory tables
   memset(memtable,0,ii);
   // save tables itself as used block
   ((u32t*)memtable)[0] = ii;
   sz = (ii=Round64k(ii))>>16;
   while (--sz) ((u32t*)memtable)[sz] = FFFF;
   // create one free memory entry
   ib = (free_block*)((u32t)memtable+ii);
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
   } 
   // init file i/o
   hlp_finit();
   // read some data from .ini
   get_ini_parm();
   // read and unpack QSINIT.LDI (zip with common code)
   make_disk1();
   log_printf("stack for rm calls: %d bytes\n", rmstack);
#ifndef INITDEBUG
   // verbose build print this from RM asm code
   log_printf("pmdata: %04X, log: %04X, iobuf: %04X, stack: %d\n", pmdataseg,
      logbufseg, DiskBufRM_Seg, stacksize);
#endif

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
   u32t   ldrsize=0;
   void  *ldrdata, *rc;

   if (!mod_secondary) return;
   // loading from boot drive, if failed - from virtual disk
   ldrdata=hlp_freadfull(loader,&ldrsize,0);
   if (!ldrdata) ldrdata=(*mod_secondary->freadfull)(loader,&ldrsize);
   if (!ldrdata||!ldrsize) return;

   // copying mini-fsd back to original location
   if (minifsd_ptr)
      rc = (*mod_secondary->memcpysafe)((void*)hlp_segtoflat(filetable.ft_mfsdseg),
         minifsd_ptr, filetable.ft_mfsdlen, 1);
   else // or restore FAT root dir on FAT boot
   if (fatroot_buf) 
      rc = (*mod_secondary->memcpysafe)((char*)hlp_segtoflat(dd_rootseg)+dd_rootofs,
         fatroot_buf,BootBPB.BPB_RootEntries*32, 1);
   else // flag success on no copy action
      rc = ldrdata;
   // copy BPB back to initial location (if available)
   if (rc && (dd_bootflags&BF_NOMFSHVOLIO)==0)
      rc = (*mod_secondary->memcpysafe)((char*)hlp_segtoflat(dd_bpbseg)+dd_bpbofs,
         &BootBPB, sizeof (BootBPB), 1);
   // copy new loader to the right place
   if (rc) memcpy((void*)hlp_segtoflat(LdrRstCS),ldrdata,ldrsize);
   hlp_memfree(ldrdata);
   // one of copy ops failed
   if (!rc) return;
   // clear screen
   vio_clearscr();
   // and call restart code
   exit_prepare();

   rmcall(LdrRstCode,0);
}

// obsolette function
u32t hlp_segtoflat(u16t Segment) { return (u32t)Segment<<PARASHIFT; }

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

AcpiMemInfo* _std int15mem(void) {
   rmcall(getfullSMAP,0);
   return (AcpiMemInfo*)DiskBufPM;
}
