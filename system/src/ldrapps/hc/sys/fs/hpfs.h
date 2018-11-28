//
// QSINIT "fslib" module
// HPFS structs
//
#ifndef QSINIT_HPFSINFO
#define QSINIT_HPFSINFO

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

typedef u32t LSN;

typedef struct {
   LSN        main;
   LSN       spare;
} RSP;

#define HPFS_SUP_SIGN1    0xF995E849
#define HPFS_SUP_SIGN2    0xFA53E9C5
#define HPFS_SUP_VOLNSIZE         32

/** sector 16 - super block */
typedef struct {
   u32t           header1;           ///< HPFSSUP_SIGN1
   u32t           header2;           ///< HPFSSUP_SIGN2
   u8t            version;           ///< version of volume
   u8t        funcversion;           ///< oldest version that can use volume
   u16t          reserved;
   LSN             rootfn;           ///< fnode of root directory
   u32t           sectors;           ///< # of sectors on volume
   u32t        badsectors;           ///< # of bad sectors on volume
   RSP            bitmaps;           ///< pointers to free space bitmaps
   RSP            badlist;           ///< badblock list chain #1
   u32t       chkdsk_date;           ///< date of last CHKDSK, 0 if never
   u32t          opt_date;           ///< date of last Optimize, 0 if never
   u32t         dbsectors;           ///< # of sectors in dir band
   LSN       dirblk_start;           ///< first sector in dir band
   LSN         dirblk_end;           ///< last sector in dir band
   LSN         dirblk_bmp;           /** dir band bitmap, 1 dnode per bit,
                                         starts on a 2k, 2k max */
   u8t  vol_name[HPFS_SUP_VOLNSIZE];
   LSN          sid_table;           ///< SID table (8 preallocated sectors)
   u8t         empty[412];
} hpfs_superblock;

#define HPFS_SPR_SIGN1    0xF9911849
#define HPFS_SPR_SIGN2    0xFA5229C5

#define HPFS_DIRTY              0x01 ///< file system is dirty
#define HPFS_SPARER             0x02 ///< spare dirblks have been used
#define HPFS_HFUSED             0x04 ///< hot fix sectors have been used
#define HPFS_BADSEC             0x08 ///< bad sectors
#define HPFS_BADBMP             0x10 ///< bad bitmap block
#define HPFS_FASTFMT            0x20 ///< fast-formatted volume
#define HPFS_OLDVER             0x80 ///< old version wrote to volume

#define HPFS_F2_DD_INSTALL      0x01 ///< install DASD limits
#define HPFS_F2_DD_SYNCH        0x02 ///< resynch DASD limits information
#define HPFS_F2_DD_RUNNING      0x04 ///< DASD limits operational
#define HPFS_F2_MM              0x08 ///< MultiMedia active
#define HPFS_F2_ACL             0x10 ///< ACLs active
#define HPFS_F2_DD_DIRTY        0x20 ///< DASD limits: disk is dirty

/** sector 17 - spare block */
typedef struct {
   u32t           header1;           ///< HPFSSPR_SIG1
   u32t           header2;           ///< HPFSSPR_SIG2
   u8t              flags;           ///< base flags (dirty, etc)
   u8t             flags2;           ///< HPFS386 flags
   u8t       contigfactor;           ///< contiguity factor for MM
   u8t              align;           ///< alignment
   LSN             hotfix;           ///< first hotfix list sector
   u32t       hotfix_used;           ///< # of hot fixes in effect
   u32t        hotfix_max;           ///< size of hot fix list
   u32t      dnspare_free;           ///< # of spare dnodes unused
   u32t       dnspare_max;           ///< length of spare dnodes list
   LSN             cp_dir;           ///< code page info sector
   u32t         codepages;           ///< number of code pages
   u32t          superCRC;           ///< CRC of superblock
   u32t          spareCRC;           ///< CRC of spareblock
   u32t      reserved[15];
   LSN  spare_dirblk[101];           ///< emergency free dnode list
} hpfs_spareblock;

/** 2k slice of bad block list */
typedef struct bad_list {
   LSN                fwd;           ///< link to continue
   LSN        list[512-1];           ///< 511 entries
} hpfs_badlist;


