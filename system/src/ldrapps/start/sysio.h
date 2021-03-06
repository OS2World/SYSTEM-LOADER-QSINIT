//
// QSINIT "start" module
// i/o handle structs
//
#ifndef QSINIT_SYSIO_REF
#define QSINIT_SYSIO_REF

#include "qstypes.h"
#include "qspdata.h"
#include "qcl/sys/qsvolapi.h"
#include "qcl/sys/qsmuxapi.h"
#include "qcl/sys/qsstream.h"
#include "qsio.h"
#include "qsint.h"
#include "qsmodext.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t _std (*dsk_access_cbf)(u32t disk, u64t sector, u32t count);

/// return 1 on success
int         io_checktime(io_time *src);

/// current process info (data in it must be used inside MT lock only!)
mt_prcdata *get_procinfo(void);

char *_std  io_fullpath_int(char *dst, const char *path, u32t size, qserr *err);

/// walk over sft and "broke" files (must be used inside MT lock only!)
u32t        sft_volumebroke(u8t vol, int enumonly);

/** close all handles, was opened by process.
    @param  pid     process id
    @param  htmask  handle type mask (IOHT_*)
    @return number of closed handles */
u32t        io_close_as(u32t pid, u32t htmask);

/** allocates new sft_entry entry.
    Must be called in locked state (sft access). */
u32t        sft_alloc(void);

/** allocates new io_handle_data entry.
    Must be called in locked state (io_handle array access). */
u32t        ioh_alloc(void);

/** search in SFT by full path.
    Must be called in locked state (sft access) */
u32t        sft_find(const char *fullpath, int name_space);

extern vol_data *_extvol;

#define IOHd_SIGN        0x64484F49     ///< IOHd string
#define SFTe_SIGN        0x65544653     ///< SFTe string
// increment for public fno (in io_handle_info/io_direntry_info structs)
#define SFT_PUBFNO       16

typedef struct _io_handle_data {
   u32t               sign;
   u32t             sft_no;             ///< sft file number
   u32t            self_no;             ///< self index in ftable array
   u32t                pid;
   u32t               next;             ///< next ioh number
   qserr           lasterr;
   qsclock        opentime;             ///< sys clock when handle was opened

   u32t           inh_main;             ///< current "main" handle for the inherited list
   u32t           inh_next;             ///< next handle in the inherited list

   u8t             inherit;             ///< dup this handle for a child
   u8t                res1;             ///< inherited handle
   u16t               res2;

   union {
      struct {
         u32t        omode;             ///< open mode (used for IOHT_STREAM too)
         u64t          pos;
      } file;
      struct {
         dir_handle_int dh;
      } dir;
      struct {
         u32t        omode;             ///< open mode (shared with .file)
         st_handle_int  xh;
         u8t           eof;             ///< EOF (^Z) received for this handle
         u8t           efe;             ///< EOF enabled for this handle
      } std;
   };
} io_handle_data;

typedef struct {
   u32t               sign;
   u32t               type;
   char             *fpath;
   u32t          ioh_first;             ///< first ioh handle
   u32t            pub_fno;             ///< public fileno
   int              broken;             ///< unrecoverably broken (no volume, etc)
   int          name_space;             ///< namespace index
   union {
      struct {
         qs_sysvolume  vol;
         char        *renn;             ///< rename request on last close
         int           del;             ///< delete request on last close
         int            ro;             ///< internal handle is r/o!
         io_handle_int  fh;
         u32t        sbits;             ///< share bits
         u8t        bshift;             ///< block size shift for sector-based i/o
      } file;
      struct {
         qs_sysvolume  vol;
         char        *renn;             ///< rename request on last close
      } dir;
      struct {
         u32t          dsk;
      } disk;
      struct {
         qs_sysstream  dev;
      } std;
      struct {
         mux_handle_int mh;
      } muxev;
      struct {
         void          *qi;
      } que;
   };
} sft_entry;

