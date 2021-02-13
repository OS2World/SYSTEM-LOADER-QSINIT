//
// QSINIT "sysview" module
// change GPT partition type GUID
//
#if !defined(__SETGPT_H)
#define __SETGPT_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TColoredText
#define Uses_TInputLine
#define Uses_TCombo
#define Uses_TStringCollection
#define Uses_TListBox
#define Uses_TSItem

#include <tv.h>
#include "tcolortx.h"
#include "tcombo.h"
#ifdef __QSINIT__
#include "qsshell.h"
#endif
#include "ptabdlg.h"

#define GPTYPESTR_LEN   64

class TSetGPTTypeDlg : public TDialog {
   char    prevValue[GPTYPESTR_LEN+1];
#ifdef __QSINIT__
   str_list  *t_values, *t_keys;
#endif
   TSItem *buildTypeList();
public:
#ifdef __QSINIT__
   char       *volname;
#endif
   unsigned char rcguid[16];

   TSetGPTTypeDlg(unsigned long disk, unsigned long index);
   ~TSetGPTTypeDlg();

   virtual void handleEvent(TEvent&);
   virtual Boolean valid( ushort );

   TInputLine *elType;
   TCombo     *cbGPTType;
   TInputLine *elTypeGUID;
   TInputLine *elName;
};

class TAOSGUIDDlg : public TDialog {
public:
   unsigned char   orgbyte;
   char           orgdrive;

   TAOSGUIDDlg(unsigned long disk, unsigned long index, char letter);
   ~TAOSGUIDDlg();
   virtual void handleEvent(TEvent&);
   virtual Boolean valid( ushort );

   TInputLine  *elDrive;
   TCombo      *cbDrvName;
   TPTypeByte  *elType;
};

#endif  // __SETGPT_H
