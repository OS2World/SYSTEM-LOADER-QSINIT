//
// QSINIT "sysview" module
// disk management dialog
// ------------------------------------------------------------
// be careful, this file was created at September, Fri 13, 2013, 256 day
// of the year ;)
//
#include "diskedit.h"
#include <stdlib.h>
#include "parttab.h"
#ifdef __QSINIT__
#include "qsshell.h"
#include "vio.h"
#endif

#if !defined( __DISKACT_H )
#include "diskact.h"
#endif

#define cmRescan    1000

#define TXINFO_RED   0x7C
#define TXINFO_BLUE  0x79

#define LARGEBOX_INC    7

TWalkDiskDialog::TWalkDiskDialog(const TRect &bounds, const char *aTitle) :
   TDialog(bounds, aTitle), TWindowInit(TWalkDiskDialog::initFrame)
{
   lst_d=0; lst_p=0; disks=0; cur_disk=0; cur_part=-1;
   lbDisk=0; lbPartList=0; partFilt=0;
#ifdef __QSINIT__
   ddta = 0;
#endif
}

TWalkDiskDialog::~TWalkDiskDialog() {
   if (lst_d) { delete lst_d; lst_d = 0; }
   if (lst_p) { delete lst_p; lst_p = 0; }
   FreeDiskData();
}

void TWalkDiskDialog::FocusToDisk(u32t disk) {
   if (disk>=disks) return;
   if (SelectedDisk()==disk) return;

   for (long ii=0; ii<lbDisk->range; ii++)
      if (ii!=lbDisk->focused)
         if (SelectedDisk(ii)==disk) {
            lbDisk->focusItem(ii);
            return;
         }
   lbDisk->focusItem(0);
}

void TWalkDiskDialog::FreeDiskData() {
#ifdef __QSINIT__
   if (disks && ddta)
      for (u32t ii=0; ii<disks; ii++) {
         if (ddta[ii].mb) { free(ddta[ii].mb); ddta[ii].mb = 0; }
         if (ddta[ii].fsname) { free(ddta[ii].fsname); ddta[ii].fsname = 0; }
         if (ddta[ii].lvmname) { free(ddta[ii].lvmname); ddta[ii].lvmname = 0; }
         if (ddta[ii].in_bm) { free(ddta[ii].in_bm); ddta[ii].in_bm = 0; }
         if (ddta[ii].gp) { free(ddta[ii].gp); ddta[ii].gp = 0; }
      }
   delete[] ddta;
   ddta = 0;
#endif
   disks = 0;
}

void TWalkDiskDialog::UpdateAll(Boolean rescan) {
#ifdef __QSINIT__
   int   ii;
   FreeDiskData();

   disks = hlp_diskcount(0);
   lst_d = new TCollection(0,10);

   if (disks) {
      ddta = new diskdata[disks];

      for (ii=0; ii<(int)disks; ii++) {
         UpdateDiskInfo(ii,rescan);
         if (ddta[ii].ssize) {
            char *str = new char[64];
            sprintf(str,"HDD %-4i  ",ii);
            strcat(str,getSizeStr(ddta[ii].ssize,ddta[ii].dsksize));
            lst_d->insert(str);
         }
      }
      lbDisk->newList(lst_d);
      // try to put focus on old place
      FocusToDisk(cur_disk);
      if (cur_disk!=SelectedDisk()) {
         cur_disk = SelectedDisk();
         cur_part = 0;
      }

      UpdatePartList();
   }   
#endif
}

