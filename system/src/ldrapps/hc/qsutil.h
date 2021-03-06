//
// QSINIT
// common functions
//
#ifndef QSINIT_UTIL
#define QSINIT_UTIL

#include "qstypes.h"
#include "qslog.h"

#ifdef __cplusplus
extern "C" {
#endif

//===================================================================
//  global memory management
//-------------------------------------------------------------------

/// @name flags for hlp_memalloc
//@{
/** Allocate unresizeable block until the end of work.
    Block cannot be freed. Available part of it (all blocks are rounded
    up to 64k) will be used internally for a small block heap */
#define QSMA_READONLY 0x001
/// return 0 instead of immediate panic on block alloc failure
#define QSMA_RETERR   0x002
/// do not clear block (affects only on first alloc, but not further reallocs)
#define QSMA_NOCLEAR  0x004
/** allocated block will be released after process exit.
    This flag is ignored when used with QSMA_READONLY. */
#define QSMA_LOCAL    0x008
/// align block to the top of memory (maximum possible address)
#define QSMA_MAXADDR  0x010
/// align block to the start of memory (minimum possible address)
#define QSMA_MINADDR  0x020
//@}

/** allocate and zero fill memory.
    This is "global" allocation, memory block is shared over system by
    default and zero filled.
    Any allocation is rounded up to 64k internally.
    
    On BIOS host - when both QSMA_MINADDR/QSMA_MAXADDR are missing, MINADDR
    is used to preserve space at the end of ram, which can be useful for OS/2
    boot. */
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
    This call allocates memory at specified physical address - if this
    range included into memory manager space and not used.

    QSINIT memory manager uses memory starting from 16Mb up to the end
    of RAM in first 4Gb area.
    Using of memory below 16Mb is prohibited (it reserved for OS/2 boot).
    Other memory (not belonging to memory manager) can be reserved by
    sys_markmem() call in qssys.h.

    Function returns aligned down to 64k address of reserved block (block
    is NOT zero filled, unlike hlp_memalloc()). Block can be used as usual
    memory block and released by hlp_memfree().

    @attention block length can be smaller than requested, it must be
    checked by hlp_memgetsize() call. This can occur on reserving memory
    at the end of memory manager space.

    Also note, that existing holes in PC memory address space already marked
    as reserved by memory manager itself (with rounding to 64k, of course).

    @param physaddr       Physical address.
    @param length         Length of memory to reserve.
    @param [out] reason   Error reason (QSMR_* value, ptr can be 0).
    @return block address or 0 if address is used or beyond memory manager
            limits. "reason" parameter will return additional information. */
void* _std hlp_memreserve(u32t physaddr, u32t length, u32t *reason);

/// @name Error reason for hlp_memreserve
//@{
#define QSMR_SUCCESS     0x000    ///< block reserved
#define QSMR_TRUNCATED   0x001    ///< block reserved, but size was cut by the end of memory.
#define QSMR_OUTOFRANGE  0x002    ///< start address is out of QSINIT memory range
#define QSMR_USED        0x003    ///< a part of this block is used!
//@}

/** query available memory.
    @param maxblock  Maximum size of single block, can be 0
    @param total     Total memory size, (can be) used by QSINIT, can be 0.
    @return total available memory in bytes */
u32t  _std hlp_memavail(u32t *maxblock, u32t *total);

/** query used memory size.
    Used addresses are always aligned to 64k (highest then decreased by 1).

    With FFFF arg function allows to query a full range of memory, used for
    QSINIT at the time of call (with exception of initialization module).

    @param pid             Process id for process owned memory (0 for global
                           allocations or FFFF for all existing blocks).
    @param [out] usedlow   Lowest address used (ptr can be 0).
    @param [out] usedhigh  Highest address used (ptr can be 0).
    @return used memory size in bytes (rounded up to 64k because of
            system memory manager alignment). */
u32t  _std hlp_memused (u32t pid, u32t *usedlow, u32t *usedhigh);

//===================================================================
//  boot filesystem (OS/2 micro-FSD) file management
//-------------------------------------------------------------------

/** open file from the boot partition via micro-FSD.
    Only one file in read-only mode can be opened, this is IBM`s micro-FSD
    design limitation.
    In EFI build this function read files from \EFI\BOOT directory of the EFI
    system volume.

    In MT mode - this call captures internal mutex and any other thread will
    be blocked on hlp_fopen(), hlp_fread(), hlp_fclose(), hlp_freadfull() and
    hlp_fexist() until this thread`s hlp_fclose() call.

    Also note, that zip_openfile() from boot volume/TFTP will grab this mutex
    until zip_close().

    During PXE boot this call is unavailable (even more limitations) - and
    files from boot source can be readed via hlp_freadfull() only.

    @param  name  File name (boot disk root dir only), up to 63 chars.
    @return -1 on no file, else file size */
u32t  _std hlp_fopen(const char *name);

/** read file from the boot partition via micro-FSD.
    @param  offset        Offset on file.
    @param  buf           Target buffer
    @param  bufsize       Size to read
    @return actually readed number of bytes */
u32t  _std hlp_fread(u32t offset, void *buf, u32t bufsize);

/// close file
void  _std hlp_fclose();

/** check file presence via micro-FSD.
    Note, that this is just the open/close pair internally, i.e. all of
    micro-FSD i/o limitations applied to this function (including global
    mutex wait).
    Function supports PXE mode as well.

    @param  name  File name (boot disk root only), up to 63 chars.
    @return 0 if file is not exist, 0xFFFFFFFF - if size is zero or unknown
            (always occurs on PXE), else size of the file. */
u32t  _std hlp_fexist(const char *name);

/** callback for hlp_freadfull.
    @param [in]  percent  Current position in percents (always 0 on PXE boot
                          because file size is unknown until the end of read)
    @param [in]  readed   Current position in bytes */
typedef void _std (*read_callback)(u32t percent, u32t readed);

/** open, read, close file & return buffer with it.
    Note, what block allocated with QSMA_LOCAL flag (process owned).

    @param [in]  name     File name
    @param [out] bufsize  Size of file
    @param [in]  cbprint  Callback for process indication, can be 0.
    @return buffer with file or 0. Buffer must be freed via hlp_memfree() */
void* _std hlp_freadfull(const char *name, u32t *bufsize, read_callback cbprint);

//===================================================================
//  low level physical disk access
//-------------------------------------------------------------------

/** return number of installed hard disks in system.
    @attention hard disks enumeration can have holes at the place of
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
    @attention BIOS 16384x63x16 is returned in geo parameter for HDDs.
    This value usable for floppy drives/ancient small HDDs only. Use
    dsk_getptgeo() to get actual MBR partition table geometry.

    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector, can be 0
    @param [out]  geo       Disk data (optional, can be 0, can return zeroed data)
    @return number of sectors on disk. Actually, geo->TotalSectors must be
            checked to be sure. */
u32t  _std hlp_disksize(u32t disk, u32t *sectsize, disk_geo_data *geo);

/** return 64bit disk size.
    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector, can be 0
    @return number of sectors on disk */
u64t  _std hlp_disksize64(u32t disk, u32t *sectsize);

/** read a physical disk.
    @param  disk      Disk number. Note, that QDSK_VOLUME can be added for
                      a mounted volume. In this case function acts as
                      a volume-level sector i/o api.
    @param  sector    Start sector
    @param  count     Number of sectors to read
    @param  data      Buffer for data
    @return number of sectors was actually readed */
u32t _std hlp_diskread(u32t disk, u64t sector, u32t count, void *data);

/** write to a physical disk.
    Be careful with QDSK_VOLUME flag! 0x100 value means boot partition, 0x102 -
    volume C: (if mounted one) and so on!

    @param  disk      Disk number. hlp_diskread() note applied here too.
    @param  sector    Start sector
    @param  count     Number of sectors to write
    @param  data      Buffer with data
    @return number of sectors was actually written */
u32t _std hlp_diskwrite(u32t disk, u64t sector, u32t count, void *data);

/** is floppy media changed?
    @param  disk      Disk number.
    @return -1 if no disk, not a floppy or no changeline on it, 0 - if was not
            changed, 1 - is changed since last call */
int  _std hlp_fddline(u32t disk);

/** translate BIOS disk number to QSINIT disk number and vise versa.
    Works on BIOS host only!
    @param  disk      Source disk number.
    @param  qs2bios   Direction flag (zero for BIOS->QS, non-zero - QS->BIOS)
    @return FFFF on error or translated disk number */
u32t _std hlp_diskbios(u32t disk, int qs2bios);

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

/// @name hlp_disktype() result (disk types)
//@{
#define HDT_INVALID     0       ///< Invalid disk number
#define HDT_REGULAR     1       ///< Regular disk device (HDD or flash drive)
#define HDT_FLOPPY      2       ///< Floppy disk
#define HDT_CDROM       3       ///< CD-ROM (rare case when it visible in BIOS)
#define HDT_RAMDISK     4       ///< Ram disk
#define HDT_EMULATED    5       ///< Other emulated device (VHDD)
//@}

/** query disk type.
    Note, that for volume handles (QDSK_VOLUME) function returns type of
    disk, where volume is placed!
    @param  disk      Disk number.
    @return HDT_* value */
u32t _std hlp_disktype(u32t disk);

/** try to mount a part of disk as a known filesystem (any FAT type or HPFS/JFS).
    This is low level mount function, use vol_mount() for partition based
    mounting.

    Note, that now mounting of the partition with 64-bit length in sectors
    is not supported in QSINIT. I.e. max mountable partition is 2Tb for 512
    bytes sector size.

    @param  drive     Drive number: 2..9 only, remounting of 0,1 is not allowed.
    @param  disk      Disk number.
    @param  sector    Start sector of partition
    @param  count     Partition length in sectors
    @return 1 on success. */
u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count);

