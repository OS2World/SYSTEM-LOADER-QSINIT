//
// QSINIT "bootos2" module
// OS/2 kernel memory arena definitions (& other misc kernel consts)
//
#ifndef _OS2KRNL_ARENAS
#define _OS2KRNL_ARENAS

#include "qstypes.h"
#pragma pack(1)

typedef struct {
   u32t         a_paddr;        // physical address
   u32t          a_size;        // size
   u32t         a_laddr;        // virtual address
   u16t           a_sel;        // selector
   u16t        a_aflags;        // flags
   u32t        a_oflags;        // object table flags
   u16t         a_owner;        // owner
} k_arena;

#define AF_INVALID   0x0001     // invalid address range
#define AF_FREE      0x0002     // free address range
#define AF_LADDR     0x0004     // a_laddr field is valid
#define AF_PADDR     0x0008     // object has sparse physical mapping
#define AF_DOSHLP    0x0010     // source is DOSHLP
#define AF_FSD       0x0020     // source is FSD
#define AF_RIPL      0x0030     // source is RIPL
#define AF_OS2KRNL   0x0040     // source is kernel image
#define AF_OS2LDR    0x0050     // source is os2ldr
#define AF_SYM       0x0060     // source is SYM file image
#define AF_SRCMASK   0x0070     // object source mask
#define AF_OBJMASK   0x1F00     // kernel object table index mask
#define AF_FAST      0x2000     // "fast memory" arena
#define AF_PEND      0x4000     // physical end of arena
#define AF_VEND      0x8000     // virtual end of arena

#define AF_OBJSHIFT       8     // kernel object table index shift

#define ARENACOUNT  (PAGESIZE/sizeof(k_arena)) // max. number of arenas

// some arena owner codes (used only, not a full list)
#define OS2ROMDATA      0xFF37     // ROM data
#define ALLOCPHYSOWNER  0xFF41     // mark block as allocated via DevHlp AllocPhys
#define DOSHLPOWNER     0xFF6D     // doshelp segment
#define FSMFSDOWNER     0xFF9D     // mini-fsd
#define OS2KRNLOWNER    0xFFAA     // os2krnl image
#define OS2LDROWNER     0xFFAB     // os2ldr discardable part
#define OS2RIPLOWNER    0xFFAC     // RIPL image
#define OS4SYMOWNER     0xFFF9     // OS/4 kernel sym file
#define MFSDSYMOWNER    0xFFFA     // OS/4 kernel mini-fsd sym file

// some kernel image special flags
#define OBJHIMEM        OBJDISCARD // store object in high memory
#define OBJTILE         OBJPRELOAD // objects in low memory with sel==seg future
#define OBJNOFLAT       OBJIOPL    // no FLAT references allowed

#define MAXSYSPAGE      0xFFFC0    // 

#define INFOSEG_SEL     0x80       // InfoSeg selector
#define INFOSEG_SIZE    0x72       // InfoSeg struct size
#define INFOSEG_VERSION 0x15       // InfoSeg version data pos

//#define MPDATAFLAT      0xFF800000 // MPDATA FLAT address

#define MVDM_INST_DATA_ADDR  (_1MB+_64KB)  // resident MVDM kernel data
#define MVDM_SINST_DATA_ADDR (_1MB+_128KB) // swappable MVDM kernel data

#pragma pack()

#endif // _OS2KRNL_ARENAS
