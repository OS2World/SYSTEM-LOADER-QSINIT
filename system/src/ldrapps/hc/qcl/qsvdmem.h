//
// QSINIT API
// huge ram disk support (for RAM above 4Gb border)
// (memory-based virtual disk)
//
#ifndef QSINIT_VDISKAPI
#define QSINIT_VDISKAPI

#ifdef __cplusplus
extern "C" {
#endif

#include "qstypes.h"
#include "qsclass.h"
#include "qsutil.h"

/// @name _qs_vdisk.init() flags
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
#define VFDF_PERSIST    0x00200       ///< try to use existing RAM disk
#define VFDF_EXACTSIZE  0x00400       ///< disk size must be equal to minsize value
//@}

/** PAE ram disk shared class.
    can be called from any module:
    @code
       qs_vdisk disk = NEW(qs_vdisk);
       DELETE(disk);
    @endcode */
typedef struct qs_vdisk_s {
   /** create ram disk.
       Only one disk at time allowed.

       VFDF_PERSIST flag will force to check for RAM disk header presence at the
       start of 4th Gb. On success (and ONLY when RAM disk has no pages below 4Gb) -
       it will be mounted and any other call parameters will be ignored!

       @param  [out]  disk        Disk handle for i/o functions
   
       @retval 0                  on success
       @retval E_SYS_EXIST        success too, but disk found by VFDF_PERSIST flag!
       @retval E_SYS_INITED       disk already inited
       @retval E_SYS_UNSUPPORTED  no PAE on this CPU
       @retval E_SYS_NOMEM        not enough memory
       @retval E_SYS_INVPARM      invalid arguments */
   qserr _exicc (*init   )(u32t minsize, u32t maxsize, u32t flags, u32t divpos,
                           char letter1, char letter2, u32t *disk);

   /** load ram disk from the disk image file.
       Only one disk at time allowed.
       Disk image file - is a plain image or "fixed size" VHD.
       @param  path               File path (can be QSINIT path or file in the
                                  root of boot volume).
       @param  flags              Flags (VFDF_NOHIGH and VFDF_NOLOW only)
       @param  [out]  disk        Disk handle for i/o functions
       @param  cbprint            Callback for file loading process (can be 0)
   
       @retval 0                  on success
       @retval E_SYS_INITED       disk already inited
       @retval E_SYS_UNSUPPORTED  no PAE on this CPU
       @retval E_SYS_NOMEM        not enough memory
       @retval E_SYS_NOFILE       no such file
       @retval E_SYS_BADFMT       this is dinamic VHD (not "fixed size") or
                                  file size is not sector aligned
       @retval E_SYS_TOOSMALL     file too small
       @retval E_DSK_ERRREAD      file read error
       @retval E_SYS_INVPARM      invalid arguments */
   qserr _exicc (*load   )(const char *path, u32t flags, u32t *disk,
                           read_callback cbprint);

   /** delete active ram disk.
       Function unmap ram disk from QSINIT and free memory, used by it.
       On QSINIT exit this function is not called - this preserve disk
       contents for OS/2 boot device driver.
       
       @return boolean success flag (1/0) */
   u32t  _exicc (*free   )(void);

   /** return common info about active ram disk.
       Any arg here can be 0.
       @param  [out]  info      disk size & geometry info
       @param  [out]  disk      Disk handle for i/o functions
       @param  [out]  sectors   Number of sectors on disk
       @param  [out]  seclo     Number of sectors in memory below 4Gb border
       @param  [out]  physpage  Number of physical 4k page with disk header
       @return error code value or 0 */
   qserr _exicc (*query  )(disk_geo_data *geo, u32t *disk, u32t *sectors,
                           u32t *seclo, u32t *physpage);

   /** replace "physical disk geometry".
       @param  geo      new disk geometry (only CHS value can be changed)
       @return 0 on success or error value */
   qserr _exicc (*setgeo )(disk_geo_data *geo);

   /** clean disk header and the start of high memory.
       ADD driver for OS/2 (HD4DISK.ADD) checks start of memory above 4Gb
       border for disk header. This call just looks at the same place and
       clean disk header and start of disk data (up 2 Mb). 

       @retval 0                  on success
       @retval E_SYS_INITED       disk already inited
       @retval E_SYS_NOMEM        no memory above 4Gb
       @retval E_SYS_NONINITOBJ   there is no disk at this location
       @retval E_SYS_UNSUPPORTED  no PAE on this CPU */
   qserr _exicc (*clean  )(void);
} _qs_vdisk, *qs_vdisk;

#ifdef __cplusplus
}
#endif

#endif // QSINIT_VDISKAPI
