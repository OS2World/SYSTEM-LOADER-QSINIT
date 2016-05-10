#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TScreen
#define Uses_TButton
#define Uses_TLabel
#define Uses_TFileDialog
#define Uses_TRadioButtons
#define Uses_TInputLine
#include <tv.h>
#include "diskedit.h"
#include "binedit.h"
#include <stdlib.h>
#include <ctype.h>
#include "tcolortx.h"
#ifndef __QSINIT__     // for fsize
#include "sp_defs.h"
#else
#include "qsint.h"     // for disk count
#include "qsdm.h"
#include "qsshell.h"
#include "qsconst.h"
#include "errno.h"
#include "qcl/rwdisk.h"
#endif
#include "diskdlg.h"
#include "diskact.h"
#include "qsio.h"

#define DISK_BUFFER   (32768)

#define LARGEBOX_INC    7

static char *getfname(int open) {
   static char fname[MAXPATH+1];

   TFileDialog *fo = new TFileDialog("*.*", open?"Import file":"Export to file",
      "~F~ile name", open?fdOpenButton:fdOKButton, open?hhSectFnRead:hhSectFnWrite);
   fo->helpCtx = hcFileDlgCommon;
   int     res = SysApp.execView(fo)==cmFileOpen;
   if (res) fo->getFileName(fname);
   SysApp.destroy(fo);
   if (!res) return 0;
   return fname;
}

TDiskCopyDialog::TDiskCopyDialog() :
       TDialog(TRect(13, 9, 67, 14), "Copying..."),
       TWindowInit(TDiskCopyDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   sectorStr = new TColoredText(TRect(3, 2, 31, 3), "", 0x79);
   insert(sectorStr);
   control = new TButton(TRect(42, 2, 52, 4), "~C~ancel", cmCancel, bfDefault);
   insert(control);
   percentStr = new TColoredText(TRect(32, 2, 40, 3), "", 0x7f);
   insert(percentStr);
   selectNext(False);
   source.isFile = 1; source.file = 0;
   destin.isFile = 1; destin.file = 0;
   bytes  = 0;
   buf32k = (u8t*)malloc(DISK_BUFFER);
}

TDiskCopyDialog::~TDiskCopyDialog() {
   if (buf32k) { free(buf32k); buf32k = 0; }
}

void TDiskCopyDialog::processNext() {
   // end reached?
   if (!bytes) { stopreason = 0; endModal(cmCancel); return; }
   // sectors in 32k
   u32t sin32k = DISK_BUFFER;
   // overflow?
   if (bytes < sin32k) sin32k = bytes;
   // read data
   u32t readed,
       s_sc32k = source.isFile? 0 : (sin32k+source.ssize-1)/source.ssize,
       d_sc32k = destin.isFile? 0 : (sin32k+destin.ssize-1)/destin.ssize;

   if (source.isFile) {
      u32t until_end = fsize(source.file) - ftell(source.file);
      if (until_end < sin32k) {
         readed  = dsk_read(destin.disk, destin.pos, d_sc32k, buf32k);
         if (readed!=d_sc32k) { stopreason = 1; endModal(cmCancel); return; }
         sin32k  = until_end;
      }
      readed  = fread(buf32k, 1, sin32k, source.file);
   } else {
      // can be at the end copy between disks with different sector size
      if (d_sc32k * destin.ssize > s_sc32k * source.ssize) {
         readed  = dsk_read(destin.disk, destin.pos, d_sc32k, buf32k);
         if (readed!=d_sc32k) { stopreason = 1; endModal(cmCancel); return; }
      }
      readed     = dsk_read(source.disk, source.pos, s_sc32k, buf32k);
      source.pos+= readed;
      readed    *= source.ssize;
   }
   if (readed) {
      u32t written = 0;
      if (destin.isFile) written = fwrite(buf32k, 1, readed, destin.file); else {
         written    = dsk_write(destin.disk, destin.pos, d_sc32k, buf32k);
         destin.pos+= written;
         written   *= destin.ssize;
      }
      if (written < readed) { stopreason = 2; endModal(cmCancel); return; }
      if (bytes < written) bytes=0; else bytes -= written;
      if (!bytes) { stopreason = 0; endModal(cmCancel); } else {
         char str[96];
         sprintf(str, "%09LX of %09LX bytes", total-bytes, total);
         replace_coltxt(&sectorStr, str);
         u32t ll = (u64t)(total-bytes) * 10000 / total;
         sprintf(str, " %3u.%02u%%", ll/100, ll%100);
         replace_coltxt(&percentStr, str);
         drawView();
      }
   }
   if (readed<sin32k) { stopreason = 1; endModal(cmCancel); }
}

void TDiskCopyDialog::startFileToDisk(FILE *src, u32t disk, u64t start, u32t sectors) {
   source.isFile = 1; source.file = src;
   destin.isFile = 0; destin.disk = disk; destin.pos = start;
   stopreason    = 0;
   dsk_size(disk, &destin.ssize, 0);
   // auto calc length in sectors
   bytes = fsize(src);
   if (sectors * destin.ssize < bytes) bytes = (u64t)sectors * destin.ssize;
   total = bytes;
}

void TDiskCopyDialog::startDiskToFile(u32t disk, u64t start, FILE *dst, u32t sectors) {
   source.isFile = 0; source.disk = disk; source.pos = start;
   destin.isFile = 1; destin.file = dst;
   stopreason    = 0;
   dsk_size(disk, &source.ssize, 0);
   bytes = (u64t)sectors * source.ssize;
   total = bytes;
}

void TDiskCopyDialog::startDiskToDisk(u32t sdisk, u64t sstart, u32t ddisk,
                                      u64t dstart, u32t sectors)
{
   source.isFile = 0; source.disk = sdisk; source.pos = sstart;
   destin.isFile = 0; destin.disk = ddisk; destin.pos = dstart;
   stopreason    = 0;
   dsk_size(sdisk, &source.ssize, 0);
   dsk_size(ddisk, &destin.ssize, 0);
   bytes = (u64t)sectors * source.ssize;
   total = bytes;
}

void TSysApp::doDiskCopy(int mode, u32t disk, u64t pos, u32t len, const char *fpath,
                         u32t dstdisk, u64t dstpos)
{
   int  stop = -1;
   u64t epos = 0;
   copyDlg   = new TDiskCopyDialog();

   if (mode==SCSEL_COPY) {
      copyDlg->startDiskToDisk(disk, pos, dstdisk, dstpos, len);
      execView(copyDlg);
      stop = copyDlg->stopreason;
      epos = copyDlg->source.pos;
   } else {
      FILE *fp = 0;
      if (mode==SCSEL_SAVE) {
         fp = fopen(fpath, "wb");
         if (fp) copyDlg->startDiskToFile(disk, pos, fp, len);
      } else
      if (mode==SCSEL_READ) {
         fp = fopen(fpath, "rb");
         if (fp) copyDlg->startFileToDisk(fp, disk, pos, len);
      }
      if (fp) {
         execView(copyDlg);
         stop = copyDlg->stopreason;
         epos = mode==SCSEL_READ?copyDlg->destin.pos:copyDlg->source.pos;
         fclose(fp);
      } else
         errDlg(MSGE_FILESIZEERR);
   }
   destroy(copyDlg);
   copyDlg = 0;

   if (stop>0)  {
      errDlg(MSGE_READERR+stop-1);
      // pos in editor window
      TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
      if (win) win->sectEd->processKey(TLongEditor::posSet,epos);
   } else
      DiskChanged(mode==SCSEL_COPY?dstdisk:disk);
}

