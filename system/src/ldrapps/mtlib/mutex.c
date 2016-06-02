//
// QSINIT
// process/thread handling support
// ------------------------------------------------------------
// most of code here should not use any API.
// exceptions are:
//  * memcpy/memset (imported by direct pointer)
//  * 64-bit mul/div (exported by direct pointer in QSINIT)
//

#include "mtlib.h"
#include "qsxcpt.h"
#include "qcl/sys/qsmuxapi.h"

#define MUTEX_SIGN       0x4458554D   ///< MUTEX string

#define instance_ret(type,inst,err)      \
   type *inst = (type*)data;             \
   if (inst->sign!=MUTEX_SIGN) return err;

#define instance_void(type,inst)         \
   type *inst = (type*)data;             \
   if (inst->sign!=MUTEX_SIGN) return;

typedef struct {
   u32t           sign;
   mt_thrdata     **wl;
   u32t        wlalloc;
   u32t          wlmax;
   mt_thrdata   *owner;
} mutex_data;

static u32t     mutex_classid = 0;
static qs_muxcvtfunc  cvtmuxh = 0;

/// common init call.
static void _std mutex_init(void *data, qs_muxcvtfunc sfunc) {
   instance_void(mutex_data, md);
   cvtmuxh = sfunc;
}

/// create mutex.
static qserr _std mutex_create(void *data, mux_handle_int *res) {
#if 0
   mutex_data *rc = (mutex_data*)malloc(sizeof(mutex_data));
   rc->sign    = MUTEX_SIGN;
   rc->wl      = 0;
   rc->wlalloc = 0;
   rc->wlmax   = 0;
#endif
   return E_SYS_UNSUPPORTED;
}

/// capture mutex to current pid/tid.
static qserr _std mutex_capture(void *data, mux_handle_int res) {
   return E_SYS_UNSUPPORTED;
}

/// release mutex.
static qserr _std mutex_release(void *data, mux_handle_int res) {
   return E_SYS_UNSUPPORTED;
}

/// delete mutex.
static qserr _std mutex_free(void *data, mux_handle_int res) {
   return E_SYS_UNSUPPORTED;
}

/// is mutex free?
static int _std mutex_isfree(void *data, mux_handle_int res) {
   return 0;
}

static void *methods_list[] = { mutex_init, mutex_create, mutex_capture,
   mutex_release, mutex_free, mutex_isfree};

void register_mutex_class(void) {
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_sysmutex)!=sizeof(methods_list)) {
      log_printf("Function list mismatch\n");
      _throw_(xcpt_align);
   }
   // register private(unnamed) class
   mutex_classid = exi_register(0, methods_list, sizeof(methods_list)/sizeof(void*),
      0, 0, 0, 0);
   if (!mutex_classid)
      log_printf("unable to register mutex provider class!\n");
}
