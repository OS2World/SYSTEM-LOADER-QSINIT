#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TDeskTop
#include <tv.h>
#include <stdlib.h>
#include "diskedit.h"
#ifdef __QSINIT__
#include "qsdm.h"
#endif

extern TListBox     *SelDiskList;
extern TCheckBoxes *ChUseOwnBoot;

#define disk_id_from_idx(ii) (ii<vdsk_cnt?DSK_VIRTUAL|ii^1:  \
    (ii<fdd_cnt+vdsk_cnt?DSK_FLOPPY|ii-vdsk_cnt:ii-fdd_cnt-vdsk_cnt))

u32t TSysApp::GetDiskDialog(int floppies, int vdisks, int dlgtype) {
   TCollection *dl = new TCollection(0,10);
   TDialog  *seldd = dlgtype==SELDISK_BOOT?makeSelBootDiskDlg():
                                           makeSelDiskDlg(dlgtype-1);
   u32t    fdd_cnt, ssize, vdsk_cnt,
           hdd_cnt = dsk_count(&fdd_cnt,&vdsk_cnt);
   int          ii;
   if (!floppies) fdd_cnt = 0;
   if (!vdisks) vdsk_cnt = 0;

   for (ii=0; ii<(int)(vdsk_cnt+fdd_cnt+hdd_cnt);) {
      u32t disk = disk_id_from_idx(ii);
      u64t size = dsk_size64(disk, &ssize);

      char *str = new char[128];
      if (ii<vdsk_cnt) {
         if (!size) { vdsk_cnt--; continue; } else
         switch (ii) {
            case 0: strcpy(str,"Virtual disk         "); break;
            case 1: strcpy(str,"Boot partition       "); break;
         }
      } else
      if (ii<vdsk_cnt+fdd_cnt) {
         if (!size) { fdd_cnt--; continue; }
         sprintf(str,"Floppy disk %i        ",ii-vdsk_cnt);
      } else {
         if (!size) { hdd_cnt--; continue; }
         sprintf(str,"HDD %i                ",ii-fdd_cnt-vdsk_cnt);
      }
      strcat(str,getSizeStr(ssize,size));
      dl->insert(str);

      ii++;
   }
   SelDiskList->newList(dl);

   int res = execView(seldd)==cmOK,
       idx = SelDiskList->focused;

   nextBootOwnMbr = dlgtype==SELDISK_BOOT?ChUseOwnBoot->mark(0):0;
   destroy(seldd);
   destroy(dl);
   return res&&idx>=0?disk_id_from_idx(idx):FFFF;
}

int TSysApp::CanChangeDisk(u32t disk) {
   TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
   if (win)
      if (win->disk==disk)
         if (win->sectEd->isChanged()) {
            win->sectEd->commitChanges();
            if (win->sectEd->isChanged()) return 0;
         }
   return 1;
}

void TSysApp::DiskChanged(u32t disk) {
   TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
   if (win)
      if (win->disk==disk || disk==FFFF) win->sectEd->dataChanged();
}

static int  se_read(int userdata, le_cluster_t cluster, void *data) {
   return dsk_read(userdata, cluster, 1, data);
}

static int  se_write(int userdata, le_cluster_t cluster, void *data) {
   return dsk_write(userdata, cluster, 1, data);
}

static void se_gettitle(int userdata, le_cluster_t cluster, char *title) {
   dsk_geo_data geo;
   memset(&geo, 0, sizeof(geo));
   dsk_size(userdata, 0, &geo);

   if (geo.Cylinders && cluster<=FFFF) {
      u32t cylsize = geo.Heads * geo.SectOnTrack,
               cyl = cluster/cylsize,
              head = cluster%cylsize/geo.SectOnTrack;
      sprintf(title, "Sector %Lu (Cyl %d Head %d Sector %d)", cluster, cyl,
         head, cluster%geo.SectOnTrack + 1);
   } else
      sprintf(title, "Sector %Lu", cluster);
}

static int se_askupdate(int userdata, le_cluster_t cluster, void *data) {
   char msg[256];
   sprintf(msg,"\x03""Write data to sector %Lu?\n", cluster);
   int rc = messageBox(msg,mfConfirmation+mfYesNoCancel);
   if (rc==cmCancel) return -1;
   return rc==cmYes?1:0;
}

