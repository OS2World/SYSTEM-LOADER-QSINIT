#ifdef __QSINIT__
#include "qssys.h"
#endif

TDialog* makeAboutDlg(void) {
   TView *control;
   
   TDialog* dlg = new TDialog(TRect(18, 7, 61, 16), "About");
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   
   control = new TColoredText(TRect(2, 2, 17, 3), "  °±±²   °±±²  ", 0x9f);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 3, 17, 4), " °²  °² °²  ", 0x9f);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 4, 17, 5), " °²  °²  °±±²  ", 0x9f);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 5, 17, 6), " °²  °²     °² ", 0x9f);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 6, 17, 7), "  °±±²   °±±²  ", 0x9f);
   dlg->insert(control);
   // version string
   char verstr[96], *cp=0, *about=0;
   strcpy(verstr,"QS Loader ");
#ifdef __QSINIT__
   u32t ver_len = sys_queryinfo(QSQI_VERSTR, 0);
   about = new char[ver_len];
   sys_queryinfo(QSQI_VERSTR, about);
   cp    = strchr(about,' ');
#endif
   strncat(verstr, cp?cp+1:"1.00", 4);
   if (about) delete about;

   control = new TColoredText(TRect(26, 2, 40, 3), verstr, 0x7f);
   dlg->insert(control);
   control = new TColoredText(TRect(23, 4, 40, 5), "Freeware, 2010-19", 0x7f);
   dlg->insert(control);
   control = new TButton(TRect(30, 6, 40, 8), "O~K~", cmOK, bfDefault);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 1, 17, 2), "ÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜ", 0x79);
   dlg->insert(control);
   control = new TColoredText(TRect(9, 7, 17, 8), "ßßßßßßßß", 0x79);
   dlg->insert(control);
   control = new TColoredText(TRect(7, 7, 9, 8), "°²", 0x9f);
   dlg->insert(control);
   control = new TColoredText(TRect(2, 7, 7, 8), "ßßßßß", 0x79);
   dlg->insert(control);
   dlg->selectNext(False);
   return dlg;
}

TListBox     *SelDiskList = 0;
TCheckBoxes *ChUseOwnBoot = 0;

TDialog* makeSelDiskDlg(int DlgType) {
   static const char *dlgtype[3] = {"Select Disk", "Boot Manager Menu", 
                                    "Select target disk"};
   TView *control;
   TDialog   *dlg = new TDialog(TRect(14, 6, 65, 16), dlgtype[DlgType]);
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   
   control = new TScrollBar(TRect(37, 1, 38, 9));
   dlg->insert(control);
   SelDiskList = new TListBox(TRect(2, 1, 37, 9), 1, (TScrollBar*)control);
   dlg->insert(SelDiskList);
   control = new TButton(TRect(39, 2, 49, 4), DlgType?"~B~oot":"O~K~", cmOK, bfDefault);
   dlg->insert(control);
   control = new TButton(TRect(39, 4, 49, 6), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);
   dlg->selectNext(False);
   return dlg;
}

TDialog* makeSelBootDiskDlg(void) {
   TView *control;
   TDialog   *dlg = new TDialog(TRect(14, 5, 65, 17), "Select HDD to boot from");
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   
   control = new TScrollBar(TRect(37, 1, 38, 9));
   dlg->insert(control);
   SelDiskList = new TListBox(TRect(2, 1, 37, 9), 1, (TScrollBar*)control);
   dlg->insert(SelDiskList);
   control = new TButton(TRect(39, 2, 49, 4), "O~K~", cmOK, bfDefault);
   dlg->insert(control);
   control = new TButton(TRect(39, 4, 49, 6), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);
   ChUseOwnBoot = new TCheckBoxes(TRect(2, 10, 24, 11),
      new TSItem("use own MBR code", 0));
   dlg->insert(ChUseOwnBoot);
   control = new TColoredText(TRect(25, 10, 41, 11), "(non-destuctive)", 0x78);
   dlg->insert(control);
   dlg->selectNext(False);
   return dlg;
}
