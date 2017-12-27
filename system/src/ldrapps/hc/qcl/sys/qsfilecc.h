//
// QSINIT API
// low level file cache implemenation class
//
#ifndef QS_FILE_CACHE_CLASS
#define QS_FILE_CACHE_CLASS

#include "qstypes.h"
#include "qsclass.h"
#include "qsio.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
   u32t   cluster;
   u32t       len;
} cc_loc;

/// callback to the filesystem: allocate clusters
typedef qserr _std (*cc_cladd) (void *udta, u32t hint, u32t count, cc_loc **list);
/// callback to the filesystem: release clusters
typedef qserr _std (*cc_clfree)(void *udta, cc_loc *list);
/// callback to the filesystem: change file attributes (size & time)
typedef qserr _std (*cc_chattr)(void *udta, io_handle_info *fi);

/// @name _qs_filecc.init call options
#define FCCF_READONLY       0x00001  ///< disable write calls and stat updates
#define FCCF_DIRECT         0x00002  ///< skip sector level cache for all calls
#define FCCF_NOCACHE        0x00004  ///< disable file level cache at all
//@}

typedef struct qs_filecc_s {
   /** set disk information.
       File must be closed during call of this function.
       @param  clsize       cluster size bit shift
       @return error code or 0 */
   qserr   _exicc (*setdisk)(void *udta, cc_cladd af, cc_clfree df, cc_chattr cf,
                             u32t disk, u64t datapos, u32t cllim, u8t clsize);
   /** opens a file.
       note, that fi->vsize must be valid. It used in cache calculations and
       data after this position is always returned as zero-filled. */
   qserr   _exicc (*init   )(io_handle_info *fi, cc_loc *list, u32t opts);
   /// flush file cache & director information
   qserr   _exicc (*flush  )(void);
   /** close file.
       File is closed even if function returns error (which is flush() call
       error, actually). */
   qserr   _exicc (*close  )(void);
   qserr   _exicc (*chsize )(u64t size);
   /* file stat.
      Both vol & fileno fields in info are filled by -1 on exit */
   qserr   _exicc (*stat   )(io_handle_info *info);
   /// return FFFF64 on error (damaged or uninitialized data)
   u64t    _exicc (*size   )(void);
   qserr   _exicc (*read   )(u64t pos, void *buf, u32t size, u32t *act);
   qserr   _exicc (*write  )(u64t pos, void *buf, u32t size, u32t *act);
} _qs_filecc, *qs_filecc;

#ifdef __cplusplus
}
#endif
#endif // QS_FILE_CACHE_CLASS
