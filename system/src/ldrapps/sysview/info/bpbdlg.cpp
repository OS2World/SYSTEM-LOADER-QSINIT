#include "bpbdlg.h"
#include "parttab.h"
#include "stdlib.h"
#include "string.h"
#include "qstypes.h"

TBPBSectorDialog::TBPBSectorDialog(void *sector) :
       TDialog(TRect(6, 2, 74, 20), "Boot sector BPB"),
       TWindowInit(TBPBSectorDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   sectordata = (uchar*)sector;

   elBPB = new TInputLong(TRect(23, 1, 30, 2), 6, 0, 65535, 0, "Bytes per sector");
   insert(elBPB);
   insert(new TLabel(TRect(2, 1, 19, 2), "Bytes per sector", elBPB));

   elSPC = new TInputLong(TRect(23, 2, 30, 3), 4, 0, 255, 0, "Sectors per cluster");
   insert(elSPC);
   insert(new TLabel(TRect(2, 2, 22, 3), "Sectors per cluster", elSPC));

   elReserved = new TInputLong(TRect(23, 3, 30, 4), 6, 0, 65535, 0, "Reserved sectors");
   insert(elReserved);
   insert(new TLabel(TRect(2, 3, 19, 4), "Reserved sectors", elReserved));

   elFatCopies = new TInputLong(TRect(23, 4, 30, 5), 4, 0, 255, 0, "FAT copies");
   insert(elFatCopies);
   insert(new TLabel(TRect(2, 4, 13, 5), "FAT copies", elFatCopies));

   elRootEnt = new TInputLong(TRect(23, 5, 30, 6), 6, 0, 65535, 0, "Root entries");
   insert(elRootEnt);
   insert(new TLabel(TRect(2, 5, 15, 6), "Root entries", elRootEnt));

   elMedia = new TInputLong(TRect(23, 6, 30, 7), 3, 0, 255, 16, "Media byte");
   insert(elMedia);
   insert(new TLabel(TRect(2, 6, 13, 7), "Media byte", elMedia));

   elDwFatSec = new TInputLong(TRect(23, 7, 30, 8), 6, 0, 65535, 0, "Sectors per FAT");
   insert(elDwFatSec);
   insert(new TLabel(TRect(2, 7, 18, 8), "Sectors per FAT", elDwFatSec));

   elSPT = new TInputLong(TRect(23, 8, 30, 9), 6, 0, 65535, 0, "Sectors per track");
   insert(elSPT);
   insert(new TLabel(TRect(2, 8, 20, 9), "Sectors per track", elSPT));

   elHeads = new TInputLong(TRect(23, 9, 30, 10), 6, 0, 65535, 0, "Heads");
   insert(elHeads);
   insert(new TLabel(TRect(2, 9, 8, 10), "Heads", elHeads));

   elOEM = new TInputLine(TRect(54, 1, 64, 2), 9);
   insert(elOEM);
   insert(new TLabel(TRect(33, 1, 44, 2), "OEM string", elOEM));

   elDwSectors = new TInputLong(TRect(54, 2, 62, 3), 7, 0, 65535, 0, "Total sectors");
   insert(elDwSectors);
   insert(new TLabel(TRect(33, 2, 47, 3), "Total sectors", elDwSectors));

   elDdSectors = new TInputLong(TRect(54, 3, 66, 4), 11, 0, (long)0xFFFFFFFF, 9, "Total sectors (big)");
   insert(elDdSectors);
   insert(new TLabel(TRect(33, 3, 53, 4), "Total sectors (big)", elDdSectors));

   elHidden = new TInputLong(TRect(54, 4, 66, 5), 11, 0, (long)0xFFFFFFFF, 9, "Hidden sectors");
   insert(elHidden);
   insert(new TLabel(TRect(33, 4, 48, 5), "Hidden sectors", elHidden));

   elPhysDisk = new TInputLong(TRect(23, 11, 30, 12), 3, 0, 255, 16, "Physical disk");
   insert(elPhysDisk);
   insert(new TLabel(TRect(2, 11, 16, 12), "Physical disk", elPhysDisk));

   elDirty = new TInputLong(TRect(23, 12, 30, 13), 5, 0, 255, 1, "Volume dirty");
   insert(elDirty);
   insert(new TLabel(TRect(2, 12, 15, 13), "Volume dirty", elDirty));

   elSize = new TInputLong(TRect(23, 13, 30, 14), 3, 0, 255, 16, "Size byte");
   insert(elSize);
   insert(new TLabel(TRect(2, 13, 12, 14), "Size byte", elSize));

   elFsType = new TInputLine(TRect(20, 14, 30, 15), 9);
   insert(elFsType);
   insert(new TLabel(TRect(2, 14, 10, 15), "FS type", elFsType));

   elVolSer = new TInputLong(TRect(20, 15, 30, 16), 9, 0, (long)0xFFFFFFFF, 24, "Volume serial");
   insert(elVolSer);
   insert(new TLabel(TRect(2, 15, 16, 16), "Volume serial", elVolSer));

   elVolLabel = new TInputLine(TRect(20, 16, 33, 17), 12);
   insert(elVolLabel);
   insert(new TLabel(TRect(2, 16, 15, 17), "Volume label", elVolLabel));

   elDddFatSec = new TInputLong(TRect(54, 6, 66, 7), 11, 0, (long)0xFFFFFFFF, 9, "Sectors per FAT");
   insert(elDddFatSec);
   insert(new TLabel(TRect(33, 6, 49, 7), "Sectors per FAT", elDddFatSec));

   elActCopy = new TInputLong(TRect(54, 7, 61, 8), 7, 0, 65535, 1, "ActiveCopy");
   insert(elActCopy);
   insert(new TLabel(TRect(33, 7, 44, 8), "ActiveCopy", elActCopy));

   elVersion = new TInputLong(TRect(54, 8, 61, 9), 7, 0, 65535, 1, "Version");
   insert(elVersion);
   insert(new TLabel(TRect(33, 8, 41, 9), "Version", elVersion));

   elRootClus = new TInputLong(TRect(54, 9, 66, 10), 11, 0, (long)0xFFFFFFFF, 9, "Root dir cluster");
   insert(elRootClus);
   insert(new TLabel(TRect(33, 9, 50, 10), "Root dir cluster", elRootClus));

   elFsInfoSec = new TInputLong(TRect(54, 10, 61, 11), 7, 0, 65535, 1, "FS Info sector");
   insert(elFsInfoSec);
   insert(new TLabel(TRect(33, 10, 48, 11), "FS Info sector", elFsInfoSec));

   elFsCopySec = new TInputLong(TRect(54, 11, 61, 12), 7, 0, 65535, 1, "Boot copy sector");
   insert(elFsCopySec);
   insert(new TLabel(TRect(33, 11, 50, 12), "Boot copy sector", elFsCopySec));

   rbBPBMode = new TRadioButtons(TRect(34, 14, 48, 16), new TSItem("~F~AT/HPFS",
      new TSItem("FAT~3~2", 0)));
   insert(rbBPBMode);
   insert(new TLabel(TRect(33, 13, 42, 14), "BPB Mode", rbBPBMode));

   control = new TButton(TRect(53, 13, 63, 15), "O~K~", cmOK, bfDefault);
   insert(control);
   control = new TButton(TRect(53, 15, 63, 17), "~C~ancel", cmCancel, bfNormal);
   insert(control);
   control = new TColoredText(TRect(33, 5, 67, 6), "컴� FAT32 컴컴컴컴컴컴컴컴컴컴컴�", 0x7F);
   insert(control);
   control = new TColoredText(TRect(2, 10, 32, 11), "컴� Extended 컴컴컴컴컴컴컴컴", 0x7F);
   insert(control);

   Boot_Record    *brw = (Boot_Record*)sector;
   Boot_RecordF32 *brd = (Boot_RecordF32*)sector;
   char buf[12];
   memcpy(buf, brw->BR_OEM, 8);
   buf[8] = 0;
   elOEM->setData(buf);
   long value = brw->BR_BPB.BPB_BytePerSect;
   elBPB->setData(&value);
   value = brw->BR_BPB.BPB_SecPerClus;
   elSPC->setData(&value);
   value = brw->BR_BPB.BPB_ResSectors;
   elReserved->setData(&value);
   value = brw->BR_BPB.BPB_FATCopies;
   elFatCopies->setData(&value);
   value = brw->BR_BPB.BPB_RootEntries;
   elRootEnt->setData(&value);
   value = brw->BR_BPB.BPB_MediaByte;
   elMedia->setData(&value);
   value = brw->BR_BPB.BPB_SecPerFAT;
   elDwFatSec->setData(&value);
   value = brw->BR_BPB.BPB_SecPerTrack;
   elSPT->setData(&value);
   value = brw->BR_BPB.BPB_Heads;
   elHeads->setData(&value);
   value = brw->BR_BPB.BPB_TotalSec;
   elDwSectors->setData(&value);
   elDdSectors->setData(&brw->BR_BPB.BPB_TotalSecBig);
   elHidden->setData(&brw->BR_BPB.BPB_HiddenSec);
   // FAT32 boot sector?
   cfmode = brw->BR_BPB.BPB_RootEntries==0 &&
            strncmp((char*)brd->BR_F32_EBPB.EBPB_FSType,"FAT",3)==0 ? 1 : 0;

   rbBPBMode->press(cfmode);
   FillToMode(cfmode);

   selectNext(False);
}

