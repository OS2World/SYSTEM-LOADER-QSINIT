#include "diskedit.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#ifdef __QSINIT__
#include "qsdm.h"
#include "qcl/cplib.h"
#endif

#define MAX_ENTRIES 128

#define Uses_MsgBox
#define Uses_TView
#define Uses_TDialog
#define Uses_TCollection
#define Uses_TScrollBar
#include "tv.h"

extern TListBox *SelDiskList;

void TSysApp::BootmgrMenu() {
   TDialog    *dlg = makeSelDiskDlg(1);
   TCollection *dl = new TCollection(0,10);
#ifdef __QSINIT__
   u32t       hdds = hlp_diskcount(0), ii, pti, cnt=0,
           *active = hdds?(u32t*)malloc(sizeof(u32t) * hdds):0,
              *dsk = hdds?(u32t*)malloc(sizeof(u32t) * MAX_ENTRIES):0;
   if (hdds && active)
      if (lvm_querybm(active))
         for (ii=0; ii<hdds; ii++)
            if (active[ii]) {
               u32t ssize;
               hlp_disksize(ii, &ssize, 0);

               for (pti=0; pti<32; pti++)
                  if (cnt<MAX_ENTRIES && (active[ii]&1<<pti)) {
                     lvm_partition_data lpd;
                     if (lvm_partinfo(ii, pti, &lpd)) {
                        char fsys[32];
                        char *str = new char[128];
                        u32t ptsize;

                        dsk_ptquery(ii,pti,0,&ptsize,fsys,0,0);

                        sprintf(str,"%-4s  %c%c  %10s  %s",
                           dsk_disktostr(ii,0), lpd.Letter?lpd.Letter:' ',
                              lpd.Letter?':':' ', fsys, getSizeStr(ssize,ptsize));

                        dl->insert(str);
                        dsk[cnt++] = ii|pti<<24;
                     }
                  }
            }
#endif
   if (dl->getCount()==0) {
      messageBox("\x03""No Boot Manager on this PC!",mfError+mfOKButton);
   } else {
      SelDiskList->newList(dl);
      int   res = execView(dlg)==cmOK,
            idx = SelDiskList->focused;
      if (res) {
#ifdef __QSINIT__
         u32t disk = dsk[idx] & QDSK_DISKMASK,
             index = dsk[idx] >> 24;
         int   err = exit_bootvbr(disk, index, 0, 0);
         char errmsg[128];
         sprintf(errmsg, "\x03""Failed to boot this entry (error:%d)", err);
         messageBox(errmsg,mfError+mfOKButton);
#endif
      }
   }
#ifdef __QSINIT__
   if (active) free(active);
   if (dsk) free(dsk);
#endif
   destroy(dlg);
   destroy(dl);
}


void TSysApp::PowerOFF() {
#ifdef __QSINIT__
   u32t    ver = hlp_hosttype()==QSHT_BIOS ? hlp_querybios(QBIO_APM) : 0;
   int  apmerr = ver&0x10000;
#else
   int  apmerr = 1;
#endif
   if (apmerr) {
      errDlg(MSGE_NOAPM);
   } else {
      if (askDlg(MSGA_POWEROFF)) {
#ifdef __QSINIT__
         if (!exit_poweroff(0))
#endif
            errDlg(MSGE_NOTSUPPORTED);
      }
   }
}

