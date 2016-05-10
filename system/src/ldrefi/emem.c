#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#include "qecall.h"

physmem_block   physmem[PHYSMEM_TABLE_SIZE];
u16t    physmem_entries;
extern u8t   *DiskBufPM;  // disk i/o buffer
extern u32t    highbase,
                highlen;

u16t _std int12mem(void) {
   return 630;
}

/* fill acpi memory info in disk i/o buffer,
   mt lock is on duaring this call */
void int15mem_int(void) {
   call64(EFN_INIT15MEM, 0, 0);
}

void init_physmem(void) {
   physmem_entries = 2;
   physmem[0].startaddr = 0;
   physmem[0].blocklen  = int12mem()<<10;
   physmem[1].startaddr = highbase;
   physmem[1].blocklen  = highlen;
}
