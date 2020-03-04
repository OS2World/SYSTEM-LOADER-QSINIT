//
// QSINIT
// start 32bit code
//
#include "stdlib.h"             // only for hlp_memcpy() flags
#include "qslocal.h"
#include "qsconst.h"
#include "qsbinfmt.h"
#include "vio.h"

#ifndef EFI_BUILD
#include "ioint13.h"
#include "parttab.h"
#include "filetab.h"
u32t                  qs16base; // base addr of 16-bit part
extern
MKBIN_HEADER        bin_header;
extern u16t      DiskBufRM_Seg; // and segment (exported)
extern struct Disk_BPB BootBPB; // boot disk BPB
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
#else
extern u32t        qd_bootdisk;
#endif // EFI_BUILD
u8t               bootio_avail; // boot partition disk i/o is available
extern
MKBIN_HEADER      *pbin_header;  // use pointer to be compatible with EFI build
extern u8t           *memtable;
extern u8t           pminitres; // result of PM init
extern u16t       puff_bufsize; // memory size need by puff.c
extern void          *puff_buf; // and pointer for it
extern u16t    physmem_entries; // number of entries in physmem
extern physmem_block   physmem[PHYSMEM_TABLE_SIZE];
extern
cache_extptr      *cache_eproc; // cache module callbacks
u8t*                     ExCvt; // OEM case table (was used for FatFs, now for stricmp only)

#define CRC32_SIZE     (1024)
#define OEMTAB_SIZE     (128)

int  start_it(void);
void unpack_start_module(void);
void check_disks(void);
void get_ini_parm(void);
void make_crc_table(void *table);
void memmgr_init(void);
void hlp_finit(void);
void _std make_reboot(int warm);

void _std init_common(void) {
   u8t *crc_table;
   // save storage key with array`s pointer and size
   sto_save(STOKEY_PHYSMEM, &physmem, physmem_entries * sizeof(physmem_block), 0);
   // memmgr is ready, allocating memory for unzip tables and other misc stuff
   crc_table   = (u8t*)hlp_memallocsig(CRC32_SIZE + puff_bufsize + OEMTAB_SIZE,
                                       "serv", QSMA_READONLY);
   puff_buf    = crc_table + CRC32_SIZE;
   ExCvt       = (u8t*)puff_buf + puff_bufsize;
   make_crc_table(crc_table);
#ifndef EFI_BUILD
   // saves mini-FSD crc32
   if (minifsd_ptr) {
      u32t crc = crc32(0, minifsd_ptr, filetable.ft_mfsdlen);
      sto_save(STOKEY_MFSDCRC, &crc, 4, 1);
   }
   /* read block, passed from the loader, who RESTARTs us.
      it should be placed in common memory, on 64k border (hlp_memalloc()) */
   if (transition_ptr && (transition_ptr&0xFFFF)==0) {
      // used just to range check here
      void *rb = hlp_memreserve(transition_ptr, sizeof(transition_data), 0);
      if (rb) {
         transition_data *td = (transition_data*)rb;
         while (td->sign==TRND_SIGN) {
            u32t  btype = td->type&~TRND_LAST;

            if (btype==0 || btype>TRND_CODEPAGE) break; else {
               u32t crc = crc32(0, (u8t*)(td+1), td->length);

               if (crc==td->crc32) {
                  void *blk = 0;
               
                  switch (btype) {
                     case TRND_ENV    :
                        blk = hlp_memallocsig(td->length, "ienv", QSMA_READONLY|
                                              QSMA_RETERR|QSMA_NOCLEAR);
                        init_env = blk;
                        break;
                     case TRND_CMDHIST:
                        blk = hlp_memalloc(td->length, QSMA_RETERR|QSMA_NOCLEAR);
                        if (blk) sto_save(STOKEY_CMDHINIT, blk, td->length, 0);
                        break;
                     case TRND_CODEPAGE:
                        crc = *(u16t*)(td+1);
                        sto_save(STOKEY_RSTCPNUM, &crc, 4, 1);
                        break;
                  }
                  if (blk) memcpy(blk, td+1, td->length);
               } else
                  break;
            }
            if (td->type&TRND_LAST) break;
            td = next_trblock(td);
         }
         hlp_memfree(rb);
      }
   }
#endif
   memset(ExCvt, '.', OEMTAB_SIZE);
   // save some storage keys for START module
   sto_save(STOKEY_BASEMEM, &memtable, 4, 1);
   if (safeMode) {
      u32t value = safeMode;
      sto_save(STOKEY_SAFEMODE, &value, 4, 1);
   }
}

/// 32-bit main() function
#ifdef EFI_BUILD
int _std init32(void) {
   bootio_avail = qd_bootdisk!=FFFF;
   // pbin_header value filled by 64-bit code
#else
int _std init32(u32t rmstack) {
   // is disk 0: available?
   bootio_avail = dd_bootflags&BF_NOMFSHVOLIO ? 0 : 1;
   pbin_header  = &bin_header;
   qs16base     = (u32t)rm16code<<PARASHIFT;
   // hello, world!
   if (pminitres) vio_strout("\nA20 error!\n"); else vio_charout('\n');
#endif // EFI_BUILD
   // init common memory manager
   memmgr_init();
   // init internal structs & save some keys for START module
   init_common();
   init_host();
   // init file i/o
   hlp_finit();
   // get some critical keys from .ini
   get_ini_parm();
   // read and unpack QSINIT.LDI (zip with common code)
   unpack_start_module();
   // minor check to see error in log
   check_disks();
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
         if (rc!=E_MOD_NOORD) {
            char msg[64];
            snprintf(msg,64,"start error %X...\n", rc);
            vio_strout(msg);
         } else // no required ordinal - just say about wrong file
            vio_strout("QSINIT.LDI version mismatch!\n");
   }
   return 0;
}