u64t TSysApp::SelectSector(int mode, u32t disk, long vol, u64t start,
                           u64t length, u64t cpos)
{
   TView *control;
   TInputLine  *sgHddPos, *sgLength = 0;
   TStaticText *sgHddName, *sgHddMax, *sgHddMin;
   int dmode = mode==SCSEL_GOTO||mode==SCSEL_TGT?0:1;
   static const char *dlgtitle[5] = { "Jump to sector", "Export sectors to file",
                                      "Import sectors from file",
                                      "Copy sectors to another disk",
                                      "Target sector for copying" };
   static int          dlghelp[5] = { hcSectGoto, hcSectExport, hcSectImport,
                                      hcSectCopy};
   static const char   *oktext[5] = { "~G~oto", "~S~ave", "~R~ead", "~O~k",
                                      "~C~opy" };

   TDialog* dlg = new TDialog(TRect(16, 7-dmode, 64, 15+dmode), dlgtitle[mode]);
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   dlg->helpCtx  = dlghelp[mode];

   sgHddPos = new TInputLine(TRect(10, 3, 28, 4), 15);
   dlg->insert(sgHddPos);
   dlg->insert(new THistory(TRect(28, 3, 31, 4), sgHddPos, hhSectorPos));
   dlg->insert(new TLabel(TRect(2, 3, 8, 4), mode==SCSEL_GOTO?"Goto":"Start", sgHddPos));
   if (dmode) {
      sgLength = new TInputLine(TRect(10, 5, 28, 6), 15);
      dlg->insert(sgLength);
      dlg->insert(new THistory(TRect(28, 5, 31, 6), sgLength, hhSectorLen));
      dlg->insert(new TLabel(TRect(2, 5, 9, 6), "Length", sgLength));
   }
   control = new TButton(TRect(36, 2, 46, 4), oktext[mode], cmOK, bfDefault);
   dlg->insert(control);

   control = new TButton(TRect(36, 4, 46, 6), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);

   u32t sectsize = 512;
   u64t  dsksize = dsk_size64(disk, &sectsize);
   if (!length) length = dsksize;
   char buf[96], *sstr;
   getDiskName(disk,buf);
   sstr = buf + strlen(buf);
   if (vol>=0) sstr+=sprintf(sstr," Volume %u",vol);
   strcpy(sstr, ": ");
   // print size
   sstr = getSizeStr(sectsize, length, True);
   strcat(buf, sstr);

   sgHddName = new TStaticText(TRect(3, 1, 34, 2), buf);
   dlg->insert(sgHddName);

   length+=start;
   snprintf(buf, 96, "Last : %11Lu (0x%LX)", length-1, length-1);
   sgHddMax = new TStaticText(TRect(3, 6+dmode*2, 36, 7+dmode*2), buf);
   dlg->insert(sgHddMax);

   snprintf(buf, 96, "First: %11Lu (0x%LX)", start, start);
   sgHddMin = new TStaticText(TRect(3, 5+dmode*2, 36, 6+dmode*2), buf);
   dlg->insert(sgHddMin);
   dlg->selectNext(False);

   u64t      rc = 0;
   u32t initlen = 1;
   char *sfname = 0;

   do {
      if (dmode) {
         char str[32];
         ulltoa(cpos, str, 10);
         sgHddPos->setData(str);

         if (mode==SCSEL_READ) {
            char *name = getfname(1);
            if (!name) break;
            u32t sz = opts_fsize(name);
            if (sz==FFFF) { errDlg(MSGE_FILESIZEERR); break; }
            initlen = (sz + sectsize-1) / sectsize;
            sfname  = strdup(name);
         }
         utoa(initlen, str, 10);
         sgLength->setData(str);
      }

      rc = FFFF64;
      if (execView(dlg)==cmOK) {
         rc = getui64(sgHddPos);
         if (rc==FFFF64) errDlg(MSGE_INVVALUE); else
         if (rc<start || rc>=start+length) errDlg(MSGE_RANGE); else
         if (dmode) {
            u64t dstpos,
                   llen = getui64(sgLength);
            u32t    len, dstdisk, dstssize;
            // limit copy size to 2TB ;)
            if (!llen || llen>=_4GBLL) { errDlg(MSGE_COPYTLENGTH); break; }
            len = llen;
            // end of disk?
            if ((u64t)rc+len >(u64t)start+length) {
               infoDlg(MSGI_LENTRUNCDISK);
               len = start+length-rc;
            }
            // get target sector location for disk->disk mode
            if (mode==SCSEL_COPY) {
               dstdisk = GetDiskDialog(1,1,SELDISK_COPYTGT);
               if (dstdisk==FFFF) break;
               dstpos  = SelectSector(SCSEL_TGT, dstdisk);
               if (dstpos==FFFF64) break;
               u64t ddsksize = dsk_size64(dstdisk, &dstssize);
               // copy will reach the end of target disk
               if (dstpos*dstssize+(u64t)len*sectsize > ddsksize*dstssize) {
                  infoDlg(MSGI_LENTRUNCTDISK);
                  len = (u64t)(ddsksize - dstpos) * dstssize / sectsize;
               }
               if (dstdisk==disk) {
                  if (rc==dstpos) { infoDlg(MSGI_SAMEPOS); break; }
                  // check for interference (too lazy do make reverse copying now ;))
                  u32t sper32k = DISK_BUFFER/sectsize;
                  if (dstpos<rc && rc-dstpos<sper32k || dstpos>rc && dstpos<rc+len) {
                     errDlg(MSGE_INTERFERENCE); break;
                  }
               }
            }
            // compare with source file size
            if (mode==SCSEL_READ && len>initlen) {
               infoDlg(MSGI_LENTRUNCFILE);
               len = initlen;
            }
            char sname[32];
            if (len==1) sprintf(sname, " %Lu", rc); else
               sprintf(sname, "s %Lu..%Lu", rc, rc+len-1);

            if (mode==SCSEL_SAVE) {
               char *name = getfname(0);
               if (!name) break;
               sfname  = strdup(name);
            }
            if (mode==SCSEL_COPY) {
               u64t dstlen = ((u64t)len * sectsize + dstssize - 1) / dstssize;
               if (dstlen==1) sprintf(buf, " %Lu", dstpos); else
                  sprintf(buf, "s %Lu..%Lu", dstpos, dstpos+dstlen-1);

               if (messageBoxRect(getwideBoxRect(), mfConfirmation + mfYesButton +
                  mfNoButton, "\x3""Are you sure you want to copy sector%s "
                     "to sector%s on disk %s?", sname, buf,
                         getDiskName(dstdisk))!=cmYes) break;
            } else {
               int ll = strlen(sfname);
               if (messageBoxRect(getwideBoxRect(), mfConfirmation + mfYesButton +
                  mfNoButton, mode==SCSEL_SAVE?
                  "\x3""Are you sure you want to write sector%s to file \"%s\"?":
                  "\x3""Are you sure you want to replace sector%s to data from file \"%s\"?",
                     sname, ll<52?sfname:sfname+(ll-52))!=cmYes) break;
            }
            // check available space on target volume
            if (mode==SCSEL_SAVE) {
               if ((u64t)sectsize * len > opts_freespace(toupper(sfname[0])-'A')) {
                  errDlg(MSGE_LOWSPACE);
                  break;
               }
               if ((u64t)sectsize * len > FFFF) { errDlg(MSGE_GT4GB); break; }
            }
            doDiskCopy(mode, disk, rc, len, sfname, dstdisk, dstpos);
         }
      }
   } while (false);
   if (sfname) free(sfname);
   destroy(dlg);
   return rc;
}

