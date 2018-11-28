//
// QSINIT "start" module
// some local functions (non-exported)
//
#ifndef QSINIT_STLOCAL
#define QSINIT_STLOCAL

#include "stdlib.h"
#include "qsmodext.h"
#include "qspdata.h"
#include "qsint.h"
#include "qs_rt.h"
#include "direct.h"
#include "vioext.h"
#include "qcl/qsmt.h"
#include "qsio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int __cdecl (*printf_function)(const char *fmt, ...);

/** trace helper macros.
    @li declare function as START_EXPORT(rmdir)
    @li add option expname = "__rmdir" into start.ref file _rmdir export
    @li rebuild runtime & start
    @li all <b>internal</b> rmdir calls will be linked as imports from entry
        _rmdir of this module and can be catched by trace */
#define START_EXPORT(x) _ ## x

/// lock environment access mutex for this process (in MT mode)
void       env_lock(void);

/// unlock environment access mutex
void       env_unlock(void);

/// malloc in process context
void*      malloc_local(u32t size);

// must be used outside clib.c - else watcom link it as forward entry ;)
int        set_errno(int errno_value);
// return new errno value and set var itself
int        set_errno_qserr(u32t err);
int        get_errno(void);
// error code convertion
int        qserr2errno(qserr errv);
// direct using of std* handles in START is not supported too
FILE*      get_stdout(void);
FILE*      get_stdin(void);

int        fcloseall_as(u32t pid);
void       exi_free_as(u32t pid);
void       chain_thread_free(mt_thrdata *th);

/// init signal handler for a new process
void       init_signals(void);

/// hlp_freadfull() with progress printing in form "caching name    x mb|x %"
void*      hlp_freadfull_progress(const char *name, u32t *bufsize);

/// free notification callbacks for specified pid
void       sys_notifyfree(u32t pid);
void       sys_hotkeyfree(u32t pid);
/// set thread owner
int  _std  mem_threadblockex(void *block, mt_thrdata *th);
/// set module owner
int  _std  mem_modblockex(void *block, u32t module);
/** get thread comment.
    note, what result is up to 16 chars & NOT terminated by zero if
    length is full 16 bytes.
    @return direct pointer to TLS variable with thread comment */
char*      mt_getthreadname(void);

void       run_notify(sys_eventcb cbfunc, u32t type, u32t info, u32t info2,
                      u32t info3, u32t pid);

/// panic error message without memmgr usage
void       panic_no_memmgr(const char *msg);

// reset video to CVIO, stop APIC timer, and so on... 
void       trap_screen_prepare(void);

/// floating point to str conversion (for printf and other)
int  _std  fptostr(double value, char *buf, int int_fmt, int prec);

// free pushd stack
void       pushd_free(void* ptr);

// random
u32t _std  random(u32t range);

// set "errorlevel" var
void       set_errorlevel(int elvl);

/** get process/thread data pointers.
    Must be called in MT lock! */
void _std  mt_getdata(mt_prcdata **ppd, mt_thrdata **pth);

/** reset chain/hook thunk for this entry.
    No any changes in chain lists, only thunk reset. Used by trap screen. */
void       mod_resetchunk(u32t mh, u32t ordinal);

/** return process context for the specified process id.
    Function exported, but unpublished in headers because it dangerous.
    It TURNS ON MT lock if MT mode active and returned value is non-zero */
process_context* _std mod_pidctx(u32t pid);

/// "trace on/off" command.
int        trace_onoff(const char *module, const char *group, int quiet, int on);

/// "trace list" command.
void       trace_list(const char *module, const char *group, int pause);

/// "trace pidlist" command.
void       trace_pid_list(int pause);

/// special query id, which allows to get private class by name (used in trace)
u32t       exi_queryid_int(const char *classname, int noprivate);

/** read string from msg.ini.
    @param section      Section name
    @param key          Key name
    @return string (need to be free()) or 0 */
char*      msg_readstr(const char *section, const char *key);

/// draw colored box
void       draw_border(u32t x, u32t y, u32t dx, u32t dy, u32t color);

/// PAE page mode active (flag = 1/0)
extern u8t  in_pagemode;
/// MT mode active (flag = 1/0)
extern u8t    in_mtmode;
/// disable task gate usage for exceptions
extern u32t   no_tgates;
/// allow Ctrl-N on display device as a task switch key
extern u32t    ctrln_d0;
/// exe delayed unpack
extern u8t    mod_delay;
/// QSINIT module handle
extern u32t   mh_qsinit;
///< "system console" session
extern u32t     sys_con;
/// service thread system queue
extern qshandle   sys_q;

///< timeout in seconds before reboot from the trap screen (0 to disable)
extern u32t   reipltime;
/// REIPL action (cad wait or reboot)
void _std  reipl(int errcode);
/// system memory manager panic screen
void _std  mempanic(u32t type, void *addr, u32t info, char here, u32t caller);

/// init new process std i/o handles
void       init_stdio(process_context *pq);

/// push len bytes from str to log
int        log_pushb(int level, const char *str, int len, u32t timemark);
/// unzip all skipped executables (called in sep.thread in MT mode)
void       unzip_delaylist(void);
/// vio_strout with ansi filtering
void       ansi_strout(char *str);
// function is not published in vio.h
void _std  vio_getmodefast(u32t *cols, u32t *lines);

u32t       getcr0(void);
u32t       getcr3(void);
u32t       getcr4(void);
void       cpuhlt(void);
u32t       _lsl_(u32t sel);

