//
// QSINIT API
// disk management
//
#ifndef QSINIT_DMFUNC
#define QSINIT_DMFUNC

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "qsutil.h"

/// @name dsk_newmbr() flags
//@{
#define DSKBR_CLEARPT   0x0001    ///< clear partition table entries (!!!)
#define DSKBR_GENDISKID 0x0002    ///< generate random "disk id"
#define DSKBR_CLEARALL  0x0004    ///< write zero-filled sector without 55AA
#define DSKBR_LVMINFO   0x0008    ///< wipe all LVM info sectors
#define DSKBR_GPTHEAD   0x0010    ///< write GPT stub record (clear MBR disk!!!)
#define DSKBR_GPTCODE   0x0020    ///< write MBR code for GPT BIOS boot
//@}

/** reset disk mbr.
    @attention DSKBR_CLEARPT / DSKBR_GPTHEAD flags WIPE entire disk!
    Function it low-level call is do not call disk rescan after changes.
    @param disk     disk number
    @param flags    DSKBR_* flags
    @return boolean (success flag) */
int   _std dsk_newmbr(u32t disk, u32t flags);


/// @name dsk_sectortype() result
//@{
#define DSKST_ERROR     0x0000    ///< no sector or i/o error
#define DSKST_EMPTY     0x0001    ///< sector is empty
#define DSKST_PTABLE    0x0002    ///< sector is partition table
#define DSKST_BOOTFAT   0x0003    ///< sector is FAT12/16/32 boot sector
#define DSKST_BOOTBPB   0x0004    ///< boot sector with BPB
#define DSKST_BOOT      0x0005    ///< boot sector without BPB
#define DSKST_DATA      0x0006    ///< sector not recognized (regular data)
#define DSKST_GPTHEAD   0x0007    ///< GPT header
#define DSKST_BOOTEXF   0x0008    ///< sector is exFAT boot sector
//@}

/** query sector type.
    @param [in]  disk     disk number
    @param [in]  sector   sector number
    @param [out] optbuf   buffer for sector, 4kb size (can be 0)
    @return DSKST_* value and readed data in optbuf if specified */
u32t  _std dsk_sectortype(u32t disk, u64t sector, u8t *optbuf);

/** query sector size on disk.
    Function accepts volumes too (i.e. vol|QDSK_VOLUME disk value).
    @param disk     disk number
    @return 0 or sector size */
u32t  _std dsk_sectorsize(u32t disk);

/** query sector type in memory buffer.
    @param sectordata     sector binary data
    @param sectorsize     sector size
    @param disk           disk number (help to analyze, use 0 for common hdd)
    @param is0            is sector 1st on disk? (help to analyze)
    @return DSKST_* value */
u32t  _std dsk_datatype(u8t *sectordata, u32t sectorsize, u32t disk, int is0);

/** query filesystem name from boot sector.
    @param disk           disk number
    @param sector         sector
    @param [out] filesys  buffer for file system type (at least 9 chars)
    @param [out] optbuf   buffer for sector, 4kb size (can be 0)
    @return the same as dsk_sectortype() and non-empty filesys if detected
            one. */
u32t  _std dsk_ptqueryfs(u32t disk, u64t sector, char *filesys, u8t *optbuf);

/// @name dsk_newvbr() sector type
//@{
#define DSKBS_AUTO      0x0000    ///< autodetect boot sector type
#define DSKBS_FAT16     0x0001    ///< FAT16 boot sector
#define DSKBS_FAT32     0x0002    ///< FAT32 boot sector
#define DSKBS_HPFS      0x0003    ///< HPFS boot record (without OS2BOOT!)
#define DSKBS_DEBUG     0x0004    ///< debug sector (same as dsk_debugvbr())
#define DSKBS_EXFAT     0x0005    ///< exFAT boot record
//@}

/** replace boot sector code by QSINIT`s one.
    @attention function does not perform any checks on location!!!!
    Function failed in case of file system mismatch between sector data and
    "type" value.

    @param disk     disk number
    @param sector   sector number
    @param type     sector type (DSKBS_* value).
    @param name     custom boot file name (11 chars on FAT, 15 on HPFS/exFAT), can be 0.
    @return 0 if success, else DFME_* error code */
u32t  _std dsk_newvbr(u32t disk, u64t sector, u32t type, const char *name);

/** write debug boot record code.
    This code does not load anything, but print DL, BPB.PhysDisk and i13x
    presence. Code can be written to any type of FAT boot sector and to
    MBR (function preserve partition table space in sector).

    Function fails if it discover exFAT boot record in this sector (it is
    incompatible with current debug code).

    @param disk     disk number
    @param sector   sector number
    @return boolean (success flag) */
int   _std dsk_debugvbr(u32t disk, u64t sector);

/** delete 55AA signature from two last bytes of sector.
    Any other data remain unchanged.
    @param disk     disk number
    @param sector   sector number
    @return boolean (success flag) */
int   _std dsk_wipe55aa(u32t disk, u64t sector);

/** write empty sectors (zero-fill).
    @attention function deny sector 0 as argument! Use dsk_newmbr() instead.
    Function accepts volumes too (i.e. vol|QDSK_VOLUME disk value).

    @param disk     disk number
    @param sector   first sector
    @param count    number of sectors to clear
    @return 0 on success or DPTE_* error code */
