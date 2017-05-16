//
// QSINIT
// internal data & structs
//
#ifndef QSINIT_LDRDATA
#define QSINIT_LDRDATA

#include "qstypes.h"
#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

#define PHYSMEM_TABLE_SIZE 16               // check definition in asm code!
typedef struct {
   u32t             startaddr;              ///< start address of memory block
   u32t              blocklen;              ///< length of memory block in bytes
} physmem_block;

extern u16t   rm16code;                     ///< real mode cs

typedef struct _AcpiMemInfo {
   u32t           BaseAddrLow;              ///< base address
   u32t          BaseAddrHigh;
   u32t             LengthLow;              ///< length in bytes
   u32t            LengthHigh;
   u32t           AcpiMemType;              ///< block type
} AcpiMemInfo;

/* Top of low memory (typical, micro-FSD size can be smaller 64k):
        -------------------------------
   7500 | buffer for RM disk read ops |
        -------------------------------
   7D00 | real mode log buffer        |
        -------------------------------
   7E00 | DMPI data (GDT, buffers)    |
        -------------------------------
   8100 | micro-FSD seg (if present)  |
        -------------------------------
   9100 | self code                   |
        ------------------------------- */

struct _free_block {
   u32t                  sign;
   struct _free_block   *prev;
   struct _free_block   *next;
   u32t                  size;              ///< size in 64k blocks
};
typedef struct _free_block free_block;
#define FREE_SIGN      0x45455246

#define DISK_BOOT               0           ///< boot volume
#define DISK_LDR                1
#define DISK_COUNT             10           ///< number of volumes in system

#define LDR_SSIZE            1024           ///< virtual disk sector size
#define LDR_SSHIFT             10           ///< virtual disk sector shift

#define UNPDELAY_SIGN  0x2169646C           ///< "ldi!" sign for delayed unpack

/** boot parameters.
    diskbuf_seg - 32k buffer in 1Mb, used for disk i/o and some other funcs,
    but can be re-used in user atomic ops. */
typedef struct {
   void          *minifsd_ptr;              ///< buffer with mini-fsd (or 0)
   u8t              boot_disk;              ///< OS2LDR boot disk
   u8t             boot_flags;              ///< OS2LDR boot flags
   u16t           diskbuf_seg;              ///< QSINIT disk buffer seg
// include filetab.h to get access to the next field ;)
#ifdef BF_MINIFSD
   struct filetable_s filetab;              ///< micro-fsd original file table data
#endif
} boot_data;

/// query 1mb available memory
u16t _std int12mem(void);

/** query "SMAP" memory table
    @return table with zero length in last entry, memory block must be free()-ed */
AcpiMemInfo* _std hlp_int15mem(void);

/// internal use only: mounted volumes data
typedef struct {
   u32t                 flags;              ///< flags for volume (VDTA_*)
   u32t                  disk;              ///< disk number
   u64t                 start;              ///< start sector of partition
   u32t                length;              ///< length in sectors
   u32t            sectorsize;              ///< disk sector in bytes
   u32t                serial;              ///< volume serial (from BPB)
   char             label[12];              ///< volume label
   u32t               badclus;              ///< bad clusters (only after format!)
   u32t                clsize;              ///< cluster size (in sectors)
   u32t                clfree;              ///< free clusters
   u32t               cltotal;              ///< total clusters
   char             fsname[9];              ///< file system name (or 0)
} vol_data;

/// vol_data flags value
//@{
#define VDTA_ON     0x001  // volume available
#define VDTA_FAT    0x010  // volume mounted to FatFs
//@}

#define STOKEY_ZIPDATA  "zip"                ///< key with .ldi file
#define STOKEY_INIDATA  "ini"                ///< key with .ini file (init time only)
#define STOKEY_VOLDATA  "vdta"               ///< volume data for start module
#define STOKEY_PCISCAN  "pciscan"            ///< full PCI bus scan (start module only)
#define STOKEY_MEMLIM   "mttrlim"            ///< memlimit assigned by VMTRR setup
#define STOKEY_SAFEMODE "safemode"           ///< safe mode was asked by left shift
#define STOKEY_PHYSMEM  "physmem"            ///< storage key with physmem data
#define STOKEY_BASEMEM  "mbase"              ///< address of "sys.mem" base
#define STOKEY_VDPAGE   "vdisk"              ///< pae disk header page number
#define STOKEY_VDNAME   "vdhdd"              ///< pae disk readable name
#define STOKEY_CMMODE   "cm_noreset"         ///< leave cpu modulation as it is on exit
#define STOKEY_MFSDCRC  "mfsdcrc"            ///< mini-FSD crc32

