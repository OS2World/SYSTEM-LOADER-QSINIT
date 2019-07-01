//
// QSINIT "fslib" module
// internal header
//
#ifndef QSINIT_FSIO_INT
#define QSINIT_FSIO_INT

#include "qsbase.h"
#include "qsint.h"
#include "stdlib.h"
#include "parttab.h"
#include "qcl/bitmaps.h"
#include "qcl/qslist.h"
#include "qcl/sys/qsvolapi.h"
#include "qcl/sys/qsfilecc.h"

extern vol_data *_extvol;

u32t register_hpfs(void);
u32t register_jfs(void);

#endif // QSINIT_FSIO_INT
