//
// QSINIT
// module/process functions
//
#ifndef QSINIT_MODULE
#define QSINIT_MODULE

#include "qstypes.h"
#include "qserr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** load module.
    @param       path    Full path to module (else 1:\\ assumed)
    @param       flags   Must be 0.
    @param [out] error   Error code, can be 0
    @param       extdta  Must be 0.
    @return module handle or 0 */
u32t  _std mod_load(char *path, u32t flags, qserr *error, void *extdta);

/** exec module.
    low level exec function.

    @param module Module handle.
    @param env    Environment data.
    @param params Arguments string.
    @return -1 on error or program exit code.
    @see cmd_exec() */
s32t  _std mod_exec(u32t module, const char *env, const char *params);

/** search and load module.
    Function searches in LIBPATH and PATH, DLL extension assumed, EXE if
    no DLL was found.
    @param name   Module name.
    @return module handle or 0 */
u32t  _std mod_searchload(const char *name, u32t *error);

/// @name mod_query flags
//@{
#define MODQ_ALL             (  0)  ///< all modules (loaded & loading)
#define MODQ_LOADED          (  1)  ///< loaded modules only
#define MODQ_LOADING         (  2)  ///< loading modules only
#define MODQ_NOINCR          (  4)  ///< do not increment usage counter
#define MODQ_LENSHIFT        (  8)  ///< optional name length pos
#define MODQ_LENMASK      (0xFF00)  ///< optional name length mask
#define MODQ_POSSHIFT        ( 16)  ///< search position shift (for N-s module)
#define MODQ_POSMASK    (0xFF0000)  ///< search position mask
//@}

/** query loaded module.
   By default, this function increment module usage counter. To enum multiple
   loaded modules with the same name index can be supplied in flags parameter
   (index << MODQ_POSSHIFT & MODQ_POSMASK).
   String without zero at the end can be used as "name" in we same way:
   length << MODQ_LENSHIFT & MODQ_LENMASK.

   @param name   Internal module name (not file!)
   @param flags  See MODQ_*
   @return module handle or 0 */
u32t  _std mod_query(const char *name, u32t flags);

/// name of system module "QSINIT"
#define MODNAME_QSINIT   "QSINIT"
/// name of system module "START"
#define MODNAME_START    "START"
/// name of disk cache system module
#define MODNAME_CACHE    "CACHE"
/// name of disk management system module
#define MODNAME_DMGR     "PARTMGR"
/// name of console management system module
#define MODNAME_CONSOLE  "CONSOLE"
/// name of codepage management module
#define MODNAME_CPLIB    "CPLIB"

/** get pointer to module function (by index).
    @param  module  Module handle.
    @param  index   Entry index.
    @return function pointer or 0 */
void *_std mod_getfuncptr(u32t module, u32t index);

/** free and unload module.
    Function will decrement usage counter until one, then tries to unload
    module. If module is current process, system module or DLL term function
    returned 0 - mod_free() returns error (module will stay in place and
    fully functional).
    @param  module            Module handle.
    @retval E_MOD_HANDLE      Bad module handle
    @retval E_MOD_FSELF       Trying to free self
    @retval E_MOD_FSYSTEM     Trying to free system module
    @retval E_MOD_LIBTERM     DLL term function denied unloading
    @retval E_MOD_EXECINPROC  Module is exe and mod_exec() in progress
    @retval E_OK              on success */
u32t  _std mod_free(u32t module);

// internal structures ---------------------------------------------------
#ifdef MODULE_INTERNAL
#include "qslxfmt.h"
#include "qsutil.h"

#define MOD_SIGN      0x484D5351   // QSMH - module struct check signature
#define MAX_IMPMOD    32           // max. number of imported modules in LE/LX
#define MAX_EXPSTART  512          // max. number of exports in "start" module

typedef struct {
   void           *address;
   u32t               size;
   u32t              flags;
   u32t            orgbase;
   u16t                sel;    ///< selector or 0
   u16t               resw;
   u32t            fixaddr;    ///< used only with MODUNP_ALT_FIXADDR (below)
   u32t              resd1;
   u32t              resd2;
} mod_object;

#define EXPORT_THUNK   (16)    ///< export thunks size

typedef struct {
   u32t            address;    ///< address for caller: 16 byte buffer with thunk
   u32t             direct;    ///< actual address in module
   u32t            ordinal;    ///< ordinal
   u32t               is16;    ///< export is 16 bit
   u16t                sel;    ///< selector field for address
   u16t            forward;    ///< unresolved forwarder entry (mod ord in address)
} mod_export;

