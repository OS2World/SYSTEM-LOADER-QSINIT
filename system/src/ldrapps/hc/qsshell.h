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
//  string lists
//-------------------------------------------------------------------

/** string list.
    Pure C API simple string list. Any str_list, allocated by system
    calls - placed in one heap block, so to free it - just call to free() */
typedef struct {
   u32t   count;
   char*  item[1];
} str_list;

/** split string to items.
    Note, what function trims spaces from both sides of all returning items.
    I.e. "key = value" strings will be always returned as "key=value".
    @param str          source string
    @param separators   string with separator characters (ex. ",-\n\t")
    @return string list, must be freed by single free() call */
str_list* _std str_split    (const char *str, const char *separators);

/** split text file to items.
    @param text         source text file witn \n or \r\n EOLs
    @param len          length of "text", can be 0 for null-term. string
    @return string list, must be freed by single free() call */
str_list* _std str_settext  (const char *text, u32t len);

/// create list from array of char*
str_list* _std str_fromptr  (char **list, int size);

/// split string by command line parsing rules (with "" support)
str_list* _std str_splitargs(const char *str);

/// copy a number of strings into new list
str_list* _std str_copylines(str_list*list, u32t first, u32t last);

/// merge list to the command line (wrap "" around lines with spaces)
char*     _std str_mergeargs(str_list*list);

/** delete entry from the list.
    Function just dec list->count value and move list->item array content,
    no any updates in memory & pointers. If index is above list size, function
    do nothing */
void      _std str_delentry (str_list*list, u32t index);

/** insert one or more entries to the list.
    Note, that function is *really* slow, it rebuilds entire list, calc each
    string length and copy one by one. Use it only when no other way.

    @param list          source string list
    @param pos           position where to insert (use FFFF for the end)
    @param str           one or more strings, splitted by a "delimeter", if
                         delimeter value is zero, then then the end of list
                         should be marked as \0\0. Can be 0 to simple duplicate
                         the list.
    @param delimeter     string delimeter
    @param free          free original list
    @return a copy of source list with inserted entries in the application
            owned heap block */
str_list* _std str_newentry (str_list*list, u32t pos, const char *str,
                             char delimeter, int free);

/// create a copy of list
#define str_duplist(x) str_newentry(x,0,0,0,0)

/** read current environment to a string list.
    See also env_create() for reverse operation.
    @return string list in the process owned heap block. */
str_list* _std str_getenv   (void);

/** get a subset of current process arguments.
    @return string list, in application owned heap block. List returned even
            on incorrect range (with list->count==0) */
str_list* _std str_getargs  (u32t first, u32t last);

/// get text to single string (must be free() -ed).
char*     _std str_gettostr (const str_list*list, char *separator);

/** search for key value in string list.
    Function searches for key=value string, starting from *pos or 0 if pos=0.
    Spaces around of '=' is NOT allowed, but all str_ functions always return
    list without such spaces (even if it present in source INI file, for
    example).
    Note, that string without '=' assumed as pure key. I.e. if it matched,
    pointer to trailing '\0' will be returned as well.

    @param list          source string list
    @param key           key name to search
    @param [in,out] pos  start position on enter, found on exit. Can be 0
    @return pointer to first character of "value" in found string or 0 */
char*     _std str_findkey  (str_list *list, const char *key, u32t *pos);

/** search for a value in the string list.
    @param list          source string list
    @param str           string to search
    @param pos           list position to start from
    @param icase         case sensitive search (0) or insensitive (1)
    @return index in the list or -1 if not found */
int       _std str_findentry(str_list *list, const char *str, u32t pos, int icase);

