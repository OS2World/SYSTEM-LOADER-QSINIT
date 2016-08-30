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
#include "diskio.h"
#include <qstypes.h>
#include "longedit.h"
#include "binedit.h"
#include "diskdlg.h"
#include "direct.h"   // NAME_MAX
#ifdef __QSINIT__
#include "qsdm.h"
#endif

void SwitchRegDialogCmd(int on);

int  SetupBootDlg (TKernBootDlg *dlg, char *kernel, char *opts);
int  CheckBootDlg (TKernBootDlg *dlg);
void RunKernelBoot(TKernBootDlg *dlg);
void SetupLdrBootDlg(TLdrBootDialog *dlg, char *file);

char *getstr (TInputLine *ln);
u32t  getuint(TInputLine *ln);
u64t  getui64(TInputLine *ln);
void  setstr (TInputLine *ln, const char *str);
void  setstrn(TInputLine *ln, const char *str, int maxlen);

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
char *opts_get(char *name);
u32t  opts_baudrate();
u32t  opts_port();
void  opts_free();
char  opts_bootdrive();
void  opts_bootkernel(char *name, char *opts);
void  opts_loadldr(char *name);
void  opts_run(char *cmdline, int echoon, int pause);
char *opts_getlog(int time = 1, int dostext = 0);
u32t  opts_cpuid(u32t index, u32t *data);
u32t  opts_getpcmem(u64t *highaddr = 0);
u32t  opts_getcputemp(void);
u32t  opts_mtrrquery(u32t *flags, u32t *state, u32t *addrbits);
u32t  opts_fsize(const char *str);
u32t  opts_memread(u64t pos, void *data);
u32t  opts_memwrite(u64t pos, void *data);
/** returns free space on disk, in bytes.
    @param drive: 0 for A:, 1 for B: and so on. */
u64t  opts_freespace(unsigned drive);
void* opts_sysalloc(u32t size);
void  opts_sysfree(void *ptr);
void* opts_freadfull(const char *name, u32t *bufsize, int *reterr);


enum TSysWindows { TWin_Log, TWin_SectEdit, TWin_Help, TWin_Calc, TWin_MemEdit,
                   TWin_Count };

class TAppWindow;
class THexEditWindow;

/// main app class
class TSysApp : public TApplication {
  u32t                   lastBrowseExtPos,
                       lastBrowseExtSlice;
public:
  char                            *kernel;
  char                       *kernel_parm;
  int                             bootcmd;
  int                      nextBootOwnMbr;
  int                      createAtTheEnd;
  int                           icaseSrch;
  u32t                       cmMemEditPos;

  TEditWindow                 *clipWindow;
  TDiskSearchDialog            *searchDlg;
  TDiskCopyDialog                *copyDlg;
  TDiskBootDialog                *bootDlg;
  THexEditorData               searchData;
  TDiskSearchDialog::stopReason
                           lastSearchStop;
  Boolean                  lastSearchMode;  // True - disk, False - memory
  TAppWindow                     *windows[TWin_Count];

  TSysApp();
  virtual ~TSysApp();

  #define CS_D     0x8000    // disk editor commands
  #define CS_M     0x4000    // memory editor commands
  #define CS_A     (CS_M|CS_D)
  #define CS_MASK  0x00FF    // conter mask
  void   SetCommandState(u16t mask, int onoff);

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

  static TStatusLine *initStatusLine( TRect r );
  static TMenuBar *initMenuBar( TRect r );

  void   SwitchLogWindow();
  void   OpenPosDiskWindow(ul disk, uq sector);
  void   OpenPosMemWindow(uq position);

  // return 0 if disk window active, 1 is mem, -1 if no both windows
  int    IsMemAction();

  TEditWindow* OpenEditor(int NewFile, int hide=False);
  void   CloseEditor();
  void   ToggleEditorCommands(int on);
  void   SaveLog(Boolean TimeMark);

  THexEditWindow* OpenHexEditor(int NewFile);