typedef struct {
   u32t      SerialNum;         ///< Volume serial
   u32t         ClSize;         ///< Sectors per FS allocation unit (cluster)
   u16t     SectorSize;         ///< Bytes per sector (virtual fs should emulate this)
   u16t      InfoFlags;         ///< Volume inormation flags (VIF_* below)

   u32t           Disk;         ///< Disk number (invalid if VIF_VFS)
   u64t    StartSector;         ///< Start sector of volume (not valid for VIF_VFS)
   u32t   TotalSectors;         ///< Total sectors on volume (no support for >2Tb volumes now)
   u32t       Reserved;         ///< Just zero
   u32t     DataSector;         ///< First data sector on volume (volume-relative, not valid for VIF_VFS)

   u32t          ClBad;         ///< Bad clusters (not supported, non-zero only after format)
   u32t        ClAvail;         ///< Clusters available
   u32t        ClTotal;         ///< Clusters total
   u64t       FSizeLim;         ///< Upper file size limit on volume filesytem
   char      Label[12];         ///< Volume Label
   char      FsName[9];         ///< File system readable name (empty if not recognized)
   u8t           FsVer;         ///< File system version (0 - not recognized)
   u16t        OemData;         ///< File system defined information
} disk_volume_data;

/// @name hlp_volinfo() result
//@{
#define FST_NOTMOUNTED  0       ///< Volume is not mounted
#define FST_FAT12       1       ///< FAT12 volume
#define FST_FAT16       2       ///< FAT16 volume
#define FST_FAT32       3       ///< FAT32 volume
#define FST_EXFAT       4       ///< ExFAT volume
#define FST_OTHER       5       ///< other filesystem
//@}

