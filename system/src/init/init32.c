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
#include "qsstor.h"
#include "qsconst.h"
#include "qsbinfmt.h"
#include "vio.h"

#ifndef EFI_BUILD
#include "ioint13.h"
#include "parttab.h"
#include "filetab.h"
u32t                  qs16base; // base addr of 16-bit part
u8t               bootio_avail; // boot partition disk i/o is available
extern 
MKBIN_HEADER        bin_header;
extern u16t      DiskBufRM_Seg; // and segment (exported)
extern struct Disk_BPB BootBPB; // boot disk BPB
extern u8t        dd_bootflags; // boot flags
extern void       *minifsd_ptr; // buffer with mini-fsd or 0
extern u16t          pmdataseg, // dpmi service data (GDT, callbacks, etc)
                     logbufseg; // real mode 4k log buffer
extern u16t           rm16code;
extern u32t          stacksize; // 32-bit part stack size
extern u16t         LdrRstCode, // save/restore os2ldr entry data for restart
                      LdrRstCS,
           dd_bpbofs,dd_bpbseg,
         dd_rootofs,dd_rootseg,
                dd_firstsector;
extern 
struct filetable_s   filetable;
#endif // EFI_BUILD
extern
MKBIN_HEADER      *pbin_header;  // use pointer to be compat. with EFI build
extern u8t           *memtable;
extern u32t       int_mem_need; // internal memory needs (exit list, etc)
extern u8t           *page_buf; // 4k buffer for various needs
extern u8t           pminitres, // result of PM init
                      safeMode; // safe mode flag
extern u16t       puff_bufsize; // memory size need by puff.c
extern void          *puff_buf; // and pointer for it
extern u16t    physmem_entries; // number of entries in physmem
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];
extern 
mod_addfunc     *mod_secondary; // secondary function table, from "start" module
extern 
cache_extptr      *cache_eproc; // cache module callbacks
extern u8t*              ExCvt; // FatFs OEM case conversion
extern u8t*          mount_buf; // buffer for hlp_mountvol()

#define CRC32_SIZE     (1024)
#define OEMTAB_SIZE     (128)

int  start_it(void);
void make_disk1(void);
void get_ini_parm(void);
void exit_init(u8t *memory);
void make_crc_table(void *table);
void memmgr_init(void);
void _std make_reboot(int warm);

void _std init_common(void) {
   u8t *crc_table;
   // save storage key with array`s pointer and size
   sto_save(STOKEY_PHYSMEM, &physmem, physmem_entries * sizeof(physmem_block), 0);
   // init common memory manager
   memmgr_init();
   // memmgr is ready, allocating memory for unzip tables and other misc stuff
   crc_table   = (u8t*)hlp_memallocsig(CRC32_SIZE + PAGESIZE + int_mem_need +
                  puff_bufsize + OEMTAB_SIZE + MAX_SECTOR_SIZE, "serv", QSMA_READONLY);
   page_buf    = (void*)(crc_table + CRC32_SIZE);
   mount_buf   = page_buf + PAGESIZE;
   puff_buf    = (void*)(mount_buf + MAX_SECTOR_SIZE);
   ExCvt       = (u8t*)puff_buf + puff_bufsize;
   exit_init(ExCvt + OEMTAB_SIZE);
   make_crc_table(crc_table);
   memset(ExCvt, '.', OEMTAB_SIZE);
   // save some storage keys for start module
   sto_save(STOKEY_BASEMEM,&memtable,4,1);
   if (safeMode) {
      u32t value = safeMode;
      sto_save(STOKEY_SAFEMODE,&value,4,1);
   }
}