u32t  _std dsk_emptysector(u32t disk, u64t sector, u32t count);

/// break copying confirmation callback
typedef int _std (*break_callback)(void *info);

/** copy sectors.
    Function accepts volumes too (i.e. vol|QDSK_VOLUME disk value).

    @param [in]  dstdisk   destination disk
    @param [in]  dststart  start sector on destination disk
    @param [in]  srcdisk   source disk
    @param [in]  srcstart  start sector on source disk
    @param [in]  count     number of sectors to copy
    @param [in]  cbprint   process indication callback (can be 0)
    @param [in]  ask       ask confirmation to break copy process, if it non-0,
                           then this function will be called on ESC key press.
                           If function returns 1, dsk_copysector() will stop
                           copying and set DPTE_UBREAK error code.
    @param [in]  askptr    info parameter for ask callback, can be 0
    @param [out] error     ptr to error code (can be 0)
    @return number of actually copied sectors. */
u32t  _std dsk_copysector(u32t dstdisk, u64t dststart,
                          u32t srcdisk, u64t srcstart,
                          u32t count, read_callback cbprint,
                          break_callback ask, void *askptr, u32t *error);

/// @name dsk_ptrescan(), dsk_setactive(), dsk_ptcreate(), etc result
//@{
#define DPTE_ERRREAD  0x0001     ///< error reading disk, pt can be used r/o
#define DPTE_FLOPPY   0x0002     ///< big floppy, pt is empty
#define DPTE_EMPTY    0x0003     ///< disk is not partitioned
#define DPTE_INVALID  0x0004     ///< invalid table, valid part must be used r/o
#define DPTE_GPTDISK  0x0005     ///< disk have GPT partition table
#define DPTE_HYBRID   0x0006     ///< disk have hybrid partition table (GPT+MBR)
#define DPTE_MBRDISK  0x0007     ///< disk have MBR partition table
#define DPTE_PINDEX   0x0010     ///< unable to find partition index
#define DPTE_PRIMARY  0x0011     ///< partition is not primary
#define DPTE_EXTENDED 0x0012     ///< partition is extended and cannot be active
#define DPTE_ERRWRITE 0x0013     ///< error writing disk
#define DPTE_RESCAN   0x0014     ///< rescan requred (start sector is not matched to pt index)
#define DPTE_NOFREE   0x0020     ///< there is no free space on this position
#define DPTE_SMALL    0x0021     ///< partition too small to create
#define DPTE_EXTERR   0x0022     ///< extended partition processing error
#define DPTE_NOPRFREE 0x0023     ///< no free space in primary table
#define DPTE_FINDEX   0x0024     ///< invalid free space index
#define DPTE_FSMALL   0x0025     ///< free space is smaller than specified
#define DPTE_LARGE    0x0026     ///< partition too large (64-bit number of sectors)
#define DPTE_MOUNTED  0x0030     ///< partition already mounted.
#define DPTE_NOLETTER 0x0031     ///< there is no free drive letter.
#define DPTE_GPTHDR   0x0040     ///< GPT header is broken
#define DPTE_2TBERR   0x0041     ///< Unable to access disk data above 2TB
#define DPTE_GPTLARGE 0x0042     ///< GPT header too large to process
#define DPTE_GPTHDR2  0x0043     ///< second GPT header is broken
#define DPTE_EXTPOP   0x0044     ///< extended partition is not empty and cannot be deleted
#define DPTE_INCOMPAT 0x0045     ///< incompatible disks (dsk_clonestruct() - sector size or spt mismatch)
#define DPTE_CSPACE   0x0046     ///< there is no free space on target disk for clone
#define DPTE_BOOTPT   0x0047     ///< unable to use boot partition as target
#define DPTE_UMOUNT   0x0048     ///< failed to unmount destination volume
#define DPTE_UBREAK   0xFFFC     ///< esc was pressed, operation not complete
#define DPTE_NOMEMORY 0xFFFD     ///< no memory to process this operation
#define DPTE_INVARGS  0xFFFE     ///< invalid arguments
#define DPTE_INVDISK  0xFFFF     ///< bad disk number specified
//@}

/** rescan partition table.
    @param disk     disk number
    @param force    set 1 to force rescan, 0 - to rescan on first use or
                    floppy drive only
    @return 0 or DPTE_ERRREAD, DPTE_FLOPPY, DPTE_EMPTY, DPTE_INVALID, DPTE_INVDISK.
       Disk still can be used on errors, but until first broken entry */
u32t  _std dsk_ptrescan(u32t disk, int force);

/** query number of indexed partitions on disk.
    @param disk     disk number
    @return total number of partitions ("index" parameter for other functions),
       0 if no viewable partitions (something like empty extended still can
       be present), -1 - in case of error, use dsk_ptrescan() to examine
       error code */
long  _std dsk_partcnt(u32t disk);

/// @name dsk_ptquery() flags
//@{
#define DPTF_PRIMARY  0x0001     ///< primary partition
#define DPTF_ACTIVE   0x0002     ///< partition have active bit set
//@}

