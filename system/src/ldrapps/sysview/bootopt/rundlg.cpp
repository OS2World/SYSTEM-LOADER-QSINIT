#define Uses_MsgBox
#define Uses_TEvent
#define Uses_TFileDialog

#include <tv.h>
#include "diskedit.h"
#include "rundlg.h"

TRunCommandDialog::TRunCommandDialog() :
       TDialog(TRect(8, 7, 71, 15), "Run command"),
       TWindowInit(TRunCommandDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   helpCtx  = hcRunExecute;

   r_cmdline = new TInputLine(TRect(12, 2, 57, 3), 256);
   r_cmdline->helpCtx = hcRunCmdline;
   insert(r_cmdline);

   insert(new THistory(TRect(57, 2, 60, 3), r_cmdline, hhCmdLine));

   insert(new TLabel(TRect(2, 2, 11, 3), "Command:", r_cmdline));

   r_opts = new TCheckBoxes(TRect(12, 4, 31, 6),
      new TSItem("Echo ON",
      new TSItem("Pause on exit", 0)));
   r_opts->helpCtx = hcRunEchoON;
   insert(r_opts);

   control = new TButton(TRect(38, 4, 48, 6), "~B~rowse", cmRunBrowse, bfNormal);
   control->helpCtx = hcRunBrowseDisk;
   insert(control);

   control = new TButton(TRect(49, 4, 58, 6), "~G~o!", cmRunExecute, bfDefault);
   control->helpCtx = hcRunExecute;
   insert(control);

   selectNext(False);
}

void TRunCommandDialog::handleEvent(TEvent& event) {
   TDialog::handleEvent(event);
   if (event.what==evCommand) {
      switch (event.message.command)  {
         case cmRunBrowse: {
            TFileDialog *fo=new TFileDialog("*.*", "Open file", "~F~ile name",
                                            fdOpenButton, hhRunFile);
            fo->helpCtx = hcFileDlgCommon;
            int     res = owner->execView(fo)==cmFileOpen;
            if (res) {
               char exename[QS_MAXPATH+1];
               fo->getData(exename);
               setstr(r_cmdline,exename);
            }
            destroy(fo);

            clearEvent(event);
            break;
         }
         case cmRunExecute: {
            char *cmd = getstr(r_cmdline);
            if (cmd&&*cmd) opts_run(cmd,r_opts->mark(0),r_opts->mark(1));
            clearEvent(event);
            break;
         }
      }
   }
}

Boolean TRunCommandDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) { }
   return rslt;
}
