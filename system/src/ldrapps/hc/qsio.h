//
// QSINIT API
// file i/o management
//
#ifndef QSINIT_FIO
#define QSINIT_FIO

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "qsshell.h"
#include "qsutil.h"
#include "time.h"

#ifndef QS_MAXPATH
#define QS_MAXPATH 260
#endif

typedef qshandle   io_handle;
typedef qshandle  dir_handle;

u8t      _std io_curdisk  (void);
qserr    _std io_setdisk  (u8t drive);
qserr    _std io_curdir   (char *buf, u32t size);
qserr    _std io_diskdir  (u8t drive, char *buf, u32t size);
/** is drive letter mounted to something?
    @param  [in]  drive       drive letter to check (0..x)
    @param  [out] lavail      last mounted drive letter (can be 0)
    @return boolean flag (1/0) */
u32t     _std io_ismounted(u8t drive, u8t *lavail);

/// hlp_mountvol() analogue, but returns error code.
qserr    _std io_mount    (u8t drive, u32t disk, u64t sector, u64t count);

/** unmount volume.
    hlp_unmountvol() doing the same, with IOUM_FORCE flag on.
    By default - function asks in popup message about open files
    on this volume. IOUM_NOASK flag force error returning instead
    and IOUM_FORCE forces unmount.
    @param  drive             drive letter to unmount (0..x)
    @param  flags             IOUM_* flags.
    @retval 0                 on success
    @retval E_DSK_BADVOLNAME  invalid "drive" parameter
    @retval E_DSK_NOTMOUNTED  volume is not mounted
    @retval E_SYS_ACCESS      unmount denied (use IOUM_FORCE to force it) */
qserr    _std io_unmount  (u8t drive, u32t flags);

/// @name io_unmount() flags
//@{
#define IOUM_FORCE         0x0001     ///< force unmount
#define IOUM_NOASK         0x0002     ///< do not ask about open files
//@}

/** query volume info.
    This is hlp_volinfo() analogue, but it returns error code instead of FAT
    type.
    @param  drive     Drive number: 0..x.
    @param  info      Volume information. Unlike hlp_volinfo(), this pointer
                      CANNOT be NULL 
    @return error code */
qserr    _std io_volinfo  (u8t drive, disk_volume_data *info);

/** set volume label.
    @param  drive     Drive number: 0..9.
    @param  label     Up to 11 chars of volume label or 0 to clear it.
    @return error code. */
qserr    _std io_vollabel (u8t drive, const char *label);

/** Get full path.
    @param  [out] buffer   buffer for result
    @param  [in]  path     path, relative to current directory (current dir
                           assumed if path=0)
    @param  [in]  size     buffer size
    @retval 0              on success
    @retval E_SYS_NOPATH   current directory is invalid
    @retval E_SYS_LONGNAME result path too long
    @retval E_SYS_BUFSMALL buffer to small to fit the name
    @retval E_SYS_INVNAME  error in path */
qserr    _std io_fullpath (char *buffer, const char *path, u32t size);

qserr    _std io_chdir    (const char *path);
qserr    _std io_mkdir    (const char *path);
qserr    _std io_rmdir    (const char *path);

typedef struct {
   u8t        dsec;  ///< 1/100 sec, 0..99
   u8t         sec;  ///< seconds, 0..59
   u8t         min;  ///< minutes, 0..59
   u8t        hour;  ///< hours, 0..23
   u8t         day;  ///< 1..31
   u8t         mon;  ///< month, 0..11
   u16t       year;  ///< years since 1781 (Uranium age)
} io_time;

typedef struct {
   u32t      attrs;
   s32t     fileno;                   ///< unique system fno if file is open, else -1
   u64t       size;
   io_time   ctime;
   io_time   atime;
   io_time   wtime;
} io_handle_info;

typedef struct {
   u32t      attrs;
   s32t     fileno;                   ///< unique system fno if file is open, else -1
   u64t       size;
   io_time   ctime;
   io_time   atime;
   io_time   wtime;
   char       name[QS_MAXPATH+1];     ///< file's name
   char       *dir;                   ///< directory`s full path (valid until io_findclose())
} io_direntry_info;

/// @name attributes
//@{
#define IOFA_READONLY           1     ///< read only entry
#define IOFA_HIDDEN             2     ///< hidden entry
#define IOFA_SYSTEM             4     ///< system entry
#define IOFA_DIR             0x10     ///< directory entry
#define IOFA_ARCHIVE         0x20     ///< archive entry
//@}

/// @name file open mode
//@{
#define IOFM_OPEN_EXISTING      4     ///< Opens the file, fails if no such file
#define IOFM_OPEN_ALWAYS        5     ///< Opens existing file, else create new
#define IOFM_CREATE_NEW         1     ///< Creates a new file, fails if file is existing
#define IOFM_CREATE_ALWAYS      7     ///< Creates a new file, existing file will be overwritten
#define IOFM_TRUNCATE_EXISTING  6     ///< Truncates existing file, fails if file not exist

#define IOFM_READ            0x10     ///< Read access
#define IOFM_WRITE           0x20     ///< Write access
#define IOFM_SHARE_READ    0x1000     ///< Share read
#define IOFM_SHARE_WRITE   0x2000     ///< Share write
#define IOFM_SHARE_DEL     0x4000     ///< Allow deletition by other processes
#define IOFM_SHARE_REN     0x8000     ///< Allow renaming/changing time/attr by other processes

