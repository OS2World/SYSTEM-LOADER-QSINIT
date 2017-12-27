//
// QSINIT
// system session management
//
#ifndef QSINIT_SESMGR
#define QSINIT_SESMGR

#include "qstypes.h"
#include "qsshell.h"

#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

/// query current session number
u32t           se_sesno   (void);
#ifdef __WATCOMC__
#pragma aux se_sesno "_*" value [eax] modify exact [eax];
#endif

typedef struct {
   u32t            mx;
   u32t            my;
   u32t       sess_no;          ///< session number
   u32t         flags;          ///< session flags
   u32t      handlers;          ///< number of *active* video handlers
   u32t       devmask;          ///< mask of devices, attached to this session
   char        *title;          ///< session title, can be 0

   struct {
      char   name[32];          ///< printable name
      u32t     dev_id;          ///< device id
      u32t  dev_flags;          ///< device handler flags
      u32t   reserved;
   } mhi[1];
} se_stats;

/// @name session flags
//@{
#define VSF_FOREGROUND  0x001   ///< foreground at least on one device
#define VSF_NOLOOP      0x002   ///< session is excluded from switch loop
#define VSF_HIDDEN      0x004   ///< session is excluded from se_enum()
//@}

/** query session information.
    @param sesno    session number.
    @return buffer with data or 0 on error. Buffer is process owned memory
            and should be released via free() call. */
se_stats* _std se_stat      (u32t sesno);

/** push key code into session`s keyboard queue.
    @param sesno       session number.
    @param code        key value
    @param statuskeys  keyboard status (key_status()).
    @return error code or zero. Note, that function denies detached sessions. */
qserr     _std se_keypush   (u32t sesno, u16t code, u32t statuskeys);

/** get foreground session number (on the display device).
    return session number or 0 on error. */
u32t      _std se_foreground(void);

/** start new session in the current thread.
    This function creates new session and switches current thread into it.
    @param devices     list of devices to use (in bits, 0..31). Use zero to
                       clone device list of current session. Note, that this
                       parameter also defines initial video mode for a new
                       session. Non-zero value sets neutral 80x25 mode and
                       zero will inherit mode from the current session.
    @param foremask    AND mask for devices to make this session foreground,
                       can be 0xFFFFFFFF. Applied to cloned device list when
                       "devices" arg is zero.
    @param title       session title, optional (can be 0)
    @retval 0               on success
    @retval E_SYS_INVPARM   Invalid "devices" bit mask
    @retval E_MT_DISABLED   MT mode is not active
    @retval E_CON_DETACHED  Main thread of detached process cannot be switched
                            to another session. */
qserr     _std se_newsession(u32t devices, u32t foremask, const char *title);

/** switch session to foreground.
    @param sesno       session number. Note, that using 0 here is internal
                       case and return no error.
    @param foremask    AND mask for devices to make this session foreground,
                       can be 0xFFFFFFFF for all.
    @return error code or zero. */
qserr     _std se_switch    (u32t sesno, u32t foremask);

/** select next session on the device.
    @param dev_id      device where to switch session
    @retval E_CON_NODEVICE  no such device
    @retval E_SYS_NOTFOUND  if device has only one active session */
qserr     _std se_selectnext(u32t dev_id);

/// set session title
qserr     _std se_settitle  (u32t sesno, const char *title);

/** set session flags.
    @param sesno    session number.
    @param mask     bit mask to apply changes to session flags, only
                    VSF_NOLOOP and VSF_HIDDEN bits are accepted.
    @param values   bit values to apply.
    @return error code or zero. */
qserr     _std se_setstate  (u32t sesno, u32t mask, u32t values);

/** add a new video handler device.
    @param handler       handler`s class name (@see _qs_vh).
    @param [out] dev_id  device id for new added device, canNOT be 0.
    @param setup         optional setup string for device init (can be 0)
    @return error code or 0 */
qserr     _std se_deviceadd (const char *handler, u32t *dev_id, const char *setup);

qserr     _std se_devicedel (u32t dev_id);

/** enum all available console devices.
    "mx", "my", "sess_no" & "title" fields are zero, "reserved" field contains
    active session number or zero if no one and "devmask" field contains bit
    mask of available devices (0..31, same as se_devicemask() function result).

    @return buffer with data or 0 on error. Buffer is process owned memory
            and should be released via free() call. */
se_stats* _std se_deviceenum(void);

/** enum sessions.
    Sessions with VSF_HIDDEN flag are excluded from this list.
    @param devmask  Bit mask to include sessions into list. Zero mask (as well
                    as 0xFFFFFFFF) means sessions on all devices.
    @return session data array with zero pointer in the last entry. All of data
            allocated in single process owned heap block and must be released
            via single free() call. Function return 0 if no sessions found. */
se_stats**_std se_enum      (u32t devmask);

/// a short form of device enumeration - return a bit list of existing devices.
u32t      _std se_devicemask(void);

/** add device to existing session.
    @param sesno    session number.
    @param dev_id   device id.
    @return error value or 0. E_CON_MODERR means that current session mode is
            not supported by the device. */
qserr     _std se_attach    (u32t sesno, u32t dev_id);

/** remove input/output handler from existing session.
    Most of remove actions are prohibited by handler`s logic, but you can
    remove "tty" handler, for example.
    @param sesno    session number.
    @param handler  handler name.
    @return error value or 0. */
qserr     _std se_detach    (u32t sesno, u32t dev_id);

#ifdef __cplusplus
}
#endif
#pragma pack()
#endif // QSINIT_SESMGR
