//
// QSINIT "sysview" module
// common header
//
#ifndef QS_SYSVIEW_COMMON
#define QS_SYSVIEW_COMMON

#define Uses_TApplication
#define Uses_TEditWindow
#define Uses_TRadioButtons
#include "kdlg.h"
#include "ldrdlg.h"
#include "rundlg.h"
#include "kdlghelp.h"
#include "kdlghist.h"
#include "pdiskio.h"
#include <qstypes.h>
#include "longedit.h"
#include "binedit.h"
#include "diskdlg.h"
#include "direct.h"   // NAME_MAX
#ifdef __QSINIT__
#include "qserr.h"
#include "qsdm.h"
#include "qssys.h"
#endif

#define cmBinTrunc     149

void SwitchRegDialogCmd(int on);

int  SetupBootDlg (TKernBootDlg *dlg, char *kernel, char *opts);
int  CheckBootDlg (TKernBootDlg *dlg);
void RunKernelBoot(TKernBootDlg *dlg);
void SetupLdrBootDlg(TLdrBootDialog *dlg, char *file);

void FillDiskInfo(TDialog* dlg, u32t disk, long index);

char *getstr (TInputLine *ln);
u32t  getuint(TInputLine *ln);
u64t  getui64(TInputLine *ln);
void  setstr (TInputLine *ln, const char *str);
void  setstrn(TInputLine *ln, const char *str, int maxlen);
/// "Import file"/"Export to file" file name dialog
char *getfname(int open, const char *custom_title = 0, const char *path = 0);
/// return truncated to 23 chars file name (with leading "...", in static buffer!)
char *getsname(const char *filename);

/** replace colored text control.
    @param [in,out] txt           Pointer to control
    @param [in]     str           New text to set
    @param [in]     const_bounds  Adjust control size to text if 0, else
                                  do not change control size at all
    @param [in]     color         Color for text (0 for previous value) */
void  replace_coltxt(TColoredText **txt, const char *str, int const_bounds = 0, ushort color = 0);
void  replace_statictxt(TStaticText **txt, const char *str, int const_bounds = 0);
TRect getwideBoxRect(int addlines = 0);
long  getRadioIdx(TRadioButtons *rb, long maxidx);
/// get disk number from list box with text "HDD 0", "HDD 1", etc..
long  getDiskNum(TListBox *lb, long item=-1);

char *getDiskName(u32t disk, char *buf=0);
char *getSizeStr(u32t sectsize, u64t disksize, Boolean trim = False);

TDialog* makeSelDiskDlg(int DlgType);
TDialog* makeSelBootDiskDlg(void);

void  opts_prepare(char *opts);
char *opts_get(const char *name);
u32t  opts_baudrate();
u32t  opts_port();
void  opts_free();
char  opts_bootdrive();
void  opts_bootkernel(char *name, char *opts);
u32t  opts_loadldr(char *name);
void  opts_run(char *cmdline, int echoon, int pause);
char *opts_getlog(int time = 1, int dostext = 0);
u32t  opts_cpuid(u32t index, u32t *data);
u32t  opts_getpcmem(u64t *highaddr = 0);
u32t  opts_getcputemp(void);
u32t  opts_acpiroot(void);
u32t  opts_mtrrquery(u32t *flags, u32t *state, u32t *addrbits);
u64t  opts_fsize(const char *str);
u32t  opts_memread(u64t pos, void *data);
u32t  opts_memwrite(u64t pos, void *data);
int   opts_fsetsize(FILE *ff, u64t newsize);
/** returns free space on disk, in bytes.
    @param drive: 0 for A:, 1 for B: and so on. */
u64t  opts_freespace(unsigned drive);
void* opts_freadfull(const char *name, u32t *bufsize, int *reterr);
u32t  opts_memend(void);
/// sleep a bit - special function for TApplication::idle
void  opts_yield();
void  opts_settitle(const char *title);
/// return 0 - no file, 1 - file, 2 - dir
int   opts_fileexist(const char *str);
int   opts_fseek(FILE *ff, long long offset, int where);
extern "C"
void  opts_beep(u32t freq, u32t duration);

/// @name flags values for TView::userValue[0]
//@{
#define TVOF_CONTEXTHELP   0x00001      ///< dialog with help topics for controls
//@}

enum  TAppWindowType { AppW_Log, AppW_Sector, AppW_Help,
                       AppW_Calc, AppW_Memory, AppW_BinFile,
                       AppW_Max = AppW_BinFile };

class TAppWindow;
class THexEdWindow;

/// main app class
class TSysApp : public TApplication {
  u32t                   lastBrowseExtPos,
                       lastBrowseExtSlice;
public:
  char                            *kernel;
  char                       *kernel_parm;
  char                        *cmEditName;
  int                             bootcmd;
  int                      nextBootOwnMbr;
  int                      createAtTheEnd,
                               createAFal;
  int                           icaseSrch,
                                 beepSrch;
  u32t                       cmMemEditPos;

  TEditWindow                 *clipWindow;
  TDiskSearchDialog            *searchDlg;
  TDiskCopyDialog                *copyDlg;
  TDiskBootDialog                *bootDlg;
  THexEditorData               searchData;
  TDiskSearchDialog::stopReason
                           lastSearchStop;
  TAppWindow                     *windows[AppW_Max+1],
                           *lastSearchWin;

  TSysApp();
  virtual ~TSysApp();

  #define CS_D     0x8000    // disk editor commands
  #define CS_M     0x4000    // memory editor commands
  #define CS_F     0x2000    // binary file editor commands
  #define CS_A     (CS_M|CS_D|CS_F)
  #define CS_MASK  0x00FF    // conter mask
  void   SetCommandState(u16t mask, int onoff);
  /// allocates new Alt-X number
  short  allocWinNumber();

  #define SELDISK_BOOT     0
  #define SELDISK_COMMON   1
  #define SELDISK_BOOTMGR  2
  #define SELDISK_COPYTGT  3
  u32t   GetDiskDialog(int floppies=1, int vdisks=1, int dlgtype=SELDISK_COMMON);

  #define SCSEL_GOTO  0   // return sector number
  #define SCSEL_SAVE  1   // return void
  #define SCSEL_READ  2   // return void
  #define SCSEL_COPY  3   // return void
  #define SCSEL_TGT   4   // return sector number
  u64t   SelectSector(int mode, u32t disk, long vol=-1, u64t start=0,
                      u64t length=0, u64t cpos=0);
  void   doDiskCopy(int mode, u32t disk, u64t pos, u32t len, const char *fpath,
                    u32t dstdisk=FFFF, u64t dstpos=FFFF64);
  void   doMemSave(u64t addr, u64t len, const char *fpath);

  static TStatusLine *initStatusLine( TRect r );
  static TMenuBar *initMenuBar( TRect r );

  void   SwitchLogWindow();
  void   OpenPosDiskWindow(ul disk, uq sector);
  void   OpenPosMemWindow(uq position);

  // return window for binary action or 0 if no one
  TAppWindow*   BinActionWindow();
  // find text or binary file editor window by file full path
  TWindow*      FindEditorWindow(const char *fullp);

  THexEdWindow* OpenHexEditor(int NewFile, const char *fname=0);
  TEditWindow*  OpenEditor(int NewFile, int hide=False, const char *fname=0);
  void          CloseEditor();
  /** toggle state of save & minor editor commands.
      binedit=1 (binary editor), 0 (text editor), -1 (auto)
      on = 1/0 or -1 for auto */
  void   ToggleEditorCommands(int binedit=-1, int on=-1);
  void   SaveLog(Boolean TimeMark);

  void   SessionListDlg();
  void   ProcInfoDlg();
  void   SessionNew();

  void   ExecCpuInfoDlg();
  void   BootmgrMenu();
  void   SearchBinStart(TAppWindow *who);
  void   SearchBinNext(TAppWindow *who);
  void   GotoMemDlg();
  void   MemSaveDlg();
  void   PowerOFF();
  void   DiskInit(u32t disk, int makegpt = 0);
  void   DiskMBRCode(u32t disk);
  int    CloneDisk(u32t disk);
  int    ClonePart(u32t srcdisk, u32t index);
  int    SaveRestVHDD(u32t disk, int rest);
  void   UpdateLVM(u32t disk, int firstTime = 0);
  void   Unmount(u8t vol);
  void   BootPartition(u32t disk, long index);
  void   BootDirect(u32t disk, long index);
  void   FormatDlg(u8t vol, u32t disk, long index);
  void   SetVolLabel(u8t vol, u32t disk, long index);
  void   BootCodeDlg(u8t vol, u32t disk, long index);
  void   QSInstDlg(u8t vol, u32t disk, long index);
  void   ChangeDirty(u8t vol, u32t disk, long index);
  void   CreatePartition(u32t disk, u32t index, int logical);
  void   MountDlg(Boolean qsmode, u32t disk, long index, char letter = 0);
  void   SetGPTType(u32t disk, u32t index);
  void   LVMRename(u32t disk, u32t index);
  /// use 0xFFFF in goHelpCtx to helpCtx of current TGroup, not common control
  void   OpenHelp(ushort goHelpCtx, int dlgmode = 0);
  /// return 1 on success, -1 on cancel pressed in "format" mode.
  int    SetCodepage(int format);

  int    IsSysWindows(TView *, TAppWindowType *wtype = 0);
  void   CloseAll();

  /// return 0 if disk modified in editor and Cancel was pressed in "write" dlg
  int    CanChangeDisk(u32t disk);
  /// call this after disk changes by PARTMGR to re-read editor window
  void   DiskChanged(u32t disk=FFFF);

  virtual void handleEvent(TEvent& event);
  virtual void getEvent(TEvent& event);
  virtual void idle();
  void   LoadFromZIPRevArchive(TInputLine *ln);

  #define MSGTYPE_QS    0
  #define MSGTYPE_CLIB  1
  char*  GetPTErr(u32t rccode, int msgtype = MSGTYPE_QS);
  void   PrintPTErr(u32t rccode, int msgtype = MSGTYPE_QS);

  /// prepare to possible "percent window" showing
  void   PercentDlgOn(const char *text);
  /// remove "percent window" and restore focus to owner
  void   PercentDlgOff(TView *owner);
  /// "percent window" callback for system use (read_callback type)
  static void _std PercentDlgCallback(u32t percent, u32t size);

  int    askDlg (int MsgType, u32t arg = 0);
  void   infoDlg(int MsgType);
  void   errDlg (int MsgType);

  void   newWindow();
};

#define MSGA_MBRCODE       (0)
#define MSGA_LVM512        (1)
#define MSGA_LVMSMALL      (2)
#define MSGA_POWEROFF      (3)
#define MSGA_ACTIVE        (4)
#define MSGA_PRIMARY       (5)
#define MSGA_LVM512EXIST   (6)
#define MSGA_QUICKFMT      (7)
#define MSGA_TURNPAE       (8)
#define MSGA_PTEMPTY       (9)
#define MSGA_ACTIVEGPT    (10)
#define MSGA_FMTCPSERR    (11)
#define MSGA_DEVICEMEM    (12)
#define MSGA_JUSTVALUE    (13)
#define MSGA_TURNMTMODE   (14)

#define MSGI_DONE          (0)
#define MSGI_SRCHNOTFOUND  (1)
#define MSGI_LENTRUNCFILE  (2)
#define MSGI_LENTRUNCDISK  (3)
#define MSGI_LENTRUNCTDISK (4)
#define MSGI_SAMEPOS       (5)
#define MSGI_LVMOK         (6)
#define MSGI_BOOTCODEOK    (7)
#define MSGI_BOOTQSOK      (8)
#define MSGI_DEVICEMEM     (9)
#define MSGI_WINSELFAILED (10)
#define MSGI_FILEREADONLY (11)