/// @name hlp_volinfo() InfoFlags bits
//@{
#define VIF_READONLY    0x0001  ///< read-only filesystem mounted
#define VIF_VFS         0x0002  ///< virtual fs (no disk data)
#define VIF_NOWRITE     0x0004  ///< read-only volume (disk i/o)
//@}

/** mounted volume info.
    Actually function is bad designed, it returns FST_NOTMOUNTED both
    for non-mounted volume and mounted unrecognized volume. To check volume
    presence info.TotalSectors can be used, it will always be 0 if volume
    is not mounted.
    Also, io_volinfo() available in i/o API, with the same functionality.

    For VIF_VFS file system Disk, StartSector and DataSector fields are invalid
    and SectorSize, ClSize, ClAvail and ClTotal only emulates "block" allocation.

    Note, that VIF_READONLY - is a read-only filesystem implementation, volume
    i/o on sector level still supports write access. But VIF_NOWRITE means
    r/o disk i/o in any case, with any possible filesystem and without it.

    VIF_READONLY is always set for HPFS and JFS volumes, VIF_NOWRITE is a
    result of "mount /ro" or other special cases (UEFI boot volume).

    @param  drive     Drive number
    @param  info      Buffer for data to fill in, can be 0.
    @return FST_* constant */
u32t _std hlp_volinfo(u8t drive, disk_volume_data *info);

