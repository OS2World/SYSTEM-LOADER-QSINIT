;
; QSINIT
; vesa info structure for new kernel
;

VesaInfoBuf     struc
ModeNum         dw      ?       ; mode number
ColorBits       dw      ?       ; 8/15/16/24/32
SuppotedModes   dw      ?

win4write       db      ?
win4read        db      ?
win_gran        dd      ?
win_size        dd      ?

Mode15          dw      ?
Mode16          dw      ?
Mode24          dw      ?
Mode32          dw      ?

FontAddr        dd      ?       ; 8x16 font linear addr - 0xA0000
WinAAddr        dd      ?       ; window A linear addr - 0xA0000
WinBAddr        dd      ?       ; window B linear addr - 0xA0000
LineLen         dd      ?       ; selected mode pitch
LFBAddr         dd      ?       ; LFB physical addr, if available
FontCopy        dd      ?       ; linear addr of 128 low chars 8x16 font
                                ; but only if BOOTFLAG_VESAFNT specified in ldrinfo
VesaInfoBuf     ends

; bit flags for VesaInfoBuf.ColorBits field
M640x480x8      equ       1
M640x480x15     equ       2
M640x480x16     equ       4
M640x480x24     equ       8
M640x480x32     equ      10h

