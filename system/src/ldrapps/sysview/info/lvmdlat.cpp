#include "lvmdlat.h"
#include "diskedit.h"
#ifdef __QSINIT__
#include "lvmdat.h"
#endif

const char *longLine = "컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�";

#define LARGEBOX_INC   3

TDLATDialog::TDLATDialog(void *sector, unsigned long sectorsize) :
   TDialog(TRect(6, 0, 71, 23+(TScreen::screenHeight>=30?LARGEBOX_INC:0)), 
   "LVM DLAT sector view"),TWindowInit(TDLATDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   helpCtx = hcDlatDlg;

   sectorBuf  = sector;
   sectorSize = sectorsize;

   elDiskName = new TInputLine(TRect(16, 1, 39, 2), 21);
   elDiskName->helpCtx = hcDlatDiskName;
   insert(elDiskName);
   insert(new TLabel(TRect(3, 1, 13, 2), "Disk name", elDiskName));

   elDiskSer = new TInputLong(TRect(16, 2, 27, 3), 9, 0, (long)0xFFFFFFFF, 29, "Disk serial");
   elDiskSer->helpCtx = hcDlatDiskSer;
   insert(elDiskSer);
   insert(new TLabel(TRect(3, 2, 15, 3), "Disk serial", elDiskSer));

   elBootSer = new TInputLong(TRect(16, 3, 27, 4), 9, 0, (long)0xFFFFFFFF, 29, "Boot serial");
   elBootSer->helpCtx = hcDlatBootSer;
   insert(elBootSer);
   insert(new TLabel(TRect(3, 3, 15, 4), "Boot serial", elBootSer));

   elInstFlags = new TInputLong(TRect(16, 4, 27, 5), 9, 0, (long)0xFFFFFFFF, 29, "Inst.flags");
   elInstFlags->helpCtx = hcDlatInstFlag;
   insert(elInstFlags);
   insert(new TLabel(TRect(3, 4, 14, 5), "Inst.flags", elInstFlags));

   elCylinders = new TInputLong(TRect(41, 2, 49, 3), 11, 0, (long)0xFFFFFFFF, 8, "Cylinders");
   elCylinders->helpCtx = hcDlatCyls;
   insert(elCylinders);
   insert(new TLabel(TRect(29, 2, 39, 3), "Cylinders", elCylinders));

   elHeads = new TInputLong(TRect(41, 3, 49, 4), 11, 0, (long)0xFFFFFFFF, 8, "Heads");
   elHeads->helpCtx = hcDlatHeads;
   insert(elHeads);
   insert(new TLabel(TRect(29, 3, 35, 4), "Heads", elHeads));

   elSPT = new TInputLong(TRect(41, 4, 49, 5), 11, 0, (long)0xFFFFFFFF, 8, "Sect/Track");
   elSPT->helpCtx = hcDlatSPT;
   insert(elSPT);
   insert(new TLabel(TRect(29, 4, 40, 5), "Sect/Track", elSPT));

   elReboot = new TInputLong(TRect(58, 4, 62, 5), 3, 0, 255, 21, "Reboot");
   elReboot->helpCtx = hcDlatReboot;
   insert(elReboot);
   insert(new TLabel(TRect(50, 4, 57, 5), "Reboot", elReboot));

   control = new TColoredText(TRect(1, 5, 64, 6), longLine, 0x7F);
   insert(control);

   int ii, largebox = TScreen::screenHeight>=30?1:0;

   for (ii=0; ii<4; ii++) {
      char nbms[4];
      int    py = 6 + ii*(4+largebox);
      sprintf(nbms, "%d.", ii+1);
      control = new TColoredText(TRect(1, py, 3, py+1), nbms, 0x7B);
      insert(control);

      if (ii)
         if (largebox) {
            control = new TColoredText(TRect(1, py-1, 64, py), longLine, 0x7F);
            insert(control);
         } else {
            control = new TColoredText(TRect(57, py-1, 64, py), "________", 0x7F);
            insert(control);
            control = new TColoredText(TRect(29, py-1, 31, py), "__", 0x7F);
            insert(control);
            control = new TColoredText(TRect(37, py-1, 41, py), "____", 0x7F);
            insert(control);
            control = new TColoredText(TRect(1, py-1, 3, py), "__", 0x7F);
            insert(control);
         }
      elVolName[ii] = new TInputLine(TRect(16, py, 39, py+1), 21);
      elVolName[ii]->helpCtx = hcDlatVolName;
      insert(elVolName[ii]);
      insert(new TLabel(TRect(3, py, 12, py+1), "Vol.name", elVolName[ii]));
      
      py++;
      elPartName[ii] = new TInputLine(TRect(16, py, 39, py+1), 21);
      elPartName[ii]->helpCtx = hcDlatPartName;
      insert(elPartName[ii]);
      insert(new TLabel(TRect(3, py, 13, py+1), "Part.name", elPartName[ii]));

      py++;
      elVolSer[ii] = new TInputLong(TRect(16, py, 28, py+1), 9, 0, (long)0xFFFFFFFF, 29, "Vol.serial");
      elVolSer[ii]->helpCtx = hcDlatVolSer;
      insert(elVolSer[ii]);
      insert(new TLabel(TRect(3, py, 14, py+1), "Vol.serial", elVolSer[ii]));
      
      py++;
      elPartSer[ii] = new TInputLong(TRect(16, py, 28, py+1), 9, 0, (long)0xFFFFFFFF, 29, "Part.serial");
      elPartSer[ii]->helpCtx = hcDlatPartSer;
      insert(elPartSer[ii]);
      insert(new TLabel(TRect(3, py, 15, py+1), "Part.serial", elPartSer[ii]));
      
      elLetter[ii] = new TInputLine(TRect(32, py, 36, py+1), 3);
      elLetter[ii]->helpCtx = hcDlatLetter;
      insert(elLetter[ii]);
      insert(new TLabel(TRect(30, py-1, 37, py), "Letter", elLetter[ii]));
      
      py-=3;
      elPartStart[ii] = new TInputLong(TRect(50, py, 62, py+1), 11, 0, (long)0xFFFFFFFF, 13, "Start");
      elPartStart[ii]->helpCtx = hcDlatPartStart;
      insert(elPartStart[ii]);
      insert(new TLabel(TRect(41, py, 47, py+1), "Start", elPartStart[ii]));
      
      py++;
      elPartSize[ii] = new TInputLong(TRect(50, py, 62, py+1), 11, 0, (long)0xFFFFFFFF, 13, "Size");
      elPartSize[ii]->helpCtx = hcDlatPartSize;
      insert(elPartSize[ii]);
      insert(new TLabel(TRect(41, py, 46, py+1), "Size", elPartSize[ii]));
      
      py++;
      elInstable[ii] = new TInputLong(TRect(50, py, 56, py+1), 5, 0, 255, 1, "Inst.");
      elInstable[ii]->helpCtx = hcDlatInstble;
      insert(elInstable[ii]);
      insert(new TLabel(TRect(41, py, 47, py+1), "Inst.", elInstable[ii]));
      
      py++;
      elBMMenu[ii] = new TInputLong(TRect(50, py, 56, py+1), 5, 0, 255, 1, "BM Menu");
      elBMMenu[ii]->helpCtx = hcDlatInBM;
      insert(elBMMenu[ii]);
      insert(new TLabel(TRect(41, py, 49, py+1), "BM Menu", elBMMenu[ii]));
   }
   control = new TButton(TRect(51, 2, 61, 4), "O~K~", cmOK, bfDefault);
   insert(control);

#ifdef __QSINIT__
   DLA_Table_Sector *dls = (DLA_Table_Sector*)sector;
   setstrn(elDiskName, dls->Disk_Name, LVM_NAME_SIZE);
   elInstFlags->setData(&dls->Install_Flags);
   elCylinders->setData(&dls->Cylinders);
   elHeads->setData(&dls->Heads_Per_Cylinder);
   elSPT->setData(&dls->Sectors_Per_Track);
   u32t value = dls->Reboot;
   elReboot->setData(&value);
   elDiskSer->setData(&dls->Disk_Serial);
   elBootSer->setData(&dls->Boot_Disk_Serial);
   for (ii=0; ii<4; ii++) {
      setstrn(elVolName[ii], dls->DLA_Array[ii].Volume_Name, LVM_NAME_SIZE);
      setstrn(elPartName[ii], dls->DLA_Array[ii].Partition_Name, LVM_NAME_SIZE);
      value = dls->DLA_Array[ii].On_Boot_Manager_Menu;
      elBMMenu[ii]->setData(&value);
      value = dls->DLA_Array[ii].Installable;
      elInstable[ii]->setData(&value);
      elPartStart[ii]->setData(&dls->DLA_Array[ii].Partition_Start);
      elPartSize[ii]->setData(&dls->DLA_Array[ii].Partition_Size);
      elPartSer[ii]->setData(&dls->DLA_Array[ii].Partition_Serial);
      elVolSer[ii]->setData(&dls->DLA_Array[ii].Volume_Serial);
      setstrn(elLetter[ii], &dls->DLA_Array[ii].Drive_Letter, 1);
   }
   static const char *errText[] = {"CRC ok", "bad signature", "bad CRC" };
   int errcode;
   if (dls->DLA_Signature1!=DLA_TABLE_SIGNATURE1 || 
      dls->DLA_Signature2!=DLA_TABLE_SIGNATURE2) errcode = 1;
   else {
      u32t crcorg = dls->DLA_CRC, crc;
      dls->DLA_CRC = 0;
      crc = lvm_crc32(LVM_INITCRC, dls, sectorsize);
      errcode = crc!=crcorg?2:0;
      dls->DLA_CRC = crcorg;
   }

   txErrText = new TColoredText(TRect(40, 1, 54, 2), errText[errcode], errcode?0x7C:0x7A);
   insert(txErrText);

#endif
   selectNext(False);
}

void TDLATDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
}

void TDLATDialog::UpdateBuffer() {
#ifdef __QSINIT__
   DLA_Table_Sector *dls = (DLA_Table_Sector*)sectorBuf;
   char  buf[32];
   long    value;
   int        ii;

   elDiskName->getData(buf);
   strncpy(dls->Disk_Name, buf, LVM_NAME_SIZE);

   elInstFlags->getData(&dls->Install_Flags);
   elCylinders->getData(&dls->Cylinders);
   elHeads->getData(&dls->Heads_Per_Cylinder);
   elSPT->getData(&dls->Sectors_Per_Track);
   elReboot->getData(&value);
   dls->Reboot = value;
   elDiskSer->getData(&dls->Disk_Serial);
   elBootSer->getData(&dls->Boot_Disk_Serial);
   for (ii=0; ii<4; ii++) {
      elVolName[ii]->getData(buf);
      strncpy(dls->DLA_Array[ii].Volume_Name, buf, LVM_NAME_SIZE);
      elPartName[ii]->getData(buf);
      strncpy(dls->DLA_Array[ii].Partition_Name, buf, LVM_NAME_SIZE);

      elBMMenu[ii]->getData(&value);
      dls->DLA_Array[ii].On_Boot_Manager_Menu = value;
      elInstable[ii]->getData(&value);
      dls->DLA_Array[ii].Installable = value;

      elPartStart[ii]->getData(&dls->DLA_Array[ii].Partition_Start);
      elPartSize[ii]->getData(&dls->DLA_Array[ii].Partition_Size);
      elPartSer[ii]->getData(&dls->DLA_Array[ii].Partition_Serial);
      elVolSer[ii]->getData(&dls->DLA_Array[ii].Volume_Serial);

      elLetter[ii]->getData(buf);
      if (buf[0] && !isalpha(buf[0]) && buf[0]!='*') buf[0] = 0;
      dls->DLA_Array[ii].Drive_Letter = toupper(buf[0]);
   }
   dls->DLA_Signature1 = DLA_TABLE_SIGNATURE1;
   dls->DLA_Signature2 = DLA_TABLE_SIGNATURE2;
   dls->DLA_CRC = 0;
   dls->DLA_CRC = lvm_crc32(LVM_INITCRC, dls, sectorSize);
#endif
}

Boolean TDLATDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) {
      UpdateBuffer();
   }
   return rslt;
}