static void ClearInputLine(TInputLine *ln) {
   ln->data[0] = EOS;
   ln->selectAll(True);
}

void TBPBSectorDialog::UpdateBuffer() {
   Boot_Record    *brw = (Boot_Record*)sectordata;
   Boot_RecordF32 *brd = (Boot_RecordF32*)sectordata;
   char  buf[16];
   long  value;

   elOEM->getData(buf);
   value = strlen(buf);
   memcpy(brw->BR_OEM, buf, min(value,8));

   elBPB->getData(&value);
   brw->BR_BPB.BPB_BytePerSect = value;
   elSPC->getData(&value);
   brw->BR_BPB.BPB_SecPerClus = value;
   elReserved->getData(&value);
   brw->BR_BPB.BPB_ResSectors = value;
   elFatCopies->getData(&value);
   brw->BR_BPB.BPB_FATCopies = value;
   elRootEnt->getData(&value);
   brw->BR_BPB.BPB_RootEntries = value;
   elMedia->getData(&value);
   brw->BR_BPB.BPB_MediaByte = value;
   elDwFatSec->getData(&value);
   brw->BR_BPB.BPB_SecPerFAT = value;
   elSPT->getData(&value);
   brw->BR_BPB.BPB_SecPerTrack = value;
   elHeads->getData(&value);
   brw->BR_BPB.BPB_Heads = value;
   elDwSectors->getData(&value);
   brw->BR_BPB.BPB_TotalSec = value;
   elDdSectors->getData(&brw->BR_BPB.BPB_TotalSecBig);
   elHidden->getData(&brw->BR_BPB.BPB_HiddenSec);

   struct Extended_BPB *ebpb = cfmode?&brd->BR_F32_EBPB:&brw->BR_EBPB;
   elPhysDisk->getData(&value);
   ebpb->EBPB_PhysDisk = value;
   elDirty->getData(&value);
   ebpb->EBPB_Dirty = value;
   elSize->getData(&value);
   ebpb->EBPB_Sign = value;
   elVolSer->getData(&ebpb->EBPB_VolSerial);

   elVolLabel->getData(buf);
   value = strlen(buf);
   memcpy(ebpb->EBPB_VolLabel, buf, min(value,11));
   elFsType->getData(buf);
   value = strlen(buf);
   memcpy(ebpb->EBPB_FSType, buf, min(value,8));

   if (cfmode) {
      struct ExtF32_BPB *fdi = &brd->BR_F32_BPB;
      elDddFatSec->getData(&fdi->FBPB_SecPerFAT);
      elActCopy->getData(&value);
      fdi->FBPB_ActiveCopy = value;
      elVersion->getData(&value);
      fdi->FBPB_Version = value;
      elRootClus->getData(&fdi->FBPB_RootClus);
      elFsInfoSec->getData(&value);
      fdi->FBPB_FSInfo = value;
      elFsCopySec->getData(&value);
      fdi->FBPB_BootCopy = value;
   }
}

