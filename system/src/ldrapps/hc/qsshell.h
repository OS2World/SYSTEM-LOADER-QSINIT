//
// QSINIT API
// shell functions
//
#ifndef QSINIT_SHELL
#define QSINIT_SHELL

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"

//===================================================================
//  ini file management
//-------------------------------------------------------------------

/// full analogue of wnd`s GetPrivateProfileString
u32t _std ini_getstr(const char *Section, const char *Key, const char *Def,
                     char *Buf, u32t BufSize, const char *IniName);

/** read string key from ini.
    @param IniName  ini file name
    @param Section  ini section name, can be 0
    @param key      key name
    @return 0 if no key, section or ini, else string maked by stddup(). */
char*_std ini_readstr(const char *IniName, const char *Section, const char *Key);

/// full analogue of wnd`s WritePrivateProfileString
u32t _std ini_setstr(const char *Section, const char *Key, const char *Str,
                     const char *IniName);

/// full analogue of wnd`s GetPrivateProfileInt (except nefative result support)
long _std ini_getint(const char *Section, const char *Key, long Value,
                     const char *IniName);

/// store int value to .ini file
u32t _std ini_setint(const char *Section, const char *Key, long Value,
                     const char *IniName);


//===================================================================
//  string lists
//-------------------------------------------------------------------

/** string list.
    Pure C API simple string list. Any str_list, allocated by system
    calls - placed in one heap block, so to free it - just call to free() */
typedef struct {
   u32t   count;
   char*  item[1];
} str_list;

/// split string to items. result must be freed by single free() call
str_list* _std str_split(const char *str,const char *separators);

/// create list from array of char*
str_list* _std str_fromptr(char **list,int size);

/** return list of keys in specified Section of IniName.
    @param [in]  IniName  name of ini file
    @param [in]  Section  name of section
    @param [out] values   list of values for returning keys (can be 0)
    return list of keys or 0 */
str_list* _std str_keylist(const char *IniName, const char *Section, str_list**values);

/// return list of sections in IniName
str_list* _std str_seclist(const char *IniName);

/** return entire Section of IniName.
    @param IniName  name of ini file
    @param Section  name of section
    @param noempty  flag, do not return empty and commented lines (with ";" at 1st pos)
    @return string list */
str_list* _std str_getsec(const char *IniName,const char *Section, int noempty);

/// split string by command line parsing rules (with "" support)
str_list* _std str_splitargs(const char *str);

/// merge list to command line (wrap "" around lines with spaces)
char*     _std str_mergeargs(str_list*list);

/// read current environment to string list
str_list* _std str_getenv(void);

/// get text to single string (must be free() -ed).
char*     _std str_gettostr(str_list*list, char *separator);

/** parse argument list and set flags from it.
    Example of call:
    @code
      static char *argstr   = "+r|-r|+s|-s|+h|-h";
      static short argval[] = { 1,-1, 1,-1, 1,-1};
      str_parseargs(al, argstr, argval, &a_R, &a_R, &a_S, &a_S, &a_H, &a_H);
    @endcode
    @param lst      list with arguments
    @param args     argument names in form: "/boot|/q|/a" or "+h|-h|+a|-a"
    @param values   array of SHORT values for EVERY argument in args string
    @param ...      pointers to INT variables to setup */
void      _std str_parseargs(str_list *lst, char* args, short *values, ...);

/// print str_list to log
void      _std log_printlist(char *info, str_list*list);

//===================================================================
//  environment (shell)
//-------------------------------------------------------------------

/** check environment variable and return integer value.
    @param name     name of env. string
    @return 1 on TRUE/ON/YES, 0 on FALSE/OFF/NO, -1 if no string,
    else integer value of env. string */
int       _std env_istrue(const char *name);

//===================================================================
//  execution (shell)
//-------------------------------------------------------------------

/** single call batch file execution.
    @param  file    File name.
    @param  args    Arguments (including batch name, for %0 %1, etc), optional.
    @return result ;) */
u32t      _std cmd_exec(const char *file,const char *args);

typedef void *cmd_state;

/** init batch file process (cmds - file data)
    @param  cmds    Single command or batch file data.
    @param  args    Command line (including batch name, for %0 %1, etc), optional.
    @return handle for cmd_run(), cmd_close(). */
cmd_state _std cmd_init (const char *cmds,const char *args);

