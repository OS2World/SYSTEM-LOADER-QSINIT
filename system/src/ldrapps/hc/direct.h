//
// QSINIT API
// 32bit direct emu
//
#ifndef QSINIT_DIRECTH
#define QSINIT_DIRECTH

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "time.h"

#pragma pack(1)

/* File attribute constants for d_attr field */
#define _A_NORMAL       0x00    // Normal file - read/write permitted
#define _A_RDONLY       0x01    // Read-only file
#define _A_HIDDEN       0x02    // Hidden file
#define _A_SYSTEM       0x04    // System file
#define _A_VOLID        0x08    // Volume-ID entry
#define _A_SUBDIR       0x10    // Subdirectory
#define _A_ARCH         0x20    // Archive file

#define NAME_MAX        QS_MAXPATH  // LFN is supported

struct dirent {
   char     d_attr;             ///< file's attribute
   time_t   d_wtime;            ///< file's time
   time_t   d_ctime;            ///< file's creation time
   u64t     d_size;             ///< file's size
   char     d_name[NAME_MAX+1]; ///< file's name
   char    *d_openpath;         ///< path specified to opendir (with trailing \)
   struct 
   dirent  *d_subdir;           ///< subdirectory data (_dos_readtree() call)
   void    *d_sysdata;          ///< internal data
};
typedef struct dirent dir_t;

int    _std chdir (const char *path);
char*  _std getcwd(char *buf,size_t size);
int    _std mkdir (const char *path);

dir_t* _std opendir(const char*);
dir_t* _std readdir(dir_t*);
int    _std closedir(dir_t*);

int    _std rmdir(const char *path);

qserr  _std _dos_findfirst(const char *path, u16t attributes, dir_t *buffer);
qserr  _std _dos_findnext(dir_t *buffer);
qserr  _std _dos_findclose(dir_t *buffer);

void   _std _dos_getdrive(unsigned *drive);
void   _std _dos_setdrive(unsigned drive, unsigned *total);

/** tiny stat function replacement.
    @param  path    File path.
    @param  buffer  Stat data.
    @return 0 on success or QS error code */
qserr  _std _dos_stat(const char *path, dir_t *buffer);

struct diskfree_t {
   u32t     total_clusters;     ///< total clusters on disk
   u32t     avail_clusters;     ///< available clusters on disk
   u32t     sectors_per_cluster;///< sectors per cluster
   u32t     bytes_per_sector;   ///< bytes per sector
};
typedef struct diskfree_t diskfree_t;

/** get disk free space.
    @param [in]  drive      Disk number (0 - default, 1 for A:/0:, 2 for B:/1:, etc.
    @param [out] diskspace  Return data.
    @return zero on success. */
qserr  _std _dos_getdiskfree(unsigned drive, diskfree_t *diskspace);

/** callback for _dos_readtree.
    @param fp           file info. d_openpath field point to file`s directory
    @param cbinfo       user data for callback proc
    @return 1 to continue processing, 0 to skip this entry and -1
            to stop call and return 0 from _dos_readtree() */
typedef int _std (*_dos_readtree_cb)(dir_t *fp, void *cbinfo);

/** read directory tree.
    @attention Note, that list, returned by this function is a thread owned
               memory. I.e. exiting of a thread, which called this function
               will release entire list.

    @param dir          Directory.
    @param mask         File mask, can be 0 (assume *.*).<br>
                        note: directories returned without pattern match,
                        but for all unmatched entries d_attr will have
                        high bit (0x80) set. "." is never returned.
    @param [out] info   Pointer to return data. Return list of dir files
                        with 0 in last entry dir_t[].d_name. In _A_SUBDIR
                        dir_t entries d_subdir field points to subdirectory
                        data in the same format (dir_t*).<br>
                        This list must be freed by single _dos_freetree() call
                        with top dir *info value.
    @param subdirs      Scan all subdirectories flag.
    @param [out] dircnt Number of found subdirectories (ptr can be 0)
    @param callback     Callback to process data immediatly while reading.
                        One of info or callback can be 0.
    @param cbinfo       User data for callback proc.
    @return total number of found files (with subdirectories). */
u32t   _std _dos_readtree(const char *dir, const char *mask,
                          dir_t **info, int subdirs, u32t *dircnt,
                          _dos_readtree_cb callback, void *cbinfo);

/** free directory tree data.
    @param info         Info value, returned by _dos_readtree()
    @return 0 or QS error code */
qserr  _std _dos_freetree(dir_t *info);

/** set file attributes.
    @param path         File path.
    @param attributes   File attributes.
    @return 0 on success or QS error code */
qserr  _std _dos_setfileattr(const char *path, unsigned attributes);

/** get file attributes.
    @param       path         File path.
    @param [out] attributes   File attributes.
    @return 0 on success or QS error code */
qserr  _std _dos_getfileattr(const char *path, unsigned *attributes);

#define _DT_MODIFY     0x0001   ///< query/set file modification time
#define _DT_CREATE     0x0002   ///< query/set file creation time

/** set file/dir time.
    @param       path         File path.
    @param       dostime      Time in DOS format.
    @param       type         Time type (_DT_* constant), OR-ed flags
                              can be used for the same time for some values.
    @return 0 on success or QS error code */
qserr  _std _dos_setfiletime(const char *path, u32t dostime, u32t type);

/** get file/dir time.
    @param       path         File path.
    @param [out] dostime      Time in DOS format.
    @return 0 on success or QS error code */
qserr  _std _dos_getfiletime(const char *path, u32t *dostime, u32t type);

/** check string for a exist directory name.
    @param dir          Directory name.
    @return 1 if string is existing directory name, else 0 */
u32t   _std hlp_isdir(const char *dir);

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif // QSINIT_DIRECTH

