#ifndef FMKFS_HEADER
#define FMKFS_HEADER

#undef  _WIN32

#include "parttab.h"
#include "diskio.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "msvhd.h"
#ifdef PHYS_IO
#include "pdiskio.h"
#endif

DWORD   get_fattime(void);

#define FST_FAT12           1
#define FST_FAT16           2

typedef struct {
   u32t     phys;
   union {
      FILE   *df;
      u32t   dsk;
   };
   u32t     type;
   u16t      spt;  // both values can be a fake in some cases
   u16t    heads;
   u32t   mbrpos[4];
   u8t   mbrtype[4];
} media;

#define MTYPE_MBR       0x0000  ///< MBR disk/image
#define MTYPE_GPT       0x0001  ///< GPT disk/image
#define MTYPE_FLOPPY    0x0002  ///< (big)floppy disk/image
#define MTYPE_TYPEMASK  0x0007
#define MTYPE_EF        0x4000  ///< type EF partition
#define MTYPE_VHD       0x8000  ///< VHD file format

/* format to FAT12/FAT16.
   return >0 - FST_* constant, <0 - errno */
int     format(DWORD vol_start, DWORD vol_size, DWORD unitsz, WORD heads,
               WORD spt, WORD n_rootdir, DWORD hsec_offset);

/// check disk for GPT partition and for fixed VHD footer
void    disk_check(media *dst, u32t *vol_start, u32t *vol_size, u8t *sec_shift);

/// write GPT partition table into an image
void    disk_init(u32t disk_size, u32t *vol_start, u32t *vol_size);

u32t    fsize(FILE *fp);

#define swapdw(x) ((((x)&0xFF)<<8)|(((x)>>8)&(0xFF)))
u32t    swapdd(u32t value);
u64t    swapdq(u64t value);

int     bsf32 (u32t value);
int     bsr32 (u32t value);

#pragma aux swapdd =  \
    "bswap   eax"     \
    parm [eax] value [eax] modify exact [eax];

#pragma aux swapdq =  \
    "bswap   eax"     \
    "bswap   edx"     \
    "xchg    eax,edx" \
    parm [edx eax] value [edx eax] modify exact [edx eax];

#pragma aux bsf32 =   \
    "bsf     eax,ecx" \
    "jnz     bye"     \
    "mov     eax,-1"  \
"bye:"                \
    parm [ecx] value [eax] modify exact [eax];

#pragma aux bsr32 =   \
    "bsr     eax,ecx" \
    "jnz     bye"     \
    "mov     eax,-1"  \
"bye:"                \
    parm [ecx] value [eax] modify exact [eax];

void    makeguid(void *guid);

#endif // FMKFS_HEADER