/** init batch file process (alternative way)
    @param  cmds    List of batch file strings.
    @param  args    Arguments (%0,%1,%2,etc), can be 0.
    @return handle for cmd_run(), cmd_close(). */
cmd_state _std cmd_initbatch(const str_list* cmds,const str_list* args);

#define CMDR_ONESTEP    0x0001        ///< run one step
#define CMDR_ECHOOFF    0x0002        ///< echo is off by default
#define CMDR_NESTCALL   0x0004        ///< internal, do not use

#define CMDR_RETERROR   0xFFFFFFFF    ///< exit code: internal error
#define CMDR_RETEND     0xFFFFFFFE    ///< exit code: execution finished

/// exec batch file (complete or one step)
u32t      _std cmd_run  (cmd_state cmdenv, u32t flags);

/// query session flags (CMDR_ECHOOFF).
u32t      _std cmd_getflags(cmd_state cmdenv);

/// free exec env
void      _std cmd_close(cmd_state cmdenv);

/** custom shell command processor.
    @param cmd      command name (uppercased)
    @param args     list with arguments (quotes around arguments are removed!)
    @return errorlevel or CMDR_RETERROR, CMDR_RETEND constants. */
typedef u32t _std (*cmd_eproc)(const char *cmd, str_list *args);

/** add custom shell command.
    Command can not overload predefined commands. Custom commands with the
    same name will be overloaded, but not deleted.

    @param name     command name
    @param proc     command processor
    @return previous installed command or 0. On error proc value returned. You
    do not need to restore previous command when remove own. */
cmd_eproc _std cmd_shelladd(const char *name, cmd_eproc proc);

/** remove previously installed custom shell command.
    @param name     command name
    @param proc     command processor, can be 0 to query current active proc.
    @return 0 or command proc, which became active for this name. */
cmd_eproc _std cmd_shellrmv(const char *name, cmd_eproc proc);

/** is this string registered as internal command?
    This function include internal commands to check.
    @param name     command name
    @return boolean */
int       _std cmd_shellquery(const char *name);

/// query list of all supported commands
str_list* _std cmd_shellqall(int ext_only);

/// help text default color (vio.h required for VIO_COLOR_LWHITE)
#define CLR_HELP  VIO_COLOR_LWHITE

/** print sequence of strings with "pause" support.
    @param fmt      string to print, without finishing \\n, but
                    can be multitined.
    @param flags    <0 - no pause, 1 - init counter, 3 - init and copy to log
                    all strings until next init, 0 - print with pause
    @param color    line color, 0 - default
    @return 1 if ESC was pressed in pause. */
u32t   __cdecl cmd_printseq(const char *fmt, int flags, u8t color, ...);

/** print long text to screen.
    @param text     text, need to be splitted to lines gracefully
    @param pause    non-zero to make pause at the end of screen. Use
                    cmd_printseq(0,1,0) to init pause line counter before.
    @param init     init lines counter
    @param color    color to print, can be 0 for default value
    @return  1 if ESC was pressed in pause. */
u32t      _std cmd_printtext(const char *text, int pause, int init, u8t color);

/** printf with "pause".
    Call to cmd_printseq() to init line counter and log copying state.
    @param fmt      string to print
    @return number of printer characters */
int    __cdecl cmd_printf(const char *fmt, ...);

/** print help message from msg.ini.
    This function init pause line counter and print text with pauses.
    @param topic    message name
    @param color    color to print, can be 0 for default value
    @return 1 on success */
int       _std cmd_shellhelp(const char *topic, u8t color);

/** get message from msg.ini.
    Function process all msg.ini redirections and return actual message.
    @param topic    message name
    @return message (need to be free() -ed) or 0 */
char*     _std cmd_shellgetmsg(const char *topic);

/** get list of topics in msg.ini.
    @param [out] values   list of text messages for returning topics (can be 0)
    @return list with all message names, exists in msg.ini */
str_list* _std cmd_shellmsgall(str_list**values);

/** direct shell command call.
    @param func     shell command function
    @param argsa    arguments, can be 0
    @param argsb    arguments, can be 0
    @return result */
u32t      _std cmd_shellcall(cmd_eproc func, const char *argsa, str_list *argsb);

/** print common error message.
    @param errorcode   common error code
    @param prefix      message prefix */
void      _std cmd_shellerr(int errorcode, const char *prefix);