void TWalkDiskDialog::UpdatePartList() {
#ifdef __QSINIT__
   if (cur_disk>=disks || !ddta) return;
   lst_p = new TCollection(0,10);

   if (ddta[cur_disk].mb) {
      dsk_mapblock *mb = ddta[cur_disk].mb;
      u32t     cylsize = ddta[cur_disk].heads * ddta[cur_disk].spt;
      do {
         char *str = new char[64], topic[16];

         if (mb->Type) {
            //if (partFilt!=0 && partFilt!=PARTFILT_PT) { delete str; continue; }
            char *msg = 0;
            if (ddta[cur_disk].is_gpt) {
               // GPT partition list
               char guidstr[40];

               dsk_guidtostr(ddta[cur_disk].gp[mb->Index].TypeGUID, guidstr);
               msg = cmd_shellgetmsg(guidstr);

               if (msg) sprintf(str, "%-24s", msg); else
                  strcpy(str, "unknown partition type  ");
            } else {
               // MBR partition list
               char *fsname = ddta[cur_disk].fsname+32*mb->Index;
               sprintf(topic, "_PTE_%02X", mb->Type);
               msg = cmd_shellgetmsg(topic);
               
               if ((mb->Flags&DMAP_PRIMARY)==0) strcpy(str,"Logical  "); else
                  strcpy(str,mb->Flags&DMAP_ACTIVE?"Active   ":"Primary  ");
               sprintf(topic, "%02X  ", mb->Type);
               strcat(str, topic);
               /// all whose types must have BPB
               if (mb->Type<=PTE_1F_EXTENDED && mb->Type!=PTE_0A_BOOTMGR || 
                  mb->Type==PTE_35_JFS) 
               {
                  if (*fsname) sprintf(topic, "%-11s", fsname); else
                     strcpy(topic, "unformatted");
               } else
               if (msg) sprintf(topic, "%-11s", msg); else
                  strcpy(topic, "           ");
               strcat(str, topic);
            }
            if (msg) free(msg);
         } else {
            /* avoid of adding slack space at the end of disk, beyond the end
               of last complete cylinder */
            if ((mb->Flags&DMAP_LEND)!=0 && mb->StartSector%cylsize == 0 &&
               mb->Length < cylsize && ddta[cur_disk].dsksize-mb->StartSector 
                  < cylsize) { delete str; break; }
            //if (partFilt!=0 && partFilt!=PARTFILT_FREE) { delete str; continue; }
            strcpy(str, "FREE                    ");
         }
         sprintf(topic," %8s",getSizeStr(ddta[cur_disk].ssize,mb->Length,True));
         strcat(str,topic);
         
         lst_p->insert(str);
      } while ((mb++->Flags&DMAP_LEND)==0);
   }
   if (cur_part>=lst_p->getCount()) cur_part = 0;
   lbPartList->newList(lst_p);

   if (lbPartList->focused!=cur_part) lbPartList->focusItem(cur_part);
#endif
}

void TWalkDiskDialog::UpdateDiskInfo(u32t disk, Boolean rescan) {
#ifdef __QSINIT__
   disk_geo_data  geo;
   if (!ddta || disk>=disks) return;
   diskdata &dd = ddta[disk];
   memset(&dd, 0, sizeof(diskdata));

   dd.scan_rc = dsk_ptrescan(disk,rescan?1:0);

   if (hlp_disksize(disk, &dd.ssize, &geo)) {
      dd.dsksize = geo.TotalSectors;
      dd.mb      = dsk_getmap(disk);
      dd.heads   = geo.Heads;
      dd.spt     = geo.SectOnTrack;
      // update to partition table/lvm values
      if (dsk_getptgeo(disk,&geo)==0) {
         dd.heads  = geo.Heads;
         dd.spt    = geo.SectOnTrack;
      }
      dd.lvminfo = lvm_checkinfo(disk);
      dd.is_gpt  = dsk_isgpt(disk,-1);
      dd.ramdisk = hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED?1:0;

      long   cnt = dsk_partcnt(disk), ii;

      if (cnt>0) {
         dd.fsname = (char*)malloc(32*cnt);
         memZero(dd.fsname);
         // get fs name from boot sector
         for (ii=0; ii<cnt; ii++) dsk_ptquery64(disk, ii, 0, 0, dd.fsname+32*ii, 0);
         // query GPT partition info
         if (dd.is_gpt) {
            dd.gp = (dsk_gptpartinfo*)malloc(sizeof(dsk_gptpartinfo) * cnt);
            memZero(dd.gp);
            for (ii=0; ii<cnt; ii++) dsk_gptpinfo(disk, ii, dd.gp+ii);
         }
         // get lvm partition name and drive letter
         if (dd.lvminfo==0) {
            lvm_partition_data info;
            dd.lvmname = (char*)malloc(32*cnt);
            dd.in_bm   = (u8t*)malloc(cnt);
            memZero(dd.lvmname);
            memZero(dd.in_bm);

            for (ii=0; ii<cnt; ii++) 
               if (lvm_partinfo(disk, ii, &info)) {
                  memcpy(dd.lvmname+32*ii, info.VolName, 20);
                  dd.in_bm  [ii] = info.BootMenu;
               }
         }
      }
      dsk_usedspace(disk, 0, &dd.usedspace);
   }
#endif
}

