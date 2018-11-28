//
// QSINIT "sysview" module
// session list
//
#if !defined( __SESLIST_H )
#define __SESLIST_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TListBox
#define Uses_TScrollBar

#include <tv.h>
#ifdef __QSINIT__
#include "qscon.h"
#include "qcl/qslist.h"
#include "qsmodext.h"
#endif

class TSesListDialog : public TDialog {
   TListBox  *lbSesList;
#ifdef __QSINIT__
   se_stats           **sl;
   u32t              secnt;
#endif
   void ReadSessionData();
   void FreeSessionData();
public:
   TSesListDialog();
   ~TSesListDialog();
   virtual void handleEvent(TEvent&);
   virtual Boolean valid(ushort);
};

class TProcessTree : public TListViewer {
protected:
#ifdef __QSINIT__
   process_information *pi,
                  **pos2pi;
   u32t              nproc;  ///< # of processes in pi
   ptr_list            str;
#endif
   void ReadProcList();
   void FreeProcList();
public:
   TProcessTree(const TRect& bounds, TScrollBar *aVScrollBar, TScrollBar *aHScrollBar);
   ~TProcessTree();

   void UpdateList();

   virtual void getText( char *, int, int );
   virtual void focusItem( int item );
#ifdef __QSINIT__
   process_information *selInfo();
#endif
};

class TProcInfoDialog : public TDialog {
   u32t             selpid;
   int           in_update;
public:
   TProcInfoDialog();
   virtual void handleEvent(TEvent&);
   void UpdateInfo();

   TProcessTree *procList;
   TListBox     *procInfo;
};

#endif  // __SESLIST_H
