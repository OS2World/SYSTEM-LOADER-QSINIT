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
#define E_TYPE_PTE          (0x00030000)         ///< partition management
#define E_TYPE_LVM          (0x00040000)         ///< LVM errors
#define E_TYPE_MT           (0x00050000)         ///< MT lib errors
#define E_TYPE_CON          (0x00060000)         ///< Console errors

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
#define E_SYS_UBREAK        (E_TYPE_SYS+0x0021)  ///< user break, operation not complete
#define E_SYS_CPLIB         (E_TYPE_SYS+0x0022)  ///< missing codepage support
#define E_SYS_EFIHOST       (E_TYPE_SYS+0x0023)  ///< not supported on EFI host
#define E_SYS_INTVAL        (E_TYPE_SYS+0x0024)  ///< invalid integer value
#define E_SYS_EOF           (E_TYPE_SYS+0x0025)  ///< (unexpected) end of file reached
#define E_SYS_CRC           (E_TYPE_SYS+0x0026)  ///< CRC error in file
#define E_SYS_NOTFOUND      (E_TYPE_SYS+0x0027)  ///< entry not found
#define E_SYS_DIRNOTEMPTY   (E_TYPE_SYS+0x0028)  ///< directory not empty
#define E_SYS_ISDIR         (E_TYPE_SYS+0x0029)  ///< specified name is a directory

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_DSK_ERRREAD       (E_TYPE_DSK+0x0001)  ///< disk read error
#define E_DSK_ERRWRITE      (E_TYPE_DSK+0x0002)  ///< disk write error
#define E_DSK_NOTREADY      (E_TYPE_DSK+0x0003)
#define E_DSK_WP            (E_TYPE_DSK+0x0004)  ///< disk is write protected
#define E_DSK_UNKFS         (E_TYPE_DSK+0x0005)  ///< filesystem is not determined
#define E_DSK_NOTMOUNTED    (E_TYPE_DSK+0x0006)  ///< device or filesystem is not mounted
#define E_DSK_BADVOLNAME    (E_TYPE_DSK+0x0007)  ///< invalid volume name
#define E_DSK_DISKNUM       (E_TYPE_DSK+0x0008)  ///< invalid disk number value
#define E_DSK_PT2TB         (E_TYPE_DSK+0x0009)  ///< partition too large
#define E_DSK_DISKFULL      (E_TYPE_DSK+0x000A)  ///< disk is full
#define E_DSK_MOUNTERR      (E_TYPE_DSK+0x000B)  ///< device mounting error
#define E_DSK_UMOUNTERR     (E_TYPE_DSK+0x000C)  ///< failed to unmount destination volume
#define E_DSK_MOUNTED       (E_TYPE_DSK+0x000D)  ///< partition already mounted
#define E_DSK_NOLETTER      (E_TYPE_DSK+0x000E)  ///< there is no free drive letter
#define E_DSK_2TBERR        (E_TYPE_DSK+0x000F)  ///< unable to access disk data above 2TB
#define E_DSK_SSIZE         (E_TYPE_DSK+0x0010)  ///< unsupported sector size
#define E_DSK_VLARGE        (E_TYPE_DSK+0x0011)  ///< volume too large for selected filesystem
#define E_DSK_VSMALL        (E_TYPE_DSK+0x0012)  ///< volume too small to fit filesystem structures
#define E_DSK_SELTYPE       (E_TYPE_DSK+0x0013)  ///< failed to select FAT type (16 or 32)
#define E_DSK_CLSIZE        (E_TYPE_DSK+0x0014)  ///< wrong cluster size parameter
#define E_DSK_CNAMELEN      (E_TYPE_DSK+0x0015)  ///< custom boot file name length too long
#define E_DSK_FSMISMATCH    (E_TYPE_DSK+0x0016)  ///< file system type mismatch (dsk_newvbr())
#define E_DSK_FILESIZE      (E_TYPE_DSK+0x0017)  ///< file size is not match file system information
#define E_DSK_FILEALLOC     (E_TYPE_DSK+0x0018)  ///< file allocation data is incorrect
#define E_DSK_FSSTRUCT      (E_TYPE_DSK+0x0019)  ///< file system structures are invalid or damaged
#define E_DSK_NOTPHYS       (E_TYPE_DSK+0x001A)  ///< volume is not a physical disk

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_PTE_FLOPPY        (E_TYPE_PTE+0x0001)  ///< big floppy, pt is empty
#define E_PTE_EMPTY         (E_TYPE_PTE+0x0002)  ///< disk is not partitioned
#define E_PTE_INVALID       (E_TYPE_PTE+0x0003)  ///< invalid table, valid part must be used r/o
#define E_PTE_GPTDISK       (E_TYPE_PTE+0x0004)  ///< disk have GPT partition table
#define E_PTE_HYBRID        (E_TYPE_PTE+0x0005)  ///< disk have hybrid partition table (GPT+MBR)
#define E_PTE_MBRDISK       (E_TYPE_PTE+0x0006)  ///< disk have MBR partition table
#define E_PTE_PINDEX        (E_TYPE_PTE+0x0007)  ///< unable to find partition index
#define E_PTE_PRIMARY       (E_TYPE_PTE+0x0008)  ///< partition is not primary
#define E_PTE_EXTENDED      (E_TYPE_PTE+0x0009)  ///< partition is extended and cannot be active
#define E_PTE_RESCAN        (E_TYPE_PTE+0x000A)  ///< rescan requred (start sector is not matched to pt index)
#define E_PTE_NOFREE        (E_TYPE_PTE+0x000B)  ///< there is no free space on this position
#define E_PTE_SMALL         (E_TYPE_PTE+0x000C)  ///< partition too small to create
#define E_PTE_EXTERR        (E_TYPE_PTE+0x000D)  ///< extended partition processing error
#define E_PTE_NOPRFREE      (E_TYPE_PTE+0x000E)  ///< no free space in primary table
#define E_PTE_FINDEX        (E_TYPE_PTE+0x000F)  ///< invalid free space index
#define E_PTE_FSMALL        (E_TYPE_PTE+0x0010)  ///< free space is smaller than specified
#define E_PTE_LARGE         (E_TYPE_PTE+0x0011)  ///< partition too large (64-bit number of sectors)
#define E_PTE_GPTHDR        (E_TYPE_PTE+0x0012)  ///< GPT header is broken
#define E_PTE_GPTLARGE      (E_TYPE_PTE+0x0013)  ///< GPT header too large to process
#define E_PTE_GPTHDR2       (E_TYPE_PTE+0x0014)  ///< second GPT header is broken
#define E_PTE_EXTPOP        (E_TYPE_PTE+0x0015)  ///< extended partition is not empty and cannot be deleted
#define E_PTE_INCOMPAT      (E_TYPE_PTE+0x0016)  ///< incompatible disks (dsk_clonestruct() - sector size or spt mismatch)
#define E_PTE_CSPACE        (E_TYPE_PTE+0x0017)  ///< there is no free space on target disk for clone
#define E_PTE_BOOTPT        (E_TYPE_PTE+0x0018)  ///< unable to use boot partition as target

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_LVM_NOINFO        (E_TYPE_LVM+0x0001)  ///< no LVM info on disk
#define E_LVM_NOBLOCK       (E_TYPE_LVM+0x0002)  ///< no partition table info block
#define E_LVM_NOPART        (E_TYPE_LVM+0x0003)  ///< no info for one of existing partition
#define E_LVM_SERIAL        (E_TYPE_LVM+0x0004)  ///< serial number mismatch
#define E_LVM_GEOMETRY      (E_TYPE_LVM+0x0005)  ///< geometry mismatch
#define E_LVM_LETTER        (E_TYPE_LVM+0x0006)  ///< already used or invalid drive letter
#define E_LVM_PTNAME        (E_TYPE_LVM+0x0007)  ///< no partition with specified name
#define E_LVM_DSKNAME       (E_TYPE_LVM+0x0008)  ///< no disk with specified name
#define E_LVM_LOWPART       (E_TYPE_LVM+0x0009)  ///< partition placed in 1st 63 sectors
#define E_LVM_BADSERIAL     (E_TYPE_LVM+0x000A)  ///< invalid serial number

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
#define E_MOD_LIBEXEC       (E_TYPE_MOD+0x0035)  ///< unable to execute DLL module

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
#define E_MT_THREADLIMIT    (E_TYPE_MT +0x0011)  ///< too many threads per process (>4k)
#define E_MT_SUSPENDED      (E_TYPE_MT +0x0012)  ///< thread is suspended
#define E_MT_WAITACTIVE     (E_TYPE_MT +0x0013)  ///< someone waits for this mutex or event

/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = */
#define E_CON_MODERR        (E_TYPE_CON+0x0001)  ///< video mode is not supported
#define E_CON_DETACHED      (E_TYPE_CON+0x0002)  ///< not supported in detached session
#define E_CON_DEVLIMIT      (E_TYPE_CON+0x0003)  ///< too many vio devices
#define E_CON_NODEVICE      (E_TYPE_CON+0x0004)  ///< no vio device with such number
#define E_CON_SESNO         (E_TYPE_CON+0x0005)  ///< invalid session number
#define E_CON_BADHANDLER    (E_TYPE_CON+0x0006)  ///< invalid vio handler device
#define E_CON_DUPDEVICE     (E_TYPE_CON+0x0007)  ///< device with such parameters already exist
#define E_CON_NOSESMODE     (E_TYPE_CON+0x0008)  ///< device has no mode for one or more active sessions
#define E_CON_BADMODEID     (E_TYPE_CON+0x0009)  ///< invalid mode id value

#endif // QS_ERR_NUMBERS
