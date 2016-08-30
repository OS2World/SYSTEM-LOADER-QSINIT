/*--------------------------------------------------------------------*/
/*                                                                    */
/*   QSINIT: big TV app                                               */
/*   Note  : this code looks like a garbage and was created only for  */
/*           speed up QSINIT`s testing... in addition this is still a */
/*           GUI (some kind of it ;))                                 */
/*--------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

#define Uses_TEvent
#define Uses_TProgram
#define Uses_MsgBox
#define Uses_TApplication
#define Uses_TKeys
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TView
#define Uses_TWindow
#define Uses_THistory
#define Uses_TScrollBar
#define Uses_TListBox
#define Uses_TDialog
#define Uses_TButton
#define Uses_TSItem
#define Uses_TScreen
#define Uses_TCheckBoxes
#define Uses_TRadioButtons
#define Uses_TCollection
#define Uses_TLabel
#define Uses_TInputLine
#define Uses_TColoredText
#define Uses_TTerminal
#define Uses_TFileEditor
#define Uses_TMenu
#define Uses_TEditWindow
#include <tv.h>
#include <tvhelp.h>
#include "diskedit.h"
#include "tcolortx.h"
#include "calendar.h"
#include "winlist.h"
#include "ptabdlg.h"
#include "bpbdlg.h"
#include "diskact.h"
#include "lvmdlat.h"
#include "helpwin.h"
#include "calc.h"

#include "edbinary.cpp"
#ifdef __QSINIT__
#include "parttab.h"
#else
#define IS_EXTENDED(pt) ((pt)==0x05 || (pt)==0x0F ||    \
   (pt)==0x15 || (pt)==0x1F || (pt)==0x85 || (pt)==0x91 || (pt)==0x9B)
#endif

#define cmEditCopy     110
#define cmEditPaste    111
#define cmViewS0       112
#define cmLoad         113
#define cmMBRCode      114
#define cmBootSetup    115
#define cmLdrBootSetup 116
#define cmRunCmdLine   117
#define cmViewLog      118
#define cmPowerOff     119
#define cmBootMBR      120
#define cmCpuInfo      121
#define cmNew          122
#define cmShowClipbrd  123
#define cmSaveLog      124
#define cmWindowList   125
#define cmCloseAll     126
#define cmScrnRefresh  127
#define cmCalendar     128
#define cmBmgrMenu     129
#define cmSectEd       130
#define cmSectFind     131
#define cmDiskInit     132
#define cmGoto         133
#define cmSectFindNext 134
#define cmSectPtEd     135
#define cmPureLog      136
#define cmSectBpbEd    137
#define cmSectToFile   138
#define cmSectToSect   139
#define cmFileToSect   140
#define cmDmgr         141
#define cmSectDlatEd   142
#define cmCalculator   143
#define cmMemEdit      144
#define cmSetCodepage  145

#define cmHelpIndex    200
#define cmHelpCont     201
#define cmAbout        202

int         Modified=false;

class TMyStatusLine : public TStatusLine {
public:
  TMyStatusLine( const TRect& bounds, TStatusDef& aDefs ) :
    TStatusLine(bounds,aDefs) {}
  virtual const char* hint( ushort aHelpCtx );
  virtual void handleEvent(TEvent &event);
};

#include "helpstr.h"

#ifdef __QSINIT__
static const char *helpFileName = "B:\\MSG\\SYSVIEW.HLP";
#else
static const char *helpFileName = "MSG\\SYSVIEW.HLP";
#endif

TSysApp SysApp;

TAppWindow::TAppWindow (const TRect &bounds, const char *aTitle, short aNumber) :
   TWindow(bounds, aTitle, aNumber+1), TWindowInit(initFrame)
{
   if (aNumber<TWin_Count) SysApp.windows[aNumber] = this;
   if (aNumber==TWin_SectEdit) SysApp.SetCommandState(CS_D,True);
   if (aNumber==TWin_MemEdit) SysApp.SetCommandState(CS_M,True);
}

void TAppWindow::removeAction() {
   SysApp.windows[number-1] = 0;
   if (number-1==TWin_SectEdit) SysApp.SetCommandState(CS_D,False);
   if (number-1==TWin_MemEdit) SysApp.SetCommandState(CS_M,False);
}

void TAppWindow::close() {
   TWindow::close();
   // is window closed actually?
   if (!frame) removeAction();
}

TSysApp::TSysApp() :
    TProgInit( &TSysApp::initStatusLine,
               &TSysApp::initMenuBar,
               &TSysApp::initDeskTop
             )
{
   TCommandSet cmd;
   cmd+=cmQuit;
   cmd+=cmHelp;
   cmd+=cmMenu;
   cmd+=cmOK;
   cmd+=cmCancel;
   enableCommands(cmd);
   ToggleEditorCommands(False);

   clipWindow = OpenEditor(1,1);
   if (clipWindow) {
      TEditor::clipboard = clipWindow->editor;
      TEditor::clipboard->canUndo = False;
   }

   kernel      = 0;
   kernel_parm = 0;
   bootcmd     = 0;
   icaseSrch   = 0;
   searchDlg   = 0;
   copyDlg     = 0;
   bootDlg     = 0;
   lastSearchStop = TDiskSearchDialog::stopVoid;
   memset(windows, 0, sizeof(windows));

   searchData.data    = 0;
   searchData.size    = 0;
   lastBrowseExtPos   = 0;
   lastBrowseExtSlice = 0;
   createAtTheEnd     = 0;
}

TSysApp::~TSysApp() {
   if (searchData.data) {
      free(searchData.data);
      searchData.data = 0;
      searchData.size = 0;
   }
}

void TMyStatusLine::handleEvent(TEvent &event) {
   if (event.what==evKeyDown && event.keyDown.keyCode==kbEsc &&
      SysApp.bootcmd==cmViewLog)
   {
      event.what = evCommand;
      event.message.command = cmQuit;
      event.message.infoPtr = 0;
   }
   TStatusLine::handleEvent(event);
}

char *getSizeStr(u32t sectsize, u64t disksize, Boolean trim) {
   static char buffer[64];
   static char *suffix[] = { "kb", "mb", "gb", "tb" };

   u64t size = disksize * (sectsize?sectsize:1);
   int   idx = 0;
   size    >>= 10;
   while (size>=100000) { size>>=10; idx++; }
   sprintf(buffer, trim?"%d %s":"%8d %s", (u32t)size, suffix[idx]);
   return buffer;
}

static ushort _cmdState[][2] = {{cmGoto,CS_A+1}, {cmSectFind,CS_A+1},
   {cmSectFindNext,CS_A+1}, {cmSectPtEd,CS_D+1}, {cmSectBpbEd,CS_D+1},
   {cmSectDlatEd,CS_D+1}, {cmSectToFile,CS_D+1}, {cmSectToSect,CS_D+1},
   {cmFileToSect,CS_D+1}, {0,0}};

void TSysApp::SetCommandState(u16t mask, int onoff) {
   int    ii = 0;
   ushort st;
   while ((st=_cmdState[ii][1])) {
      if (st&mask) {
         if (onoff) {
            if ((st&CS_MASK)==0) enableCommand(_cmdState[ii][0]);
            _cmdState[ii][1]++;
         } else
         if ((st&CS_MASK))
            if ((--_cmdState[ii][1]&CS_MASK)==0) disableCommand(_cmdState[ii][0]);
      }
      ii++;
   }
}

int TSysApp::askDlg(int MsgType, u32t arg) {
   static char *askMsgArray[]={"\3""Replace MBR code on disk %u?\n",     // 0
                               "\3""This is a huge disk (>500Gb)!\n"     // 1
                               "\3""Make it OS/2 LVM compatible?\n",
                               "\3""Write OS/2 LVM signatures to disk?", // 2
                               "\3""The system will now be powered off.\n"
                               "\3""Do you want to continue?",           // 3
                               "\3""Make this partition active?\n",      // 4
                               "\3""This disk will always be used as secondary?\n", // 5
                               "\3""This huge disk (>500Gb) is incompatible with LVM.\n"
                               "\3""Continue?\n",                        // 6
                               "\3""Perform quick format?\n",            // 7
                               "\3""Access to this area requires PAE paging mode."
                                   "Turn it on?\n",                      // 8
                               "\3""There is no active partition in partition table!\n"
                               "\3""Continue?\n",                        // 9
                               "\3""Set \"BIOS Bootable\" attribute for this partition?\n", // 10
                               "\3""Codepage selection failed. Continue format?\n" // 11
                              };
   char *mptr = askMsgArray[MsgType];

   if (strchr(mptr, '%')) {
      static char str[128];
      snprintf(str, 128, mptr, arg);
      mptr = str;
   }
   return messageBox(mptr, mfConfirmation+mfYesButton+mfNoButton)==cmYes?1:0;
}

void TSysApp::infoDlg(int MsgType) {
   static char *infoMsgArray[]={"\3""Done!",
                                "\3""String was not found.",
                                "\3""Length truncated to file size!",
                                "\3""Length truncated to disk size!",
                                "\3""Length truncated to target disk size!",
                                "\3""Source and destination is the same!",
                                "\3""LVM information is valid and not outdated!",
                                "\3""Bootstrap code updated.",
                                "\3""QSINIT files transfered."
                               };
   messageBox(infoMsgArray[MsgType], mfInformation+mfOKButton);
}

void TSysApp::errDlg(int MsgType) {
   static char *errMsgArray[]={"\3""This is a floppy disk!",
                               "\3""Not supported by system!",
                               "\3""This is boot partition, not a physical disk!",
                               "\3""Failed to write MBR!",
                               "\3""Disk is not empty!",
                               "\3""Unable to read partition table!",
                               "\3""Invalid value specified!",
                               "\3""Value is out of range!",
                               "\3""There is no APM on this PC!",
                               "\3""Search process was stopped because of read error!",
                               "\3""Too long search string!",
                               "\3""Unable to open help file!",
                               "\3""This is not a file!",
                               "\3""Insufficient disk space to write a file!",
                               "\3""File size too large for target filesystem!",
                               "\3""Source and destination interference each other!",
                               "\3""Disk read error!",
                               "\3""Disk write error!",
                               "\3""Select action from one of the action lists and press Enter!",
                               "\3""Boot sector is empty!",
                               "\3""Action failed!",
                               "\3""Number of sectors must be in range 1..4294967295!",
                               "\3""Not a GPT disk!",
                               "\3""Invalid GUID string format!",
                               "\3""LVM info query error!",
                               "\3""No memory to perform this action!",
                               "\3""Unable to open selected file!",
                               "\3""Specified file is empty!",
                               "\3""Volume mount error!",
                               "\3""Sector size is different on disks!",
                               "\3""There is no VHDD module services!",
                               "\3""Share error - file in use!",
                               "\3""File is not a VHDD image!",
                               "\3""Number of sectors is not matched!",
                               "\3""Unable to mount disk image temporary!",
                               "\3""Selected disk is invalid!",
                               "\3""Unable to create file!",
                               "\3""Unable to locate QSINIT boot files!",
                               "\3""QSINIT unable to write to this filesystem type",
                               "\3""Boot code for this filesystem unable to handle partition above 2Tb border",
                               "\3""Boot code for this filesystem unable to handle partition which crosses 2Tb border",
                               "\3""Boot file name on FAT is limited to 11 characters",
                               "\3""Unable to write QSINIT binaries!"
                               };
   messageBox(errMsgArray[MsgType], mfError+mfOKButton);
}

void TSysApp::getEvent(TEvent& event) {
   TApplication::getEvent(event);
   if (event.what==evCommand) {
      static int in_help = 0;
      // catch help here else cmHelp will close menus and change "current"
      if (event.message.command==cmHelp && !in_help) {
         in_help = 1;
         OpenHelp(getHelpCtx(), 1);
         clearEvent(event);
         in_help = 0;
      }
   }
}

void TSysApp::handleEvent(TEvent& event) {
  TApplication::handleEvent(event);
  if (event.what==evCommand) {
    switch (event.message.command)  {
      case cmSectFindNext:
         if (lastSearchStop == TDiskSearchDialog::stopFound ||
            lastSearchStop == TDiskSearchDialog::stopReadErr)
         {
            TSysWindows who = lastSearchMode?TWin_SectEdit:TWin_MemEdit;
            if (windows[who]) {
               windows[who]->select();
               SearchBinNext(lastSearchMode);
               break;
            }
         } // go to common find dialog if no search or it was ended
      case cmSectFind: {
         int is_mem = IsMemAction();
         if (is_mem>=0) {
            windows[is_mem?TWin_MemEdit:TWin_SectEdit]->select();
            SearchBinStart(is_mem?False:True);
         }
         break;
      }
      case cmHelpCont:
         OpenHelp(hcHelpCont);
         break;
      case cmSectEd: {
            u32t dsk = GetDiskDialog();
            if (dsk!=FFFF) OpenPosDiskWindow(dsk,0);
         }
         break;
      case cmSectBpbEd :
      case cmSectDlatEd: {
            TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
            win->select();
            u64t  sector;
            win->sectEd->cursorLine(&sector);
            uchar *sdata = win->sectEd->changedArray(sector);

            if (event.message.command==cmSectDlatEd) {
               TDLATDialog *sdle = new TDLATDialog(sdata, win->sectEd->clusterSize());
               if (execView(sdle)==cmOK) win->sectEd->drawView();
               destroy(sdle);
            } else
            if (event.message.command==cmSectBpbEd) {
               // create & exec dialog
               TBPBSectorDialog *sbte = new TBPBSectorDialog(sdata);
               if (execView(sbte)==cmOK) win->sectEd->drawView();
               destroy(sbte);
            }
         }
         break;
      case cmSectPtEd: {
            TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
            win->select();
            u64t  sector;
            win->sectEd->cursorLine(&sector);
            uchar *sdata = win->sectEd->changedArray(sector);
            // get LVM sectors per track for warning message
            dsk_geo_data geo;
            dsk_size(win->disk, 0, &geo);
            // create & exec dialog
            TPTSectorDialog *spte = new TPTSectorDialog(sdata, geo.SectOnTrack);
            execView(spte);

            if (spte->dlgrc==TPTSectorDialog::actGoto) {
               u32t act_goto = spte->gosector;
               if (sector)
                  act_goto += IS_EXTENDED(spte->goptbyte)? lastBrowseExtPos :
                     lastBrowseExtSlice;
               /* trying to handle multiple extended partitions (one of it
                  can be "hidden") - by remembering start of partition to
                  which we`re press 1..4 in MBR */
               if (IS_EXTENDED(spte->goptbyte)) {
                  if (sector==0) lastBrowseExtPos = spte->gosector;
                  lastBrowseExtSlice = act_goto;
               } else
               if (sector==0) {
                  lastBrowseExtPos   = 0;
                  lastBrowseExtSlice = 0;
               }
               win->sectEd->processKey(TLongEditor::posSet, act_goto);
            } else
            if (spte->dlgrc==TPTSectorDialog::actOk)
               win->sectEd->drawView();
            destroy(spte);
         }
         break;
      case cmDiskInit: {
            u32t dsk = GetDiskDialog(0,0);
            if (dsk!=FFFF) DiskInit(dsk);
         }
         break;
      case cmGoto: {
         int act = IsMemAction();
         if (act>=0) {
            TDskEdWindow *win = (TDskEdWindow*)windows[act?TWin_MemEdit:TWin_SectEdit];
            if (win) win->select();

            if (act>0) GotoMemDlg(); else
            if (win) {
               u64t pos = SelectSector(SCSEL_GOTO, win->disk);
               if (pos!=FFFF) win->sectEd->processKey(TLongEditor::posSet, pos);
            }
         }
         break;
      }
      case cmMemEdit:
         OpenPosMemWindow(cmMemEditPos);
         cmMemEditPos = 0;
         break;
      case cmFileToSect:
      case cmSectToSect:
      case cmSectToFile: {
         TDskEdWindow *win = (TDskEdWindow*)windows[TWin_SectEdit];
         if (win) {
            u64t  sector;
            u32t     cmd = event.message.command;
            win->select();
            win->sectEd->cursorLine(&sector);
            SelectSector(cmd==cmSectToFile?SCSEL_SAVE:(cmd==cmSectToSect?SCSEL_COPY:
               SCSEL_READ),win->disk, -1, 0, 0, sector);
         }
         break;
      }
      case cmDmgr: {
         TDMgrDialog *dmgr=new TDMgrDialog();
         execView(dmgr);

         if (dmgr->goToDisk!=FFFF && SysApp.bootcmd!=cmDmgr)
            OpenPosDiskWindow(dmgr->goToDisk, dmgr->goToSector);

         destroy(dmgr);
         if (SysApp.bootcmd==cmDmgr) message(application,evCommand,cmQuit,NULL);
         break;
      }
      case cmAbout: {
         TDialog *about=makeAboutDlg();
         execView(about);
         destroy(about);
         break;
      }
      case cmNew: OpenEditor(True); break;
      case cmLoad: OpenEditor(False); break;
      case cmShowClipbrd:
         if (clipWindow) {
            clipWindow->select();
            clipWindow->show();
         }
         break;
      case cmSetCodepage:
         if (!SetCodepage(0)) errDlg(MSGE_COMMONFAIL);
         break;
      case cmPowerOff:
         PowerOFF();
         break;
      case cmBootMBR: {
         u32t dsk = GetDiskDialog(0,0,SELDISK_BOOT);
         if (dsk!=FFFF) {
            int rc;
#ifdef __QSINIT__
            u32t flags = nextBootOwnMbr ? EMBR_OWNCODE : 0;
            rc = exit_bootmbr(dsk,flags);
            if (rc==ENODEV)
               if (!askDlg(MSGA_PTEMPTY)) break;
                  else rc = exit_bootmbr(dsk,flags|EMBR_NOACTIVE);
#endif
            errDlg(rc==EIO?MSGE_MBRREAD:MSGE_COMMONFAIL);
         }
         break;
      }
      case cmMBRCode: {
         u32t dsk = GetDiskDialog(0,0);
         if (dsk!=FFFF) DiskMBRCode(dsk);
         break;
      }
      case cmBootSetup: {
         TKernBootDlg *dlg = new TKernBootDlg();
         if (SetupBootDlg(dlg,kernel,kernel_parm)) {
            execView(dlg);
            destroy(dlg);
            if (kernel) message(application,evCommand,cmQuit,NULL);
            break;
         } else {
            destroy(dlg);    // no break here
            kernel_parm = 0; // incorrect RESTART in parameters
         }
      }
      case cmLdrBootSetup: {
         TLdrBootDialog *dlg = new TLdrBootDialog();
         SetupLdrBootDlg(dlg,kernel);
         int   res = execView(dlg);
         destroy(dlg);
         if (res==cmSelectKernelDialog)
            message(application,evCommand,cmBootSetup,NULL);
         else
         if (kernel) message(application,evCommand,cmQuit,NULL);
         break;
      }
      case cmBmgrMenu:
         BootmgrMenu();
         if (SysApp.bootcmd==cmBmgrMenu) message(application,evCommand,cmQuit,NULL);
         break;
      case cmRunCmdLine: {
         TRunCommandDialog *rcmdd=new TRunCommandDialog();
         execView(rcmdd);
         destroy(rcmdd);
         // update disk view
         DiskChanged();
         break;
      }
      case cmWindowList: {
         TWinListDialog *wld=new TWinListDialog();
         execView(wld);
         destroy(wld);
         break;
      }
      case cmScrnRefresh: TProgram::application->redraw(); break;

      case cmCalendar   : {
         TCalendarWindow *cal=(TCalendarWindow *)validView(new TCalendarWindow);
         if (cal) {
            cal->helpCtx = hcCalendar;
            deskTop->insert(cal);
         }
         break;
      }
      case cmCalculator : {
         TCalculatorWindow *win = (TCalculatorWindow*)windows[TWin_Calc];

         if (win) win->select(); else {
            win = (TCalculatorWindow *)validView(new TCalculatorWindow);
            if (win) deskTop->insert(win);
         }
         break;
      }
      case cmTile    : deskTop->tile(deskTop->getExtent()); break;
      case cmCascade : deskTop->cascade(deskTop->getExtent()); break;
      case cmCpuInfo : ExecCpuInfoDlg(); break;
      case cmCloseAll: CloseAll(); break;
      case cmSaveLog : SaveLog(True); break;
      case cmPureLog : SaveLog(False); break;
      case cmViewLog : SwitchLogWindow(); break;
      default:
        return;
    }
    clearEvent( event );            // clear event after handling
  }
}

