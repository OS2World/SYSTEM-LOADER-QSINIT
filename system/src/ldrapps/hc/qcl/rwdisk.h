//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
//
#ifndef VHDD_INTERNALS
#define VHDD_INTERNALS

#include "qstypes.h"
#include "qsclass.h"
#include "qsutil.h"
#include "errno.h"

/** "emudisk" shared class.
    can be called from any module:
    @code
       qs_emudisk disk = NEW(qs_emudisk);
       DELETE(disk);
    @endcode
    all int results is errno value here. */
typedef struct qs_emudisk_s {
   /// create new image file
   int  _std (*make   )(const char *fname, u32t sectorsize, u64t sectors);
   /// open existing image file
   int  _std (*open   )(const char *fname);
   /** query "disk" info.
       @param  info     disk size & geometry info, can be 0
       @param  fname    buffer for disk image file name (_MAX_PATH+1), can be 0
       @param  sectors  number of "disk sectors" in image file, can be 0
       @param  used     number of used "disk sectors" in image file, can be 0
       @return 0 on success, else error value */
   int  _std (*query  )(disk_geo_data *info, char *fname, u32t *sectors, u32t *free);
   /// read "sectors"
   u32t _std (*read   )(u64t sector, u32t count, void *data);
   /// write "sectors"
   u32t _std (*write  )(u64t sector, u32t count, void *data);
   /** search file for zeroed sectors and mark it to re-use.
       @param  freeF6   assume sectors, filled by byte 0xF6 as free too (boolean).
       @return number of sectors was freed */
   u32t _std (*compact)(int freeF6);
   /** mount as QSINIT disk.
       @return disk number or negative value on error */
   s32t _std (*mount  )(void);
   /** query QSINIT disk number.
       @return disk number or negative value if not mounted */
   s32t _std (*disk   )(void);
   /// unmount "disk" from QSINIT
   int  _std (*umount )(void);
   /// close image file
   int  _std (*close  )(void);
} _qs_emudisk, *qs_emudisk;

#endif // VHDD_INTERNALS
