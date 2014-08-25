//
// QSINIT
// common functions
//
#ifndef QSINIT_UTIL
#define QSINIT_UTIL

#include "qstypes.h"
#include "qslog.h"
#include "qstime.h"

#ifdef __cplusplus
extern "C" {
#endif

//===================================================================
//  global memory management
//-------------------------------------------------------------------

/// @name flags for hlp_memalloc
//@{
/** Allocate unresizeable block until the end of work.
    Block cannot be freed. Available part of it (all blocks is rounded
    up to 64k) will be used internally for small block heap */
#define QSMA_READONLY 0x001
/// return 0 instead of immediate panic on block alloc failure
#define QSMA_RETERR   0x002
/// do not clear block (flag affect only first alloc, but not further reallocs)
#define QSMA_NOCLEAR  0x004
//@}

/// allocate and zero fill memory
void* _std hlp_memalloc(u32t size, u32t flags);

/** free memory.
    @param addr  Address, must point to the begin of allocated block */
void  _std hlp_memfree(void *addr);

/** realloc region of memory.
    Additional memory is zero-filled
    @result new/old address */
void* _std hlp_memrealloc(void *addr, u32t newsize);

/// query list of constant blocks (internal use only)
u32t  _std hlp_memqconst(u32t *array, u32t pairsize);

/// query block size by pointer
u32t  _std hlp_memgetsize(void *addr);

/** try to reserve (allocate) physical memory.
    This call allocate memory at specified physical address - if this
    range included into memory manager space and not used.

    QSINIT memory manager use memory starting from 16Mb to first hole
    in PC memory map.
    Use of memory below 16Mb is prohibited (it reserved for OS/2 boot).
    Other memory (not belonged to memory manager) can be allocated by
    sys_markmem() call in qssys.h.

    Function return aligned down to 64k address of reserved block (block
    is NOT zero filled, unlike hlp_memalloc()). Block can be used as usual
    memory block and released by hlp_memfree().

    @attention block length can be smaller than requested, it must be
    checked by hlp_memgetsize() call. This can occur on reserving memory
    at the end of memory manager space.

    @param physaddr Physical address (QSINIT FLAT space is NOT zero-based!!)
    @param length   Length of memory to reserve.
    @return block address or 0 if address is used or beyond memory manager
            limits. */
void* _std hlp_memreserve(u32t physaddr, u32t length);

/** query available memory.
    @param maxblock  Maximum size of single block, can be 0
    @param total     Total memory size, (can be) used by QSINIT, can be 0.
    @return total available memory in bytes */
u32t  _std hlp_memavail(u32t *maxblock, u32t *total);

/// dump global memory table to log/serial port
void  _std hlp_memprint(void);

/** dump complete memory table to log/serial port.
    This function print entire PC memory, including blocks above 4Gb */
void  _std hlp_memcprint(void);

//===================================================================
//  boot filesystem (OS/2 micro-FSD) file management
//-------------------------------------------------------------------

/// init file i/o
void  _std hlp_finit();

/** open file.
    @param  name  File name (boot disk root dir only).
    @result -1 on no file, else file size */
u32t  _std hlp_fopen(const char *name);

/// read file
u32t  _std hlp_fread(u32t offset, void *buf, u32t bufsize);

/// close file
void  _std hlp_fclose();

/** callback for hlp_freadfull.
    @param [in]  percent  Currently readed percent (always 0 on PXE boot)
    @param [in]  readed   Currently readed size in bytes */
typedef void _std (*read_callback)(u32t percent, u32t readed);

/** open, read, close file & return buffer with it.
    @param [in]  name     File name
    @param [out] bufsize  Size of file
    @param [in]  cbprint  Callback for process indication, can be 0.
    @result buffer with file or 0. Buffer must be freed via hlp_memfree() */
void* _std hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint);

/** shutdown entire file i/o.
    @attention warning! this call shutdown both micro-FSD and virtual disk management!!!
*/
void  _std hlp_fdone();

//===================================================================
//  virtual FS current directory
//-------------------------------------------------------------------

/** set current dir.
    Set current dir for current or other (if full path specified) drive.
    @param  path   Path to set, can be relative
    @result success flag (1/0) */
u32t  _std hlp_chdir(const char *path);

/** set current drive
    @param  drive  drive number: 0 - boot disk (FAT boot only), 1 - virtual disk,
                   2-9 - mounted volumes.
    @result success flag (1/0) */
u32t  _std hlp_chdisk(u8t drive);

/** get current drive.
    @result disk number as in hlp_chdisk() */
u8t   _std hlp_curdisk(void);

