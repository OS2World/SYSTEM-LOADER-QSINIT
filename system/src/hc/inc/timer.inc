;
; Physical Timer
;

PORT_CNT0         equ     40h              ; counter 0 data port
PORT_CNT1         equ     41h              ; counter 1 data port
PORT_CNT2         equ     42h              ; counter 2 data port
PORT_CW           equ     43h              ; control word for all counters
PORT_SYSB         equ     61h              ; system control port B
;  Values that can be written to PORT_CW
CW_BCD            equ     01h              ; bit set => BCD counting; bit clear => binary
CW_CNTMODE        equ     0Eh              ; count mode 0 to 5
CW_RWMODE         equ     30h              ; Read/Write modes
CW_SELCNT         equ     0C0h             ; select counter
;  Values that can be written to the CW_CNTMODE field of PORT_CW
CM_MODE0          equ     00h              ; Mode 0: Interrupt on terminal count
CM_MODE1          equ     02h              ; Mode 1: Hardware retriggerable one-shot
CM_MODE2          equ     04h              ; Mode 2: Rate generator
CM_MODE3          equ     06h              ; Mode 3: Square wave mode
CM_MODE4          equ     08h              ; Mode 4: S/W triggered strobe
CM_MODE5          equ     0Ah              ; Mode 5: H/W triggered strobe (retriggerable)
CM_CNT0           equ     02h              ; Counter 0 in Read-Back command
CM_CNT1           equ     04h              ; Counter 1 in Read-Back command
CM_CNT2           equ     08h              ; Counter 2 in Read-Back command
;  Values that can be written to the CW_RWMODE field of PORT_CW
RW_LATCHCNT       equ     00h              ; Latch counter command
RW_LSB            equ     10h              ; Read/Write least significant byte only
RW_MSB            equ     20h              ; Read/Write most significant byte only
RW_LSBMSB         equ     30h              ; Read/Write LSB first, then MSB
RW_NOLATCHCOUNT   equ     20h              ; No latch count in Read-Back command
RW_NOLATCHSTATUS  equ     10h              ; No latch status in Read-Back command
;  Values that can be written to the CW_SELCNT field of PORT_CW
SC_CNT0           equ     00h              ; Counter 0
SC_CNT1           equ     40h              ; Counter 1
SC_CNT2           equ     80h              ; Counter 2
SC_READBACK       equ     0C0h             ; Read-Back command
;  Read-Back command format (write to PORT_CW)
RB_SELCNT0        equ     02h              ; select counter 0
RB_SELCNT1        equ     04h              ; select counter 1
RB_SELCNT2        equ     08h              ; select counter 2
RB_NOLATCHSTATUS  equ     10h              ; bit clear => latch status of counters
RB_NOLATCHCNT     equ     20h              ; bit clear => latch count of counters
;  Status Byte format (read from corresponding counter data port)
STATUS_BCD        equ     CW_BCD           ; counter is in BCD counting mode
STATUS_CNTMODE    equ     CW_CNTMODE       ; counter mode (see CM_MODEx definitions)
STATUS_RWMODE     equ     CW_RWMODE        ; Read/Write mode (see RW_* definitions)
STATUS_NULLCNT    equ     40h              ; Null count (new count has not been loaded)
STATUS_OUTPUT     equ     80h              ; state of output pin of counter
;  System Control Port B Format
SYSB_RESETIRQ0    equ     80h              ; set this bit to reset IRQ0 (W)
SYSB_PARITYCHK    equ     80h              ; parity check has occurred (R)
SYSB_CHNCHK       equ     40h              ; channel check has occurred (R)
SYSB_SPKOUT       equ     20h              ; speaker timer output state (R)
SYSB_REFREQ       equ     10h              ; refresh request toggle (R)
SYSB_PARITYENABLE equ     08h              ; parity check enable (R/W)
SYSB_CHNCHKENABLE equ     04h              ; channel check enable (R/W)
SYSB_SPKENABLE    equ     02h              ; speaker data enable (R/W)
SYSB_SPKGATE      equ     01h              ; speaker timer gate enable (R/W)
SYSB_SPKBITS      equ     03h              ; speaker bit mask
;  Miscellaneous constants
TIMERIRQ          equ     00h              ; IRQ number of timer interrupt
TIMERFREQ         equ     1234CFh          ; timer frequency in Hz
TIMERFREQ_LO      equ     34CFh            ; timer frequency splitted to
TIMERFREQ_HI      equ     12h              ; words

BIOS_TICS         equ     046Ch            ; bios timer tics offset