/** query MBR partition.
    This function does not scan disk, so call to dsk_ptrescan(disk,0) first,
    to make sure, what disk info is actual.

    @attention function returns only MBR and hybrid partitions, but fails on
               GPT, use dsk_ptquery64() instead.

    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [out] start    start sector of the partition (can be 0)
    @param [out] size     partition length in sectors (can be 0)
    @param [out] filesys  readable filesystem description, 16 bytes (can be 0)
    @param [out] flags    partition type flags (DPTF_*, can be 0)
    @param [out] ptofs    offset from own partition table (primary or
                          extended partition slice). This is value for
                          BPB "hidden sectors" field (can be 0, return -1
                          on error).
    @return partition type byte from partition table */
u8t   _std dsk_ptquery(u32t disk, u32t index,
                       u32t *start, u32t *size, char *filesys, u32t *flags,
                       u32t *ptofs);

/** query MBR/GPT partition.
    This function run without disk scanning, so call to dsk_ptrescan(disk,0)
    first, to make sure, what disk info is actual.

    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [out] start    start sector of the partition (64-bit, can be 0)
    @param [out] size     partition length in sectors (64-bit, can be 0)
    @param [out] filesys  readable filesystem description, 16 bytes (can be 0)
    @param [out] flags    partition type flags (DPTF_*, can be 0)
    @return partition type byte from partition table. For all GPT partitions
            it will be PTE_EE_UEFI. */
u8t   _std dsk_ptquery64(u32t disk, u32t index,
                         u64t *start, u64t *size, char *filesys, u32t *flags);

/** query used disk space borders.
    @param [in]  disk     disk number
    @param [out] first    first used sector on disk (by existing partition!)
    @param [out] last     last used sector on disk. Note, that extended partition
                          can have a lot of free space behind this sector, but
                          only existing partitions counted here.
    @return 0 on success, DPTE_EMPTY if disk is empty, else disk scan error */
u32t  _std dsk_usedspace(u32t disk, u64t *first, u64t *last);

/** set active partition (MBR disks).
    Function deny all types of extended partition.
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @return 0 on success or DPTE_* constant. */
u32t  _std dsk_setactive(u32t disk, u32t index);

/** set partition type byte.
    Can return DPTE_EXTENDED in case of extended partition processing error.
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [in]  type     new partition type
    @return 0 on success or DPTE_* constant. */
u32t  _std dsk_setpttype(u32t disk, u32t index, u8t type);

/** is partition mounted?
    @param disk           disk number
    @param index          partition index, can be -1 for entire disk mount
    @return 0 if not mounted or error occured, else disk number (i.e.
            QDSK_VOLUME|volume). */
u32t  _std dsk_ismounted(u32t disk, long index);

/** query mounted volume partition index.
    @param [in]  vol      volume (0..9)
    @param [out] disk     disk number (can be 0)
    @return partition index or -1 */
long  _std vol_index(u8t vol, u32t *disk);

/** mount volume from partition.
    Unlike hlp_mountvol this function get partition number as parameter.
    @param [in,out] vol      in: wanted volume (2..9) or 0 to auto, out:
                             really mounted (or already existed) drive letter
    @param [in]     disk     disk number
    @param [in]     index    partition index
    @return 0 on success or DPTE_* constant. Vol value filled with current
            drive letter in case of DPTE_MOUNTED error code */
u32t  _std vol_mount(u8t *vol, u32t disk, u32t index);

/// @name vol_format() result
//@{
#define DFME_NOMOUNT  0x0001     ///< volume is not mounted
#define DFME_VINDEX   0x0002     ///< unable to find partition index
#define DFME_SSIZE    0x0003     ///< unsupported sector size (>4kb)
#define DFME_SMALL    0x0004     ///< volume too small to fit filesystem structures
#define DFME_FTYPE    0x0005     ///< failed to select FAT type (16 or 32)
#define DFME_UMOUNT   0x0006     ///< failed to unmount destination volume
#define DFME_MOUNT    0x0007     ///< failed to mount volume after format
#define DFME_IOERR    0x0008     ///< disk i/o error in system areas
#define DFME_PTERR    0x0009     ///< disk partition table is invalid
#define DFME_INTERNAL 0x000A     ///< internal error
#define DFME_EXTERR   0x000B     ///< extended partition processing error
#define DFME_UBREAK   0x000C     ///< esc was pressed, format not complete
#define DFME_NOMEM    0x000D     ///< no free memory to process format
#define DFME_LARGE    0x000E     ///< volume too large for selected filesystem
#define DFME_UNKFS    0x000F     ///< unknown file system type
#define DFME_FSMATCH  0x0010     ///< file system type mismatch (dsk_newvbr())
#define DFME_CSIZE    0x0011     ///< wrong cluster size parameter
#define DFME_CPLIB    0x0012     ///< missing codepage support
#define DFME_CNAMELEN 0x0013     ///< custom boot file name length too long
//@}

