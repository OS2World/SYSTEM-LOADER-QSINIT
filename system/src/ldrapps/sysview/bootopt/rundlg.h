#if !defined( __RUNDLG_H )
#define __RUNDLG_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_THistory
#define Uses_TCheckBoxes
#define Uses_TSItem

#include <tv.h>

#define cmRunBrowse        520
#define cmRunExecute       521

class TRunCommandDialog : public TDialog {
public:
   TRunCommandDialog( );
   virtual void handleEvent( TEvent& );
   virtual Boolean valid( ushort );

   TInputLine *r_cmdline;
   TCheckBoxes *r_opts;
};

#endif  // __RUNDLG_H
