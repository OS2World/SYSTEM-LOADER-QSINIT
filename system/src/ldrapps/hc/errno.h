//
// QSINIT API
// 32bit c lib errno emu
//
#ifndef QSINIT_ERRNO
#define QSINIT_ERRNO

#ifdef __cplusplus
extern "C" {
#endif

int* __stdcall _get_errno(void);

#define errno  (*_get_errno())

#define EZERO             0    // No error
#define ENOENT            1    // No such file or directory
#define E2BIG             2    // Arg list too big
#define ENOEXEC           3    // Exec format error
#define EBADF             4    // Bad file number
#define ENOMEM            5    // Not enough memory
#define EACCES            6    // Permission denied
#define EEXIST            7    // File exists
#define EXDEV             8    // Cross-device link
#define EINVAL            9    // Invalid argument
#define ENFILE           10    // File table overflow
#define EMFILE           11    // Too many open files
#define ENOSPC           12    // No space left on device
// math errors
#define EDOM             13    // Argument too large
#define ERANGE           14    // Result too large
// file locking error
#define EDEADLK          15    // Resource deadlock would occur
#define EDEADLOCK   EDEADLK    // ...
#define EINTR            16    // System call interrupted
#define ECHILD           17    // Child does not exist
// POSIX errors
#define EAGAIN           18    // Resource unavailable, try again
#define EBUSY            19    // Device or resource busy
#define EFBIG            20    // File too large
#define EIO              21    // I/O error
#define EISDIR           22    // Is a directory
#define ENOTDIR          23    // Not a directory
#define EMLINK           24    // Too many links
#define ENOTBLK          25    // Block device required
#define ENOTTY           26    // Not a character device
#define ENXIO            27    // No such device or address
#define EPERM            28    // Not owner
#define EPIPE            29    // Broken pipe
#define EROFS            30    // Read-only file system
#define ESPIPE           31    // Illegal seek
#define ESRCH            32    // No such process
#define ETXTBSY          33    // Text file busy
#define EFAULT           34    // Bad address
#define ENAMETOOLONG     35    // Filename too long
#define ENODEV           36    // No such device
#define ENOLCK           37    // No locks available in system
#define ENOSYS           38    // Unknown system call
#define ENOTEMPTY        39    // Directory not empty
// additional Standard C error
#define EILSEQ           40    // Illegal multibyte sequence

#define EWOULDBLOCK  EAGAIN    // Operation would block
#define EBFONT           41    // Bad font file format
#define ENOSTR           42    // Device not a stream
#define ENODATA          43    // No data available
#define ETIME            44    // Timer expired
#define EOVERFLOW        45    // Value too large for defined data type
#define EBADFD           46    // File descriptor in bad state
#define ELIBACC          47    // Can not access a needed DLL module
#define ELIBBAD          48    // Accessing a corrupted DLL
#define ELIBMAX          49    // Attempting to link in too many DLLs
#define ELIBEXEC         50    // Cannot execute a DLL directly
#define EADDRINUSE       51    // Address already in use
#define EADDRNOTAVAIL    52    // Cannot assign requested address
#define ENOBUFS          53    // No buffer space available
#define EALREADY         54    // Operation already in progress
#define EINPROGRESS      55    // Operation now in progress

#define ENOMEDIUM        56    // No medium found
#define EMEDIUMTYPE      57    // Wrong medium type

// qsinit
#define ENOMNT        10240    // Device or filesystem is not mounted
#define EINVOP        10241    // Invalid operation

#ifdef __cplusplus
}
#endif

#endif // QSINIT_ERRNO
