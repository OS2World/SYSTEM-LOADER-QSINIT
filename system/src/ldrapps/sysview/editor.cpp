#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TDeskTop
#define Uses_TFileEditor
#define Uses_TEditWindow
#define Uses_TFileDialog
#include <tv.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "diskedit.h"
#if defined(__WATCOMC__) && !defined(__QSINIT__)
#include <share.h>
#endif
#include <io.h>
#ifdef __QSINIT__
#include <qsio.h>
#endif

ushort execDialog(TDialog *d, void *data) {
   TView *p = TProgram::application->validView(d);
   if (p==0) return cmCancel; else {
      if (data!=0) p->setData(data);

      ushort result = TProgram::deskTop->execView(p);
      if (result!=cmCancel && data!=0) p->getData(data);
      TObject::destroy(p);

      return result;
   }
}

TDialog *createFindDialog() {
   TDialog *dlg = new TDialog(TRect(0,0,38,12), "Find");
   dlg->options |= ofCentered;

   TInputLine *control = new TInputLine(TRect(3,3,32,4), 80);
   dlg->insert(control);
   dlg->insert(new TLabel(TRect(2,2,15,3), "~T~ext to find", control));
   dlg->insert(new THistory(TRect(32,3,35,4), control, 10));

   dlg->insert(new TCheckBoxes(TRect(3,5,35,7), new TSItem("~C~ase sensitive",
      new TSItem("~W~hole words only",0))));
   dlg->insert(new TButton(TRect(14,9,24,11), "O~K~", cmOK, bfDefault));
   dlg->insert(new TButton(TRect(26,9,36,11), "Cancel", cmCancel, bfNormal));

   dlg->selectNext( False );
   return dlg;
}

TDialog *createReplaceDialog() {
   TDialog *dlg = new TDialog(TRect(0,0,40,16), "Replace");
   dlg->options |= ofCentered;

   TInputLine *control = new TInputLine(TRect(3,3,34,4), 80);
   dlg->insert(control);
   dlg->insert(new TLabel(TRect(2,2,15,3), "~T~ext to find", control));
   dlg->insert(new THistory(TRect(34,3,37,4), control, 10));

   control = new TInputLine(TRect(3,6,34,7), 80);
   dlg->insert(control);
   dlg->insert(new TLabel(TRect(2,5,12,6), "~N~ew text", control));
   dlg->insert(new THistory(TRect(34,6,37,7), control,11));

   dlg->insert(new TCheckBoxes(TRect(3,8,37,12), new TSItem("~C~ase sensitive",
      new TSItem("~W~hole words only", new TSItem("~P~rompt on replace",
         new TSItem("~R~eplace all", 0))))));
   dlg->insert(new TButton(TRect(17,13,27,15), "O~K~", cmOK, bfDefault));
   dlg->insert(new TButton(TRect(28,13,38,15), "Cancel", cmCancel, bfNormal));

   dlg->selectNext( False );
   return dlg;
}

ushort doEditDialog(int dialog,...) {
   va_list arg;
   char buf[128];
   switch (dialog) {
      case edOutOfMemory:
         return messageBox("\x3""Not enough memory for this operation",
                             mfError|mfOKButton);
      case edReadError: {
         va_start(arg,dialog);
         snprintf(buf, 128, "\x3""Error reading file %s.",va_arg(arg,pvoid));
         va_end(arg);
         return messageBox(buf, mfError|mfOKButton);
      }
      case edWriteError: {
         va_start( arg, dialog );
         snprintf(buf, 128, "\x3""Error writing file %s.",va_arg(arg,char*));
         va_end( arg );
         return messageBox(buf, mfError|mfOKButton);
      }
      case edCreateError: {
         va_start( arg, dialog );
         snprintf(buf, 128, "\x3""Error creating file %s.",va_arg(arg,char*));
         va_end( arg );
         return messageBox(buf, mfError|mfOKButton);
      }
      case edSaveModify: {
         va_start( arg, dialog );
         snprintf(buf, 128, "\x3""%s has been modified. Save?",va_arg(arg,char*));
         va_end( arg );
         return messageBox(buf, mfInformation|mfYesNoCancel);
      }
      case edSaveUntitled:
         return messageBox("\x3""Save untitled file?",mfInformation|mfYesNoCancel);
      case edSaveAs: {
         va_start( arg, dialog );
         TFileDialog *dlg = new TFileDialog("*.*","Save file as","~N~ame",
                                            fdOKButton,hhEditorSaveAs);
         dlg->helpCtx = hcFileDlgCommon;
         return execDialog(dlg,va_arg(arg,char*));
      }
      case edFind: {
         va_start( arg, dialog );
         return execDialog(createFindDialog(),va_arg(arg,char*));
      }
      case edSearchFailed:
         return messageBox("\x3""Search string not found.",mfError|mfOKButton);
      case edReplace: {
         va_start( arg, dialog );
         return execDialog(createReplaceDialog(),va_arg(arg,char*));
      }
      case edReplacePrompt: {
         //  Avoid placing the dialog on the same line as the cursor
         TRect r( 0, 1, 40, 8 );
         r.move( (TProgram::deskTop->size.x-r.b.x)/2, 0 );
         TPoint t = TProgram::deskTop->makeGlobal( r.b );
         t.y++;
         va_start( arg, dialog );
         TPoint *pt = va_arg(arg, TPoint*);
         if( pt->y <= t.y )
             r.move( 0, TProgram::deskTop->size.y - r.b.y - 2 );
         va_end( arg );
         return messageBoxRect(r,"\x3""Replace this occurence?",mfYesNoCancel|mfInformation);
      }
   }
   return 0;
}

