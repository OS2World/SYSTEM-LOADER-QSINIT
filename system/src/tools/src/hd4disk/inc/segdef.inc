;
;
                   .686p

_TEXT              segment byte public USE16 'CODE'
_TEXT              ends

CODEEND            segment byte public USE16 'CODE'
CODEEND            ends

DDHEADER           segment word public USE16 'HEADER'
DDHEADER           ends

_DATA              segment word public USE16 'DATA'
_DATA              ends

_BSS               segment word public USE16 'BSS'
_BSS               ends

CONST              segment para public USE16 'DATA'
CONST              ends
CONST2             segment para public USE16 'DATA'
CONST2             ends

DATAEND            segment word public USE16 'DMARK'
DATAEND            ends

_REL16             segment dword public USE16 'REL16'
_REL16             ends

_REL32             segment dword public USE32 'REL32'
_REL32             ends

DGROUP             group DDHEADER, _DATA, CONST, CONST2, _BSS, DATAEND
RELCODE            group _REL16, _REL32
