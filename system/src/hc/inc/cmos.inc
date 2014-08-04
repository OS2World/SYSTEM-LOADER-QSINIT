;
; QSINIT
; cmos refs
;

NMI_MASK        =       80h          ;
CMOS_MASK       =       3Fh          ;
CMOS_SEC        =       00h          ;
CMOS_MIN        =       02h          ;
CMOS_HRS        =       04h          ;
CMOS_DOW        =       06h          ;
CMOS_DAY        =       07h          ;
CMOS_MON        =       08h          ;
CMOS_YRS        =       09h          ;
CMOS_CENT       =       32h          ;
CMOS_REG_A      =       0Ah          ;
CMOS_REG_B      =       0Bh          ;
CMOS_REG_C      =       0Ch          ;
CMOS_REG_D      =       0Dh          ;
CMOS_ADDR       =       70h          ;
CMOS_DATA       =       71h          ;

; bit constants for register A
CMOS_A_UIP      =       80h          ; update in progress (R/O)
CMOS_A_DV32K    =       20h          ; standard base 32768KHz rate
CMOS_A_DIV_MASK =       0Fh          ;
CMOS_A_IN2      =       15           ; 2Hz, 500ms
CMOS_A_IN4      =       14           ; 4Hz, 250ms
CMOS_A_IN8      =       13           ; 8Hz, 125ms
CMOS_A_IN16     =       12           ; 16Hz, 62.5ms
CMOS_A_IN32     =       11           ; 32Hz, 31.25ms
CMOS_A_IN64     =       10           ; 64Hz, 15.625ms
CMOS_A_IN128    =        9           ; 128Hz, 7.8125ms
CMOS_A_IN256    =        8           ; 256Hz, 3.90625ms
CMOS_A_IN512    =        7           ; 512Hz, 1.953125ms
CMOS_A_IN1024   =        6           ; 1KHz, 976.562us
CMOS_A_IN2048   =        5           ; 2KHz, 488.281us
CMOS_A_IN4096   =        4           ; 4KHz, 244.141us
CMOS_A_IN8192   =        3           ; 8KHz, 122.070us

; constants to calculate frequencies (IRQ frequency = BaseFreq >> (divider-1))
CMOS_BASEFREQ   =    32768           ; base rate of clock
CMOS_MIN_DIV    =        3           ; minimal allowed divider of the base rate

CMOS_B_DENY     =     0C2h           ; Bit 7 = Update inhibit             
CMOS_B_NORM     =     042h           ; Bit 6 = Periodic interrupt enable  
                                     ; Bit 5 = Alarm interrupt enable     
                                     ; Bit 4 = Update interrupt enable    
                                     ; Bit 3 = Square wave enable         
                                     ; Bit 2 = Binary data format         
                                     ; Bit 1 = 24 hour time mode          
                                     ; Bit 0 = Daylight Savings enable    

cmosset         macro   addr
                mov     al,addr
                out     CMOS_ADDR,al
                jmp     $+2
                endm

cmossetnj       macro   addr
                mov     al,addr
                out     CMOS_ADDR,al
                endm

cmosread        macro
                in      al,CMOS_DATA
                jmp     $+2
                endm

cmoswrite       macro   value
                out     CMOS_DATA,al
                jmp     $+2
                endm

cmosreset       macro
                mov     al,CMOS_REG_D
                out     CMOS_ADDR,al
                jmp     $+2
                in      al,CMOS_DATA
                jmp     $+2
                endm
