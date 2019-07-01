//
// QSINIT "fslib" module
// JFS structs
//
#ifndef QSINIT_JFSINFO
#define QSINIT_JFSINFO

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JFS_SUPER_SIGN   0x3153464A // "JFS1"
#define JFS_VERSION               1 // version number

#define JFS_AIX          0x80000000 ///< AIX support
#define JFS_OS2          0x40000000 ///< OS/2 support
#define JFS_DFS          0x20000000 ///< DCE DFS LFS support
#define JFS_LINUX        0x10000000 ///< Linux support

#define JFS_SPARSE       0x00020000 ///< sparse regular file support

typedef struct {
   u32t        tv_sec;
   u32t       tv_nsec;
} jfs_time;

#pragma pack(1)
typedef struct {
   u32t          sign;
   u32t       version;   ///< version number
   u64t          size;   ///< aggregate size in hardware/LVM blocks
   u32t         bsize;   ///< aggregate block size in bytes; 
   u16t       l2bsize;   ///< log2 of bsize
   u16t     l2bfactor;   ///< log2 (bsize/hardware block size)
   u32t        pbsize;   ///< hardware/LVM block size in bytes
   u16t      l2pbsize;   ///< log2 of pbsize
   u16t     reserved1;   ///< data alignment
   u32t        agsize;   ///< allocation group size in aggr. blocks
   u32t          flag;   ///< aggregate attributes
   u32t         state;   ///< mount/unmount/recovery state
   u32t      compress;   ///< > 0 if data is compressed
   u64t          ait2;   ///< first extent of secondary aggregate inode table
   u64t          aim2;   ///< first extent of secondary aggregate inode map
   u32t        logdev;   ///< device address of log
   u32t     logserial;   ///< log serial number at aggregate mount
   u64t        logpxd;   ///< inline log extent
   u64t       fsckpxd;   ///< inline fsck work space extent
   jfs_time      time;   ///< time last updated
   u32t    fsckloglen;   ///< Number of filesystem blocks reserved for the fsck service log.  
   u8t        fscklog;   /** which fsck service log is most recent:
                             0 => no service log data yet
                             1 => the first one
                             2 => the 2nd one */
   char     fpack[11];   ///< file system volume name 
   // extend FS features, not used at all
   u64t         xsize;   ///< extend size
   u64t      xfsckpxd;   ///< extend fsckpxd
   u64t       xlogpxd;   ///< extend logpxd
   // 136 bytes boundary (jfs header lies about 128 here)
   char        attach;   ///< flag: set when aggregate is attached
   u8t   reserved2[7];   ///< reserved - set to 0
   u64t   totalUsable;   ///< # of 1K blocks available to "normal" users.
   u64t       minFree;   ///< # of 1K blocks held in reserve for exclusive use of root.
   u64t      realFree;   ///< # of free 1K blocks can be used by "normal" users.
   u8t     empty[344];
} jfs_superblock;

typedef struct {
   u32t         addr1;
   u32t         addr2;
} jfs_pxd;

#define pxd_addr(pxd) ((u64t)((pxd)->addr1&0xFF000000)<<8 | (pxd)->addr2)
#define pxd_len(pxd)  ((pxd)->addr1&0xFFFFFF)

///< data extent descriptor
typedef struct {
   u8t           flag;
   u8t         res[3];
   u32t          size;   ///< size in byte
   u32t         addr1;
   u32t         addr2;
} jfs_dxd;

typedef struct {
   u8t           flag;
   u16t          res2;
   u8t           off1;   ///< offset in unit of fsblksize
   u32t          off2;
   u32t         addr1;   ///< address in unit of fsblksize
   u32t         addr2;
} jfs_xad;

#define XTREEMAXSLOT     256
#define XTENTRYSTART       2

#define xad_addr(xad) ((u64t)((xad)->addr1&0xFF000000)<<8 | (xad)->addr2)
#define xad_ofs(xad)  ((u64t)(xad)->off1<<32 | (xad)->off2)
#define xad_len(xad)  ((xad)->addr1&0xFFFFFF)

typedef union {
   struct xtheader {
      u64t       next;
      u64t       prev;
      u8t        flag;
      u8t        res1;
      s16t  nextindex;
      s16t   maxentry;
      s16t       res2;
      jfs_pxd    self;
   } header;
   jfs_xad        xad[XTREEMAXSLOT];
} jfs_xtreepage;

///< DASD limit information
typedef struct {
   u8t         thresh;   ///< alert threshold (in percent)
   u8t          delta;   ///< alert threshold delta (in percent)
   u8t           res1;
   u8t       limit_hi;   ///< DASD limit (in logical blocks)
   u32t      limit_lo;
   u8t        res2[3];
   u8t        used_hi;   ///< DASD usage (in logical blocks)
   u32t       used_lo;
} jfs_dasd;

#define DTSLOTDATALEN    15

/** directory page slot
    directory entry consists of type dependent head/only segment/slot and
    additional segments/slots linked via next field; end marked with -1 */
typedef struct {
   s8t           next;
   s8t            cnt;
   u16t          name[DTSLOTDATALEN];
} jfs_dtslot;

