//
// QSINIT "partmgr" module
// HPFS format instance data
//
#ifndef QSINIT_HPFSFMT
#define QSINIT_HPFSFMT

#include "hpfs.h"
#include "qcl/bitmaps.h"
#include "qcl/qslist.h"
#include "pscan.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPB               4                  ///< sectors per buffer
#define SEC_EARLY       (HPFSSEC_SPAREB+1)   ///< start searching from this pos
#define SEC_PER_BAND    (SPB * 512 * 8)      ///< sectors per one bitmap
#define MAX_HOTFIXES    100                  ///< do not touch this!
#define SPARE_DIRBLK     20                  ///< 20 spare DIRBLKs
#define SEC_PER_DIRBLK  (sizeof(hpfs_dirblk)/512)
#define MAX_DIRBLK     4000                  ///< max DIRBLKs in dirblk band
#define FORMAT_LIMIT    (_2GB/512*32)        ///< max. # of sectors (64Gb)


typedef struct {
   bit_map             bmp;                ///< disk bitmap
   dd_list          bslist;                ///< bad sectors list
   u8t                 *pf;                ///< 32k buffer for various ops
   u32t             ptsize;                ///< used partition size

   u32t            voldisk;                ///< physical disk
   u64t           volstart;                ///< volume start sector

   u32t          nfreemaps;                ///< # of bitmap blocks on disk
   u32t            *mapidx;                ///< bitmap sectors index
   u32t          mapidxlen;                ///< # of 2k blocks in bitmap index

   hpfs_badlist    *bblist;                ///< bad block list
   u32t           bbidxlen;                ///< # of 2k blocks in bad block list

   bit_map           dbbmp;                ///< DIRBLK bitmap

   hpfs_superblock     sup;                ///< super block
   hpfs_spareblock     spr;                ///< spare block
   hpfs_cpblock        cpi;                ///< code page dir sector

   u32t           hotfixes[SPB*512/4];     ///< 2k block for hotfixes
   hpfs_dirblk     rootdir;                ///< root DIRBLK
   hpfs_fnode       rootfn;                ///< root FNODE

   union {
      u8t            sb[512];
      struct Boot_Record  br;
   };
   union {
      u8t    cpd_sector[512];
      hpfs_cpdata        cpd;
   };
} fmt_info;

u32t hpfs_sectorcrc(u8t *sector);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_HPFSFMT
