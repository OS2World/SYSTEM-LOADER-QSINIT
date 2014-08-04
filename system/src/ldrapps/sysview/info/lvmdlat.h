//
// QSINIT "sysview" module
// "view as LVM DLAT sector" dialog
//
#if !defined( __LVMDLAT_H )
#define __LVMDLAT_H
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TScreen
#define Uses_TColoredText
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TInputLong

#include <tv.h>

#include "tcolortx.h"
#include "tinplong.h"

class TDLATDialog : public TDialog {
   void          *sectorBuf;
   unsigned long sectorSize;
public:
   TDLATDialog(void *sector, unsigned long sectorsize);
   virtual void handleEvent(TEvent&);
   virtual Boolean valid(ushort);
   void    UpdateBuffer();

   TInputLine *elDiskName;
   TInputLong *elDiskSer;
   TInputLong *elBootSer;
   TInputLong *elInstFlags;
   TInputLong *elCylinders;
   TInputLong *elHeads;
   TInputLong *elSPT;
   TInputLong *elReboot;
   TInputLong *elVolSer[4];
   TInputLong *elPartSer[4];
   TInputLong *elPartStart[4];
   TInputLong *elPartSize[4];
   TInputLong *elBMMenu[4];
   TInputLong *elInstable[4];
   TInputLine *elLetter[4];
   TInputLine *elVolName[4];
   TInputLine *elPartName[4];
   TColoredText *txErrText;
};

#endif  // __LVMDLAT_H
