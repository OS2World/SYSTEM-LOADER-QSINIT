//
// QSINIT "start" module
// some local functions (not exported)
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

/** make copy of process environment.
    Heap context is default (START.DLL)
    @param pq        Process context.
    @param addspace  Additional space to allocate in result memory block.
    @return environment in malloc(envlen()+addspace) buffer */
char* _std envcopy(process_context *pq, u32t addspace);

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

/// hlp_freadfull() with progress printing in form "caching name    x mb|x %"
void*      hlp_freadfull_progress(const char *name, u32t *bufsize);

/// free notification callbacks for specified pid
void       sys_notifyfree(u32t pid);
/// exec notification list (SECB_* in qssys.h)
void _std  sys_notifyexec(u32t eventtype, u32t infovalue);
/// set thread owner
int  _std  mem_threadblockex(void *block, u32t pid, u32t tid);
/// set module owner
int _std   mem_modblockex(void *block, u32t module);
/** get thread comment.
    note, what result is up to 16 chars & NOT terminated by zero if
    length is full 16 bytes.
    @return direct pointer to TLS variable with thread comment */
char*      mt_getthreadname(void);

/** global printed lines counter.
    Used for pause message calculation, updated by common vio functions and
    CONSOLE (in virtual modes) */
u32t*_std  vio_ttylines();

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

/// "trace on/off" command.
int        trace_onoff(const char *module, const char *group, int quiet, int on);

/// "trace list" command.
void       trace_list(const char *module, const char *group, int pause);

/** read string from msg.ini.
    @param section      Section name
    @param key          Key name
    @return string (need to be free()) or 0 */
char*      msg_readstr(const char *section, const char *key);

/// draw colored box
void       draw_border(u32t x, u32t y, u32t dx, u32t dy, u32t color);

/// QSINIT is in the page mode (flag = 1/0)
extern u8t  in_pagemode;

/// QSINIT in MT mode (flag = 1/0)
extern u8t    in_mtmode;

/// disable task gate usage for exceptions
extern u32t   no_tgates;

/// exe delayed unpack
extern u8t    mod_delay;

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
u32t       _lsl_(u32t sel);

#ifdef __WATCOMC__
#pragma aux getcr0 = "mov eax,cr0" modify exact [eax];
#pragma aux getcr3 = "mov eax,cr3" modify exact [eax];
#pragma aux getcr4 = "mov eax,cr4" modify exact [eax];
#pragma aux _lsl_  = "lsl eax,eax" parm [eax] value [eax] modify exact [eax];
#endif

void _std  set_xcr0(u64t value);
u64t _std  get_xcr0(void);

/** get mtlib class instance.
    Instance is created once and shared over START module, so do not use
    DELETE on it.
    @return class instance or 0 if no MTLIB module */
qs_mtlib   get_mtlib(void);

/// custom dump mdt
void       log_mdtdump_int(printf_function pfn);

/// internal i/o fullpath for clib implementation
char* _std io_fullpath_int(char *dst, const char *path, u32t size, qserr *err);

/// internal mt_closehandle() with ability to force closing of busy mutex
qserr _std mt_closehandle_int(qshandle handle, int force);

qserr      qe_closehandle(qshandle handle);

void _std  fpu_stsave   (process_context *newpq);
void _std  fpu_strest   (void);
void _std  fpu_rmcallcb (void);
void _std  fpu_reinit   (void);

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

/// split text with carry and eol (^) support.
void       splittext(const char *text, u32t width, TStrings &lst);

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
int        shl_dbcard(const spstr &setup, int __cdecl (*pfn)(const char *fmt, ...));

/// ansi state push/pop for cpp functions
class ansi_push_set {
   u32t state;
public:
   ansi_push_set(int newstate) { state = vio_setansi(newstate); }
   ~ansi_push_set() { vio_setansi(state); }
};

class MTLOCK_THIS_FUNC {
public:
   MTLOCK_THIS_FUNC() { mt_swlock(); }
   ~MTLOCK_THIS_FUNC() { mt_swunlock(); }
};

#endif // __cplusplus
#endif // QSINIT_STLOCAL
