#include "diskedit.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"
#ifdef __QSINIT__
#include "qssys.h"
#include "qsint.h"
#include "filetab.h"
#include "qcl/qsvdmem.h"
#endif

#define Uses_MsgBox
#define Uses_TView
#define Uses_TRect
#define Uses_TDeskTop
#include "tv.h"
#include "../../hc/qsshell.h"

#define K_OPTS_CNT   8
#define DEB_OPTS_CNT 3

static const char *k_opts_param[K_OPTS_CNT] = { "PRELOAD", "ALTE", "NOLOGO",
   "NOREV", "DEFMSG", "VIEWMEM", "NOAF" ,"NOMTRR" };
static const char *deb_opts_param[DEB_OPTS_CNT] = { "CTRLC", "FULLCABLE",
   "VERBOSE" };

static TStaticText *reptext_common(TStaticText *txo, const char *str,
                                   int const_bounds, int color)
{
   if (!txo||!str) return txo;
   ushort       col;
   TGroup    *owner = txo->owner;
   u32t        slen = strlen(str);
   if (color>=0)
      col = color>0?color:((TColoredText*)txo)->getTheColor();

   TRect rr(txo->getBounds());
   if (!const_bounds) rr.b.x=rr.a.x+slen+1;
   owner->removeView(txo);
   txo->owner = 0;
   SysApp.destroy(txo);
   txo  = color>=0 ? new TColoredText(rr,str,col) :
                     new TStaticText (rr,str);
   owner->insert(txo);
   return txo;
}

void replace_coltxt(TColoredText **txt, const char *str, int const_bounds, ushort color) {
   if (!txt||!*txt||!str) return;

   TColoredText *rc = (TColoredText*)reptext_common(*txt, str, const_bounds, color);
   *txt = rc;
}

void replace_statictxt(TStaticText **txt, const char *str, int const_bounds) {
   if (!txt||!*txt||!str) return;

   TStaticText *rc = reptext_common(*txt, str, const_bounds, -1);
   *txt = rc;
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

char *getsname(const char *filename) {
   static char fnstr[32];
   int         fnlen = strlen(filename);
   if (fnlen>20) {
      strcpy(fnstr, "...");
      strcat(fnstr, filename+fnlen-20);
   } else
      strcpy(fnstr, filename);
   return fnstr;
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
   u32t ii;
   for (ii=0; ii<K_OPTS_CNT; ii++) {
      nm = opts_get(k_opts_param[ii]);
      if (nm) dlg->k_opts->press(ii);
   }
   for (ii=0; ii<DEB_OPTS_CNT; ii++) {
      nm = opts_get(deb_opts_param[ii]);
      if (nm) dlg->deb_opts->press(ii);
   }
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
   nm=opts_get("CALL");
   if (nm) setstr(dlg->cmd_name,nm);

   nm=opts_get("SOURCE");
   if (nm) {
      char lt = nm[0];
#ifdef __QSINIT__
      if (lt=='@') {
         u32t   vdisk, ii;
         qs_vdisk  rd = NEW(qs_vdisk);

         if (rd) {
            if (rd->query(0,&vdisk,0,0,0)==0)
               for (ii = DISK_LDR+1; ii<DISK_COUNT; ii++) {
                  disk_volume_data vi;
                  hlp_volinfo(ii,&vi);
                  if (vi.TotalSectors && vi.Disk==vdisk) { lt = 'A'+ii; break; }
               }
            DELETE(rd);
         }
      }
#endif // __QSINIT__
      if (isdigit(lt)) lt = lt-'0'+'A';
      if (lt) lt = toupper(lt);
      if (isalpha(lt)) dlg->source = lt;
   }
   // just save it
   nm=opts_get("VALIMIT");
   if (nm) dlg->valimit = atoi(nm);

   nm=opts_get("LOGLEVEL");
   if (nm)
      setstr(dlg->k_loglevel, *nm=='3'?"all":(*nm>='0'&&*nm<='2'?nm:"no"));
   else
      setstr(dlg->k_loglevel,"no");
   char *istr = 0;
#ifdef __QSINIT__
   u32t   bfl = sys_queryinfo(QSQI_OS2BOOTFLAGS, 0);
   // boot volume information (based on SOURCE or defaults)
   if (!dlg->source && (bfl&BF_NOMFSHVOLIO)) istr = sprintf_dyn("unknown source");
      else
   {
      u32t  disk;
      long index = vol_index((dlg->source?dlg->source:'A')-'A', &disk);
      if (index<0) istr = sprintf_dyn("unknown partition"); else {
         lvm_partition_data pd;
         if (lvm_partinfo(disk, index, &pd)) {
            istr = sprintf_dyn("LVM %c:\n%s partition %u", pd.Letter,
               strupr(dsk_disktostr(disk,0)), index);
         } else {
            istr = sprintf_dyn("LVM info absent\n%s partition %u",
               strupr(dsk_disktostr(disk,0)), index);
         }
         if (dlg->source) istr = strcat_dyn(istr, "\n(source redirection)");
      }
   }
#endif
   if (istr) {
      replace_coltxt(&dlg->l_lvminfo, istr, 1);
      free(istr);
      istr = 0;
   }

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
   growstr (&opts,"CALL=%s,"      ,dlg->cmd_name   );

   u32t baud = opts_baudrate(),
       nbaud = getuint(dlg->deb_rate), ii;
   if (nbaud!=baud) growuint(&opts,"BAUDRATE=%d,",dlg->deb_rate);
   /// append stored SOURCE option
   if (dlg->source) {
      char sb[16];
      snprintf(sb, 16, "SOURCE=%c,", dlg->source);
      opts = strcat_dyn(opts, sb);
   }
   vl = getstr(dlg->k_loglevel);
   if (vl && *vl)
      if (isdigit(*vl)) growuint(&opts,"LOGLEVEL=%u,",dlg->k_loglevel); else
         if (*vl=='a') opts = strcat_dyn(opts, "LOGLEVEL=3,");

   if (dlg->valimit==0) opts = strcat_dyn(opts, "VALIMIT,"); else
   if (dlg->valimit>0) {
      char sb[24];
      snprintf(sb, 24, "VALIMIT=%u,", dlg->valimit);
      opts = strcat_dyn(opts, sb);
   }

   for (ii=0; ii<K_OPTS_CNT; ii++)
      if (dlg->k_opts->mark(ii)) {
         opts = strcat_dyn(opts, k_opts_param[ii]);
         opts = strcat_dyn(opts, ",");
      }
   for (ii=0; ii<DEB_OPTS_CNT; ii++)
      if (dlg->deb_opts->mark(ii)) {
         opts = strcat_dyn(opts, deb_opts_param[ii]);
         opts = strcat_dyn(opts, ",");
      }
   if (!dlg->k_opts->mark(1)) {
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
   vl = getstr(dlg->k_name);
   opts_bootkernel(vl,opts?opts:"");
   if (opts) free(opts);
}