const char* TMyStatusLine::hint(ushort aHelpCtx) {
  if (aHelpCtx>=2000&&aHelpCtx<HELP_LASTHLPIDX) return Hints[aHelpCtx-2000];
     else return EmptyStr;
}

TMenuBar *CreateMenuBar( TRect r ) {
  r.b.y = r.a.y + 1;    // set bottom line 1 line below top line
  return new TMenuBar(r,
   *new TSubMenu("~Q~S",hcNoContext)+
      *new TMenuItem("~R~un command",cmRunCmdLine,kbF4,hcRunExecute,"F4")+
      *new TMenuItem("System ~l~og",kbNoKey,
          new TMenu(
         *new TMenuItem("~V~iew",cmViewLog,kbNoKey,hcViewLog,"",
          new TMenuItem("~S~ave",cmSaveLog,kbNoKey,hcSaveLog,"",
          new TMenuItem("Save ~P~ure",cmPureLog,kbNoKey,hcPureLog,""
         )))))+
      *new TMenuItem("~S~elect codepage",cmSetCodepage,kbNoKey,hcSetCodepage,"")+
      newLine()+
      *new TMenuItem("~C~PU info",cmCpuInfo,kbNoKey,hcCpuInfo,"")+
      newLine()+
      *new TMenuItem( "E~x~it", cmQuit, kbCtrlX, hcExit, "Ctrl-X" )+
      *new TMenuItem("~P~ower off",cmPowerOff,kbNoKey,hcPowerOff,"")+
   *new TSubMenu("~S~ystem",hcNoContext)+
      *new TMenuItem("~D~isk management",cmDmgr,kbCtrlD,hcDmgrDlg,"Ctrl-D")+
      newLine()+
      *new TMenuItem("~P~hysical sectors",cmSectEd,kbNoKey,hcSectEdit,"")+
      *new TMenuItem("~V~iew sector as",kbNoKey,
          new TMenu(
         *new TMenuItem("Partition table",cmSectPtEd,kbCtrlF2,hcViewAsPT,"Ctrl-F2",
          new TMenuItem("Boot sector",cmSectBpbEd,kbCtrlF3,hcViewAsBPB,"Ctrl-F3",
          new TMenuItem("LVM DLAT sector",cmSectDlatEd,kbCtrlF4,hcDlatDlg,"Ctrl-F4"
         )))))+
      *new TMenuItem("~C~opy sector to",kbNoKey,
          new TMenu(
         *new TMenuItem("File",cmSectToFile,kbNoKey,hcSectExport,"",
          new TMenuItem("Sector",cmSectToSect,kbNoKey,hcSectCopy,""
         ))))+
      *new TMenuItem("~C~opy sector from file",cmFileToSect,kbNoKey,hcSectImport,"")+
      newLine()+
      *new TMenuItem("Physical ~m~emory",cmMemEdit,kbNoKey,hcMemEdit,"")+
      newLine()+
      *new TMenuItem("~G~o to",cmGoto,kbF11,hcGoto,"F11")+
      *new TMenuItem("~F~ind binary data",cmSectFind,kbCtrlF,hcSectFind,"Ctrl-F")+
      *new TMenuItem("~F~ind next",cmSectFindNext,kbCtrlA,hcSectFindNext,"Ctrl-A")+
   *new TSubMenu("~B~oot",hcNoContext)+
      *new TMenuItem("OS/2 ~k~ernel",cmBootSetup,kbNoKey,hcNoContext,"")+
      *new TMenuItem("OS/2 ~l~oader (os2ldr)",cmLdrBootSetup,kbNoKey,hcLdrName,"")+
      *new TMenuItem("~M~aster boot record (MBR)",cmBootMBR,kbNoKey,hcBootMBR,"")+
      *new TMenuItem("~B~oot Manager menu",cmBmgrMenu,kbNoKey,hcBootMGR,"")+
   *new TSubMenu("~E~dit",hcNoContext)+
     *new TMenuItem("~N~ew",cmNew,kbNoKey,hcFileNew,"",
      new TMenuItem("~L~oad",cmLoad,kbF3,hcFileLoad,"F3",
      new TMenuItem("~S~ave",cmSave,kbF2,hcFileSave,"F2",
      new TMenuItem("Save ~a~s",cmSaveAs,kbShiftF2,hcFileSaveAs,"Shift-F2",
      new TMenuItem(0, 0, 0, hcNoContext, 0,
      new TMenuItem("~F~ind",cmFind,kbF7,hcFind,"F7",
      new TMenuItem("~R~eplace",cmReplace,kbF8,hcReplace,"F8",
      new TMenuItem("Search again",cmSearchAgain,kbShiftF7,hcFindAgain,"Shift-F7",
      new TMenuItem(0, 0, 0, hcNoContext, 0,
      new TMenuItem("~U~ndo",cmUndo,kbAltBack,hcUndo,"Alt-Bkspc",
      new TMenuItem("Cu~t~",cmCut,kbShiftDel,hcCut,"Shift-Del",
      new TMenuItem("~C~opy",cmCopy,kbCtrlIns,hcCopy,"Ctrl-Ins",
      new TMenuItem("~P~aste",cmPaste,kbShiftIns,hcPaste,"Shift-Ins",
      new TMenuItem("C~l~ear",cmClear,kbCtrlDel,hcClear,"Ctrl-Del"
      ))))))))))))))+
   *new TSubMenu("~W~indow",hcNoContext)+
      *new TMenuItem("~T~ile",cmTile,kbNoKey,hcTile,0)+
      *new TMenuItem("C~a~scade",cmCascade,kbNoKey,hcCascade,0)+
      *new TMenuItem("Cl~o~se all",cmCloseAll,kbNoKey,hcCloseAll,0)+
      *new TMenuItem("~R~efresh display",cmScrnRefresh,kbNoKey,hcScrnRefresh,0)+
      newLine()+
      *new TMenuItem("Show clipboard",cmShowClipbrd,kbNoKey,hcShowClipbrd,"")+
      newLine()+
      *new TMenuItem("~S~ize/Move",cmResize,kbCtrlF5,hcResize,"Ctrl-F5")+
      *new TMenuItem("~Z~oom",cmZoom,kbF6,hcZoom,"F5")+
      *new TMenuItem("~N~ext",cmNext,kbF6,hcNext,"F6")+
      *new TMenuItem("~P~revious",cmPrev,kbShiftF6,hcPrev,"Shift-F6")+
      *new TMenuItem("~C~lose",cmClose,kbAltF3,hcClose,"Alt-F3")+
      newLine()+
      *new TMenuItem("List...",cmWindowList,kbAlt0,hcWindowList,"Alt-0")+
   *new TSubMenu("~H~elp",hcNoContext)+
      //*new TMenuItem( "~I~ndex",cmHelpIndex,kbShiftF1,hcNoContext,"Shift-F1")+
      *new TMenuItem( "~C~ontents",cmHelpCont,kbNoKey,hcNoContext,"")+
      newLine()+
      *new TMenuItem("Calc",cmCalculator,kbF9,hcCalculator,"F9")+
      *new TMenuItem("Ca~l~endar",cmCalendar,kbNoKey,hcCalendar,0)+
      newLine()+
      *new TMenuItem( "~A~bout",cmAbout,kbNoKey, hcNoContext,"")
  );
}