// *************************************************************************

TDiskSearchDialog::TDiskSearchDialog() :
       TDialog(TRect(13, 9, 67, 14), "Searching..."),
       TWindowInit(TDiskSearchDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   sectorStr = new TColoredText(TRect(3, 2, 31, 3), "", 0x79);
   insert(sectorStr);
   control = new TButton(TRect(42, 2, 52, 4), "~C~ancel", cmCancel, bfDefault);
   insert(control);
   percentStr = new TColoredText(TRect(33, 2, 41, 3), "", 0x7f);
   insert(percentStr);
   selectNext(False);
   setLimits(0, FFFF, 0);
}

void TDiskSearchDialog::processNext() {
   // must be reinited before continue search...
   if (stop==stopFound) return;
   // end reached?
   if (sector>=end) {
      stop = stopEnd;
      endModal(cmCancel);
   }
   // sectors in 32k
   u32t sin32k = DISK_BUFFER/sectorsize;
   // overflow?
   if (end+1-sector < sin32k) sin32k = end+1-sector;
   // read data
   u32t readed;
   if (disk==DISK_MEMORY) {
      for (u32t ii=0; ii<sin32k; ii++)
         if (opts_memread(sector+ii<<8, buf32k + sectorsize*(ii+1)))
            readed++; else break;
   } else
      readed = dsk_read(disk, sector, sin32k, buf32k + sectorsize);

   if (readed) {
      THexEditorData &dt = SysApp.searchData;
      // warning: posinsec can be negative
      u8t *spos = buf32k + sectorsize + posinsec,
          *epos = buf32k + (readed+1) * sectorsize - dt.size,
         *sdata = (u8t*)dt.data,
           f1ch = *sdata,
        isal1st = isalpha(f1ch);

      while (spos<=epos) {
         // case insensitive
         if (SysApp.icaseSrch) {
            if (isal1st) {
               while (spos<=epos) {
                  if ((f1ch&~0x20)==(*spos&~0x20)) break;
                  if (spos==epos) { spos=0; break; }
                  spos++;
               }
            } else
            if (*spos!=f1ch) spos = (u8t*)memchr(spos, f1ch, epos-spos+1);
            if (!spos) break;

            int ii;
            for (ii=0; ii<dt.size; ii++)
               if (toupper(spos[ii])!=toupper(sdata[ii])) break;
            if (ii>=dt.size) { stop = stopFound; break; }
            spos++;
         } else {
            if (*spos!=f1ch) {
               spos = (u8t*)memchr(spos, f1ch, epos-spos+1);
               if (!spos) break;
            }
            if (dt.size==1) { stop = stopFound; break; } else
            if (spos[1]==sdata[1])
               if (memcmp(spos,sdata,dt.size)==0) { stop = stopFound; break; }
            spos++;
         }
      }
      if (stop==stopFound) {
         posinsec = spos - buf32k;
         sector--;
         while (posinsec>=sectorsize) { posinsec-=sectorsize; sector++; }

         endModal(cmCancel);
      } else {
         char str[64];

         // copy last sector for next search
         memcpy(buf32k, buf32k + readed * sectorsize, sectorsize);
         sector  += readed;
         posinsec = -dt.size+1;
         // end of disk was reached
         if (sector-1 >= end) {
            stop = stopEnd;
            endModal(cmCancel);
         } else {
            if (disk==DISK_MEMORY)
               sprintf(str, "Pos %010LX of %010X", sector<<8, end<<8);
            else
               sprintf(str, "Sector %09LX of %09LX", sector, end);
            replace_coltxt(&sectorStr, str);
            u32t ll = (sector - start) * 10000 / (end - start);
            sprintf(str, " %3u.%02u%%", ll/100, ll%100);
            replace_coltxt(&percentStr, str);
            drawView();
         }
      }
   }
   if (readed<sin32k) {
      if (stop==stopVoid) stop = stopReadErr;
      endModal(cmCancel);
   }
}

void TDiskSearchDialog::setLimits(u64t startpos, u64t endpos, u32t ofs_instart) {
   start    = startpos;
   end      = endpos;
   sector   = startpos;
   posinsec = ofs_instart;
   stop     = stopVoid;
}

void TSysApp::SearchBinStart(Boolean is_disk) {
   TView *control;
   int   largeBox = TScreen::screenHeight>=32 ? LARGEBOX_INC : 0;
   TDialog   *dlg = new TDialog(TRect(0, 1, 80, 22+largeBox), "Search binary data");
   if (!dlg) return;
   dlg->options  |= ofCenterX | ofCenterY;
   dlg->helpCtx   = hcSectFind;

   control = new TScrollBar(TRect(78, 1, 79, 17+largeBox));
   dlg->insert(control);
   THexEditor *edBox = new THexEditor(TRect(1, 1, 78, 17+largeBox), (TScrollBar*)control);
   dlg->insert(edBox);
   TCheckBoxes *opts = new TCheckBoxes(TRect(2, 18+largeBox, 24, 19+largeBox),
      new TSItem("case insensitive", 0));
   dlg->insert(opts);
   if (icaseSrch) opts->press(0);

   control = new TButton(TRect(56, 18+largeBox, 66, 20+largeBox), "~F~ind",
      cmOK, bfDefault);
   dlg->insert(control);

   control = new TButton(TRect(67, 18+largeBox, 77, 20+largeBox), "~C~ancel",
      cmCancel, bfNormal);
   dlg->insert(control);

   dlg->selectNext(Boolean(False));

   if (!searchData.data) {
      searchData.data = malloc(1);
      searchData.size = 1;
      *(char*)searchData.data = 0;
   }
   edBox->setData(&searchData);

   if (execView(dlg)==cmOK) {
      icaseSrch = opts->mark(0);
      edBox->getData(&searchData);
      destroy(dlg);

      SearchBinNext(is_disk);
   } else
      destroy(dlg);
}