#define DTIHDRSIZE       10
#define DTIHDRDATALEN	 11

/// internal node entry head/only segment
typedef struct {
   jfs_pxd         xd;   ///< child extent descriptor
   s8t           next;
   u8t         namlen;
   u16t          name[DTIHDRDATALEN];
} jfs_idtentry;

#define DTLHDRSIZE        6
#define DTLHDRDATALEN    13

/// leaf node entry head/only segment
typedef struct {
   u32t       inumber;
   s8t           next;
   u8t         namlen;
   u16t          name[DTLHDRDATALEN];
} jfs_ldtentry;

typedef union {
   struct {
      jfs_dasd   dasd;   ///< DASD limit/usage info
      u8t        flag;
      s8t   nextindex;   ///< next free entry in stbl
      s8t     freecnt;   ///< free count
      s8t    freelist;   ///< freelist header
      u32t    idotdot;   ///< parent inode number
      s8t     stbl[8];   ///< sorted entry index table
   } header;
   jfs_dtslot slot[9];
} jfs_dtroot;


/** directory regular page.
   entry slot array of 32 byte slot
  
   sorted entry slot index table (stbl):
   contiguous slots at slot specified by stblindex, 1-byte per entry
     512 block:  16 entry tbl (1 slot)
    1024 block:  32 entry tbl (1 slot)
    2048 block:  64 entry tbl (2 slot)
    4096 block: 128 entry tbl (4 slot)
  
   data area:
     512 block:  16 - 2 =  14 slot
    1024 block:  32 - 2 =  30 slot
    2048 block:  64 - 3 =  61 slot
    4096 block: 128 - 5 = 123 slot
  
   Index is zero based; index fields refer to slot index except nextindex which
   refers to entry index in stbl;
   end of entry stot list or freelist is marked with -1 */
typedef union {
   struct {
      u64t       next;   ///< next sibling
      u64t       prev;   ///< previous sibling
      u8t        flag;
      s8t   nextindex;   ///< next entry index in stbl
      s8t     freecnt;
      s8t    freelist;   ///< slot index of head of freelist
      u8t     maxslot;   ///< number of slots in page slot[]
      s8t   stblindex;   ///< slot index of start of stbl
      u8t       rsrvd[2];
      jfs_pxd    self;   ///< self pxd
   } header;
   jfs_dtslot    slot[128];
} jfs_dtpage;


typedef struct {
   u8t            res;
   u8t           flag;   ///< 0 if free
   u8t           slot;   ///< slot within leaf page of entry
   u8t          addr1;   ///< upper 8 bits of leaf page address
   u32t         addr2;   ///< low dd of leaf page -OR- index of next entry when this entry was deleted
} jfs_dir_table_slot;

#define BT_TYPE       0x07
#define BT_ROOT       0x01  ///< root page
#define BT_LEAF       0x02  ///< leaf page
#define BT_INTERNAL   0x04  ///< internal page
#define BT_RIGHTMOST  0x10  ///< rightmost page
#define BT_LEFTMOST   0x20  ///< leftmost page

typedef struct {
   u32t   di_inostamp;   ///< stamp to show inode belongs to fileset
   s32t    di_fileset;   ///< fileset number
   u32t     di_number;   ///< inode number, aka file serial number
   u32t        di_gen;   ///< inode generation number
   jfs_pxd   di_ixpxd;   ///< inode extent descriptor
   u64t       di_size;   ///< size
   u64t    di_nblocks;   ///< number of blocks allocated
   u32t      di_nlink;   ///< number of links to the object
   u32t        di_uid;   ///< user id of owner
   u32t        di_gid;   ///< group id of owner
   u32t       di_mode;   ///< attribute, format and permission

   jfs_time  di_atime;   ///< time last data accessed
   jfs_time  di_ctime;   ///< time last status changed
   jfs_time  di_mtime;   ///< time last data modified
   jfs_time  di_otime;   ///< time created

   jfs_dxd     di_acl;   ///< acl descriptor
   jfs_dxd      di_ea;   ///< ea descriptor

   s32t   di_next_idx;   ///< next available dir_table index
   s32t    di_acltype;   ///< Type of ACL

   union {
      /* this table contains the information needed to find a directory
         entry from a 32-bit index. If index is small enough, the table
         is inline, otherwise, an x-tree root overlays this table */
      jfs_dir_table_slot table[12];
      u8t              extdata[96];
   };
   union {
      jfs_dtroot        dtroot;       ///< dtree root
      u8t               xtroot[288];  ///< jfs_xtreepage - 18(16) entries
      struct {
         jfs_dasd         dasd;
         jfs_dxd           dxd;
         union {
             u32t         rdev;
             u8t       symlink[128];
         };
         u8t          inlineea[128];
      } special;
   };
} jfs_dinode;

/// @name dinode.di_mode
//@{
#define IFMT             0x0000F000  ///< mask of file type
#define IFDIR            0x00004000  ///< directory
#define IFREG            0x00008000  ///< regular file
#define IFLNK            0x0000A000  ///< symbolic link

