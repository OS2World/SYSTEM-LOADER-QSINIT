//
// QSINIT
// virtual disk management module
//
#ifndef QSINIT_VHDD_INT
#define QSINIT_VHDD_INT

#include "qsbase.h"
#include "qsint.h"
#include "qserr.h"
#include "qsdm.h"
#include "qcl/qsvdimg.h"
#include "qcl/sys/qsedinfo.h"
#include "stdlib.h"
#include "dskinfo.h"

u32t _std diskio_read (u32t disk, u64t sector, u32t count, void *data);
u32t _std diskio_write(u32t disk, u64t sector, u32t count, void *data);

extern qs_dyndisk    mounted[];
extern u32t          cid_ext,         ///< qs_extdisk class id
                    cid_vhdd,
                    cid_vhdf,
                    cid_binf;

u32t init_rwdisk(void);
u32t init_vhdisk(void);
u32t init_bfdisk(void);

#define VHDD_SIGN        0x64644856   ///< VHdd string

#define instance_ret(type,inst,err)   \
   type *inst = (type*)data;          \
   if (inst->sign!=VHDD_SIGN) return err;

#define instance_void(type,inst)      \
   type *inst = (type*)data;          \
   if (inst->sign!=VHDD_SIGN) return;

/// "qs_dyndisk_ext" class data
typedef struct {
   u32t           sign;
   qs_dyndisk     self;
} de_data;

#endif // QSINIT_VHDD_INT
