#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TDeskTop
#include <tv.h>
#include <stdlib.h>
#include <io.h>
#include <stdio.h>
#include "diskedit.h"
#ifdef __QSINIT__
#include "qsio.h"
#endif

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
   TMemEdWindow *win = (TMemEdWindow*)windows[AppW_Memory];

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
            /* allow full 4Gb view to browse device space in FLAT
               mode */
            if (memend<_4GBLL) memend = _4GBLL;
            memend    >>= 8;
            win->led.clusters    = memend;
            win->led.clustersize = 256;
            win->led.lcbytes     = win->led.clustersize;
            win->led.poswidth    = memend>_4GBLL*16?10:0; // forces 10 digits on >64Gb of memory ;)
            win->led.showtitles  = 0;
            win->led.userdata    = 0;
            win->led.noreaderr   = 1;
            win->led.read        = mem_read;
            win->led.write       = mem_write;
            win->led.gettitle    = mem_gettitle;
            win->led.askupdate   = mem_askupdate;
            win->led.setsize     = 0;
            win->memEd->setData(&win->led);
         }
      }
   }
   if (win && application->validView(win) && win->memEd)
      win->memEd->posToCluster(position>>8, position&0xFF);
}

static TInputLine *FillMemPosDlg(TDialog* dlg, int epos = 0) {
   TInputLine   *sgMemPos = new TInputLine(TRect(9, 2, 27, 3), 11);
   sgMemPos->helpCtx = hcMemPos;
   dlg->insert(sgMemPos);
   dlg->insert(new THistory(TRect(27, 2, 30, 3), sgMemPos, hhMemGoto));

   u64t memend = 0;
   opts_getpcmem(&memend);
   char lstr[32];
   sprintf(lstr, "%010LX", memend);
   
   dlg->insert(new TColoredText(TRect(24, 4+epos, 41, 5+epos), lstr, 0x78));
   dlg->insert(new TColoredText(TRect(3, 4+epos, 24, 5+epos),
      "End of memory (hex): ", 0x78));
   return sgMemPos;
}

void TSysApp::GotoMemDlg() {
   TDialog* dlg = new TDialog(TRect(18, 8, 61, 14), "Jump to memory");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;

   TInputLine *sgMemPos = FillMemPosDlg(dlg);
   
   dlg->insert(new TLabel(TRect(2, 2, 7, 3), "Goto", sgMemPos));
   dlg->insert(new TButton(TRect(31, 2, 41, 4), "~G~oto", cmOK, bfDefault));
 
   dlg->selectNext(False);

   if (execView(dlg)==cmOK) {
      uq pos = strtoull(getstr(sgMemPos), 0, 16);
      OpenPosMemWindow(pos);
   }
   destroy(dlg);
}

void TSysApp::MemSaveDlg() {
   TDialog* dlg = new TDialog(TRect(18, 7, 61, 15), "Save memory block");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;

   TInputLine *sgMemPos = FillMemPosDlg(dlg,2),
              *sgMemLen = new TInputLine(TRect(9, 4, 27, 5), 18);
   sgMemLen->helpCtx = hcMemLen;
   dlg->insert(sgMemLen);
   dlg->insert(new THistory(TRect(27, 4, 30, 5), sgMemLen, hhMemSaveLen));
   dlg->insert(new TLabel(TRect(2, 2, 8, 3), "Start", sgMemPos));
   dlg->insert(new TLabel(TRect(2, 4, 7, 5), "Size",  sgMemLen));
   //TView         *control;
   dlg->insert(new TButton(TRect(31, 2, 41, 4), "~S~ave", cmOK, bfDefault));

   dlg->selectNext(False);

   if (execView(dlg)==cmOK) {
      uq      pos = strtoull(getstr(sgMemPos), 0, 16), len,
           memend = 0;
      char *lplen = getstr(sgMemLen);
      // prevent octal
      while (lplen[0]=='0' && lplen[1]=='0') lplen++;
      len = strtoull(lplen,0,0);

      opts_getpcmem(&memend);
      if (pos>=memend || pos+len<pos) errDlg(MSGE_RANGE); else {
         u32t  devpos = opts_memend();
         int       ok = 1;
         // just trunc by the end of memory without any warning
         if (pos+len>memend) len = memend-pos;

         if (pos<_4GBLL && pos+len>devpos)
            if (!askDlg(MSGA_DEVICEMEM)) ok = 0;
         if (ok) {
            char *fn = getfname(0);
            if (fn) {
               char mbtext[144];
               fn = strdup(fn);
               snprintf(mbtext, 144, "\x03""Save range %08LX..%08LX to "
                                     "\"%s\"?\n", pos, pos+len-1, fn);
               ok = messageBox(mbtext, mfConfirmation+mfYesButton+mfNoButton)==cmYes;
               if (ok) doMemSave(pos, len, fn);
               free(fn);
            }
         }

      //if (pos+len>=memend)
      }

   }
   destroy(dlg);
}

void TSysApp::doMemSave(u64t addr, u64t len, const char *fpath) {
   FILE *fp = fopen(fpath, "wb");
   if (fp) {
#ifdef __QSINIT__
      io_handle_info   hi;
      disk_volume_data vi;
      // check target volume
      if (!io_pathinfo(fpath,&hi))
         if (!io_volinfo(hi.vol, &vi)) {
            int err = 1;
            if (len > (u64t)vi.ClAvail*vi.SectorSize*vi.ClSize) errDlg(MSGE_LOWSPACE);
               else
            if (len > vi.FSizeLim) errDlg(MSGE_GT4GB);
               else err = 0;
            if (err) {
               fclose(fp);
               ::remove(fpath);
               return;
            }
         }
#endif // __QSINIT__
      copyDlg = new TDiskCopyDialog();
      copyDlg->startMemToFile(addr, fp, len);
      execView(copyDlg);
      int  stop = copyDlg->stopreason;
      u64t epos = stop==1 ? copyDlg->source.pos : _telli64(fileno(fp));
      fclose(fp);
      destroy(copyDlg);
      copyDlg = 0;

      if (stop>0)  {
         // pos in editor window after memory read error
         if (stop==1) {
            TMemEdWindow *win = (TMemEdWindow*)windows[AppW_Memory];
            if (win) {
               win->select();
               win->memEd->posToCluster(epos>>8, epos&0xFF);
            }
         }
         char mbtext[96];
         snprintf(mbtext, 96, "\x03""%s error at pos 0x%09X",
            stop==1?"Memory read":"File write", epos);
         messageBox(mbtext, mfError+mfOKButton);
      }
   } else
      errDlg(MSGE_FILEOPENERR);
}
