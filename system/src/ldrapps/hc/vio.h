//
// QSINIT
// text mode & keyboard support
//
#ifndef QSINIT_VIO
#define QSINIT_VIO

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** print char to current cursor position.
    @param   ch characted to print.
    @return 1 if new line was started (\n or end of line) */
u32t  _std vio_charout(char ch);

/** print string to the current cursor position.
    @param   str  string to print.
    @return actual number of new lines (both \n and the end of screen) occured
            during the source string output. */
u32t  _std vio_strout(const char *str);

/** print string to the current cursor position with ^A..^_ special support.
    Function shows x01..x1F as *green* A.._ characters.

    @param   str  string to print.
    @return actual number of new lines (both \n and the end of screen) occured
            during the source string output. */
u32t  _std vio_strout_spec(const char *str);

/// clear screen
void  _std vio_clearscr(void);

/// set current cursor posistion
void  _std vio_setpos(u8t line, u8t col);

/// get current cursor posistion
void  _std vio_getpos(u32t *line, u32t *col);

/** get number of printed lines after last mode change.
    Counter is zero after mode change or vio_resetmode() call.
    @param  nval   replace counter to this value. Negative value
                   (-1) means only query.
    @return number of printed lines. */
u32t  _std vio_ttylines(s32t nval);

/** copy data to screen.
    @param  col    start column
    @param  line   start line
    @param  width  rectangle width
    @param  height rectangle height
    @param  buf    src buffer (array of dw in text mode screen format)
    @param  pitch  length of single line in buffer (in bytes), can be 0 for
                   default value (width*2) */
void  _std vio_writebuf(u32t col, u32t line, u32t width, u32t height,
                        void *buf, u32t pitch);

/** copy data from screen.
    @param  col    start column
    @param  line   start line
    @param  width  rectangle width
    @param  height rectangle height
    @param  buf    dst buffer (array of dw in text mode screen format)
    @param  pitch  length of single line in buffer (in bytes), can be 0 for
                   default value (width*2) */
void  _std vio_readbuf (u32t col, u32t line, u32t width, u32t height,
                        void *buf, u32t pitch);

/** draws a rectangle.
    @param   col    start column
    @param   line   start line
    @param   width  rectangle width
    @param   height rectangle height
    @param   ch     character, use 0 for default (space)
    @param   color  color value */
void  _std vio_fillrect(u32t col, u32t line, u32t width, u32t height,
                        char ch, u16t color);

/** draws a "shadow".
    Function draws a "shadow" at the botton right corner of specified
    rectangle. Shadow is just grayed screen content at this position.
    Shadow occupy one line starting at line+height and one columns at
    col+width.
    @param   col    start column
    @param   line   start line
    @param   width  rectangle width
    @param   height rectangle height */
void _std vio_drawshadow(u32t col, u32t line, u32t width, u32t height);

#define VIO_SHAPE_FULL      0x00    ///< full-size
#define VIO_SHAPE_HALF      0x01    ///< bottom half of character
#define VIO_SHAPE_WIDE      0x02    ///< wide line
#define VIO_SHAPE_LINE      0x03    ///< single line
#define VIO_SHAPE_NONE      0x04    ///< no cursor

/** get current cursor shape.
    @return VIO_SHAPE_* constant. */
u16t  _std vio_getshape(void);

/** set one of default cursor shapes.
    @param shape   type (VIO_SHAPE_* const) */
void  _std vio_setshape(u16t shape);

#define VIO_COLOR_BLACK     0x000
#define VIO_COLOR_BLUE      0x001
#define VIO_COLOR_GREEN     0x002
#define VIO_COLOR_CYAN      0x003
#define VIO_COLOR_RED       0x004
#define VIO_COLOR_MAGENTA   0x005
#define VIO_COLOR_BROWN     0x006
#define VIO_COLOR_WHITE     0x007
#define VIO_COLOR_GRAY      0x008
#define VIO_COLOR_LBLUE     0x009
#define VIO_COLOR_LGREEN    0x00A
#define VIO_COLOR_LCYAN     0x00B
#define VIO_COLOR_LRED      0x00C
#define VIO_COLOR_LMAGENTA  0x00D
#define VIO_COLOR_YELLOW    0x00E
#define VIO_COLOR_LWHITE    0x00F

#define VIO_COLOR_RESET     VIO_COLOR_WHITE   ///< reset color to default

/** set output color for console mode.
    @param color   bgrd<<4|text color */
void  _std vio_setcolor(u16t color);

/// get current color
u16t  _std vio_getcolor(void);

/** call bios to set 80x25, 80x43 or 80x50 mode.
    @param lines   Number of lines (25,43 ot 50). */
void  _std vio_setmode(u32t lines);

/** set console mode.
    Actually, function can set non-80 cols modes only after loading CONSOLE
    module, but it allow to use it transparenly (without static linking).

    @param cols           Number of columns cols (80).
    @param lines          Number of lines (25,43 ot 50).
    return success flag (1/0) */
int   _std vio_setmodeex(u32t cols, u32t lines);

/** check current videomode and reset it to 80x25.
    Function checks current video and if it equal to 80x25 - clears screen,
    else resets mode to 80x25 */
void  _std vio_resetmode(void);

/** get current text mode.
    @param [out] cols     Number of columns (80,132,etc)
    @param [out] lines    Number of lines (25,43,50,etc)
    @return 1 on success, else current mode is not text. */
u8t   _std vio_getmode(u32t *cols, u32t *lines);

