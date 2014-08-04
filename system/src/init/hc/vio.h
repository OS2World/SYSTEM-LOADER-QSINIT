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
u32t _std vio_charout(char ch);

/** print string to current cursor position.
    @param   str  string to print.
    @return number of new lines occured (both \n and end of line), i.e. number
    of scrolled lines. */
u32t _std vio_strout(const char *str);

/// clear screen
void _std vio_clearscr(void);

/// set current cursor posistion
void _std vio_setpos(u8t line, u8t col);

/// get current cursor posistion
void _std vio_getpos(u32t *line, u32t *col);

/** copy data to screen.
    @param  col    start column
    @param  line   start line
    @param  width  rectangle width
    @param  height rectangle height
    @param  buf    src buffer (array of dw in text mode screen format)
    @param  pitch  length of single line in buffer (in bytes), can be 0 for
                   default value (width*2) */
void _std vio_writebuf(u32t col, u32t line, u32t width, u32t height, 
                       void *buf, u32t pitch);

/** copy data from screen.
    @param  col    start column
    @param  line   start line
    @param  width  rectangle width
    @param  height rectangle height
    @param  buf    dst buffer (array of dw in text mode screen format)
    @param  pitch  length of single line in buffer (in bytes), can be 0 for
                   default value (width*2) */
void _std vio_readbuf (u32t col, u32t line, u32t width, u32t height, 
                       void *buf, u32t pitch);

/** set cursor shape.
    @param start   cursor start line (bit 5 == 1 then cursor hidden)
    @param end     cursor end line */
void _std vio_setshape(u8t start, u8t end);

/** get current cursor shape.
    @return cursor shape word (in the same way as in BIOS and vio_setshape func). */
u16t _std vio_getshape(void);

#define VIO_SHAPE_FULL      0x00    ///< full-size
#define VIO_SHAPE_HALF      0x01    ///< bottom half of character
#define VIO_SHAPE_WIDE      0x02    ///< wide line
#define VIO_SHAPE_LINE      0x03    ///< single line
#define VIO_SHAPE_NONE      0x04    ///< no cursor

/** set one of default cursor shapes.
    @param shape   type (VIO_SHAPE_* const) */
void _std vio_defshape(u8t shape);

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

#define VIO_COLOR_RESET     0x107       ///< reset color to default

/** set current cursor posistion.
    Specify 0x1xx in "color" to turn colored output off (but low 8 bits still 
    will be used to fill new lines color attribute after scroll).
    @param color   text/bgrd color */
void _std vio_setcolor(u16t color);

/** call bios to set 80x25, 80x43 or 80x50 mode.
    @param lines   Number of lines (25,43 ot 50). */
void _std vio_setmode(u32t lines);

/** set text mode.
    Actually, function can set non-80 cols modes only after loading CONSOLE
    module, but it allow to use it transparenly (without static linking).

    @param cols    Number of columns cols (80).
    @param lines   Number of lines (25,43 ot 50).
    return success flag (1/0) */
int  _std vio_setmodeex(u32t cols, u32t lines);

/** check current videomode and reset it to 80x25.
    Function check current video and if it 80x25 - clear screen, else reset
    mode to 80x25 */
void _std vio_resetmode(void);

/** get current text mode.
    This function must be called after manual mode switch via direct int 10h
    call to update vio_* functions internal info.

    @param [out] cols    Number of columns (40,80,132)
    @param [out] lines   Number of lines (25,43,50,etc)
    @return 1 on success, else current mode is not text. */
u8t  _std vio_getmode(u32t *cols, u32t *lines);

/** set intensity or blink for text mode background.
    After mode init INTENSITY is selected (unlike BIOS).
    @param value    Use 1 for intensity and 0 for blink */
void _std vio_intensity(u8t value);

/** read key.
    @return pair scan-code<<8|character (int 16h ah=10h)
    @see key_wait() */
u16t _std key_read(void);

/// is key pressed?
u8t  _std key_pressed(void);

/** wait key with timeout.
    @param seconds Seconds to wait for.
    @return key pressed or 0
    @see key_read() */
u16t _std key_wait(u32t seconds);

/** allow hlt execution while key waiting.
    @param  on  Boolean flag (1/0).
    @return previous state. */
int  _std key_waithlt(int on);

/** set keyboard rate and delay.
    @param rate    0=30 rpts/sec, 1=26...1Fh=2.
    @param delay   0=250ms, 1=500ms, 2=750ms, 3=1 second */
void _std key_speed(u8t rate, u8t delay);

/** get keyboard rate and delay.
    @param [out] rate   ptr to rate, can be 0 
    @param [out] delay  ptr to delay, can be 0 */
void _std key_getspeed(u8t *rate, u8t *delay);

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

/// get system keys status
u32t _std key_status(void);

/** push key press to keyboad buffer.
    @param code    Pair scancode<<8|character
    @return 1 on success. */
u8t  _std key_push(u16t code);

/** pc speaker sound.
    Function is non-blocking and exit after sound start, next call will 
    stop previous unfinished sound.

    @param freq    frequency, Hz
    @param ms      beep length, ms */
void _std vio_beep(u16t freq, u32t ms);

/// is sound playing now?
u8t  _std vio_beepactive(void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VIO
