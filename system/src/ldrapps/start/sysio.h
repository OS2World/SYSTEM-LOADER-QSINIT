//
// QSINIT "start" module
// i/o handle structs
//
#ifndef QSINIT_SYSIO_REF
#define QSINIT_SYSIO_REF

#include "qstypes.h"
#include "qcl/sys/qsvolapi.h"
#include "qspdata.h"
#include "qsio.h"
#include "qsint.h"

#ifdef __cplusplus
extern "C" {
#endif

/// return 1 on success
int io_checktime(io_time *src);

/// current process info (data in it must be used inside MT lock only!)
mt_prcdata *get_procinfo(void);

char *_std io_fullpath_int(char *dst, const char *path, u32t size, qserr *err);

/// walk over sft and "broke" files (must be used inside MT lock only!)
u32t sft_volumebroke(u8t vol, int enumonly);

/** close all handles, was opened by process.
    @param  pid     process id
    @param  htmask  handle type mask (IOHT_*)
    @return number of closed handles */
u32t io_close_as(u32t pid, u32t htmask);

/// cache ioctl (must be used inside MT lock only!)
void cache_ctrl(u8t vol, u32t action);

extern vol_data *_extvol;

#define IOHd_SIGN        0x64484F49     ///< IOHd string
#define SFTe_SIGN        0x65544653     ///< SFTe string

typedef struct _io_handle_data {
   u32t               sign;
   u32t             sft_no;             ///< sft file number
   u32t                pid;
   u32t               next;             ///< next ioh number
   qserr           lasterr;

   union {
      struct {
         u64t          pos;
         u32t        omode;             ///< open mode
      } file;
      struct {
         dir_handle_int dh;
      } dir;
   };
} io_handle_data;

typedef struct {
   u32t               sign;
   u32t               type;
   char             *fpath;
   u32t          ioh_first;             ///< first ioh handle
   u32t            pub_fno;             ///< public fileno
   int              broken;             ///< unrecoverably broken (no volume, etc)
   union {
      struct {
         qs_sysvolume  vol;
         char        *renn;             ///< rename request on last close
         int           del;             ///< delete request on last close
         io_handle_int  fh;
         u32t        sbits;             ///< share bits
      } file;
      struct {
         qs_sysvolume  vol;
         char        *renn;             ///< rename request on last close
      } dir;
      struct {
         u32t           dh;
      } disk;
      struct {
         u32t          num;             ///< 0=stdin, 1=stdout, 2=stderr, 3=stdaux (log)
      } std;
   };
} sft_entry;

#define IOHT_FILE        0x00001
#define IOHT_DISK        0x00002
#define IOHT_STDIO       0x00004
#define IOHT_DUMMY       0x00008        ///< dummy zero entry
#define IOHT_DIR         0x00010

#define IOH_HBIT      0x40000000        ///< addition to IOH index to make io_handle

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // QSINIT_SYSIO_REF

