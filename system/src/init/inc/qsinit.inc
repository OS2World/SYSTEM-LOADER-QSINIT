;
; QSINIT
; common constants
;

;cc #ifndef QSINIT_ASMDEFS
;cc #define QSINIT_ASMDEFS

          QSINIT_SEGREAL = 9100h                                ; qsinit segment (CONSTANT!)
          OS2LDR_SEGREAL = 1000h                                ; initial os2ldr cs

          MFSDBASE       = 07C0h                                ; lowest MFSD phys address
          RIPLBASE       = 0800h                                ; lowest RIPL phys address

          DISKBUF_SIZE   = 8000h                                ; size of real mode disk read buffer
          LOGBUF_SIZE    = 1000h                                ; internal log handler RM buffer


          SELCODE        = 08h                                  ; PMODE_TEXT code selector
          SELZERO        = 10h                                  ; selector of entire memory space
          SELCALLBACKDS  = 18h                                  ; callback DS selector
          SELREAL        = 20h                                  ; real mode attributes selector

; note! exception handler in "start" module assume FLAT DS = FLAT CS + SEL_INCR
          SEL32CODE      = 28h                                  ; 32bit CS based flat code
          SEL32DATA      = 30h                                  ; 32bit CS based flat data

          STO_BUF_LEN    = 8                                    ; internal storage buffer size
          STOINIT_ESIZE  = 20                                   ; internal storage entry size

          MAX_QS_DISK    = 16                                   ; number of supported fdd+hdd
          MAX_QS_FLOPPY  = 2                                    ; number of supported fdds

;cc #endif // QSINIT_ASMDEFS
