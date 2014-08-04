
_DATA           segment
; PIC1 initialization sequence
Pic1InitSeq     dw      PIC1_PORT0
                db      ICW1_ICW + ICW1_ICW4_NEEDED
                db      PIC1_IRQ_NEW
                db      1 SHL PIC_SLAVE_IRQ
                db      ICW4_8086_MODE
; PIC2 initialization sequence
Pic2InitSeq     dw      PIC2_PORT0
                db      ICW1_ICW + ICW1_ICW4_NEEDED
                db      PIC2_IRQ_NEW
                db      PIC_SLAVE_IRQ
                db      ICW4_8086_MODE
_DATA           ends


_TEXT           segment
remap_pic       proc    near
                assume  cs:dgroup,ds:dgroup,es:nothing,ss:dgroup

                pushfd                                          ; disable interrupts
                cli                                             ;
                xor     ax,ax                                   ;
                mov     es,ax                                   ;
                mov     si, PIC1_IRQ_ORG SHL 2                  ; copy interrupt vectors
                mov     edi, PIC1_IRQ_NEW SHL 2                 ; from 08h to 50h
                mov     cx, NUM_IRQ_PER_PIC                     ;
@@pic_copyloop:
                mov     eax, es:[si]                            ;
                stosd                                           ;
                add     si, 4                                   ;
                loop    @@pic_copyloop                          ;

                lea     si,Pic1InitSeq                          ; send init strings:
                call    @@pic_send                              ; to PIC1
                lea     si,Pic2InitSeq                          ;
                call    @@pic_send                              ; and PIC2
                mov     al,OCW2_SET_PRIORITY + 2                ;
                out     PIC1_PORT0,al                           ; set IRQ 2 priority
                popfd                                           ;
                ret
@@pic_send:
                lodsw                                           ; port addr
                mov     dx,ax                                   ;
                inc     dx                                      ;
                in      al,dx                                   ; saving PIC mask
                push    ax                                      ;
                dec     dx                                      ;
                outsb                                           ; sending ICW1
                inc     dx                                      ;
                push    cx                                      ;
                mov     cx,3                                    ; and ICW2,3,4
            rep outsb                                           ;
                pop     cx                                      ;
                pop     ax                                      ; restore PIC mask
                out     dx,al                                   ;
                ret                                             ;
remap_pic       endp
_TEXT           ends