void TBPBSectorDialog::SetILToMode(TInputLong *il, int newmode) {
   il->setState(sfDisabled,Boolean(!newmode));
   // disable error message in non-FAT32 mode
   if (newmode) il->ilOptions &= ~ilBlankEqZero; else
      il->ilOptions |= ilBlankEqZero;
   if (!newmode) ClearInputLine(il);
}

void TBPBSectorDialog::FillToMode(int newmode) {
   Boot_Record    *brw = (Boot_Record*)sectordata;
   Boot_RecordF32 *brd = (Boot_RecordF32*)sectordata;
   long          value;

   SetILToMode(elActCopy,newmode);
   SetILToMode(elVersion,newmode);
   SetILToMode(elRootClus,newmode);
   SetILToMode(elFsInfoSec,newmode);
   SetILToMode(elFsCopySec,newmode);
   SetILToMode(elDddFatSec,newmode);

   if (newmode) {
      struct ExtF32_BPB *fdi = &brd->BR_F32_BPB;
      elDddFatSec->setData(&fdi->FBPB_SecPerFAT);
      value = fdi->FBPB_ActiveCopy;
      elActCopy->setData(&value);
      value = fdi->FBPB_Version;
      elVersion->setData(&value);
      elRootClus->setData(&fdi->FBPB_RootClus);
      value = fdi->FBPB_FSInfo;
      elFsInfoSec->setData(&value);
      value = fdi->FBPB_BootCopy;
      elFsCopySec->setData(&value);
   }

   struct Extended_BPB *ebpb = newmode?&brd->BR_F32_EBPB:&brw->BR_EBPB;
   value = ebpb->EBPB_PhysDisk;
   elPhysDisk->setData(&value);
   value = ebpb->EBPB_Dirty;
   elDirty->setData(&value);
   value = ebpb->EBPB_Sign;
   elSize->setData(&value);
   elVolSer->setData(&ebpb->EBPB_VolSerial);
   char buf[12];
   memcpy(buf, ebpb->EBPB_VolLabel, 11); buf[11] = 0;
   elVolLabel->setData(buf);
   memcpy(buf, ebpb->EBPB_FSType, 8); buf[8] = 0;
   elFsType->setData(buf);

   cfmode = newmode;
}

void TBPBSectorDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
   // mark was changed
   if (Xor(rbBPBMode->mark(1),cfmode)) {
      UpdateBuffer();
      FillToMode(rbBPBMode->mark(1)?1:0);
      drawView();
   }
}

Boolean TBPBSectorDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) {
      UpdateBuffer();
   }
   return rslt;
}