/** unmount previously mounted partition.
    @param  drive     Drive number: 2..9 only, unmounting of 0,1 is not allowed.
    @return 1 on success. */
u32t _std hlp_unmountvol(u8t drive);

/** unmount all QSINIT volumes from the specified disk.
    Function does not unmount boot partition!
    @param  disk      Disk number
    @return number of unmounted volumes. */
u32t _std hlp_unmountall(u32t disk);

//===================================================================
//  date/time
//-------------------------------------------------------------------

/// return timer counter value (18.2 times per sec, for randomize, etc)
u32t _std tm_counter(void);

/// return time/date in dos format. flag CF is set for 1 second overflow
u32t _std tm_getdate(void);

/// set current time (with 2 seconds granularity)
void _std tm_setdate(u32t dostime);

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
    Function checks Sel, Offset, Length and page access for the source, but
    does not check the Destination.

    @param Destination   Destination buffer
    @param Offset        Source offset
    @param Sel           Source selector
    @param Length        length of data to copy
    @return number of bytes was copied */
u32t  _std hlp_copytoflat(void* Destination, u32t Offset, u32t Sel, u32t Length);

/** convert real mode segment to flat address.
    QSINIT FLAT space is zero-based since version 0.12 (rev 281), so this
    function is void now and may be removed.

    BUT! To use memory above the end of RAM in first 4GBs (call sys_endofram()
    to query this border) - pag_physmap() MUST be used. This is the only way
    to preserve access to this memory when QSINIT will be switched into PAE
    paging mode (can occur at any time by single API call).

    @param Segment       Real mode segment
    @return flat address */
u32t  _std hlp_segtoflat(u16t Segment);

/// rm 16:16 to qsinit flat addr conversion (for lvalue only)
#define hlp_rmtopm(x) (hlp_segtoflat((u32t)(x)>>16) + (u16t)(x))

/** real mode function call with arguments.
    @param rmfunc  16 bit far pointer / FLAT addr,
                   if high word (segment) < 10, then this is FLAT addr in
                   first 640k. Use hlp_rmcallreg() to skip this crazy logic.
    @param dwcopy  number of words to copy to the real mode stack. RMC_* can
                   be ORed here (note, that absence of RMC_EXITCALL in final
                   QSINIT`s exit will cause REAL troubles for feature code).
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
    @param [in]     dwcopy  number of words to copy to the real mode stack,
                            RMC_* flags can be ORed here. RMC_IRET assumed for
                            the interrupt call.
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
    @return bool - success flag (it is 486 CPU if no cpuid) */
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
#define QSBT_NONE        0    ///< no boot source (not used now)
#define QSBT_FAT         1    ///< FAT boot (0:/A: drive available)
#define QSBT_FSD         2    ///< boot partition use micro-FSD i/o
#define QSBT_PXE         3    ///< PXE boot, only hlp_freadfull() can be used
#define QSBT_SINGLE      4    ///< FAT/FAT32/exFAT boot without OS/2 (no OS2BOOT in the root)
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
#define QBIO_KEYREADNW   2    ///< keyboard poll (status<<16|code)
#define QBIO_EQLIST      3    ///< BIOS equipment list (int 11h, return ax)
//@}