long getDiskNum(TListBox *lb, long item) {
   char diskname[16], *cp;
   lb->getText(diskname, item>=0?item:lb->focused, 15);
   cp = strchr(diskname, ' ');
   if (cp) return atoi(cp);
   return -1;
}

long TWalkDiskDialog::SelectedDisk(long item) {
   return getDiskNum(lbDisk,item);
}

void TWalkDiskDialog::NavCurDiskChanged(int prevpos) {
   long dsk = SelectedDisk();
   if (dsk>=0) {
      cur_disk = dsk;
      cur_part = 0;
      UpdatePartList();
   }
}

void TWalkDiskDialog::NavCurPartChanged(int prevpos) {
   cur_part = lbPartList->focused;
}

void TWalkDiskDialog::handleEvent(TEvent &event) {
   int dprev = lbDisk->focused,
       pprev = lbPartList->focused;

   TDialog::handleEvent(event);

   if (dprev!=lbDisk->focused) NavCurDiskChanged(dprev); else
   if (pprev!=lbPartList->focused) NavCurPartChanged(pprev);
}

static const char *DMGR_CMD = "DMGR",
                *FORMAT_CMD = "FORMAT";

TDMgrDialog::TDMgrDialog(int largeBox) :
   TWalkDiskDialog(TRect(4, 2, 76, 21+(largeBox?LARGEBOX_INC:0)), "Disk Management"),
   TWindowInit(TDMgrDialog::initFrame)
{
   lst_a=0; lst_e=0; goToDisk=FFFF; goToSector=FFFF64;
   // add 7 to height if screen lines >=32
   largeBox = largeBox? LARGEBOX_INC : 0;

   TView *control;
   options |= ofCenterX | ofCenterY;
   helpCtx = hcDmgrDlg;
  
   control = new TScrollBar(TRect(26, 2, 27, 8));
   insert(control);
   lbDisk = new TListBox(TRect(2, 2, 26, 8), 1, (TScrollBar*)control);
   lbDisk->helpCtx = hcDmgrDisk;
   insert(lbDisk);
   insert(new TLabel(TRect(1, 1, 6, 2), "Disk", lbDisk));

   control = new TScrollBar(TRect(58, 2, 59, 6));
   insert(control);
   lbDiskAction = new TListBox(TRect(29, 2, 58, 6), 1, (TScrollBar*)control);
   lbDiskAction->helpCtx = hcDmgrDiskAction;
   insert(lbDiskAction);
   insert(new TLabel(TRect(46, 1, 58, 2), "Disk action", lbDiskAction));

   control = new TScrollBar(TRect(37, 10, 38, 18+largeBox));
   insert(control);
   lbPartList = new TListBox(TRect(2, 10, 37, 18+largeBox), 1, (TScrollBar*)control);
   lbPartList->helpCtx = hcDmgrPart;
   insert(lbPartList);
   insert(new TLabel(TRect(1, 9, 11, 10), "Partition", lbPartList));

   control = new TScrollBar(TRect(58, 10, 59, 18+largeBox));
   insert(control);
   lbAction = new TListBox(TRect(40, 10, 58, 18+largeBox), 1, (TScrollBar*)control);
   lbAction->helpCtx = hcDmgrAction;
   insert(lbAction);
   insert(new TLabel(TRect(51, 9, 59, 10), "Action", lbAction));

   control = new TButton(TRect(60, 4, 70, 6), "~M~ake", cmOK, bfDefault);
   insert(control);
   control = new TButton(TRect(60, 6, 70, 8), "~R~escan", cmRescan, bfNormal);
   control->helpCtx = hcDmgrRescan;
   insert(control);
   /*control = new TButton(TRect(60, 8, 70, 10), "~H~elp", cmHelp, bfNormal);
   insert(control);*/
   control = new TButton(TRect(60, 8, 70, 10), "~E~xit", cmCancel, bfNormal);
   insert(control);

   txInfo = new TColoredText(TRect(29, 7, 59, 9), "", TXINFO_RED);
   insert(txInfo);

   cur_disk=0; cur_part=0;
   UpdateAll();
  
   selectNext(False);
}

