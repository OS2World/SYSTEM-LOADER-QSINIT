//
// QSINIT API
// stat.h emu
//
#ifndef QSINIT_STAT_H
#define QSINIT_STAT_H

#ifdef __cplusplus
extern "C" {
#endif
#include "time.h"

#pragma pack(1)

struct stat {
   u16t          st_dev;   ///< volume file resides on
   u32t          st_ino;   ///< always 0
   u16t         st_mode;   ///< file mode
   u16t        st_nlink;   ///< always 1
   u16t          st_uid;   ///< always 0
   u16t          st_gid;   ///< always 0
   u16t         st_rdev;   ///< always 0
   u64t         st_size;   ///< file size
   time_t      st_atime;   ///< should be file last access time
   time_t      st_mtime;   ///< file last modify time
   time_t      st_ctime;   ///< should be file creation time
};

// file attribute constants for st_mode field
#define S_IFMT       0xF000  ///< type of file
#define S_IFDIR      0x4000  ///< directory
#define S_IFCHR      0x2000  ///< character special file
#define S_IFIFO      0x1000  ///< FIFO
#define S_IFREG      0x8000  ///< regular
#define S_IREAD      0x0100  ///< read permission
#define S_IWRITE     0x0080  ///< write permission
#define S_IEXEC      0x0040  ///< execute permission

#define S_ISBLK(m)    0
#define S_ISCHR(m)    (((m)&S_IFMT)==S_IFCHR)
#define S_ISDIR(m)    (((m)&S_IFMT)==S_IFDIR)
#define S_ISFIFO(m)   (((m)&S_IFMT)==S_IFIFO)
#define S_ISREG(m)    (((m)&S_IFMT)==S_IFREG)
#define S_ISLNK(m)    0
#define S_ISSOCK(m)   0

#define S_IRWXU      0x01C0
#define S_IRUSR      0x0100
#define S_IWUSR      0x0080
#define S_IXUSR      0x0040

#define S_IRWXG      0x0038
#define S_IRGRP      0x0020
#define S_IWGRP      0x0010
#define S_IXGRP      0x0008

#define S_IRWXO      0x0007
#define S_IROTH      0x0004
#define S_IWOTH      0x0002
#define S_IXOTH      0x0001

int      _std stat(const char *path, struct stat *buf);

#define _stati64 stat

#pragma pack()

#ifdef __cplusplus
}
#endif
#endif // QSINIT_STAT_H
