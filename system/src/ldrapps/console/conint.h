//
// QSINIT
// screen support dll: internal refs
//
#ifndef CONDLL_INTERNAL
#define CONDLL_INTERNAL

#include "qstypes.h"
#include "console.h"
#include "graphdef.h"
#include "qcl/qslist.h"

#define PREDEFINED_MODES    3   // number of predefined text modes
#define TEXTEMU_MODES       2   // maximum number of emulated text modes
#define MAXVESA_MODES     128
#define MAXEMU_MODES       64

#define BytesBP(bits) ((bits) + 7 >> 3)
#define BytesPerFont(pix) (pix<=9?1:(pix>16?4:2))

///< internal portion of mode info
typedef struct {
   int          vesaref;        //< internal host data index
   u32t         modenum;        //< host mode number
   int         realmode;        //< real mode for emulated modes (graphic console)
} con_intinfo;

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
/// leave current mode callback
typedef void _std (*platform_leavemode)(void);
/// blit memory<->video
typedef u32t _std (*platform_copy)     (u32t mode, u32t x, u32t y, u32t dx, 
                                        u32t dy, void *buf, u32t pitch, int write);
/// scroll screen lines
typedef u32t _std (*platform_scroll)   (u32t mode, u32t ys, u32t yd, u32t lines);
/// clear screen lines
typedef u32t _std (*platform_clear)    (u32t mode, u32t ypos, u32t lines, u32t color);
/// flush shadow buffer to screenm (if available)
typedef u32t _std (*platform_flush)    (u32t mode, u32t x, u32t y, u32t dx, u32t dy);
/// install native fonts (actually, only BIOS fonts now);
typedef void _std (*platform_addfonts) (void);
/// fill mode list array
typedef void _std (*platform_setup)    (void);
/// release resourses on module unloading
typedef void _std (*platform_close)    (void);

extern platform_setmode      pl_setmode;
extern platform_leavemode  pl_leavemode;
extern platform_copy            pl_copy;
extern platform_flush          pl_flush;
extern platform_scroll        pl_scroll;
extern platform_clear          pl_clear;
extern platform_addfonts     pl_addfont;
extern platform_setup          pl_setup;
extern platform_close          pl_close;

/// current mode index in mode info
extern int                 current_mode;
/// current REAL mode index in mode info
extern int                    real_mode;
/// public mode info array
extern con_modeinfo              *modes;
/// internal mode info array
extern con_intinfo                *mref;
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
/// usage of "physmap" enabled (EFI only)
extern u32t              fbaddr_enabled;


/// init VESA output function
int   plinit_vesa(void);

/// init EFI output function
int   plinit_efi(void);

/// allocate shadow buffer for specified mode
void *alloc_shadow(con_modeinfo *cmi, int noclear);

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

/// install vio function hooks
void  con_insthooks(void);

/// remove vio function hooks
void  con_removehooks(void);

/// emu text mode screen write
void _std evio_writebuf(u32t col, u32t line, u32t width, u32t height, 
                        void *buf, u32t pitch);

/// emu text mode screen read
void _std evio_readbuf (u32t col, u32t line, u32t width, u32t height, 
                        void *buf, u32t pitch);

// low level text output call.
void _std con_drawchar(con_drawinfo *cdi, u32t color, u32t bgcolor, u32t *chdata,
                       u8t* mempos, u32t pitch, int cursor);

/// vio emulation mode was set
void evio_newmode();

/// close emulation
void evio_shutdown();

/// common copy to shadow and/or frame buffer if available at least one of them
u32t common_copy (u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf,
                       u32t pitch, int write);

/// common clear of shadow and/or frame buffer
u32t common_clear(u32t mode, u32t ypos, u32t lines, u32t color);

/// common scroll screen lines (in shadow and/or frame buffer)
u32t common_scroll(u32t mode, u32t ys, u32t yd, u32t lines);

/// common shadow buffer flush (if both shadow & frame exists)
u32t common_flush(u32t mode, u32t x, u32t y, u32t dx, u32t dy);

/** fill video memory.
    @param   Dest       start address
    @param   d_X        width in pixels
    @param   d_Y        height in pixels
    @param   LineLen_D  line length, required
    @param   Color      color value
    @param   vbpps      bytes per pixel, must be 1..4 only */
void _std con_fillblock(void *Dest, u32t d_X, u32t d_Y, u32t LineLen_D, 
                        u32t Color, u32t vbpps);

#endif // CONDLL_INTERNAL