TEditWindow* TSysApp::OpenEditor(int NewFile, int hide, const char *fn) {
   char fname[MAXPATH+1];

   if (!NewFile) {
      if (fn) {
         strncpy(fname, fn, MAXPATH);
         fname[MAXPATH] = 0;
      } else {
         TFileDialog *fo = new TFileDialog("*.*", "Open file", "~F~ile name",
                                           fdOpenButton, hhEditorOpen);
         fo->helpCtx = hcFileDlgCommon;
         int     res = execView(fo)==cmFileOpen;
         if (res) fo->getFileName(fname);
         destroy(fo);
         if (!res) return 0;
      }
      // check for a second instance (at least here)
      char    *fp = _fullpath(0, fname, 0);
      TWindow *cw = FindEditorWindow(fp);
      free(fp);

      if (cw) {
         cw->select();
         errDlg(MSGE_FILEOPENED);
         return 0;
      }
   }
   TEditor::editorDialog = doEditDialog;

   TRect         rr = deskTop->getExtent();
   TEditWindow *win = new TEditWindow(rr, NewFile?0:fname, allocWinNumber());

   if (win && application->validView(win)) {
      if (hide) win->hide();
      deskTop->insert(win);
      ToggleEditorCommands();
   }
   return win;
}

void TSysApp::ToggleEditorCommands(int binedit, int on) {
   // no windows on desktop - just autodisable in any case
   if (deskTop->current==0) {
      binedit = -1;
      on      = 0; 
   } else {
      TAppWindowType wt;
      if (!IsSysWindows(deskTop->current,&wt)) {
         if (binedit==-1) binedit = 0; 
         if (on==-1) on = 1;
      } else
      if (wt==AppW_BinFile) {
         if (binedit==-1) binedit = 1;
         if (on==-1) on = 1;
      } else
      if (on==-1) on = 0;
   }
   TCommandSet cmd;
   cmd+=cmSave;
   if (binedit==0 || binedit==-1 && on==0) {
      cmd+=cmSaveAs;
      cmd+=cmFind;  cmd+=cmReplace; cmd+=cmSearchAgain; cmd+=cmUndo;
      cmd+=cmCut;   cmd+=cmCopy;    cmd+=cmPaste;       cmd+=cmClear;
   }
   if (on) enableCommands(cmd); else disableCommands(cmd);
}

void TSysApp::SaveLog(Boolean TimeMark) {
   TFileDialog *dlg;
   time_t       now = time(0);
   struct tm    tme = *localtime(&now);
   char     namebuf[MAXPATH+1];

   snprintf(namebuf, MAXPATH+1, "%02d%02d%02d_%02d%02d.log", tme.tm_year%100,
      tme.tm_mon + 1, tme.tm_mday, tme.tm_hour, tme.tm_min);

   dlg = new TFileDialog("*.*", "Save system log to file", "~N~ame",
                         fdOKButton, hhLogFile);
   dlg->helpCtx = hcFileDlgCommon;

   if (execDialog(dlg,namebuf)!=cmCancel) {
      char  *log = opts_getlog(TimeMark,True);
      int   llen = strlen(log),
             err = 0;
      FILE *fout = fopen(namebuf,"wb");
      if (!fout) doEditDialog(edCreateError,namebuf); else {
         if (fwrite(log,1,llen,fout)!=llen)
            doEditDialog(edWriteError,namebuf);
         fclose(fout);
      }
      free(log);
   }
}

void THexEdWindow::handleEvent(TEvent &event) {
   TAppWindow::handleEvent(event);
   if (event.what==evCommand)
      if (event.message.command==cmSave) {
         hexEd->processKey(TLongEditor::posNullOp);
         clearEvent(event);
      } else
      if (event.message.command==cmBinTrunc) {
         hexEd->truncateData();
         clearEvent(event);
      }
}

