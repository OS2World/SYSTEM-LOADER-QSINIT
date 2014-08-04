//
// QSINIT "sysview" module
// "view as BPB" dialog
//
#if !defined( __BPBDLG_H )
#define __BPBDLG_H

#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TButton
#define Uses_TColoredText
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TInputLong
#define Uses_TRadioButtons
#define Uses_TSItem

#include <tv.h>

#include "tcolortx.h"
#include "tinplong.h"

class TBPBSectorDialog : public TDialog {
    uchar      *sectordata;
    int             cfmode;
    void FillToMode(int newmode);
    void SetILToMode(TInputLong *il,int newmode);
    void UpdateBuffer();
public:
    TBPBSectorDialog(void *sector);
    virtual void handleEvent( TEvent& );
    virtual Boolean valid( ushort );

    TInputLong *elBPB;
    TInputLong *elSPC;
    TInputLong *elReserved;
    TInputLong *elFatCopies;
    TInputLong *elRootEnt;
    TInputLong *elMedia;
    TInputLong *elDwFatSec;
    TInputLong *elSPT;
    TInputLong *elHeads;
    TInputLine *elOEM;
    TInputLong *elDwSectors;
    TInputLong *elDdSectors;
    TInputLong *elHidden;
    TInputLong *elPhysDisk;
    TInputLong *elDirty;
    TInputLong *elSize;
    TInputLine *elFsType;
    TInputLong *elVolSer;
    TInputLine *elVolLabel;
    TInputLong *elDddFatSec;
    TInputLong *elActCopy;
    TInputLong *elVersion;
    TInputLong *elRootClus;
    TInputLong *elFsInfoSec;
    TInputLong *elFsCopySec;
    TRadioButtons *rbBPBMode;
};

#endif  // __BPBDLG_H