/// @name vol_format() flags
//@{
#define DFMT_ONEFAT   0x0001     ///< one copy of FAT instead of two
#define DFMT_QUICK    0x0002     ///< do not test volume for bad sectors
#define DFMT_WIPE     0x0004     ///< wipe all disk with zeroes
#define DFMT_NOALIGN  0x0008     ///< do not align FAT tables and data to 4k
#define DFMT_NOPTYPE  0x0010     ///< do not change volume type in partition table
#define DFMT_BREAK    0x0020     ///< allow break by ESC with confirmation dialog
#define DFMT_FORCE    0x0040     ///< force unmount before format, if open files present
//@}

/** format volume with FAT12/16/32.
    @param vol       volume (0..9)
    @param flags     format flags (DFMT_*)
    @param unitsize  cluster size, use 0 for auto selection
    @param cbprint   process indication callback (can be 0, not used if
                     no DFMT_WIPE flag or DFMT_QUICK flag is set)
    @return 0 if success, else DFME_* error code */
u32t  _std vol_format(u8t vol, u32t flags, u32t unitsize, read_callback cbprint);

/** format file system.
    @param vol       volume (0..9)
    @param fsname    fs name (FAT, HPFS or EXFAT now)
    @param flags     format flags (DFMT_*)
    @param unitsize  cluster size for FAT/exFAT, use 0 for auto selection
    @param cbprint   process indication callback (can be 0, not used if
                     no DFMT_WIPE flag or DFMT_QUICK flag is set)
    @return 0 if success, else DFME_* error code */
u32t  _std vol_formatfs(u8t vol, char *fsname, u32t flags, u32t unitsize,
                        read_callback cbprint);

/** is volume marked as dirty?
    Function checks/sets DIRTY state, but only for known filesystems (FAT/HPFS).
    @param vol       volume (0..9)
    @param on        new state (0/1) or -1 for returning current state only
    @return previous state (0/1) or negative value on error (DFME_*) */
int   _std vol_dirty(u8t vol, int on);

/** query disk text name.
    @param disk     disk number
    @param buffer   buffer for result, can be 0 for static (unique per
                    thread). At least 8 bytes.
    @return 0 on error, buffer or static buffer ptr */
char* _std dsk_disktostr(u32t disk, char *buffer);

/** convert disk text notation to disk number.
    Accepted strings are: hd0, fd0, hdd0, fdd0, h0, f0, a:, 0:
    @param str      buffer with disk name (with NO spaces before)
    @return disk number or FFFF (0xFFFFFFFF) if no such disk in system */
u32t  _std dsk_strtodisk(const char *str);

/** format disk size to short string.
    Returned string cannot be longer than 8 chars (it have 5 digits max:
    "99999 kb", but "100 mb").
    @param sectsize   disk sector size
    @param disksize   number of sectors
    @param width      min width of output string (left-padded with spaces), use 0 to ignore.
    @param buf        buffer for result, can be 0 for static (unique per thread). At least 9 bytes.
    @return buf or static buffer pointer with result string */
char* _std dsk_formatsize(u32t sectsize, u64t disksize, int width, char *buf);

/** return existing partition table geometry.
    This function return "virtual" CHS from partition table. If OS/2 LVM
    DLAT present - values from it will be returned (geo->SectOnTrack can
    be 127 or 255 in this case and all CHS values in PT are broken).
    Cannot be used to determine empty disk geo.
    @param [in]   disk      Disk number.
    @param [out]  geo       Disk geometry data, can return zeroed data on
                            error.
    @result 0 on success or DPTE_* constant. */
u32t  _std dsk_getptgeo(u32t disk, disk_geo_data *geo);

/** init empty hard disk (write MBR partition table signature).
    @param disk     Disk number.
    @param lvmmode  Set to 1 for OS/2 LVM support on.
                    This key also makes a format for huge disks (>500Gb)
                    mostly incompatible with other systems. Such HDD can
                    be used only as non-bootable secondary disks.
                    Set to 2 the same, but it will force "separate" parameter
                    of lvm_initdisk() function to 1.
                    Note, if this arg > 2 - DPTE_INVARGS will be returned.
    @return 0 on success, or DPTE_* error. DPTE_EMPTY is returned if disk is
            NOT empty here. */
u32t  _std dsk_ptinit(u32t disk, int lvmmode);

/// disk free block info
typedef struct {
   u32t            Disk;        ///< Disk number
   u32t      BlockIndex;        ///< Free block index
   u64t     StartSector;        ///< Start sector
   u64t          Length;        ///< Length in sectors
   u32t           Flags;        ///< Block flags
   u16t           Heads;        ///< CHS: assumed number of heads in disk geometry
   u16t    SectPerTrack;        ///< CHS: assumed sectors per track (up to 255 with LVM)
} dsk_freeblock;

/// @name flags for dsk_freeblock.Flags
//@{
#define DFBA_PRIMARY  0x0001     ///< primary partition can be created
#define DFBA_LOGICAL  0x0002     ///< logical partition can be created
#define DFBA_CHSP     0x0004     ///< start pos is good CHS aligned for primary partition
#define DFBA_CHSL     0x0008     ///< start pos is good CHS aligned for logical partition
#define DFBA_CHSEND   0x0010     ///< end of block is good CHS aligned
#define DFBA_AF       0x0020     ///< start of block is AF aligned
//@}
/** do not rescan disk before partition creation.
    for internal dsk_ptcreate() use basically */