/// code page info entry
typedef struct {
   u16t           country;           ///< country code
   u16t          codepage;           ///< code page id
   u32t         cpdataCRC;           ///< checksum of code page data
   LSN             cpdata;           ///< sector with code page data
   u16t             index;           ///< index of code page (on volume)
   u16t        dbcsranges;           ///< # of DBCS ranges (0 -> no DBCS)
} hpfs_cpinfo;


#define HPFS_CPINFOPERSEC         31 ///< cp info entries per sector

/// code page info sector
typedef struct {
   u32t            header;           ///< signature (HPFS_CPDIR_SIG)
   u32t         codepages;           ///< # of cp info in this sector
   u32t             first;           ///< index of 1st cp info in this sector
   LSN                fwd;           ///< link to continue
   hpfs_cpinfo     cpinfo[HPFS_CPINFOPERSEC];
} hpfs_cpblock;


#define HPFS_CPDATAPERSEC          3 ///< cp data entries per sector

//// code page data sector
typedef struct {
   u32t            header;           ///< signature (HPFS_CPDAT_SIG)
   u16t         codepages;           ///< count of code pages in this sector
   u16t             first;           ///< index of 1st cp data in this sector
   u32t           dataCRC[HPFS_CPDATAPERSEC];
   u16t           dataofs[HPFS_CPDATAPERSEC];
} hpfs_cpdata;


typedef struct {
   u16t           country;           ///< country code
   u16t          codepage;           ///< code page id
   u16t        dbcsranges;           ///< # of DBCS ranges
   u8t        uprtab[128];
   u16t      dbcsrange[1];
} hpfs_cpdataentry;


#define ATTR_DIRECTORY	0x10

typedef struct {
   u16t           recsize;           ///< this DIRENT size (aligned to 4!)
   u8t              flags;           ///< flags (HPFS_DF_*)
   u8t              attrs;           ///< file attributes
   LSN              fnode;           ///< FNODE sector
   u32t          time_mod;           ///< last modification time
   u32t             fsize;           ///< file size
   u32t       time_access;           ///< last access time
   u32t       time_create;           ///< FNODE creation time
   u32t             ealen;           ///< # bytes in extended attributes
   u8t            aclsize;			 ///< number of ACLs (low 3 bits)
   u8t            cpindex;           ///< codepage index on volume
   u8t            namelen;           ///< file name length
   char           name[1];           ///< length (namelen bytes)
} hpfs_dirent;

#define HPFS_DF_SPEC          0x0001 ///< .. entry
#define HPFS_DF_ACL           0x0002 ///< ACL
#define HPFS_DF_BTP           0x0004 ///< entry has a btree down pointer
#define HPFS_DF_END           0x0008 ///< last record flag
#define HPFS_DF_ATTR          0x0010 ///< EA list present
#define HPFS_DF_PERM          0x0020 ///< permission list present
#define HPFS_DF_XACL          0x0040 ///< explicit ACL (?)
#define HPFS_DF_NEEDEAS       0x0080 ///< "need" EAs

#define HPFS_DIRBLKSIZE         2048 ///< size of directory block

typedef struct {
   u32t               sig;           ///< signature
   u32t         firstfree;           ///< offset of first free byte
   u32t            change;           ///< change count (low bit is flag - 1 for topmost block)
   LSN             parent;           ///< parent directory if not topmost, else FNODE sector
   LSN               self;           ///< sector # of this dirblk
   u8t             startb;           ///< first dir entry record
   u8t   reserved[HPFS_DIRBLKSIZE-21];
} hpfs_dirblk;

#define HPFSSEC_SUPERB            16 ///< superblock - sector 16
#define HPFSSEC_SPAREB            17 ///< spareblock - sector 17

typedef struct {
   u8t               flag;           ///< bit flags
   u8t        reserved[3];
   u8t               free;           ///< # of free entries
   u8t               used;           ///< # of used entries
   u16t           freeofs;           ///< offset to first free entry
} hpfs_alblk;