TDMgrDialog::~TDMgrDialog() {
   if (lst_a) { delete lst_a; lst_a = 0; }
   if (lst_e) { delete lst_e; lst_e = 0; }
}

void TDMgrDialog::FreeDiskData() {
   TWalkDiskDialog::FreeDiskData();
   act_disk_cnt = 0;
   act_part_cnt = 0;
}

void TDMgrDialog::RunCommand(const char *cmd, const char *args) {
#ifdef __QSINIT__
   TProgram::application->suspend();
   vio_clearscr();

   cmd_eproc cmdptr = cmd_shellrmv(cmd, 0);
   if (!cmdptr) return;

   cmd_shellcall(cmdptr, args, 0);

   vio_setcolor(VIO_COLOR_GRAY);
   vio_strout("\npress any key");
   vio_setcolor(VIO_COLOR_RESET);
   key_read();

   TProgram::application->resume();
   TProgram::application->redraw();
#endif
}

void TDMgrDialog::NavCurPartChanged(int prevpos) {
   TWalkDiskDialog::NavCurPartChanged(prevpos);
#ifdef __QSINIT__
   UpdateActionList(ddta[cur_disk].scan_rc ? True : False);
#endif
}

void TDMgrDialog::handleEvent( TEvent& event) {
   TWalkDiskDialog::handleEvent(event);
   switch (event.what) {
      case evCommand:
         if (event.message.command==cmRescan) UpdateAll(True);
         break;
      default:
         break;
   }
}

void TDMgrDialog::AddAction(TCollection *list, u8t action) {
   static const char *text[] = {"Boot", "Clone", "Init disk", "Init disk (GPT)",
      "Replace MBR code", "Restore MBR backup", "Update LVM info", "Save MBR backup",
      "Wipe disk", "Write LVM info",
      0,
      "Boot", "Delete", "Format", "Make active", "Mount", "Unmount",
      "LVM drive letter", "LVM rename", "Clone", "Create logical", 
      "Create primary", "Set type GUID", "Sector view"};

   if (action==actp_first) return; else
   if (action<actp_first) {
      if (act_disk_cnt>=MAX_ACTION) return;
      action_disk[act_disk_cnt++] = action;
   } else {
      if (act_part_cnt>=MAX_ACTION) return;
      action_part[act_part_cnt++] = action;
   }
   list->insert(newStr(text[action]));
}