#define DFBA_NRESCAN  0x8000

/** query list of free space on disk.
    Function returns 0 in case of partition table error. To query error code
    use dsk_ptrescan(), for example.
    @param disk     Disk number.
    @param info     Buffer for data, can be 0.
    @param size     Number of entries in info, can be 0.
    @return number of required entries. */
u32t  _std dsk_ptgetfree(u32t disk, dsk_freeblock *info, u32t size);

/** create MBR partition.
    LVM info will be updated if it exists on disk and new partition is CHS
    aligned both on start and end positions.

    Common problem here is a difference between MBR tables and free space list.
    We`re calculating free space not by records, but by "free space" between
    existing partitions, so can get a some king of strange effects when extended
    partition slices written in a wrong way.

    Created partition can be shifted from start location and size.

    Use dsk_gptcreate() for GPT disks, this function will return DPTE_GPTDISK.

    @param disk     Disk number.
    @param start    Start sector of new partition
    @param size     Size of new partition in sectors, can be 0 for full free
                    block size.
    @param flags    Flags for creation (only one of DFBA_PRIMARY/DFBA_LOGICAL
                    must be specified).
    @param type     Partition type byte. Error will be returned on creation
                    logical partition with "extended" type.
    @return 0 on success, or DPTE_* error. */
u32t  _std dsk_ptcreate(u32t disk, u32t start, u32t size, u32t flags, u8t type);

/** delete partition.
    Function return DPTE_RESCAN in case of mismatch pt index to start sector of
    it.
    @param disk     Disk number.
    @param index    Partition index
    @param start    Start sector of partition (for additional check)
    @return 0 on success, or DPTE_* error. */
u32t  _std dsk_ptdel(u32t disk, u32t index, u64t start);

/** merge neighboring free blocks in extended partition.
    By default, this action is not required, all FDISKs must do it self.
    @param disk     Disk number.
    @return 0 on success, or DPTE_* error. */
u32t  _std dsk_extmerge(u32t disk);

/** delete empty extended partition.
    By default, this action is not required, all FDISKs must do it self.
    @param disk     Disk number.
    @return 0 on success, or DPTE_* error (DPTE_EXTPOP if extended partition
            is not empty). */
u32t  _std dsk_extdelete(u32t disk);

/// @name flags for dsk_ptalign()
//@{
#define DPAL_PERCENT   0x0001   ///< size is in percents, not megabytes.
#define DPAL_CHSSTART  0x0002   ///< align start to cylinder
#define DPAL_CHSEND    0x0004   ///< align end to cylinder
#define DPAL_AF        0x0008   ///< align start to 4k (advanced format)
#define DPAL_ESPC      0x0010   ///< allocate at the end of free space
//@}

/** Convert free space to aligned start sector/size for new partition creation.

    @param [in]   disk      Disk number.
    @param [in]   freeidx   Free space index (from dsk_ptgetfree()).
    @param [in]   ptsize    Partition size in megabytes or percents, use 0
                            for full free space size.
    @param [in]   altype    Flags (DPAL_).
    @param [out]  start     Calculated start sector.
    @param [out]  size      Calculated partition size, sectors.
    @result 0 on success or DPTE_* constant. */
u32t  _std dsk_ptalign(u32t disk, u32t freeidx, u32t ptsize, u32t altype,
                       u64t *start, u64t *size);

/// is partition byte mean "extended" partition?
#define IS_EXTENDED(pt) ((pt)==PTE_05_EXTENDED || (pt)==PTE_0F_EXTENDED ||    \
   (pt)==PTE_15_EXTENDED || (pt)==PTE_1F_EXTENDED || (pt)==PTE_85_EXTENDED || \
   (pt)==PTE_91_EXTENDED || (pt)==PTE_9B_EXTENDED)

/// is partition byte mean "GPT" partition?
#define IS_GPTPART(pt) ((pt)==PTE_EE_UEFI || (pt)==PTE_EF_UEFI)

/// @name flags for dsk_mapblock.Flags
//@{
#define DMAP_PRIMARY   0x0001   ///< Primary partition / possibility to create it
#define DMAP_ACTIVE    0x0002   ///< Active partition
#define DMAP_LEND      0x0004   ///< Last entry in list
#define DMAP_LOGICAL   0x0008   ///< Logical partition / possibility to create it
#define DMAP_MOUNTED   0x0010   ///< Partition mounted in QSINIT (DriveQS field is valid)
#define DMAP_LVMDRIVE  0x0020   ///< LVM drive letter is available
//@}

typedef struct {
   u64t     StartSector;        ///< Start sector
   u64t          Length;        ///< Length in sectors
   u32t           Index;        ///< Partition or free space index (depends on Type)
   u8t             Type;        ///< Partition type (0 for free space)
   u8t            Flags;        ///< Flags: DMAP_*, both for used and free
   char         DriveQS;        ///< QSINIT drive letter (currently mounted, '0'..'9')
   char        DriveLVM;        ///< LVM drive letter ('A'..'Z')
} dsk_mapblock;

