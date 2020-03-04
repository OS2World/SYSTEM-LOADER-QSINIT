//
// QSINIT "sysview" module
// "Disk management" dialog
//
#if !defined( __DISKACT_H )
#define __DISKACT_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TColoredText
#define Uses_TLabel
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TCollection

#ifdef __QSINIT__
#include "qsdm.h"
#endif

#define MAX_ACTION  24

#define PARTFILT_PT      1     ///< show partitions only
#define PARTFILT_FREE    2     ///< show free space entries only

#include <tv.h>

/// walking over disks/partitions (abstract class, actually)
class TWalkDiskDialog : public TDialog {
protected:
   TCollection   *lst_d, *lst_p;
   u32t           disks;
   u32t        cur_disk;
   int         cur_part;
   // other types should be inserted after CDFS!
   typedef enum { FAT12, FAT16, FAT32, FAT64, HPFS, JFS, CDFS, NTFS, UNKFS } knowntype;

   void getfs(u32t disk, u64t sector, char *fsname, u8t *buf, knowntype &fst);

#ifdef __QSINIT__
   struct diskdata {
      dsk_mapblock    *mb;
      u32t          ssize;
      u64t        dsksize;
      u64t      usedspace;
      u32t          heads;
      u32t            spt;
      u32t        lvminfo;
      u32t        scan_rc;
      knowntype   *fstype;
      char        *fsname;
      char       *lvmname;
      u8t          *in_bm;
      int          is_gpt;  // dsk_isgpt() value
      int         ramdisk;  // emulated disk
      int          letter;  // >=0 for mounted big floppy (drive letter)
      char      bf_fs[10];  // big floppy file system name
      knowntype bf_fstype;
      dsk_gptpartinfo *gp;  // non-zero on GPT disk only
      TCollection *bsdata;  // boot sectors
   };
   diskdata       *ddta;
#endif
   virtual void FocusToDisk(u32t disk);
   virtual void FreeDiskData();
   virtual void FreeDiskData(u32t disk);
   virtual void UpdatePartList();
   virtual void UpdateAll(Boolean rescan = False);
   // current disk changed as result of handleEvent()
   virtual void NavCurDiskChanged(int prevpos);
   // current partition changed as result of handleEvent()
   virtual void NavCurPartChanged(int prevpos);
   long SelectedDisk(long item=-1);
   void UpdateDiskInfo(u32t disk, Boolean rescan = False);
public:
   TWalkDiskDialog(const TRect &bounds, const char *aTitle);
   ~TWalkDiskDialog();
   virtual void handleEvent( TEvent& );

   TListBox         *lbDisk;
   TListBox     *lbPartList;
   /// partition filter (PARTFILT_* value, 0 by default)
   u32t            partFilt;
};

class TDMgrDialog : public TWalkDiskDialog {
   TCollection   *lst_a, *lst_e;

   u8t      action_disk[MAX_ACTION];
   u8t      action_part[MAX_ACTION];
   u32t    act_disk_cnt,
           act_part_cnt;
   int          no_vhdd;

   enum { actd_mbrboot, actd_clone, actd_init, actd_initgpt, actd_bf_format,
          actd_bf_mount, actd_mbrcode, actd_restvhdd, actd_bf_unmount,
          actd_bf_code, actd_updlvm, actd_savevhdd, actd_wipe, actd_writelvm,
          actd_bf_inst, actd_bf_dirty, actd_bf_label, actd_detect,

          actp_first, actp_boot, actp_delete, actp_format, actp_active,
          actp_mount, actp_unmount, actp_letter, actp_rename, actp_clone,
          actp_makelp, actp_makepp, actp_setgptt, actp_view, actp_bootcode,
          actp_inst, actp_dirty, actp_bootos2, actp_label
        } action;

   virtual void FreeDiskData();
   virtual void UpdatePartList();
   virtual void NavCurPartChanged(int prevpos);
   void UpdateActionList(Boolean empty);
   void AddAction(TCollection *list, u8t action);
   void RunCommand(const char *cmd, const char *args);
   int  SectorSelGoto(void);
   Boolean ActionCall(u8t action, u32t disk, long index);
protected:
   virtual void UpdateAll(Boolean rescan = False);
public:
   TDMgrDialog();
   ~TDMgrDialog();
   virtual void handleEvent( TEvent& );
   virtual Boolean valid( ushort );

   TListBox   *lbDiskAction;
   TListBox       *lbAction;
   TColoredText     *txInfo;
   /// go to this disk/sector in sector view after exit
   u32t            goToDisk;
   u64t          goToSector;
};

class TCloneVolDlg : public TWalkDiskDialog {
   virtual void UpdatePartList();
   virtual void NavCurPartChanged(int prevpos);
   u64t              ptsize;
   u32t       disk, ptindex;
public:
   TCloneVolDlg(u32t srcdisk, u32t index);

   virtual void handleEvent( TEvent& );
   virtual Boolean valid( ushort );

   TColoredText  *txFileSys;
   TColoredText    *txLName;
   TColoredText  *txComment;

   u32t          targetDisk;
   u32t         targetIndex;
};

#endif  // __DISKACT_H
