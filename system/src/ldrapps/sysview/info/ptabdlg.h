//
// QSINIT "sysview" module
// "view as partition table" dialog
//
#if !defined( __PARTTBL_H )
#define __PARTTBL_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TColoredText
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TInputLong
#define Uses_TCheckBoxes
#define Uses_TListBox
#define Uses_TSItem

#include <tv.h>
#include "tcolortx.h"
#include "tinplong.h"

class TPTSectorDialog : public TDialog {
   uchar      *sectordata;

   TCheckBoxes  *cbActive[4];
   TInputLong   *elPtByte[4];
   TInputLong   *elCyl1  [4];
   TInputLong   *elHead1 [4];
   TInputLong   *elSect1 [4];
   TInputLong   *elCyl2  [4];
   TInputLong   *elHead2 [4];
   TInputLong   *elSect2 [4];
   TInputLong   *elStart [4];
   TInputLong   *elLength[4];
   TColoredText *warnText;
public:
   unsigned long gosector;
   unsigned char goptbyte;

   enum execAction { actCancel, actOk, actGoto };
   execAction       dlgrc;

   TPTSectorDialog(void *sector, unsigned warning_spt = 0);
   virtual void shutDown();
   virtual void handleEvent( TEvent& );
   virtual Boolean valid( ushort );
};

class TPTypeByte : public TInputLong {
   unsigned long  prevValue;
   TColoredText  *cmControl;
   void updateComment();
public:
   char          *type0text;
   TPTypeByte(const TRect& bounds, TColoredText *attComment,
      const char* labelName = 0) : TInputLong(bounds, 3, 0, 255, 
         ilPureHex|ilHex, labelName) , type0text(0)
            { prevValue = 0xFFFFFFFF; cmControl = attComment; }
   virtual void handleEvent(TEvent& event);
   virtual void setData(void *rec);
};

#endif  // __PARTTBL_H
