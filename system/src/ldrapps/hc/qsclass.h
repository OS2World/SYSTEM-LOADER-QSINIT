//
// QSINIT API
// exportable pure C "classes" implementation
//
#ifndef QSINIT_EXPCLASSES
#define QSINIT_EXPCLASSES
/**
@file qsclass.h
@ingroup API2
Pure C "classes" implementation.

This API provides support for shared "interfaces" (structures with pure
function list and hidden instance data for it). After creating it looks
like classes in C code.
For example:
@code
   dd_list list = NEW(dd_list);
   list->add(2);
   list->add(1);
   list->sort(1,1);
   list->clear();
   DELETE(list);
@endcode

In MT mode all "class" instances are thread-unsafe, by default. As option,
a special internal mutex can be created for instance, it will protect class
method calls.

Tracing supported for "classes" as well. Trace info can be added into REF
file for this module - and then will be packed into module`s TRC file.

*/

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @name exi_create() & exi_createid() flags
//@{
#define EXIF_SHARED     0x0001    ///< create global instance, not assigned to the current process.
#define EXIF_MTSAFE     0x0002    ///< cover all calls by mutex, unique for this instance
//@}

/** Create "class" instance with specified name.
    Note, what if EXIC_GMUTEX was selected in class registration, EXIF_MTSAFE
    flag is void and every call in all instances will wait for a global class
    mutex.
    Function deny class names, created with EXIC_PRIVATE flag.

    @param  classname  Class name.
    @param  flags      Creation flags (EXIF_*)
    @return pointer to created instance or 0. */
void*   _std exi_create(const char *classname, u32t flags);

/** Create "class" instance by ID.
    @param  classid    Class ID.
    @param  flags      Creation flags (EXIF_*)
    @return pointer to created instance or 0. */
void*   _std exi_createid(u32t classid, u32t flags);

/** Free instance.
    @param  instance   Pointer to instance. */
void    _std exi_free(void *instance);

/// new(classtype) macro
#define NEW(x) ((x)exi_create(# x, 0))
/// new global instance
#define NEW_G(x) ((x)exi_create(# x, EXIF_SHARED))
/// new thread-safe instance
#define NEW_M(x) ((x)exi_create(# x, EXIF_MTSAFE))
/// new global thread-safe instance ;)
#define NEW_GM(x) ((x)exi_create(# x, EXIF_SHARED|EXIF_MTSAFE))

/// delete(instance) macro
#define DELETE(x) exi_free(x)

/// calling convention for class methods
#define _exicc    __cdecl
/// lead args for class method implementation functions
#define EXI_DATA  void *data, void *__caller

/** change instance type.
    @param  instance   Pointer to instance.
    @param  global     Instance type (process owned(0) or global(1) or just
                       query current type(-1)). Local instances will be
                       deleted by process exit code.
    @return previous state or -1 on error */
int     _std exi_share(void *instance, int global);

/** enable/disable per-instance mutex usage in MT mode.
    This function switches thread-safe state for the single instance. By
    default instances have no any synchronization (i.e. unsafe). Enable call
    creates individual mutex, which will block every call of class methods.

    This functionality is supported for all classes automatically.
    Call can be done before MT mode activation.
    Function returns error (-1) if mutex is busy at the time of disable call.

    Class registration can also ask for the global single mutex for all class
    instances and such mode does not controlled by the class user. In this case
    function will always return value of 2.

    @param  instance   Pointer to instance.
    @param  enable     New state (enable=1 / disable=0 / query=-1)
    @return previous state or -1 on error. */
int     _std exi_mtsafe(void *instance, int enable);

/** increase instance mutex lock counter manually.
    Note, that user should not touch instance mutex by default - all locking
    performed automatically during any method call. This call and exi_unlock()
    pair is for the case when a group of actions with instance must be locked.

    Number of exi_unlock() calls must be equal to number of exi_lock().
    Both calls do nothing when MT mode is not started.

    Also note, that instance pointer must be valid, this call is NOT protected
    by exception handler like exi_classid() and exi_classname().

    @param  instance   Pointer to instance. */
void    _std exi_lock(void *instance);

/// decrease instance mutex lock counter
void    _std exi_unlock(void *instance);

/** Query instance class name.
    Function returns name for all classes, including created with EXIC_PRIVATE
    flag.
    @param  instance   Pointer to instance.
    @return class name or 0. This string must be free()-ed after use */
char*   _std exi_classname(void *instance);

/** Query instance class ID.
    Call is safe with any pointer (verification is protected by exception
    handler) - i.e. function can be used as IsItClass(ptr). This note includes
    exi_classname() above.

    @param  instance   Pointer to instance.
    @return class ID or 0. */
u32t    _std exi_classid(void *instance);

/** Query internal instance data.
    Format of class data is hidden from user, by default. This function is
    for solving of some kind of class implemenations, basically.
    @param  instance   Pointer to instance.
    @return internal data or 0. */
void*   _std exi_instdata(void *instance);

/** Query class id by name.
    @attention function not just name querying, but forces loading of module
               with requested class.
    Function will return 0 for class name, created with EXIC_PRIVATE flag.
    @param  classname  Class name.
    @return class id or 0. */
u32t    _std exi_queryid(const char *classname);

/** Query class name presence.
    @param  classname  Class name.
    @return EXIQ_* value */
u32t    _std exi_query(const char *classname);

/// @name exi_query() value
//@{
#define EXIQ_UNKNOWN    0x0000    ///< class name unknown
#define EXIQ_KNOWN      0x0001    ///< class known, but NOT available
#define EXIQ_AVAILABLE  0x0003    ///< class name present and its supporting module is loaded
#define EXIQ_PRIVATE    0x0007    ///< the same as EXIQ_AVAILABLE, but class is private!
//@}

