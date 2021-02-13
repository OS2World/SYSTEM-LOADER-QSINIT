//
// QSINIT
// virtual HDD management class
//
#ifndef VHDD_INTERNALS
#define VHDD_INTERNALS

#include "qstypes.h"
#include "qsclass.h"
#include "qsutil.h"

/** "dyndisk" shared class.
    can be called from any module:
    @code
       qs_dyndisk disk = NEW(qs_dyndisk);
       DELETE(disk);
    @endcode */
typedef struct qs_dyndisk_s {
   /// create new image file
   qserr _exicc (*make   )(const char *fname, u32t sectorsize, u64t sectors);
   /** open existing image file.

       BIN format supports options in form:
        "option1=x;option2=y"
       Option list:
       * sector=x - sector size, where x=512,1024,2048,4096, default is 512
       * offset=x - offset of image in the file, default is 0
       * size=x   - length of image in the file (in sectors!), default - from
                    offset to the the end of the file.
       * spt=x    - forced sectors per track value
       * heads=x  - forced heads value

       All formats support option:
       * readonly - r/o file mode ("ro" is also accepted)

       @param  fname              file path
       @param  options            options string
       @retval 0                  on success
       @retval E_SYS_EOF          file too small for offset/size/sector args
       @retval E_DSK_INVCHS       invalid spt/heads value
       @retval E_SYS_INITED       file already opened
       @retval E_SYS_BADFMT       unknown file format
       @retval E_DSK_ERRREAD      i/o error
       @retval E_SYS_UNSUPPORTED  unknown options. */
   qserr _exicc (*open   )(const char *fname, const char *options);
   /** query "disk" info.
       @param  info     disk size & geometry info, can be 0
       @param  fname    buffer for disk image file name (_MAX_PATH+1), can be 0
       @param  sectors  number of "disk sectors" in image file, can be 0
       @param  used     number of used "disk sectors" in image file, can be 0
       @param  state    state flags (EDSTATE_*)
       @return 0 on success, else error value */
   qserr _exicc (*query  )(disk_geo_data *info, char *fname, u32t *sectors,
                           u32t *used, u32t *state);
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
       Function does not start the actual tracing! It just switches including
       of these instances into global tracing when it turned on.
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
} _qs_dyndisk, *qs_dyndisk;

#define VHDD_VHDD      "qs_dyndisk"     ///< VHDD disk image handling class
#define VHDD_VHD_F     "qs_vhddisk"     ///< Fixed size VHD handling class
#define VHDD_VHD_B     "qs_bindisk"     ///< Plain binary disk image class

#endif // VHDD_INTERNALS