/** parse argument list and set flags from it.
    Example of call:
    @code
      static char *argstr   = "+r|-r|+s|-s|+h|-h";
      static short argval[] = { 1,-1, 1,-1, 1,-1};
      str_parseargs(al, 0, 0, argstr, argval, &a_R, &a_R, &a_S, &a_S, &a_H, &a_H);
    @endcode
    @param lst      list with arguments
    @param firstarg start position of first argument in the list
    @param flags    options (SPA_* below)
    @param args     arguments in form: "/boot|/q|/a" or "+h|-h|+a|-a"
    @param values   array of SHORT values for EVERY argument in args
    @param ...      pointers to INT variables to setup
    @return 0 if ret_list==0 else NEW list with removed known arguments,
            this list must be released by free() call. Note, that list
            returned even with str_list.count=0. */
str_list* _std str_parseargs(str_list *lst, u32t firstarg, u32t flags,
                             char* args, short *values, ...);

/// @name str_parseargs() flags
//@{
#define SPA_RETLIST       0x00000001   ///< return updated arg list (newly allocated!)
#define SPA_NOSLASH       0x00000002   ///< all keys in args are without '/'
#define SPA_NODASH        0x00000004   ///< do not accept '-' instead of '/'
//@}


/// print str_list to log
void      _std log_printlist(char *info, str_list*list);

/** split key = value string to the key and the value.
    Function searches for '=' then trim all space characters (' ', '\t') around
    key and value and returns a pointer to the value. If no '=' in the string -
    pointer to the terminating zero returned.

    A *copy* of the key in the application owned heap block is saved in "key",
    if it is non-zero. This block should be released by user.

    @return pointer to value (in the source string!). */
char*     _std str_keyvalue(const char *str, char **key);

/** calculate string length without embedded ANSI sequences.
    @param str      source string
    @return string length without ANSI sequences in it */
u32t      _std str_length   (const char *str);

//===================================================================
//  environment (shell)
//-------------------------------------------------------------------

/** check environment variable and return integer value.
    @param name     name of env. string
    @return 1 on TRUE/ON/YES, 0 on FALSE/OFF/NO, -1 if no string,
    else integer value of env. string */
int       _std env_istrue(const char *name);

/// set environment variable (integer) value
void      _std env_setvar(const char *name, int value);

/** get environment variable.
    Unlike getenv() - function is safe in MT mode.
    @param name     name of env. string 
    @param shint    flag 1 to subst shell "internal" variables like DBPORT,
                    SAFEMODE and so on (see "if /?" for the full list)
    @return string in heap block (must be free()-ed) or 0. */
char*     _std env_getvar(const char *name, int shint);

//===================================================================
//  execution (shell)
//-------------------------------------------------------------------

/** single call batch file execution.
    @param  file    File name.
    @param  args    Arguments (including batch name, for %0 %1, etc), optional.
    @return result, CMDR_NOFILE if no file */
u32t      _std cmd_exec(const char *file, const char *args);

/** exec [exec_*] section from extcmd.ini.
    @param  section Section name suffix
    @param  args    Arguments (including batch name, for %0 %1, etc), optional.
    @return result, CMDR_NOFILE if no section */
u32t      _std cmd_execint(const char *section, const char *args);

typedef void *cmd_state;

/** init batch file process (cmds - file data)
    @param  cmds    Single command or batch file data.
    @param  args    Command line (including batch name, for %0 %1, etc), optional.
    @return handle for cmd_run(), cmd_close(). */
cmd_state _std cmd_init (const char *cmds, const char *args);

/** init batch file process (alternative way)
    @param  cmds    List of batch file strings.
    @param  args    Arguments (%0,%1,%2,etc), can be 0.
    @return handle for cmd_run(), cmd_close(). */
cmd_state _std cmd_initbatch(const str_list* cmds, const str_list* args);

#define CMDR_ONESTEP    0x0001         ///< run one step
#define CMDR_ECHOOFF    0x0002         ///< echo is off by default
#define CMDR_NESTCALL   0x0004         ///< internal, do not use
#define CMDR_DETACH     0x0008         ///< any process launch must be detached