/// cache i/o ioctl
//@{
#define CC_SYNC          0x0001              /// FatFs CTRL_SYNC command
#define CC_MOUNT         0x0002              /// Mount volume
#define CC_UMOUNT        0x0003              /// Unmount volume
#define CC_SHUTDOWN      0x0004              /// Disk i/o shutdown
#define CC_FLUSH         0x0006              /// Flush all
#define CC_RESET         0x0007              /// FatFs disk_initialize()
//@}
/// cache ioctl function
typedef void _std (*cache_ioctl_func)(u8t vol, u32t action);
/// cache read function
typedef u32t _std (*cache_read_func)(u32t disk, u64t pos, u32t ssize, void *buf);
/// cache write function
typedef u32t _std (*cache_write_func)(u32t disk, u64t pos, u32t ssize, void *buf);

typedef struct {
   /// number of entries in this table
   u32t    entries;
   /// ioctl function
   cache_ioctl_func     cache_ioctl;
   /// sector read function (MT locked state guaranteed during call)
   cache_read_func       cache_read;
   /// sector write function (MT locked state guaranteed during call)
   cache_write_func     cache_write;
} cache_extptr;

/** install/remove external cache processor.
    Call should be made in MT locked state. CACHE module is a single user of
    this function.
    @param  fptr  Function, use NULL to remove handler.
    @return success flag (1/0) */
u32t _std hlp_runcache(cache_extptr *fptr);

/// internal disk i/o bits
//@{
#define QDSK_IAMCACHE   0x40000              /// special flag for cache nested call
#define QDSK_IGNACCESS  0x80000              /// skip access call
//@}

/// printf char output function type
typedef void _std (*print_outf)(int ch, void *stream);

/** common internal printf, used by all printf functions.
    @param  fp     user data, "stream" parameter for outc
    @param  fmt    format string
    @param  arg    pointer to variable arguments
    @param  outc   character output callback
    @return number of printed characters */
int _std _prt_common(void *fp, const char *fmt, long *arg, print_outf outc);

typedef int _std (*scanf_getch)(void *stream);
typedef int _std (*scanf_ungetch)(int ch, void *stream);

/** common internal scanf, used by all scanf functions.
    @param  fp     user data, "stream" parameter for gc/ugc
    @param  fmt    format string
    @param  arg    pointer to variable arguments
    @param  gc     get char callback, can be 0 (fp will be used as string)
    @param  ugc    unget char callback, can be 0
    @return common scanf return value */
int _std _scanf_common(void *fp, const char *fmt, char **args, scanf_getch gc,
                       scanf_ungetch ugc);

/** lock context switching over system.
    Note, what this is internal call for system api implementation, not user
    level apps.
    Lock has counter, number of mt_swunlock() calls must be equal to number
    of mt_swlock().
    Process/thread exit resets lock to 0.

    @attention mt_waitobject() and all its users like mt_muxcapture() or
               long usleep() - will reset lock to 0! */
void      mt_swlock  (void);
/// unlock context switching
void      mt_swunlock(void);

// eliminate any regs saving
#if defined(__WATCOMC__)
#pragma aux mt_swlock   "_*" modify exact [];
#pragma aux mt_swunlock "_*" modify exact [];
#endif

/** atomic bidirectional list link.
    Note, that logic is different if "last" arg available. Without it
    new item becomes first, with it - last in list.
    @param  src     item to add
    @param  lnkoff  offset of prev & next fields in item
    @param  first   pointer to list head variable
    @param  last    pointer to list tail variable (can be 0) */
void _std hlp_blistadd(void *src, u32t lnkoff, void **first, void **last);

/// atomic bidirectional list unlink (pair for hlp_blistadd()).
void _std hlp_blistdel(void *src, u32t lnkoff, void **first, void **last);

/// OEM-Unicode bidirectional conversion
u16t _std ff_convert  (u16t src, unsigned int to_unicode);
/// Unicode upper-case conversion
u16t _std ff_wtoupper (u16t src);

#ifdef __cplusplus
}
#endif
#pragma pack()
#endif // QSINIT_LDRDATA