#ifndef EFI_BUILD
static void add_trblock(transition_data *td, void *data, u32t len, u32t type) {
   td->sign   = TRND_SIGN;
   td->length = len;
   td->type   = type;
   memcpy(td+1, data, len);
   td->crc32  = crc32(0, data, len);
   // log_printf("rst %u %X %u!\n", type, td->crc32, len);
}
#endif // EFI_BUILD

// exit and restart another os2ldr
qserr _std exit_restart(char *loader, void *env, int nosend) {
#ifndef EFI_BUILD
   u32t   ldrsize = 0, rc,
           envlen = env ? env_length(env) : 0, hlen;
   void  *ldrdata;
   char    *hdata;
   void   *trdata;

   if (!mod_secondary) return E_SYS_UNSUPPORTED;
   // loading from boot drive, if failed - from virtual disk
   ldrdata = hlp_freadfull(loader, &ldrsize, 0);
   if (!ldrdata) ldrdata = mod_secondary->freadfull(loader, &ldrsize);
   if (!ldrdata || !ldrsize) return E_SYS_NOFILE;
   if (ldrsize>_512KB) return E_SYS_NOMEM;

   // copy mini-fsd back to original location
   if (minifsd_ptr)
      rc = mod_secondary->memcpysafe((void*)hlp_segtoflat(filetable.ft_mfsdseg),
         minifsd_ptr, filetable.ft_mfsdlen, MEMCPY_PG0);
   else {
      u32t bootfs = mod_secondary->hlp_volinfo(DISK_BOOT, 0),
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
                  rc = mod_secondary->memcpysafe((char*)hlp_segtoflat(dd_rootseg)+
                     dd_rootofs, btr+1, rootlen, MEMCPY_PG0);
         }
         hlp_memfree(btr);
      } else // flag success on no copy action
         rc = 1;
   }
   // copy BPB back to initial location (if available)
   if (rc && (dd_bootflags&BF_NOMFSHVOLIO)==0)
      rc = mod_secondary->memcpysafe((char*)hlp_segtoflat(dd_bpbseg)+dd_bpbofs, &BootBPB,
         dd_bootflags&BF_NEWBPB?sizeof(struct Disk_NewPB):sizeof(BootBPB), MEMCPY_PG0);
   // copy new loader to the right place
   if (rc) memcpy((void*)hlp_segtoflat(LdrRstCS), ldrdata, ldrsize);
   hlp_memfree(ldrdata);
   // one of copy ops failed
   if (!rc) return E_SYS_SOFTFAULT;
   // build data for the next loader
   hdata  = nosend ? 0 : mod_secondary->shl_history();
   hlen   = hdata ? strlen(hdata)+1 : 0;
   rc     = 0;
   trdata = 0;
   // environment copy
   if (envlen) rc += Round16(envlen) + sizeof(transition_data);
   // cmd.exe history
   if (hlen)   rc += Round16(hlen) + sizeof(transition_data);
   // current codepage
   if (cp_num && !nosend) rc += 16 + sizeof(transition_data);
   if (rc) {
      trdata = hlp_memalloc(rc, QSMA_RETERR|QSMA_NOCLEAR|QSMA_MAXADDR);
      if (trdata) {
         transition_data *td = (transition_data *)trdata;
         td->sign = 0;
         td->type = 0;
         // save env strings block
         if (envlen) add_trblock(td, env, envlen, TRND_ENV);
         // save shell history block
         if (hlen) {
            if (td->type) td = next_trblock(td);
            add_trblock(td, hdata, hlen, TRND_CMDHIST);
         }
         // save current codepage
         if (cp_num && !nosend) {
            if (td->type) td = next_trblock(td);
            add_trblock(td, &cp_num, 2, TRND_CODEPAGE);
         }
         if (td->type) td->type |= TRND_LAST;
      }
   }
   // clear screen
   vio_clearscr();
   // disable micro-FSD "terminate" call
   mfsd_noterm = 1;
   // and call restart code
   exit_prepare();
   // this size will be copied to original filetable
   filetable.ft_loaderlen = ldrsize;
   // and launch it
   rmcall(LdrRstCode, RMC_EXITCALL|2, trdata);
   return E_SYS_SOFTFAULT;
#else
   return E_SYS_EFIHOST;
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

/// install cache callback functions
u32t hlp_runcache(cache_extptr *fptr) {
   if (fptr && fptr->entries!=3) return 0;
   cache_eproc = fptr;
   return 1;
}

/// call installed cache callback with supplied action code
u32t _std hlp_cachenotify(u8t vol, u32t action) {
   if (cache_eproc) (*cache_eproc->cache_ioctl)(vol, action);
   return cache_eproc?1:0;
}