TMenuBar *TSysApp::initMenuBar( TRect r ) {
  r.b.y = r.a.y + 1;    // set bottom line 1 line below top line
  return new TMenuBar(r, 0);
}

void TSysApp::idle() {
   static int NeedStart = 1;
   TApplication::idle();
   if (NeedStart) {
      NeedStart = 0;
      SetCommandState(CS_D,False);

      if (!bootcmd || bootcmd==cmMemEdit) {
         removeView(menuBar);
         menuBar->owner = 0;
         destroy(menuBar);
         menuBar = CreateMenuBar(getExtent());
         insert(menuBar);
      }
      if (bootcmd) message(application,evCommand,bootcmd,NULL);
   }
   ToggleEditorCommands(!(deskTop->current==0||IsSysWindows(deskTop->current)));
   // search/copy on disk is active ...
   if (searchDlg) searchDlg->processNext(); else
   if (copyDlg) copyDlg->processNext(); else
   if (bootDlg) bootDlg->processNext();
}

int TSysApp::IsSysWindows(TView *obj) {
   if (!obj) return False;
   for (int ii=0;ii<TWin_Count;ii++)
      if (windows[ii]==obj) return True;
   return False;
}

int TSysApp::CountSysWindows() {
   int ii, count=0;
   for (ii=0;ii<TWin_Count;ii++)
      if (windows[ii]) count++;
   return count;
}

