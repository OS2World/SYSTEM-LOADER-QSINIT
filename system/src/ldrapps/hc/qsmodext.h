//
// QSINIT API
// additional module/process functions
//

#ifndef QSINIT_MODULE_EXTRA
#define QSINIT_MODULE_EXTRA

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "qsmodint.h"

#ifndef QSINIT_CHAININC
typedef struct mod_chaininfo_s {
   int                     mc_entry;
#ifdef QS_BASE_SELDESC               // include seldesc.inc to get access here
   struct pushad_s
#else
   void
#endif
                           *mc_regs;
#ifdef QSINIT_CHAININC               // include qschain.inc to get access here
   struct ordinal_data
#else
   void
#endif
                        *mc_orddata;
   void*                 mc_replace;
   u32t                 mc_userdata;
} mod_chaininfo;
#endif // QSINIT_CHAININC

/** api chain(hook) proc.
    There are two variants of chaining: on module exports and on common
    functions. Module exports supports chaining without any additional steps.
    For common function chaining you must create calling thunk (by
    mod_buildthunk() call) and use it as function address in any calls.

    Chaining allow interseption on entry and exit of call and replacement of
    calling function to the compatible one. I.e. you can receive stack and
    registers before/after call and type/modify it transparently or, just,
    put your own function instead of original.


    Registers in mc_regs (except esp) can be changed both on entry and exit.
    In entry chain mc_regs.esp points to return address, on exit - to stack
    after function return.

    Field mc_replace contain a copy of replacement function address from
    ordinal_data struct. Entry proc can change it to 0 or compatible function
    address to change processing for this call only.
    Exit proc receive an address of actually called function in this field.

    Field mc_userdata can be used to save data between entry/exit chains. It
    initialized by zero before entry call. Be careful - it shared by all hooks,
    installed for this function!

    Exit chain is a harder choice. It cannot be used if callee changes ebp
    register and it can, theoretically, exhaust internal stack of 128 recursive
    calls if exception occured in callee function and catched in caller.
    Also, do not set it on mod_exec() ;) because it recusive in fact (every
    module execute mod_exec() until child`s exit).
    Stack exhaustion will cause stop exit chain calling for this ordinal and
    produce warning message to log on every call.
    Altering ebp will cause immediate panic (critical data saved in it
    during function call).

    By default entry/exit hook appends to the end of chain list for this
    function. APICN_FIRSTPOS can be added to mod_apichain() "chaintype" arg
    to insert function to the first pos.
    Entry/exit hooks used by tracing. To cooperate with it all exit chains
    must use APICN_FIRSTPOS flag (it is better for trace to be the 1st in
    entry list and last in exit).

    Entry/exit hooks calling occurs in MT locked state (i.e. thread switching
    is blocked). This, also, mean what they should not be too long.

    @param  info    Call info.
    @return 1 to process next chain, 0 to stop chain list processing */
typedef int _std (*mod_chainfunc)(mod_chaininfo *info);

// @name mod_apichain() type
//@{
#define APICN_ONENTRY   0x0001    ///< chain on entry
#define APICN_ONEXIT    0x0002    ///< chain on exit
#define APICN_REPLACE   0x0003    ///< replace function
#define APICN_FIRSTPOS  0x0080    ///< insert entry/exit chain to the start of chain list
//@}

/** install api chain procedure.

    Intercepted call will be performed in this way:
    * call all entry functions (if exists) until end of list or 0 returned by
      one of them
    * call replacement function (if specified) or original function
    * call all exit functions (if exists) until end of list or 0 returned by
      one of them

    Note, what if multiple replacement handlers was installed for function, the
    last installed will be in use.

    @param  module      Module handle
    @param  ordinal     Function ordinal
    @param  chaintype   Type of chaining
    @param  handler     Function to install, this is mod_chainfunc for entry
                        and exit and replaced function type for replace
    @return success flag (1/0) */
int   _std mod_apichain  (u32t module, u32t ordinal, u32t chaintype, void *handler);

/** remove api chain procedure.
    @param  module      Module handle
    @param  ordinal     Function ordinal (can be 0 for all ordinals, but such
                        arg will be denied for QSINIT and START modules).
    @param  chaintype   Type of chaining, can be 0 for all chaining types.
    @param  handler     Function to remove, can be 0 for all functions of
                        this type
    @return number of removed functions */
u32t  _std mod_apiunchain(u32t module, u32t ordinal, u32t chaintype, void *handler);

/** build calling thunk for function.
    Returned thunk address must be used by any callers instead of original
    function pointer - to allow interception by chaining code.
    Next calls with the same function parameter will return the same thunk.

    @param  module      Module handle
    @param  function    Function address
    @return thunk address or 0 on error */
void *_std mod_buildthunk(u32t module, void *function);

/** free calling thunk.
    Call is optional, all thunks for this module will be released automatically
    while module unloading.
    @param  module      Module handle
    @param  thunk       Thunk addr from mod_buildthunk(), can be 0 for all
                        thunks in this module (this will force unchaining too).
    @return number of released thunks */
u32t  _std mod_freethunk (u32t module, void *thunk);

/** install chain procedure for a function.
    @param  module      Module handle. Hooks will be removed on module unloading.
    @param  thunk       Pointer to calling thunk (returned by mod_buildthunk())
    @param  chaintype   Type of chaining
    @param  handler     Function to install, this is mod_chainfunc for entry
                        and exit and replaced function type for replace
    @return success flag (1/0) */
