//
// QSINIT API
// huge ram disk support (for RAM above 4Gb border)
//
#ifndef QSINIT_VDISKAPI
#define QSINIT_VDISKAPI

#ifdef __cplusplus
extern "C" {
#endif

#include "qstypes.h"
#include "qsutil.h"

/// @name sys_vdiskinit() flags
//@{
#define VFDF_NOHIGH     0x00001       ///< do not use memory above 4Gb
#define VFDF_NOLOW      0x00002       ///< do not use memory below 4Gb
#define VFDF_NOFMT      0x00004       ///< do not format disk
#define VFDF_SPLIT      0x00008       ///< split disk to 2 partitions
#define VFDF_PERCENT    0x00010       ///< split pos in percent of disk size
#define VFDF_EMPTY      0x00020       ///< make disk with empty partition table
#define VFDF_NOFAT32    0x00040       ///< leave large disks unformatted (do not use FAT32)
#define VFDF_FAT32      0x00080       ///< try to use FAT32 for any possible disk size (i.e.>=32Mb)
#define VFDF_HPFS       0x00100       ///< format partition(s) to HPFS
//@}

/** create ram disk.
    Only one disk at time allowed.
    @param  [out]  disk      Disk handle for i/o functions

    @retval 0         on success
    @retval EEXIST    disk already exist
    @retval ENODEV    no PAE on this CPU
    @retval ENOMEM    not enough memory
    @retval EINVAL    invalid arguments */
u32t _std sys_vdiskinit(u32t minsize, u32t maxsize, u32t flags,
                        u32t divpos, char letter1, char letter2,
                        u32t *disk);

/** load ram disk from disk image file.
    Only one disk at time allowed.
    Disk image file - is a plain image or "fixed size" VHD.
    @param  path             File path (can be QSINIT path or file in the
                             root of boot volume).
    @param  flags            Flags (VFDF_NOHIGH and VFDF_NOLOW only)
    @param  [out]  disk      Disk handle for i/o functions
    @param  cbprint          Callback for file loading process (can be 0)

    @retval 0         on success
    @retval EEXIST    disk already exist
    @retval ENODEV    no PAE on this CPU
    @retval ENOMEM    not enough memory
    @retval ENOENT    no such file
    @retval ENOTBLK   this is dinamic VHD (not "fixed size") or file size is
                      not sector aligned
    @retval EFBIG     file too small or too large
    @retval EIO       file read error
    @retval EINVAL    invalid arguments */
u32t _std sys_vdiskload(const char *path, u32t flags, u32t *disk,
                        read_callback cbprint);

/** delete active ram disk.
    Function unmap ram disk from QSINIT and free memory, used by it.
    On QSINIT exit this function is not called - this preserve disk
    contents for OS/2 boot device driver.

    @return boolean success flag (1/0) */
u32t _std sys_vdiskfree(void);

/** return common info about active ram disk.
    @param  [out]  disk      Disk handle for i/o functions
    @param  [out]  sectors   Number of sectors on disk
    @param  [out]  physpage  Number of physical 4k page with disk header
    @return boolean success flag (1/0) */
u32t _std sys_vdiskinfo(u32t *disk, u32t *sectors, u32t *physpage);


#ifdef __cplusplus
}
#endif

#endif // QSINIT_VDISKAPI