void TSysApp::SearchBinNext(Boolean is_disk) {
   TMemEdWindow     *w0 = 0;
   TDskEdWindow     *w1 = 0;
   TLongEditor  *hxview = 0;
   u64t    total, stsec;

   if (is_disk) {
      w1 = (TDskEdWindow*)windows[TWin_SectEdit];
      if (!w1) return;
      // up to 512 bytes (to roll back by 1 sector max)
      if (searchData.size>512) { errDlg(MSGE_SRCHLONG); return; }
      // setup search dialog
      searchDlg       = new TDiskSearchDialog();
      searchDlg->disk = w1->disk;
      total           = dsk_size64(w1->disk, &searchDlg->sectorsize);
      hxview          = w1->sectEd;
   } else {
      w0 = (TMemEdWindow*)windows[TWin_MemEdit];
      if (!w0) return;
      // up to 256 bytes (to roll back by 1 "cluster" max)
      if (searchData.size>w0->led.clustersize) { errDlg(MSGE_SRCHLONG); return; }

      searchDlg       = new TDiskSearchDialog();
      searchDlg->disk = DISK_MEMORY;
      searchDlg->sectorsize = w0->led.clustersize;
      total           = w0->led.clusters;
      hxview          = w0->memEd;
   }
   int line          = hxview->cursorLine(&stsec);
   searchDlg->buf32k = (u8t*)malloc(DISK_BUFFER + searchDlg->sectorsize);
   lastSearchMode    = is_disk;
   // searching from the next line ...
   if (++line >= searchDlg->sectorsize>>4) {
      line = 0;
      if (++stsec >= total) stsec = 0;
   }
   searchDlg->setLimits(stsec, total-1, line<<4);
   // go!
   execView(searchDlg);

   lastSearchStop = searchDlg->stop;
   free(searchDlg->buf32k);

   u64t t_sector = searchDlg->sector;
   u32t t_secpos = searchDlg->posinsec;

   destroy(searchDlg);
   searchDlg = 0;
   // show message boxes after zeroing searchDlg (else ::idle will kill as)
   if (lastSearchStop == TDiskSearchDialog::stopEnd) {
      infoDlg(MSGI_SRCHNOTFOUND);
   } else {
      // pos in both cases: it was founded or cancel pressed
      hxview->posToCluster(t_sector, t_secpos);
      if (lastSearchStop == TDiskSearchDialog::stopReadErr)
         errDlg(MSGE_SRCHREADERR);
   }
}

// *************************************************************************

TDiskBootDialog::TDiskBootDialog(u32t disk, long index) :
       TDialog(TRect(17, 9, 63, 14), "Boot"),
       TWindowInit(TDiskSearchDialog::initFrame)
{
#ifdef __QSINIT__
   startms  = tm_counter();
#endif
   cseconds = FFFF;
   TView *control;
   options |= ofCenterX | ofCenterY;
   is_disk  = index<0;
   timeStr  = new TColoredText(TRect(3, 2, 32, 3), "", 0x71);
   insert(timeStr);
   control  = new TButton(TRect(34, 2, 44, 4), "~C~ancel", cmCancel, bfDefault);
   insert(control);
   selectNext(False);
   processNext();
}

void TDiskBootDialog::processNext() {
   u32t now;
#ifdef __QSINIT__
   now = tm_counter();
#endif
   now = (now-startms)/19;
   if (now!=cseconds) {
      char buf[64];
      snprintf(buf, 64, "Boot %s in %u second%c", is_disk?"disk":"partition",
         4-now, now==3?' ':'s');
      cseconds = now;
      replace_coltxt(&timeStr, buf);
   }
   if (cseconds==4) endModal(cmOK);
}

void TSysApp::BootPartition(u32t disk, long index) {
   if (bootDlg) return;
   bootDlg = new TDiskBootDialog(disk, index);
   // wait delay
   int ok = execView(bootDlg)==cmOK;
   destroy(bootDlg);
   bootDlg = 0;
   // go!
   if (ok) {
      int rc = MSGE_COMMONFAIL;
#ifdef __QSINIT__
      if (index>=0) rc = exit_bootvbr(disk,index,0,0); else {
         rc = exit_bootmbr(disk,0);
         if (rc==ENODEV) {
            if (!askDlg(MSGA_PTEMPTY)) return; else
               rc = exit_bootmbr(disk,EMBR_NOACTIVE);
         }
      }
      if (rc==ENODEV) rc = MSGE_BSEMPTY; else
         rc = rc==EIO?MSGE_MBRREAD:MSGE_COMMONFAIL;
#endif
      errDlg(rc);
   }
}

// *************************************************************************

void TSysApp::CreatePartition(u32t disk, u32t fspidx, int logical) {
#ifdef __QSINIT__
   TView *control;
   dsk_freeblock *info = new dsk_freeblock[fspidx+1];

   if (dsk_ptgetfree(disk, info, fspidx+1)<fspidx+1) {
      errDlg(MSGE_COMMONFAIL);
      return;
   }
   TDialog *dlg = new TDialog(TRect(15, 8, 64, 15), logical?
      "Create logical partition":"Create primary partition");
   if (!dlg) return;
   dlg->options|= ofCenterX | ofCenterY;
   dlg->helpCtx = hcDmgrPtCreate;

   TInputLine *elPtSize = new TInputLine(TRect(19, 3, 31, 4), 11);
   elPtSize->helpCtx = hcDmgrPtSize;
   dlg->insert(elPtSize);
   dlg->insert(new TLabel(TRect(2, 3, 18, 4), "Partition size:", elPtSize));

   TCheckBoxes *cbFlags = new TCheckBoxes(TRect(3, 5, 33, 6),
      new TSItem("at the end of free space", 0));
   dlg->insert(cbFlags);

   control = new TButton(TRect(37, 2, 47, 4), "Cr~e~ate", cmOK, bfDefault);
   dlg->insert(control);
   control = new TButton(TRect(37, 4, 47, 6), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);

   u32t sectsize;
   char sizestr[80];
   // free space size
   hlp_disksize(disk, &sectsize, 0);
   snprintf(sizestr, 80, "Free space    :  %s",
      getSizeStr(sectsize, info[fspidx].Length, True));
   control = new TStaticText(TRect(3, 2, 28, 3), sizestr);
   dlg->insert(control);
   setstr(elPtSize, "100%");
   if (createAtTheEnd) cbFlags->press(0);

   dlg->selectNext(False);
   int ok = execView(dlg)==cmOK;
   createAtTheEnd = cbFlags->mark(0);

   if (ok) {
      u64t   pt_pos, pt_len;
      char   *szstr = getstr(elPtSize);
      u32t   ptsize = str2long(szstr), rc,
             aflags = createAtTheEnd?DPAL_ESPC:0;
      int   gptdisk = dsk_isgpt(disk,-1) > 0;
      destroy(dlg);

      if (!ptsize) { errDlg(MSGE_INVVALUE); return; }
      if (szstr[strlen(szstr)-1]=='%') {
         aflags |= DPAL_PERCENT;
         if (!ptsize || ptsize>100) { errDlg(MSGE_RANGE); return; }
      }
      /* align to CHS on MBR disks & AF on GPT,
         actually dsk_ptalign() ignore AF on MBR and CHS on GPT ;)) */
      aflags |= gptdisk ? DPAL_AF : DPAL_CHSSTART|DPAL_CHSEND;
      // align partition coordinates
      rc = dsk_ptalign(disk, fspidx, ptsize, aflags, &pt_pos, &pt_len);
      // and create it!
      if (!rc)
         if (gptdisk)
            rc = dsk_gptcreate(disk, pt_pos, pt_len, DFBA_PRIMARY, 0);
         else
            rc = dsk_ptcreate(disk, pt_pos, pt_len, logical?DFBA_LOGICAL:
               DFBA_PRIMARY, 0x0C);
      if (rc) PrintPTErr(rc); else infoDlg(MSGI_DONE);
      // notify sector editor window
      DiskChanged(disk);
   } else
      destroy(dlg);
   delete[] info;
#endif // __QSINIT__
}