/** get current dir on specified drive.
    @param  drive   drive number: 0 - boot disk (FAT boot only), 1 - virtual disk,
                    2-9 - mounted volumes.
    @result current dir or 0 */
char* _std hlp_curdir(u8t drive);

//===================================================================
//  low level physical disk access
//-------------------------------------------------------------------

/** return number of installed hard disks in system.
    @param  [out]  floppies - ptr to number of floppy disks, can be 0.
    @result number of hard disks */
u32t  _std hlp_diskcount(u32t *floppies);

#define QDSK_FLOPPY      0x080    ///< add to disk number to index floppy disks
#define QDSK_VIRTUAL     0x100    ///< add to disk number to index QSINIT memory disks
#define QDSK_DISKMASK    0x07F    ///< disk index mask
/** skip sector cache call.
    internal flag for hlp_diskread/hlp_diskwrite disk number only! */
#define QDSK_DIRECT    0x20000

typedef struct {
   u32t      Cylinders;
   u32t          Heads;
   u32t    SectOnTrack;
   u32t     SectorSize;
   u64t   TotalSectors;
} disk_geo_data;

/** return disk size.
    @attention BIOS 16384x63x16 has returned in geo parameter for HDDs.
    This value usable for floppy drives/ancient small HDDs only. Use
    dsk_getptgeo() to get actual MBR partition table geometry.

    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector, can be 0
    @param [out]  geo       Disk data (optional, can be 0, can return zeroed data)
    @result number of sectors on disk */
u32t  _std hlp_disksize(u32t disk, u32t *sectsize, disk_geo_data *geo);

/** return 64bit disk size.
    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector, can be 0
    @result number of sectors on disk */
u64t  _std hlp_disksize64(u32t disk, u32t *sectsize);

/** read physical disk.
    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of sectors to read
    @param  data      Buffer for data
    @result number of sectors was actually readed */
u32t _std hlp_diskread(u32t disk, u64t sector, u32t count, void *data);

/** write to physical disk.
    Be careful with QSEL_VIRTUAL flag! 0x100 value mean boot partition, 0x101 -
    virtual disk with QSINIT apps!

    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of sectors to write
    @param  data      Buffer with data
    @result number of sectors was actually written */
u32t _std hlp_diskwrite(u32t disk, u64t sector, u32t count, void *data);

/** is FDD was changed?
    @param  disk      Disk number.
    @return -1 if no disk, not a floppy or no changeline on it, 0 - if was not
            changed, 1 - is changed since last call */
int  _std hlp_fddline(u32t disk);

/// @name hlp_diskmode() flags
//@{
#define HDM_USECHS      0       ///< Use ancient CHS BIOS mode (up to 8 Gb size)
#define HDM_USELBA      1       ///< Use LBA access (up to 2Tb)
#define HDM_QUERY       2       ///< Query only bit
#define HDM_EMULATED    4       ///< Disk is emulated (return only)
//@}

/** switch disk access mode.
    "CHSONLY" ini key makes the same thing (HDM_USECHS) for boot disk.
    @param  disk      Disk number (HDD only).
    @param  flags     Flags value. Use HDM_QUERY to query current mode,
                      HDM_USECHS or HDM_USELBA to set new mode.
                      HDM_EMULATED can only be returned with HDM_QUERY
                      flag, such disks not accept mode change.
    @result (HDM_QUERY|current access mode) or 0 on error */
u32t _std hlp_diskmode(u32t disk, u32t flags);

/** try to mount a part of disk as FAT/FAT32 partition.
    @param  drive     Drive number: 2..9 only, remounting of 0,1 is not allowed.
    @param  disk      Disk number.
    @param  sector    Start sector of partition
    @param  count     Partition length in sectors
    @result 1 on success. */
u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count);

typedef struct {
   u32t      SerialNum;         ///< Volume serial (from BPB)
   u32t         ClSize;         ///< Sectors per cluster
   u32t    RootDirSize;         ///< Number of root directory entries (FAT12/16)
   u16t     SectorSize;         ///< Bytes per sector
   u16t      FatCopies;         ///< Number of FAT copies
   u32t           Disk;         ///< Disk number
   u64t    StartSector;         ///< Start sector of volume (partition)
   u32t   TotalSectors;         ///< Total sectors on volume
   u32t     DataSector;         ///< First data sector on volume (disk-relative)
   char      Label[12];         ///< Volume Label
   u32t    BadClusters;         ///< Bad clusters (not supported, non-zero only after format)
} disk_volume_data;