int   _std mod_fnchain   (u32t module, void *thunk, u32t chaintype, void *handler);

/** remove chain procedure.
    @param  module      Module handle
    @param  thunk       Pointer to calling thunk, can be 0 for all functions in
                        this module (except module entiries, which uses
                        mod_apiunchain()).
    @param  chaintype   Type of chaining, can be 0 for all chaining types
    @param  handler     Function to remove, can be 0 for all functions of
                        this type
    @return number of removed functions */
u32t  _std mod_fnunchain (u32t module, void *thunk, u32t chaintype, void *handler);


/** get direct pointer to module function (by index).
    Function return direct pointer to original function (without thunk, used
    to intercept calls).
    Do not query it without CRITICAL needs, because API chaining is used
    widely (in cache, trace, console and so on...). I.e., in many cases direct
    call will cause system malfunction.

    And there is no way to query active APICN_REPLACE replacement for this
    ordinal because it can be unchained at any time and became invalid (for
    example - after module unloading).

    @param  module      Module handle.
    @param  ordinal     Function ordinal.
    @return pointer to function code or 0. */
void* _std mod_apidirect(u32t module, u32t ordinal);

/// get current process pid
u32t  _std mod_getpid   (void);

/** check pid existence.
    @return success flag 1/0 */
u32t  _std mod_checkpid (u32t pid);

/** get pid of process, executing this EXE module.
    @param        module  Module handle.
    @param [out]  parent  Parent process PID (can be 0)
    @return 0 on no process, module or module is DLL, else - process PID */
u32t  _std mod_getmodpid(u32t module, u32t *parent);

/** query current or parent process module name.
    @param  [out] name    Buffer for name (128 bytes), returned in upper case.
    @param        parent  Flag 1 for parent, 0 for current process.
    @return current pid or 0 on error (GS register altered) */
u32t  _std mod_appname(char *name, u32t parent);

/** query module name.
    @param        module  Module handle.
    @param  [out] buffer  Buffer for name (128 bytes), returned in upper case,
                          can be 0.
    @return 0 if no module, "buffer" parameter or ptr to string with name
            (not for modification & valid only while module is loaded!) */
char *_std mod_getname(u32t module, char *buffer);

/** query module by eip.
    @param [in]  eip     EIP value
    @param [out] object  Module`s object number (zero-based)
    @param [out] offset  Offset in object
    @param [in]  cs      Selector (optional, use 0 for FLAT space)
    @result Module handle or 0 */
module* _std mod_by_eip(u32t eip, u32t *object, u32t *offset, u16t cs);

typedef struct _mod_info {
   u32t                handle;    ///< module handle
   u32t                 flags;    ///< module flags
   u32t             start_ptr;    ///< module start pointer
   u32t             stack_ptr;    ///< module stack pointer
   u32t                 usage;    ///< usage counter
   u32t               objects;    ///< number of objects
   u32t              baseaddr;    ///< base address
   char             *mod_path;    ///< module full path
   char                 *name;    ///< module name
   /** zero-term list of used modules. Pointers cross-linked in the same memory
       block, returned by mod_enum() */
   struct _mod_info  *imports[MAX_IMPMOD+1];
} module_information;

/** enum all modules in the system.
    @param [out] pmodl   List of modules, must be released via free().
    @return number of modules in returning list */
u32t  _std mod_enum(module_information **pmodl);

/** query single object information.
    @param        module  Module handle.
    @param        object  Object index (0..x)
    @param  [out] addr    Address of object in FLAT space, 0 to skip
    @param  [out] size    Size of object, 0 to skip
    @param  [out] flags   Flags of object (see LX format), 0 to skip
    @param  [out] sel     Selector (for 16-bit objects), 0 to skip
    @return error code or 0 */
qserr _std mod_objectinfo(u32t module, u32t object, u32t *addr, u32t *size,
                          u32t *flags, u16t *sel);

/** return process list.
    Function returns active processes list as a process id enumeration
    array with zero in the last entry. Returning list must be released via
    free() call */
u32t* _std mod_pidlist(void);


typedef struct _thread_info {
   u32t                   tid;
   u32t                fibers;
   u32t           activefiber;
   u32t                 state;
   u32t                   sid;    ///< thread`s session id
   u32t                 flags;    ///< TFLM_* flags
   u64t                  time;
} thread_information;

typedef struct _process_info {
   u32t                   pid;    ///< process id
   module_information     *mi;    ///< executable module and all of imported DLLs
   char               *curdir;    ///< current directory
   u32t               threads;    ///< # of active threads
   u32t                  ncld;    ///< # of child processes
   struct _process_info **cll,    ///< child list
                      *parent;    ///< parent process (0 for pid 1)
   thread_information     *ti;    ///< thread list
} process_information;

/** enum all processes in the system.
    Returned data is bounded into the single block and should be released
    via one free() call. 

    This is a kind of snapshot of system state.
    @param [out] ppdl    Pointer to the returned process list.
    @return number of processes in list */
u32t  _std mod_processenum(process_information **ppdl);

/// dump process tree to log
void  _std mod_dumptree(void);
/// dump single process context to log
void  _std log_dumppctx(process_context* pq);

/** makes a copy of process environment for mod_exec() function.
    @param  pq          Process context
    @param  addspace    Additional space to allocate in result memory block.
    @return environment data in application heap block (use free() to release it). */
char* _std env_copy    (process_context *pq, u32t addspace);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MODULE_EXTRA
