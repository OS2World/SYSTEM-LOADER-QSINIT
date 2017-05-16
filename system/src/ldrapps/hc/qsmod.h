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
    @param       path    Path to module (full or relative to current dir)
    @param       flags   Must be 0.
    @param [out] error   Error code, can be 0
    @param       extdta  Must be 0.
    @return module handle or 0 */
u32t  _std mod_load(const char *path, u32t flags, qserr *error, void *extdta);

/** exec module.
    Low level exec function.
    See also mod_execse().

    @param       module  Module handle.
    @param       env     Environment data, use 0 to make a parent`s copy.
    @param       params  Arguments string.
    @param       mtdata  MTLIB-specific data, must be 0 in common call.
    @return -1 on error or program exit code.
    @see cmd_exec() */
s32t  _std mod_exec(u32t module, const char *env, const char *params, void *mtdata);

/** search and load module.
    Function searches in LIBPATH and PATH, DLL extension assumed, EXE if
    no DLL was found.
    @param       name    Module name.
    @param       flags   Must be 0.
    @param [out] error   Error code, can be 0
    @return module handle or 0 */
u32t  _std mod_searchload(const char *name, u32t flags, qserr *error);

/// @name mod_query() flags
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
qserr _std mod_free(u32t module);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MODULE