/** query sorted disk map.
    All GPT partitions marked as DMAP_PRIMARY, partitions with "BIOS Bootable"
    attribute ON - as DMAP_ACTIVE (this can occur multiple times(!)).

    @param disk     Disk number.
    @return 0 on invalid disk number/disk scan error or an array of
           dsk_mapblock (must be free()-ed), sorted by StartSector,
           with DMAP_LEND in Flags field of last entry */
dsk_mapblock* _std dsk_getmap(u32t disk);


/// @name flags for dsk_clonestruct()
//@{
#define DCLN_MBRCODE   0x0001   ///< copy MBR code
#define DCLN_SAMEIDS   0x0100   ///< clone IDs (GUIDs on GPT & serials on LVM)
#define DCLN_IDENT     0x0200   ///< make identical copy (MBR only)
#define DCLN_NOWIPE    0x0400   ///< do now wipe boot sectors on target disk
#define DCLN_IGNSPT    0x8000   ///< ignore sectors per track mismatch
//@}

/** clone disk structure to the empty disk of the same or larger size.
    Function copies only disk structure, not actual partition data. Use
    dsk_clonedata() to copy partition sectors.

    DCLN_IDENT flag force to direct copying of all MBR records byte to byte.
    This mode is not recommended, actually. Flag DCLN_IDENT does NOT include
    DCLN_SAMEIDS - i.e. after direct copying LVM serials will be updated to
    new values (by default).

    DCLN_NOWIPE prevents wiping of boot sectors on newly created partitions
    on target disk. Wiping is ON by default - to remove possible remains of
    old filesystem data.

    @param dstdisk   Destination disk.
    @param srcdisk   Source disk.
    @param flags     Flags (DCLN_).
    @return 0 on success, DPTE_CSPACE if target disk is not empty or too
            small to fit source disk structure, DPTE_ERRWRITE on write error,
            any of DPTE_* errors if srcdisk scan was unsuccessful, DPTE_HYBRID
            if source disk has hybrid partition table (GPT+MBR), DPTE_INCOMPAT
            on sector size or sectors per track value mismatch. */
u32t  _std dsk_clonestruct(u32t dstdisk, u32t srcdisk, u32t flags);

/// @name flags for dsk_clonedata()
//@{
#define DCLD_NOBREAK   0x0001   ///< operation is NON-breakable by ESC key
#define DCLD_SKIPBPB   0x0002   ///< do NOT fix values in boot sector`s BPB
//@}

/** clone partition data to other existing partition of the same or larger size.

    This function knows nothing about file system structures - it just copying
    data. Only one thing it can change - some fields in BPB structure in boot
    sector of destination, but only if valid BPB was detected.

    @param dstdisk   Destination disk.
    @param dstindex  Partition index on destination disk.
    @param srcdisk   Source disk.
    @param srcindex  Partition index on source disk.
    @param cbprint   process indication callback (can be 0)
    @param flags     Flags (DCLD_).
    @return 0 on success or DPTE_* error code */
u32t  _std dsk_clonedata(u32t dstdisk, u32t dstindex,
                         u32t srcdisk, u32t srcindex,
                         read_callback cbprint, u32t flags);

//===================================================================
//  OS/2 LVM information access
//-------------------------------------------------------------------

/// LVM disk info
typedef struct {
   u32t      DiskSerial;        ///< serial number assigned to this disk.
   u32t      BootSerial;        ///< serial number of the boot disk.
   u32t       Cylinders;        ///< cylinders
   u32t           Heads;        ///< heads
   u32t    SectPerTrack;        ///< sectors per track. Can be 255!!!
   char        Name[20];        ///< printable disk name
} lvm_disk_data;

/** query LVM info for selected disk
    @param [in]  disk     disk number
    @param [out] info     disk info, can be 0 to check for at least
                          one LVM signature presence.
    @return boolean (success flag) */
int   _std lvm_diskinfo(u32t disk, lvm_disk_data *info);


/// LVM partition info
typedef struct {
   u32t      DiskSerial;        ///< serial number assigned to this disk.
   u32t      BootSerial;        ///< serial number of the boot disk.
   u32t       VolSerial;        ///< serial number of volume (0 for Boot Manager)
   u32t      PartSerial;        ///< serial number of partition
   u32t           Start;        ///< partition start sector
   u32t            Size;        ///< partition length, in sectors
   u8t         BootMenu;        ///< partition is in Boot Manager menu
   char          Letter;        ///< drive letter assigned to the partition (0 if none)
   char     VolName[20];        ///< printable volume name (empty for Boot Manager partition)
   char    PartName[20];        ///< printable partition name
} lvm_partition_data;

/** query LVM info for selected partition
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @param [out] info     partition info, can be 0 to check LVM signature presence.
    @return boolean (success flag) */
int   _std lvm_partinfo(u32t disk, u32t index, lvm_partition_data *info);

/** query IBM Boot Manager bootable partitions.
    Only partitions with index 0..31 on every disk can be quered.
    @param [out] active   array of dwords (one per every HDD in system), active bits
                          in this dword mark Boot Manager Menu partitions (bit 0 for
                          partition index 0 and so on ...)
    @return boolean (success flag) */
int   _std lvm_querybm(u32t *active);

