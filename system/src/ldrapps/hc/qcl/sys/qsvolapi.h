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
               but function returns E_DSK_UNKFS error code here */
   qserr     _exicc (*init)     (u8t vol, u32t flags, void *bootsec);
   /// unmount volume
   qserr     _exicc (*done)     (void);
   /** query volume info.
       note, that info->FsVer should contain 0 if FS was not recognized, else
       any non-zero value (ex. FAT type)
       @param   info     Returning information
       @param   fast     Fast version flag (1/0). Fast call returns zero or
                         wrong information in serial number, label and free
                         space fields.
       @return error code */
   qserr     _exicc (*volinfo)  (disk_volume_data *info, int fast);
   qserr     _exicc (*setlabel) (const char *label);
   /** mounted drive letter.
       @return -1 if volume is not mounted, else volume number (0..x) */
   int       _exicc (*drive)    (void);

   /** open a file.
       @param   [in]  name    File path
       @param   [in]  mode    File mode (file is always opened once per system,
                              in write mode)
       @param   [out] pfh     Ptr to file handle
       @param   [out] action  Actual action was performed on open.
       @return error code. Note, what E_SYS_READONLY means SUCCESS open, but
               file will deny any write activity. */
   qserr     _exicc (*open)     (const char *name, u32t mode, io_handle_int *pfh, u32t *action);
   qserr     _exicc (*read)     (io_handle_int fh, u64t pos, void *buffer, u32t *size);
   qserr     _exicc (*write)    (io_handle_int fh, u64t pos, void *buffer, u32t *size);
   qserr     _exicc (*flush)    (io_handle_int fh);
   qserr     _exicc (*close)    (io_handle_int fh);
   qserr     _exicc (*size)     (io_handle_int fh, u64t *size);
   qserr     _exicc (*setsize)  (io_handle_int fh, u64t newsize);

   qserr     _exicc (*setattr)  (const char *path, u32t attr);
   qserr     _exicc (*getattr)  (const char *path, u32t *attr);
   qserr     _exicc (*setexattr)(const char *path, const char *aname, void *data, u32t size);
   qserr     _exicc (*getexattr)(const char *path, const char *aname, void *buffer, u32t *size);
   str_list  _exicc (*lstexattr)(const char *path);
   qserr     _exicc (*remove)   (const char *path);
   qserr     _exicc (*renmove)  (const char *oldname, const char *newname);

   qserr     _exicc (*makepath) (const char *path);
   qserr     _exicc (*rmdir)    (const char *path);

   qserr     _exicc (*pathinfo) (const char *path, io_handle_info *info);
   qserr     _exicc (*setinfo)  (const char *path, io_handle_info *info, u32t flags);

   qserr     _exicc (*diropen)  (const char *path, dir_handle_int *pdh);
   qserr     _exicc (*dirnext)  (dir_handle_int dh, io_direntry_info *de);
   qserr     _exicc (*dirclose) (dir_handle_int dh);
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
#define SFIO_NOMOUNT            1     ///< Force mounting and ignore FS detection
#define SFIO_FORCE              2     ///< Force mounting (if FS is not recognized)
//@}

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VOLIO_CLASS
