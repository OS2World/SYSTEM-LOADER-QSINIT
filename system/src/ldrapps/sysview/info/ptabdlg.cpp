#include "ptabdlg.h"
#include "parttab.h"
#include <stdlib.h>
#include <stdio.h>
#include "diskedit.h"
#ifdef __QSINIT__
#include "qsshell.h"
#endif

#define mbrvar(mbr,x) \
   struct MBR_Record *mbr = &((struct Disk_MBR*)sectordata)->MBR_Table[x];

#define step_x      (19)
#define cmGotoRec0  (1000)

void TPTypeByte::updateComment() {
   unsigned long qv;
   getData(&qv);
   if (qv != prevValue) {
      char *msg = 0, topic[16];
      if (!qv && type0text) {
         sprintf(topic, " %-11s", type0text);
      } else {
#ifdef __QSINIT__
         sprintf(topic, "_PTE_%02X", qv);
         msg = cmd_shellgetmsg(topic);
#endif
         sprintf(topic, " %-11s", msg?msg:"Other");
         if (msg) free(msg);
      }
      replace_coltxt(&cmControl, topic);
      prevValue = qv;
   }
}

void TPTypeByte::handleEvent(TEvent& event) {
   TInputLong::handleEvent(event);
   updateComment();
}

void TPTypeByte::setData(void *rec) {
   TInputLong::setData(rec);
   updateComment();
}

TPTSectorDialog::TPTSectorDialog(void *sector, unsigned warning_spt) :
       TDialog(TRect(0, 1, 79, 20), "Partition table sector"),
       TWindowInit(TPTSectorDialog::initFrame)
{
   sectordata = (uchar*)sector;
   options   |= ofCenterX | ofCenterY;
   helpCtx    = hcViewAsPT;

   int ii = 0;
   for (ii=0; ii<4; ii++) {
      // horiz.step of controls
      int stp = step_x * ii;

      cbActive[ii] = new TCheckBoxes(TRect(15+stp, 2, 20+stp, 3), new TSItem(".", 0));
      insert(cbActive[ii]);
      insert(new TLabel(TRect(7+stp, 2, 14+stp, 3), "Active", cbActive[ii]));

      TColoredText *pttext = new TColoredText(TRect(7+stp, 4, 20+stp, 5), "", 0x3F);
      insert(pttext);
      elPtByte[ii] = new TPTypeByte(TRect(2+stp, 4, 6+stp, 5), pttext, "Type");
      insert(elPtByte[ii]);
      insert(new TLabel(TRect(2+stp, 3, 7+stp, 4), "Type", elPtByte[ii]));

      elCyl1[ii] = new TInputLong(TRect(2+stp, 7, 8+stp, 8), 5, 0, 1023, 0, "Cylinder");
      insert(elCyl1[ii]);

      elHead1[ii] = new TInputLong(TRect(9+stp, 7, 14+stp, 8), 4, 0, 255, 0, "Head");
      insert(elHead1[ii]);

      elSect1[ii] = new TInputLong(TRect(15+stp, 7, 20+stp, 8), 4, 0, 63, 0, "Sector");
      insert(elSect1[ii]);

      elCyl2[ii] = new TInputLong(TRect(2+stp, 9, 8+stp, 10), 5, 0, 1023, 0, "Cylinder");
      insert(elCyl2[ii]);
      insert(new TLabel(TRect(2+stp, 8, 6+stp, 9), "Cyl", elCyl2[ii]));

      elHead2[ii] = new TInputLong(TRect(9+stp, 9, 14+stp, 10), 4, 0, 255, 0, "Head");
      insert(elHead2[ii]);
      insert(new TLabel(TRect(8+stp, 8, 13+stp, 9), "Head", elHead2[ii]));
 
      elSect2[ii] = new TInputLong(TRect(15+stp, 9, 20+stp, 10), 4, 0, 63, 0, "Sector");
      insert(elSect2[ii]);
      insert(new TLabel(TRect(15+stp, 8, 19+stp, 9), "Sec", elSect2[ii]));

      elStart[ii] = new TInputLong(TRect(2+stp, 12, 14+stp, 13), 11, 0, (long)0xFFFFFFFF, 9, "LBA start");
      insert(elStart[ii]);
      insert(new TLabel(TRect(2+stp, 11, 12+stp, 12), "LBA start", elStart[ii]));

      elLength[ii] = new TInputLong(TRect(2+stp, 14, 14+stp, 15), 11, 0, (long)0xFFFFFFFF, 9, "Length");
      insert(elLength[ii]);
      insert(new TLabel(TRect(2+stp, 13, 9+stp, 14), "Length", elLength[ii]));

      char buf[32];
      sprintf(buf, "#~%d~", ii+1);
      TButton *ptnbut = new TButton(TRect(14+stp, 12, 20+stp, 14), buf, cmGotoRec0+ii, bfNormal);
      insert(ptnbut);
      // disable button on 0 pt type
      mbrvar(rec,ii);
      if (!rec->PTE_Type) ptnbut->setState(sfDisabled,True);

      if (ii<3)
         insert(new TColoredText(TRect(20+stp, 1, 21+stp, 15), "³³³³³³³³³³³³³³", 0x7F));

      insert(new TColoredText(TRect(2+stp, 5, 20+stp, 6), " ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", 0x7F));
      insert(new TColoredText(TRect(2+stp, 10, 20+stp, 11), " ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ", 0x7F));

      insert(new TStaticText(TRect(5+stp, 6, 18+stp, 7), "Start/End CHS"));

      sprintf(buf, "Entry #%d", ii+1);
      insert(new TColoredText(TRect(3+stp, 1, 11+stp, 2), buf, 0x75));
   }
   insert(new TButton(TRect(49, 16, 59, 18), "O~K~", cmOK, bfDefault));
   insert(new TButton(TRect(60, 16, 70, 18), "~C~ancel", cmCancel, bfNormal));

   if (warning_spt > 63) {
      char buf[80];
      sprintf(buf, "Warning! LVM use %u sectors geometry!", warning_spt);
      warnText = new TColoredText(TRect(2, 17, 47, 18), buf, 0x7E);
      insert(warnText);
   }

   for (ii=0; ii<4; ii++) {
      mbrvar(rec,ii);
      long value = rec->PTE_Type;
      elPtByte[ii]->setData(&value);
      elStart [ii]->setData(&rec->PTE_LBAStart);
      elLength[ii]->setData(&rec->PTE_LBASize);

      if (rec->PTE_Active&0x80) cbActive[ii]->press(0);

      value = rec->PTE_CSStart>>8 | rec->PTE_CSStart<<2 & 0x300;
      elCyl1[ii]->setData(&value);
      value = rec->PTE_HStart;
      elHead1[ii]->setData(&value);
      value = rec->PTE_CSStart&0x3F;
      elSect1[ii]->setData(&value);
      value = rec->PTE_CSEnd>>8 | rec->PTE_CSEnd<<2 & 0x300;
      elCyl2[ii]->setData(&value);
      value = rec->PTE_HEnd;
      elHead2[ii]->setData(&value);
      value = rec->PTE_CSEnd&0x3F;
      elSect2[ii]->setData(&value);
   }

   selectNext(False);
   dlgrc    = actCancel;
   goptbyte = 0;
}

