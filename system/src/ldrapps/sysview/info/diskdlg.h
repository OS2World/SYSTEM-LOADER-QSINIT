//
// QSINIT "sysview" module
// disk dialogs
//
#ifndef __DISKDLGS_H
#define __DISKDLGS_H

#define Uses_TApplication
#define Uses_TDialog
#include <stdio.h>
#include <tv.h>
#include "longedit.h"

#define DISK_MEMORY (0xFFFFFFFF)

class TDiskSearchDialog : public TDialog {
   u64t      start, end;
public:
   u32t            disk,    // DISK_MEMORY for memory search
             sectorsize;
   u8t          *buf32k;
   u64t          sector;
   long        posinsec;

   enum stopReason { stopVoid, stopEnd, stopFound, stopReadErr };
   stopReason    stop;

   TDiskSearchDialog();
   void setLimits (u64t startpos, u64t endpos, u32t ofs_instart);
   void processNext();

   TColoredText *sectorStr;
   TColoredText *percentStr;
};

struct DiskCopySide {
   int    isFile;
   u32t     disk;
   u64t      pos;
   FILE    *file;
   u32t    ssize;
};

class TDiskCopyDialog : public TDialog {
public:
   DiskCopySide  source,
                 destin;
   u64t    bytes, total;
   u8t          *buf32k;
   // 0 - ok, 1 - read error, 2 - write error
   int       stopreason;

   TDiskCopyDialog();
   ~TDiskCopyDialog();
   void startFileToDisk(FILE *src, u32t disk, u64t start, u32t sectors);
   void startDiskToFile(u32t disk, u64t start, FILE *dst, u32t sectors);
   void startDiskToDisk(u32t sdisk, u64t sstart, u32t ddisk, u64t dstart, u32t sectors);
   void processNext();

   TColoredText *sectorStr;
   TColoredText *percentStr;
};

class TDiskBootDialog : public TDialog {
   u32t          startms,
                cseconds;
   int           is_disk;
   TColoredText *timeStr;
public:
   TDiskBootDialog(u32t disk, long index);
   void processNext();
};

#endif // __DISKDLGS_H
