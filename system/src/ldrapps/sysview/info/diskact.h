//
// QSINIT "sysview" module
// "Disk management" dialog
//
#if !defined( __DISKACT_H )
#define __DISKACT_H

#define Uses_TStreamable
#define Uses_TStreamableClass
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

#define MAX_ACTION  16

#include <tv.h>
class TDMgrDialog : public TDialog {
   TCollection   *lst_d, *lst_p, *lst_a, *lst_e;
   u32t           disks;
   u32t        cur_disk;
   int         cur_part;
#ifdef __QSINIT__
   struct diskdata {
      dsk_mapblock    *mb;
      u32t          ssize;
      u64t        dsksize;
      u32t          heads;
      u32t            spt;
      u32t        lvminfo;
      u32t        scan_rc;
      char        *fsname;
      char       *lvmname;
      u8t          *in_bm;
      int          is_gpt;  // dsk_isgpt() value
      dsk_gptpartinfo *gp;  // non-zero on GPT disk only
   };
   diskdata       *ddta;
#endif
   u8t      action_disk[MAX_ACTION];
   u8t      action_part[MAX_ACTION];
   u32t    act_disk_cnt,
           act_part_cnt;

   enum { actd_mbrboot, actd_init, actd_initgpt, actd_mbrcode, actd_updlvm,
          actd_wipe, actd_writelvm,
          actp_first, actp_boot, actp_delete, actp_format, actp_active,
          actp_mount, actp_unmount, actp_letter, actp_rename, actp_makelp,
          actp_makepp, actp_setgptt, actp_view
        } action;

   long SelectedDisk(long item=-1);
   void FocusToDisk(u32t disk);
   void FreeDiskData();
   void UpdatePartList();
   void UpdateActionList(Boolean empty);
   void UpdateDiskInfo(u32t disk, Boolean rescan = False);
   void UpdateAll(Boolean rescan = False);
   void AddAction(TCollection *list, u8t action);
   void RunCommand(const char *cmd, const char *args);
public:
   TDMgrDialog(int largeBox = 0);
   ~TDMgrDialog();
   virtual void handleEvent( TEvent& );
   virtual Boolean valid( ushort );

   TListBox *lbDisk;
   TListBox *lbPartList;
   TListBox *lbDiskAction;
   TListBox *lbAction;
   TColoredText *txInfo;
   /// go to this disk/sector in sector view after exit
   u32t      goToDisk;
   u64t      goToSector;
};

#endif  // __DISKACT_H
