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
#define FREE_SIGN  0x45455246

#define DISK_BOOT       0  // does not work in micro-FSD boot
#define DISK_LDR        1
#define DISK_COUNT     10  // number of volumes in system

#define LDR_SSIZE    1024  // virtual disk sector size
#define LDR_SSHIFT     10  // virtual disk sector shift

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
    @attention data returned in disk i/o buffer!
    @return table with zero length in last entry */
AcpiMemInfo* _std int15mem(void);

/// internal use only: mounted 2:-9: volume data
typedef struct {
   u32t                 flags;              ///< flags for volume
   u32t                  disk;              ///< disk number
   u64t                 start;              ///< start sector of partition
   u32t                length;              ///< length in sectors
   u32t            sectorsize;              ///< disk sector in bytes
   u32t                serial;              ///< volume serial (from BPB)
   char             label[12];              ///< volume label
   u32t               badclus;              ///< bad clusters (only after format!)
} vol_data;

#define STOKEY_ZIPDATA  "zip"                ///< key with .ldi file
#define STOKEY_DELAYCNT "mod_delay"          ///< key with delayed module count
#define STOKEY_INIDATA  "ini"                ///< key with .ini file (init time only)
#define STOKEY_FATDATA  "fdta"               ///< FATFS* data for start module
#define STOKEY_VOLDATA  "vdta"               ///< volume data for start module
#define STOKEY_PCISCAN  "pciscan"            ///< full PCI bus scan (start module only)
#define STOKEY_MEMLIM   "mttrlim"            ///< memlimit assigned by VMTRR setup
#define STOKEY_SAFEMODE "safemode"           ///< safe mode was asked by left shift
#define STOKEY_PHYSMEM  "physmem"            ///< storage key with physmem data
#define STOKEY_BASEMEM  "mbase"              ///< address of "sys.mem" base
#define STOKEY_VDPAGE   "vdisk"              ///< pae disk header page number
#define STOKEY_VDNAME   "vdhdd"              ///< pae disk readable name

/// cache i/o ioctl
//@{
#define CC_SYNC          0x0001              /// FatFs CTRL_SYNC command
#define CC_MOUNT         0x0002              /// Mount volume
#define CC_UMOUNT        0x0003              /// Unmount volume
#define CC_SHUTDOWN      0x0004              /// Disk i/o shutdown
#define CC_IDLE          0x0005              /// Idle action (keyboard read)
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
   /// sector read function
   cache_read_func       cache_read;
   /// sector write function
   cache_write_func     cache_write;
} cache_extptr;

/** install/remove external cache processor
    @param  fptr  Function, use NULL to remove handler.
    @return success flag (1/0) */
u32t _std hlp_runcache(cache_extptr *fptr);

/// printf char output function type
typedef void _std (*print_outf)(int ch, void *stream);

/** common internal printf, used by all printf functions.
    @param  fp     user data, "stream" parameter for outc
    @param  fmt    format string
    @param  arg    pointer to variable arguments
    @param  outc   character output callback
    @return number of printed characters */
int _std _prt_common(void *fp, const char *fmt, long *arg, print_outf outc);

#ifdef __cplusplus
}
#endif
#pragma pack()
#endif // QSINIT_LDRDATA
