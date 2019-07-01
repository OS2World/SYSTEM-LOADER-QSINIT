;
; QSINIT
; doshlp code - segments
;

PUBLIC_INFO     segment byte public USE16 'PUBINFO'
PUBLIC_INFO     ends

DOSHLP_CODE     segment para public USE16 'DHCODE'
DOSHLP_CODE     ends

DOSHLP_DATA     segment para public USE16 'DHDATA'
DOSHLP_DATA     ends

OEMHLP_CODE     segment para public USE16 'OEMCODE'
OEMHLP_CODE     ends

OEMHLP_DATA     segment para public USE16 'OEMDATA'
OEMHLP_DATA     ends

DOSHLP32_CODE   segment para public USE32 'DH32CODE'
DOSHLP32_CODE   ends

RESPART_END     segment para public USE16 'MARKER'
RESPART_END     ends

BOOTHLP_CODE    segment para public USE16 'DISCODE'
BOOTHLP_CODE    ends

BOOTHLP_DATA    segment para public USE16 'DISDATA'
BOOTHLP_DATA    ends

BOOTHLP32_CODE  segment para public USE32 'DISCODE'
BOOTHLP32_CODE  ends

_TEXT           segment para public USE16 'DISCODE'
_TEXT           ends

_DATA           segment para public USE16 'DISDATA'
_DATA           ends

_BSS            segment para public USE16 'BSS'
_BSS            ends

IODELAY_FIXB    segment word public USE16 'FIXDATA'
IODELAY_FIXB    ends

IODELAY_FIXUP   segment word public USE16 'FIXDATA'
IODELAY_FIXUP   ends

IODELAY_FIXE    segment word public USE16 'FIXDATA'
IODELAY_FIXE    ends

COMMON_FIXB     segment word public USE16 'FIXDATA2'
COMMON_FIXB     ends

COMMON_FIXUP    segment word public USE16 'FIXDATA2'
COMMON_FIXUP    ends

COMMON_FIXE     segment word public USE16 'FIXDATA2'
COMMON_FIXE     ends

_STACK          segment word public USE16 'STACK'
_STACK          ends

LASTSEG         segment para public USE16 'FINI'
LASTSEG         ends

DGROUP GROUP    PUBLIC_INFO,DOSHLP_CODE,DOSHLP_DATA,OEMHLP_CODE,OEMHLP_DATA,\
                DOSHLP32_CODE,RESPART_END,BOOTHLP_CODE,\
                BOOTHLP32_CODE,BOOTHLP_DATA,_TEXT,_DATA,IODELAY_FIXB,\
                IODELAY_FIXUP,IODELAY_FIXE,COMMON_FIXB,COMMON_FIXUP,\
                COMMON_FIXE,_BSS,_STACK,LASTSEG