TStatusLine *TSysApp::initStatusLine(TRect r) {
   r.a.y = r.b.y - 1;     // move top to 1 line above bottom
   return new TMyStatusLine(r,
      *new TStatusDef(0,0xFFFF) +
      *new TStatusItem( 0, kbF10, cmMenu) +
      *new TStatusItem("~F1~ Help", kbF1, cmHelp) +
      *new TStatusItem( 0 /*"~Alt-F3~ Close"*/, kbAltF3, cmClose) +
      *new TStatusItem( 0, kbF5, cmZoom) +
      *new TStatusItem( 0, kbCtrlF5, cmResize) +
      *new TStatusItem( 0, kbF6, cmNext) +
      *new TStatusItem( 0, kbAltX, cmQuit)
   );
}

/* trying to select (or ask) memory or disk editor window for Search/Goto
   dialogs */
int TSysApp::IsMemAction() {
   // no one
   if (!windows[TWin_MemEdit] && !windows[TWin_SectEdit]) return -1;

   if (windows[TWin_MemEdit])
      if (windows[TWin_MemEdit]->getState(sfActive|sfSelected)!=0) return 1;
   if (windows[TWin_SectEdit])
      if (windows[TWin_SectEdit]->getState(sfActive|sfSelected)!=0) return 0;
   if (windows[TWin_MemEdit] && windows[TWin_SectEdit]) {
      TRadioButtons *rb;
      TDialog      *dlg = new TDialog(TRect(24, 9, 55, 13), "What window?");
      if (!dlg) return 0;
      dlg->options  |= ofCenterX | ofCenterY;
      rb = new TRadioButtons(TRect(2, 1, 16, 3), new TSItem("Memory", new TSItem("Disk", 0)));
      dlg->insert(rb);
      dlg->insert(new TButton(TRect(19, 1, 29, 3), "O~K~", cmOK, bfDefault));
      dlg->selectNext(False);
      execView(dlg);
      int rc = rb->mark(0) ? 1 : 0;
      destroy(dlg);
      return rc;
   }
   return windows[TWin_MemEdit] ? 1 : 0;
}