typedef struct {
   u32t         size;
   u16t           mx;    ///< mode width
   u16t           my;    ///< mode height
   u16t        fontx;    ///< font width (in dots)
   u16t        fonty;    ///< font height (in dots)
   u32t        flags;    ///< mode information flags
   u32t      mode_id;    ///< unique mode identifier over system
   /* data starting from this point provided for graphics modes only,
      when CONSOLE module is active */
   u32t          gmx;
   u32t          gmy;
   u32t       gmbits;    ///< bits per pixel
   u32t        rmask;    ///< red color mask (bpp>=15)
   u32t        gmask;    ///< green color mask (bpp>=15)
   u32t        bmask;    ///< blue color mask (bpp>=15)
   u32t        amask;    ///< reserved bits mask (bpp>=15)
   u32t      gmpitch;    ///< graphics mode line length
} vio_mode_info;

#define VIO_MI_SHORT   (FIELDOFFSET(vio_mode_info,gmx))
#define VIO_MI_FULL    (sizeof(vio_mode_info))

/// @name vio_mode_info.flags bits
//@{
#define VMF_GRAPHMODE        0x00001   ///< graphic mode
#define VMF_VGAPALETTE       0x00002   ///< VGA palette present (8 bit graphic modes)
#define VMF_FONTx2           0x00100   ///< font scaling x2
#define VMF_FONTx3           0x00200   ///< font scaling x3
#define VMF_FONTx4           0x00300   ///< font scaling x4
#define VMF_FONTxMASK        0x00300   ///< font scaling mask
//@}

/** get video mode(s) information.
    Function return information about one or more (or all of available) video
    modes. Note, that mx, my and dev_id values are used as a filters for the
    entire mode list.
    @param mx             text mode width, use 0 for any available width
    @param my             text mode height, use 0 for any available height
    @param dev_id         use -1 for mode on any device, else - device id
                          (0 - screen).
    @return one or more mode information records in user`s heap buffer (must
            be free()-ed). The end of list indicated by vio_mode_info.size
            field = 0. */
vio_mode_info* _std vio_modeinfo(u32t mx, u32t my, int dev_id);

/** set mode using unique mode_id value.
    Note, that mode_id is a device-specific value, this mean what if another
    device, attached to current session has no mode with the same screen
    resolution - it will be detached (i.e. session will lost output to this
    device).
    mode_id can be queried via vio_modeinfo() mode enumeration.
    return 0 on success or error code */
qserr _std vio_setmodeid(u32t mode_id);

/** query mode information by mode_id value.
    Note, that mode_id is a device-specific value.
    mib->size should be set to VIO_MI_SHORT or VIO_MI_FULL before the call.
    return 0 on success or error code */
qserr _std vio_querymodeid(u32t mode_id, vio_mode_info *mib);

/** set intensity or blink for text mode background.
    After mode init INTENSITY is selected (unlike BIOS).
    Note, that EFI & graphic console modes just ignore intensity changes and
    have no blink support.

    @param value    Use 1 for intensity and 0 for blink */
void  _std vio_intensity(u8t value);

/** infinite wait for a key press.
    @return pair scan-code<<8|character (int 16h ah=10h) */
u16t  _std key_read(void);

/// is key pressed?
u8t   _std key_pressed(void);

/** wait key with timeout.
    @param seconds        seconds to wait for.
    @return pressed key code or 0 */
u16t  _std key_wait(u32t seconds);

/** wait key with timeout.
    Be careful, key_wait() uses seconds, but this function - milliseconds.

    @param       ms       time to wait for, ms. Value of 0xFFFFFFFF means
                          forever.
    @param [out] status   ptr to receive keyboard status at the moment of
                          key press, can be 0.
    @return key code or 0 */
u16t  _std key_waitex(u32t ms, u32t *status);

/** clear keyboard buffer.
    Function clears keyboard queue for the current session.
    @return number of abandoned keystrokes */
u32t  _std key_clear(void);

/** set keyboard rate and delay.
    @param rate    0=30 rpts/sec, 1=26...1Fh=2.
    @param delay   0=250ms, 1=500ms, 2=750ms, 3=1 second */
void  _std key_speed(u8t rate, u8t delay);

/** get keyboard rate and delay.
    @param [out] rate     ptr to rate, can be 0
    @param [out] delay    ptr to delay, can be 0 */
void  _std key_getspeed(u8t *rate, u8t *delay);

/// @name result of key_status (bit values)
//@{
#define KEY_SHIFT       0x000001
#define KEY_ALT         0x000002
#define KEY_CTRL        0x000004
#define KEY_NUMLOCK     0x000008
#define KEY_CAPSLOCK    0x000010
#define KEY_SCROLLLOCK  0x000020
#define KEY_SHIFTLEFT   0x000040
#define KEY_SHIFTRIGHT  0x000080
#define KEY_CTRLLEFT    0x000100
#define KEY_CTRLRIGHT   0x000200
#define KEY_ALTLEFT     0x000400
#define KEY_ALTRIGHT    0x000800
#define KEY_SYSREQ      0x001000
#define KEY_INSERT      0x002000
//@}

/// special code for Ctrl-Break (available in BIOS version only)
#define KEYC_CTRLBREAK    0xFE00

/// get system keys status
u32t  _std key_status(void);

/** push key press to keyboad buffer.
    @param code    Pair scancode<<8|character
    @return 1 on success. */
u8t   _std key_push(u16t code);

/** pc speaker sound.
    Function is non-blocking and exit after sound start, next call will
    stop previous unfinished sound.

    @param freq    frequency, Hz
    @param ms      beep length, ms */
void  _std vio_beep(u16t freq, u32t ms);

/// is sound playing now?
u8t   _std vio_beepactive(void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VIO