void TPTSectorDialog::shutDown() {
   TDialog::shutDown();
}

void TPTSectorDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
 
   if (event.what==evCommand) {
      if (event.message.command>=cmGotoRec0 && event.message.command<cmGotoRec0+4) {
         dlgrc = actGoto;

         long value, idx = event.message.command-cmGotoRec0;
         elStart[idx]->getData(&gosector);
         elPtByte[idx]->getData(&value);
         goptbyte = value;

         clearEvent(event);
         endModal(cmOK);
      }
   }
}

Boolean TPTSectorDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) {
      if (dlgrc==actCancel) dlgrc = actOk;

      // updating all - disk editor will compare changes for as
      long value, ii;
      for (ii=0; ii<4; ii++) {
         mbrvar(rec,ii);
         elPtByte[ii]->getData(&value);
         rec->PTE_Type = value;
         elStart [ii]->getData(&rec->PTE_LBAStart);
         elLength[ii]->getData(&rec->PTE_LBASize);
   
         if (cbActive[ii]->mark(0)) rec->PTE_Active|=0x80;
            else rec->PTE_Active&=0x7F;

         elHead1[ii]->getData(&value);
         rec->PTE_HStart = value;
         elHead2[ii]->getData(&value);
         rec->PTE_HEnd   = value;

         elSect1[ii]->getData(&value);
         rec->PTE_CSStart = rec->PTE_CSStart&~0x3F | value&0x3F;
         elSect2[ii]->getData(&value);
         rec->PTE_CSEnd   = rec->PTE_CSEnd  &~0x3F | value&0x3F;

         elCyl1[ii]->getData(&value);
         rec->PTE_CSStart = (value&0xFF)<<8 | value>>2 & 0xC0 | rec->PTE_CSStart&0x3F;
         elCyl2[ii]->getData(&value);
         rec->PTE_CSEnd   = (value&0xFF)<<8 | value>>2 & 0xC0 | rec->PTE_CSEnd  &0x3F;
      }
   }
   return rslt;
}
