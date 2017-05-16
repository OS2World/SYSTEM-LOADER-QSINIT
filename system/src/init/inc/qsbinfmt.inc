;
; QSINIT
; binary dual-mode file format
;

;cc #ifndef QS_MKBIN_FMT
;cc #define QS_MKBIN_FMT

; include type defs only
;cc #if defined(__QSINIT__) || defined(QS_BASE_TYPES)
;cc #include "qstypes.h"
;cc #elif defined(EFIX64)
;cc #include "qsloader.h"
;cc #else
;cc #error I want my types!
;cc #endif

;  Runable real mode binary file with 32-bit PM part and fixups for it.
;
;  Code must do all things by self - set PM, unpack and fixup 32-bit part,
;  zero missed in file BSS parts and so on. File only provide storage for
;  fixups and compressed 32-bit code.
;
;  ----------------
;  | 16-bit object
;  ----------------
;  | MKBIN_HEADER
;  ----------------
;  | 32-bit object (compressed or not)
;  ---------------- 
;  | 32-bit fixups
;  ----------------
;
;  Alternatively - header can be placed somethere in 16-bit object. In this
;  case - converter check signature and use rmStored value from it.
;

MKBIN_SIGN      equ     30314642h  ; BF10

;cc #pragma pack(1)
mkbin_info      struc
mkbSign         dd      ?
mkbSize         dd      ?

rmSize          dw      ?          ;/< 16: total size
rmStored        dw      ?          ;/< 16: number of bytes, actually stored in file
pmSize          dd      ?          ;/< 32: total size
pmStored        dd      ?          ;/< 32: number of bytes, actually stored in file
pmCodeLen       dd      ?          ;/< 32: code obj.length (can be 0 if one obj in LX)
pmPacked        dd      ?          ;/< 32: total size (packed or not), with fixups
pmTotal         dd      ?          ;/< 32: total size (unpacked), with fixups
pmEntry         dd      ?          ;/< 32: entry point from LX header

fxSelCnt        dd      ?          ;/< number of selector fixups
fxOfsCnt        dd      ?          ;/< number of 32-bit ofs fixups
fxRelCnt        dd      ?          ;/< number of 32-bit relative fixups
mkbin_info      ends

;cc typedef struct mkbin_info MKBIN_HEADER;

FXSEL16         equ     0001h
FXSEL32CS       equ     0002h
FXSEL32DS       equ     0003h

rels_info       struc
rs_offset       dd      ?
rs_seltype      db      ?
rels_info       ends
;cc typedef struct rels_info rels_type;

FXRELO_T16      equ     01h        ;/< target is 16-bit part
FXRELO_S16      equ     02h        ;/< offset is 16-bit part

relo_info       struc
ro_offset       dd      ?
ro_target       dd      ?
ro_flags        db      ?
relo_info       ends
;cc typedef struct relo_info relo_type;

; relative fixup -> this can be only fixup from 32bit to 16bit object.
relr_info       struc
rr_offset       dd      ?
rr_target       dw      ?
relr_info       ends
;cc typedef struct relr_info relr_type;

;cc #pragma pack()

;cc #endif // QS_MKBIN_FMT
