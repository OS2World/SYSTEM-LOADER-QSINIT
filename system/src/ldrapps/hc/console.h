#ifndef QS_MOD_CONSOLE
#define QS_MOD_CONSOLE

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Query video memory size and address.
    @param  [out] fbaddr   Video memory address, can be 0. Returned value may
                           be zero too - if address is unknown.
    @return memory size in bytes.*/
u32t _std con_vmemsize(u64t *fbaddr);

typedef struct {
   u32t         size;
   u32t        flags;    ///< mode information flags (see VMF_* in vio.h)
   u32t      mode_id;    ///< mode id 
   u16t           mx;    ///< mode width
   u16t           my;    ///< mode height
   u32t         bits;    ///< bits per pixel
   u32t        pitch;    ///< mode line length
   u32t        rmask;    ///< red color mask (bpp>=15)
   u32t        gmask;    ///< green color mask (bpp>=15)
   u32t        bmask;    ///< blue color mask (bpp>=15)
   u32t        amask;    ///< reserved bits mask (bpp>=15)
} con_mode_info;

/** return real mode list.
    Only native modes are returned here.

    Note, that mode_id of graphic modes, returned in this list - may be
    accepted only by con_exitmode() function, vio_setmodeid() will deny it,
    because there is no "concept" of graphics mode now.

    @return mode information records in application owned heap block (must
            be free()-ed). The end of list indicated by con_mode_info.size
            field = 0. */
con_mode_info* _std con_gmlist(void);

/** set "shutdown" screen mode.
    @param   mode_id   mode to set after loader exit instead of restoration
                       of default mode. Pure graphic mode id from con_gmlist()
                       accepted here.
    @return error code or 0. */
qserr _std con_exitmode(u32t mode_id);

/** Set VGA palette.
    @param   pal       768 bytes buffer with palette (RGB RGB ...) */
void _std con_setvgapal(void *pal);

/** Get VGA palette.
    @param   pal       768 bytes buffer for pallete (RGB RGB ...) */
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

/** Return a copy of specified font.
    Note, that function return font on *exact* size match only!
    Even if con_fontavail() returns true (because graphic console able to use
    smaller font), this function may return zero.

    @param   width     font width (9==8 here)
    @param   height    font height
    @return zero if no font found or application owned heap block with binary data */
void*_std con_fontcopy(int width, int height);

/// refresh font cache for the current active mode
void _std con_fontupdate(void);

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
