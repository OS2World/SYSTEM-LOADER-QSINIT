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
#include "lvm.h"

#define shellprc(col,x,...)  cmd_printseq(x,0,col,__VA_ARGS__)
#define shellprn(x,...)      cmd_printseq(x,0,0,__VA_ARGS__)
#define shellprt(x)          cmd_printseq(x,0,0)

#define MAX_SECTOR_SIZE  4096

typedef struct {
   u32t               disk;
   int              inited;
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

hdd_info *get_by_disk(u32t disk);

/// "dmgr list" subcommand
u32t shl_dm_list(const char *cmd, str_list *args, u32t disk, u32t pos);
/// "dmgr pm" subcommand
u32t shl_pm     (const char *cmd, str_list *args, u32t disk, u32t pos);
/// "dmgr gpt" subcommand
u32t shl_dm_gpt (const char *cmd, str_list *args, u32t disk, u32t pos);

/** "dmgr boot print"  command
    @return errno */
int  dsk_printbpb(u32t disk, u64t sector);

/** flush PT sector
    @param disk     disk number
    @return 0 on success, or DPTE_* error. */
u32t dsk_flushquad(u32t disk, u32t quadpos);

/** is pt sector contain only empty records?
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @param ign_ext  ignore extended partition records
    @return boolean (1/0) */
u32t dsk_isquadempty(hdd_info *info, u32t quadpos, int ign_ext);

/** find index of extended record in sector`s partition table.
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @return 0..3 for index of -1 on error */
int  dsk_findextrec(hdd_info *info, u32t quadpos);

/** find index of logical partition record in sector`s partition table.
    @param info     disk info table
    @param quadidx  index (0 for primary, 1..x - extended)
    @return 0..3 for index of -1 on error */
int  dsk_findlogrec(hdd_info *info, u32t quadpos);

/// get disk size string
char *get_sizestr(u32t sectsize, u64t disksize);

/** wipe boot sector.
    Function clears 55AA signature and FS name if BPB was detected.
    @param disk     disk number
    @param sector   sector
    @return boolean (success flag) */
int dsk_wipevbs(u32t disk, u64t sector);

/** wipe pt sector.
    @param disk     disk number
    @param quadidx  index (0 for primary, 1..x - extended)
    @param delsig   delete 0x55AA signature - 1/0.
    @return 0 on success, or DPTE_* error. */
u32t dsk_wipequad(u32t disk, u32t quadidx, int delsig);

/** write empty partition table sector.
    @param disk     disk number
    @param sector   sector
    @param table    optional table to copy (can be 0). 
                    LBA values here must be in disk format, not absolute 
                    offsets from the start of disk as stored in hdd_info!
    @return 0 on success, or DPTE_* error. */
u32t dsk_emptypt(u32t disk, u32t sector, struct MBR_Record *table);

/** delete GPT partition.
    Function return DPTE_RESCAN in case of mismatch pt index to start sector of
    it.
    @param disk     Disk number.
    @param index    Partition index
    @param start    Start sector of partition (for additional check)
    @return 0 on success, or DPTE_* error. */
u32t dsk_gptdel(u32t disk, u32t index, u64t start);

/** flush GPT data to disk
    @param info     disk info table
    @return 0 on success, or DPTE_* error. */
u32t dsk_flushgpt(hdd_info *hi);

/** parse disk number.
    Accept hd0, h0, fd2, f2, 1:, 0:, A:, B:
    @param  str  disk number str.
    @return -errno or disk number */
long get_disk_number(char *str);

/** enable/disable cache for specified disk.
    If CACHE module is loaded - this is call to hlp_cacheon(), else - it do
    nothing.
    @return previous state (-1 if no cache or disk is not supported) */
int  dsk_cacheset(u32t disk, int enable);

/// unload dynamically linked CACHE.DLL (decrement usage)
void cache_unload(void);

/** check LM_CACHE_ON_MOUNT env key and load CACHE.DLL if asked in it
    key format is:
       set LM_CACHE_ON_MOUNT = on, 5% */
void cache_envstart(void);

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
    @return 0 on success, or DPTE_* error. */
u32t _std pt_action(u32t action, u32t disk, u32t index, u32t arg);

/// confirmation dialog with default NO button
int  confirm_dlg(const char *text);

/** print error to screen.
    @param  mask        prefix for msg.ini (ex. _DPTE)
    @param  altmsg      alt. text if message number not present
    @param  rc          error code to print */
void common_errmsg(const char *mask, const char *altmsg, u32t rc);

#endif // QSINIT_PSCAN
