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

static const char *DMGR_CMD = "DMGR",
                *FORMAT_CMD = "FORMAT";

TDMgrDialog::TDMgrDialog(int largeBox) :
   TDialog(TRect(4, 2, 76, 21+(largeBox?LARGEBOX_INC:0)), "Disk Management"),
   TWindowInit(TDMgrDialog::initFrame)
{
   lst_d=0; lst_p=0; lst_a=0; lst_e=0; disks=0; cur_disk=0; cur_part=-1;
   goToDisk=FFFF; goToSector=FFFF64;
   // add 7 to height if screen lines >=32
   largeBox = largeBox? LARGEBOX_INC : 0;
#ifdef __QSINIT__
   ddta = 0;
#endif
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

   cur_disk = 0; cur_part = 0;
   UpdateAll();
  
   selectNext(False);
}

TDMgrDialog::~TDMgrDialog() {
   if (lst_d) { delete lst_d; lst_d = 0; }
   if (lst_p) { delete lst_p; lst_p = 0; }
   if (lst_a) { delete lst_a; lst_a = 0; }
   if (lst_e) { delete lst_e; lst_e = 0; }
   FreeDiskData();
}

void TDMgrDialog::FreeDiskData() {
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


void TDMgrDialog::UpdateDiskInfo(u32t disk, Boolean rescan) {
#ifdef __QSINIT__
   disk_geo_data geo;
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
   }
#endif
}

long TDMgrDialog::SelectedDisk(long item) {
   char diskname[16], *cp;
   lbDisk->getText(diskname, item>=0?item:lbDisk->focused, 15);
   cp = strchr(diskname, ' ');
   if (cp) return atoi(cp);
   return -1;
}

void TDMgrDialog::FocusToDisk(u32t disk) {
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

void TDMgrDialog::handleEvent( TEvent& event) {
   int dprev = lbDisk->focused,
       pprev = lbPartList->focused;

   TDialog::handleEvent(event);
   switch (event.what) {
      case evCommand:
         if (event.message.command==cmRescan) UpdateAll(True);
         break;
      default:
         break;
   }
   if (dprev!=lbDisk->focused) {
      long dsk = SelectedDisk();
      if (dsk>=0) {
         cur_disk = dsk;
         cur_part = 0;
         UpdatePartList();
      }
   } else
   if (pprev!=lbPartList->focused) {
      cur_part = lbPartList->focused;
#ifdef __QSINIT__
      UpdateActionList(ddta[cur_disk].scan_rc ? True : False);
#else
      UpdatePartList();
#endif
   }
}

void TDMgrDialog::UpdateAll(Boolean rescan) {
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

void TDMgrDialog::AddAction(TCollection *list, u8t action) {
   static const char *text[] = {"Boot", "Init disk", "Init disk (GPT)",
      "Replace MBR code", "Update LVM info", "Wipe disk", "Write LVM info", 
      0,
      "Boot", "Delete", "Format", "Make active", "Mount", "Unmount",
      "LVM drive letter", "LVM rename", "Create logical", "Create primary",
      "Set type GUID", "Sector view"};

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
#ifdef __QSINIT__
   if (cur_disk>=disks || !ddta) return;
   lst_p = new TCollection(0,10);

   if (ddta[cur_disk].mb) {
      dsk_mapblock *mb = ddta[cur_disk].mb;
      u32t     cylsize = ddta[cur_disk].heads * ddta[cur_disk].spt;
      do {
         char *str = new char[64], topic[16];

         if (mb->Type) {
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
                  < cylsize) break;
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

   // update disk action list
   lst_e = new TCollection(0,10);

   u32t lvme = ddta[cur_disk].lvminfo, 
     badcode = lvme==LVME_SERIAL || lvme==LVME_GEOMETRY || lvme==LVME_EMPTY ||
               lvme==LVME_FLOPPY || lvme==LVME_GPTDISK || lvme==LVME_LOWPART;
   act_disk_cnt = 0;
   // boot from disk
   if (lvme!=LVME_EMPTY) AddAction(lst_e, actd_mbrboot);
   // init disk/change mbr code
   AddAction(lst_e, lvme==LVME_EMPTY?actd_init:actd_mbrcode);
   if (lvme==LVME_EMPTY) AddAction(lst_e, actd_initgpt);
   // LVM info can be fixed
   if (lvme && !badcode && lvme!=LVME_NOINFO) AddAction(lst_e, actd_updlvm);
   // disk is not empty
   if (lvme!=LVME_EMPTY) AddAction(lst_e, actd_wipe);
   // no LBVM info at all
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
   if (!empty && cur_disk<disks && cur_part>=0 && cur_part<lbPartList->range 
      && ddta[cur_disk].mb) 
   {
      char ptinfo[384];
      dsk_mapblock *mb = ddta[cur_disk].mb + cur_part;
      ptinfo[0]        = 0;
      act_part_cnt     = 0;

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

         AddAction(lst_a, actp_boot);
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
               case actd_init    : SysApp.DiskInit(cur_disk, 0); break;
               case actd_initgpt : SysApp.DiskInit(cur_disk, 1); break;
               case actd_mbrcode : SysApp.DiskMBRCode(cur_disk); break;
               case actd_wipe    : 
                  sprintf(cmdbuf, "mbr hd%u wipe", cur_disk);
                  RunCommand(DMGR_CMD, cmdbuf);
                  UpdateAll(True);
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
