//
// QSINIT
// Virtual PC fixed size image footer
//

#ifndef MSVHD_STRUCT
#define MSVHD_STRUCT

#include "qstypes.h"

// big endian byte order
typedef struct _vhd_footer {
   char     creator[8]; // "conectix"
   u32t       features; // always 2?
   u32t        version; // 0x10000
   u64t       nextdata; // offset of next header structure, FFFF64 if no one
   u32t         crtime; // seconds since 01.01.2000 (UTC)
   char     appname[4]; // "vpc "
   u16t          major; // 5.3 for MS VPC 2007
   u16t          minor;
   char      osname[4]; // "Wi2k"
   u64t       org_size;
   u64t       cur_size;
   u16t           cyls;
   u8t           heads; // max 16
   u8t             spt; // max 255
   u32t           type; // 2 for fixed size image
   u32t       checksum; // check sum (this struct except this field)
   u8t        uuid[16]; // parent disk UUID
   u8t     saved_state;
} vhd_footer;

#define VHD_TIME_BASE 946684800   // unixtime of 01.01.2000 00:00:00

#endif // MSVHD_STRUCT