/** query LVM presence.
    Function is not gurantee LVM info quality, it checks only for presence.
    @param physonly       flag to 1 to not count virtual HDDs (PAE ram disk and so on).
    @return bit mask for first 31 HDDs (1 for presented LVM info, 0 for not)
       and 0x80000000 bit if next HDDs (32...x) have LVM info. I.e. function
       will return 0 if no LVM at all and non-zero value if LVM info present at
       least somewhere. */
u32t  _std lvm_present(int physonly);

/// @name lvm_checkinfo(), lvm_assignletter() result codes
//@{
#define LVME_NOINFO   0x0001     ///< no LVM info on disk
#define LVME_NOBLOCK  0x0002     ///< no partition table info block
#define LVME_NOPART   0x0003     ///< no info for one of existing partition
#define LVME_SERIAL   0x0004     ///< serial number mismatch
#define LVME_GEOMETRY 0x0005     ///< geometry mismatch
#define LVME_DISKNUM  0x0006     ///< wrong disk number argument
#define LVME_INDEX    0x0007     ///< no partition with specified index
#define LVME_LETTER   0x0008     ///< already used or invalid drive letter
#define LVME_FLOPPY   0x0009     ///< disk is floppy formatted
#define LVME_EMPTY    0x000A     ///< disk is empty
#define LVME_IOERROR  0x000B     ///< disk read/write error
#define LVME_PTNAME   0x000C     ///< no partition with specified name
#define LVME_DSKNAME  0x000D     ///< no disk with specified name
#define LVME_GPTDISK  0x000E     ///< disk is GPT formatted
#define LVME_LOWPART  0x000F     ///< partition placed in 1st 63 sectors
#define LVME_INVARGS  0xFFFE     ///< invalid arguments
//@}

/** check LVM info consistency.
    @param  disk     disk number
    @return LVME_NOINFO..LVME_DISKNUM or 0 on success. */
int   _std lvm_checkinfo(u32t disk);

/** query used drive letters.
    @return bit mask: bit 0 for A: is used, bit 1 for B: and so on. */
u32t  _std lvm_usedletters(void);

/** assign drive letter for LVM volume.
    This is OS/2 drive letter, not QSINIT.
    @param disk     disk number
    @param index    partition index
    @param letter   drive letter to set ('A'..'Z' or 0 for no letter).
    @param force    force duplicate drive letters.
    @return 0 on success or LVME_* error code. */
u32t  _std lvm_assignletter(u32t disk, u32t index, char letter, int force);

/// @name name type for lvm_setname()
//@{
#define LVMN_DISK      0x0000     ///< disk name
#define LVMN_PARTITION 0x0001     ///< partition name
#define LVMN_VOLUME    0x0002     ///< volume name
//@}

/** set ASCII disk/volume name.
    @param disk      disk number
    @param index     partition index (ignored for LVMN_DISK)
    @param nametype  name type (LVMN_*). LVMN_PARTITION and LVMN_VOLUME flags
                     can be combined to set the same name for partition and
                     volume name DLAT fields.
    @param name      name to set
    @return 0 on success or LVME_* error code. */
u32t  _std lvm_setname(u32t disk, u32t index, u32t nametype, const char *name);

/** write LVM signatures to disk.
    Better use OS/2 LVM or DFSEE.
    @param disk      disk number
    @param geo       disk virtual CHS geometry, can be 0 for
                     auto-detect.
    @param separate  Disk will have "boot disk serial number" equal to own
                     "serial number". This option must be ON for disks,
                     which will be used for master boot, but now connected
                     as secondary. Use 1 for ON, 0 for OFF and -1 for AUTO
                     (previous value will be used if LVM was exist on disk).
    @return 0 on success or LVME_* error code. */
u32t  _std lvm_initdisk(u32t disk, disk_geo_data *vgeo, int separate);

/** wipe all LVM info from disk.
    Function clears all DLAT entries for all partitions and checks first 255
    sectors of disk for missing DLATs from previous format.

    @param  disk     disk number
    @return LVME_NOINFO..LVME_DISKNUM or 0 on success. */
int   _std lvm_wipeall(u32t disk);

/** search for partition by it`s name.
    @param [in]     name     Partition name.
    @param [in]     nametype Name type (LVMN_*).
                             Volume name can be empty in some cases
                             (boot manager partition, for example).
    @param [in,out] disk     Disk number, init it with 0xFFFFFFFF for all disks
                             or actual disk number for search. Return disk
                             number.
    @param [out]    index    Partition index, 0 for disk name search.
    @return 0 on success or LVME_* error code. LVME_NAME returned if
            no such name. */
u32t  _std lvm_findname(const char *name, u32t nametype, u32t *disk, u32t *index);

//===================================================================
//  GPT specific functions
//-------------------------------------------------------------------

/** convert GPT GUID to string.
    @param  guid    16 bytes GUID array
    @param  str     target string (at least 38 bytes)
    @return boolean (success flag) */
int   _std dsk_guidtostr(void *guid, char *str);

/** convert string to GPT GUID.
    String format must follow example: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.
    @param  str     Source string
    @param  guid    16 bytes GUID array
    @return boolean (success flag) */
