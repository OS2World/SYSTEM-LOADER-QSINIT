//
// QSINIT
// screen support dll: internal refs
//
#ifndef CONDLL_INTERNAL
#define CONDLL_INTERNAL

#include "qstypes.h"
#include "vbedata.h"
#include "dpmi.h"
#include "console.h"
#include "graphdef.h"

#define PREDEFINED_MODES    3   // number of predefined text modes
#define TEXTEMU_MODES       2   // maximum number of emulated text modes
#define MAXVESA_MODES     256

#define TEXTMEM_SEG    0xB800

#define BytesBP(bits) ((bits) + 7 >> 3)
#define BytesPerFont(pix) (pix<=9?1:(pix>16?4:2))

typedef struct {
   int          vesaref;
   u32t         modenum;
} modeinfo;

u32t int10h(struct rmcallregs_s *regs);
#ifdef __WATCOMC__
#pragma aux int10h = \
   "mov   ax,300h"   \
   "mov   bx,10h"    \
   "xor   ecx,ecx"   \
   "int   31h"       \
   "setnc al"        \
   "movzx eax,al"    \
   parm  [edi]       \
   value [eax]       \
   modify [ebx ecx];
#endif // __WATCOMC__

#define MoveBlock(Sour,Dest,d_X,d_Y,LineLen_S,LineLen_D) {  \
  u8t *dest=(u8t*)(Dest), *sour=(u8t*)(Sour);               \
  u32t d_y=d_Y,d_x=d_X,l_d=LineLen_D,l_s=LineLen_S;         \
  for (;d_y>0;d_y--) {                                      \
    memcpy(dest,sour,d_x); dest+=l_d; sour+=l_s;            \
  }                                                         \
}

/** query all available vesa modes
    @param         vinfo    array for modes (256 entries)
    @param         minfo    mode info array for mode numbers
    @param         mmask    bitmask for color modes: bit 0 = 8 bit, 
                            bit 1 = 15 bit, bit 2 = 16 bit, bit 3 = 24 bit,
                            bit 4 = 32 bit, bit 5 = 1 bit,  bit 6 = 4 bit.
                            By default 0x5F is used.
    @param   [out] memsize  Total video memory size, can be 0
    @return  number of supported modes (<=256) */
u32t  vesadetect(VBEModeInfo *vinfo, modeinfo *minfo, u32t mmask, u32t *memsize);

/// call mode info for single mode
u32t  vesamodeinfo(VBEModeInfo *vinfo, u16t mode);

/// set mode
u32t  vesasetmode(VBEModeInfo *vinfo, u16t mode, u32t flags);

/** write read pixel data.
    @attention warning! this function does not check coordinates!
    @param   vinfo     current mode info
    @param   buf       buffer with in/out data
    @param   pitch     buffer line length, required
    @param   write     action: 1 = write to screen, 0 - read from screen
    @return true if success */
u32t  vesamemcpy(con_modeinfo *mi, VBEModeInfo *vinfo, u32t x, u32t y, 
                 u32t dx, u32t dy, void *buf, u32t pitch, int write);

/// scroll vesa screen by "lines" lines
u32t  vesascroll(con_modeinfo *mi, u32t ys, u32t yd, u32t lines);

/// fill number of screen lines by color value
u32t  vesaclear(con_modeinfo *mi, u32t ypos, u32t lines, u32t color);

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

/// vio emulation mode was set
void _std evio_newmode();

/// close emulation
void _std evio_shutdown();

// select bank
void _std con_selectbank(int window, unsigned long winpos);

// low level text output call.
void _std con_drawchar(con_drawinfo *cdi, u32t color, u32t bgcolor, u32t *chdata,
                       u8t* mempos, u32t pitch, int cursor);

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
