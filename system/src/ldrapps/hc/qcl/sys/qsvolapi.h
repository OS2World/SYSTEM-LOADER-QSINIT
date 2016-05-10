//
// QSINIT API
// low level volume api
//
#ifndef QSINIT_VOLIO_CLASS
#define QSINIT_VOLIO_CLASS

#include "qstypes.h"
#include "qsclass.h"
#include "qsutil.h"
#include "qsio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t   io_handle_int;
typedef u32t  dir_handle_int;

/** Low level volume api, provided by fs implementation.
    This code is, actually, internal thing and should not be used
    on user level (even if you can acquire such ptr in some way ;). */
typedef struct qs_sysvolume_s {
   /** initialize it.
       @param   vol      Volume (0==A, 1==B and so on)
       @param   flags    SFIO_* flags
       @param   bootsec  Boot sector data (only when append==0)
       @return error code. If SFIO_FORCE is set and no FS - volume is mounted,
               but function returns E_DSK_UNCKFS error code here */
   qserr     _std (*init)     (u8t vol, u32t flags, void *bootsec);
   /// unmount volume
   qserr     _std (*done)     (void);
   /** query volume info.
       note, that info->FsVer should contain 0 if FS was not recognized, else
       any non-zero value (ex. FAT type)
       @return error code */
   qserr     _std (*volinfo)  (disk_volume_data *info);
   qserr     _std (*setlabel) (const char *label);
   /** mounted drive letter.
       @return -1 if volume is not mounted, else volume number (0..x) */
   int       _std (*drive)    (void);

   /** open a file.
       @param   [in]  name    File path
       @param   [in]  mode    File mode (file is always opened once per system,
                              in write mode)
       @param   [out] pfh     Ptr to file handle
       @param   [out] action  Actual action was performed on open
       @return error code */
   qserr     _std (*open)     (const char *name, u32t mode, io_handle_int *pfh, u32t *action);
   qserr     _std (*read)     (io_handle_int fh, u64t pos, void *buffer, u32t *size);
   qserr     _std (*write)    (io_handle_int fh, u64t pos, void *buffer, u32t *size);
   qserr     _std (*flush)    (io_handle_int fh);
   qserr     _std (*close)    (io_handle_int fh);
   qserr     _std (*size)     (io_handle_int fh, u64t *size);
   qserr     _std (*setsize)  (io_handle_int fh, u64t newsize);

   qserr     _std (*setattr)  (const char *path, u32t attr);
   qserr     _std (*getattr)  (const char *path, u32t *attr);
   qserr     _std (*setexattr)(const char *path, const char *aname, void *data, u32t size);
   qserr     _std (*getexattr)(const char *path, const char *aname, void *buffer, u32t *size);
   str_list  _std (*lstexattr)(const char *path);
   qserr     _std (*remove)   (const char *path);
   qserr     _std (*renmove)  (const char *oldname, const char *newname);

   qserr     _std (*makepath) (const char *path);
   qserr     _std (*rmdir)    (const char *path);

   qserr     _std (*pathinfo) (const char *path, io_handle_info *info);
   qserr     _std (*setinfo)  (const char *path, io_handle_info *info, u32t flags);

   qserr     _std (*diropen)  (const char *path, dir_handle_int *pdh);
   qserr     _std (*dirnext)  (dir_handle_int dh, io_direntry_info *de);
   qserr     _std (*dirclose) (dir_handle_int dh);
} _qs_sysvolume, *qs_sysvolume;

/// @name io_open() mode value
//@{
#define SFOM_OPEN_EXISTING      0     ///< Opens the file, fails if the file is not existing
#define SFOM_OPEN_ALWAYS        1     ///< Opens existing file, else create new
#define SFOM_CREATE_NEW         2     ///< Creates a new file, fails if file is existing
#define SFOM_CREATE_ALWAYS      3     ///< Creates a new file, existing file will be overwritten
//@}

/// @name init() flags
//@{
#define SFIO_APPEND             1     ///< Boot time flag (i/o replacement in START`s start)
#define SFIO_FORCE              2     ///< Force mounting (if FS is not recognized)
//@}

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VOLIO_CLASS
