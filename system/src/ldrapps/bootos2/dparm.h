//
// QSINIT "bootos2" module
// advanced disk tasks
//
#ifndef QSBT2_DISKPARM
#define QSBT2_DISKPARM

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "ioint13.h"

#define PAEIO_MEMREQ  (128*1024)

/// print BPB
void print_bpb(struct Disk_BPB *bpb);

/// check volume type and make BPB for it
int  replace_bpb(u8t vol, struct Disk_BPB *pbpb);

/// fill enulated disk parameters in oemhlp disk tables
void setup_ramdisk(u8t disk, struct ExportFixupData *efd, u8t *bhpart);
#ifdef __cplusplus
}
#endif
#endif // QSBT2_DISKPARM
