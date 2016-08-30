//
// QSINIT
// dinamycally expanded virtual HDD for partition management testing
// (file-based virtual disk)
//
#ifndef VHDD_INTERNALS
#define VHDD_INTERNALS

#include "qstypes.h"
#include "qsclass.h"
#include "qsutil.h"

/** "emudisk" shared class.
    can be called from any module:
    @code
       qs_emudisk disk = NEW(qs_emudisk);
       DELETE(disk);
    @endcode */
typedef struct qs_emudisk_s {
   /// create new image file
   qserr _exicc (*make   )(const char *fname, u32t sectorsize, u64t sectors);
   /** open existing image file.
       @param  fname    file path
       @retval 0             on success
       @retval E_DSK_UNCKFS  unknown file format. */
   qserr _exicc (*open   )(const char *fname);
   /** query "disk" info.
       @param  info     disk size & geometry info, can be 0
       @param  fname    buffer for disk image file name (_MAX_PATH+1), can be 0
       @param  sectors  number of "disk sectors" in image file, can be 0
       @param  used     number of used "disk sectors" in image file, can be 0
       @return 0 on success, else error value */
   qserr _exicc (*query  )(disk_geo_data *info, char *fname, u32t *sectors, u32t *free);
   /// read "sectors"
   u32t  _exicc (*read   )(u64t sector, u32t count, void *data);
   /// write "sectors"
   u32t  _exicc (*write  )(u64t sector, u32t count, void *data);
   /** search file for zeroed sectors and mark it to re-use.
       @param  freeF6   assume sectors, filled by byte 0xF6 as free too (boolean).
       @return number of sectors was freed */
   u32t  _exicc (*compact)(int freeF6);
   /** mount as QSINIT disk.
       @return disk number or negative value on error */
   s32t  _exicc (*mount  )(void);
   /** query QSINIT disk number.
       @return disk number or negative value if not mounted */
   s32t  _exicc (*disk   )(void);
   /// unmount "disk" from QSINIT
   qserr _exicc (*umount )(void);
   /// close image file
   qserr _exicc (*close  )(void);

   /** enable/disable tracing of this disk instance calls.
       Function not starts actual tracing! It just switches including of
       these instances into global tracing when it turned on.
       Undocumented "VHDD trace disk" command can be used in console to
       turn bitmap tracing ON for this disk.

       @param  self     enable this instance trace (0/1 or -1 to skip setup),
                        enabled by default.
       @param  bitmaps  enable internal bitmaps trace (0/1 or -1 to skip setup),
                        disabled by default.
    */
   void  _exicc (*trace  )(int self, int bitmaps);
   /** replace "physical disk geometry".
       @param  geo      new disk geometry (only CHS value can be changed)
       @return 0 on success or error value */
   qserr _exicc (*setgeo )(disk_geo_data *geo);
} _qs_emudisk, *qs_emudisk;

#endif // VHDD_INTERNALS
