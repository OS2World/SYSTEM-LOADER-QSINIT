;
; QSINIT
; common segments definition
;
                   .386p

                   include inc/qsinit.inc

INITCODE           segment dword public USE16 'INIT'
INITCODE           ends

_TEXT              segment dword public USE16 'CODE'
_TEXT              ends

HICODE             segment dword public USE16 'CODEHI'
HICODE             ends

PMODE_TEXT         segment dword public USE16 'CODE'
PMODE_TEXT         ends

CODE32             segment dword public USE32 'CODE'
CODE32             ends

_CODE32            segment dword public USE32 'CODE'
_CODE32            ends

DATA32             segment dword public USE32 'DATA'
DATA32             ends

_DATA              segment dword public USE16 'DATA'
_DATA              ends

_ABOUT             segment byte public USE16 'EDATA'
_ABOUT             ends

STARTBSS           segment para public USE16 'BSS'
STARTBSS           ends

_BSS               segment dword public USE16 'BSS'
_BSS               ends

BSS                segment dword public USE16 'BSS'
BSS                ends

_BSS32             segment dword public USE32 'BSS'
_BSS32             ends

_STACK             segment para stack USE16 'STACK'
_STACK             ends

CONST              segment para public USE16 'DATA'
CONST              ends
CONST2             segment para public USE16 'DATA'
CONST2             ends
CONST3             segment para public USE32 'DATA'
CONST3             ends
CONST4             segment para public USE32 'DATA'
CONST4             ends
CONST5             segment para public USE32 'DATA'
CONST5             ends

DGROUP GROUP       INITCODE, _TEXT, PMODE_TEXT, CODE32, _CODE32, HICODE, DATA32, _DATA, CONST, CONST2, CONST3, CONST4, CONST5, _ABOUT, STARTBSS, _BSS, BSS, _BSS32, _STACK