/// 32-bit main() function
#ifdef EFI_BUILD
int _std init32(void) {
   // pbin_header value filled by 64-bit code
#else
int _std init32(u32t rmstack) {
   pbin_header = &bin_header;
   qs16base    = (u32t)rm16code<<PARASHIFT;
   // hello, world!
   if (pminitres) vio_strout("\nA20 error!\n"); else vio_charout('\n');
#endif // EFI_BUILD
   // init internal structs & save some keys for START module
   init_common();
#ifndef EFI_BUILD
   // is disk 0: available?
   bootio_avail = dd_bootflags&BF_NOMFSHVOLIO ? 0 : 1;
#endif // EFI_BUILD
   // init file i/o
   hlp_finit();
   // read some data from .ini
   get_ini_parm();
   // read and unpack QSINIT.LDI (zip with common code)
   make_disk1();
#ifndef EFI_BUILD
   // just log it
   log_printf("stack for rm calls: %d bytes\n", rmstack);
#ifndef INITDEBUG
   // verbose build print this from RM asm code
   log_printf("pmdata: %04X, log: %04X, iobuf: %04X, stack: %d\n", pmdataseg,
      logbufseg, DiskBufRM_Seg, stacksize);
#endif
#endif
   {  // load and launch START module
      int rc = start_it();
      if (rc)
         if (rc!=MODERR_NOORDINAL) {
            char msg[64];
            snprintf(msg,64,"start error %d...\n", rc);
            vio_strout(msg);
         } else // no required ordunal - so just report about wrong file
            vio_strout("QSINIT.LDI version mismatch!\n");
   }
   hlp_fdone();
   return 0;
}

// exit and restart another os2ldr
void exit_restart(char *loader) {
#ifndef EFI_BUILD
   u32t   ldrsize=0, rc;
   void  *ldrdata;

   if (!mod_secondary) return;
   // loading from boot drive, if failed - from virtual disk
   ldrdata=hlp_freadfull(loader,&ldrsize,0);
   if (!ldrdata) ldrdata=(*mod_secondary->freadfull)(loader,&ldrsize);
   if (!ldrdata||!ldrsize) return;

   // copying mini-fsd back to original location
   if (minifsd_ptr)
      rc = (*mod_secondary->memcpysafe)((void*)hlp_segtoflat(filetable.ft_mfsdseg),
         minifsd_ptr, filetable.ft_mfsdlen, 1);
   else {
      u32t bootfs = hlp_volinfo(DISK_BOOT, 0),
          rootlen = BootBPB.BPB_RootEntries * 32;

      if ((bootfs==FST_FAT12 || bootfs==FST_FAT16) && rootlen && !dd_bootflags) {
         // re-read FAT root dir on FAT boot
         struct Boot_Record *btr = (struct Boot_Record*)hlp_memalloc(4096+rootlen, 0);
         rc = 0;
         if (hlp_diskread(QDSK_VOLUME|DISK_BOOT, 0, 1, btr)) {
            struct Common_BPB *bpb = &btr->BR_BPB;
            u32t              nsec;
            if (BootBPB.BPB_RootEntries!=bpb->BPB_RootEntries)
               log_printf("warning! BPB mismatch!\n");

            nsec = rootlen/bpb->BPB_BytePerSect;
            if (rootlen%bpb->BPB_BytePerSect) nsec++;
            /* read boot volume FAT12/16 root dir sectors and copy it to
               original place */
            if (hlp_diskread(QDSK_VOLUME|DISK_BOOT, bpb->BPB_ResSectors + 
               bpb->BPB_FATCopies*bpb->BPB_SecPerFAT, nsec, btr+1)==nsec)
                  rc = (*mod_secondary->memcpysafe)((char*)hlp_segtoflat(dd_rootseg)+
                     dd_rootofs, btr+1, rootlen, 1);
         }
         hlp_memfree(btr);
      } else // flag success on no copy action
         rc = 1;
   }
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
#else
   log_printf("exit_restart() is unsupported!\n");
#endif // EFI_BUILD
}

void _std exit_reboot(int warm) {
   // shut down QS services
   exit_prepare();
   // and reboot
   make_reboot(warm);
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