void TDMgrDialog::UpdatePartList() {
   TWalkDiskDialog::UpdatePartList();
#ifdef __QSINIT__
   if (cur_disk>=disks || !ddta) return;
   u32t ii, clone = 0;
   // seaching for empty disk of suitable size (to add "Clone" string)
   if (ddta[cur_disk].scan_rc==0 && ddta[cur_disk].usedspace!=FFFF64)
      for (ii=0; ii<disks; ii++) 
         if (ii!=cur_disk && ddta[ii].scan_rc==DPTE_EMPTY)
            if (ddta[ii].ssize==ddta[cur_disk].ssize && 
               ddta[ii].dsksize>=ddta[cur_disk].usedspace) { clone = 1; break; }
   // update disk action list
   lst_e = new TCollection(0,10);

   u32t lvme = ddta[cur_disk].lvminfo, 
     badcode = lvme==LVME_SERIAL || lvme==LVME_GEOMETRY || lvme==LVME_EMPTY ||
               lvme==LVME_FLOPPY || lvme==LVME_GPTDISK || lvme==LVME_LOWPART;
   act_disk_cnt = 0;
   // boot from disk
   if (lvme!=LVME_EMPTY && !ddta[cur_disk].ramdisk) AddAction(lst_e, actd_mbrboot);
   if (lvme!=LVME_FLOPPY) {
      // clone disk to another empty one
      if (clone) AddAction(lst_e, actd_clone);
      // init disk/change mbr code
      AddAction(lst_e, lvme==LVME_EMPTY?actd_init:actd_mbrcode);
   }
   if (lvme==LVME_EMPTY) {
      AddAction(lst_e, actd_initgpt);
      AddAction(lst_e, actd_restvhdd);
   }
   // LVM info can be fixed
   if (lvme && !badcode && lvme!=LVME_NOINFO) AddAction(lst_e, actd_updlvm);
   // disk is not empty
   if (lvme!=LVME_EMPTY) {
      AddAction(lst_e, actd_savevhdd);
      AddAction(lst_e, actd_wipe);
   }
   // no LVM info at all
   if (lvme==LVME_NOINFO) AddAction(lst_e, actd_writelvm);

   lbDiskAction->newList(lst_e);

   if (ddta[cur_disk].scan_rc) {
      char* msg = SysApp.GetPTErr(ddta[cur_disk].scan_rc);
      replace_coltxt(&txInfo, msg?msg:"", 1, TXINFO_RED);
      free(msg);
      UpdateActionList(True);
   } else {
      replace_coltxt(&txInfo,"",1);
      UpdateActionList(False);
   }
   // update partition action list
#endif
}

void TDMgrDialog::UpdateActionList(Boolean empty) {
#ifdef __QSINIT__
   lst_a = new TCollection(0,10);
   act_part_cnt = 0;

   if (!empty && cur_disk<disks && cur_part>=0 && cur_part<lbPartList->range 
      && ddta[cur_disk].mb) 
   {
      char ptinfo[384];
      dsk_mapblock *mb = ddta[cur_disk].mb + cur_part;
      ptinfo[0]        = 0;

      if (mb->Type) {
         u32t Index = ddta[cur_disk].mb[cur_part].Index;

         if (ddta[cur_disk].fsname)
            sprintf(ptinfo, "%s", ddta[cur_disk].fsname+32*Index);
         // print "active" for GPT disks
         if (ddta[cur_disk].is_gpt)
            if (ddta[cur_disk].mb[cur_part].Flags&DMAP_ACTIVE) {
               if (ptinfo[0]) strcat(ptinfo, ", ");
               strcat(ptinfo, "Bootable");
            }
         // lvm drive letter
         char drv = ddta[cur_disk].mb[cur_part].DriveLVM;
         if (drv) {
            if (ptinfo[0]) strcat(ptinfo, ", ");
            char *cp = ptinfo + strlen(ptinfo);
            sprintf(cp, "LVM %c:", drv);
         }
         // qsinit drive letter
         drv = ddta[cur_disk].mb[cur_part].DriveQS;
         if (drv) {
            if (ptinfo[0]) strcat(ptinfo, ", ");
            char *cp = ptinfo + strlen(ptinfo);
            if (drv=='0') strcpy(cp, "boot partition"); else
               sprintf(cp, "mounted as %c:/%c:", drv, drv-'0'+'A');
         }
         if (!ddta[cur_disk].ramdisk) AddAction(lst_a, actp_boot);
         // not allowed for boot partition
         if (drv!='0') {
            AddAction(lst_a, actp_delete);
            AddAction(lst_a, actp_format);
         }

         if ((mb->Flags&DMAP_PRIMARY) && (mb->Flags&DMAP_ACTIVE)==0)
            AddAction(lst_a, actp_active);

         if (!drv || drv>'1') 
            AddAction(lst_a, drv?actp_unmount:actp_mount);

         if (mb->Type!=PTE_0A_BOOTMGR && ddta[cur_disk].lvminfo==0) {
            AddAction(lst_a, actp_letter);
            AddAction(lst_a, actp_rename);
         }
         AddAction(lst_a, actp_clone);
         // change type guid on GPT
         if (ddta[cur_disk].is_gpt) AddAction(lst_a, actp_setgptt);
         // not a single dialog mode
         if (!SysApp.bootcmd) AddAction(lst_a, actp_view);
      } else {
         if (mb->Flags&DMAP_PRIMARY) AddAction(lst_a, actp_makepp);
         if (mb->Flags&DMAP_LOGICAL) AddAction(lst_a, actp_makelp);
      }
      replace_coltxt(&txInfo, ptinfo, 1, TXINFO_BLUE);
   }
   lbAction->newList(lst_a);
#endif
}

