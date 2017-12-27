;cc #ifndef QS_BASE_SERPORT_INC
;cc #define QS_BASE_SERPORT_INC
;
; Serial port
;

BD_150          equ     150 
BD_300          equ     300 
BD_600          equ     600 
BD_1200         equ     1200 
BD_2400         equ     2400 
BD_4800         equ     4800 
BD_9600         equ     9600 
BD_19200        equ     19200 
BD_38400        equ     38400 
BD_57600        equ     57600 
BD_115200       equ     115200 

COM1_PORT       equ     03F8h
COM2_PORT       equ     02F8h
COM_DAT         equ     00h                                     ;
COM_IEN         equ     01h                                     ; Interrupt Enable Register
COM_IER         equ     02h                                     ; Interrupt Error Register
COM_LCR         equ     03h                                     ; Line Control Register
COM_MCR         equ     04h                                     ; Modem Control Register
COM_LSR         equ     05h                                     ; Line Status Register
COM_MSR         equ     06h                                     ; Modem Status Register
COM_DLL         equ     00h                                     ; Divisor Latch (LSB)
COM_DLM         equ     01h                                     ; Divisor Latch (MSB)
COM_FCR         equ     02h                                     ; FIFO Control Register

CLOCK_RATEL     equ     1
CLOCK_RATEH     equ     0C200h
LC_DLAB         equ     80h

COM_SENDREADY   equ     01100000b                               ; transmitter ready

;cc #endif // QS_BASE_SERPORT_INC
