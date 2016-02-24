
DATA16          segment
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
; PIC1 initialization sequence
Pic1RestSeq     dw      PIC1_PORT0
                db      ICW1_ICW + ICW1_ICW4_NEEDED
                db      PIC1_IRQ_ORG
                db      1 SHL PIC_SLAVE_IRQ
                db      ICW4_8086_MODE
; PIC2 initialization sequence
Pic2RestSeq     dw      PIC2_PORT0
                db      ICW1_ICW + ICW1_ICW4_NEEDED
                db      PIC2_IRQ_ORG
                db      PIC_SLAVE_IRQ
                db      ICW4_8086_MODE
DATA16          ends


TEXT16          segment
                assume  cs:G16, ds:G16, es:nothing, ss:G16

remap_pic       proc    near
                cld                                             ;
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
                call    pic_send                                ; to PIC1
                lea     si,Pic2InitSeq                          ;
                call    pic_send                                ; and PIC2
                mov     al,OCW2_SET_PRIORITY + 2                ;
                out     PIC1_PORT0,al                           ; set IRQ 2 priority
                popfd                                           ;
                ret
remap_pic       endp

;
; part below is used from pm2rm DPMI code, when reset IRQs asked by special flag
;
                assume  cs:G16, ds:nothing, es:nothing, ss:nothing

pic_send        proc    near
                lods    word ptr cs:[si]                        ; port addr
                mov     dx,ax                                   ;
                inc     dx                                      ;
                in      al,dx                                   ; saving PIC mask
                push    ax                                      ;
                dec     dx                                      ;
                outs    dx,byte ptr cs:[si]                     ; sending ICW1
                inc     dx                                      ;
                push    cx                                      ;
                mov     cx,3                                    ; and ICW2,3,4
            rep outs    dx,byte ptr cs:[si]                     ;
                pop     cx                                      ;
                pop     ax                                      ; restore PIC mask
                out     dx,al                                   ;
                ret                                             ;
pic_send        endp

; function called inside of global DPMI`s cli
                public  rest_pic
rest_pic        proc    near
                cld                                             ;
                SaveReg <si,dx,ax>                              ;
                lea     si,Pic1RestSeq                          ; send init strings:
                call    pic_send                                ; to PIC1
                lea     si,Pic2RestSeq                          ;
                call    pic_send                                ; and PIC2
                mov     al,OCW2_SET_PRIORITY + 2                ;
                out     PIC1_PORT0,al                           ; set IRQ 2 priority
                RestReg <ax,dx,si>                              ;
                ret                                             ;
rest_pic        endp

TEXT16          ends
