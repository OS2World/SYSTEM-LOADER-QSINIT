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

/* this is not real existing shared class.
   "qs_extdisk" is a typedef for it, every "emulated disk" can be supplied
   with class internally and this class must be compatible with "qs_extdisk".

   I.e. if hlp_diskclass() returns a valid pointer - then it can be used to
   manage this disk. */

typedef struct qs_extdisk_s {
   /** get disk geometry. 
       @param  [out] geo    disk info data.
       @return 0 on success, else EDERR_* error value value */
   u32t  _std (*getgeo)(disk_geo_data *geo);
   /** set disk geometry. 
       Function can support changing of CHS for emulated disks, this used in
       "dmgr clone" command.
       @param  geo          disk info data.
       @return 0 on success, else EDERR_* error value value */
   u32t  _std (*setgeo)(disk_geo_data *geo);
   /** get printable disk info.
       @return printable string (must be free()-ed) or 0 if no info. */
   char* _std (*getname)(void);
   /** set read-only state.
       Function can ignore this action.
       @param state         new state (1 - r/o, 0 - r/w, -1 - query only)
       @return previous (current with state=-1 arg) disk r/o state or -1 
               on error */
   int   _std (*setro)(int state);
} _qs_extdisk, *qs_extdisk;

#define EDERR_NOSUPP     0x0001    ///< operation is not supported
#define EDERR_INVDISK    0x0002    ///< disk is in invalid state (unmounted?)
#define EDERR_INVARG     0x0003    ///< invalid argument
#define EDERR_IOERR      0x0004    ///< disk i/o error

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
