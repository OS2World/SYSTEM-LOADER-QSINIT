#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#include "qecall.h"

physmem_block   physmem[PHYSMEM_TABLE_SIZE];
u16t    physmem_entries;
extern u8t   *DiskBufPM;  // disk i/o buffer
extern u32t    highbase,
                highlen,
             int12msize;
u16t          headblock;

AcpiMemInfo *int15mem_copy(void);

u16t _std int12mem(void) {
   return int12msize ? int12msize>>10 : 640;
}

// fill memory info in disk i/o buffer
void int15mem_int(void) {
   void *buffer = hlp_memalloc(_64KB, QSMA_NOCLEAR|QSMA_LOCAL);
   call64(EFN_INIT15MEM, 0, 4, buffer, 0, 0, 0);
   hlp_memfree(buffer);
}

void init_physmem(void) {
   physmem_entries = 2;
   physmem[0].startaddr = 0;
   physmem[0].blocklen  = int12mem()<<10;
   physmem[1].startaddr = highbase;
   physmem[1].blocklen  = highlen;
   // starting block for memory manager init
   headblock = 1;
}

AcpiMemInfo* _std exit_efi(void **loadermem, u32t *loadersize) {
   /* unlike in int15mem_int() this allocation is forever, buffer will be
      reused by 64-bit code to store GDT, IDT & exception stack */
   void *buffer = hlp_memalloc(_128KB, QSMA_NOCLEAR);
   // shutdown i/o, at least ....
   exit_prepare();
   // get memory map & call ExitBootServices
   call64(EFN_INIT15MEM, 0, 4, buffer, 1, loadermem, loadersize);
   // call is safe without lock because exit_prepare() above set it forever
   return int15mem_copy();
}