#ifdef __WATCOMC__
#pragma aux getcr0 = "mov eax,cr0" modify exact [eax];
#pragma aux getcr3 = "mov eax,cr3" modify exact [eax];
#pragma aux getcr4 = "mov eax,cr4" modify exact [eax];
#pragma aux _lsl_  = "lsl eax,eax" parm [eax] value [eax] modify exact [eax];
#pragma aux cpuhlt = \
   "sti"             \
   "hlt"             \
   modify exact [];
#endif

void _std  set_xcr0(u64t value);
u64t _std  get_xcr0(void);

/** get mtlib class instance.
    Instance created only once and shared over START module, so NEVER use
    DELETE on it.
    @return class instance or 0 if no MTLIB module */
qs_mtlib   get_mtlib(void);

/// custom dump mdt
void       log_mdtdump_int(printf_function pfn);

/// dump file systems
void       fs_list_int(printf_function pfn);

/// internal i/o fullpath for clib implementation
char* _std io_fullpath_int(char *dst, const char *path, u32t size, qserr *err);

/// internal mt_closehandle() with ability to force closing of busy mutex
qserr _std mt_closehandle_int(qshandle handle, int force);

qserr      qe_closehandle(qshandle handle);

/// enum sessions & close all empty of them
void  _std se_closeempty();

/** return maximum session number + 1.
    Function returns 0 while sesmgr is not ready.
    Session list may have holes in it, i.e. this value is NOT a number of
    sessions.
    Function is MT safe. */
u32t       se_curalloc  (void);

void  _std fpu_stsave   (process_context *newpq);
void  _std fpu_strest   (void);
void  _std fpu_rmcallcb (void);
void  _std fpu_reinit   (void);

/** set cr3 and cr4 regs.
    Function also updates internal variables of DPMI code.
    @param  syscr3     New cr3 value, FFFF to ignore
    @param  syscr4     New cr4 value, FFFF to ignore */
void  _std sys_setcr3(u32t syscr3, u32t syscr4);

/** get current directory to malloc-ed buffer.
    see also getcurdir() below */
char* _std getcurdir_dyn(void);

#ifdef __cplusplus
}
/// str_getlist, which returns memory in process context
str_list*  str_getlist_local(TStringVector &lst);

/// call external shell command directly
u32t       shl_extcall(spstr &cmd, TStrings &plist);

/// "trace pid" command.
int        trace_pid(TList &pid, int lnot, int lnew);

/// change file ext
spstr      changeext(const spstr &src, const spstr &newext);

/** make full path.
    @param path         Path to expand.
    @return success flag */
int        fullpath(spstr &path);

/** get current directory
    see also getcurdir_dyn() above.
    @param path         Target string.
    @return success flag */
int        getcurdir(spstr &path);

/** split file name to dir & name.
    @param [in]  fname  File name to split
    @param [out] dir    Directory, with '\' only in rare cases like \name
                        or 1:\name.
    @param [out] name   Name without path */
void       splitfname(const spstr &fname, spstr &dir, spstr &name);

void       dir_to_list(spstr &Dir, dir_t *info, d exclude_attr,
                       TStrings &rc, TStrings *dirs);

#define SplitText_NoAnsi   1    ///< disable ansi line length calculation
#define SplitText_HelpMode 2    ///< help printing, with & as soft-cr

/// split text with carry and eol (^) support.
void       splittext(const char *text, u32t width, TStrings &lst, u32t flags=0);

/** waiting for Y/N/A/Esc key.
    @param allow_all  Allow 'A' key press.
    @return -1 on Esc, 0 - N, 1 - Y, 2 - A. */
int        ask_yn(int allow_all=0);

/** search and load module.
    @param [in]  name   Module path (name will be uppercased by function)
    @param [out] error  Error code
    @return module handle or 0 on error */
module*    load_module(spstr &name, u32t *error);

/** read string from extcmd.ini.
    @param section      Section name
    @param key          Key name
    @return string */
spstr      ecmd_readstr(const char *section, const char *key);

/** read section from extcmd.ini.
    @param       section   Section name
    @param [out] lst       Section text
    @return success flag (1/0), i.e. existence flag. */
int        ecmd_readsec(const spstr &section, TStrings &lst);

/// get command list from extcmd.ini
void       ecmd_commands(TStrings &rc);

/// cpp version of cmd_initbatch() call
cmd_state  cmd_init2(TStrings &file, TStrings &args);

/** DBCARD INI option parsing and execution.
    @param setup        Setup string from INI or MODE CON command.
    @param pfn          Printf function for messages (log_printf() or printf()).
    @return success flag (1/0) */
int        shl_dbcard(const spstr &setup, printf_function pfn);

/// ansi state push/pop for cpp functions
class ansi_push_set {
   u32t state;
public:
   ansi_push_set(int newstate) { state = vio_setansi(newstate); }
   ~ansi_push_set() { vio_setansi(state); }
};

/// thread switching lock for cpp functions
class MTLOCK_THIS_FUNC {
public:
   MTLOCK_THIS_FUNC() { mt_swlock(); }
   ~MTLOCK_THIS_FUNC() { mt_swunlock(); }
};

extern "C" {
extern mod_addfunc       *mod_secondary;
#pragma aux mod_secondary  "_*";
}
#else // __cplusplus
extern mod_addfunc* _std  mod_secondary;
#endif // __cplusplus

#endif // QSINIT_STLOCAL
