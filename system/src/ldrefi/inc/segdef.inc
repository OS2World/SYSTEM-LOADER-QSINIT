;
; QSINIT EFI loader
; common segments definition
;
                .386p

                include inc/qsinit.inc

_TEXT           segment dword public USE32 'CODE'
_TEXT           ends

_DATA           segment dword public USE32 'DATA'
_DATA           ends

_BSS            segment dword public USE32 'BSS'
_BSS            ends
