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

/// allocate and zero fill memory.
void* _std hlp_memalloc(u32t size, u32t flags);

/** allocate and zero fill memory with info signature.
    The same as hlp_memalloc(), but with 4 chars signature to be saved for
    information reasons (can be viewed in hlp_memprint() dump).
    @param size      Size of memory to alloc.
    @param sig       Signature, 4 chars string, can be 0.
    @param flags     Options (QSMA_* flags).
    @return pointer to allocated block or 0 (if QSMA_RETERR specified). */
void* _std hlp_memallocsig(u32t size, const char *sig, u32t flags);

/** free memory.
    @param addr      Address, must point to the begin of allocated block */
void  _std hlp_memfree(void *addr);

/** realloc region of memory.
    Additional memory is zero-filled
    @return new/old address */
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

    @param physaddr  Physical address.
    @param length    Length of memory to reserve.
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
    In EFI build function read files from \EFI\BOOT dir of EFI system volume.
    @param  name  File name (boot disk root dir only), up to 63 chars.
    @return -1 on no file, else file size */
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
    @return buffer with file or 0. Buffer must be freed via hlp_memfree() */
void* _std hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint);

/** shutdown entire file i/o.
    @attention warning! this call shutdown both micro-FSD and virtual disk
               management!!! */
void  _std hlp_fdone();

//===================================================================
//  virtual FS current directory
//-------------------------------------------------------------------

/** set current dir.
    Set current dir for current or other (if full path specified) drive.
    @param  path   Path to set, can be relative
    @return success flag (1/0) */
u32t  _std hlp_chdir(const char *path);

/** set current drive
    @param  drive  drive number: 0 - boot disk (FAT boot only), 1 - virtual disk,
                   2-9 - mounted volumes.
    @return success flag (1/0) */
u32t  _std hlp_chdisk(u8t drive);

/** get current drive.
    @return disk number as in hlp_chdisk() */
u8t   _std hlp_curdisk(void);

/** get current dir on specified drive.
    @param  drive   drive number: 0 - boot disk (FAT boot only), 1 - virtual disk,
                    2-9 - mounted volumes.
    @return current dir or 0 */
char* _std hlp_curdir(u8t drive);

//===================================================================
//  low level physical disk access
//-------------------------------------------------------------------

/** return number of installed hard disks in system.
    @attention hard disks numeration can have holes at the place of
               "removed" disks!
    @param  [out]  floppies - ptr to number of floppy disks, can be 0.
    @return maximum hdd used number + 1 */
u32t  _std hlp_diskcount(u32t *floppies);

#define QDSK_FLOPPY      0x080    ///< floppy disk
#define QDSK_VOLUME      0x100    ///< volume (i.e. 0x100 - 0:, 0x101 - 1:, etc)
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
    @return number of sectors on disk */
u32t  _std hlp_disksize(u32t disk, u32t *sectsize, disk_geo_data *geo);

/** return 64bit disk size.
    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector, can be 0
    @return number of sectors on disk */
u64t  _std hlp_disksize64(u32t disk, u32t *sectsize);

/** read physical disk.
    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of sectors to read
    @param  data      Buffer for data
    @return number of sectors was actually readed */
u32t _std hlp_diskread(u32t disk, u64t sector, u32t count, void *data);

/** write to physical disk.
    Be careful with QSEL_VIRTUAL flag! 0x100 value mean boot partition, 0x101 -
    virtual disk with QSINIT apps!

    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of sectors to write
    @param  data      Buffer with data
    @return number of sectors was actually written */
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
    In addition, this is disk presence check - pure HDM_QUERY flag for
    HDD arg must return HDM_QUERY bit on in result.

    "CHSONLY" ini key makes the same thing (HDM_USECHS) for boot disk.
    @param  disk      Disk number (HDD only).
    @param  flags     Flags value. Use HDM_QUERY to query current mode,
                      HDM_USECHS or HDM_USELBA to set new mode.
                      HDM_EMULATED can only be returned with HDM_QUERY
                      flag, such disks have no mode change support.
    @return (HDM_QUERY|current access mode) or 0 on error */
