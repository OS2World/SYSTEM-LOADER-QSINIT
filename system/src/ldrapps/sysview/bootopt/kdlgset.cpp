#include "diskedit.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"

#define Uses_MsgBox
#define Uses_TView
#define Uses_TRect
#define Uses_TDeskTop
#include "tv.h"
#include "../../hc/qsshell.h"

void replace_coltxt(TColoredText **txt, const char *str, int const_bounds, ushort color) {
   if (!txt||!*txt||!str) return;
   TColoredText *txo = *txt;
   ushort    col = color?color:txo->getTheColor();
   TGroup *owner = txo->owner;

   TRect rr(txo->getBounds());
   if (!const_bounds) rr.b.x=rr.a.x+strlen(str)+1;
   owner->removeView(txo);
   txo->owner = 0;
   SysApp.destroy(txo);
   txo  = new TColoredText(rr,str,col);
   owner->insert(txo);
   *txt = txo;
}

void replace_statictxt(TStaticText **txt, const char *str, int const_bounds) {
   if (!txt||!*txt||!str) return;
   TStaticText *txo = *txt;
   TGroup *owner = txo->owner;

   TRect rr(txo->getBounds());
   if (!const_bounds) rr.b.x=rr.a.x+strlen(str)+1;
   owner->removeView(txo);
   txo->owner = 0;
   SysApp.destroy(txo);
   txo  = new TStaticText(rr,str);
   owner->insert(txo);
   *txt = txo;
}

void setstr(TInputLine *ln, const char *str) {
   char buf[512];
   strncpy(buf,str,512); buf[511]=0;
   ln->setData(buf);
}

void setstrn(TInputLine *ln, const char *str, int maxlen) {
   char buf[512];
   if (maxlen>511) maxlen=511;
   strncpy(buf,str,maxlen); buf[maxlen]=0;
   ln->setData(buf);
}

char *getstr(TInputLine *ln) {
   static char buf[512];
   char *ab=0, *gptr;
   if (ln->dataSize()>511) ab = (char*)malloc(ln->dataSize()+10);
   gptr  = ab?ab:buf;
   *gptr = 0;
   ln->getData(gptr);
   if (ab) {
      memcpy(buf,ab,511);
      buf[511]=0;
      free(ab);
   }
   // trim value
   ab=buf;
   while (*ab==' ') ab++;
   if (ab!=buf) memmove(buf,ab,strlen(ab)+1);
   ab=buf+strlen(buf);
   while (ab!=buf)
      if (*--ab!=' ') break; else *ab=0;

   return buf;
}

u32t getuint(TInputLine *ln) {
   char *vl = getstr(ln);
   // prevent octal conversion
   while (*vl&&(*vl==' '||*vl=='\t')) vl++;
   while (*vl=='0'&&vl[1]>='0'&&vl[1]<='9') vl++;
   return *vl?strtoul(vl,NULL,0):FFFF;
}

u64t getui64(TInputLine *ln) {
   char *vl = getstr(ln);
   // prevent octal conversion
   while (*vl&&(*vl==' '||*vl=='\t')) vl++;
   while (*vl=='0'&&vl[1]>='0'&&vl[1]<='9') vl++;
   return *vl?strtoull(vl,NULL,0):FFFF64;
}

static void setint(TInputLine *ln, const char *str, int hex_chars) {
   char buf[32];
   u32t value = strtoul(str,0,0);
   if (hex_chars) {
      char fmt[8];
      sprintf(fmt,"0x%%0%dX",hex_chars);
      sprintf(buf,fmt,value);
   } else {
      sprintf(buf,"%u",value);
   }
   ln->setData(buf);
}

long getRadioIdx(TRadioButtons *rb, long maxidx) {
   for (long ll=0; ll<maxidx; ll++) 
      if (rb->mark(ll)) return ll;
   return -1;
}

TRect getwideBoxRect(int addlines) {
   TRect r(0, 0, 60, 9+addlines);
   r.move((TProgram::deskTop->size.x - r.b.x) / 2,
          (TProgram::deskTop->size.y - r.b.y) / 2);
   return r;
}

char *getDiskName(u32t disk, char *buf) {
   static char lbuf[48];
   if (!buf) buf=lbuf;

   if (disk & DSK_FLOPPY) sprintf(buf, "FDD %i", disk&DSK_DISKMASK); else
   if (disk == DSK_VIRTUAL) strcpy(buf,"Boot partition"); else
   if (disk == DSK_VIRTUAL+1) strcpy(buf,"Virtual disk"); else
      sprintf(buf, "HDD %i", disk&DSK_DISKMASK);
   return buf;
}

int SetupBootDlg(TKernBootDlg *dlg, char *kernel, char *opts) {
   opts_prepare(opts);
   if (kernel) setstr(dlg->k_name,kernel);
   char *nm=opts_get("RESTART");
   if (nm) return 0;
   nm=opts_get("NOREV");
   if (nm) dlg->k_opts->press(3);
   nm=opts_get("NOLOGO");
   if (nm) dlg->k_opts->press(2);
   nm=opts_get("PRELOAD");
   if (nm) dlg->k_opts->press(0);
   nm=opts_get("ALTE");
   if (nm) dlg->k_opts->press(1);
   nm=opts_get("NOLFB");
   if (nm) dlg->k_opts->press(4);
   nm=opts_get("VIEWMEM");
   if (nm) dlg->k_opts->press(5);
   nm=opts_get("CTRLC");
   if (nm) dlg->deb_opts->press(0);
   nm=opts_get("FULLCABLE");
   if (nm) dlg->deb_opts->press(1);
   nm=opts_get("VERBOSE");
   if (nm) dlg->deb_opts->press(2);
   nm=opts_get("CFGEXT");
   if (nm) setstr(dlg->k_cfgext,nm);
   nm=opts_get("LOGSIZE");
   if (nm) setint(dlg->k_logsize,nm,0);
   nm=opts_get("MEMLIMIT");
   if (nm) setint(dlg->k_limit,nm,0);
   nm=opts_get("SYM");
   if (nm) setstr(dlg->deb_symname,nm);
   nm=opts_get("DBPORT");
   if (nm) setint(dlg->deb_port,nm,4);

   char bstr[32];
   if (!kernel) { // direct call - read port from QSINIT
      u32t port = opts_port();
      if (port) {
         sprintf(bstr,"0x%04X",port);
         setstr(dlg->deb_port,bstr);
      }
   }

   nm=opts_get("LETTER");
   if (nm) setstr(dlg->k_letter,nm); else {
      bstr[0]=opts_bootdrive();
      bstr[1]=0;
      setstr(dlg->k_letter,bstr);
   }

   u32t baud = opts_baudrate();
   sprintf(bstr,"%u",baud);
   setstr(dlg->deb_rate,bstr);

   nm=opts_get("PKEY");
   *bstr=0;
   if (nm) {
      baud = strtoul(nm,0,0)>>8;
      if (baud==0x12) dlg->k_opts->press(1); else {
         sprintf(bstr,"Alt-F%c",baud-0x68+0x31);
      }
   }
   setstr(dlg->k_pkey,*bstr?bstr:"none");

   opts_free();
   return 1;
}

void SetupLdrBootDlg(TLdrBootDialog *dlg, char *file) {
   if (file) setstr(dlg->bf_name,file);
}

int CheckBootDlg(TKernBootDlg *dlg) {
   char *vl=getstr(dlg->k_letter);
   if (*vl&&(strlen(vl)>1||(toupper(*vl)<'A'||toupper(*vl)>'X'))) {
      messageBox("\x03""Incorrect boot drive letter!",mfError+mfOKButton);
      dlg->setCurrent(dlg->k_letter, TView::normalSelect);
      return 0;
   }
   vl=getstr(dlg->k_limit);
   if (*vl) {
      if (atoi(vl)<16) {
         messageBox("\x03""Memory limit can`t be < 16Mb!",mfError+mfOKButton);
         dlg->setCurrent(dlg->k_limit, TView::normalSelect);
         return 0;
      }
   }
   return 1;
}

#ifdef __QSINIT__
static void growuint(char **opts, char *fmt, TInputLine *ln) {
   char spbuf[128];
   u32t lsz=getuint(ln);
   if (lsz!=FFFF) {
      sprintf(spbuf,fmt,lsz);
      *opts = strcat_dyn(*opts,spbuf);
   }
}

static void growstr(char **opts, char *fmt, TInputLine *ln) {
   char spbuf[512];
   char *vl=getstr(ln);
   if (vl&&*vl) {
      sprintf(spbuf,fmt,vl);
      *opts = strcat_dyn(*opts,spbuf);
   }
}
#endif

void RunKernelBoot(TKernBootDlg *dlg) {
   char *opts = 0, *vl;
#ifdef __QSINIT__
   growuint(&opts,"LOGSIZE=%d,"   ,dlg->k_logsize  );
   growuint(&opts,"MEMLIMIT=%d,"  ,dlg->k_limit    );
   growuint(&opts,"DBPORT=0x%04X,",dlg->deb_port   );
   growstr (&opts,"SYM=%s,"       ,dlg->deb_symname);
   growstr (&opts,"CFGEXT=%s,"    ,dlg->k_cfgext   );
   growstr (&opts,"LETTER=%s,"    ,dlg->k_letter   );

   u32t baud = opts_baudrate(),
       nbaud = getuint(dlg->deb_rate);
   if (nbaud!=baud) growuint(&opts,"BAUDRATE=%d,",dlg->deb_rate);

   if (dlg->k_opts->mark(0)) opts = strcat_dyn(opts,"PRELOAD,");
   if (dlg->k_opts->mark(2)) opts = strcat_dyn(opts,"NOLOGO,");
   if (dlg->k_opts->mark(3)) opts = strcat_dyn(opts,"NOREV,");
   if (dlg->k_opts->mark(4)) opts = strcat_dyn(opts,"NOLFB,");
   if (dlg->k_opts->mark(5)) opts = strcat_dyn(opts,"VIEWMEM,");
   if (dlg->deb_opts->mark(0)) opts = strcat_dyn(opts,"CTRLC,");
   if (dlg->deb_opts->mark(1)) opts = strcat_dyn(opts,"FULLCABLE,");
   if (dlg->deb_opts->mark(2)) opts = strcat_dyn(opts,"VERBOSE,");
   if (dlg->k_opts->mark(1)) opts = strcat_dyn(opts,"PKEY=0x1200,"); else {
       vl=getstr(dlg->k_pkey);
       if (vl&&vl[5]>='1'&&vl[5]<='5') {
          char buf[32];
          sprintf(buf,"PKEY=0x%02X00,",(u32t)vl[5]-0x31+0x68);
          opts = strcat_dyn(opts,buf);
       }
   }
   if (opts) {
      vl = opts+strlen(opts)-1;
      if (*vl==',') *vl = 0;
   }
#endif
   vl=getstr(dlg->k_name);
   opts_bootkernel(vl,opts?opts:"");
   if (opts) free(opts);
}
