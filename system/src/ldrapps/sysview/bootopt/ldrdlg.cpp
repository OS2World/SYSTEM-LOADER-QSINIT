#define Uses_MsgBox
#define Uses_TEvent

#include <tv.h>

#include "diskedit.h"
#include "ldrdlg.h"

TLdrBootDialog::TLdrBootDialog() :
       TDialog(TRect(15, 8, 64, 15), "OS2LDR file launch"),
       TWindowInit(TLdrBootDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;

   bf_name = new TInputLine(TRect(13, 2, 43, 3), 29);
   bf_name->helpCtx = hcLdrName;
   insert(bf_name);

   insert(new THistory(TRect(43, 2, 46, 3), bf_name, hhLdrName));

   insert(new TLabel(TRect(2, 2, 12, 3), "File name", bf_name));

   control = new TButton(TRect(9, 4, 19, 6), "~L~oad", cmLaunchLdr, bfDefault);
   control->helpCtx = hcLdrLoad;
   insert(control);

   control = new TButton(TRect(20, 4, 30, 6), "~C~ancel", cmCancel, bfNormal);
   insert(control);

   control = new TButton(TRect(31, 4, 41, 6), "~K~ernel", cmSelectKernelDialog, bfNormal);
   control->helpCtx = hcLdrKernel;
   insert(control);

   selectNext(False);
}

void TLdrBootDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
   if (event.what==evCommand) {
      switch (event.message.command)  {
         case cmSelectKernelDialog: 
            endModal(event.message.command);
            clearEvent(event);
            break;
         case cmLaunchLdr: {
            char *ldr = getstr(bf_name);
            if (ldr&&*ldr) {
               u32t rc = opts_loadldr(ldr);
               SysApp.PrintPTErr(rc);
            }
            clearEvent(event);
            break;
         }
      }
   }
}

Boolean TLdrBootDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) { }
   return rslt;
}