u32t _std hlp_diskmode(u32t disk, u32t flags);

/** try to mount a part of disk as FAT/FAT32 partition.
    @param  drive     Drive number: 2..9 only, remounting of 0,1 is not allowed.
    @param  disk      Disk number.
    @param  sector    Start sector of partition
    @param  count     Partition length in sectors
    @return 1 on success. */
u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count);

typedef struct {
   u32t      SerialNum;         ///< Volume serial (from BPB)
   u32t         ClSize;         ///< Sectors per FS allocation unit (cluster)
   u32t    RootDirSize;         ///< Number of root directory entries (FAT12/16)
   u16t     SectorSize;         ///< Bytes per sector
   u16t      FatCopies;         ///< Number of FAT copies
   u32t           Disk;         ///< Disk number
   u64t    StartSector;         ///< Start sector of volume (partition)
   u32t   TotalSectors;         ///< Total sectors on volume
   u32t     DataSector;         ///< First data sector on volume (disk-relative)
   u32t          ClBad;         ///< Bad clusters (not supported, non-zero only after format)
   u32t        ClAvail;         ///< Clusters available
   u32t        ClTotal;         ///< Clusters total
   char      Label[12];         ///< Volume Label
   char      FsName[9];         ///< File system readable name (empty if not recognized)
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
    @return FAT_* constant */
u32t _std hlp_volinfo(u8t drive, disk_volume_data *info);

/** set volume label.
    @param  drive     Drive number: 0..9.
    @param  label     Up to 11 chars of volume label or 0 to clear it.
    @return 1 on success. */
u32t _std hlp_vollabel(u8t drive, const char *label);

/** unmount previously mounted partition.
    @param  drive     Drive number: 2..9 only, unmounting of 0,1 is not allowed.
    @return 1 on success. */
u32t _std hlp_unmountvol(u8t drive);

/** unmount all QSINIT volumes from specified disk.
    Function can not unmount boot partition!
    @param  disk      Disk number
    @return number of unmounted volumes. */
u32t _std hlp_unmountall(u32t disk);

//===================================================================
//  other
//-------------------------------------------------------------------

/** allocate selector(s).
    @param  count     Number of selectors (remember - QSINIT GDT is SMALL :))
    @return base number of 0 */
u16t  _std hlp_selalloc(u32t count);

/** GDT descriptor setup.
    @param  selector  GDT offset (i.e. selector with 0 RPL field)
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
    @attention DS selector in non-paged QSINIT mode can use up-down "stack"
         selector type and limit field in it is reversed in value (see Intel
         manuals).
    @param [in]  Sel     Selector
    @param [out] Limit   Limit, can be zero.
    @return 0xFFFFFFFF if failed. */
u32t  _std hlp_selbase(u32t Sel, u32t *Limit);

/** free selector.
    this function can`t be used to free system selectors (flat cs, ds, etc).
    @return bool - success flag */
u32t  _std hlp_selfree(u32t selector);

/** copy Length bytes from Sel:Offset to FLAT:Destination.
    Function check Sel,Offset & Length, but does not check Destination.
    @param Destination   Destination buffer
    @param Offset        Source offset
    @param Sel           Source selector
    @param Length        length of data to copy
    @return number of bytes was copied */
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
                   if high word (segment) < 10, then this is FLAT addr in
                   first 640k
    @param dwcopy  number of words to copy to real mode stack. RMC_* flags can
                   be ORed here (note, that absence of RMC_EXITCALL in final
                   QSINIT`s exit can cause real troubles for feature code).
    @param ...     arguments. Be careful with 32bit push and 16bit args! ;)

    @return real mode dx:ax and OF, SF, ZF, AF, PF, CF flags */
u32t __cdecl hlp_rmcall(u32t rmfunc, u32t dwcopy, ...);

/// @name for dwcopy parameter of hlp_rmcall() and hlp_rmcallreg()
//@{
#define RMC_IRET      0x40000000  ///< call function with iret frame
#define RMC_EXITCALL  0x80000000  ///< exit QSINIT (special logic: PIC reset and so on)
//@}

// include converted dpmi.inc to get access here
#ifdef QS_BASE_DPMIDATA
/** real mode function call with registers and arguments.
    @param [in]     intnum  call interrupt if intnum = 0..255, use -1 to far
                            call with cs:ip in regs.
    @param [in,out] regs    registers. cs:ip must be filled for far call,
                            ss:sp can be 0 to use DPMI stack.
    @param [in]     dwcopy  number of words to copy to real mode stack,
                            RMC_* flags can be ORed here. RMC_IRET assumed for
                            interrupt call.
    @param [in]     ...     arguments, be careful with 32bit push and 16bit args.

    @return real mode dx:ax and OF, SF, ZF, AF, PF, CF flags, modified real
            mode registers in regs */
u32t __cdecl hlp_rmcallreg(int intnum, rmcallregs_t *regs, u32t dwcopy, ...);
#endif

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

/** query type of boot.
    @return QSBT_* constant */
u32t _std hlp_boottype(void);

/// @name result of hlp_hosttype()
//@{
#define QSHT_BIOS        0    ///< BIOS environment
#define QSHT_EFI         1    ///< 64-bit EFI environment (QSINIT still 32-bit ;))
//@}

/** query host environment.
    @return QSHT_* constant */
u32t _std hlp_hosttype(void);

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
    @param  disk      Disk number (QDSK_*)
    @param  ownmbr    Flag 1 to replace disk MBR code to self-provided
    @retval EINVAL    invalid disk handle
    @retval ENODEV    empty partition table
    @retval EIO       i/o error occured
    @retval ENOTBLK   disk is emulated hdd
    @retval ENOSYS    this is EFI host and BIOS boot unsupported */
int  _std exit_bootmbr(u32t disk, u32t flags);

/** boot from specified partition.
    @param disk       Disk number (QDSK_*)
    @param index      Partition index (see dmgr ops/qsdm.h)
    @param letter     Set drive letter for OS/2 boot (0 to ignore/default)
    @param sector     Alternative boot sector binary data (use 0 here,
                      this data is for booting another/saved boot sector -
                      like NTLDR do).
    @retval EINVAL    on invalid disk handle or index
    @retval ENODEV    on empty or unrecognized data in boot sector,
    @retval EIO       if i/o error occured
    @retval ENOTBLK   if disk is emulated hdd
    @retval EFBIG     boot sector of partition is beyond of 2TB border
    @retval ENOSYS    this is EFI host and BIOS boot not supported
    @retval EFAULT    bad address in "sector" arg */
int  _std exit_bootvbr(u32t disk, u32t index, char letter, void *sector);

/** power off the system (APM only).
    Suspend can work too - on some middle aged motherboards ;)
    @return 0 if failed, 1 if system was really powered off */
u32t _std exit_poweroff(int suspend);

/** reboot the system.
    @param warm       Wark reboot flag (1/0).
    @return if failed */
void _std exit_reboot(int warm);

/** internal call: prepare to leave QSINIT.
    this function call all installed exit callbacks
    @see exit_handler() */
void _std exit_prepare(void);

/** internal call: exit_prepare() was called or executing just now.
    @return 0 - for no, 1 - if called already and 2 if you called from
            exit_handler() callback */
u32t _std exit_inprocess(void);

/// exit QSINIT callback function, called from exit_prepare.
typedef void (*exit_callback)(void);

/** add/remove exit_callback handler.
    @param func  Callback ptr
    @param add   bool - add/remove */
void _std exit_handler(exit_callback func, u32t add);

/** setup PIC reset on QSINIT exit.
    By default QSINIT restores PIC mapping to 08h/70h (BIOS based version).
    This call can be used to skip this (leave PIC mapped to 50h/70h).
    Function works for all exit functions, can be called multiple times
    before real exit.
    @param on    Flag - 1=restore default mapping, 0=leave as it is */
void _std exit_restirq(int on);

#ifdef __cplusplus
}
#elif defined(__WATCOMC__) // exit call to jmp replacement in watcom C only
void _std exit_pm32s(int rc);

#pragma aux exit_pm32s aborts;
#define exit_pm32 exit_pm32s
#endif // __WATCOMC__

#endif // QSINIT_UTIL