#define CMDR_RETERROR   0xFFFFFFFF     ///< exit code: internal error
#define CMDR_RETEND     0xFFFFFFFE     ///< exit code: execution finished
#define CMDR_NOFILE     0xFFFFFFFD     ///< exit code: missing file (in cmd_exec())

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

/// @name cmd_argtype() result
//@{
#define ARGT_FILE         0x0001       ///< any file except batch
#define ARGT_BATCHFILE    0x0002       ///< .cmd or .bat file
#define ARGT_DISKSTR      0x0003       ///< A: B: 1:
#define ARGT_SHELLCMD     0x0004       ///< shell command
#define ARGT_UNKNOWN      0x0005       ///< failed to determine string type
//@}

/** query type of argument.
    @param [in,out] arg   file or command name, in buffer of QS_MAXPATH length,
                          if result is ARGT_FILE or ARGT_BATCHFILE - file name
                          in arg will be expanded to full path!
    @return argument type (ARGT_*). */
u32t      _std cmd_argtype  (char *arg);

/// query list of all supported commands
str_list* _std cmd_shellqall(int ext_only);

/// help text default color (vio.h required for VIO_COLOR_LWHITE)
#define CLR_HELP  VIO_COLOR_LWHITE

#define  PRNSEQ_NOPAUSE     (-1)       ///< print without "pause" check
#define  PRNSEQ_INIT        (1)        ///< init line counter
#define  PRNSEQ_LOGCOPY     (2)        ///< copy to log until next init
#define  PRNSEQ_INITNOPAUSE (5)        ///< init & no pause until next init

/** print sequence of strings with "pause" support.
    @param fmt      string to print, without finishing \\n, but
                    can be multitined.
    @param flags    init - PRNSEQ_*, 0 - print with pause, -1 - no pause
    @param color    line color, 0 - default
    @return 1 if ESC was pressed in pause. */
u32t   __cdecl cmd_printseq(const char *fmt, int flags, u8t color, ...);

#define  PRNTXT_HELPMODE    (8)        ///< special help text split (with soft-cr)

/** print long text to the screen.
    "^" acts as EOL in the text.
    @param text     text, need to be splitted to lines gracefully
    @param flags    init - PRNSEQ_* + PRNTXT_*, 0 - print with pause,
                    -1 - no pause
    @param color    color to print, can be 0 for default value
    @return  1 if ESC was pressed in pause. */
u32t      _std cmd_printtext(const char *text, int flags, u8t color);

#define  PRNFIL_SPLITTEXT  (16)        ///< split text via cmd_printtext()

/** print file contents to the screen.
    @param handle   file handle, use _os_handle() for FILE* source
    @param len      data length, from current file position, negative value
                    mean "until the end of file". Position is changed on exit.
    @param flags    PRNSEQ_* + PRNTXT_* + PRNFIL_* flags
    @param color    line color, 0 - default
    @return 0 on success, E_SYS_UBREAK if ESC was pressed in pause, else
            error code */
qserr     _std cmd_printfile(qshandle handle, long len, int flags, u8t color);

/** printf with "pause".
    Call to cmd_printseq() to init line counter and log copying state.
    @param fmt      string to print
    @return number of printer characters */
int    __cdecl cmd_printf(const char *fmt, ...);

/** split long text to a lines of defined width.
    All Tabs assumed as 4 spaces.
    ^ symbol means eol, as well as char codes 10 and 13.
    @param text     text to split
    @param width    maximum line width
    @param flags    options (SplitText_* bits below)
    @return string list in the application owned heap block */
str_list* _std cmd_splittext(const char *text, u32t width, u32t flags);

#define SplitText_NoAnsi   1           ///< disable ansi line length calculation
#define SplitText_HelpMode 2           ///< help printing, with & as soft-cr

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

/** set/replace message in msg.ini.
    @param topic    message name
    @param topic    mesage text */