void THexEdWindow::shutDown() {
   TAppWindow::shutDown();
   if (srcFile) { fclose(srcFile); srcFile=0; }
   if (srcPath) { free(srcPath); srcPath=0; }
}

void THexEdWindow::GotoFilePosDlg() {
   TDialog* dlg = new TDialog(TRect(18, 8, 61, 14), "Jump to offset");
   if (!dlg) return;
   dlg->options|= ofCenterX | ofCenterY;
   dlg->helpCtx = hcGoto;

   TInputLine *sgMemPos = new TInputLine(TRect(9, 2, 27, 3), 11);
   sgMemPos->helpCtx = hcBinFilePos;
   dlg->insert(sgMemPos);
   dlg->insert(new THistory(TRect(27, 2, 30, 3), sgMemPos, hhBinFileGoto));

   TLongEditorData hi;
   hexEd->getData(&hi);
   // current file size
   u64t sz = hi.clusters*hi.clustersize;
   if (hi.lcbytes)
      sz = sz - (sz?hi.clustersize:0) + hi.lcbytes;
   char lstr[32];
   sprintf(lstr, "%010LX", sz-1);
   
   dlg->insert(new TColoredText(TRect(22, 4, 39, 5), lstr, 0x78));
   dlg->insert(new TColoredText(TRect(3, 4, 22, 5),
      "End of file (hex): ", 0x78));
   
   dlg->insert(new TLabel(TRect(2, 2, 7, 3), "Goto", sgMemPos));
   dlg->insert(new TButton(TRect(31, 2, 41, 4), "~G~oto", cmOK, bfDefault));
 
   dlg->selectNext(False);

   if (SysApp.execView(dlg)==cmOK) {
      u64t pos = strtoull(getstr(sgMemPos), 0, 16);
      if (pos<=sz) hexEd->posToCluster(pos>>8, pos&0xFF);
         else SysApp.errDlg(MSGE_RANGE);
   }
   destroy(dlg);
}

static int binfile_rw(int wr, int userdata, le_cluster_t cluster, void *data) {
   THexEdWindow *wn = (THexEdWindow*)userdata;
   u64t    fullsize = wn->hexEd->fullSize(),
             clsize = fullsize>>wn->bshift;
   u32t     recsize = 1<<wn->bshift;
   if (fullsize & recsize-1) clsize++;
   if (clsize-1==cluster) recsize = fullsize & recsize-1;

   if (opts_fseek(wn->srcFile, cluster<<wn->bshift, SEEK_SET)) return 0;

   u32t res = (wr?fwrite(data, 1, recsize, wn->srcFile):
                  fread (data, 1, recsize, wn->srcFile))==recsize;
   if (res && wr) {
      fflush(wn->srcFile);
      // update file size when last cluster changed
      if (clsize-1==cluster) opts_fsetsize(wn->srcFile, fullsize);
   }
   return res;
}


static int binfile_read(int userdata, le_cluster_t cluster, void *data) {
   return binfile_rw(0, userdata, cluster, data);
}

static int binfile_write(int userdata, le_cluster_t cluster, void *data) {
   return binfile_rw(1, userdata, cluster, data);
}

static void binfile_gettitle(int userdata, le_cluster_t cluster, char *title) {
   THexEdWindow *wn = (THexEdWindow*)userdata;

   sprintf(title, "%09LX", cluster<<wn->bshift);
}

static int binfile_setsize(int userdata, le_cluster_t clusters, unsigned long last) {
   THexEdWindow *wn = (THexEdWindow*)userdata;

   s64t csize = wn->hexEd->fullSize();
   u64t nsize = ((clusters?clusters-1:0)<<wn->bshift) + last;

   if (csize<0) return 0;
   if (nsize==csize+1) return 1;
   if (nsize<csize) {
      char msg[144];
      sprintf(msg,"\x03""Truncate file \"%s\" at position %09LX (%Lu)?\n", 
         getsname(wn->srcPath), nsize, nsize);
      
      int rc = messageBox(msg,mfConfirmation+mfOKCancel);
      if (rc==cmOK) {
         if (opts_fsetsize(wn->srcFile,nsize)==0) {
            SysApp.errDlg(MSGE_FILETRUNCERR);
            return 0;
         }
         return 1;
      }
   }
   return 0;
}

static int binfile_askupdate(int userdata, le_cluster_t cluster, void *data) {
   char    msg[144];
   THexEdWindow *wn = (THexEdWindow*)userdata;

   sprintf(msg,"\x03""Update %u bytes at position %09LX or file \"%s\" ?\n",
      1<<wn->bshift, cluster<<wn->bshift, getsname(wn->srcPath));

   int rc = messageBox(msg,mfConfirmation+mfYesNoCancel);
   if (rc==cmCancel) return -1;
   return rc==cmYes?1:0;
}