#define IOHT_FILE        0x00001
#define IOHT_DISK        0x00002
#define IOHT_STREAM      0x00004
#define IOHT_DUMMY       0x00008        ///< dummy zero entry
#define IOHT_DIR         0x00010
#define IOHT_MUTEX       0x00020
#define IOHT_QUEUE       0x00040
#define IOHT_EVENT       0x00080
///< only types allowed to duplicate/inherit
#define IOHT_DUP      (IOHT_FILE|IOHT_MUTEX|IOHT_EVENT|IOHT_QUEUE|IOHT_STREAM)

#define IOH_HBIT      0x40000000        ///< addition to IOH index to make io_handle


#define NAMESPC_FILEIO         0        ///< file i/o namespace (files & dirs)
#define NAMESPC_MUTEX          1        ///< mutexes namespace
#define NAMESPC_QUEUE          2        ///< queues namespace
#define NAMESPC_EVENT          3        ///< events namespace
#define NAMESPC_CHARDEV        4

/** check user io_handle handle and return pointers to objects.
    @attention if no error returned, then MT lock IS ON!
    @param [in]  ifh           handle to check
    @param [in]  accept_types  IOHT_* combination (allowed handle types)
    @param [out] pfh           pointer to io_handle_data, can be 0
    @param [out] pfe           pointer to sft_entry, can be 0
    @return error code */
qserr           io_check_handle  (io_handle ifh, u32t accept_types,
                                  io_handle_data **pfh, sft_entry **pfe);
/// do NOT use me!
qserr           io_check_handle_spec(io_handle ifh, u32t accept_types,
                                  io_handle_data **pfh, sft_entry **pfe);
/*  get handle and check that its open time is equal to "checktime".
    Handle can be closed or even replaced asynchronously, so opentime value
    can be used a safe mark to compare.

    @attention if handle returned, then MT lock IS ON!

    @param [in]  ifh           handle to check
    @param [in]  accept_types  IOHT_* combination (allowed handle types)
    @param [in]  checktime     system clock value, should match to opentime.
    @param [out] pfe           pointer to sft_entry, can be 0
    @return pointer to io_handle_data or 0 if there is no such handle or
            time value does not match  */
io_handle_data *io_same_handle   (io_handle ifh, u32t accept_types,
                                  qsclock checktime, sft_entry **pfe);
/// unlink ioh from sft and zero ftable entry
void            ioh_unlink       (sft_entry *fe, u32t ioh_index);
/// is sft has only one user object?
int             ioh_singleobj    (sft_entry *fe);
/// common handle creation part, must be called inside MT lock
io_handle_data *ioh_setuphandle  (u32t ifno, u32t fno, u32t pid);
/// set heap onwer for new sft entries
void            sft_setblockowner(sft_entry *fe, u32t fno);
/// have no locks inside, must be called inside MT lock
int _std        qe_available_spec(qshandle queue, u32t *ppid, u32t *sft_no);
/// MTLIB service function, ignores access rights
qe_eid* _std    qe_getschedlist  (qshandle queue);
// call for io_dumpsft() dump, assumes MT lock too
int _std        qe_available_info(void *pi, u32t *n_sched);
/** start of MT mode callback.
    setup_loader_mt() calls this to get disk access cb funcs. */
void            setup_fio_mt     (dsk_access_cbf *rcb, dsk_access_cbf *wcb);
/// try to mount ramfs to a volume
qserr           io_mount_ramfs   (u8t vol);
/// handle inheritance for a new process
void _std       io_inherit       (void);


extern sft_entry* volatile      *sftable;
extern io_handle_data* volatile  *ftable;
extern qs_sysmutex                 mtmux;

typedef struct {
   char    *fsname;
   u16t     offset;
   u16t       size;
   u8t    *cmpdata;
} fs_detect_list;

/** return FS detection list.
    Function returns parsed [fsdetect] section contents, in the single module
    owned heap block. Last entry in list is zero-filled */
fs_detect_list* ecmd_readfsdetect();
/// try lo load module for the filesystem, listed in extcmd.ini
void            ecmd_loadfslib   (const char *fsname);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // QSINIT_SYSIO_REF
