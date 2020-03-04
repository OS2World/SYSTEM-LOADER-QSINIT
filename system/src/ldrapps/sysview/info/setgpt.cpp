#include "setgpt.h"
#include "diskedit.h"
#include "stdlib.h"
#ifdef __QSINIT__
#include "qsdm.h"
#include "qsshell.h"
#include "qsint.h"
#endif

TSItem *TSetGPTTypeDlg::buildTypeList() {
   TSItem *rc = 0;
#ifdef __QSINIT__
   volname  = 0;
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
       TDialog(TRect(11, 6, 68, 17), "GPT partition"),
       TWindowInit(TSetGPTTypeDlg::initFrame)

{
   TView *control;
   options |= ofCenterX | ofCenterY;

   FillDiskInfo(this, disk, index);

   elName = new TInputLine(TRect(24, 2, 54, 3), 38);
   elName->helpCtx = hcGPTTypeName;
   insert(elName);
   insert(new TLabel(TRect(23, 1, 32, 2), "New name", elName));

   elType = new TInputLine(TRect(24, 4, 51, 5), GPTYPESTR_LEN);
   elType->helpCtx = hcGPTType;
   insert(elType);

   TSItem *typelist = buildTypeList();

   cbGPTType = new TCombo(TRect(51, 4, 54, 5), (TInputLine*)elType, 
      cbxOnlyList | cbxDisposesList | cbxNoTransfer, typelist);
   insert(cbGPTType);
   insert(new TLabel(TRect(23, 3, 32, 4), "New type", elType));

   elTypeGUID = new TInputLine(TRect(3, 7, 41, 8), 37);
   elTypeGUID->helpCtx = hcGPTTypeGUID;
   insert(elTypeGUID);
   insert(new TLabel(TRect(32, 6, 40, 7), "Type ID", elTypeGUID));

   char guid[40], *ptstr = 0;
#ifdef __QSINIT__
   int ii;
   dsk_gptpartinfo pi;
   dsk_gptpinfo(disk, index, &pi);
   dsk_guidtostr(pi.TypeGUID, guid);

   setstr(elTypeGUID, guid);
   ptstr = guid[0] ? cmd_shellgetmsg(guid) : 0;

   volname = new char[37];
   volname[36] = 0;

   for (ii=0; ii<36; ii++)
      if (!pi.Name[ii]) { volname[ii]=0; break; } else {
         u8t ch = ff_convert(pi.Name[ii],0);
         volname[ii] = ch ? ch : '?';
      }
   setstr(elName, volname);

   if (ptstr) {
      setstr(elType, ptstr);
      free(ptstr);
   }

   dsk_guidtostr(pi.GUID, guid);
#else
   guid[0] = 0;
#endif
   control = new TButton(TRect(44, 6, 54, 8), "~U~pdate", cmOK, bfDefault);
   insert(control);
   control = new TButton(TRect(44, 8, 54, 10), "~C~ancel", cmCancel, bfNormal);
   insert(control);

   control = new TColoredText(TRect(4, 9, 40, 10), guid, 0x7f);
   insert(control);
   control = new TStaticText(TRect(31, 8, 40, 9), "Volume ID");
   insert(control);

   selectNext(False);

   rcguid[0] = 0;
   prevValue[0] = 0;
}

TSetGPTTypeDlg::~TSetGPTTypeDlg() {
#ifdef __QSINIT__
   if (t_values) { free(t_values); t_values = 0; }
   if (t_keys) { free(t_keys); t_keys = 0; }
   if (volname) { delete volname; volname = 0; }
#endif
}

void TSetGPTTypeDlg::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
#ifdef __QSINIT__
   char buf[GPTYPESTR_LEN+1];
   elType->getData(buf);
   if (buf[0] && strcmp(buf, prevValue)) {
      for (int ii=0; ii<t_values->count; ii++)
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
      if (rslt) {
         char *vn = getstr(elName);
         if (strcmp(vn, volname))
            if (strchr(vn,'?')) {
               rslt = False;
               SysApp.errDlg(MSGE_BADNAME);
            }
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
            char *vn = getstr(dlg->elName);
            if (strcmp(vn, dlg->volname)) {
               memset(&pi.Name, 0, sizeof(pi.Name));
               // convert name to unicode
               for (int ii=0; ii<36; ii++)
                  if (!vn[ii]) break; else
                     if ((pi.Name[ii] = ff_convert(vn[ii],1))==0) break;
            }
            memcpy(pi.TypeGUID, dlg->rcguid, 16);
            err = dsk_gptpset(disk, index, &pi);
         }
         if (err) PrintPTErr(err);
#endif
      }
      destroy(dlg);
   }
}