/// @name hlp_volinfo() result
//@{
#define FST_NOTMOUNTED  0       ///< Volume is not mounted
#define FST_FAT12       1       ///< FAT12 volume
#define FST_FAT16       2       ///< FAT16 volume
#define FST_FAT32       3       ///< FAT32 volume
//@}

/** mounted volume info.
    Actually function is bad designed, it return FST_NOTMOUNTED both
    on non-mounted volume and mounted non-FAT volume. To check volume
    presence info.TotalSectors can be used, it will always be 0 if volume
    is not mounted.

    @param  info      Buffer for data to fill in, can be 0.
    @result FAT_* constant */
u32t _std hlp_volinfo(u8t drive, disk_volume_data *info);

/** set volume label.
    @param  drive     Drive number: 0..9.
    @param  label     Up to 11 chars of volume label or 0 to clear it.
    @result 1 on success. */
u32t _std hlp_vollabel(u8t drive, const char *label);

/** unmount previously mounted partition.
    @param  drive     Drive number: 2..9 only, unmounting of 0,1 is not allowed.
    @result 1 on success. */
u32t _std hlp_unmountvol(u8t drive);

//===================================================================
//  other
//-------------------------------------------------------------------

/** allocate selector(s).
    @param  count     Number of selectors (remember - QSINIT GDT is SMALL :))
    @result base number of 0 */
u16t  _std hlp_selalloc(u32t count);

/** GDT descriptor setup.
    @param  selector  GDT offset (i.e. selector ith 0 RPL field)
    @param  base      Base address (physical or current DS based)
    @param  limit     Limit in pages or bytes
    @param  type      Setup flags
    @return bool - success flag. */
u32t  _std hlp_selsetup(u16t selector, u32t base, u32t limit, u32t type);

/// @name flags for hlp_selsetup
//@{
#define QSEL_CODE    0x001    ///< code selector, else data
#define QSEL_16BIT   0x002    ///< selector is 16 bit
#define QSEL_BYTES   0x004    ///< limit in bytes, not pages
#define QSEL_LINEAR  0x008    ///< base is FLAT address, not physical (obsolette, ignored)
#define QSEL_R3      0x010    ///< ring 3
//@}

/** query selector base address and limit.
    @attention this is physical base, not address in QSINIT FLAT space.

    @param [in]  Sel     Selector
    @param [out] Limit   Limit, can be zero.
    @result 0xFFFFFFFF if failed. */
u32t  _std hlp_selbase(u32t Sel, u32t *Limit);

/** free selector.
    this function can`t be used to free system selectors (flat cs, ds, etc).
    @result bool - success flag */
u32t  _std hlp_selfree(u32t selector);

/** copy Length bytes from Sel:Offset to FLAT:Destination.
    Function check Sel,Offset & Length, but does not check Destination.
    @param Destination   Destination buffer
    @param Offset        Source offset
    @param Sel           Source selector
    @param Length        length of data to copy
    @result number of bytes was copied */
u32t  _std hlp_copytoflat(void* Destination, u32t Offset, u32t Sel, u32t Length);

/** convert real mode segment to flat address.
    QSINIT FLAT space is zero-based since version 0.12 (rev.281).

    BUT! To use any memory above the end of RAM in first 4GBs (call
    sys_endofram() to query it) - pag_physmap() MUST be used. This is the
    only way to preserve access to this memory when QSINIT will be switched
    to PAE paging mode (can occur at any time by single API call).

    @param Segment       Real mode segment
    @return flat address */
u32t  _std hlp_segtoflat(u16t Segment);

/// rm 16:16 to qsinit flat addr conversion (for lvalue only)
#define hlp_rmtopm(x) (hlp_segtoflat((u32t)(x)>>16) + (u16t)x)

/** real mode function call with arguments.
    @param rmfunc  16 bit far pointer / FLAT addr,
                   if high word (segment) < 10, then this is FLAT addr in first 640k
    @param dwcopy  Number of words to copy to real mode stack.
    @param ...     Whose arguments. Be careful with 32bit push and 16bit args! ;)

    @result real mode dx:ax and OF, SF, ZF, AF, PF, CF flags */
u32t __cdecl hlp_rmcall(u32t rmfunc,u32t dwcopy,...);

#ifdef __WATCOMC__
#define rmcall(func,...) hlp_rmcall((u32t)&(func),__VA_ARGS__)
#endif

/** execute cpuid instruction.
    @param [in]     index      cpuid command index
    @param [out]    data       4 dwords buffer for eax,ebx,ecx,edx values
    @return bool - success flag (it is 486 CPU if no cpuid present) */
u32t _std hlp_getcpuid(u32t index, u32t *data);

