;
; QSINIT
; le/lx dll module initializaion
;
                .486p

CODE32          segment byte public USE32 'CODE'
                extrn _LibMain:near
                extrn _tm_counter:near
                extrn _sys_exfunc4:near
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT
start:
                mov     eax, [esp+4]
                mov     __Module, eax
                push    [esp+8]
                push    eax
                call    _tm_counter
                mov     __RandSeed, eax
                call    _LibMain
                add     esp, 8
                ret
;----------------------------------------------------------------
next_random:
                mov     eax,8088405h
                mul     __RandSeed
                inc     eax
                mov     __RandSeed,eax
                ret

                public  _random
_random         label   near
                mov     ecx, [esp+4]
                call    next_random
                mul     ecx                     ; Random * Range / 1 0000 0000h
                mov     eax,edx                 ; 0 <= eax < Range
                ret     4
CODE32          ends

_DATA           segment dword public USE32 'DATA'
                public  __Module, __RandSeed, __IsDll
; this is module* struct, referenced in qsmod.h. DO NOT modify contents, this
; is a system data actually ;)
__Module        dd      0
__RandSeed      dd      1
__IsDll         dd      1
_DATA           ends

                end start
