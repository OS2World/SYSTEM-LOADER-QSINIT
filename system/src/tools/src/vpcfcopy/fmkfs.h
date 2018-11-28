#ifndef FMKFS_HEADER
#define FMKFS_HEADER

#undef  _WIN32

#include "parttab.h"
#include "diskio.h"
#include <string.h>
#include <stdlib.h>

DWORD get_fattime(void);

#define FST_FAT12           1
#define FST_FAT16           2

/* format to FAT12/FAT16.
   return >0 - FST_* constant, <0 - errno */
int format(DWORD vol_start, DWORD vol_size, DWORD unitsz, WORD heads, WORD spt, WORD n_rootdir);

#endif // FMKFS_HEADER
