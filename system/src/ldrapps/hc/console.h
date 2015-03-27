#ifndef QS_MOD_CONSOLE
#define QS_MOD_CONSOLE

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @name con_setmode flags
//@{
#define CON_GRAPHMODE         0x0001   ///< graphic mode
#define CON_NOSCREENCLEAR     0x0002   ///< do not clear screen
#define CON_COLOR_1BIT        0x0000   ///< mono graphic mode
#define CON_COLOR_4BIT        0x0100   ///< 16 colors graphic mode
#define CON_COLOR_8BIT        0x0200   ///< 256 colors graphic mode
#define CON_COLOR_15BIT       0x0300   ///< 64k colors graphic mode
#define CON_COLOR_16BIT       0x0400   ///< 64k colors graphic mode
#define CON_COLOR_24BIT       0x0500   ///< 16m colors graphic mode (24 bits)
#define CON_COLOR_32BIT       0x0600   ///< 16m colors graphic mode (32 bits)
#define CON_EMULATED          0x1000   ///< emulated text mode (info flag only)
#define CON_SHADOW            0x2000   ///< mode with shadow buffer
//@}

/** Set text or graphic video mode.
    @param  x      Mode width
    @param  y      Mode height
    @param  flags  Mode flags
    @return 1 on success */
u32t _std con_setmode(u32t x, u32t y, u32t flags);

/** Query mode presence.
    @param  x      Mode width
    @param  y      Mode height
    @param  flags  Mode flags
    @return 1 on success */
u32t _std con_modeavail(u32t x, u32t y, u32t flags);

typedef struct {
   u32t      infosize; ///< structure size
   u32t         flags; ///< flags for con_setmode
   u32t         width; ///< mode width
   u32t        height; ///< mode height
   u32t          bits; ///< number of bits
   u32t         rmask; ///< red color mask
   u32t         gmask; ///< green color mask
   u32t         bmask; ///< blue color mask
   u32t         amask; ///< reserved bits mask
   u32t        font_x; ///< character width (for text modes)
   u32t        font_y; ///< character height (for text modes)
   u32t       memsize; ///< size of video memory, can be 0 if physaddr==0
   u32t      physaddr; ///< physical address of video memory, can be 0
   u8t       *physmap; ///< physical address, mapped to FLAT (only when mode active)
   u32t      mempitch; ///< video memory line length
   u8t        *shadow; ///< address of shadow buffer (if present and mode active)
   u32t   shadowpitch; ///< shadow buffer line length
} con_modeinfo;

/** Query all available modes.
    @attention this is internal mode info array, actually, so use it as r/o.
    This pointer is constant value over all CONSOLE life time.

    @param  [out] modes    Number of available modes
    @return array of con_modeinfo structs */
con_modeinfo *_std con_querymode(u32t *modes);

/** Query current mode info.
    @return con_modeinfo struct */
con_modeinfo *_std con_currentmode(void);

/// Get current mode.
void _std con_getmode(u32t *x, u32t *y, u32t *flags);

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
    @param   name      shot file name, can be 0 to use default grab name and
                       path (screen shot grabbing must be ON).
    @param   mute      set 1 to prevent beep on saving
    @return 0 or file i/o errno value */
int  _std con_makeshot(const char *name, int mute);

#ifdef __cplusplus
}
#endif
#endif // QS_MOD_CONSOLE