#define IOFM_OPEN_MASK     0x0007     ////< bit mask for open type in open mode
#define IOFM_SHARE_MASK    0xF000     ////< bit mask for share bits in open mode

#define IOFM_CLOSE_DEL       0x40     ///< Delete file on last close
//@}

qserr    _std io_open     (const char *name, u32t mode, io_handle *pfh,
                           u32t *action);
u32t     _std io_read     (io_handle fh, const void *buffer, u32t size);
u32t     _std io_write    (io_handle fh, const void *buffer, u32t size);

/** set file position.
    @param fh      file handle
    @param offset  offset value (signed!)
    @param origin  pointer move method
    @return new position or -1LL (64-bit 0xFFF.... value) - on error,
            known as FFFF64 constant */
u64t     _std io_seek     (io_handle fh, s64t offset, u32t origin);

/// @name io_seek() origin value
//@{
#define IO_SEEK_SET             0
#define IO_SEEK_CUR             1
#define IO_SEEK_END             2
//@}

/** query file position.
    @param fh      file handle
    @return position or -1LL (64-bit 0xFFF.... value) - on error, well
            known as FFFF64 constant */
u64t     _std io_pos      (io_handle fh);

qserr    _std io_flush    (io_handle fh);
qserr    _std io_close    (io_handle fh);

/** query file size.
    @param fh      file handle
    @param size    ptr to returning file size
    @return error code */
qserr    _std io_size     (io_handle fh, u64t *size);

qserr    _std io_setsize  (io_handle fh, u64t newsize);

/** setup additional file options.
    @param fh      file handle
    @param flag    option(s) (IOFS_*). Note, that IOFS_DETACHED can be
                   selected by file owner only and cannot be reset back,
                   IOFS_RENONCLOSE can only be reset to 0 (i.e. this
                   terminates file renaming/moving action) and IOFS_BROKEN
                   cannot be set, only return by io_getstate().
                   Broken files deny most ops, but IOFS_DETACHED still can be
                   set for it.
    @param value   value to set for flag(s) above (1 or 0)
    @return error code */
qserr    _std io_setstate (io_handle fh, u32t flags, u32t value);
qserr    _std io_getstate (io_handle fh, u32t *flags);

qserr    _std io_lasterror(io_handle fh);
qserr    _std io_info     (io_handle fh, io_direntry_info *info);

u32t     _std io_filetype (io_handle fh);
/** duplicate file handle.
    @param  [in]  src      file handle to duplicate
    @param  [out] dst      ptr to new handle
    @param  [in]  priv     make private handle if source is detached (shared)
    @return error code */
qserr    _std io_duphandle(io_handle src, io_handle *dst, int priv);

/// @name io_open() action value
//@{
#define IOFN_EXISTED           0    ///< file existed
#define IOFN_CREATED           1    ///< file was created
#define IOFN_TRUNCATED         2    ///< file was truncated to zero size
//@}

/// @name io_setstate() flags
//@{
#define IOFS_DETACHED     0x0001    ///< detached handle (shared over system)
#define IOFS_DELONCLOSE   0x0002    ///< delete file on close
#define IOFS_RENONCLOSE   0x0004    ///< rename/move on close was scheduled
#define IOFS_BROKEN       0x0008    ///< file is broken (volume was unmounted and so on)
//@}

/// @name io_filetype() result
//@{
#define IOFT_BADHANDLE         0    ///< handle is invalid
#define IOFT_FILE              1    ///< handle is file
#define IOFT_CHAR              2    ///< handle is character device (con, aux)
#define IOFT_UNKNOWN           7    ///< handle type is unknown
//@}

qserr    _std io_setexattr(const char *path, const char *aname,
                           void *data, u32t size);
qserr    _std io_getexattr(const char *path, const char *aname,
                           void *buffer, u32t *size);
str_list*_std io_lstexattr(const char *path);
qserr    _std io_remove   (const char *path);

qserr    _std io_pathinfo (const char *path, io_handle_info *info);
qserr    _std io_setinfo  (const char *path, io_handle_info *info, u32t flags);

/// @name io_setinfo() flags
//@{
#define IOSI_ATTRS             1    ///< set file attributes
#define IOSI_CTIME             2    ///< set file creation time
#define IOSI_ATIME             4    ///< set file access time
#define IOSI_WTIME             8    ///< set file write time
//@}

qserr    _std io_diropen  (const char *path, dir_handle *pdh);
qserr    _std io_dirnext  (dir_handle dh, io_direntry_info *de);
qserr    _std io_dirclose (dir_handle dh);

qserr    _std io_lock     (io_handle fh, u64t start, u64t len);
qserr    _std io_unlock   (io_handle fh, u64t start, u64t len);

/** rename/move file.
    Note, that operation can be performed on open file, error E_SYS_DELAYED
    will be returned, but rename/move action still will be performed - after
    the last file close. You can unschedule it via io_setstate() function.

    Also note, what file must be opened with IOFM_SHARE_REN flag - to be
    renamed in this way from another process.

    @param src     src file name
    @param dst     dst file name
    @return error code */
qserr    _std io_move     (const char *src, const char *dst);

void     _std io_timetoio (io_time *dst, time_t src);
time_t   _std io_iototime (io_time *src);

/// dump system file table to log
void     _std io_dumpsft  (void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_FIO

