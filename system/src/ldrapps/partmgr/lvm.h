//
// QSINIT "partmgr" module
// OS/2 LVM functions header
//
#ifndef QSINIT_LVMFUNC
#define QSINIT_LVMFUNC

#include "lvmdat.h"

#ifdef __cplusplus
extern "C" {
#endif

/// init crc table for LVM code
void lvm_buildcrc(void);

/** get partition's pos in MBR and DLAT arrays
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [out] recidx   index in hdd_info.pts array
    @param [out] dlatidx  index in dlat record ( dlat[recidx>>2].DLA_Array[dlatidx] )
    @return 0 on success or error code. */
qserr _std lvm_dlatpos(u32t disk, u32t index, u32t *recidx, u32t *dlatidx);

/** flush DLAT sector for specified parimary or extended MBR.
    @param disk     disk number
    @param quadidx  dlat index (0 for primary, 1..x - extended)
    @return 0 on success or error code. */
qserr _std lvm_flushdlat(u32t disk, u32t quadidx);

/// flush all modified DLATs (with CRC=FFFF). Function doesn`t checks anything!
qserr _std lvm_flushall(u32t disk, int force);

/// zero all DLAT sectors on disk
qserr _std lvm_wipeall(u32t disk);

/** replace serial numbers for disk & partitions.
    LVM info must be in valid state, i.e. lvm_checkinfo() must return ok
    before this call.
    @param disk         disk number
    @param disk_serial  new disk serial number (use 0 to auto-generate)
    @param boot_serial  new boot disk serial number (use 0 to auto-handle, i.e.
                        it will be replaced to disk_serial only if it was the
                        same before).
    @param genpt        flag to 1 to generate new serial numbers for partitions
    @return 0 on success or error code. */
qserr _std lvm_newserials(u32t disk, u32t disk_serial, u32t boot_serial, int genpt);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_LVMFUNC