char* TSysApp::GetPTErr(u32t rccode, int msgtype) {
#ifdef __QSINIT__
   char topic[16];
   sprintf(topic, msgtype==MSGTYPE_LVM?"_LVME%02d":
      (msgtype==MSGTYPE_FMT?"_FMT%02d":"_DPTE%02d"), rccode);
   return cmd_shellgetmsg(topic);
#else
   return 0;
#endif
}

void TSysApp::PrintPTErr(u32t rccode, int msgtype) {
   char msg[384], *mtext;
   msg[0  ] = 3;
   msg[383] = 0;
   mtext = GetPTErr(rccode, msgtype);
   if (mtext) strncpy(msg+1, mtext, 382); else
      sprintf(msg+1, "Error code %d", rccode);

   messageBox(msg, mfError+mfOKButton);
}

void TSysApp::Unmount(u8t vol) {
#ifdef __QSINIT__
   char msg[80];
   snprintf(msg, 80, "\3""Unmount QSINIT drive %c:/%c:?", vol+'0', vol+'A');
   if (messageBox(msg, mfConfirmation+mfYesButton+mfNoButton)==cmYes) {
      if (io_unmount(vol,0)) errDlg(MSGE_COMMONFAIL);
   }
#endif
}

static void FillDiskInfo(TDialog* dlg, u32t disk, u32t index) {
   TView   *control;
   char     str[36];
   u32t  sectorsize = 0;
   u64t      ptsize = 0;
   char volname[24] = { 0 },
        filesys[17] = { 0 };
#ifdef __QSINIT__
   dsk_ptquery64(disk, index, 0, &ptsize, filesys, 0);
   hlp_disksize(disk, &sectorsize, 0);
   lvm_partition_data pi;
   if (lvm_partinfo(disk, index, &pi)) {
      strncpy(volname, pi.VolName, 20);
      volname[20] = 0;
   }
#endif
   snprintf(str, 36, "Disk     : %d", disk);
   control = new TColoredText(TRect(3, 2, 4+strlen(str), 3), str, 0x7F);
   dlg->insert(control);

   snprintf(str, 36, "Partition: %d", index);
   control = new TColoredText(TRect(3, 3, 4+strlen(str), 4), str, 0x7F);
   dlg->insert(control);

   strcpy(str, "FS info  : ");
   strcat(str, filesys);
   control = new TColoredText(TRect(3, 4, 4+strlen(str), 5), str, 0x7F);
   dlg->insert(control);

   strcpy(str, "Size     : ");
   if (ptsize && sectorsize) strcat(str, getSizeStr(sectorsize, ptsize, True));
   control = new TColoredText(TRect(3, 5, 4+strlen(str), 6), str, 0x7F);
   dlg->insert(control);

   if (volname[0]) {
      strcpy(str, "LVM name : ");
      strcat(str, volname);
      control = new TColoredText(TRect(3, 6, 4+strlen(str), 7), str, 0x7F);
      dlg->insert(control);
   }
}

void TSysApp::MountDlg(Boolean qsmode, u32t disk, u32t index, char letter) {
   TView *control;

   TDialog* dlg = new TDialog(TRect(14, 7, 65, 16), qsmode?"Mount QSINIT disk":
      "LVM volume letter");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;
   u32t usedmask = 0;
#ifdef __QSINIT__
   if (qsmode) {
      for (int ii=2; ii<DISK_COUNT; ii++) {
         disk_volume_data vi;
         hlp_volinfo(ii, &vi);
         if (vi.TotalSectors) usedmask |= 1<<ii;
      }
      usedmask |= 3;
      usedmask |= ~((1<<DISK_COUNT)-1);
   } else {
      usedmask  = lvm_usedletters();
      usedmask |= ~((1<<'Z'-'A'+1)-1);
   }
#endif
   TInputLine *elDrive = new TInputLine(TRect(27, 3, 31, 4), 11);
   dlg->helpCtx = qsmode ? hcQSDrive : hcLVMDrive;
   dlg->insert(elDrive);

   TSItem *list = 0, *next;
   for (char ltr=qsmode?'C':'@'; ltr<='Z'; ltr++)
      if (ltr=='@' || (1<<ltr-'A'&usedmask)==0) {
         char tmp[4];
         tmp[0] = ltr=='@'?'-':ltr;
         tmp[1] = ltr=='@'?0:':';
         tmp[2] = 0;
         TSItem *item = new TSItem(tmp,0);
         if (!list) next = list = item; else { next->next = item; next = item; }
      }
   TCombo *cbDrvName = new TCombo(TRect(31, 3, 34, 4), elDrive, cbxOnlyList |
      cbxDisposesList | cbxNoTransfer, list);
   dlg->insert(cbDrvName);
   dlg->insert(new TLabel(TRect(25, 2, 34, 3), "to drive", elDrive));

   control = new TButton(TRect(39, 2, 49, 4), "~O~k", cmOK, bfDefault);
   dlg->insert(control);
   control = new TButton(TRect(39, 4, 49, 6), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);

   FillDiskInfo(dlg, disk, index);

   char str[36];
   if (letter) {
      sprintf(str, "%c:", toupper(letter));
      setstr(elDrive, str);
   }

   dlg->selectNext(False);

   if (execView(dlg)==cmOK) {
      char rcbuf[24];
      elDrive->getData(rcbuf);
      destroy(dlg);
      char ltr = rcbuf[0];
      if ((ltr<'A' || ltr>'Z') && ltr!='-') errDlg(MSGE_INVVALUE); else {
#ifdef __QSINIT__
         if (qsmode) {
            u8t wl = ltr - 'A';
            u32t pterr  = vol_mount(&wl, disk, index);
            if (pterr) PrintPTErr(pterr);
         } else {
            u32t lvmerr = lvm_assignletter(disk, index, ltr=='-'?0:ltr, 0);
            if (lvmerr) PrintPTErr(lvmerr, MSGTYPE_LVM);
         }
#endif
      }
   } else
      destroy(dlg);
}