Boolean TDMgrDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) {
      if (current!=lbDiskAction && current!=lbAction) {
         rslt = False;
         SysApp.errDlg(MSGE_SELACTION);
      } else
      if (current==lbDiskAction) {
         u32t act = lbDiskAction->focused;
         if (act<act_disk_cnt) {
            char cmdbuf[32];

            if (SysApp.CanChangeDisk(cur_disk))
            switch (action_disk[act]) {
               case actd_mbrboot : SysApp.BootPartition(cur_disk,-1); break;
               case actd_clone   : 
                  if (SysApp.CloneDisk(cur_disk)) UpdateAll(True);
                  return False;
               case actd_init    : SysApp.DiskInit(cur_disk, 0); break;
               case actd_initgpt : SysApp.DiskInit(cur_disk, 1); break;
               case actd_mbrcode : SysApp.DiskMBRCode(cur_disk); break;
               case actd_wipe    : 
                  sprintf(cmdbuf, "mbr hd%u wipe", cur_disk);
                  RunCommand(DMGR_CMD, cmdbuf);
                  UpdateAll(True);
                  return False;
               case actd_restvhdd: SysApp.SaveRestVHDD(cur_disk,1); break;
               case actd_savevhdd:
                  SysApp.SaveRestVHDD(cur_disk,0);
                  return False;
               case actd_updlvm  :
               case actd_writelvm:
                  SysApp.UpdateLVM(cur_disk, action_disk[act]==actd_writelvm);
                  break;
            }
         }
         UpdateDiskInfo(cur_disk);
         UpdatePartList();
         rslt = False;
      } else
      if (current==lbAction) {
         Boolean rescan = False;
         u32t       act = lbAction->focused;

         if (act<act_part_cnt) {
#ifdef __QSINIT__
            char cmdbuf[32];
            u32t  Index = ddta[cur_disk].mb[cur_part].Index;

            switch (action_part[act]) {
               case actp_boot    : 
                  if (SysApp.CanChangeDisk(cur_disk)) 
                     SysApp.BootPartition(cur_disk,Index);
                  break;
               case actp_delete  : 
                  if (SysApp.CanChangeDisk(cur_disk)) {
                     sprintf(cmdbuf, "pm hd%u delete %u", cur_disk, Index);
                     RunCommand(DMGR_CMD, cmdbuf);
                     UpdateAll(True);
                  }
                  break;
               case actp_format  :
                  if (SysApp.CanChangeDisk(cur_disk)) {
                     char  qsd = ddta[cur_disk].mb[cur_part].DriveQS;
#if 0
                     int quick = SysApp.askDlg(MSGA_QUICKFMT);
                     sprintf(cmdbuf, "%c: %s", qsd, quick?"/quick":"");
                     RunCommand(FORMAT_CMD, cmdbuf);
#else
                     SysApp.FormatDlg(qsd?qsd-'0':0, cur_disk, Index);
#endif
                     rescan = True;
                  }
                  break;
               case actp_active  : 

                  if (SysApp.CanChangeDisk(cur_disk) && SysApp.askDlg(
                     ddta[cur_disk].is_gpt?MSGA_ACTIVEGPT:MSGA_ACTIVE)) 
                  {
                     u32t rc = ddta[cur_disk].is_gpt? 
                               dsk_gptactive(cur_disk, Index):
                               dsk_setactive(cur_disk, Index);
                     if (rc) SysApp.PrintPTErr(rc);
                        else SysApp.DiskChanged(cur_disk);
                  } else 
                     return False;
                  break;
               case actp_mount   : 
                  SysApp.MountDlg(True, cur_disk, Index);
                  break;
               case actp_unmount : {
                  char qsd = ddta[cur_disk].mb[cur_part].DriveQS;
                  if (qsd>='2') SysApp.Unmount(qsd-'0');
                  break;
               }
               case actp_letter  :
                  SysApp.MountDlg(False, cur_disk, Index, ddta[cur_disk].mb[cur_part].DriveLVM);
                  break;
               case actp_clone   :
                  if (SysApp.ClonePart(cur_disk, Index)) UpdateAll(True);
                  break;
               case actp_makelp  :
               case actp_makepp  :
                  SysApp.CreatePartition(cur_disk, Index, action_part[act]==actp_makelp);
                  break;
               case actp_setgptt :
                  SysApp.SetGPTType(cur_disk, Index);
                  rescan = True;
                  break;
               case actp_rename  :
                  SysApp.LVMRename(cur_disk, Index);
                  rescan = True;
                  break;
               case actp_view    :
                  if (SysApp.CanChangeDisk(cur_disk)) {
                     goToDisk   = cur_disk;
                     goToSector = ddta[cur_disk].mb[cur_part].StartSector;
                     return True;
                  }
                  break;
            }
#endif
         }
         UpdateDiskInfo(cur_disk, rescan);
         UpdatePartList();
         rslt = False;
      }
   }
   return rslt;
}