void      _std cmd_shellsetmsg(const char *topic, const char *text);

/** get list of topics in msg.ini.
    @param [out] values   list of text messages for returning topics (can be 0)
    @return list with all message names, exists in msg.ini */
str_list* _std cmd_shellmsgall(str_list**values);

/** direct shell command call.
    Arguments united in orger argsa+argsb. Func can be shell common command or
    "mode" subcommand.
    @param func     shell command function
    @param argsa    arguments, can be 0
    @param argsb    arguments, can be 0
    @return result */
u32t      _std cmd_shellcall(cmd_eproc func, const char *argsa, str_list *argsb);

/** print common error message.
    @param errtype     error type (EMSG_* constant)
    @param errorcode   common error code
    @param prefix      message prefix */
void      _std cmd_shellerr(u32t errtype, int errorcode, const char *prefix);

#define EMSG_QS      0x0000            ///< QSINIT error codes (qserr.h)
#define EMSG_CLIB    0x0001            ///< clib error codes (errno.h)

/** get common error message.
    @param errtype     error type (EMSG_* constant)
    @param errorcode   common error code
    @return message (need to be free() -ed) or 0 */
char*     _std cmd_shellerrmsg(u32t errtype, int errorcode);

/** add MODE command device handler.
    Note, that if no device handler present at the time of MODE command
    execution, command will search MODE_DEV_HANDLER environment variable
    (DEV - device name), where module name must be specified and try to
    load this module permanently.
    If no such variable - extcmd.ini will be asked for module name.

    Typically, this function must be called from such module at the moment
    of its loading.

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

/** get current active proc for the MODE device.
    If no device handler loaded then function searches for the handler as
    described in cmd_modeadd() and returns zero only if command handler for
    this device is not found.

    @param name     device name
    @return 0 if device handler not found or command proc */
cmd_eproc _std cmd_modeget(const char *name);

/// query list of all supported devices in MODE command
str_list* _std cmd_modeqall(void);

/** get shell history list.
    read internal shell (cmd.exe) user commands history list.
    @return string list in the process owned heap block or 0 if list is
            empty. */
str_list* _std cmd_historyread(void);

/// clear shell history list.
qserr     _std cmd_historywipe(void);

//===================================================================
//  shell commands, can be called directly (shell)
//-------------------------------------------------------------------

/** COPY command.
    Use cmd_shellcall() for easy processing.

    @attention Any shell command, executed via shl_* functions will use
               CURRENT process context, i.e. it will have access to any
               files, locked by your process, your current process
               environment and so on ...

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
/// ANSI command.
u32t      _std shl_ansi   (const char *cmd, str_list *args);
/// MOVE command.
u32t      _std shl_move   (const char *cmd, str_list *args);
/// REBOOT command.
u32t      _std shl_reboot (const char *cmd, str_list *args);
/// CHCP command.
u32t      _std shl_chcp   (const char *cmd, str_list *args);
/// DELAY command.
u32t      _std shl_delay  (const char *cmd, str_list *args);
/// DETACH command.
u32t      _std shl_detach (const char *cmd, str_list *args);
/// START command.
u32t      _std shl_start  (const char *cmd, str_list *args);
/// SM command.
u32t      _std shl_sm     (const char *cmd, str_list *args);
/// STOP command.
u32t      _std shl_stop   (const char *cmd, str_list *args);
/// PS command.
u32t      _std shl_ps     (const char *cmd, str_list *args);
/// VER command.
u32t      _std shl_ver    (const char *cmd, str_list *args);
/// MD5 command.
u32t      _std shl_md5    (const char *cmd, str_list *args);
/// AT command.
u32t      _std shl_at     (const char *cmd, str_list *args);
/// SETINI command.
u32t      _std shl_setini (const char *cmd, str_list *args);
/// SETKEY command.
u32t      _std shl_setkey (const char *cmd, str_list *args);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_SHELL
