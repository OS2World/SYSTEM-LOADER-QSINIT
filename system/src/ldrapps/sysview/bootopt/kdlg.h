#if !defined( __KERNELOPTS_H )
#define __KERNELOPTS_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TColoredText
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_THistory
#define Uses_TCheckBoxes
#define Uses_TCombo
#define Uses_TStringCollection
#define Uses_TListBox
#define Uses_TPXPictureValidator
#define Uses_TSItem

#include <tv.h>

#include "tcolortx.h"
#include "tcombo.h"

#define cmImportKernel  500
#define cmLaunchKernel  501
#define cmLoadBatchFile 502
#define cmLoadKernel    503

class TKernBootDlg : public TDialog {
public:
   TKernBootDlg();
   virtual void handleEvent(TEvent&);
   virtual Boolean valid(ushort);
   /// SOURCE=... drive letter (display only)
   char   source;
   long  valimit;

   TInputLine      *k_name;
   TInputLine    *cmd_name;
   TCheckBoxes     *k_opts;
   TInputLine      *k_pkey;
   TInputLine     *k_limit;
   TInputLine   *k_logsize;
   TInputLine  *k_loglevel;
   TInputLine    *k_cfgext;
   TInputLine    *k_letter;
   TCheckBoxes   *deb_opts;
   TInputLine    *deb_port;
   TInputLine    *deb_rate;
   TInputLine *deb_symname;
   TColoredText *l_lvminfo;
};

#endif  // __KERNELOPTS_H
