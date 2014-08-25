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

#ifndef QSINIT_MODULE
#define MODULE_INTERNAL
#include "qsmod.h"
#endif // QSINIT_MODULE

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

    By default entry/exit hook appends to the end of chain list for this
    function. APICN_FIRSTPOS can be added to mod_apichain() "chaintype" arg
    to insert function to the first pos.
    Entry/exit hooks used by tracing. To cooperate with it all exit chains
    must use APICN_FIRSTPOS flag (it is better for trace to be the 1st in
    entry list and last in exit).

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

    @param  module      Module handle
    @param  ordinal     Function ordinal
    @param  chaintype   Type of chaining
    @param  handler     Function to install, this is mod_chainfunc for entry
                        and exit and replaced function type for replace
    @return success flag (1/0) */
int   _std mod_apichain  (u32t module, u32t ordinal, u32t chaintype, void *handler);

/** remove api chain procedure.
    @param  module      Module handle
    @param  ordinal     Function ordinal, can be 0 for all ordinals
    @param  chaintype   Type of chaining, can be 0 for all chaining types
    @param  handler     Function to remove, can be 0 for all functions of
                        this type
    @return number of removed functions */
u32t  _std mod_apiunchain(u32t module, u32t ordinal, u32t chaintype, void *handler);

/** get direct pointer to module function (by index).
    Function return direct pointer to original function (without thunk, used
    to intercept calls).
    Do not query it without CRITICAL needs, because API chaining is used 
    widely (in cache, trace, graphic console and so on...)

    And there is no way to query active APICN_REPLACE replacement for this
    ordinal because it can be unchained at any time and became invalid (for
    example - after module unloading).

    @param  module      Module handle.
    @param  ordinal     Function ordinal.
    @return pointer to function code or 0. */
void* _std mod_apidirect(u32t module, u32t ordinal);

/// get current pid (does not used seriously)
u32t  _std mod_getpid(void);

/** query current or parent process module name.
    @param  [out] name    Buffer for name (128 bytes), returned in upper case.
    @param        parent  Flag 1 for parent, 0 for current process.
    @return current pid or 0 on error (GS register altered) */
u32t  _std mod_appname(char *name, u32t parent);

/** query module name.
    @param        module  Module handle.
    @param  [out] buffer  Buffer for name (128 bytes), returned in upper case,
                          can be 0.
    @return 0 if no module, "buffer" parameter with name if it !=0 or pointer
            to module name in system module info data (do not modify this 
            string!) */
char *_std mod_getname(u32t module, char *buffer);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MODULE_EXTRA