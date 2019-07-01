#include "clib.h"
#include "qsint.h"
#include "qsinit.h"
#include "qsconst.h"
#include "dskinfo.h"
#include "qslog.h"
#include "qecall.h"

extern vol_data*      extvol;
struct qs_diskinfo  qd_array[MAX_QS_DISK];
u8t                  useAFio = 1;
extern u8t           qd_fdds,
                     qd_hdds;
u32t              mfsd_fsize;      // size of opened file
extern u64t     qs_bootstart,
                  qs_bootlen;
extern u32t      qd_bootdisk;

u8t _std raw_io(struct qs_diskinfo *di, u64t start, u32t sectors, u8t write) {
   return call64(EFN_DSKIO, 2, 4, di, start, sectors, (u32t)write);
}

u16t _std  mfs_open (void) {
   return call64(EFN_MFSOPEN, 0, 1, &mfsd_fsize);
}

u32t __cdecl mfs_read(u32t offset, u32t buf, u32t readsize) {
   return call64(EFN_MFSREAD, 0, 3, offset, buf, readsize);
}

u32t __cdecl mfs_close(void) {
   return call64(EFN_MFSCLOSE, 0, 0);
}

u32t __cdecl mfs_term(void) {
   return call64(EFN_MFSTERM, 0, 0);
}

u16t _std strm_open(void) {
   mfsd_fsize = FFFF;
   return 1;
}

u16t __cdecl strm_read(u32t buf, u16t readsize) { return 0; }

u16t _std fat_open (void) {
   mfsd_fsize = FFFF;
   return 1;
}

u32t __cdecl fat_read (u32t offset, void *buf, u32t readsize) { return 0; }
void __cdecl fat_close(void) {}
void __cdecl fat_term (void) {}

// setup boot partition i/o
void init_vol0() {
   if (qd_bootdisk<qd_hdds) {
      vol_data *vdta   = extvol;
      vdta->length     = qs_bootlen;
      vdta->start      = qs_bootstart;
      vdta->disk       = qd_bootdisk;
      vdta->flags      = VDTA_ON|VDTA_RO;
      vdta->sectorsize = qd_array[qd_bootdisk+MAX_QS_FLOPPY].qd_sectorsize;
      log_printf("vol.0: %02X %LX %08X \n", vdta->disk, vdta->start, vdta->length);
   }
}
