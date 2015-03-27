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
         return execDialog(new TFileDialog("*.*","Save file as","~N~ame",
            fdOKButton,hhEditorSaveAs),va_arg(arg,char*));
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

TEditWindow* TSysApp::OpenEditor(int NewFile, int hide) {
   char fname[MAXPATH+1];

   if (!NewFile) {
      TFileDialog *fo = new TFileDialog("*.*", "Open file", "~F~ile name",
                                        fdOpenButton, hhEditorOpen);
      int res=execView(fo)==cmFileOpen;
      if (res) fo->getFileName(fname);
      destroy(fo);
      if (!res) return 0;
   }
   TEditor::editorDialog = doEditDialog;

   TRect         rr = deskTop->getExtent();
   TEditWindow *win = new TEditWindow(rr,NewFile?0:fname,wnNoNumber);

   if (win && application->validView(win)) {
      if (hide) win->hide();
      deskTop->insert(win);
      ToggleEditorCommands(True);
   }
   return win;
}

void TSysApp::ToggleEditorCommands(int on) {
   TCommandSet cmd;
   cmd+=cmSave;
   cmd+=cmSaveAs;
   cmd+=cmFind;
   cmd+=cmReplace;
   cmd+=cmSearchAgain;
   cmd+=cmUndo;
   cmd+=cmCut;
   cmd+=cmCopy;
   cmd+=cmPaste;
   cmd+=cmClear;
   if (on) enableCommands(cmd); else disableCommands(cmd);
}

/*
static void CbCloseWindow(TView *win, void *arg) {
   if ((win->options&ofSelectable)!=0 && win->getState(sfVisible)) {
      const char    *title = ((TWindow*)win)->getTitle(0);
      TWinListDialog *inst = (TWinListDialog*)arg;
      int              pos = inst->wl->getCount();
      if (pos<MAX_WINLIST) {
         char *str = new char[strlen(title)+1];
         strcpy(str,title);
         inst->winlist[pos] = win;
         inst->wl->insert(str);
      }
   }
}*/

void TSysApp::CloseAll() {
   TView *fm;
   do {
      fm = deskTop->firstMatch(sfVisible, ofSelectable);
      if (fm) {
         if (fm==clipWindow) clipWindow->hide(); else {
            if (fm->valid(cmClose)) {
               int ii;
               // notify about closed windows
               for (ii = 0; ii<TWin_Count; ii++)
                  if (windows[ii]==fm) {
                     windows[ii]->removeAction();
                     break;
                  }
               // and remove it
               deskTop->remove(fm);
               destroy(fm);
            } else
               return;
         }
      }
   } while (fm);
}

void TSysApp::SaveLog(Boolean TimeMark) {
   time_t now    = time(0);
   struct tm tme = *localtime(&now);
   char namebuf[MAXPATH+1];
   snprintf(namebuf, MAXPATH+1, "%02d%02d%02d_%02d%02d.log", tme.tm_year%100,
      tme.tm_mon + 1, tme.tm_mday, tme.tm_hour, tme.tm_min);

   if (execDialog(new TFileDialog("*.*","Save system log to file","~N~ame",
      fdOKButton,101),namebuf)!=cmCancel)
   {
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
