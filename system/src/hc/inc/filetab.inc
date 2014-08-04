;
; QSINIT
; micro-FSD interface
;

filetable_s     struc
ft_cfiles       dw      ?
ft_loaderseg    dw      ?
ft_loaderlen    dd      ?
ft_museg        dw      ?
ft_mulen        dd      ?
ft_mfsdseg      dw      ?
ft_mfsdlen      dd      ?
ft_riplseg      dw      ?
ft_ripllen      dd      ?
ft_muOpen       dd      ?
ft_muRead       dd      ?
ft_muClose      dd      ?
ft_muTerminate  dd      ?
ft_resofs       dd      ?
ft_reslen       dd      ?
filetable_s     ends

BF_NOMFSHVOLIO  equ     001h     
BF_RIPL         equ     002h     
BF_MINIFSD      equ     004h     
BF_NOPICINIT    equ     008h     
BF_MICROFSD     equ     010h     
BF_RESERVED     equ     0E0h