// ------------------------------------------------------------

TCloneVolDlg::TCloneVolDlg(u32t srcdisk, u32t index) :
   TWalkDiskDialog(TRect(6, 5, 73, 18), "Select target partition"),
   TWindowInit(TCloneVolDlg::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   helpCtx = hcVolClone;

   disk    = srcdisk;
   ptindex = index;

   control = new TScrollBar(TRect(27, 2, 28, 7));
   insert(control);
   lbDisk = new TListBox(TRect(3, 2, 27, 7), 1, (TScrollBar*)control);
   lbDisk->helpCtx = hcVolCloneDisk;
   insert(lbDisk);
   insert(new TLabel(TRect(2, 1, 7, 2), "Disk", lbDisk));

   control = new TScrollBar(TRect(64, 2, 65, 9));
   insert(control);
   lbPartList = new TListBox(TRect(29, 2, 64, 9), 1, (TScrollBar*)control);
   lbPartList->helpCtx = hcVolClonePart;
   insert(lbPartList);
   insert(new TLabel(TRect(28, 1, 38, 2), "Partition", lbPartList));

   control = new TButton(TRect(42, 10, 52, 12), "~G~o", cmOK, bfDefault);
   insert(control);
   control = new TButton(TRect(53, 10, 63, 12), "~H~elp", cmHelp, bfNormal);
   insert(control);

   txFileSys = new TColoredText(TRect(3, 8, 28, 9), "-", 0x7F);
   insert(txFileSys);
   txLName   = new TColoredText(TRect(3, 9, 38, 10), "-", 0x7F);
   insert(txLName);
   txComment = new TColoredText(TRect(3, 10, 38, 11), "-", 0x7E);
   insert(txComment);
#ifdef __QSINIT__
   dsk_ptquery64(srcdisk, index, 0, &ptsize, 0, 0);
#endif
   cur_disk=0; cur_part=0; partFilt=PARTFILT_PT;
   UpdateAll();

   selectNext(False);
}