#define IREADONLY        0x02000000  ///< no write access to file
#define IARCHIVE         0x40000000  ///< archive bit
#define ISYSTEM          0x08000000  ///< system file
#define IHIDDEN          0x04000000  ///< hidden file
#define IRASH            0x4E000000  ///< mask for changeable attributes
#define INEWNAME         0x80000000  ///< non-8.3 filename format
#define IDIRECTORY       0x20000000  ///< directory (shadow of real bit)
#define ATTRSHIFT                25  ///< bit shift to make dos attr bits
#define ISWAPFILE        0x00800000  ///< file open for pager swap space
//@}

/// @name fixed reserved inode number
//@{
#define AGGR_RESERVED_I           0  ///< aggregate inode (reserved)
#define AGGREGATE_I               1  ///< aggregate inode map inode
#define BMAP_I                    2  ///< aggregate block allocation map inode
#define LOG_I                     3  ///< aggregate inline log inode
#define BADBLOCK_I                4  ///< aggregate bad block inode
#define FILESYSTEM_I             16  ///< 1st/only fileset inode
//@}

/// @name per fileset inode
//@{
#define FILESET_RSVD_I            0  ///< fileset inode (reserved)
#define FILESET_EXT_I             1  ///< fileset inode extension
#define ROOT_I                    2  ///< fileset root inode
#define ACL_I                     3  ///< fileset ACL inode
#define FILESET_OBJECT_I          4  ///< 1st fileset inode available for a file/dir/link
//@}

// PSIZE >= file system block size >= physical block size >= DISIZE

/// @name fs fundamental size
//@{
#define PSIZE                  4096  ///< jfs "page" size
#define DISIZE                  512  ///< on-disk inode size
#define L2DISIZE                  9  ///< bit shift of DISIZE
#define IDATASIZE               256  ///< inode inline data size
#define IXATTRSIZE              128  ///< inode inline extended attribute size

#define INOSPERIAG             4096  ///< number of disk inodes per iag
#define L2INOSPERIAG             12  ///< bit shift for # of disk inodes per iag
#define INOSPEREXT               32  ///< number of disk inode per extent
#define L2INOSPEREXT              5
#define IXSIZE   (DISIZE*INOSPEREXT) ///< inode extent size
#define INOSPERPAGE               8  ///< number of disk inodes per 4K page
#define L2INOSPERPAGE             3  ///< bit shift INOSPERPAGE

#define INODE_EXTENT_SIZE    IXSIZE  ///< inode extent size
#define JFS_MAXSIZE    ((u64t)1<<52)
#define MINJFS            0x1000000  ///< Minimum size of a JFS partition
//@}

/// @name fixed byte offset address
//@{
#define JFS_SUPER1_OFS       0x8000  ///< primary superblock physical offset
#define JFS_AIMAP_OFF        0x9000  ///< control page of aggregate inode map
#define JFS_AITBL_OFF        0xB000  ///< 1st extent of aggregate inode table
#define JFS_SUPER2_OFS (JFS_AITBL_OFF+INODE_EXTENT_SIZE)  ///< secondary superblock
#define JFS_BMAP_OFF   (JFS_SUPER2_OFS + PSIZE) ///< bitmap
//@}

#define EXTSPERIAG              128  ///< number of disk inode extent per iag
#define SMAPSZ                    4  ///< number of words per summary map

/// convert inode number to iag number */
#define INOTOIAG(ino)   ((ino) >> L2INOSPERIAG)

/// convert iag number to logical block number of the iag page
#define IAGTOLBLK(iagno,l2nbperpg)  (((iagno) + 1) << (l2nbperpg))

/// starting block number of the 4K page of an inode extent that contains ino
#define INOPBLK(pxd,ino,l2nbperpg) \
  (pxd_addr(pxd) + ((((ino) & INOSPEREXT-1) >> L2INOSPERPAGE) << (l2nbperpg)))

/// inode allocation group page (per 4096 inodes of an AG)
typedef struct {
   u64t       agstart;   ///< starting block of ag
   s32t        iagnum;   ///< inode allocation group number
   s32t    inofreefwd;   ///< ag inode free list forward
   s32t   inofreeback;   ///< ag inode free list back
   s32t    extfreefwd;   ///< ag inode extent free list forward
   s32t   extfreeback;   ///< ag inode extent free list back
   s32t       iagfree;   ///< iag free list
   u32t       inosmap[SMAPSZ]; ///< summary map: 1 bit per inode extent
   u32t       extsmap[SMAPSZ]; ///< sum map of mapwords w/ free extents
   s32t     nfreeinos;   ///< number of free inodes
   s32t     nfreeexts;   ///< number of free extents
   u8t      pad[1976];   ///< pad to 2048 bytes
   u32t          wmap[EXTSPERIAG]; ///< working allocation map
   u32t          pmap[EXTSPERIAG]; ///< persistent allocation map
   jfs_pxd     inoext[EXTSPERIAG]; ///< inode extent addresses
} jfs_iag;

#pragma pack()

#ifdef __cplusplus
}
#endif
#endif // QSINIT_JFSINFO