static uchar loglncolor(const char *s) {
   char *cc = strchr(s,'[');
   if (cc++) {
      uchar rc = *cc;
      if (rc<'0'||rc>'3') rc=*cc++;
      if (rc>='0'&&rc<='3') {
         static uchar llcol[4] = //{ 0x0E, 0x0F, 0x0B, 0x03 };
                                   { 0x0A, 0x0F, 0x07, 0x08 };
         return llcol[rc-'0'];
      }
   }
   return 0x0F;
}

void TSysApp::SwitchLogWindow() {
   if (windows[TWin_Log]) {
      windows[TWin_Log]->select();
      // refresh?
   } else {
      TRect       rr = deskTop->getExtent();
      TWindow   *win = new TAppWindow(rr,"Log view",TWin_Log);

      if (win && application->validView(win)) {
         const int TermBufferSize = 256*1024;

         rr = TRect(1, 1, win->size.x-1, win->size.y-1);
         win->options |= ofTileable;
         win->helpCtx  = hcViewLog;

         TTerminal *term = new TTerminal(rr,
            win->standardScrollBar(sbHorizontal|sbHandleKeyboard),
            win->standardScrollBar(sbVertical|sbHandleKeyboard), TermBufferSize);

         char *log = opts_getlog();
         int  llen = strlen(log), ii;
         if (llen<TermBufferSize - 1024) term->do_sputn(log,llen); else {
            for (ii=0; ii<llen; ii+=TermBufferSize/2)
               term->do_sputn(log+ii,llen-ii>TermBufferSize/2?TermBufferSize/2:llen-ii);
         }
         free(log);
         term->getLineColor=loglncolor;

         win->insert(term);
         deskTop->insert(win);
         term->scrollTo(0, 0);
      }
   }
}

void TSysApp::OpenHelp(ushort goHelpCtx, int dlgmode) {
   if (dlgmode) {
      FILE *hpf = fopen(helpFileName, "rb");
      if (!hpf) {
         errDlg(MSGE_NOHELPFILE);
         return;
      }
      THelpWindow *win = new THelpWindow(new THelpFile(hpf), goHelpCtx);
      execView(win);
      destroy(win);
   } else
   if (!windows[TWin_Help]) {
      FILE *hpf = fopen(helpFileName, "rb");
      if (!hpf) {
         errDlg(MSGE_NOHELPFILE);
         return;
      }
      TAppHlpWindow *win = new TAppHlpWindow(new THelpFile(hpf), goHelpCtx);
      deskTop->insert(win);
   } else {
      TAppHlpWindow *win = (TAppHlpWindow*)windows[TWin_Help];
      win->goTopic(goHelpCtx);
      win->select();
   }
}

extern "C" int tvMain(int argc,char *argv[]) {
   if (argc>2) {
      if (stricmp(argv[1],"/boot")==0||stricmp(argv[1],"/rest")==0) {
         SysApp.kernel = argv[2];
         SysApp.kernel_parm = argc>3?argv[3]:0;
         SysApp.bootcmd = toupper(argv[1][1])=='R'?cmLdrBootSetup:cmBootSetup;
      }
   } else
   if (argc>1) {
      if (stricmp(argv[1],"/log")==0) SysApp.bootcmd = cmViewLog; else
      if (stricmp(argv[1],"/bm")==0) SysApp.bootcmd = cmBmgrMenu; else
      if (stricmp(argv[1],"/dm")==0) SysApp.bootcmd = cmDmgr; else
      if (strnicmp(argv[1],"/mem",4)==0) {
         SysApp.bootcmd = cmMemEdit;
         // /mem:pos or /mem to go to 2nd Mb
         if (argv[1][4]==':') SysApp.cmMemEditPos = strtoul(argv[1]+5, 0, 16);
            else SysApp.cmMemEditPos = 0x100000;
      }
   }
   SysApp.run();
   SysApp.shutDown();
   return 0;
}
