;
; QSINIT
; some useful macroses
;

NULL     equ    0
FALSE    equ    0
TRUE     equ   -1

;*** SaveReg - generate code to save registers on stack
SaveReg macro reglist                                           ; push those registers
     irp reg,<reglist>
         push reg
     endm
endm

;*** RestoreReg - generate code to restore registers from the stack
RestReg macro reglist                                           ; pop those registers
    irp reg,<reglist>
         pop reg
    endm
endm