/** add MODE command device handler.
    Note, what if no device handler present at the time of MODE command
    execution, command will search MODE_DEV_HANDLER environment variable 
    (DEV - device name), where module name must be specified and try to
    load this module permanently. 
    Typically, this function must be called from such module at the moment
    of it`s loading.

    @param name     device name
    @param proc     command processor
    @return previous installed command or 0. On error proc value returned. You
    do not need to restore previous command when remove own. */
cmd_eproc _std cmd_modeadd(const char *name, cmd_eproc proc);

/** remove previously installed MODE command device handler.
    @param name     device name
    @param proc     command processor, can be 0 to query current active proc.
    @return 0 or command proc, which became active for this device. */
cmd_eproc _std cmd_modermv(const char *name, cmd_eproc proc);

/// query list of all supported devices in MODE command
str_list* _std cmd_modeqall(void);

//===================================================================
//  shell commands, can be called directly (shell)
//-------------------------------------------------------------------

/** COPY command.
    @param cmd      "COPY" string
    @param args     arguments.
    @return 0, errorlevel or CMDR_RETERROR, CMDR_RETEND. See cmd_eproc. */
u32t      _std shl_copy   (const char *cmd, str_list *args);

/// TYPE command.
u32t      _std shl_type   (const char *cmd, str_list *args);

/// DIR command.
u32t      _std shl_dir    (const char *cmd, str_list *args);

/// HELP command.
u32t      _std shl_help   (const char *cmd, str_list *args);

/// RESTART command.
u32t      _std shl_restart(const char *cmd, str_list *args);

/// MKDIR command.
u32t      _std shl_mkdir  (const char *cmd, str_list *args);

/// CHDIR command.
u32t      _std shl_chdir  (const char *cmd, str_list *args);

/// MEM command.
u32t      _std shl_mem    (const char *cmd, str_list *args);

/// DEL command.
u32t      _std shl_del    (const char *cmd, str_list *args);

/// BEEP command.
u32t      _std shl_beep   (const char *cmd, str_list *args);

/// RMDIR command.
u32t      _std shl_rmdir  (const char *cmd, str_list *args);

/// UNZIP command.
u32t      _std shl_unzip  (const char *cmd, str_list *args);

/// LM command.
u32t      _std shl_loadmod(const char *cmd, str_list *args);

/// MOUNT command.
u32t      _std shl_mount  (const char *cmd, str_list *args);

/// UMOUNT command.
u32t      _std shl_umount (const char *cmd, str_list *args);

/// POWER command.
u32t      _std shl_power  (const char *cmd, str_list *args);

/// DMGR command.
u32t      _std shl_dmgr   (const char *cmd, str_list *args);

/// LABEL command.
u32t      _std shl_label  (const char *cmd, str_list *args);

/// TRACE command.
u32t      _std shl_trace  (const char *cmd, str_list *args);

/// REN command.
u32t      _std shl_ren    (const char *cmd, str_list *args);

/// MTRR command.
u32t      _std shl_mtrr   (const char *cmd, str_list *args);

/// PCI command.
u32t      _std shl_pci    (const char *cmd, str_list *args);

/// DATE command.
u32t      _std shl_date   (const char *cmd, str_list *args);

/// TIME command.
u32t      _std shl_time   (const char *cmd, str_list *args);

/// FORMAT command.
u32t      _std shl_format (const char *cmd, str_list *args);

/// LVM command.
u32t      _std shl_lvm    (const char *cmd, str_list *args);

/// MSGBOX command.
u32t      _std shl_msgbox (const char *cmd, str_list *args);

/// CACHE command.
u32t      _std shl_cache  (const char *cmd, str_list *args);

/// MKSHOT command.
u32t      _std shl_mkshot (const char *cmd, str_list *args);

/// MODE command.
u32t      _std shl_mode   (const char *cmd, str_list *args);

/// RAMDISK command.
u32t      _std shl_ramdisk(const char *cmd, str_list *args);

/// LOG command.
u32t      _std shl_log    (const char *cmd, str_list *args);

/// ATTRIB command.
u32t      _std shl_attrib (const char *cmd, str_list *args);

/// GPT command.
u32t      _std shl_gpt    (const char *cmd, str_list *args);

/// PUSHD command.
u32t      _std shl_pushd  (const char *cmd, str_list *args);

/// POPD command.
u32t      _std shl_popd   (const char *cmd, str_list *args);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_SHELL