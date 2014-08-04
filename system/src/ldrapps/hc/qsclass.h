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

This API provide support for shared "interfaces" (structures with pure
function list and instance data for it). After creating it looks like
classes in C code.
For example:
@code
   dd_list list = (dd_list)exi_create("dd_list");
   list->add(2);
   list->add(1);
   list->sort(1,1);
   list->clear();
   exi_free(list);
@endcode */

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create "class" instance with specified name.
    @param  classname  Class(interface) name.
    @return pointer to created instance or 0. */
void*   _std exi_create(const char *classname);

/** Create "class" instance by ID.
    @param  classid    Class(interface) ID.
    @return pointer to created instance or 0. */
void*   _std exi_createid(u32t classid);

/** Query instance class name.
    @param  instance   Pointer to instance.
    @return class name or 0. COPY this data immediatly and do not touch it! */
char*   _std exi_classname(void *instance);

/** Query instance class ID.
    @param  instance   Pointer to instance.
    @return class ID or 0. */
u32t    _std exi_classid(void *instance);

/** Constructor/destruction function for "class".
    Constructor (if present) called at the end of exi_create call.
    Destructor (if present) called in the start of exi_free call.

    @param  userdata   Pointer to user data. */
typedef void _std (*exi_pinitdone)(void *userdata);

/** Register new interface.
    Actually instance data looks in the following way:

    @code
    ------------ private part -----------
    signature (4 bytes)
    class id (4 bytes)
    user data size (4 bytes)
    reserved (4 bytes)
    ------------ public part ------------
    public function list (4 * funcscount)
    ------------ private part -----------
    user data
    space for 16 byte align
    call thunks (16 * funcscount)
    -------------------------------------
    @endcode
    Instance user data will be zeroed after create function.

    @param  classname   Class name.
    @param  funcs       Pointer to list of funcions.
    @param  funcscount  Number of funcions.
    @param  datasize    Size of user data in instance.
    @param  constructor Constructor function or 0.
    @param  destructor  Destructor function or 0.
    @return instance ID or 0 (no duplicate names allowed, unregister it first). */
u32t    _std exi_register(const char *classname, void **funcs, u32t funccount,
                          u32t datasize, exi_pinitdone constructor,
                          exi_pinitdone destructor);

/** Deregister "class" by ID.
    @param  classid    Class(interface) ID.
    @return success indicator. */
int     _std exi_unregister(u32t classid);

/** Free instance.
    @param  instance   Pointer to instance. */
void    _std exi_free(void *instance);

/// new(classtype) macro
#define NEW(x) ((x)exi_create(# x))
/// delete(instance) macro
#define DELETE(x) exi_free(x)

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXPCLASSES
