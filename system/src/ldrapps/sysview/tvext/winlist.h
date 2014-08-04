#if !defined( __WINLIST_H )
#define __WINLIST_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TCollection

#include <tv.h>

#define MAX_WINLIST  1024

class TWinListDialog : public TDialog {
   TListBox    *wlb;
   TCollection  *wl;
   TView   *winlist[MAX_WINLIST];
   void FillWindowList();
   static void NameAdd(TView *win, void *arg);
public:
   TWinListDialog();
   virtual void handleEvent(TEvent&);
   virtual Boolean valid(ushort);
};
#endif  // __WINLIST_H
