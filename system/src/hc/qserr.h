//
// QSINIT API
// error numbers
//
#ifndef QS_ERR_NUMBERS
#define QS_ERR_NUMBERS

#define E_OK                (0x00000000)         ///< lucky you ;)

#define E_TYPE_MASK         (0xFFFF0000)         ///< mask for error type
#define E_TYPE_SYS          (0x00000000)         ///< common errors
#define E_TYPE_MOD          (0x00010000)         ///< module loader errors
#define E_TYPE_DSK          (0x00020000)         ///< disk i/o errors
#define E_TYPE_MT           (0x00050000)         ///< MT lib errors

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_SYS_DONE          (E_TYPE_SYS+0x0001)  ///< duplicate call/operation
#define E_SYS_NOMEM         (E_TYPE_SYS+0x0002)  ///< no memory available
#define E_SYS_NOFILE        (E_TYPE_SYS+0x0003)  ///< no such file
#define E_SYS_NOPATH        (E_TYPE_SYS+0x0004)  ///< no such path
#define E_SYS_INVNAME       (E_TYPE_SYS+0x0005)  ///< invalid file or path name
#define E_SYS_EXIST         (E_TYPE_SYS+0x0006)  ///< file or directory exists
#define E_SYS_ACCESS        (E_TYPE_SYS+0x0007)  ///< access denied
#define E_SYS_INVPARM       (E_TYPE_SYS+0x0008)  ///< invalid parameter
#define E_SYS_INVOBJECT     (E_TYPE_SYS+0x0009)  ///< invalid object
#define E_SYS_FILES         (E_TYPE_SYS+0x000A)  ///< too many open files
#define E_SYS_SHARE         (E_TYPE_SYS+0x000B)  ///< operation is rejected by sharing policy
#define E_SYS_TIMEOUT       (E_TYPE_SYS+0x000C)  ///< operation failed due timeout
#define E_SYS_INCOMPPARAMS  (E_TYPE_SYS+0x000D)  ///< operation cannot be done with such arg combination
#define E_SYS_UNSUPPORTED   (E_TYPE_SYS+0x000E)  ///< unsupported feature
#define E_SYS_ZEROPTR       (E_TYPE_SYS+0x000F)  ///< zero pointer supplied (invalid parameter)
#define E_SYS_LONGNAME      (E_TYPE_SYS+0x0010)  ///< too long file name or path
#define E_SYS_INVTIME       (E_TYPE_SYS+0x0011)  ///< invalid time value
#define E_SYS_FSLIMIT       (E_TYPE_SYS+0x0012)  ///< filesystem file size limit reached
#define E_SYS_BUFSMALL      (E_TYPE_SYS+0x0013)  ///< supplied buffer too small
#define E_SYS_INVHTYPE      (E_TYPE_SYS+0x0014)  ///< invalid handle type
#define E_SYS_PRIVATEHANDLE (E_TYPE_SYS+0x0015)  ///< handle is valid, but for other process
#define E_SYS_SEEKERR       (E_TYPE_SYS+0x0016)  ///< seek to the invalid position
#define E_SYS_DELAYED       (E_TYPE_SYS+0x0017)  ///< operation delayed until resource is free
#define E_SYS_DISKMISMATCH  (E_TYPE_SYS+0x0018)  ///< rename/move destination must be on the same volume
#define E_SYS_BROKENFILE    (E_TYPE_SYS+0x0019)  ///< file is broken (on unmounted volume and so on)
#define E_SYS_NONINITOBJ    (E_TYPE_SYS+0x001A)  ///< object was not initialized
#define E_SYS_READONLY      (E_TYPE_SYS+0x001B)  ///< object is read-only
#define E_SYS_TOOLARGE      (E_TYPE_SYS+0x001C)  ///< value too large
#define E_SYS_TOOSMALL      (E_TYPE_SYS+0x001D)  ///< value too small
#define E_SYS_INITED        (E_TYPE_SYS+0x001E)  ///< object already initialized
#define E_SYS_BADFMT        (E_TYPE_SYS+0x001F)  ///< incompatible file format
#define E_SYS_SOFTFAULT     (E_TYPE_SYS+0x0020)  ///< software fault occurs

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_DSK_IO            (E_TYPE_DSK+0x0001)
#define E_DSK_NOTREADY      (E_TYPE_DSK+0x0002)
#define E_DSK_WP            (E_TYPE_DSK+0x0003)  ///< disk is write protected
#define E_DSK_UNCKFS        (E_TYPE_DSK+0x0004)  ///< filesystem is not determined
#define E_DSK_NOTMOUNTED    (E_TYPE_DSK+0x0005)  ///< device or filesystem is not mounted
#define E_DSK_BADVOLNAME    (E_TYPE_DSK+0x0006)  ///< invalid volume name
#define E_DSK_DISKNUM       (E_TYPE_DSK+0x0007)  ///< invalid disk number value
#define E_DSK_PT2TB         (E_TYPE_DSK+0x0008)  ///< partition too large
#define E_DSK_DISKFULL      (E_TYPE_DSK+0x0009)  ///< disk is full
#define E_DSK_MOUNTERR      (E_TYPE_DSK+0x000A)  ///< device mounting error

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_MOD_NOT_LX        (E_TYPE_MOD+0x0001)  ///< not LE/LX
#define E_MOD_FLAGS         (E_TYPE_MOD+0x0002)  ///< bad module flags
#define E_MOD_EMPTY         (E_TYPE_MOD+0x0003)  ///< empty module (no start address, etc)
#define E_MOD_UNSUPPORTED   (E_TYPE_MOD+0x0004)  ///< unsupported feature (export by name, etc)
#define E_MOD_BROKENFILE    (E_TYPE_MOD+0x0005)  ///< broken file
#define E_MOD_NOSELECTOR    (E_TYPE_MOD+0x0006)  ///< no free selector
#define E_MOD_OBJLOADERR    (E_TYPE_MOD+0x0007)  ///< object load error
#define E_MOD_INVPAGETABLE  (E_TYPE_MOD+0x0008)  ///< invalid page table
#define E_MOD_NOEXTCODE     (E_TYPE_MOD+0x0009)  ///< additional code not loaded
#define E_MOD_MODLIMIT      (E_TYPE_MOD+0x000A)  ///< too many imported modules
#define E_MOD_BADFIXUP      (E_TYPE_MOD+0x000B)  ///< invalid fixup
#define E_MOD_NOORD         (E_TYPE_MOD+0x000C)  ///< no required ordinal
#define E_MOD_ITERPAGEERR   (E_TYPE_MOD+0x000D)  ///< decompress error
#define E_MOD_BADEXPORT     (E_TYPE_MOD+0x000E)  ///< invalid entry table entry
#define E_MOD_INITFAILED    (E_TYPE_MOD+0x000F)  ///< DLL module init proc return 0
#define E_MOD_CRCERROR      (E_TYPE_MOD+0x0010)  ///< CRC error in delayed module unzipping
#define E_MOD_EXPLIMIT      (E_TYPE_MOD+0x0011)  ///< too manu exports ("start" module only)
#define E_MOD_READERROR     (E_TYPE_MOD+0x0012)  ///< disk read error
#define E_MOD_START16       (E_TYPE_MOD+0x0013)  ///< start object can`t be 16 bit
#define E_MOD_STACK16       (E_TYPE_MOD+0x0014)  ///< stack object can`t be 16 bit
#define E_MOD_HANDLE        (E_TYPE_MOD+0x0030)  ///< bad module handle
#define E_MOD_FSELF         (E_TYPE_MOD+0x0031)  ///< trying to free self
#define E_MOD_FSYSTEM       (E_TYPE_MOD+0x0032)  ///< trying to free system module
#define E_MOD_LIBTERM       (E_TYPE_MOD+0x0033)  ///< DLL term function denied unloading
#define E_MOD_EXECINPROC    (E_TYPE_MOD+0x0034)  ///< mod_exec() in progress

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_MT_DISABLED       (E_TYPE_MT +0x0001)  ///< MT mode does not started or disabled
#define E_MT_TIMER          (E_TYPE_MT +0x0002)  ///< unable to install timer handlers
#define E_MT_OLDCPU         (E_TYPE_MT +0x0003)  ///< no local APIC on CPU (P5 and below)
#define E_MT_BADTID         (E_TYPE_MT +0x0004)  ///< bad thread ID
#define E_MT_BADFIB         (E_TYPE_MT +0x0005)  ///< bad fiber index
#define E_MT_BUSY           (E_TYPE_MT +0x0006)  ///< thread is waiting for something
#define E_MT_ACCESS         (E_TYPE_MT +0x0007)  ///< main or system thread cannot be terminated
#define E_MT_GONE           (E_TYPE_MT +0x0008)  ///< thread has finished
#define E_MT_TLSINDEX       (E_TYPE_MT +0x0009)  ///< TLS slot number is invalid
#define E_MT_BADPID         (E_TYPE_MT +0x000A)  ///< unknown process ID
#define E_MT_NOTSUSPENDED   (E_TYPE_MT +0x000B)  ///< thread is not suspended
#define E_MT_DUPNAME        (E_TYPE_MT +0x000C)  ///< duplicate sync object name
#define E_MT_NOTOWNER       (E_TYPE_MT +0x000D)  ///< caller does not own the semaphore
#define E_MT_SEMBUSY        (E_TYPE_MT +0x000E)  ///< semaphore is busy
#define E_MT_LOCKLIMIT      (E_TYPE_MT +0x000F)  ///< too many nested lock calls (>64k)
#define E_MT_SEMFREE        (E_TYPE_MT +0x0010)  ///< semaphore is free

#endif // QS_ERR_NUMBERS
