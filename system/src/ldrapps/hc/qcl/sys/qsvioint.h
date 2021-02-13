//
// QSINIT API
// low level vio support
//
#ifndef QSINIT_VIO_INT_CLASSES
#define QSINIT_VIO_INT_CLASSES

#include "qstypes.h"
#include "qsclass.h"
#include "stdarg.h"
#include "vio.h"

#ifdef __cplusplus
extern "C" {
#endif

/// keyboard handler template class.
typedef struct qs_kh_s {
   /** first call after constructor.
       This class creation must be used only by video handlers now, never call
       them directly.
       Setup format: empty string or 0 both for qs_kh_tty and qs_kh_vio.
       @return error code or 0 */
   qserr _exicc (*init     )(const char *setup);
   /// output device for this instance (or 0)
   void* _exicc (*output   )(void);
   u16t  _exicc (*read     )(int wait, u32t *status);
   u32t  _exicc (*status   )(void);
} _qs_kh, *qs_kh;

/// @name qs_gh color flags
//@{
#define VGF_PAL8            0x01
#define VGF_BGR             0x02          ///< reverse color order in 16/24/32 bits
//@}

/** graphic handler template class.
    This is a definition for output handlers, as qs_vh above. */
typedef struct qs_gh_s {
   qserr _exicc (*setmode  )(u32t width, u32t height, void *extmem);
   /** write pixel data to screen.
       @param   x       Start x point
       @param   y       Start y point
       @param   dx      Number of columns
       @param   dy      Number of lines
       @param   src     Source data in current mode color format
       @param   pitch   Source data line length
       @return error code or 0 */
   qserr _exicc (*write    )(u32t x, u32t y, u32t dx, u32t dy, void *src, u32t pitch);
   /** read pixel data from screen.
       @param   x       Start x point
       @param   y       Start y point
       @param   dx      Number of columns
       @param   dy      Number of lines
       @param   dst     Destination data buffer
       @param   pitch   Destination data line length
       @return error code or 0 */
   qserr _exicc (*read     )(u32t x, u32t y, u32t dx, u32t dy, void *dst, u32t pitch);
   /** fills rectangle on screen.
       @param   x       Start x point
       @param   y       Start y point
       @param   dx      Number of columns
       @param   dy      Number of lines
       @param   color   Color value in current mode color format
       @return error code or 0 */
   qserr _exicc (*clear    )(u32t x, u32t y, u32t dx, u32t dy, u32t color);
   /** set VGA palette.
       @param   pal    768 bytes buffer with palette (RGB RGB ...) */
   qserr _exicc (*setvgapal)(void *pal);
   /** get VGA palette.
       @param   pal    768 bytes buffer for pallete (RGB RGB ...) */
   qserr _exicc (*getvgapal)(void *pal);
} _qs_gh, *qs_gh;

/// video handler settings
typedef struct vh_settings_s {
   u16t        color;
   u16t        shape;        ///< cursor shape value
   s32t           px;        ///< current position (-1 means - unsupported)
   s32t           py;        ///< current position (-1 means - unsupported)
   u32t         opts;        ///< active option bits
   u32t     ttylines;        ///< current tty lines counter (zero after mode change)
} vh_settings;

/** video handler template class.
    This is not a real class, but a definition for vio output handlers.
    Each one of them should be defined as follows. */
typedef struct qs_vh_s {
   /** first call after constructor.
       setup string format:
         qs_vh_tty:  "port;rate" or "port" for default 115200.
         qs_vh_vio, qs_viobuf : empty or 0.
       @return error code or 0 */
   qserr _exicc (*init     )(const char *setup);
   /** input device for this instance.
       Init call may create keyboard handler instance as a pair for
       this video handler. It presence can be queried via this call.
       @return linked or 0 */
   qs_kh _exicc (*input    )(void);
   /// graphic device for this instance (if available)
   qs_gh _exicc (*graphic  )(void);
   /** information about this handler.
       Note, that mode_id field has no high part with device number here,
       but mode number itself (low word) must be a constant value.

       If VHI_SLOW flag returned here, vio system starts to optimize
       vio_writebuf() call on the device, handled by this class. It scans for
       memory changes (in a shadow copy of device memory) and then write only
       mismatched data.
       Now this logic used on TTY and native UEFI consoles.

       @param   modes   List of available modes in system heap (caller
                        must release it via free()), pointer can be 0.
                        Function may return 0 value if class has no any
                        predefined modes.
       @param   prnname buffer for printable name (32 bytes, can be 0)
       @return VHI_* flags */
   u32t  _exicc (*info     )(vio_mode_info **modes, char *prnname);
   /** set mode.
       By default, 80x25 mode is used.
       Buffer is NOT cleared when extmem supplied, but extmem parameter can
       be denied by implementation.

       Color is white and cursor position = 0,0 on exit.

       @param   width   Mode width (40..32768)
       @param   height  Mode height (25..)
       @param   extmem  External pointer for buffer, can be 0.
       @return error code or 0 */
   qserr _exicc (*setmode  )(u32t width, u32t height, void *extmem);
   /// set mode by mode id
   qserr _exicc (*setmodeid)(u16t modeid);
   /** reset output to 80x25.
       Function sets mode or just clears the screen.
       All output parameters are dropped to default values on exit. */
   qserr _exicc (*reset    )(void);
   /// clear screen
   void  _exicc (*clear    )(void);

   qserr _exicc (*getmode  )(u32t *modex, u32t *modey, u16t *modeid);

   void  _exicc (*setcolor )(u16t color);

   void  _exicc (*setpos   )(u32t line, u32t col);
   /** set output options.
       @param   opts    flags (VHSET_*)
       @return 0 on success or error code on incorrect parameter */
   qserr _exicc (*setopts  )(u32t opts);
   /** query current output state.
       @param   sb      Buffer for returning information.
       @return VBM_* flags combination */
   qserr _exicc (*state    )(vh_settings *sb);
   /** print char to current cursor position.
       @param   ch      characted to print.
       @return 1 if new line was started (\n or end of line) */
   u32t  _exicc (*charout  )(char ch);
   /** print string to the current cursor position.
       Note, that this is "low level" function and it ignore ANSI sequences!
       @param   str     string to print.
       @return number of new lines occured (both \n and end of line), i.e.
               number of scrolled lines. */
   u32t  _exicc (*strout   )(const char *str);
   /** write rect buffer to the screen.
       @return error code or 0 */
   qserr _exicc (*writebuf )(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
   /** copy rect from screen to buffer.
       @return error code or 0 */
   qserr _exicc (*readbuf  )(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
   /** return direct pointer to text memory.
       Function can be usupported by implemenation.
       @return text buffer or zero */
   u16t* _exicc (*memory   )(void);
   /** draws a rectangle.
       @param   col     start column
       @param   line    start line
       @param   width   rectangle width
       @param   height  rectangle height
       @param   ch      character, use 0 for default (space)
       @param   color   color value */
   void  _exicc (*fillrect )(u32t col, u32t line, u32t width, u32t height, char ch, u16t color);
} _qs_vh, *qs_vh;

#define QS_VH_BUFFER  "qs_viobuf"  ///< vio buffer class

/// @name qs_vh options
//@{
#define VHSET_WRAPX   0x00010000   ///< move cursor to col 0 instead of new line
#define VHSET_BLINK   0x00020000   ///< blink instead of intensity (vga only)
#define VHSET_SHAPE   0x00040000   ///< low word contain cursor shape value
#define VHSET_LINES   0x80000000   ///< bit means that remain 31 is a line counter value
//@}

/// @name qs_vh abilities
//@{
#define VHI_ANYMODE       0x0001   ///< class MAY accept not only listed modes
#define VHI_MEMORY        0x0002   ///< memory() function supported
#define VHI_BLINK         0x0004   ///< blink supported
#define VHI_SLOW          0x0008   ///< slow output: writebuf optimization
//@}

/** clone entire state of src.
    Function copies mode, color, cursor position, screen data and
    ttylines counter.
    @param   mode_id  Non-zero value allow to use setmode/setmodeid call
                      to make mode resolution equal to the source. SE_CLONE_MODE
                      will force setmode call and any other assumed as mode id
                      for setmodeid call. Mode id should be valid value for
                      dst instance.
                      For the zero value function returns error on mode mismatch.
    @return  error code or 0 */
qserr     _std se_clone(qs_vh src, qs_vh dst, u16t mode_id);

#define SE_CLONE_MODE     0xFFFF

/** internal call for device substitution.
    Function checks new handler and enum existing sessions to verify - is new
    device able to support all of them?
    Crazy code, actually, but called on every CONSOLE load/unload.
    @return 0 on success or error code. */
qserr     _std se_deviceswap(u32t dev_id, qs_vh np, qs_vh *op);

/** return qs_viobuf instance with a copy of session`s screen.
    returned instance is process-owned and should be DELETE(d), zero returned
    on incorrect session number */
qs_vh     _std se_screenshot(u32t sesno);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VIO_INT_CLASSES

