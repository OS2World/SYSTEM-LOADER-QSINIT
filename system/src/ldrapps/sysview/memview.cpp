#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TDeskTop
#include <tv.h>
#include <stdlib.h>
#include <stdio.h>
#include "diskedit.h"

int mem_read(int userdata, le_cluster_t cluster, void *data) {
   return opts_memread(cluster<<8, data);
}

static int  mem_write(int userdata, le_cluster_t cluster, void *data) {
   return opts_memwrite(cluster<<8, data);
}

static void mem_gettitle(int userdata, le_cluster_t cluster, char *title) {
   sprintf(title, "Memory at %09LX", cluster<<8);
}

static int mem_askupdate(int userdata, le_cluster_t cluster, void *data) {
   char msg[256];
   sprintf(msg,"\x03""Update 256 bytes at position %09LX?\n", cluster<<8);
   int rc = messageBox(msg,mfConfirmation+mfYesNoCancel);
   if (rc==cmCancel) return -1;
   return rc==cmYes?1:0;
}

void TSysApp::OpenPosMemWindow(uq position) {
   TMemEdWindow *win = (TMemEdWindow*)windows[TWin_MemEdit];

   if (!win) {
      TRect rr = deskTop->getExtent();
      win = new TMemEdWindow(rr, "Memory view");
   
      if (win && application->validView(win)) {
         rr = TRect(1, 1, win->size.x-1, win->size.y-1);
         win->options |= ofTileable;
      
         win->memEd = new TLongEditor(rr);
         win->insert(win->memEd);
         deskTop->insert(win);
      
         if (win->memEd) {
            u64t memend = 0;
            opts_getpcmem(&memend);
            memend    >>= 8;
            win->led.clusters    = memend;
            win->led.clustersize = 256;
            win->led.showtitles  = 0;
            win->led.userdata    = 0;
            win->led.noreaderr   = 1;
            win->led.read        = mem_read;
            win->led.write       = mem_write;
            win->led.gettitle    = mem_gettitle;
            win->led.askupdate   = mem_askupdate;
            win->memEd->setData(&win->led);
         }
      }
   }
   if (win && application->validView(win) && win->memEd)
      win->memEd->posToCluster(position>>8, position&0xFF);
}


void TSysApp::GotoMemDlg() {
   TInputLine   *sgMemPos;
   TColoredText *sgMemMax;
   TView         *control;
   
   TDialog* dlg = new TDialog(TRect(18, 8, 61, 14), "Jump to memory");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;
   
   sgMemPos = new TInputLine(TRect(9, 2, 27, 3), 11);
   dlg->insert(sgMemPos);
   
   dlg->insert(new THistory(TRect(27, 2, 30, 3), (TInputLine*)sgMemPos, hhMemGoto));
   
   dlg->insert(new TLabel(TRect(2, 2, 7, 3), "Goto", sgMemPos));
   
   control = new TButton(TRect(31, 2, 41, 4), "~G~oto", cmOK, bfDefault);
   dlg->insert(control);

   u64t memend = 0;
   opts_getpcmem(&memend);
   char lstr[32];
   sprintf(lstr, "%010LX", memend);
   
   sgMemMax = new TColoredText(TRect(24, 4, 41, 5), lstr, 0x78);
   dlg->insert(sgMemMax);
   
   control = new TColoredText(TRect(3, 4, 24, 5), "End of memory (hex): ", 0x78);
   dlg->insert(control);
   
   dlg->selectNext(False);

   if (execView(dlg)==cmOK) {
      uq pos = strtoull(getstr(sgMemPos), 0, 16);
      OpenPosMemWindow(pos);
   }
   destroy(dlg);
}
