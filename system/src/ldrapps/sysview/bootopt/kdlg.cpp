#define Uses_MsgBox
#define Uses_TEvent

#include <tv.h>
#include "diskedit.h"
#include "kdlg.h"

TKernBootDlg::TKernBootDlg() :
       TDialog(TRect(7, 1, 73, 21), "OS/2 kernel boot options"),
       TWindowInit(TKernBootDlg::initFrame)
{
   TView *control, *control1;
   options |= ofCenterX | ofCenterY;

   k_name = new TInputLine(TRect(16, 2, 51, 3), 33);
   k_name->helpCtx = hcKernName;
   insert(k_name);

   insert(new THistory(TRect(51, 2, 54, 3), (TInputLine*)k_name, hhKernelName));

   insert(new TLabel(TRect(2, 2, 15, 3), "Kernel file:", k_name));

   k_opts = new TCheckBoxes(TRect(2, 5, 23, 11),
      new TSItem("Preload BASEDEV",
      new TSItem("Edit CONFIG.SYS",
      new TSItem("No logo",
      new TSItem("No revision",
      new TSItem("No LFB for VESA", 
      new TSItem("View memory", 0)))))));
   k_opts->helpCtx = hcKernOptions;
   insert(k_opts);

   insert(new TLabel(TRect(2, 4, 10, 5), "Options", k_opts));

   k_pkey = new TInputLine(TRect(25, 5, 34, 6), 11);
   k_pkey->helpCtx = hcKernPushKey;
   insert(k_pkey);

   control1 = new TCombo(TRect(35, 5, 38, 6), (TInputLine*)k_pkey, 
      cbxOnlyList | cbxDisposesList | cbxNoTransfer,
      new TSItem("Alt-F1",
      new TSItem("Alt-F2",
      new TSItem("Alt-F3",
      new TSItem("Alt-F4",
      new TSItem("Alt-F5",
      new TSItem("none", 0)))))));
   insert(control1);

   insert(new TLabel(TRect(24, 4, 33, 5), "Push key", k_pkey));

   k_limit = new TInputLine(TRect(25, 7, 34, 8), 10);
   k_limit->helpCtx = hcKernMemLimit;
   insert(k_limit);

   insert(new TLabel(TRect(24, 6, 37, 7), "Limit memory", k_limit));

   k_logsize = new TInputLine(TRect(25, 9, 34, 10), 10);
   k_logsize->helpCtx = hcKernLogSize;
   insert(k_logsize);

   insert(new TLabel(TRect(24, 8, 33, 9), "Log size", k_logsize));

   k_cfgext = new TInputLine(TRect(44, 5, 49, 6), 4);
   k_cfgext->helpCtx = hcKernConfigExt;
   insert(k_cfgext);

   insert(new TLabel(TRect(43, 4, 64, 5), "CONFIG.SYS extention", k_cfgext));

   k_letter = new TInputLine(TRect(44, 7, 49, 8), 3);
   k_letter->helpCtx = hcKernDriveLetter;
   insert(k_letter);

   insert(new TLabel(TRect(43, 6, 61, 7), "Boot drive letter", k_letter));

   deb_opts = new TCheckBoxes(TRect(2, 13, 23, 16),
      new TSItem("Allow Ctrl-C",
      new TSItem("Full COM cable",
      new TSItem("Verbose log", 0))));
   deb_opts->helpCtx = hcKernDebOptions;
   insert(deb_opts);

   insert(new TLabel(TRect(2, 12, 10, 13), "Options", deb_opts));

   deb_port = new TInputLine(TRect(25, 13, 34, 14), 10);
   deb_port->helpCtx = hcKernCOMPortAddr;
   insert(deb_port);

   insert(new TLabel(TRect(24, 12, 41, 13), "COM port address", deb_port));

   deb_rate = new TInputLine(TRect(25, 15, 34, 16), 13);
   deb_rate->helpCtx = hcKernCOMPortRate;
   insert(deb_rate);

   control1 = new TCombo(TRect(35, 15, 38, 16), (TInputLine*)deb_rate, 
      cbxOnlyList | cbxDisposesList | cbxNoTransfer,
      new TSItem("115200",
      new TSItem("1200",
      new TSItem("150",
      new TSItem("19200",
      new TSItem("2400",
      new TSItem("300",
      new TSItem("38400",
      new TSItem("4800",
      new TSItem("57600",
      new TSItem("600",
      new TSItem("9600", 0))))))))))));
   insert(control1);

   insert(new TLabel(TRect(24, 14, 34, 15), "Baud rate", deb_rate));

   deb_symname = new TInputLine(TRect(44, 13, 58, 14), 12);
   deb_symname->helpCtx = hcKernSYMName;
   insert(deb_symname);

   insert(new THistory(TRect(58, 13, 61, 14), (TInputLine*)deb_symname, 2));

   insert(new TLabel(TRect(43, 12, 59, 13), "Kernel SYM file", deb_symname));

   control = new TButton(TRect(22, 17, 32, 19), "B~o~ot", cmLaunchKernel, bfDefault);
   insert(control);
   control = new TButton(TRect(34, 17, 44, 19), "~C~ancel", cmCancel, bfNormal);
   insert(control);
   control = new TButton(TRect(54, 2, 64, 4), "~I~mport", cmImportKernel, bfNormal);
   control->helpCtx = hcKernImport;
   insert(control);

   control = new TColoredText(TRect(2, 11, 64, 12), "컴컴컴컴컴컴컴컴컴컴 Debug ke"
      "rnel options 컴컴컴컴컴컴컴컴컴컴", 0x7f);
   insert(control);

   control = new TStaticText(TRect(35, 7, 38, 8), "Mb");
   insert(control);
   control = new TStaticText(TRect(35, 9, 38, 10), "kb");
   insert(control);

   selectNext(False);
}

void TKernBootDlg::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
   if (event.what==evCommand) {
      switch (event.message.command)  {
         case cmLaunchKernel: 
            if (CheckBootDlg(this)) {
               RunKernelBoot(this);
               messageBox("\x03""Failed!",mfError+mfOKButton);
            }
            clearEvent(event); 
            break;
         case cmImportKernel: {
            SysApp.LoadFromZIPRevArchive(k_name);
            clearEvent(event); 
            break;
         }
      }
   }
}

Boolean TKernBootDlg::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) { }
   return rslt;
}
