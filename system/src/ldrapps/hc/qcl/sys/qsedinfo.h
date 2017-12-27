//
// QSINIT API
// common interface for emulated disk management
//
#ifndef QSINIT_EXT_DISK_INFO
#define QSINIT_EXT_DISK_INFO

#include "qstypes.h"
#include "qsclass.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Extended disk info shared class.
    This is not really existing shared class.
    "qs_extdisk" is a typedef for it, every "emulated disk" can be supplied
    with class internally and this class must be compatible with "qs_extdisk".

    I.e. if hlp_diskclass() returns a valid pointer - then it can be used to
    manage this disk. */

typedef struct qs_extdisk_s {
   /** get disk geometry.
       @param  [out] geo    disk info data.
       @return 0 on success, else error code */
   qserr _exicc (*getgeo)(disk_geo_data *geo);
   /** set disk geometry.
       Function can support changing of CHS for emulated disks, this used in
       "dmgr clone" command.
       @param  geo          disk info data.
       @return 0 on success, else error code */
   qserr _exicc (*setgeo)(disk_geo_data *geo);
   /** get printable disk info.
       @return printable string (must be free()-ed) or 0 if no info. */
   char* _exicc (*getname)(void);
   /** query/set disk state.
       Function can ignore anything except EDSTATE_QUERY.
       @param state         new state or query call (with EDSTATE_QUERY)
       @return current state */
   u32t  _exicc (*state)(u32t state);
} _qs_extdisk, *qs_extdisk;


/// @name qs_extdisk->state in/out value
//@{
#define EDSTATE_QUERY 0x80000000   ///< query current state flags
#define EDSTATE_RO        0x0001   ///< disk is r/o
#define EDSTATE_NOCACHE   0x0002   ///< disk i/o is not cached by default
#define EDSTATE_RAM       0x0004   ///< storage is RAM
//@}

/** query additional interface for this disk.
    @param disk      disk number
    @param name      interface name ("qs_extdisk" assumed if arg is 0).
    @return 0 or "class" pointer. This is global pointer and it must not be
            deleted! */
qs_extdisk _std hlp_diskclass(u32t disk, const char *name);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_EXT_DISK_INFO
