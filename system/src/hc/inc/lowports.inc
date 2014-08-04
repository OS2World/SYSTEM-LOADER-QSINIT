;
;  Miscellaneous PC ports (NMI, speaker, etc)
;

SPEAKER_PORT    equ     61h                                     ;
SPEAKER_MASK    equ     03h                                     ;
SPEAKER_MASK_OFF equ    0FCh                                    ;

KBD_STATUS_PORT equ     64h                                     ;

NMI_STATUS_PORT equ     61h                                     ;
NMI_PORT        equ     70h                                     ;
ENABLE_NMI      equ     0dh                                     ;
DISABLE_NMI     equ     8dh                                     ;
DISABLE_NMI_REQ equ     80h                                     ;
NMI_IOCHK       equ     04h                                     ;
NMI_FAILSAFE    equ     08h                                     ;

NPX_BUSY_PORT   equ     0F0h                                    ;
NPX_REAL_PORT   equ     0F1h                                    ;
