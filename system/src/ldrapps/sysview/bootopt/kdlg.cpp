#define Uses_MsgBox
#define Uses_TEvent

#include <tv.h>
#include "diskedit.h"
#include "kdlg.h"

TKernBootDlg::TKernBootDlg() :
       TDialog(TRect(7, 1, 73, 22), "OS/2 kernel boot options"),
       TWindowInit(TKernBootDlg::initFrame)
{
   TView *control, *control1;
   options|= ofCenterX | ofCenterY;
   source  =  0;
   valimit = -1;

   k_name = new TInputLine(TRect(16, 2, 51, 3), 33);
   k_name->helpCtx = hcKernName;
   insert(k_name);
   insert(new THistory(TRect(51, 2, 54, 3), k_name, hhKernelName));
   insert(new TLabel(TRect(2, 2, 15, 3), "Kernel file:", k_name));

   control = new TButton(TRect(54, 2, 64, 4), "~I~mport", cmImportKernel, bfNormal);
   control->helpCtx = hcKernImport;
   insert(control);

   cmd_name = new TInputLine(TRect(16, 4, 51, 5), 240);
   cmd_name->helpCtx = hcKernBatchName;
   insert(cmd_name);
   insert(new THistory(TRect(51, 4, 54, 5), cmd_name, hhBatchName));
   insert(new TLabel(TRect(2, 4, 14, 5), "Call batch:", cmd_name));

   control = new TButton(TRect(54, 4, 64, 6), "~S~elect", cmLoadBatchFile, bfNormal);
   control->helpCtx = hcKernBatchName;
   insert(control);

   k_opts = new TCheckBoxes(TRect(2, 7, 23, 15),
      new TSItem("Preload BASEDEV",
      new TSItem("Edit CONFIG.SYS",
      new TSItem("No logo",
      new TSItem("No revision",
      new TSItem("Use OS2LDR.MSG",
      new TSItem("View memory",
      new TSItem("No AF alignment",
      new TSItem("Reset MTRR", 0)))))))));
   k_opts->helpCtx = hcKernOptions;
   insert(k_opts);
   insert(new TLabel(TRect(2, 6, 10, 7), "Options", k_opts));

   k_pkey = new TInputLine(TRect(25, 7, 34, 8), 11);
   k_pkey->helpCtx = hcKernPushKey;
   insert(k_pkey);

   control1 = new TCombo(TRect(35, 7, 38, 8), k_pkey,
      cbxOnlyList | cbxDisposesList | cbxNoTransfer,
      new TSItem("Alt-F1",
      new TSItem("Alt-F2",
      new TSItem("Alt-F3",
      new TSItem("Alt-F4",
      new TSItem("Alt-F5",
      new TSItem("none", 0)))))));
   insert(control1);
   insert(new TLabel(TRect(24, 6, 33, 7), "Push key", k_pkey));

   k_limit = new TInputLine(TRect(25, 9, 34, 10), 10);
   k_limit->helpCtx = hcKernMemLimit;
   insert(k_limit);
   insert(new TLabel(TRect(24, 8, 37, 9), "Limit memory", k_limit));

   k_logsize = new TInputLine(TRect(25, 11, 34, 12), 10);
   k_logsize->helpCtx = hcKernLogSize;
   insert(k_logsize);
   insert(new TLabel(TRect(24, 10, 33, 11), "Log size", k_logsize));

   k_loglevel = new TInputLine(TRect(25, 13, 30, 14), 4);
   k_loglevel->helpCtx = hcKernLogLevel;
   insert(k_loglevel);

   control1 = new TCombo(TRect(30, 13, 33, 14), k_loglevel,
      cbxOnlyList | cbxDisposesList | cbxNoTransfer,
      new TSItem("no",
      new TSItem("0",
      new TSItem("1",
      new TSItem("2",
      new TSItem("all", 0))))));
   insert(control1);
   insert(new TLabel(TRect(24, 12, 34, 13), "Log level", k_loglevel));

   k_cfgext = new TInputLine(TRect(44, 7, 49, 8), 4);
   k_cfgext->helpCtx = hcKernConfigExt;
   insert(k_cfgext);
   insert(new TLabel(TRect(43, 6, 64, 7), "CONFIG.SYS extention", k_cfgext));

   k_letter = new TInputLine(TRect(44, 9, 49, 10), 3);
   k_letter->helpCtx = hcKernDriveLetter;
   insert(k_letter);
   insert(new TLabel(TRect(43, 8, 61, 9), "Boot drive letter", k_letter));

   deb_opts = new TCheckBoxes(TRect(2, 16, 23, 19),
      new TSItem("Allow Ctrl-C",
      new TSItem("Full COM cable",
      new TSItem("Verbose log", 0))));
   deb_opts->helpCtx = hcKernDebOptions;
   insert(deb_opts);
   insert(new TLabel(TRect(2, 15, 16, 16), "Debug options", deb_opts));

   deb_port = new TInputLine(TRect(25, 16, 34, 17), 10);
   deb_port->helpCtx = hcKernCOMPortAddr;
   insert(deb_port);
   insert(new TLabel(TRect(24, 15, 41, 16), "COM port address", deb_port));

   deb_rate = new TInputLine(TRect(25, 18, 34, 19), 13);
   deb_rate->helpCtx = hcKernCOMPortRate;
   insert(deb_rate);

   control1 = new TCombo(TRect(35, 18, 38, 19), deb_rate,
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
   insert(new TLabel(TRect(24, 17, 34, 18), "Baud rate", deb_rate));

   deb_symname = new TInputLine(TRect(44, 16, 58, 17), 12);
   deb_symname->helpCtx = hcKernSYMName;
   insert(deb_symname);
   insert(new THistory(TRect(58, 16, 61, 17), deb_symname, hhSymName));
   insert(new TLabel(TRect(43, 15, 59, 16), "Kernel SYM file", deb_symname));

   control = new TButton(TRect(42, 18, 52, 20), "B~o~ot", cmLaunchKernel, bfDefault);
   insert(control);
   control = new TButton(TRect(52, 18, 62, 20), "~C~ancel", cmCancel, bfNormal);
   insert(control);
   control = new TStaticText(TRect(35, 9, 38, 10), "Mb");
   insert(control);
   control = new TStaticText(TRect(35, 11, 38, 12), "kb");
   insert(control);

   l_lvminfo = new TColoredText(TRect(44, 11, 64, 14), "" /*"LVM L:\n"
       "HDD1 Partition 22\n"
       "(source redirection)"*/, 0x79);
   insert(l_lvminfo);

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
         case cmLoadBatchFile: {
            char *bn = getfname(1, "Select batch file");
            if (bn) cmd_name->setData(bn);
            clearEvent(event);
            break;
         }
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