int TSysApp::SetCodepage(int format) {
#ifdef __QSINIT__
   TInputLine *cpline;
   TView     *control;
   TDialog       *dlg = new TDialog(TRect(17, 6, 62, 15+format), "Select codepage");
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   dlg->helpCtx  = hcSetCodepage;

   cpline = new TInputLine(TRect(13, 4+format, 19, 5+format), 5);
   cpline->helpCtx = hcSelectCodepage;
   dlg->insert(cpline);

   qs_cpconvert cpl = NEW(qs_cpconvert);
   if (!cpl) return 0;
   u16t     current = cpl->getsyscp();
   TSItem     *list = 0, *next;
   char         tmp[8];
   // no ask in format sequence and selected codepage
   if (format && current) { destroy(dlg); DELETE(cpl); return 1; }
   // get codepahge list (in heap block)
   u16t *cpla = cpl->cplist(), *cplist = cpla;
   while (*cplist) {
      itoa(*cplist++, tmp, 10);
      TSItem *item = new TSItem(tmp,0);
      if (!list) next = list = item; else { next->next = item; next = item; }
   }
   free(cpla); cpla = 0;

   control = new TCombo(TRect(19, 4+format, 22, 5+format), cpline, cbxOnlyList |
                       cbxDisposesList | cbxNoTransfer, list);
   dlg->insert(control);
   dlg->insert(new TLabel(TRect(2, 4+format, 12, 5+format), "Codepage:", cpline));

   control = new TButton(TRect(22, 6+format, 32, 8+format), "O~K~", cmOK, bfDefault);
   dlg->insert(control);

   control = new TButton(TRect(32, 6+format, 42, 8+format), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);

   control = new TStaticText(TRect(3, 2, 42, 3+format), format? "There is no codepage "
             "selected, but HPFS format requires one. Default is 850.":
             "Codepage affects file i/o in QSINIT.");
   dlg->insert(control);
   dlg->selectNext(False);

   if (!current) current = 850;
   itoa(current, tmp, 10);
   setstr(cpline, tmp);

   int  res = execView(dlg)==cmOK;
   if (res) {
      u32t cp = getuint(cpline);
      res = cpl->setsyscp(cp);
   } else
      res = -1;

   DELETE(cpl);
   destroy(dlg);
   return res;
#else
   return -1;
#endif
}

//------------------------------------------------------------------
static TColoredText *prc_state = 0;
static TDialog      *prc_dlg   = 0;
static char         *prc_head  = 0;

static TDialog* makePercentDlg() {
   TDialog* dlg = new TDialog(TRect(18, 10, 61, 13), prc_head);
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;

   prc_state = new TColoredText(TRect(2, 1, 41, 2), "", 0x7C);
   dlg->insert(prc_state);

   dlg->selectNext(False);
   return dlg;
}

void _std TSysApp::PercentDlgCallback(u32t percent, u32t size) {
   static u32t lsize = 0, lprc;
   int        insdlg = 0,
              drawme = 0;

   if (!prc_dlg && percent<50) {
      prc_dlg = makePercentDlg();
      insdlg  = 1;
      lsize   = 0;
      lprc    = FFFF;
   }
   if (prc_dlg && (size-lsize>=_1MB || percent!=lprc)) {
      char buf[80], pvbuf[24];
      TRect rr(prc_state->getBounds());
      int width=rr.b.x-rr.a.x, len=width*percent/100;

      memset(buf,177,width);
      if (len) memset(buf,219,len);
      buf[width]=0;
      if (percent) {
         sprintf(pvbuf,"%02d%%",percent);
      } else
      if (size) {
         sprintf(pvbuf,"%d Mb",size>>20);
      } else
         pvbuf[0] = 0;
      if (pvbuf[0]) {
         len = strlen(pvbuf);
         memcpy(buf+(width-len)/2,pvbuf,len);
      }
      prc_dlg->removeView(prc_state);
      prc_state=new TColoredText(rr,buf,0x7C);
      prc_dlg->insert(prc_state);
      lsize  = size;
      drawme = 1;
   }
   if (insdlg) {
      SysApp.insert(prc_dlg);
      prc_dlg->show();
   }
   if (drawme) TProgram::application->redraw();
}

void TSysApp::PercentDlgOn(const char *text) {
   if (prc_dlg || !text) return;
   prc_head = strdup(text);
}

void TSysApp::PercentDlgOff(TView *owner) {
   if (prc_head) { free(prc_head); prc_head = 0; }
   if (!prc_dlg) return;
   remove(prc_dlg);
   destroy(prc_dlg);
   // restore focus on parent dialog
   if (owner) setCurrent(owner, enterSelect);
   prc_state = 0;
   prc_dlg   = 0;
}