struct fninfo {
   TWindow    *win;
   char  *filepath;
};

static void enum_findname(TView *win, void *arg) {
   fninfo *rp = (fninfo*)arg;
   if ((win->options&ofSelectable)!=0 && win->getState(sfVisible)) {
      TAppWindowType wt;
      if (!SysApp.IsSysWindows(win,&wt)) {
         TEditWindow *wn = (TEditWindow*)win;
         char        *fp = _fullpath(0, wn->editor->fileName, 0);
         if (stricmp(rp->filepath, fp)==0) rp->win = wn;
         free(fp);
      } else
      if (wt==AppW_BinFile) {
         THexEdWindow *wn = (THexEdWindow*)win;
         if (wn->srcPath)
            if (stricmp(rp->filepath, wn->srcPath)==0) rp->win = wn;
      }
   }
}

TWindow* TSysApp::FindEditorWindow(const char *fullp) {
   fninfo fi = { 0, (char*)fullp };
   TProgram::deskTop->forEach(enum_findname, &fi);
   return fi.win;
}

THexEdWindow* TSysApp::OpenHexEditor(int NewFile, const char *fn) {
   char fname[MAXPATH+1];

   if (fn) {
      strncpy(fname, fn, MAXPATH);
      fname[MAXPATH] = 0;
   } else {
      TFileDialog *fo = new TFileDialog("*.*", NewFile?"Create binary file":
         "Open binary file", "~F~ile name", NewFile?fdReplaceButton:fdOpenButton,
            hhBinEditOpen);
      if (NewFile) fo->setData("");
      fo->helpCtx = hcFileDlgCommon;
      int     res = execView(fo)==(NewFile?cmFileReplace:cmFileOpen);
      if (res) fo->getFileName(fname);
      destroy(fo);
      if (!res) return 0;
   }
   char        *srcp = _fullpath(0, fname, 0);
   TWindow       *rw = FindEditorWindow(srcp);
   THexEdWindow *win = 0;
   FILE        *srcf = 0;

   do {
      if (rw) {
         rw->select();
         errDlg(MSGE_FILEOPENED);
         break;
      }
      // it is exists?
      if (NewFile) {
         int fe = opts_fileexist(fname);
         if (fe==2) { errDlg(MSGE_DIRNAME); break; }
         if (fe==1) {
            char  msg[80];
            snprintf(msg, 80, "\3""File \"%s\" already exists. Replace it?",
               getsname(fname));
            int arc = messageBox(msg, mfConfirmation+mfYesButton+mfNoButton);
            if (arc!=cmYes) break;
         }
      }
      /* deny write access for any other.
         But, the funny thing is "Run" dialog, which executes command in the
         context of THIS process */
      srcf = _fsopen(fname, NewFile?"w+b":"r+b", SH_DENYWR);
      if (!srcf) {
         errDlg(NewFile?MSGE_FILECREATERR:MSGE_FILEOPENERR);
         break;
      }
      s64t  flen = _filelengthi64(fileno(srcf));
      if (flen<0) {
         errDlg(MSGE_READERR);
         break;
      }
      TRect rr = deskTop->getExtent();
      win = new THexEdWindow(rr, fname);
      
      if (win && application->validView(win)) {
         rr = TRect(1, 1, win->size.x-1, win->size.y-1);
         win->options|= ofTileable;
         win->hexEd   = new TLongEditor(rr);
         win->srcFile = srcf;  srcf = 0;
         win->srcPath = srcp;  srcp = 0;
         win->insert(win->hexEd);
         deskTop->insert(win);
      
         if (win->hexEd) {
            // get block size for binary file editor
#ifdef __QSINIT__
            win->led.clustersize = io_blocksize(_os_handle(fileno(win->srcFile)));
            win->bshift = bsf32(win->led.clustersize);
#else    
            win->led.clustersize = 512;
            win->bshift = 9;
#endif   
            win->led.clusters    = (u64t)flen>>win->bshift;
            win->led.lcbytes     = flen & (1<<win->bshift) - 1;
         
            if (win->led.lcbytes) win->led.clusters++;
            if (win->led.clusters && !win->led.lcbytes)
               win->led.lcbytes = win->led.clustersize;
         
            win->led.poswidth    = 0;
            win->led.showtitles  = 0;
            win->led.userdata    = (int)win;
            win->led.noreaderr   = 1;
            win->led.read        = binfile_read;
            win->led.write       = binfile_write;
            win->led.gettitle    = binfile_gettitle;
            win->led.askupdate   = binfile_askupdate;
            win->led.setsize     = binfile_setsize;
            win->hexEd->setData(&win->led);
         }
      }
   } while (0);
   if (srcf) fclose(srcf);
   if (srcp) free(srcp);

   return win;
}