u32t _std hlp_querybios(u32t index);

/** query safe mode state.
    Safe mode invoked by left shift press on QSINIT loading.
    @return bool - yes/no. */
u32t _std hlp_insafemode(void);


/// @name exit_pm32 error codes
//@{
#define QERR_NO486       1
#define QERR_SOFTFAULT   4
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
    This is complete exit from QSINIT to the real mode in BIOS host and
    back to the UEFI (on EFI host).

    @param rc  Error code for text message */
void _std exit_pm32(int rc);

/** exit and restart another OS/2 loader.
    This is "RESTART" shell command API analogue.

    Note, that QSINIT starting from rev 500 is able to inherit shell history
    and custom environment from the current loader.
    Starting from rev 511 active codepage is also inherited.

    @param loader  OS2LDR file name
    @param env     Optional environment for the main loader process (affects
                   only QSINIT starting from rev 500), can be 0.
    @param nosend  Do not send shell history and codepage number to the next
                   loader (1/0)
    @return error code. */
qserr _std exit_restart(char *loader, void *env, int nosend);

/// @name exit_bootmbr flags
//@{
#define EMBR_OWNCODE     1    ///< use own MBR code instead of disk one
#define EMBR_NOACTIVE    2    ///< ignore active partition absence
//@}

/** boot from specified HDD.
    exit_bootvbr() should be used to boot from floppy.
    @param  disk      Disk number (QDSK_*)
    @param  flags     Options (EMBR_*)
    @retval EINVAL    invalid disk handle
    @retval ENODEV    empty partition table
    @retval EIO       i/o error occured
    @retval ENOTBLK   disk is emulated hdd
    @retval ENOSYS    this is EFI host and BIOS boot unsupported */
int  _std exit_bootmbr(u32t disk, u32t flags);

/** boot from specified partition (volume).
    @param disk       Disk number (QDSK_*), floppy accepted too.
    @param index      Partition index (see dmgr ops/qsdm.h), -1 for floppy
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
int  _std exit_bootvbr(u32t disk, long index, char letter, void *sector);

/** power off the system (APM only).
    Suspend can work too - on some middle aged motherboards ;)
    @return 0 if failed, 1 if system was really powered off */
u32t _std exit_poweroff(int suspend);

/** reboot the system.
    @param warm       Warm reboot flag (1/0).
    @return if failed */
void _std exit_reboot(int warm);

/** prepare to leave QSINIT.
    This function calls all installed exit callbacks (see sys_notifyevent()).
    This function stops MT mode (actually, set MT lock forewer).
    This function terminates both micro-FSD and virtual disk management!

    Actually, this must the last call before custom QSINIT exit, for ex.:
    @code
       memset(&regs, 0, sizeof(regs));
       regs.r_ip  = MBR_LOAD_ADDR;
       regs.r_edx = disk + 0x80;
       exit_prepare();
       memcpy((char*)hlp_segtoflat(0)+MBR_LOAD_ADDR, mbr, 512);
       hlp_rmcallreg(-1, &regs, RMC_EXITCALL);
    @endcode
    Basic runtime still available after it, and can be called, but most
    of services like cache, console, virtual disks and so on - terminated
    already and using it will cause unpredicted results.

    Any exit API functions make this call internally, i.e. it must be
    called only in custom exit ways, like direct jump to the real mode code
    (example above).

    @see exit_handler() */
void _std exit_prepare(void);

/** internal call: exit_prepare() was called or executing just now.
    @return 0 - if not, 1 - if called already, 2 - call in progress) */
u32t _std exit_inprocess(void);

/** setup PIC reset on QSINIT exit.
    By default QSINIT restores PIC mapping to 08h/70h (BIOS based version).
    This call can be used to skip this (leave PIC mapped to 50h/70h).
    Function works for all exit functions, can be called multiple times
    before real exit.
    Affects BIOS host version only, of course.
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
