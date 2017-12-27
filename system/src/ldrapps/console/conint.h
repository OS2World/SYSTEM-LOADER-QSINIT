//
// QSINIT
// screen support dll: internal refs
//
#ifndef CONDLL_INTERNAL
#define CONDLL_INTERNAL

#include "qsbase.h"
#include "console.h"
#include "graphdef.h"
#include "qcl/qslist.h"
#include "qsvdata.h"
#include "malloc.h"
#include "qsint.h"

#define MAX_MODES              2048

#define BytesBP(bits) ((bits) + 7 >> 3)
#define BytesPerFont(pix) (pix<=9?1:(pix>16?4:2))

/// @name con_setmode flags
//@{
#define CON_GRAPHMODE        0x00001   ///< graphic mode
#define CON_NOSCREENCLEAR    0x00002   ///< do not clear screen
#define CON_COLOR_1BIT       0x00000   ///< mono graphic mode
#define CON_COLOR_4BIT       0x00100   ///< 16 colors graphic mode
#define CON_COLOR_8BIT       0x00200   ///< 256 colors graphic mode
#define CON_COLOR_15BIT      0x00300   ///< 64k colors graphic mode
#define CON_COLOR_16BIT      0x00400   ///< 64k colors graphic mode
#define CON_COLOR_24BIT      0x00500   ///< 16m colors graphic mode (24 bits)
#define CON_COLOR_32BIT      0x00600   ///< 16m colors graphic mode (32 bits)
#define CON_EMULATED         0x01000   ///< emulated text mode (info flag only)
/// mode with shadow buffer (forced on EFI and non-LFB VESA modes)
#define CON_SHADOW           0x02000
#define CON_INDEX_MASK       0xF0000   ///< index (0..15) for modes with same size/colors
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
   u32t         flags;   ///< flags for con_setmode
   u32t         width;   ///< mode width
   u32t        height;   ///< mode height
   u32t          bits;   ///< number of bits
   u32t         rmask;   ///< red color mask
   u32t         gmask;   ///< green color mask
   u32t         bmask;   ///< blue color mask
   u32t         amask;   ///< reserved bits mask
   u32t        font_x;   ///< character width (for text modes)
   u32t        font_y;   ///< character height (for text modes)
   u32t      physaddr;   ///< physical address of video memory, can be 0
   u8t       *physmap;   ///< physical address, mapped to FLAT (only when mode active)
   u32t      mempitch;   ///< video memory line length
   u8t        *shadow;   ///< address of shadow buffer (if present and mode active)
   u32t   shadowpitch;   ///< shadow buffer line length
   u32t       modenum;   ///< host mode number
   int       realmode;   ///< real mode for emulated modes (graphic console)
} modeinfo;

typedef struct {
   int     x;
   int     y;
   u8t   bin[];
} fontbits;

#define MoveBlock(Sour,Dest,d_X,d_Y,LineLen_S,LineLen_D) {  \
  u8t *dest=(u8t*)(Dest), *sour=(u8t*)(Sour);               \
  u32t d_y=d_Y,d_x=d_X,l_d=LineLen_D,l_s=LineLen_S;         \
  for (;d_y>0;d_y--) {                                      \
    memcpy(dest,sour,d_x); dest+=l_d; sour+=l_s;            \
  }                                                         \
}

/// set mode
typedef u32t _std (*platform_setmode)  (u32t x, u32t y, u32t flags);
/// set mode by index
typedef u32t _std (*platform_setmodeid)(u32t mode_id);
/// leave current mode callback
typedef void _std (*platform_leavemode)(void);
/// blit memory<->video
typedef u32t _std (*platform_copy)     (u32t mode, u32t x, u32t y, u32t dx, 
                                        u32t dy, void *buf, u32t pitch, int write);
/// scroll screen lines
typedef u32t _std (*platform_scroll)   (u32t mode, u32t ys, u32t yd, u32t lines);
/// clear screen lines
typedef u32t _std (*platform_clear)    (u32t mode, u32t x, u32t y, u32t dx, u32t dy,
                                        u32t color);
/// flush shadow buffer to screen (if available)
typedef u32t _std (*platform_flush)    (u32t mode, u32t x, u32t y, u32t dx, u32t dy);
/// install native fonts (only BIOS fonts now, actually);
typedef void _std (*platform_addfonts) (void);
/// fill mode list array
typedef void _std (*platform_setup)    (void);
/// release resourses on module unloading
typedef void _std (*platform_close)    (void);
/** optional direct blit call.
    This call required only if common_flush_nofb() is used as pl_flush
    (i.e. only when no direct access to video memory).
    All parameters is verified before this call. */
typedef u32t _std (*platform_dirblit)  (u32t x, u32t y, u32t dx, u32t dy, void *src,
                                        u32t srcpitch);
/** optional direct clear call.
    This call required only if common_clear_nofb() is used as pl_clear
    (i.e. only when no direct access to video memory).
    All parameters is verified before this call. */
typedef u32t _std (*platform_dirclear) (u32t x, u32t y, u32t dx, u32t dy, u32t color);

extern platform_setmode      pl_setmode;
extern platform_setmodeid  pl_setmodeid;
extern platform_leavemode  pl_leavemode;
extern platform_copy            pl_copy;
extern platform_flush          pl_flush;
extern platform_scroll        pl_scroll;
extern platform_clear          pl_clear;
extern platform_addfonts     pl_addfont;
extern platform_dirblit      pl_dirblit;
extern platform_dirclear    pl_dirclear;
extern platform_setup          pl_setup;
extern platform_close          pl_close;