/// loaded module internal ref
typedef struct _module {
   u32t               sign;
   u32t              flags;    ///< next 2 fields accessed by offset from asm launcher
   u32t          start_ptr;    ///< 16/32 bit module start pointer
   u32t          stack_ptr;    ///< 16/32 bit module start pointer

   u32t              usage;    ///< usage counter
   u32t            objects;    ///< number of objects

   void          *baseaddr;    ///< base address
   char          *mod_path;    ///< module full path

   struct _module    *prev,
                     *next;    ///< module list
   struct _module  *impmod[MAX_IMPMOD+1]; ///< zero-term. list of used modules
   mod_export        *exps;    ///< exported functions
   u32t            exports;    ///< number of exports
   u8t             *thunks;    ///< array for thunks, allocated by exports build code

   char name[E32MODNAME+1];    ///< 127 bytes (LX format limit)
   mod_object       obj[1];
} module;

/** process context data. */
typedef struct _process_context {
   u32t               size;   ///< size of this struct (must be at 0 pos)
   u32t                pid;   ///< process id
   char            *envptr;   ///< env. ptr (can be updated)
   char           *cmdline;   ///< command line (as module receive in start function)
   module            *self;   ///< current module
   module          *parent;   ///< parent module
   u32t              flags;   ///< internal flags
   struct
   _process_context  *pctx;   ///< parent process context

   u32t          rtbuf[16];   ///< data buffer for runtime data
   u32t        userbuf[16];   ///< data buffer for user data
} process_context;

/// @name process_context.rtbuf indexes (internal)
//@{
#define RTBUF_TMPNAME          0         ///< tmpname next used index
#define RTBUF_STDIN            1         ///< stdio file handle
#define RTBUF_STDOUT           2         ///< stdout file handle
#define RTBUF_STDERR           3         ///< stderr file handle
#define RTBUF_STDAUX           4         ///< stdaux file handle
#define RTBUF_STCLOCK          5         ///< process start tm_counter()
#define RTBUF_PUSHDST          6         ///< PUSHD shell command stack
#define RTBUF_ANSIBUF          7         ///< ANSI command state buffer
#define RTBUF_PROCDAT          8         ///< process internal data
//@}

#define PCTX_BIGMEM            0x0001    ///< envptr was hlp_memalloc-ed
#define PCTX_ENVCHANGED        0x0002    ///< envptr was changed by runtime

#ifndef QSMEMOWNER_MODLDR
#define QSMEMOWNER_MODLDR      0x4243444D
#define QSMEMOWNER_COPROCESS   0x42434F50
#define QSMEMOWNER_COTHREAD    0xFFFFD000
#endif

/** return current executed module process context */
process_context* _std mod_context(void);

/// @name module.flags values
//@{
#define MOD_LIBRARY            0x000004  ///< module is dll
#define MOD_LOADING            0x000008  ///< module is in loading list
#define MOD_INITDONE           0x000010  ///< dll init/term done flags
#define MOD_TERMDONE           0x000020  ///<
#define MOD_IMPDONE            0x000040  ///< all imported modules was unloaded
#define MOD_LOADER             0x000080  ///< this module initiate loading process
#define MOD_NOFIXUPS           0x000100  ///< fixups pre-applied in module
#define MOD_EXECPROC           0x000200  ///< mod_exec() in process (for exe module)
#define MOD_SYSTEM             0x000400  ///< "system" module (QSINIT & START only)
//@}

