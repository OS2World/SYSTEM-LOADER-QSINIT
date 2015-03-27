//
// QSINIT API
// 32bit c lib errno emu
//
#ifndef QSINIT_ERRNO
#define QSINIT_ERRNO

#ifdef __cplusplus
extern "C" {
#endif

// wlink go crazy on errno export/import in "start" module itself
#ifndef NO_ERRNO
extern int errno;
#pragma aux errno "_*";
#endif

#define EZERO   0       /* No error */
#define ENOENT  1       /* No such file or directory */
#define E2BIG   2       /* Arg list too big */
#define ENOEXEC 3       /* Exec format error */
#define EBADF   4       /* Bad file number */
#define ENOMEM  5       /* Not enough memory */
#define EACCES  6       /* Permission denied */
#define EEXIST  7       /* File exists */
#define EXDEV   8       /* Cross-device link */
#define EINVAL  9       /* Invalid argument */
#define ENFILE  10      /* File table overflow */
#define EMFILE  11      /* Too many open files */
#define ENOSPC  12      /* No space left on device */
/* math errors */
#define EDOM    13      /* Argument too large */
#define ERANGE  14      /* Result too large */
/* file locking error */
#define EDEADLK 15      /* Resource deadlock would occur */
#define EDEADLOCK 15    /* ... */
#define EINTR   16      /* System call interrupted */
#define ECHILD  17      /* Child does not exist */
/* POSIX errors */
#define EAGAIN  18      /* Resource unavailable, try again */
#define EBUSY   19      /* Device or resource busy */
#define EFBIG   20      /* File too large */
#define EIO     21      /* I/O error */
#define EISDIR  22      /* Is a directory */
#define ENOTDIR 23      /* Not a directory */
#define EMLINK  24      /* Too many links */
#define ENOTBLK 25      /* Block device required */
#define ENOTTY  26      /* Not a character device */
#define ENXIO   27      /* No such device or address */
#define EPERM   28      /* Not owner */
#define EPIPE   29      /* Broken pipe */
#define EROFS   30      /* Read-only file system */
#define ESPIPE  31      /* Illegal seek */
#define ESRCH   32      /* No such process */
#define ETXTBSY 33      /* Text file busy */
#define EFAULT  34      /* Bad address */
#define ENAMETOOLONG 35 /* Filename too long */
#define ENODEV  36      /* No such device */
#define ENOLCK  37      /* No locks available in system */
#define ENOSYS  38      /* Unknown system call */
#define ENOTEMPTY 39    /* Directory not empty */
/* additional Standard C error */
#define EILSEQ  40      /* Illegal multibyte sequence */
/* qsinit */
#define ENOMNT 10240    /* Device or filesystem is not mounted */
#define EINVOP 10241    /* Invalid operation */

#ifdef __cplusplus
}
#endif

#endif // QSINIT_ERRNO
