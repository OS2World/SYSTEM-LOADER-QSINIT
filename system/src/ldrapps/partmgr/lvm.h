//
// QSINIT "partmgr" module
// OS/2 LVM functions header
//
#ifndef QSINIT_LVMFUNC
#define QSINIT_LVMFUNC

#include "lvmdat.h"

/** get partition's pos in MBR and DLAT arrays
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [out] recidx   index in hdd_info.pts array
    @param [out] dlatidx  index in dlat record ( dlat[recidx>>2].DLA_Array[dlatidx] )
    @return 0 on success or LVME_* error code. */
int  lvm_dlatpos(u32t disk, u32t index, u32t *recidx, u32t *dlatidx);

/** flush DLAT sector for specified parimary or extended MBR.
    @param disk     disk number
    @param quadidx  dlat index (0 for primary, 1..x - extended)
    @return 0 on success or LVME_* error code. */
int  lvm_flushdlat(u32t disk, u32t quadidx);

/// zero all DLAT sectors on disk
int  lvm_wipeall(u32t disk);

#endif // QSINIT_LVMFUNC