void TSysApp::OpenPosDiskWindow(ul disk, uq sector) {
   char title[128];

   if (windows[TWin_SectEdit]) windows[TWin_SectEdit]->close();
   if (windows[TWin_SectEdit]) return;

   strcpy(title,"Sector view ");

   if (disk & DSK_FLOPPY) {
      sprintf(title+strlen(title), "(Floppy disk %i)", disk&DSK_DISKMASK);
   } else
   if (disk == DSK_VIRTUAL) strcat(title,"(Boot partition)");
     else
   if (disk == DSK_VIRTUAL+1) strcat(title,"(Virtual disk)"); else {
      sprintf(title+strlen(title), "(HDD %i)", disk&DSK_DISKMASK);
   }

   TRect          rr = deskTop->getExtent();
   TDskEdWindow *win = new TDskEdWindow(rr, title);

   if (win && application->validView(win)) {
      rr = TRect(1, 1, win->size.x-1, win->size.y-1);
      win->options |= ofTileable;

      win->sectEd = new TLongEditor(rr);
      win->disk   = disk;
      win->insert(win->sectEd);
      deskTop->insert(win);

      if (win->sectEd) {
         win->led.clusters   = dsk_size64(disk, &win->led.clustersize);
         win->led.showtitles = 1;
         win->led.noreaderr  = 0;
         win->led.userdata   = disk;
         win->led.read       = se_read;
         win->led.write      = se_write;
         win->led.gettitle   = se_gettitle;
         win->led.askupdate  = se_askupdate;
         win->sectEd->setData(&win->led);
         win->sectEd->processKey(TLongEditor::posSet, sector);
      }
   }
}

void TSysApp::DiskInit(u32t dsk, int makegpt) {
   if (CanChangeDisk(dsk)) {
      u64t sz = dsk_size64(dsk, 0);
#ifdef __QSINIT__
      u32t rc = dsk_ptrescan(dsk, 0);
      
      if (rc==DPTE_ERRREAD) errDlg(MSGE_MBRREAD); else
      if (rc!=DPTE_EMPTY) errDlg(MSGE_NOTEMPTY); else {
         if (makegpt) {
            rc = dsk_gptinit(dsk);
         } else {
            int islvm = askDlg(sz>63*255*65535?MSGA_LVM512:MSGA_LVMSMALL)?1:0;
            // ask about secondary HDD boot disk serial
            if (dsk && islvm && !askDlg(MSGA_PRIMARY)) islvm = 2;
            // create disk
            rc = dsk_ptinit(dsk, islvm);
         }
         if (!rc) {
            DiskChanged(dsk);
            infoDlg(MSGI_DONE);
         } else {
            if (rc==DPTE_EMPTY) errDlg(MSGE_NOTEMPTY); else
            if (rc==DPTE_ERRWRITE) errDlg(MSGE_MBRWRITE);
         }
      }
#endif
   }
}

void TSysApp::DiskMBRCode(u32t dsk) {
   if (CanChangeDisk(dsk)) {
      if (dsk==DSK_VIRTUAL) errDlg(MSGE_BOOTPART); else
      if ((dsk&DSK_FLOPPY)!=0) errDlg(MSGE_FLOPPY); else
      {
         if (askDlg(MSGA_MBRCODE, dsk)) {
#ifndef __QSINIT__
            int      ok = 0;
#else
            int puregpt = dsk_isgpt(dsk,-1)==1,
                     ok = dsk_newmbr(dsk, puregpt?DSKBR_GPTCODE:0);
#endif
            if (!ok) errDlg(MSGE_MBRWRITE); else {
               DiskChanged(dsk);
               infoDlg(MSGI_DONE);
            }
         }
      }
   }
}

void TSysApp::UpdateLVM(u32t disk, int firstTime) {
   if (CanChangeDisk(disk)) {
#ifdef __QSINIT__
      u32t lvmrc = lvm_checkinfo(disk);
      if (!lvmrc) infoDlg(MSGI_LVMOK); else {
         int  sep = firstTime? 0 : -1;

         if (!firstTime) {
            char *msg = GetPTErr(lvmrc, MSGTYPE_LVM), buf[128];
            snprintf(buf, 128, "\3""Current LVM error: %s\n\3""Update LVM info?", msg);
            free(msg);
            if (messageBox(buf, mfConfirmation+mfYesButton+mfNoButton)!=cmYes) return;
         } else {
            if (!askDlg(MSGA_LVMSMALL)) return;
            // check 500Gb limit
            if (hlp_disksize(disk,0,0) > 63*255*65535 && dsk_partcnt(disk)>0)
               if (!askDlg(MSGA_LVM512EXIST)) return;
            // ask about secondary HDD boot disk serial
            if (disk && !askDlg(MSGA_PRIMARY)) sep = 1;
         }
         lvmrc = lvm_initdisk(disk, 0, sep);
         DiskChanged(disk);
         if (lvmrc) PrintPTErr(lvmrc, MSGTYPE_LVM); else infoDlg(MSGI_DONE);
      }
#endif
   }
}