extern u32t                   vmem_addr; // MAY be known
extern u32t                   vmem_size;

extern vio_handler                *cvio;
/// native console mode active (vh_vio)
extern int                    in_native;

/// current mode index in mode info
extern int                 current_mode;
/// current REAL mode index in mode info
extern int                    real_mode;
/// mode info array
extern modeinfo*       modes[MAX_MODES];
/// number of modes available
extern u32t                    mode_cnt;
/// install text modes only ("VESA = NO" environment key present)
extern int                     textonly;
/// highest allowed resolutions (limited by "VESA" environment key too)
extern u32t    modelimit_x, modelimit_y;
/** bit mask of enabled color resolutions.
    bit 0 = 8 bit, bit 1 = 15, bit 2 = 16, bit 3 = 24, bit 4 = 32,
    bit 5 = 1 bit, bit 6 = 4.
    By default 0x5F is used. */
extern u32t            enabled_modemask;
/** size of modeinfo array.
    ptr to modeinfo assumed as const and returned to user as array,
    so max number of modes limited by this preallocated value */
extern u32t                  mode_limit;
/// mode was changed by lib, at least once.
extern int                 mode_changed;
/// current mode flags
extern u32t               current_flags;
/// list of font arrays
extern ptr_list                  sysfnt;
/// usage of "physmap" enabled (EFI) / LFB enabled (BIOS)
extern u32t              fbaddr_enabled;
/// forced memory address
extern u32t               fbaddr_forced;
/// forced memory size (with fbaddr_forced only)
extern u32t                 fbaddr_size;

/// init VESA output function
int   plinit_vesa(void);

/// init EFI output function
int   plinit_efi(void);

/// allocate shadow buffer for specified mode
int   alloc_shadow(modeinfo *cmi, int noclear);

/// search mode number
int   search_mode(u32t x, u32t y, u32t flags);

/// free all assigned to current mode arrays
void  con_unsetmode();

/// get bit for "enabled_modemask" from Bits_Per_Pixel value.
int   bpp2bit(u32t bpp);

/// query 8x8, 8x14, 9x14, 8x16, 9x16 bios font data
void  con_queryfonts(void);

/** build 32-bit array font with font.
    @param   width     font width
    @param   height    font height
    @return malloc-ed array */
void *con_buildfont(int width, int height);

/// free bios font data
void  con_freefonts(void);

/// install vio device
int   con_catchvio(void);

/// try to remove remove vio device
int   con_releasevio(void);

void  con_addfontmodes(int width, int height);

/// emu text mode screen write
//void _std evio_writebuf(u32t col, u32t line, u32t width, u32t height, 
//                        void *buf, u32t pitch);

/// emu text mode screen read
//void _std evio_readbuf (u32t col, u32t line, u32t width, u32t height, 
//                        void *buf, u32t pitch);

// low level text output call.
void _std con_drawchar(con_drawinfo *cdi, u32t color, u32t bgcolor, u32t *chdata,
                       u8t* mempos, u32t pitch, int cursor);

/// vio emulation mode was set
void evio_newmode();

/// close emulation
void evio_shutdown();

/// common copy to shadow and/or frame buffer if available at least one of them
u32t common_copy      (u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf,
                       u32t pitch, int write);

/** common clear of shadow and/or frame buffer.
    Note that x=0 and dx=0 mean entire line here and in common_clear_nofb() */
u32t common_clear     (u32t mode, u32t x, u32t y, u32t dx, u32t dy, u32t color);

/** common clear of shadow and/or frame buffer.
    "No FB pointer" supported, but requires pl_dirclear for it */
u32t common_clear_nofb(u32t mode, u32t x, u32t y, u32t dx, u32t dy, u32t color);

/// common scroll screen lines (in shadow and/or frame buffer)
u32t common_scroll    (u32t mode, u32t ys, u32t yd, u32t lines);

/** common scroll screen lines.
    "No FB pointer" variant. Function calls common_scroll()
    to scroll shadow and then platform flush to repaint screen */
u32t common_scroll_nofb(u32t mode, u32t ys, u32t yd, u32t lines);

/// common shadow buffer flush (if both shadow & frame exists)
u32t common_flush      (u32t mode, u32t x, u32t y, u32t dx, u32t dy);

/** common shadow buffer flush (if both shadow & frame exists)
    "No FB pointer" supported, but requires pl_dirblit for it */
u32t common_flush_nofb (u32t mode, u32t x, u32t y, u32t dx, u32t dy);

/** fill video memory.
    @param   Dest       start address
    @param   d_X        width in pixels
    @param   d_Y        height in pixels
    @param   LineLen_D  line length, required
    @param   Color      color value
    @param   vbpps      bytes per pixel, must be 1..4 only */
void _std con_fillblock(void *Dest, u32t d_X, u32t d_Y, u32t LineLen_D, 
                        u32t Color, u32t vbpps);

typedef char GIFPalette[256][3];

void*     con_writegif (int *ressize, void *srcdata, int x, int y,
                        GIFPalette *plt, int trcolor, const char *copyright);

#endif // CONDLL_INTERNAL
