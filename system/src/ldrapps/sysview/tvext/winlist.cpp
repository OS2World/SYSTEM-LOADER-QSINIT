//-------------------------------------------------------
//                       winlist.cpp
//-------------------------------------------------------
#define Uses_TEvent
#define Uses_TCollection
#define Uses_TDeskTop
#define Uses_TProgram
#include <tv.h>
#include <string.h>
#include "winlist.h"

TWinListDialog::TWinListDialog() :
       TDialog(TRect(9, 5, 71, 18), "Window list"),
       TWindowInit(TWinListDialog::initFrame)

{
   TView *control;
   options |= ofCenterX | ofCenterY;
  
   control = new TScrollBar(TRect(48, 1, 49, 12));
   insert(control);
  
   wlb = new TListBox(TRect(2, 1, 48, 12), 1, (TScrollBar*)control);
   insert(wlb);
  
   control = new TButton(TRect(50, 2, 60, 4), "~S~elect", cmOK, bfDefault);
   insert(control);

   control = new TButton(TRect(50, 4, 60, 6), "~E~xit", cmCancel, bfNormal);
   insert(control);

   selectNext(False);
   FillWindowList();
}

void TWinListDialog::NameAdd(TView *win, void *arg) {
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
}

void TWinListDialog::FillWindowList() {
   wl = new TCollection(0,10);
   memset(winlist, 0, sizeof(winlist));
   TProgram::deskTop->forEach(NameAdd,this);
   wlb->newList(wl);
}

void TWinListDialog::handleEvent( TEvent& event) {
   switch (event.what) {
      case evCommand:
         switch (event.message.command) {
            case cmOK:
               if (wlb->focused>=0 && wlb->focused<MAX_WINLIST && winlist[wlb->focused])
                  winlist[wlb->focused]->select();
               break;
         }
         break;
   }
   TDialog::handleEvent(event);
}

Boolean TWinListDialog::valid(ushort command) {
   return TDialog::valid(command);
}