void TSysApp::FormatDlg(u8t vol, u32t disk, u32t index) {
#ifdef __QSINIT__
   TView *control;
   if (!vol) {
      u32t rc = vol_mount(&vol, disk, index);
      if (rc && rc!=DPTE_MOUNTED) {
         PrintPTErr(rc);
         return;
      }
   }
   disk_volume_data di;
   hlp_volinfo(vol, &di);
   // check - was it really mounted?
   if (!di.TotalSectors) { errDlg(MSGE_MOUNTERROR); return; }

   int  allow_fat = di.TotalSectors <= 32768 * 65526 / di.SectorSize,
                    // min # of sectors for FAT32 (approx, +1024 for garantee)
      allow_fat32 = di.TotalSectors >= 65536 + (65536*4*2)/di.SectorSize + 40 + 1024,
                    // check 64gb limit & sector size
       allow_hpfs = di.SectorSize==512 && di.TotalSectors < _2GB/512*32;

   int  *fs_list[] = { &allow_fat, &allow_fat32, &allow_hpfs, 0};
   char  *fs_str[] = { "FAT", "FAT32", "HPFS", 0 };

   TDialog* dlg = new TDialog(TRect(18, 5, 62, 17), "Format partition");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;

   TInputLine *fmtname = new TInputLine(TRect(3, 9, 15, 10), 11);
   fmtname->helpCtx = hcFmtFsName;
   dlg->insert(fmtname);

   TSItem *list = 0, *next;
   int       ii = 0, fnset = 0;
   // fill available file system list
   while (fs_list[ii]) {
      if (*fs_list[ii]) {
         // set first available
         if (!fnset) { setstr(fmtname, fs_str[ii]); fnset=1; }
         TSItem *item = new TSItem(fs_str[ii],0);
         if (!list) next = list = item; else { next->next = item; next = item; }
      }
      ii++;
   }

   control = new TCombo(TRect(15, 9, 18, 10), fmtname, cbxOnlyList|
      cbxDisposesList|cbxNoTransfer, list);
   dlg->insert(control);
   dlg->insert(new TLabel(TRect(2, 8, 14, 9), "File system", fmtname));

   TRadioButtons *rbFmtType = new TRadioButtons(TRect(24, 2, 40, 5),
      new TSItem("Quick", new TSItem("Long",new TSItem("Wipe disk", 0))));
   rbFmtType->helpCtx = hcFmtType;
   dlg->insert(rbFmtType);
   dlg->insert(new TLabel(TRect(23, 1, 28, 2), "Type", rbFmtType));

   dlg->insert(new TButton(TRect(22, 9, 32, 11), "~O~k", cmOK, bfDefault));
   dlg->insert(new TButton(TRect(32, 9, 42, 11), "~C~ancel", cmCancel, bfNormal));

   FillDiskInfo(dlg, disk, index);

   dlg->selectNext(False);

   int         ok = execView(dlg)==cmOK;
   int   fs_index = 0;
   u32t  unitsize = 0;
   ///
   for (; fs_str[fs_index]; fs_index++)
      if (strcmp(fs_str[fs_index],getstr(fmtname))==0) break;

   int     isHPFS = getstr(fmtname)[0]=='H';

   /* at least one OS/2 installed (we have LVM info on real HDD), so check for
      partition size and force 32k cluster (else vol_format() will made it 64k for
      >=48Gb volumes) */
   if (ok) {
      // check for too large FAT32 cluster size
      if (fs_index==1 && lvm_present(1)) {
         if ((u64t)di.TotalSectors*di.SectorSize >= 48*(u64t)_1GB) {
           int rc = messageBox("\x03""Volume is too large, but 64k cluster not "
                               "supported by OS/2.\n\x03Use 32k instead?",
                               mfConfirmation+mfYesNoCancel);
           if (rc==cmCancel) ok = false; else
           if (rc==cmYes) unitsize = 32768;
         }
      }
      /* check for too small FAT32 partition size and adjust unitsize for it
         (else it will be formatted as FAT16) */
      if (fs_index==1) {
         u32t  csize = _64KB;
         // making too big for FAT16 number of clusters
         while (di.TotalSectors * (u64t)di.SectorSize / csize <= _64KB)
            if (csize > di.SectorSize) csize>>=1; else break;
         if (di.TotalSectors*(u64t)di.SectorSize/csize>_64KB && csize<_64KB)
            unitsize = csize;
      }
   }

   // set codepage if no one is selected now
   if (ok && isHPFS) {
      int cpres = SetCodepage(1);
      if (cpres<0) ok = 0; else
      if (!cpres)
         if (!askDlg(MSGA_FMTCPSERR)) ok = 0;
   }

   if (ok) {
      long   idx = getRadioIdx(rbFmtType, 3);
      u32t flags = DFMT_BREAK;
      if (idx<=0) flags|=DFMT_QUICK; else
      if (idx==2) flags|=DFMT_WIPE;
      destroy(dlg);

      TView *svowner = current;

      PercentDlgOn("Format partition");
      u32t rc = vol_formatfs(vol, fs_index<=1?"FAT":fs_str[fs_index], flags,
                             unitsize, PercentDlgCallback);

      PercentDlgOff(svowner);
      SysApp.DiskChanged(disk);

      if (rc) {
         PrintPTErr(rc, MSGTYPE_FMT);
         return;
      } else {
         disk_volume_data vi;
         static const char *ftstr[4] = {"unrecognized", "FAT12", "FAT16", "FAT32"};
         char             fsname[32];
         u32t fstype = hlp_volinfo(vol, &vi);
         u32t clsize = vi.ClSize * vi.SectorSize;

         dlg = new TDialog(TRect(17, 3, 63, 19), "Format");
         if (!dlg) return;
         dlg->options |= ofCenterX | ofCenterY;

         strncpy(fsname, fstype||!vi.FsName[0] ? ftstr[fstype] : vi.FsName, 32);

         dlg->insert(new TButton(TRect(18, 13, 28, 15), "O~K~", cmOK, bfDefault));
         dlg->insert(new TColoredText(TRect(3, 2, 41, 4), "Format complete.", 0x7A));
         dlg->insert(new TStaticText(TRect(3, 4, 31, 5), "The type of files system is "));
         dlg->insert(new TColoredText(TRect(31, 4, 32+strlen(fsname), 5), fsname, 0x79));

         char buf[96];
         sprintf(buf, "%s total disk space.", getSizeStr(vi.SectorSize, vi.TotalSectors));
         dlg->insert(new TStaticText(TRect(3, 5, 4+strlen(buf), 6), buf));

         sprintf(buf, "%s are available.", getSizeStr(clsize, vi.ClAvail));
         dlg->insert(new TStaticText(TRect(3, 6, 4+strlen(buf), 7), buf));

         if (vi.ClBad) {
            sprintf(buf, "%s in bad sectors.", getSizeStr(clsize, vi.ClBad));
            dlg->insert(new TStaticText(TRect(3, 7, 4+strlen(buf), 8), buf));
         }
         sprintf(buf, "%7d bytes in each allocation unit.", clsize);
         dlg->insert(new TStaticText(TRect(4, 9, 4+strlen(buf), 10), buf));

         sprintf(buf, "%7d total allocation units on disk.", vi.ClTotal);
         dlg->insert(new TStaticText(TRect(4, 10, 4+strlen(buf), 11), buf));

         sprintf(buf, "%7d units available on disk.\n", vi.ClAvail);
         dlg->insert(new TStaticText(TRect(4, 11, 4+strlen(buf), 12), buf));

         dlg->selectNext(False);
         execView(dlg);
      }
   }
   destroy(dlg);
#endif // QSINIT
}


