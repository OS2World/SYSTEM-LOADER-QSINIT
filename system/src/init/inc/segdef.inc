;
; QSINIT
; common segments definition
;
                   .386p

                   include inc/qsinit.inc

INITCODE           segment dword public USE16 'INIT'
INITCODE           ends

TEXT16             segment dword public USE16 'LCODE'
TEXT16             ends

LOADER             segment dword public USE32 'LCODE'
LOADER             ends

PMODE_TEXT         segment dword public USE16 'LCODE'
PMODE_TEXT         ends

_TEXT              segment dword public USE32 'CODE'
_TEXT              ends

_DATA              segment dword public USE32 'DATA'
_DATA              ends

_BSS               segment dword public USE32 'BSS'
_BSS               ends

DATA16             segment dword public USE16 'LDATA'
DATA16             ends

CONST3             segment dword public USE16 'LDATA'
CONST3             ends

CONST4             segment dword public USE16 'LDATA'
CONST4             ends

BSSINIT            segment dword public USE16 'BSS16'
BSSINIT            ends

_BSS16             segment dword public USE16 'BSS16'
_BSS16             ends

BSSFINI            segment dword public USE16 'BSS16'
BSSFINI            ends

G16  GROUP  INITCODE, TEXT16, PMODE_TEXT, LOADER, DATA16, CONST3, CONST4, BSSINIT, _BSS16, BSSFINI
