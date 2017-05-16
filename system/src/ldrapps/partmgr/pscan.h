//
// QSINIT "partmgr" module
// partition scan functions (not exported)
//
#ifndef QSINIT_PSCAN
#define QSINIT_PSCAN

#include "qstypes.h"
#include "qsutil.h"
#include "qsshell.h"
#include "parttab.h"
#include "qsdm.h"
#include "qslog.h"
#include "qserr.h"
#include "lvm.h"
#include "qsconst.h"
#include "qcl/sys/qsedinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define shellprc(col,x,...)  cmd_printseq(x,0,col,__VA_ARGS__)
#define shellprn(x,...)      cmd_printseq(x,0,0,__VA_ARGS__)
#define shellprt(x)          cmd_printseq(x,0,0)

#define FMT_FILL         0xF6      ///< old good format byte

typedef struct {
   u32t               disk;
   int              inited; ///< first time scan done (can be reset by other code)
   u32t            pt_size; ///< number of total pts entries (multipl. of 4)
   u32t            pt_view; ///< number of viewable partitions
   u32t            scan_rc;
   u32t          lba_error; ///< number of extended records with wrong LBA in parent
   u32t            lvm_spt; ///< LVM sectors per track value (can be 127 and 255!)
   u32t           lvm_snum; ///< LVM`s disk serial
   u32t          lvm_bsnum; ///< LVM`s boot disk serial
   disk_geo_data      info;
   struct MBR_Record  *pts; ///< partition records (4 x X)
   u32t            *ptspos; ///< positions of pt sectors (X)
   u32t             extpos; ///< selected extended partition start sector
   u32t             extlen; ///< selected extended partition length
   u32t           extstart; ///< first sector of USED space in extended part.
   u64t             extend; ///< end of USED space in extended part.
   u64t          usedfirst; ///< first sec, used by user partitions or FFFF64
   u64t           usedlast; ///< last sec, used by user partitions or FFFF64
   u32t             *index; ///< translation of pts to viewable partition index
   DLA_Table_Sector  *dlat; ///< LVM info sectors
   dsk_freeblock      *fsp; ///< free space list
   u32t           fsp_size; ///< number of blocks in free space list
   int         gpt_present; ///< # of GPT partitions in MBR
   int              hybrid; ///< hybrid mbr & gpt on disk (ignored now, but detected)
   int             non_lvm; ///< LVM is impossible on this disk

   u32t            gpthead; ///< GPT header sector
   struct GPT_Record  *ptg; ///< GPT partition records
   u32t           gpt_size; ///< number of GPT partition records
   u32t        gpt_sectors; ///< number of sectors used for GPT partition records
   u32t         *gpt_index; ///< translation of ptg to viewable partition index
   u32t           gpt_view; ///< number of viewable GPT + MBR partitions!
   struct Disk_GPT  *ghead, ///< GPT header sector copy (1st)
                   *ghead2; ///< GPT header sector copy (2nd), can be 0 on error
} hdd_info;

extern long memOwner, memPool;

hdd_info *get_by_disk(u32t disk);

/// "dmgr list" subcommand
u32t      shl_dm_list(const char *cmd, str_list *args, u32t disk, u32t pos);
/// "dmgr pm" subcommand
u32t      shl_pm     (const char *cmd, str_list *args, u32t disk, u32t pos);
/// "dmgr gpt" subcommand
u32t      shl_dm_gpt (const char *cmd, str_list *args, u32t disk, u32t pos);

/** "dmgr boot print"  command
    @return errno */
int       dsk_printbpb(u32t disk, u64t sector);

/** flush PT sector
    @param disk     disk number
    @return 0 on success, or error code. */
qserr     dsk_flushquad(u32t disk, u32t quadpos);

/** is pt sector contain only empty records?
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @param ign_ext  ignore extended partition records
    @return boolean (1/0) */
u32t      dsk_isquadempty(hdd_info *info, u32t quadpos, int ign_ext);

/** find index of extended record in sector`s partition table.
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @return 0..3 for index of -1 on error */
int       dsk_findextrec(hdd_info *info, u32t quadpos);

/** find index of logical partition record in sector`s partition table.
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @return 0..3 for index of -1 on error */
int       dsk_findlogrec(hdd_info *info, u32t quadpos);

/// get disk size string
#define get_sizestr(sectorsize,disksize) \
   dsk_formatsize(sectorsize, disksize, 11, 0)

/** wipe boot sector.
    Function clears 55AA signature and FS name if BPB was detected.
    @param disk     disk number
    @param sector   sector
    @return boolean (success flag) */
int       dsk_wipevbs(u32t disk, u64t sector);