void TSysApp::LVMRename(u32t disk, u32t index) {
   TInputLine *el;
   TView *control;
   char     cname[24];
#ifdef __QSINIT__
   lvm_partition_data lpd;
   if (!lvm_partinfo(disk, index, &lpd)) {
      errDlg(MSGE_LVMQUERY);
      return;
   }
   strncpy(cname+1, lpd.VolName, 20);
   cname[0]  = ' ';
   cname[21] = 0;
#else
   cname[0]  = 0;
   cname[1]  = 0;
#endif
   TDialog* dlg = new TDialog(TRect(7, 9, 72, 14), "Rename LVM partition");
   if (!dlg) return;
   dlg->options |= ofCenterX | ofCenterY;

   el = new TInputLine(TRect(29, 2, 51, 3), 21);
   el->setData(cname+1);
   dlg->insert(el);

   control = new TColoredText(TRect(3, 2, 25, 3), cname, 0x1F);
   dlg->insert(control);

   control = new TColoredText(TRect(26, 2, 28, 3), "->", 0x7C);
   dlg->insert(control);

   control = new TButton(TRect(53, 2, 63, 4), "O~K~", cmOK, bfDefault);
   dlg->insert(control);

   dlg->selectNext(False);
   if (execView(dlg)==cmOK) {
      el->getData(cname);

      if (!strlen(cname)) errDlg(MSGE_INVVALUE); else {
#ifdef __QSINIT__
         u32t rc = lvm_setname(disk, index, LVMN_PARTITION|LVMN_VOLUME, cname);
         if (rc) PrintPTErr(rc, MSGTYPE_LVM);
#endif
      }
   }
   destroy(dlg);
}


int TSysApp::CloneDisk(u32t srcdisk) {
#ifdef __QSINIT__
   TDialog*  dlg = new TDialog(TRect(10, 6, 69, 17), "Clone disk");
   if (!dlg) return 0;
   dlg->options |= ofCenterX | ofCenterY;
   dlg->helpCtx  = hcDskClone;

   u32t     ii, disks = hlp_diskcount(0);
   TCollection *lst_d = new TCollection(0,10);
   u64t      usedsize;
   dsk_usedspace(srcdisk, 0, &usedsize);
   if (usedsize==FFFF64) return 0; else usedsize++;

   for (ii=0; ii<disks; ii++) {
      u32t  scan_rc = dsk_ptrescan(ii,0), sector;
      if (scan_rc!=DPTE_EMPTY) continue;

      disk_geo_data geo;
      if (!hlp_disksize(ii, &sector, &geo)) continue;

      if (sector) {
         char *str = new char[64];
         sprintf(str,"HDD %-4i  ",ii);
         strcat(str,getSizeStr(sector,geo.TotalSectors));
         lst_d->insert(str);
      }
   }

   TListBox *dcdisks = new TListBox(TRect(3, 2, 28, 5), 1, 0);
   dcdisks->helpCtx = hcDskCloneSel;
   dlg->insert(dcdisks);
   dlg->insert(new TLabel(TRect(2, 1, 20, 2), "Target empty disk", dcdisks));
   dcdisks->newList(lst_d);

   TRadioButtons *dctype = new TRadioButtons(TRect(3, 7, 24, 9),
      new TSItem("Structure only", new TSItem("Entire disk", 0)));
   dctype->helpCtx = hcDskCloneStruct;
   dlg->insert(dctype);
   dlg->insert(new TLabel(TRect(2, 6, 13, 7), "Clone type", dctype));

   TCheckBoxes *dcopts = new TCheckBoxes(TRect(31, 2, 56, 7),
      new TSItem("Clone IDs", new TSItem("Copy MBR code",
         new TSItem("Identical", new TSItem("Ignore CHS mismatch",
            new TSItem("Wipe old data", 0))))));
   static u16t dcoptflags[5] = { DCLN_SAMEIDS, DCLN_MBRCODE, DCLN_IDENT,
                                 DCLN_IGNSPT, DCLN_NOWIPE };

   dcopts->helpCtx = hcDskCloneID;
   dlg->insert(dcopts);
   dlg->insert(new TLabel(TRect(31, 1, 39, 2), "Options", dcopts));

   TView *control = new TButton(TRect(28, 8, 37, 10), "~G~o!", cmOK, bfDefault);
   dlg->insert(control);

   control = new TButton(TRect(37, 8, 47, 10), "~C~ancel", cmCancel, bfNormal);
   dlg->insert(control);

   control = new TButton(TRect(47, 8, 57, 10), "~H~elp", cmHelp, bfNormal);
   dlg->insert(control);
   dlg->selectNext(False);

   static u32t saveopts = 1<<4; // "Wipe old data" is on by default
   int           rescan = 0;
   for (ii=0; ii<5; ii++)
      if ((1<<ii & saveopts)) dcopts->press(ii);

   if (execView(dlg)==cmOK) {
      u32t clflags = 0;
      saveopts = 0;
      for (ii=0; ii<5; ii++)
         if (dcopts->mark(ii)) {
            saveopts |= 1<<ii;
            clflags  |= dcoptflags[ii];
         }
      long hdd = getDiskNum(dcdisks);

      ii = dsk_clonestruct(hdd, srcdisk, clflags);
      if (ii) PrintPTErr(ii); else
      if (dctype->mark(1)) {
         long pcnt = dsk_partcnt(srcdisk);
         // copy partitions
         if (pcnt>0)
            for (ii=0; ii<pcnt; ii++) {
               char      nbuf[64];
               TView *svowner = current;

               sprintf(nbuf, "Partition %d of %d", ii+1, pcnt);
               SysApp.PercentDlgOn(nbuf);
               u32t rc = dsk_clonedata(hdd, ii, srcdisk, ii,
                                       SysApp.PercentDlgCallback, 0);
               SysApp.PercentDlgOff(svowner);
               if (rc) {
                  if (rc!=DPTE_UBREAK) PrintPTErr(rc);
                  break;
               }
            }
      }
      rescan = 1;
   }
   destroy(dlg);
   // return flag to rescan for dmgr dialog
   return rescan;
#else
   return 0;
#endif // __QSINIT__
}

