;
; QSINIT
; common constants
;

;cc #ifndef QSINIT_ASMDEFS
;cc #define QSINIT_ASMDEFS

          DISKBUF_SIZE   = 8000h                                ; size of real mode disk read buffer
          LOGBUF_SIZE    = 1000h                                ; internal log handler RM buffer
;cr #ifndef EFI_BUILD
ifndef EFI_BUILD
          QSINIT_SEGREAL = 9100h                                ; qsinit segment (CONSTANT!)
          OS2LDR_SEGREAL = 1000h                                ; initial os2ldr cs

          MFSDBASE       = 07C0h                                ; lowest MFSD phys address
          RIPLBASE       = 0800h                                ; lowest RIPL phys address

          QSM_STACK16    = 7168                                 ; internal 16-bit stack size
          QSM_STACK32    = 16384                                ; internal 32-bit stack size
;cc #if 0  // selectors in common & EFI build are different
          SELCODE        = 08h                                  ; TEXT16 code selector
          SELZERO        = 10h                                  ; selector of entire memory space
          SELCALLBACKDS  = 18h                                  ; callback DS selector
          SELREAL        = 20h                                  ; real mode attributes selector
; in many places assumed FLAT DS = FLAT CS + 8
          SEL32CODE      = 28h                                  ; 32bit CS based flat code
          SEL32DATA      = 30h                                  ; 32bit CS based flat data
          SELDATA        = 38h                                  ; TEXT16 data selector
;cc #endif
;cr #endif // EFI_BUILD
endif
          STO_BUF_LEN    = 12                                   ; internal storage buffer size
          STOINIT_ESIZE  = 20                                   ; internal storage entry size

          MAX_QS_DISK    = 16                                   ; number of supported fdd+hdd
          MAX_QS_FLOPPY  = 2                                    ; number of supported fdds

          MFSD_NAME_LEN  = 128                                  ; max length of micro-FSD name
;cc #endif // QSINIT_ASMDEFS