#define HPFS_ABF_NODE           0x80 ///< if not a leaf node
#define HPFS_ABF_BIN            0x40 ///< suggest using binary search to find
#define HPFS_ABF_FNP            0x20 ///< parent is an FNODE
#define HPFS_ABF_NFG            0x01 ///< high bit of free value

typedef struct {
   u32t               log;           ///< file sector number
   u32t            runlen;           ///< length of run in sectors
   u32t              phys;           ///< disk sector number of data block
} hpfs_alleaf;

typedef struct {
   u32t               log;           ///< children have data below this pos
   u32t              phys;           ///< disk sector number of child block
} hpfs_alnode;

#define HPFS_LEAFPERSEC           40
#define HPFS_NODEPERSEC           60

typedef struct {
   u32t           header;            ///< signature
   u32t             self;            ///< sector number of block itself
   u32t           parent;            ///< sector number of parent block
   hpfs_alblk        alb;            ///< header for array of objects
   union {
      hpfs_alleaf   aall[HPFS_LEAFPERSEC];
      hpfs_alnode   aaln[HPFS_NODEPERSEC];
   };
} hpfs_alsec;

#define HPFS_LEAFPERFNODE          8
#define HPFS_NODEPERFNODE         12

typedef struct {
   hpfs_alblk        alb;            ///< header for array of objects
   union {
      hpfs_alleaf   aall[HPFS_LEAFPERFNODE];
      hpfs_alnode   aaln[HPFS_NODEPERFNODE];
   };
   u32t            vdlen;            ///< length of valid data in file
} hpfs_filestorage;


typedef struct {
   u32t           dl_max;
   u32t           dl_use;
} hpfs_dlrec;

/// storage pointer
typedef struct {
   u32t           runlen;
   LSN               lsn;
} hpfs_sptr;

typedef struct {
   hpfs_sptr          sp;            ///< length of disk-resident data
   u16t            fnlen;            ///< length of fnode-resident data
   u8t             flags;            ///< flags (HPFS_EAF_*)
} hpfs_auxinfo;

#define HPFS_EAF_BIGD           0x01 ///< EA has big data field
#define HPFS_EAF_DAT            0x02 ///< if BIGD is set, sp.lsn points to hpfs_alsec
#define HPFS_EFF_NEED           0x80 ///< this is a 'NEED' EA

#define HPFS_FNNAMELEN            16
#define HPFS_UIDSIZE              16

typedef struct {
   u32t           header;            ///< signature
   u32t           hist_s;            ///< sequential read history (not used)
   u32t           hist_r;            ///< fast read history (not used)
   char        name[HPFS_FNNAMELEN]; ///< first 15 bytes of file name.
   LSN              pdir;            ///< parent fnode (directory)
   hpfs_auxinfo      acl;            ///< ACLs
   u8t          histbits;            ///< count of valid history bits
   hpfs_auxinfo       ea;            ///< EA info
   u8t              flag;            ///< flags
   hpfs_filestorage  fst;            ///< file storage
   u32t           needea;            ///< number of EA`s with NEEDEA set
   char         uid[HPFS_UIDSIZE];   ///< UID value
   u16t           aclofs;            ///< offset of first ACL
   u8t     reserved[10-sizeof(hpfs_dlrec)];
   hpfs_dlrec      ddrec;            ///< DASD record
   /// free space, ACL & EA stored here
   u8t       rspace[512-66-2*sizeof (hpfs_auxinfo)-sizeof (hpfs_filestorage)];
} hpfs_fnode;

#define HPFS_FNF_DIR            0x01 ///< is a directory fnode

#define HPFS_ANODE_SIG    0x37E40AAE
#define HPFS_DNODE_SIG   (HPFS_ANODE_SIG + 0x40000000)
#define HPFS_FNODE_SIG   (HPFS_ANODE_SIG + 0xC0000000)
#define HPFS_CPDIR_SIG    0x494521F7
#define HPFS_CPDAT_SIG   (HPFS_CPDIR_SIG + 0x40000000)

#pragma pack()

#ifdef __cplusplus
}
#endif
#endif // QSINIT_HPFSINFO
