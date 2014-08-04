;
; Interrupt controller
;

NUMBER_PICS             = 2                                 ;
NUM_IRQ_PER_PIC         = 8                                 ;
NUM_IRQ                 = (NUMBER_PICS * NUM_IRQ_PER_PIC)   ;
PIC_SLAVE_IRQ           = 2                                 ;

PIC1_IRQ_ORG            = 08h                               ;
PIC2_IRQ_ORG            = 70h                               ;

PIC1_IRQ_NEW            = 50h                               ;
PIC2_IRQ_NEW            = 70h                               ;

PIC1_PORT0              = 20h                               ; master PIC
PIC1_PORT1              = 21h                               ;
PIC2_PORT0              = 0A0h                              ; slave PIC
PIC2_PORT1              = 0A1h                              ;

ICW1_ICW4_NEEDED        = 00000001b                         ;
ICW1_SINGLE             = 00000010b                         ;
ICW1_INTERVAL4          = 00000100b                         ;
ICW1_LEVEL_TRIG         = 00001000b                         ;
ICW1_ICW                = 00010000b                         ;

ICW4_8086_MODE          = 00000001b                         ;
ICW4_AUTO_EOI           = 00000010b                         ;
ICW4_BUF_MODE_SLAVE     = 00001000b                         ;
ICW4_BUF_MODE_MASTER    = 00001100b                         ;
ICW4_SPEC_FULLY_NESTED  = 00010000b                         ;

OCW2_NON_SPECIFIC_EOI   = 00100000b                         ;
OCW2_SPECIFIC_EOI       = 01100000b                         ;
OCW2_SET_PRIORITY       = 11000000b                         ;

OCW3_READ_IRR           = 00001010b                         ;
OCW3_READ_ISR           = 00001011b                         ;

IRQ_NPX                 = 13                                ;