void TCloneVolDlg::UpdatePartList() {
   TWalkDiskDialog::UpdatePartList();
   NavCurPartChanged(-1);
}

void TCloneVolDlg::NavCurPartChanged(int prevpos) {
   TWalkDiskDialog::NavCurPartChanged(prevpos);
   // clear common info controls by default
   int empty_common = 1;
#ifdef __QSINIT__
   if (cur_disk<disks && cur_part>=0 && cur_part<lbPartList->range 
      && ddta[cur_disk].mb) 
   {
      static const char *hint[7] = { "Looks good",
         "Smaller than requested",
         "Too large (slack space)",
         "Suitable but have data on it",
         "Source partition",
         "Sector size mismatch",
         "Create a partition here first!"
      };
      static const ushort hintcol[7] = { 0x7A, 0x7C, 0x7E, 0x7E, 0x7A, 0x7C, 0x7E };

      int hintnum = 0;

      dsk_mapblock *mb = cur_part>=0 ? ddta[cur_disk].mb + cur_part : 0;
      if (mb->Type) {
         char    str[128],
                  *fsname = mb ? ddta[cur_disk].fsname  + 32*mb->Index : "",
                 *lvmname = mb && ddta[cur_disk].lvmname ? 
                                 ddta[cur_disk].lvmname + 32*mb->Index : "-";
         sprintf(str, "FS  : %s", fsname);
         replace_coltxt(&txFileSys, str, 1);
         sprintf(str, "LVM : %s", lvmname);
         if (mb->DriveLVM) sprintf(str+strlen(str), ", %c:", mb->DriveLVM);
         replace_coltxt(&txLName, str, 1, 0x7F);
         empty_common = 0;

         if (cur_disk==disk && mb->Index==ptindex) hintnum = 4; else
            if (ddta[cur_disk].ssize != ddta[disk].ssize) hintnum = 5; else
               if (mb->Length<ptsize) hintnum = 1; else
                  if (mb->Length>ptsize+ptsize/10) hintnum = 2; else
                     if (*fsname) hintnum = 3;
      } else {
         if (ddta[cur_disk].ssize != ddta[disk].ssize) hintnum = 5; else
            if (mb->Length<ptsize) hintnum = 1; else hintnum = 6;
      }
      replace_coltxt(&txComment, hint[hintnum], 1, hintcol[hintnum]);
   } else
      replace_coltxt(&txComment, "", 1);
#endif
   if (empty_common) {
      replace_coltxt(&txFileSys, "", 1);
      replace_coltxt(&txLName, "", 1);
   }
}

void TCloneVolDlg::handleEvent( TEvent& event) {
   TWalkDiskDialog::handleEvent(event);
}

Boolean TCloneVolDlg::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
#ifdef __QSINIT__
   if (rslt && (command == cmOK)) {
      dsk_mapblock *mb = cur_part>=0 ? ddta[cur_disk].mb + cur_part : 0;

      if (!mb || !mb->Type) rslt = False; else {
         targetDisk  = cur_disk;
         targetIndex = ddta[cur_disk].mb[cur_part].Index;

         if (targetDisk==disk && targetIndex==ptindex) rslt = False;
      }
      if (!rslt) SysApp.errDlg(MSGE_INVVALUE);
   }
#endif
   return rslt;
}
