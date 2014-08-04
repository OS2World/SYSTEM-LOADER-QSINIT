;cc #ifndef QS_HD4D_SWITCHPAGE
;cc #define QS_HD4D_SWITCHPAGE

PAETAB_OFS      equ     3000h                                   ; PAE tables pos in pdsel
COPYPAGE_ADDR   equ     0FFC00000h                              ;

SWSEL_CODELIN   equ     8                                       ; code mapped to OS/2 lin addr
SWSEL_CODEPHYS  equ     16                                      ; code mapped to phys addr
SWSEL_FLAT      equ     24                                      ; flat data
SWSEL_LIMIT     equ     31                                      ; gdt limit

swpage_s        struc
                db      3964 dup(?)
sw_esp          dd      ?                                       ; esp in stack
sw_ss           dd      ?                                       ; ss in stack
sw_paeon        db      ?                                       ; var
sw_pad1         db      ?                                       ;
sw_pad2         dw      ?                                       ;
sw_headpage     dd      ?                                       ; disk header page
sw_sgn_size     dd      ?                                       ;
sw_sgn_phys     dd      ?                                       ;
sw_buf64k       dd      ?                                       ; 64k buffer for unaligned ops
sw_buf64k_len   dd      ?                                       ; 64k const (to emulate SGEntry)
sw_headcopy     dd      ?                                       ; phys addr of header copy
sw_paeppd       dd      ?                                       ; 
sw_sectors      dd      ?                                       ; var
sw_sgbcnt       dd      ?                                       ; var
sw_sgphys       dd      ?                                       ; var
sw_ephys        dd      ?                                       ; var
sw_savegdt      dw      ?                                       ; OS/2 gdt ptr
                dd      ?                                       ;
sw_gdtlimit     dw      ?                                       ; own gdt ptr
sw_gdtbase      dd      ?                                       ;
sw_pdpte        dq      4 dup (?)                               ; PAE PDPTE (32 bytes aligned)
sw_gdt          dd      8 dup (?)                               ; gdt buffer
swpage_s        ends

;cc typedef struct swpage_s  SwitchPage;

;cc #endif // QS_HD4D_SWITCHPAGE
