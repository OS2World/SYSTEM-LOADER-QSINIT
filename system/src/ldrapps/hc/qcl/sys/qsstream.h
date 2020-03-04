//
// QSINIT API
// low level stream api
//
#ifndef QSINIT_STREAMIO_CLASS
#define QSINIT_STREAMIO_CLASS

#include "qstypes.h"
#include "qsclass.h"
#include "qsshell.h"
#include "qsio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t   st_handle_int;

/** This code is, actually, internal thing and must not be used
    at the user level (even if you can get such a ptr in some way ;).
    
    Also note, that unlike qs_sysvolume, which is always called in MT lock,
    this one is called UNLOCKed (because of speed of characted device i/o).

    So, implementation should care about MT sync by itself.    
*/
typedef struct qs_sysstream_s {
   /// if name is 0, then pfh is a parent handle for inheritance
   qserr     _exicc (*open)     (const char *name, st_handle_int *pfh);
   qserr     _exicc (*read)     (st_handle_int fh, void *buffer, u32t *size);
   /// return number of available to read bytes
   u32t      _exicc (*avail)    (st_handle_int fh);
   qserr     _exicc (*write)    (st_handle_int fh, void *buffer, u32t *size);
   qserr     _exicc (*flush)    (st_handle_int fh);
   qserr     _exicc (*close)    (st_handle_int fh);
   /// return existing device list (in the application owned heap block)
   str_list* _exicc (*devlist)  (void);
} _qs_sysstream, *qs_sysstream;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_STREAMIO_CLASS