#define MSGE_FLOPPY        (0)
#define MSGE_NOTSUPPORTED  (1)
#define MSGE_BOOTPART      (2)
#define MSGE_MBRWRITE      (3)
#define MSGE_NOTEMPTY      (4)
#define MSGE_MBRREAD       (5)
#define MSGE_INVVALUE      (6)
#define MSGE_RANGE         (7)
#define MSGE_NOAPM         (8)
#define MSGE_SRCHREADERR   (9)
#define MSGE_SRCHLONG     (10)
#define MSGE_NOHELPFILE   (11)
#define MSGE_FILESIZEERR  (12)
#define MSGE_LOWSPACE     (13)
#define MSGE_GT4GB        (14)
#define MSGE_INTERFERENCE (15)
#define MSGE_READERR      (16)
#define MSGE_WRITEERR     (17)
#define MSGE_SELACTION    (18)
#define MSGE_BSEMPTY      (19)
#define MSGE_COMMONFAIL   (20)
#define MSGE_COPYTLENGTH  (21)
#define MSGE_NOTGPT       (22)
#define MSGE_GUIDFMT      (23)
#define MSGE_LVMQUERY     (24)
#define MSGE_NOMEMORY     (25)
#define MSGE_FILEOPENERR  (26)
#define MSGE_FILEEMPTY    (27)
#define MSGE_MOUNTERROR   (28)
#define MSGE_SECSIZEMATCH (29)
#define MSGE_NOVHDD       (30)
#define MSGE_ACCESSDENIED (31)
#define MSGE_NOTVHDDFILE  (32)
#define MSGE_NSECMATCH    (33)
#define MSGE_VMOUNTERR    (34)
#define MSGE_INVDISK      (35)
#define MSGE_FILECREATERR (36)
#define MSGE_NOBOOTFILES  (37)
#define MSGE_UNCKFS       (38)
#define MSGE_ABOVE2TB     (39)
#define MSGE_CROSS2TB     (40)
#define MSGE_BOOTFN11     (41)
#define MSGE_QSWRITEERR   (42)
#define MSGE_FILETRUNCERR (43)
#define MSGE_DIRNAME      (44)
#define MSGE_FILEOPENED   (45)
#define MSGE_FSUNSUITABLE (46)
#define MSGE_BADNAME      (47)

extern TSysApp SysApp;

class TAppWindow: public TWindow {
   void removeAction();
   friend void TSysApp::CloseAll();
public:
   TAppWindow (const TRect &bounds, const char *aTitle, TAppWindowType wt);
   virtual void close();

   TAppWindowType wType;
};

class TDskEdWindow: public TAppWindow {
public:
   TLongEditorData led;
   TLongEditor *sectEd;
   u32t           disk;

   TDskEdWindow (const TRect &bounds, const char *aTitle) :
      TAppWindow (bounds, aTitle, AppW_Sector), TWindowInit(initFrame)
   {
      sectEd  = 0;
      disk    = FFFF;
      helpCtx = hcSectEdit;
   }
};

class TMemEdWindow: public TAppWindow {
public:
   TLongEditorData led;
   TLongEditor  *memEd;

   TMemEdWindow (const TRect &bounds, const char *aTitle) :
      TAppWindow (bounds, aTitle, AppW_Memory), TWindowInit(initFrame)
   {
      memEd   = 0;
      helpCtx = hcMemEdit;
      led.userdata = 0;
   }
};

class THexEdWindow: public TAppWindow {
public:
   TLongEditorData led;
   TLongEditor  *hexEd;
   FILE       *srcFile;
   u32t         bshift;
   char       *srcPath;

   THexEdWindow (const TRect &bounds, const char *aTitle) :
      TAppWindow (bounds, aTitle, AppW_BinFile), TWindowInit(initFrame)
   {
      hexEd   = 0;
      srcFile = 0;
      srcPath = 0;
      helpCtx = hcBinEdit;
   }
   virtual void handleEvent( TEvent& );
   virtual void shutDown();
   void GotoFilePosDlg();
};

/** mount volume until the end of scope.
    Volume will be unmounted in destructor, but only if it was mounted here */
class TempVolumeMounter {
   int remove_me;
   u8t      mvol;
public:
   // vol should 0 for common use, index can be -1 for big floppy
   TempVolumeMounter(u8t &vol, u32t disk, long index, Boolean noremove = False) {
      remove_me = 0; mvol = 0;
      if (!vol || vol==0xFF) {
#ifdef __QSINIT__
         vol = 0;
         u32t rc = vol_mount(&vol, disk, index, 0);
         if (rc && rc!=E_DSK_MOUNTED) {
            SysApp.PrintPTErr(rc);
            // reset it to the incorrect value
            vol = 0xFF;
            return;
         } else {
            // E_DSK_MOUNTED means - no remove at caller`s end
            remove_me = !rc && !noremove;
            mvol = vol;
         }
#endif
      }
   }
   ~TempVolumeMounter() {
#ifdef __QSINIT__
      if (remove_me) hlp_unmountvol(mvol);
#endif
   }
};

#endif // QS_SYSVIEW_COMMON