/// external functions located in "start" module
typedef struct {
   /// number of entries in this table
   u32t    entries;
   /// build export table
   int     _std (*mod_buildexps)(module *mh, lx_exe_t *eh);
   /// search and loading imported module
   u32t    _std (*mod_searchload)(const char *name, u32t *error);
   /// unpack ITERDATA1
   u32t    _std (*mod_unpack1)(u32t DataLen, u8t* Src, u8t *Dst);
   /// unpack ITERDATA2
   u32t    _std (*mod_unpack2)(u32t DataLen, u8t* Src, u8t *Dst);
   /// unpack ITERDATA3 (OS/4)
   u32t    _std (*mod_unpack3)(u32t DataLen, u8t* Src, u8t *Dst);
   /// module unloading callback (free export table)
   void    _std (*mod_freeexps)(module *mh);
   /// heap alloc
   void*   _std (*mem_alloc)(long Owner, long Pool, u32t Size);
   /// heap realloc
   void*   _std (*mem_realloc)(void*, u32t);
   /// heap free
   void    _std (*mem_free)(void*);
   /// read file from virtual disk
   void*   _std (*freadfull)(const char *name, unsigned long *bufsize);
   /** push string to log.
       @param flags   log flags
       @param msg     message to save
       @param time    time label, can be 0 to use current */
   int     _std (*log_push)(int flags, const char *msg, u32t time);
   /// save data to storage
   void    _std (*sto_save)(const char *entry, void *data, u32t len, int copy);
   /// flush data to storage after rm call
   void    _std (*sto_flush)(void);
   /// delayed exe unzip
   int     _std (*unzip_ldi)(void *mem, u32t size, const char *path);
   /** start process callback.
       called when module is loaded, can return non-zero to deny exec */
   int     _std (*start_cb)(process_context *pq);
   /** exit process callback.
       called when module was exited, can change result code */
   s32t    _std (*exit_cb)(process_context *pq, s32t rc);
   /// hlp_memprint() call, moved to start due lack of space
   void    _std (*memprint)(void *,void *,u8t *,u32t *,u32t);
   /** memcpy with catched exceptions.
       @return 0 if exception occured. */
   u32t    _std (*memcpysafe)(void *dst, const void *src, u32t length, int page0);
   /** module loaded callback.
       Called before returning ok to user. But module is not guaranteed to
       be in loaded list at this time, it can be the one of loading imports
       for someone. */
   void    _std (*mod_loaded)(module *mh);
   /** throw exception (QSINIT trap screen).
       Exception, also, can be catched in caller */
   void    _std (*sys_throw)(u32t num, const char* file, u32t line);
   /// hlp_volinfo moved to START, but used by exit_restart()
   u32t    _std (*hlp_volinfo)(u8t drive, disk_volume_data *info);
   /// just _fullpath
   qserr   _std (*fullpath)(char *buffer, const char *path, u32t size);
   /// get current directory of active process (string is malloc-ed)
   char*   _std (*getcurdir)(void);
   /// delete file
   qserr   _std (*unlink)(const char *path);
   /// batch free
   u32t    _std (*mem_freepool)(long Owner, long Pool);
} mod_addfunc;

/// extreq parameter for mod_unpackobj()
typedef struct {
   /// flags. See modobj_extreq_flags
   u32t    flags;
   /// find export callback (instead of default search)
   mod_export* _std (*findexport)(module *mh, u16t ordinal);
   /// apply fixup callback (return 0 to skip fixup)
   u32t _std (*applyfixup)(module *mh, void *addr, u8t type, u16t sel, u32t offset);
   /// object loaded callback (before starting to apply fixups)
   void _std (*objloaded)(module *mh, u32t object);
} modobj_extreq;

/// @name modobj_extreq flags
//@{
#define MODUNP_ALT_FIXADDR     0x0001    ///< use mod_object.fixaddr for fixups
#define MODUNP_FINDEXPORT      0x0002    ///< call *findexport instead of std search
#define MODUNP_FINDEXPORTFX    0x0004    ///< call *applyfixup for imported functions
#define MODUNP_APPLYFIXUP      0x0008    ///< call *applyfixup for all fixups
#define MODUNP_OBJLOADED       0x0010    ///< call *objloaded
//@}

/** load and unpack objects
    e32_datapage must be changed to offset from LX header, not begin of file
    extreq can/must be NULL ;)
    warning: executable data must have additional dword at the end (iterdata
    unpack code can temporary swap dword behind end of source data to zero)

    @param mh       Module handle
    @param eh       LE/LX exe header
    @param ot       Pointer to LE/LX object table
    @param object   Object number (zero-based!)
    @param destaddr Address to place module.
    @param extreq   Additional options for loading kernel and other strange things
    @see modobj_extreq
*/
int  _std mod_unpackobj(module *mh, lx_exe_t *eh, lx_obj_t *ot, u32t object,
                  u32t destaddr, void *extreq);

/// link module to list
void _std mod_listadd(module **list, module *module);

/// unlink module from list
void _std mod_listdel(module **list, module *module);

/// merge two module lists
void _std mod_listlink(module **to, module *first);

/// apply flags to all modules in list ( | and &~ )
void _std mod_listflags(module *first, u32t fl_or, u32t fl_andnot);

/** query module by eip.
    @attention warning! function located in "start" module

    @param [in]  eip     EIP value
    @param [out] object  Module`s object number (zero-based)
    @param [out] offset  Offset in object
    @param [in]  cs      Selector (optional, use 0 for FLAT space)
    @result Module handle or 0 */
module* _std mod_by_eip(u32t eip, u32t *object, u32t *offset, u16t cs);

#endif // MODULE_INTERNAL
#ifdef __cplusplus
}
#endif
#endif // QSINIT_MODULE