/** wipe pt sector.
    @param disk     disk number
    @param quadidx  index (0 for primary, 1..x - extended)
    @param delsig   delete 0x55AA signature - 1/0.
    @return 0 on success, or E_PTE_* error. */
qserr     dsk_wipequad(u32t disk, u32t quadidx, int delsig);

/** write empty partition table sector.
    @param disk     disk number
    @param sector   sector
    @param table    optional table to copy (can be 0).
                    LBA values here must be in disk format, not absolute
                    offsets from the start of disk as stored in hdd_info!
    @return 0 on success, or E_PTE_* error. */
qserr     dsk_emptypt(u32t disk, u32t sector, struct MBR_Record *table);

/** delete GPT partition.
    Function return E_PTE_RESCAN in case of mismatch pt index to start sector of
    it.
    @param disk     Disk number.
    @param index    Partition index
    @param start    Start sector of partition (for additional check)
    @return 0 on success, or E_PTE_* error. */
qserr     dsk_gptdel(u32t disk, u32t index, u64t start);

/** parse disk number.
    Accept hd0, h0, fd2, f2, 1:, 0:, A:, B:
    @param  str  disk number str.
    @return -errno or disk number */
long      get_disk_number(char *str);

/** check LM_CACHE_ON_MOUNT env key and load CACHE.DLL if asked in it
    key format is:
       set LM_CACHE_ON_MOUNT = on, 5% */
void      cache_envstart(void);

/// free internally used CACHE instance
void      cache_free(void);

#define ACTION_SETACTIVE       0
#define ACTION_SETTYPE         1   ///< arg is new type
#define ACTION_DELETE          2   ///< arg is start sector (for additional check)

#define ACTION_NORESCAN  0x10000   ///< do not force rescan before action
#define ACTION_DIRECT    0x20000   ///< add for direct index in pts array
#define ACTION_MASK      0x000FF

/** common pt sector action.
    @param  action      action to do (ACTION_*) with added flags
    @param  disk        disk number
    @param  index       viewable or direct partition index (depends on ACTION_DIRECT)
    @param  arg         argument for call, depends on action
    @return 0 on success, or E_PTE_* error. */
u32t _std pt_action(u32t action, u32t disk, u32t index, u32t arg);

/** finalizing format action.
    @param  vol         volume to re-mount back
    @param  di          volume info
    @param  ptbyte      partition byte type to set (-1 to ignore)
    @param  volidx      volume index on disk (for ptbyte, -1 to ignore)
    @param  badcnt      number of bad clusters (for format message text)
    @return 0 on success, or E_PTE_* error. */
u32t      format_done(u8t vol, disk_volume_data di, long ptbyte, long volidx, u32t badcnt);

u32t _std hpfs_format(u8t vol, u32t flags, read_callback cbprint);

int  _std hpfs_dirty(u8t vol, int on);

void*_std hpfs_freadfull(u8t vol, const char *name, u32t *bufsize);

u32t _std exf_format(u8t vol, u32t flags, u32t unitsize, read_callback cbprint);

/** update exFAT boot record.
    @param  disk        disk number
    @param  sector      boot sector
    @param  data        one or more sectors
    @param  nsec        # of sectors in data
    @param  zerorem     zero remain sectors in boot area (bool flag = 1/0)
    @return boolean (success flag) */
int       exf_updatevbr(u32t disk, u64t sector, void *data, u32t nsec, u32t zerorem);

/// exFAT checksum calc
u32t      exf_sum(u8t src, u32t sum);

/// fill sectors by "value" (expanded version of dsk_emptysector()
u32t _std dsk_fillsector(u32t disk, u64t sector, u32t count, u8t value);

/** scan sectors for DLAT record.
    @param  disk        disk number
    @param  start       start sector (zero is NOT accepted)
    @param  count       number of sectors to scan
    @return 0 if not found or error occured, else sector number. */
u32t _std lvm_finddlat(u32t disk, u32t sector, u32t count);

/// VHDD disk?
int       dsk_vhddmade(u32t disk);

/// confirmation dialog with default NO button
int       confirm_dlg(const char *text);

/** get QS error message.
    @return string in heap block, for unknown code returns "altmsg"
            message */
char*     make_errmsg  (qserr rc, const char *altmsg);

/// lock common shared mutex
void      scan_lock  (void);
/// unlock common shared mutex
void      scan_unlock(void);

#ifdef __cplusplus
}
// global mutex
class FUNC_LOCK {
public:
   FUNC_LOCK() { scan_lock(); }
   ~FUNC_LOCK() { scan_unlock(); }
};

#endif
#endif // QSINIT_PSCAN