/** read MSR register.
    Function does not protect from wrong index!
    Use hlp_getmsrsafe() instead of this function.
    @param [in]     index      register numner
    @param [out]    ddlo       ptr to low dword (eax value), can be 0
    @param [out]    ddhi       ptr to high dword (edx value), can be 0
    @return bool - success flag, return 0 in both dwords if failed */
u32t _std hlp_readmsr(u32t index, u32t *ddlo, u32t *ddhi);

/** write MSR register.
    Function does not protect from wrong index & value!
    Use hlp_setmsrsafe() instead of this function.
    @param  index      register numner
    @param  ddlo       low dword (eax value)
    @param  ddhi       high dword (edx value)
    @return bool - success flag */
u32t _std hlp_writemsr(u32t index, u32t ddlo, u32t ddhi);

/// @name result of hlp_boottype()
//@{
#define QSBT_NONE        0    ///< no boot source
#define QSBT_FAT         1    ///< boot partition is FAT and accessed as 0: drive
#define QSBT_FSD         2    ///< boot partition use micro-FSD i/o
#define QSBT_PXE         3    ///< PXE boot, only hlp_freadfull() can be used
#define QSBT_SINGLE      4    ///< FAT boot without OS/2 (no OS2BOOT in the root)
//@}

/** query current type of boot.
    @return QSBT_* constant */
u32t _std hlp_boottype(void);

/// @name type of hlp_querybios() call
//@{
#define QBIO_APM         0    ///< query APM presence, return ver or 0x100xx, where xx - error code
#define QBIO_PCI         1    ///< query PCI BIOS (B101h), return bx<<16|cl<<8| al or 0 if no PCI
//@}

u32t _std hlp_querybios(u32t index);

/** query safe mode state.
    Safe mode invoked by left shift press on QSINIT loading.
    @return bool - yes/no. */
u32t _std hlp_insafemode(void);

/// @name exit_pm32 error codes
//@{
#define QERR_NO486       1
#define QERR_DPMIERR     6
#define QERR_NOMEMORY    8
#define QERR_NOTHING     9
#define QERR_MCBERROR   10
#define QERR_FATERROR   11
#define QERR_DISKERROR  12
#define QERR_NOEXTDATA  13
#define QERR_RUNEXTDATA 14
#define QERR_NODISKMEM  15
//@}

/** abort execution and stop.
    @param rc  Error code for text message */
void _std exit_pm32(int rc);

/** exit and restart another os2ldr.
    @param loader  OS2LDR file name */
void _std exit_restart(char *loader);

/// @name exit_bootmbr flags
//@{
#define EMBR_OWNCODE     1    ///< use own MBR code instead of disk one
#define EMBR_NOACTIVE    2    ///< ignore active partition absence
//@}

/** boot from specified HDD.
    @param disk    Disk number (QDSK_*)
    @param ownmbr  Flag 1 to replace disk MBR code to self-provided
    @return EINVAL on invalid disk handle, ENODEV on empty partition table,
            EIO if i/o error occured and ENOTBLK if disk is emulated hdd */
int  _std exit_bootmbr(u32t disk, u32t flags);

/** boot from specified partition.
    @param disk    Disk number (QDSK_*)
    @param index   Partition index (see dmgr ops/qsdm.h)
    @param letter  Set drive letter for OS/2 boot (0 to ignore/default)
    @return EINVAL  on invalid disk handle or index, 
            ENODEV  on empty or unrecognized data in boot sector, 
            EIO     if i/o error occured,
            ENOTBLK if disk is emulated hdd,
            EFBIG   if partition (or a part of it) is beyond of 2TB border */
int  _std exit_bootvbr(u32t disk, u32t index, char letter);

/** power off the system (APM only).
    Suspend can work too - on some middle aged motherboards ;)
    @return 0 if failed, 1 if system really powered off */
u32t _std exit_poweroff(int suspend);

/** internal call: prepare to leave QSINIT.
    this function call all installed exit callbacks
    @see exit_handler() */
void _std exit_prepare(void);

/// exit QSINIT callback function, called from exit_prepare.
typedef void (*exit_callback)(void);

/** add/remove exit_callback handler.
    @param func  Callback ptr
    @param add   bool - add/remove */
void _std exit_handler(exit_callback func, u32t add);

#ifdef __cplusplus
}
#elif defined(__WATCOMC__) // exit call to jmp replacement in watcom C only
void _std exit_pm32s(int rc);

#pragma aux exit_pm32s aborts;
#define exit_pm32 exit_pm32s
#endif // __WATCOMC__

#endif // QSINIT_UTIL
