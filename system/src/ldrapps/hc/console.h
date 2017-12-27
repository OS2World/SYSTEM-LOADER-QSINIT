#ifndef QS_MOD_CONSOLE
#define QS_MOD_CONSOLE

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Query video memroy size and address.
    @param  [out] fbaddr   Video memory address, can be 0. Returned value may
                           be zero too - if address is unknown.
    @return memory size in bytes.*/
u32t _std con_vmemsize(u64t *fbaddr);

/** Write pixel data to screen.
    @param  x      Start x point
    @param  y      Start y point
    @param  dx     Number of columns
    @param  dy     Number of lines
    @param  src    Source data in current mode color format
    @param  pitch  Source data line length
    @return true if success */
u32t _std con_write(u32t x, u32t y, u32t dx, u32t dy, void *src, u32t pitch);

/** Read pixel data from screen.
    @param  x      Start x point
    @param  y      Start y point
    @param  dx     Number of columns
    @param  dy     Number of lines
    @param  src    Destination data buffer
    @param  pitch  Destination data line length
    @return true if success */
u32t _std con_read(u32t x, u32t y, u32t dx, u32t dy, void *dst, u32t pitch);

/** Fills rectangle on screen.
    @param  x      Start x point
    @param  y      Start y point
    @param  dx     Number of columns
    @param  dy     Number of lines
    @param  color  Color value in current mode color format
    @return true if success */
u32t _std con_clear(u32t x, u32t y, u32t dx, u32t dy, u32t color);

/** Set VGA palette.
    @param  pal    768 bytes buffer with palette (RGB RGB ...) */
void _std con_setvgapal(void *pal);

/** Get VGA palette.
    @param  pal    768 bytes buffer for pallete (RGB RGB ...) */
void _std con_getvgapal(void *pal);

/** Add or replace one of fonts for graphic console.
    Font must contain all 256 symbols. Width<=9 (yes, nine!) - assume byte per
    line in font, <=16 - word, <=32 dword.
    Replacement will occur only on next mode change.

    @param   width     font width
    @param   height    font height
    @param   data      font data, not required after call */
void _std con_fontadd(int width, int height, void *data);

/** Check presence of fonts with specific size.
    @param   width     font width
    @param   height    font height
    @return presence flag (1/0) */
u32t _std con_fontavail(int width, int height);

/** Add new "virtual console" mode.
    @param  fntx   font width
    @param  fnty   font height
    @param  modex  Mode width
    @param  modey  Mode height
    @return 0 on success, EINVAL on bad font size, ENODEV - no such mode, 
            EEXIST - mode with same fnt & screen resolution already exists, 
            ENOTTY - no installed font of this size, ENOSPC - mode info
            array is full */
int _std con_addtextmode(u32t fntx, u32t fnty, u32t modex, u32t modey);

/** Install screenshot handler.
    The same as MKSHOT ON shell command.

    @param   path      target directory for screen shots, can be 0 for current
                       dir.
    @return 0 or error value (ENOENT if dir not exist) */
int  _std con_instshot(const char *path);

/// remove screenshot handler
void _std con_removeshot(void);

/** make a screen shot.
    @param   sesno     session number
    @param   name      shot file name, can be 0 to use default grab name and
                       path (screen shot grabbing must be ON to use path).
    @param   mute      set 1 to prevent beep on saving
    @return 0 or file i/o errno value */
int  _std con_makeshot(u32t sesno, const char *name, int mute);

#ifdef __cplusplus
}
#endif
#endif // QS_MOD_CONSOLE
