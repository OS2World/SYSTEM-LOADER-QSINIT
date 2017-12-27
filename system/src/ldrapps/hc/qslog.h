//
// QSINIT
// debug/log functions
//
#ifndef QSINIT_DEBUGLOG
#define QSINIT_DEBUGLOG

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** print character to the *debug* serial port */
void  _std  hlp_seroutchar(u32t symbol);

/** print string to the *debug* serial port */
void  _std  hlp_seroutstr (char* Buf);

/** query debug serial port parameters.
    @param [out] baudrate     Pointer to baudrate or 0
    @return port address or 0 */
u16t  _std  hlp_seroutinfo(u32t *baudrate);

/** set *debug* serial port parameters.
    Note, that hlp_serialset() call with the same port address will disable
    *debug* com port output.

    @param port       COM port address. use 0 for previous value, 0xFFFF to
                      disable debug COM port output.
    @param baudrate   use 0 for previos value, else valid baud rate
    @return 1 on success, 0 on invalid baud rate (port addr can`t be checked) */
int   _std  hlp_seroutset (u16t port, u32t baudrate);

/** switch debug com port to vt100 terminal (and back).
    @param sesno      Session number, use 0 to revert port back to debug output
                      mode.
    @return error code or 0 */
qserr _std  hlp_serconsole(u32t sesno);

/** set 16450-compatible serial port baud rate.
    @param port       COM port address.
    @param baudrate   valid baud rate
    @return 1 on success, 0 on invalid baud rate (port addr can`t be checked) */
int   _std  hlp_serialrate(u16t port, u32t baudrate);

/** get a characer from 16450-compatible serial port.
    @param port       COM port address.
    @return character or zero */
u8t   _std  hlp_serialin(u16t port);

/** print character to 16450-compatible serial port.
    @param port       COM port address.
    @param chr        Symbol to print */
void  _std  hlp_serialout(u16t port, u8t chr);

/** init 16450-compatible serial port.
    Note, that function disables *debug* com port output if "port" parameter
    match to current debug port.

    @param port       COM port address.
    @param baudrate   Valid baud rate, 0 for 115200.
    @return 1 on success, 0 on invalid baud rate (port addr can`t be checked) */
int   _std  hlp_serialset(u16t port, u32t baudrate);

/// print to log/serial port.
int __cdecl log_printf(const char *fmt, ...);

/** print to log/serial port.
    @param level      priority (0..3)
    @param fmt        printf format string
    @return standard vsnprintf result value */
int __cdecl log_it(int level, const char *fmt, ...);

/** push string directly into log.
    @param level      priority (0..3)
    @param str        string to put
    @return success flag */
int   _std  log_push(int level, const char *str);

/** push string directly into log (with own time mark).
    Note, that saving string into log causes request of current time
    from system. On some rare ops this can cause malfunction of
    current actions, "time" parameter allow to specify time mark
    directly to solve this trouble.

    @param level      log flags (priority and LOGIF_SECOND)
    @param str        string to put
    @param time       time mark (can be 0 to get current)
    @return success flag */
int   _std  log_pushtm(int level, const char *str, u32t time);

/// dump MDT table
void  _std  log_mdtdump(void);

/// dump opened files
void  _std  log_ftdump(void);

/// dump session list
void  _std  log_sedump(void);


#ifdef __WATCOMC__
/** dump CPU registers into log & debug port.
    Function preserves all registers and flags, ss selector must be FLAT.
    @param level      priority (0..3) */
void        log_dumpregs(int level);
/// dump CPU registers into debug port ONLY.
void        hlp_dumpregs(void);

#pragma aux log_dumpregs "_*" parm routine modify exact [];
#pragma aux hlp_dumpregs "_*" modify exact [];

#else  // !__WATCOMC__
void  _std  log_dumpregs(int level);
void  _std  hlp_dumpregs(void);
#endif


/// @name flags for log_gettext
//@{
#define LOGTF_LEVELMASK 0x0003 ///< mask for max level value
#define LOGTF_LEVEL     0x0004 ///< print log entry level
#define LOGTF_TIME      0x0008 ///< print log entry time
#define LOGTF_FLAGS     0x0010 ///< print log entry flags
#define LOGTF_DATE      0x0020 ///< print log entry date
#define LOGTF_DOSTEXT   0x0040 ///< put \r\n at the end of line (\n by default)
#define LOGTF_SHARED    0x0080 ///< allocate shared heap block
//@}

/** query log in text form.
    @param flags    Flags
    @return log text, must be freed by free() */
char*  _std log_gettext(u32t flags);

/// clear log
void   _std log_clear(void);

/// @name log levels
//@{
#define LOG_HIGH        0
#define LOG_NORMAL      1
#define LOG_MISC        2
#define LOG_GARBAGE     3
//@}

#ifdef VERBOSE_LOG
#define log_misc log_it
#else
#ifdef __WATCOMC__
#define log_misc(...)
#else
static void log_misc(int level, char *fmt,...) {}
#endif
#endif

#ifdef LOG_INTERNAL

#define LOG_SIGNATURE  (0x45474F4C)

/// @name flags for log_header
//@{
#define LOGIF_LEVELMASK 0x0003 ///< mask for priority value
#define LOGIF_SECOND    0x0004 ///< 1 second bit for dostime
#define LOGIF_USED      0x0008 ///< entry is used
#define LOGIF_DELAY     0x0010 ///< entry was delayed
#define LOGIF_REALMODE  0x0020 ///< entry was created in real mode
//@}

/// log header. Warning! Size 16 assumed in log.c!
typedef struct {
   u32t        sign;          ///< sign, 0 on end of log data
   u32t     dostime;          ///< dostime of this entry or 0
   u32t      offset;          ///< offset to next log_header, in 16 bytes
   u32t       flags;          ///< flags
} log_header;

typedef void _std (*log_querycb)(log_header *log, void *extptr);

/** query log.
    Log output is blocked until cbproc exit, data in
    log is NOT sorted by time.
    @param  cbproc   Callback proc to read log.
    @param  extptr   App data for cbproc call
    @retval EINVAL   Invalid cbproc value (zero)
    @retval EBUSY    Another one log_query() in progress (this can be
                     recursive call from cbproc())
    @retval 0        On success */
int    _std log_query(log_querycb cbproc, void *extptr);

/// flush delayed/realmode log entries to actual log (internal call).
void   _std log_flush(void);

#endif // LOG_INTERNAL

#ifdef __cplusplus
}
#endif

#endif // QSINIT_DEBUGLOG