int   _std dsk_strtoguid(const char *str, void *guid);

/** generate pseudo-unique GPT GUID.
    @param  [out] guid  16 bytes GUID array */
void _std dsk_makeguid(void *guid);

/** check disk/partition type (GPT or MBR).
    @param disk     Disk number.
    @param index    Partition index (use -1 to check entire disk type).

    Note: If disk is hybrid, but partition located in MBR only - function will return 0,
    not 2 for it.

    @return -1 on invalid disk number/partition index, 0 for MBR disk/partition,
       1 for GPT, >1 if disk have hybrid partition table (both GPT and MBR) or
       partition is hybrid (located both un MBR and GPT) */
int   _std dsk_isgpt(u32t disk, long index);

typedef struct {
   u8t         GUID[16];        ///< disk GUID
   u64t       UserFirst;        ///< first usable sector
   u64t        UserLast;        ///< last usable sector
   u64t         Hdr1Pos;
   u64t         Hdr2Pos;
} dsk_gptdiskinfo;

/** query GPT disk info.
    @param  [in]  disk    Disk number
    @param  [out] dinfo   Buffer for disk info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if disk is MBR). */
u32t  _std dsk_gptdinfo(u32t disk, dsk_gptdiskinfo *dinfo);

/** update GPT disk info.
    Function set new GUID.
    Function allow to change UserFirst/UserLast positions (!), but only if all
    existing partitions still covered by new UserFirst..UserLast value
    (else DPTE_NOFREE returned). Set both UserFirst/UserLast to 0 to skip this.
    Hdr1Pos and Hdr2Pos values ignored.

    @param  disk    Disk number
    @param  dinfo   Buffer with new disk info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if disk is MBR). */
u32t  _std dsk_gptdset(u32t disk, dsk_gptdiskinfo *dinfo);

typedef struct {
   u8t     TypeGUID[16];        ///< partition type GUID
   u8t         GUID[16];        ///< partition GUID
   u64t     StartSector;        ///< Start sector
   u64t          Length;        ///< Length in sectors
   u64t            Attr;        ///< Attributes
   u16t        Name[36];        ///< Readable name
} dsk_gptpartinfo;

/** query GPT partition info.
    @param  [in]  disk    Disk number
    @param  [in]  index   Partition index
    @param  [out] pinfo   Buffer for partition info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if disk is MBR). */
u32t  _std dsk_gptpinfo(u32t disk, u32t index, dsk_gptpartinfo *pinfo);

/** update GPT partition info.
    StartSector and Length parameters must be equal to actual partition data else
    DPTE_RESCAN error will be returned.
    DPTE_INVARGS returned if one of GUIDs or Name is empty (zeroed).

    @param  disk    Disk number
    @param  index   Partition index
    @param  pinfo   Buffer with new partition info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if disk/partition is MBR). */
u32t  _std dsk_gptpset(u32t disk, u32t index, dsk_gptpartinfo *pinfo);

/** init empty hard disk (write GPT partition table).
    @param disk     Disk number.
    @return 0 on success, or DPTE_* error. DPTE_EMPTY is returned
            if disk is NOT empty here. */
u32t  _std dsk_gptinit(u32t disk);

/** create GPT partition.
    Use dsk_ptcreate() for MBR disks, this function will return DPTE_NOFREE
    if disk is MBR and DPTE_HYBRID if partition table is hybrid.

    @param disk     Disk number.
    @param start    Start sector of new partition
    @param size     Size of new partition in sectors, can be 0 for full free
                    block size.
    @param flags    Flags for creation (only DFBA_PRIMARY accepted).
    @param guid     Partition type GUID. Can be 0 ("Windows Data" assumed).
    @return 0 on success, or DPTE_* error. */
u32t  _std dsk_gptcreate(u32t disk, u64t start, u64t size, u32t flags, void *guid);

/** set active partition (GPT disks).
    This function enum all partitions and DROP "BIOS Bootable" attribute, then
    set it for selected partition.
    Not sure about this bit support by someone at all, but QSINIT MBR code for
    GPT disks searches for this attribute and launch partition with it.

    @param [in]  disk     disk number
    @param [in]  index    partition index
    @return 0 on success or DPTE_* constant. */
u32t  _std dsk_gptactive(u32t disk, u32t index);

/// @name name type for dsk_gptfind()
//@{
#define GPTN_DISK      0x0000     ///< disk GUID
#define GPTN_PARTITION 0x0001     ///< partition GUID
#define GPTN_PARTTYPE  0x0002     ///< partition type GUID
//@}

/** search for partition by it`s name.
    @param [in]     guid     GUID to search.
    @param [in]     guidtype GUID type (GPTN_*).
                             For GPTN_PARTTYPE argument first founded
                             partition will be returned.
    @param [in,out] disk     Disk number, init it with 0xFFFFFFFF for all disks
                             or actual disk number for search. Return disk
                             number.
    @param [out]    index    Partition index, 0 for disk name search.
    @return 0 on success or DPTE_* constant. DPTE_PINDEX returned if no
            such GUID. */
u32t  _std dsk_gptfind(void *guid, u32t guidtype, u32t *disk, u32t *index);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_DMFUNC
