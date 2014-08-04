#if !defined( __LDRDLG_H )
#define __LDRDLG_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_THistory

#include <tv.h>

#define cmSelectKernelDialog 510
#define cmLaunchLdr          511

class TLdrBootDialog : public TDialog {
public:
    TLdrBootDialog( );
    virtual void handleEvent( TEvent& );
    virtual Boolean valid( ushort );

    TInputLine *bf_name;
};

#endif  // __LDRDLG_H