/** Query number of "methods" in "class".
    @param  classid    Class ID.
    @return number of methods or 0. */
u32t    _std exi_methods(u32t classid);

/** Query handle of the module, who implements class code.
    @param  classid    Class ID.
    @return module handle or 0 if no such class id */
u32t    _std exi_provider(u32t classid);

/** Constructor/destruction function for "class".
    Constructor (if present) called at the end of exi_create call.
    Destructor (if present) called in the start of exi_free call.

    @param  instance   Pointer to instance.
    @param  userdata   Pointer to user data. */
typedef void _std (*exi_pinitdone)(void *instance, void *userdata);

/** Register new interface.
    Actually instance data looks in this way:

    @code
    ------------ private part -----------
    signature (4 bytes)
    class id (4 bytes)
    user data size (4 bytes)
    internal data (20 bytes)
    ------------ public part ------------
    public function list (4 * num_of_funcs)
    ------------ private part -----------
    user data
    space for 16 byte align
    call thunks (32 * num_of_funcs)
    -------------------------------------
    @endcode
    Instance user data will be zeroed after create function.

    @attention  All "methods" _MUST_ use cdecl calling convention!

    EXIC_GMUTEX flag creates single global mutex for all class instances.

    EXIC_PRIVATE flag disables exi_create() call for this class, but any other
    logic is untouched, i.e. exi_query() and exi_classname() works as usual,
    class can be traced and its name is visible in the debug dump (Ctrl-Alt-F3).

    EXIC_EXCLUSIVE means that this class can have no more than ONE instance
    per system.

    Namespace for classes is *global*. Even for a class that marked as private
    no duplicate name is allowed.

    @param  classname   Class name. Can be 0, in this case unique random name
                        will be auto-generated.
    @param  funcs       Pointer to list of funcions.
    @param  funccount   Number of funcions.
    @param  datasize    Size of user data in instance, can be 0. If data size
                        is 0 - both constructor/destructor and method functions
                        will recieve 0 in data pointer field.
    @param  flags       Options (EXIC_*)
    @param  constructor Constructor function or 0.
    @param  destructor  Destructor function or 0.
    @param  module      Module handle of module with class code.
                        Class will be unregistered automatically while module
                        unloading (this will, also, cause panic screen on any
                        existing lost instances ;)).
                        Parameter can be 0 if all functions placed in the same
                        module.
    @return class ID or 0 (no duplicate names allowed, unregister it first). */
u32t    _std exi_register(const char *classname, void **funcs, u32t funccount,
                          u32t datasize, u32t flags, exi_pinitdone constructor,
                          exi_pinitdone destructor, u32t module);

/// @name exi_register() flags
//@{
#define EXIC_GMUTEX     0x0001    ///< create global mutex, one for all instances
#define EXIC_PRIVATE    0x0002    ///< class can be created by exi_createid() ONLY
#define EXIC_EXCLUSIVE  0x0004    ///< only one instance per system is possible
//@}

/** Deregister "class" by ID.
    Class cannot be deregistered if class instances still present.
    @param  classid    Class ID.
    @return success indicator. */
int     _std exi_unregister(u32t classid);

/** Print "class" info to log.
    @param  classid    Class ID.
    @return success indicator. */
int     _std exi_printclass(u32t classid);

/** check consistency.
    Function walks over all existing instances and checks signature and class
    id match. Called automatically on module exit.
    Note, that existing structure damage can cause exception xcpt_exierr
    (trap screen), but this function returns to user with error instead of it.
    @return 0 if something wrong */
int     _std exi_checkstate(void);

/** Instance enumeration callback.
    Callback function should be atomic, because entire exi_instenum() call
    locks MT mode.
    @param  classid    Class id.
    @param  instance   Pointer to instance.
    @param  userdata   Pointer to user data of this instance.
    @return 1 to continue enum and 0 to stop it */
typedef int _std (*exi_pinstenumcb)(u32t classid, void *instance, void *userdata);

/// @name exi_instenum() etype value
//@{
#define EXIE_GLOBAL     0x0001    ///< enum global instances
#define EXIE_CURRENT    0x0002    ///< enum current process instances
#define EXIE_LOCAL      0x0004    ///< enum instances for all processes except current
#define EXIE_ALL        (EXIE_GLOBAL|EXIE_CURRENT|EXIE_LOCAL)
//@}

/** Enumerate class instances.
    @param  classid    Class id.
    @param  efunc      Enumeration callback function.
    @param  etype      Enumeration type
    @return success flag (1/0). */
int     _std exi_instenum(u32t classid, exi_pinstenumcb efunc, u32t etype);

/** Enable chaining/tracing of specified class id.
    Function called internally by trace code to rebuild calling thunks.
    Also can be called to use chaining on class methods.
    Operation is not reversable.
    @param  classid    Class ID.
    @return success indicator. */
int     _std exi_chainon(u32t classid);

/** Switch single instance chaining mode.
    Call is safe with any pointer.
    @param  instance   Pointer to instance.
    @param  enable     New chaining state (enable=1 / disable=0 / query=-1)
    @return previous state or -1 on error */
int     _std exi_chainset(void *instance, int enable);

/** return list of thunks for all methods of class.
    Thunks are pointers, returned by mod_buildthunk(). These pointers can
    be used in mod_fnchain() to set chaining functions on class methods.
    @param  classid    Class ID.
    @return list of thunk pointers or 0. */
pvoid*  _std exi_thunklist(u32t classid);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXPCLASSES
