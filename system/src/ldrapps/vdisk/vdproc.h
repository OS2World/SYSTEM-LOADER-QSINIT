//
// QSINIT
// PAE ramdisk support module internals
//
#ifndef VDISK_INTERNAL
#define VDISK_INTERNAL

#include "qsbase.h"
#include "qcl/qsvdmem.h"
#include "qsdm.h"
#include "stdlib.h"

extern u32t  classid_vd;  ///< qs_vdisk class id

u32t  _exicc vdisk_init(EXI_DATA, u32t minsize, u32t maxsize, u32t flags,
                        u32t divpos, char letter1, char letter2, u32t *disk);

u32t  _exicc vdisk_load(EXI_DATA, const char *path, u32t flags, u32t *disk,
                        read_callback cbprint);

qserr _exicc vdisk_query(EXI_DATA, disk_geo_data *geo, u32t *disk,
                         u32t *sectors, u32t *seclo, u32t *physpage);

u32t  _exicc vdisk_free(EXI_DATA);

int  register_class(void);
int  unregister_class(void);

#endif // VDISK_INTERNAL
