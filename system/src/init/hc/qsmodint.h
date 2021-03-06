//
// QSINIT
// internal module handling & QSINIT binary support
//
#ifndef QSINIT_MOD_INT
#define QSINIT_MOD_INT

#include "qsmod.h"
#include "qsutil.h"
#include "qssys.h"
#include "stddef.h"   // for printf_function type

#ifdef __cplusplus
extern "C" {
#endif

#include "qslxfmt.h"

#define MOD_SIGN      0x484D5351   /// QSMH - module struct check signature
#define MAX_IMPMOD            32   /// maximum number of imported modules
#define MAX_EXPSTART        1024   /// maximum number of exports in START
#define MAX_NAMELEN          127   /// import/export name limit

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
   char              *name;    ///< name
} mod_export;

/// loaded module internal ref
typedef struct _module {
   u32t               sign;
   u32t              flags;    ///< next 2 fields should be bounded to make asm code happy
   u32t          start_ptr;    ///< module start pointer
   u32t          stack_ptr;    ///< module stack pointer

   u32t              usage;    ///< usage counter
   u32t            objects;    ///< number of objects

   void          *baseaddr;    ///< base address
   char          *mod_path;    ///< module full path

   char          *name_buf;    ///< mod_getname() string (after 1st fn call only)
   struct _module    *prev,
                     *next;    ///< module list
   struct _module  *impmod[MAX_IMPMOD+1]; ///< zero-term. list of used modules
   mod_export        *exps;    ///< exported functions
   u32t            exports;    ///< number of exports
   u8t             *thunks;    ///< array for thunks, allocated by exports build code
   void              *rtab,    ///< name tables (internal)
                    *nrtab;

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
   qshandle     *inherited;   ///< zero-term list of inherited handles (0 if no one)

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
#define RTBUF_STDNUL           9         ///< nul file handle
#define RTBUF_ENVMUX          10         ///< mutex for clib env. funcs
#define RTBUF_ENVORG          11         ///< original environment data
#define RTBUF_RTFINI          12         ///< exit() support (internal)
#define RTBUF_RTEXIT          13         ///< exit() support (internal)
#define RTBUF_ARGV            14         ///< argv array
#define RTBUF_ARGC            15         ///< argc value
//@}

#define PCTX_ENVCHANGED        0x0002    ///< envptr was changed by runtime

/** return current executed module process context */
process_context* _std mod_context(void);

/** stack, as it filled by launch32() code in QSINIT.
    Assumed by MTLIB & mod_chain() code. */
typedef struct {
   void      *retaddr;
   module     *module;
   u32t      reserved;
   char       *envptr;
   char      *cmdline;
   u32t     startaddr;
   u32t    parent_esp;
} launch32_stack;

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

/** @name mod_load() flags.
    Some of them defined in mdt.c only, other here, but for internal use too. */
//@{
/// delete module file from disk after success load
#define LDM_UNLINKFILE           0x0001
/** load module from memory location.
    Path arg of mod_load() points to memory block and extdta to dword
    with file size. Block must be the global memory block, it will be released
    by mod_load(). */
#define LDM_MEMORY               0x0004
//@}

/// external functions located in "start" module
typedef struct {
   /// number of entries in this table
   u32t           entries;
   /// must be the second entry! (asm code uses it by direct offset)
   int        dbport_lock;
   /// build export table
   int     _std (*mod_buildexps)(module *mh, lx_exe_t *eh);
   /// search and loading imported module
   u32t    _std (*mod_searchload)(const char *name, u32t flags, qserr *error);
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
   /** start process callback. */
   void    _std (*start_cb)(void);
   /** exit process callback.
       called when module was exited, can change result code */
   s32t    _std (*exit_cb)(process_context *pq, s32t rc);
   /// hlp_memprint() call, moved to start due lack of space
   void    _std (*memprint)(printf_function,void*,void*,u8t*,u32t*,u32t,process_context**);
   /** memcpy with catched exceptions.
       see hlp_memcpy().
       @return 0 if exception occured. */
   u32t    _std (*memcpysafe)(void *dst, const void *src, u32t length, u32t flags);
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
   /// notification on system events
   void    _std (*sys_notifyexec)(u32t eventtype, u32t infovalue);
   /// register notification
   u32t    _std (*sys_notifyevent)(u32t eventmask, sys_eventcb cbfunc);
   /// QSINIT in MT mode (set by START module)
   int          in_mtmode;
   /// mutex for module loader (valid in MT mode only)
   qshandle     ldr_mutex;
   /// mutex for micro-FSD access (valid in MT mode only)
   qshandle     mfs_mutex;
   /// mutex capture
   qserr   _std (*muxcapture)(qshandle handle);
   /// mutex wait
   qserr   _std (*muxwait)(qshandle handle, u32t timeout_ms);
   /// mutex release
   qserr   _std (*muxrelease)(qshandle mtx);
   /// make a copy in heap block, block owner is START
   char*   _std (*envcopy)(process_context *pq, u32t addspace);
   /** callback - is it possible to write.
       Can be 0!
       @return modified count value */
   u32t    _std (*dsk_canwrite)(u32t disk, u64t sector, u32t count);
   /** callback - is it possible to read.
       Can be 0!
       @return modified count value */
   u32t    _std (*dsk_canread) (u32t disk, u64t sector, u32t count);
   // FP to str convertion (for printf)
   int     _std (*fptostr)(double value, char *buf, int int_fmt, int prec);
   /** save/change FPU state before exec.
       Function used in non-MT mode only, in parent process context */
   void    _std (*fp_save)(process_context *newpq);
   /** restore FPU state after exec.
       Function used in non-MT mode only, in child process context */
   void    _std (*fp_rest)(void);
   /// use supplied below MFS functions instead of default ones
   int        mfs_replace;
   /// micro-FSD file open
   u16t    _std (*mfs_open)(const char *name, u32t *filesize);
   /// micro-FSD file read
   u32t __cdecl (*mfs_read)(u32t offset, void *buf, u32t readsize);
   /// micro-FSD file close
   void __cdecl (*mfs_close)(void);

   qserr   _std (*io_open)(const char *name, u32t mode, qshandle *pfh, u32t *action);
   u32t    _std (*io_read)(qshandle fh, const void *buffer, u32t size);
   u32t    _std (*io_write)(qshandle fh, const void *buffer, u32t size);
   u64t    _std (*io_seek)(qshandle fh, s64t offset, u32t origin);
   qserr   _std (*io_size)(qshandle fh, u64t *size);
   qserr   _std (*io_setsize)(qshandle fh, u64t newsize);
   qserr   _std (*io_close)(qshandle fh);
   qserr   _std (*io_lasterror)(qshandle fh);
   /** this call allocates "bit_map" class every time!
       @return FFFF if no bit found */
   u32t    _std (*bitfind)(void *data, u32t size, int on, u32t len, u32t hint);
   void    _std (*setbits)(void *dst, u32t pos, u32t count, u32t flags);
   /// go to system memory manager panic screen
   void    _std (*mempanic)(u32t type, void *addr, u32t info, char here, u32t caller);

   u64t    _std (*tlsget)(u32t index);
   qserr   _std (*tlsaddr)(u32t index, u64t **slotaddr);
   qserr   _std (*tlsset)(u32t index, u64t value);

   u32t      removedentry;
   /// get shell command history (process owned block)
   char*   _std (*shl_history)(void);
   void    _std (*dccommit)(u32t code);
} mod_addfunc;

/// @name mod_addfunc.mempanic type value
//@{
#define MEMHLT_MEMTABLE    0   ///< memtable data damaged
#define MEMHLT_FBSIZEERR   1   ///< free block size error
#define MEMHLT_REALLOCRO   2   ///< trying realloc read-only block
#define MEMHLT_WRONGADDR   3   ///< wrong address (yes, it is fatal!)
#define MEMHLT_FBSIGN      4   ///< free block signature damaged
#define MEMHLT_FBHEADER    5   ///< free block header damaged
//@}

/// extreq parameter for mod_unpackobj()
typedef struct {
   /// flags. See modobj_extreq_flags
   u32t    flags;
   /// find export callback (instead of default search)
   mod_export* _std (*findexport)(module *mh, u16t ordinal, const char *name, u8t nlen);
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
/// set TS flag in cr0
void      sys_settsflag(void);
/// reset TS flag in cr0
void      sys_clrtsflag(void);
#if defined(__WATCOMC__)
#pragma aux sys_settsflag = \
     "pushfd"               \
     "cli"                  \
     "mov     eax,cr0"      \
     "or      eax,8"        \
     "mov     cr0,eax"      \
     "popfd"                \
     modify exact [eax];
#pragma aux sys_clrtsflag = \
     "clts"                 \
     modify exact [];
#endif

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MOD_INT