  void   ExecCpuInfoDlg();
  void   BootmgrMenu();
  void   SearchBinStart(Boolean is_disk);
  void   SearchBinNext(Boolean is_disk);
  void   GotoMemDlg();
  void   PowerOFF();
  void   DiskInit(u32t disk, int makegpt = 0);
  void   DiskMBRCode(u32t disk);
  int    CloneDisk(u32t disk);
  int    ClonePart(u32t srcdisk, u32t index);
  int    SaveRestVHDD(u32t disk, int rest);
  void   UpdateLVM(u32t disk, int firstTime = 0);
  void   Unmount(u8t vol);
  void   BootPartition(u32t disk, long index);
  void   FormatDlg(u8t vol, u32t disk, u32t index);
  void   BootCodeDlg(u8t vol, u32t disk, u32t index);
  void   QSInstDlg(u8t vol, u32t disk, u32t index);
  void   ChangeDirty(u8t vol, u32t disk, u32t index);
  void   CreatePartition(u32t disk, u32t index, int logical);
  void   MountDlg(Boolean qsmode, u32t disk, u32t index, char letter = 0);
  void   SetGPTType(u32t disk, u32t index);
  void   LVMRename(u32t disk, u32t index);
  void   OpenHelp(ushort goHelpCtx, int dlgmode = 0);
  /// return 1 on success, -1 on cancel pressed in "format" mode.
  int    SetCodepage(int format);

  // count number of system(numbered) windows (editor windows is NOT system)
  int    CountSysWindows();
  int    IsSysWindows(TView *);
  void   CloseAll();

  /// return 0 if disk modified in editor and Cancel was pressed in "write" dlg
  int    CanChangeDisk(u32t disk);
  /// call this after disk changes by PARTMGR to re-read editor window
  void   DiskChanged(u32t disk=FFFF);

  virtual void handleEvent(TEvent& event);
  virtual void getEvent(TEvent& event);
  virtual void idle();
  void   LoadFromZIPRevArchive(TInputLine *ln);

  #define MSGTYPE_PT    0
  #define MSGTYPE_LVM   1
  #define MSGTYPE_FMT   2
  #define MSGTYPE_QS    3
  #define MSGTYPE_CLIB  4
  char*  GetPTErr(u32t rccode, int msgtype = MSGTYPE_PT);
  void   PrintPTErr(u32t rccode, int msgtype = MSGTYPE_PT);

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

#define MSGI_DONE          (0)
#define MSGI_SRCHNOTFOUND  (1)
#define MSGI_LENTRUNCFILE  (2)
#define MSGI_LENTRUNCDISK  (3)
#define MSGI_LENTRUNCTDISK (4)
#define MSGI_SAMEPOS       (5)
#define MSGI_LVMOK         (6)
#define MSGI_BOOTCODEOK    (7)
#define MSGI_BOOTQSOK      (8)

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

extern TSysApp SysApp;

class TAppWindow: public TWindow {
public:
   TAppWindow (const TRect &bounds, const char *aTitle, short aNumber);
   virtual void close();
   // do not call it directly!
   void removeAction();
};

class TDskEdWindow: public TAppWindow {
public:
   TLongEditorData led;
   TLongEditor *sectEd;
   u32t           disk;

   TDskEdWindow (const TRect &bounds, const char *aTitle) :
      TAppWindow (bounds, aTitle, TWin_SectEdit), TWindowInit(initFrame)
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
      TAppWindow (bounds, aTitle, TWin_MemEdit), TWindowInit(initFrame)
   {
      memEd   = 0;
      helpCtx = hcMemEdit;
   }
};

/** mount volume until the end of scope.
    Volume will be unmounted in destructor, but only if was mounted here */
class TempVolumeMounter {
   int remove_me;
   u8t      mvol;
public:
   TempVolumeMounter(u8t &vol, u32t disk, u32t index, Boolean noremove = False) {
      remove_me = 0; mvol = 0;
      if (!vol || vol==0xFF) {
#ifdef __QSINIT__
         vol = 0;
         u32t rc = vol_mount(&vol, disk, index);
         if (rc && rc!=DPTE_MOUNTED) {
            SysApp.PrintPTErr(rc);
            // reset it to incorrect value
            vol = 0xFF;
            return;
         } else {
            // DPTE_MOUNTED means - no remove at caller`s end
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
