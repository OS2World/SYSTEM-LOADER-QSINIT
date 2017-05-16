#include "setgpt.h"
#include "diskedit.h"
#include "stdlib.h"
#ifdef __QSINIT__
#include "qsdm.h"
#include "qsshell.h"
#endif

TSItem *TSetGPTTypeDlg::buildTypeList() {
   TSItem *rc = 0;
#ifdef __QSINIT__
   t_values = 0;
   t_keys   = cmd_shellmsgall(&t_values);
   int ii;
   for (ii=0; ii<t_keys->count; ii++)
      if (strlen(t_keys->item[ii])==36 && t_keys->item[ii][8]=='-' && 
         t_keys->item[ii][13]=='-' && t_keys->item[ii][18]=='-' &&
            t_keys->item[ii][23]=='-') 
      {
         rc = new TSItem(t_values->item[ii], rc);
      } else {
         // speed up search process (we got full msg.ini here actually ;))
         t_values->item[ii][0] = 0;
      }
#endif
   return rc;
}

TSetGPTTypeDlg::TSetGPTTypeDlg(unsigned long disk, unsigned long index) :
       TDialog(TRect(17, 5, 61, 18), "Set partition type"),
       TWindowInit(TSetGPTTypeDlg::initFrame)

{
   TView *control;
   options |= ofCenterX | ofCenterY;

   elTypeGUID = new TInputLine(TRect(3, 8, 41, 9), GPTYPESTR_LEN);
   elTypeGUID->helpCtx = hcGPTTypeGUID;
   insert(elTypeGUID);

   control = new TButton(TRect(17, 10, 27, 12), "O~K~", cmOK, bfDefault);
   insert(control);

   elType = new TInputLine(TRect(3, 6, 30, 7), GPTYPESTR_LEN);
   elType->helpCtx = hcGPTType;
   insert(elType);

   TSItem *typelist = buildTypeList();

   cbGPTType = new TCombo(TRect(30, 6, 33, 7), (TInputLine*)elType,
      cbxOnlyList | cbxDisposesList | cbxNoTransfer, typelist);
   insert(cbGPTType);

   control = new TStaticText(TRect(2, 1, 14, 2), "Current type");
   insert(control);

   char guid[40], *ptstr = 0;
#ifdef __QSINIT__
   dsk_gptpartinfo pi;
   dsk_gptpinfo(disk, index, &pi);
   dsk_guidtostr(pi.TypeGUID, guid);

   ptstr   = guid[0] ? cmd_shellgetmsg(guid) : 0;
#else
   guid[0] = 0;
#endif

   control = new TColoredText(TRect(4, 3, 40, 4), guid, 0x7f);
   insert(control);

   control = new TColoredText(TRect(4, 2, 40, 3), ptstr ? ptstr : "Custom type", 0x7b);
   insert(control);
   if (ptstr) free(ptstr);

   control = new TStaticText(TRect(2, 5, 14, 6), "New type");
   insert(control);

   rcguid[0] = 0;
   prevValue[0] = 0;
}

TSetGPTTypeDlg::~TSetGPTTypeDlg() {
#ifdef __QSINIT__
   if (t_values) { free(t_values); t_values = 0; }
   if (t_keys) { free(t_keys); t_keys = 0; }
#endif
}

void TSetGPTTypeDlg::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
#ifdef __QSINIT__
   char buf[GPTYPESTR_LEN+1];
   elType->getData(buf);
   if (strcmp(buf, prevValue)) {
      int ii;
      for (ii=0; ii<t_values->count; ii++)
         if (strcmp(t_values->item[ii],buf) == 0) {
            char bstr[GPTYPESTR_LEN+1];
            strcpy(bstr, t_keys->item[ii]);

            elTypeGUID->setData(bstr);
            break;
         }
      strcpy(prevValue, buf);
   }
#endif
}

Boolean TSetGPTTypeDlg::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK)) {
#ifdef __QSINIT__
      char  buf[GPTYPESTR_LEN+1];
      elTypeGUID->getData(buf);

      if (!dsk_strtoguid(buf, rcguid)) {
         rslt = False;
         SysApp.errDlg(MSGE_GUIDFMT);
      }
#endif
   }
   return rslt;
}

void TSysApp::SetGPTType(u32t disk, u32t index) {
#ifdef __QSINIT__
   if (dsk_isgpt(disk, index)<=0) {
      errDlg(MSGE_NOTGPT);
   } else 
#endif
   {
      TSetGPTTypeDlg *dlg = new TSetGPTTypeDlg(disk, index);

      if (execView(dlg)==cmOK) {
#ifdef __QSINIT__
         dsk_gptpartinfo pi;
         u32t err = dsk_gptpinfo(disk, index, &pi);
         if (err==0) {
            memcpy(pi.TypeGUID, dlg->rcguid, 16);
            err = dsk_gptpset(disk, index, &pi);
         }
         if (err) PrintPTErr(err);
#endif
      }
      destroy(dlg);
   }
}