int TSysApp::ClonePart(u32t srcdisk, u32t index) {
#ifdef __QSINIT__
   TCloneVolDlg *cld = new TCloneVolDlg(srcdisk, index);
   u32t      dstdisk, dstindex;

   int res = execView(cld)==cmOK;
   if (res) {
      dstdisk  = cld->targetDisk,
      dstindex = cld->targetIndex;
   }
   destroy(cld);
   if (res) {
      char  fsname[10], sdname[8], ddname[8], slen[9], dlen[9], msg[128];
      u64t    dstsize, srcsize;
      u32t  s_sectsz, d_sectsz;
      if (!dsk_ptquery64(dstdisk, dstindex, 0, &dstsize, fsname, 0)) {
         log_printf("dst index is invalid\n");
         errDlg(MSGE_RANGE);
         return 0;
      }
      if (!dsk_ptquery64(srcdisk, index, 0, &srcsize, 0, 0)) {
         log_printf("source index is invalid\n");
         errDlg(MSGE_RANGE);
         return 0;
      }
      dsk_disktostr(srcdisk, sdname);  hlp_disksize(srcdisk, &s_sectsz, 0);
      dsk_disktostr(dstdisk, ddname);  hlp_disksize(dstdisk, &d_sectsz, 0);
      dsk_formatsize(s_sectsz, srcsize, 0, slen);
      dsk_formatsize(d_sectsz, dstsize, 0, dlen);

      if (dstsize<srcsize) { PrintPTErr(DPTE_CSPACE); return 0; }
      if (s_sectsz!=d_sectsz) { errDlg(MSGE_SECSIZEMATCH); return 0; }

      snprintf(msg, 128, "\3""Do you want to copy disk %s partition %d "
         "(%s) to disk %s partition %d (%s)?\n", sdname, index, slen, ddname,
            dstindex, dlen);
      if (messageBox(msg, mfConfirmation+mfYesButton+mfNoButton)==cmYes) {
         int go = 1;
         if (fsname[0]) {
            snprintf(msg, 128, "\3""Current file system on target is %s. It will be lost!\n",
              fsname);
            go = messageBox(msg, mfConfirmation+mfYesButton+mfNoButton)==cmYes;
         }
         if (go) {
            TView *svowner = current;
            PercentDlgOn("Cloning partition");
            u32t rc = dsk_clonedata(dstdisk, dstindex, srcdisk, index,
                                    PercentDlgCallback, 0);
            PercentDlgOff(svowner);
            if (rc) PrintPTErr(rc);
         }
      }
   }
   return res;
#else
   return 0;
#endif // __QSINIT__
}


int TSysApp::SaveRestVHDD(u32t disk, int rest) {
#ifdef __QSINIT__
   char imgname[QS_MAXPATH+1];
   TFileDialog *fo = new TFileDialog("*.*", rest?"Open backup image":"Create backup image",
                     "~F~ile name", rest?fdOpenButton:fdOKButton, hhVhddImage);
   fo->helpCtx = hcFileDlgCommon;
   int res = execView(fo)==cmFileOpen;
   if (res) fo->getData(imgname);
   destroy(fo);

   if (res) {
      qs_emudisk ed = NEW(qs_emudisk);
      if (!ed) { errDlg(MSGE_NOVHDD); return 0; }

      disk_geo_data  rdi;
      memset(&rdi, 0, sizeof(disk_geo_data));
      hlp_disksize(disk, 0, &rdi);

      if (!rdi.TotalSectors) res = MSGE_INVDISK; else {
         if (!rest && _access(imgname,F_OK)==0) {
            char *msg = sprintf_dyn("\x03""Overwrite file \"%s\"?\n", imgname);
            res = messageBox(msg, mfConfirmation+mfYesButton+mfNoButton);
            free(msg);
            res = res==cmYes ?0:-1;
            // delete previous file
            if (!res) unlink(imgname);
         } else
            res = 0;

         if (!res) {
            // open file and check result
            res = rest ? ed->open(imgname) :
                         ed->make(imgname, rdi.SectorSize, rdi.TotalSectors);
            if (res)
               switch (res) {
                  case EIO   : res = rest?MSGE_READERR:MSGE_WRITEERR; break;
                  case EBUSY :
                  case EACCES: res = MSGE_ACCESSDENIED; break;
                  case EEXIST: res = MSGE_FILECREATERR; break;
                  case EBADF : res = MSGE_NOTVHDDFILE; break;
                  default    : res = MSGE_FILEOPENERR;
               }
         }
      }
      // query file info and compare sector/disk size
      if (!res && rest) {
         disk_geo_data  vdi;
         memset(&vdi, 0, sizeof(disk_geo_data));

         ed->query(&vdi, 0, 0, 0);
         if (vdi.SectorSize != rdi.SectorSize) res = MSGE_SECSIZEMATCH; else
            if (vdi.TotalSectors != rdi.TotalSectors) res = MSGE_NSECMATCH;
               else res = 0;
      }
      // exit on errors above
      if (res) {
         DELETE(ed);
         if (res>0) errDlg(res);
         return 0;
      }

      char ddname[8];
      dsk_disktostr(disk, ddname);
      char *msg = rest ? sprintf_dyn("\x03""Write image data from file \"%s\" to disk %s?\n",
                                     imgname, ddname) :
                         sprintf_dyn("\x03""Backup MBR data from disk %s to \"%s\"?\n",
                                     ddname, imgname);
      res = messageBox(msg, mfConfirmation+mfYesButton+mfNoButton);
      free(msg);

      if (res==cmYes) {
         s32t vdisk = ed->mount();
         if (vdisk<0) { errDlg(MSGE_VMOUNTERR); res=0; } else {
            // clone it at last!
            res = dsk_clonestruct(rest?disk:vdisk, rest?vdisk:disk,
               DCLN_MBRCODE|DCLN_SAMEIDS|DCLN_IDENT|DCLN_NOWIPE|DCLN_IGNSPT);
            ed->umount();
            ed->close();
            if (res) PrintPTErr(res);
            res = res?0:1;
         }
      } else {
         res = 0;
         ed->close();
         // remove unfinished file
         if (!rest) unlink(imgname);
      }
      DELETE(ed);
      return res;
   }
#endif // __QSINIT__
   return 0;
}

