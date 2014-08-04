;
; QSINIT
; Real<->PM switching code
; Based on PMODE 3.07 by Tran (a.k.a. Thomas Pytel).
;
                .386
                include inc/segdef.inc
                include inc/basemac.inc
                include inc/qsinit.inc
                include inc/cpudef.inc 
                include inc/seldesc.inc 
                include inc/iopic.inc

                public  _pm_info, _pm_init
                public  _syscr3, _syscr4, _systr

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; DATA
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
SYSSELECTORS    = 9                                             ; number of system selectors in GDT

off             equ     offset
; ----------------------------------------------------------------------------
_BSS            segment
                align 4
codebase        dd      ?                                       ; PMODE_TEXT linear address
pmstacklen      dd      ?                                       ; protected mode stack length in bytes
pmstackbase     dd      ?                                       ; bottom of protected mode stack area
pmstacktop      dd      ?                                       ; top of protected mode stack area
callbackbase    dd      ?                                       ; base of real mode callbacks
callbackseg     dw      ?                                       ; segment of callbacks

rmstackbase     dw      ?                                       ; bottom of real mode stack area
rmstacktop      dw      ?                                       ; top of real mode stack area
rmstackparmtop  dw      ?                                       ; for functions 0300h, 0301h, 0302h

gdtseg          dw      ?                                       ; segment of GDT
processortype   db      ?                                       ; processor type
                db      ?                                       ; just here for alignment

tempd0          label   dword                                   ; temporary variables
tempw0          label   word                                    ;
tempb0          db      ?                                       ;
tempb1          db      ?                                       ;
tempw1          label   word                                    ;
tempb2          db      ?                                       ;
tempb3          db      ?                                       ;
tempd1          label   dword                                   ;
tempw2          label   word                                    ;
tempb4          db      ?                                       ;
tempb5          db      ?                                       ;
tempw3          label   word                                    ;
tempb6          db      ?                                       ;
tempb7          db      ?                                       ;

                extrn   _rm16code:word                          ; rm code segment
                extrn   intrmatrix:byte                         ; int route table
                extrn   _safeMode:byte                          ; safe mode is active
_BSS            ends

_DATA           segment
                align   4
int3vector      dd      intrmatrix+3                            ; protected mode INT 3 vector
                dw      SELCODE

_selzero        dw      SELZERO                                 ; for immediate segreg loading
_selcallbackds  dw      SELCALLBACKDS                           ; for immediate segreg loading

gdtlimit        dw      ?                                       ; GDT limit
gdtbase         dd      ?                                       ; GDT base
idtlimit        dw      7ffh                                    ; IDT limit
idtbase         dd      ?                                       ; IDT base
rmidtlimit      dw      3ffh                                    ; real mode IDT limit
rmidtbase       dd      0                                       ; real mode IDT base

_syscr3         dd      0                                       ; system cr3 value
_syscr4         dd      0                                       ; cr4 flags to set

pmtormswrout    dd      off xr_pmtormsw                         ; addx of protected to real routine

_pm_selectors   dw      96                                      ; max selectors
_pm_rmstacklen  dw      40h                                     ; real mode stack length, in para
_pm_pmstacklen  dw      80h                                     ; protected mode stack length, in para
_pm_rmstacks    db      4                                       ; real mode stack nesting
_pm_pmstacks    db      2                                       ; protected mode stack nesting
_pm_callbacks   db      16                                      ; number of real mode callbacks

_pm_initerr     dw      0                                       ; pm_init error code
_systr          dw      0                                       ; task register value

picslave        db      PIC2_IRQ_NEW                            ; PIC slave base interrupt
picmaster       db      PIC1_IRQ_NEW                            ; PIC master base interrupt

                align   2
int31functbl    dw      0900h, 0901h, 0902h, 0000h, 0001h, 0002h
                dw      0003h, 0006h, 0007h, 0008h, 0009h
                dw      000Ah, 000Bh, 000Ch, 000Eh, 000Fh
                dw      0200h, 0201h, 0204h, 0205h
                dw      0300h, 0301h, 0302h, 0303h, 0304h, 0305h, 0306h
                dw      0400h
INT31FUNCNUM    = ($ - int31functbl) / 2

int31routtbl    dw      int310900, int310901, int310902, int310000, int310001, int310002
                dw      int310003, int310006, int310007, int310008, int310009
                dw      int31000a, int31000b, int31000c, int31000e, int31000f
                dw      int310200, int310201, int310204, int310205
                dw      int310300, int310301, int310302, int310303, int310304, int310305, int310306
                dw      int310400
flatdsmode      dw      0D096h, 0DF92h
_DATA           ends
; ----------------------------------------------------------------------------
PMODE_TEXT      segment
assume          cs:DGROUP, ds:DGROUP

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; DETECT/INIT CODE
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

;°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
; Get protected mode info
; Out:
;   AX = return code:
;     0000h = successful
;     0001h = no 80386+ detected
;     0002h = system already in protected mode
;   CF = set on error, if no error:
;     BX = number of paragraphs needed for protected mode data
;     CL = processor type:
;       02h = 80286
;       03h = 80386
;       04h = 80486
;       05h = 80586
;       06h-FFh = reserved for future use
;°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
_pm_info        proc    near
                SaveReg <dx,si,di,bp,ds,es,bx,cx>               ; preserve registers

                call    @@detect_processor                      ; get processor type
                mov     ax,1                                    ; error code in case no processor 386+
                cmp     cl,3                                    ; is processor 386+?
                jb      @@infofail                              ; if no, no VCPI
                mov     processortype,cl                        ; store processor type

                smsw    ax                                      ; AX = machine status word
                test    al,1                                    ; is system in protected mode?
                mov     ax,2                                    ; error code in case protected mode
                jnz     short @@infofail                        ; if in protected mode, fail

                mov     bx,80h                                  ; BX = memory requirement (IDT)
                movzx   ax,_pm_rmstacks                         ; size of real mode stack area
                imul    ax,_pm_rmstacklen
                add     bx,ax

                movzx   ax,_pm_pmstacks                         ; size of protected mode stack area
                imul    ax,_pm_pmstacklen
                add     bx,ax

                movzx   ax,_pm_callbacks                        ; size of callbacks
                imul    ax,25
                add     ax,0fh
                shr     ax,4
                add     bx,ax

                mov     ax,_pm_selectors                        ; size of GDT
                add     ax,1+SYSSELECTORS+5
                shr     ax,1
                add     bx,ax

                add     sp,4                                    ; skip BX and CX on stack
                xor     ax,ax                                   ; success code, also clear carry flag
;-----------------------------------------------------------------------------
@@infodone:
                RestReg <es,ds,bp,di,si,dx>                     ; restore other registers
                ret                                             ; return
;-----------------------------------------------------------------------------
@@infofail:
                RestReg <cx,bx>                                 ; restore BX and CX
                stc                                             ; carry set, failed
                jmp     short @@infodone
;-----------------------------------------------------------------------------
@@detect_processor:                                             ; get processor: 286, 386, 486, or 586
                xor     cl,cl                                   ; processor type 0 in case of exit

                pushf                                           ; transfer FLAGS to BX
                pop     bx
                mov     ax,bx                                   ; try to clear high 4 bits of FLAGS
                and     ah,0fh

                push    ax                                      ; transfer AX to FLAGS
                popf
                pushf                                           ; transfer FLAGS back to AX
                pop     ax

                and     ah,0f0h                                 ; isolate high 4 bits
                cmp     ah,0f0h
                je      short @@detect_processordone            ; if bits are set, CPU is 8086/8
                mov     cl,2                                    ; processor type 2 in case of exit
                or      bh,0f0h                                 ; try to set high 4 bits of FLAGS

                push    bx                                      ; transfer BX to FLAGS
                popf
                pushf                                           ; transfer FLAGS to AX
                pop     ax

                and     ah,0f0h                                 ; isolate high 4 bits
                jz      short @@detect_processordone            ; if bits are not set, CPU is 80286
                inc     cx                                      ; processor type 3 in case of exit

                SaveReg <eax,ebx>                               ; preserve 32bit registers

                pushfd                                          ; transfer EFLAGS to EBX
                pop     ebx

                mov     eax,ebx                                 ; try to flip AC bit in EFLAGS
                xor     eax,40000h
                push    eax                                     ; transfer EAX to EFLAGS
                popfd
                pushfd                                          ; transfer EFLAGS back to EAX
                pop     eax

                xor     eax,ebx                                 ; AC bit fliped?
                jz      short @@detect_processordone2           ; if no, CPU is 386
                inc     cx                                      ; processor type 4 in case of exit
                mov     eax,ebx                                 ; try to flip ID bit in EFLAGS
                xor     eax,200000h

                push    eax                                     ; transfer EAX to EFLAGS
                popfd
                pushfd                                          ; transfer EFLAGS back to EAX
                pop     eax

                xor     eax,ebx                                 ; ID bit fliped?
                jz      short @@detect_processordone2           ; if no, CPU is 486
                inc     cx                                      ; processor type 5, CPU is 586
@@detect_processordone2:
                RestReg <ebx,eax>                               ; restore 32bit registers

@@detect_processordone:
                ret
_pm_info        endp

;°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
; Initialize protected mode
; In:
;   ES = real mode segment for protected mode data
;   SS == DS
; Out:
;   AX = return code:
;     0000h = successful
;     0001h = no 80386+ detected
;     0002h = system already in protected mode
;     0004h = could not enable A20 gate (PM opened & CF is NOT set in this case)
;   CF = set on error, if no error:
;     ESP = high word clear
;     CS = 16bit selector for real mode CS with limit of 64k
;     SS = selector for real mode SS with limit of 64k
;     DS = the same selector
;     ES = selector for FLAT zero-based space
;     FS = 0 (NULL selector)
;     GS = 0 (NULL selector)
;°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
_pm_init        proc    near
                SaveReg <bx,cx>                                 ; get initial info on protected mode
                call    _pm_info
                RestReg <cx,bx>
                jnc     short @@initf0                          ; error?
                mov     _pm_initerr,ax
                ret                                             ; yup, abort
@@initf0:                                                       ; no error, init protected mode
                pushad
;                push    ds
                mov     bp,sp
                cld
                mov     ax,cs                                   ; guarantee ds=cs
                mov     ds,ax                                   ;
                mov     word ptr xr_pmtormsw_cs,ax              ;

                xor     eax,eax
                mov     ax,cs                                   ; set base addx of PMODE_TEXT
                shl     eax,4
                mov     codebase,eax

                call    enablea20                               ; enable A20

                mov     ax,es                                   ; set IDT base address
                movzx   ebx,ax
                shl     ebx,4
                mov     idtbase,ebx

                movzx   bx,_pm_rmstacks                         ; set top and base of real mode stack
                imul    bx,_pm_rmstacklen                       ; area for interrupt redirection
                add     ax,80h                                  ; from protected mode
                mov     rmstackbase,ax
                add     ax,bx
                mov     rmstacktop,ax

                xor     di,di                                   ; set up IDT
                mov     dx,word ptr picslave
                xor     ecx,ecx
@@vxr_initl0:
                lea     eax,[SELCODE*10000h+ecx+off intrmatrix]
                stos    dword ptr es:[di]

                mov     eax,8e00h                               ; interrupt gate type
                mov     bl,cl                                   ; isolate high 5 bits of int num
                and     bl,0f8h

                test    cl,0f8h                                 ; one of the low 8 interrupts?
                jz      short @@vxr_initl0f0                    ; if yes, store as interrupt gate

                cmp     bl,dl                                   ; one of the high IRQs?
                je      short @@vxr_initl0f0                    ; if yes, store as interrupt gate
                cmp     bl,dh                                   ; one of the low IRQs?
                je      short @@vxr_initl0f0                    ; if yes, store as interrupt gate

                or      ah,1                                    ; set to trap gate type
@@vxr_initl0f0:
                stos    dword ptr es:[di]

                inc     cl                                      ; increment interrupt number
                jnz     @@vxr_initl0                            ; loop if more interrupts to go

                mov     word ptr es:[8*31h],off int31           ; protected mode INT 31h
                mov     word ptr es:[8*21h],off int21           ; protected mode INT 21h
                mov     word ptr es:[8*3],off intr              ; protected mode INT 3

                mov     es,rmstacktop                           ; set next data area ptr to end of
                                                                ; real mode stack area
;-----------------------------------------------------------------------------
                mov     ax,es                                   ; set protected mode stack area base
                movzx   eax,ax                                  ; for callbacks
                shl     eax,4
                mov     pmstackbase,eax

                movzx   ecx,_pm_pmstacklen                      ; set protected mode stack area top
                movzx   ebx,_pm_pmstacks                        ; for callbacks
                shl     ecx,4
                mov     pmstacklen,ecx                          ; protected mode stack size in bytes
                imul    ebx,ecx
                add     ebx,eax
                mov     pmstacktop,ebx                          ; protected mode stack area top

                mov     cl,_pm_callbacks                        ; CL = number of callbacks
                or      cl,cl                                   ; any callbacks?
                jz      short @@vxr_initf3                      ; if no, done with this part

                mov     callbackbase,ebx                        ; top of stacks is base of callbacks
                shr     ebx,4                                   ; BX = seg of callback area
                mov     callbackseg,bx

                mov     es,bx                                   ; ES = seg of callback area
                xor     di,di                                   ; location within callback seg

                mov     ax,_rm16code                            ; own real mode cs
@@vxr_initl1:
                mov     word ptr es:[di],6066h                  ; PUSHAD instruction
                mov     byte ptr es:[di+2],068h                 ; PUSH WORD instruction
                mov     word ptr es:[di+3],0                    ; immediate 0 used as free flag
                mov     word ptr es:[di+5],06866h               ; PUSH DWORD instruction
                mov     byte ptr es:[di+11],0B9h                ; MOV CX,? instruction
                mov     word ptr es:[di+14],06866h              ; PUSH DWORD instruction
                mov     byte ptr es:[di+20],0EAh                ; JMP FAR PTR ?:? intruction
                mov     word ptr es:[di+21],off callback
                mov     word ptr es:[di+23],ax

                add     di,25                                   ; increment ptr to callback
                dec     cl                                      ; decrement loop counter
                jnz     @@vxr_initl1                            ; if more callbacks to do, loop

                add     di,0fh                                  ; align next data area on paragraph
                shr     di,4
                add     bx,di
                mov     es,bx                                   ; set ES to base of next data area
@@vxr_initf3:
;-----------------------------------------------------------------------------
                mov     gdtseg,es                               ; store segment of GDT

                mov     ax,es                                   ; set GDT base address
                movzx   eax,ax
                shl     eax,4
                mov     gdtbase,eax

                mov     cx,_pm_selectors                        ; set GDT limit
                lea     ecx,[8*ecx+8*5+8*SYSSELECTORS-1]
                mov     gdtlimit,cx

                xor     di,di                                   ; clear GDT with all 0
                inc     cx
                shr     cx,1
                xor     ax,ax
                rep     stos word ptr es:[di]

@@vxr_initf1:
                mov     word ptr es:[SELZERO],0FFFFh            ; set SELZERO descriptor
                mov     word ptr es:[SELZERO+5],0DF92h

                mov     word ptr es:[SELCALLBACKDS],0FFFFh      ; set callback DS descriptor
                mov     word ptr es:[SELCALLBACKDS+5],0DF92h

                mov     word ptr es:[SELREAL],0FFFFh            ; set real mode attributes descriptor
                mov     word ptr es:[SELREAL+5],01092h

                mov     ax,cs                                   ; set SELCODE32 and SELDATA32
                mov     bx,SEL32CODE                            ; descriptors for cs-based FLAT
                mov     cx,0FFFFh                               ; 
                mov     dx,0DF9Ah                               ;
                call    vxr_initsetdsc                          ;
                mov     ax,cs                                   ; normally creates expand down FLAT
                movzx   dx,_safeMode                            ; with one missing page at 0 address.
                mov     cx,dx                                   ;
                shl     dx,1                                    ;
                mov     di,offset flatdsmode                    ; in safe mode creates 4G data FLAT
                add     di,dx                                   ;
                mov     dx,[di]                                 ;
                neg     cx                                      ; FFFF on 4G, 0 on expand down
                call    vxr_initsetdsc                          ;

                mov     ax,cs                                   ; set SELCODE descriptor (PMODE_TEXT)
                mov     bx,SELCODE                              ; BX = index to SELCODE descriptor
                mov     cx,0FFFFh                               ; CX = limit (64k)
                mov     dx,109Ah                                ; DX = access rights
                call    vxr_initsetdsc

                mov     bx,8*SYSSELECTORS                       ; BX = base of free descriptors
                push    bx                                      ; store selector

                mov     ax,ss                                   ; set caller SS descriptor
                mov     dx,5092h
                call    vxr_initsetdsc

;-----------------------------------------------------------------------------
                mov     cx,SELZERO                              ; CX = ES descriptor, SELZERO
                pop     dx                                      ; DX = SS descriptor, from stack
                mov     ax,dx
;                mov     ax,SELZERO                              ; AX = DS descriptor, SELZERO
                movzx   ebx,sp                                  ; EBX = SP, current SP - same stack
                mov     si,SELCODE                              ; target CS is SELCODE, same segment
                mov     edi,off @@vxr_initf2                    ; target EIP

                jmp     xr_rmtopmsw                             ; jump to mode switch routine
;-----------------------------------------------------------------------------
@@vxr_initf2:                                                   ;
                xor     bx,bx                                   ; init successful, carry clear
                popad                                           ;
                mov     ax,_pm_initerr                          ;

                ret                                             ; 
;-----------------------------------------------------------------------------
vxr_initsetdsc:                                                 ; set descriptor
                movzx   eax,ax                                  ; EAX = base of segment
                shl     eax,4
                mov     word ptr es:[bx],cx                     ; limit = CX
                mov     dword ptr es:[bx+2],eax                 ; base address = EAX
                mov     word ptr es:[bx+5],dx                   ; access rights = DX
                add     bx,8                                    ; increment descriptor index
                retn
_pm_init        endp

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
enablea20       proc    near                                    ; hardware enable gate A20
                pushf
                push    fs
                push    gs
                cli

                xor     ax,ax                                   ; set A20 test segments 0 and 0ffffh
                mov     fs,ax
                dec     ax
                mov     gs,ax

                call    enablea20test                           ; is A20 already enabled?
                jz      short @@enablea20done                   ; if yes, done

                in      al,92h                                  ; PS/2 A20 enable
                or      al,2
                jmp     short $+2
                jmp     short $+2
                jmp     short $+2
                out     92h,al

                call    enablea20test                           ; is A20 enabled?
                jz      short @@enablea20done                   ; if yes, done

                call    enablea20kbwait                         ; AT A20 enable
                jnz     short @@enablea20f0

                mov     al,0D1h
                out     64h,al

                call    enablea20kbwait
                jnz     short @@enablea20f0

                mov     al,0DFh
                out     60h,al

                call    enablea20kbwait

@@enablea20f0:                                                  ; wait for A20 to enable
                mov     cx,800h                                 ; do 800h tries

@@enablea20l0:
                call    enablea20test                           ; is A20 enabled?
                jz      @@enablea20done                         ; if yes, done

                in      al,40h                                  ; get current tick counter
                jmp     short $+2
                jmp     short $+2
                jmp     short $+2
                in      al,40h
                mov     ah,al

@@enablea20l1:                                                  ; wait a single tick
                in      al,40h
                jmp     short $+2
                jmp     short $+2
                jmp     short $+2
                in      al,40h
                cmp     al,ah
                je      @@enablea20l1

                loop    @@enablea20l0                           ; loop for another try

                mov     ax,4                                    ; error, A20 did not enable
                mov     _pm_initerr,ax                          ; error code 4             
@@enablea20done:
                RestReg <gs,fs>
                popf
                ret

;-----------------------------------------------------------------------------
enablea20kbwait:                                                ; wait for safe to write to 8042
                xor     cx,cx
@@enablea20kbwaitl0:
                jmp     short $+2
                jmp     short $+2
                jmp     short $+2
                in      al,64h                                  ; read 8042 status
                test    al,2                                    ; buffer full?
                loopnz  @@enablea20kbwaitl0                     ; if yes, loop
                ret

;-----------------------------------------------------------------------------
enablea20test:                                                  ; test for enabled A20
                mov     al,fs:[0]                               ; get byte from 0:0
                mov     ah,al                                   ; preserve old byte
                not     al                                      ; modify byte
                xchg    al,gs:[10h]                             ; put modified byte to 0ffffh:10h
                cmp     ah,fs:[0]                               ; set zero if byte at 0:0 not modified
                mov     gs:[10h],al                             ; put back old byte at 0ffffh:10h
                ret                                             ; return, zero if A20 enabled
enablea20       endp

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; PROTECTED MODE KERNEL CODE
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
                public  xr_rmtopmsw
xr_rmtopmsw:                                                    ; XMS/raw real to protected switch
                pushfd                                          ; store EFLAGS
                cli
                push    cx                                      ; store CX (protected mode ES)
                push    ax                                      ; store AX (protected mode DS)
                mov     eax,cs:_syscr3                          ;
                or      eax,eax                                 ;
                jz      @@rmtopmf0                              ;
                mov     cr3,eax                                 ; set cr3 and cr4 flags
                mov     eax,cr4                                 ; 
                or      eax,cs:_syscr4                          ;
                mov     cr4,eax                                 ;
                mov     eax,CPU_CR0_PG or CPU_CR0_WP            ; paging on
@@rmtopmf0:
                lidt    fword ptr cs:idtlimit                   ; load protected mode IDT
                or      al,CPU_CR0_PE                           ;
                lgdt    fword ptr cs:gdtlimit                   ; load protected mode GDT

                mov     ecx,cr0                                 ; switch to protected mode
                or      ecx,eax                                 ;
                mov     cr0,ecx                                 ;

                db      0EAh                                    ; JMP FAR PTR SELCODE:$+4
                dw      $+4,SELCODE                             ;  (clear prefetch que)
                pop     ds                                      ; load protected mode DS
                pop     es                                      ; load protected mode ES
                xor     ax,ax                                   ;
                mov     fs,ax                                   ; load protected mode FS with NULL
                mov     gs,ax                                   ; load protected mode GS with NULL
                mov     cx,cs:_systr                            ;
                jcxz    @@rmtopmf1                              ; load task register if available
                str     ax                                      ;
                cmp     ax,cx                                   ; task register was not changed
                jz      @@rmtopmf1                              ;
                mov     fs,cs:_selzero                          ; task register was changed,
                mov     eax,cs:gdtbase                          ; clear busy flag and load
                movzx   ecx,cx                                  ; our`s value
                and     fs:[eax+ecx*8].d_access,D_TSSBUSY_CLR   ;
                xor     ax,ax                                   ;
                mov     fs,ax                                   ;
                ltr     cx                                      ;
@@rmtopmf1:
                pop     eax                                     ;
                mov     ss,dx                                   ; load protected mode SS:ESP
                mov     esp,ebx                                 ;
                and     ah,0BFh                                 ; set NT=0 in old EFLAGS
                push    ax                                      ; set current FLAGS
                popf                                            ;
                push    eax                                     ; store old EFLAGS
                push    esi                                     ; store protected mode target CS
                push    edi                                     ; store protected mode target EIP
                iretd                                           ; go to targed addx in protected mode

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
xr_pmtormsw:                                                    ; XMS/raw protected to real switch
                push    cx                                      ; store CX (real mode ES)
                pushf                                           ; store FLAGS
                cli
                push    ax                                      ; store AX (real mode DS)
                mov     ds,cs:_selzero                          ; DS -> 0 (beginning of memory)
                mov     ecx,cs:codebase                         ; get offset of PMODE_TEXT from 0
                pop     ds:tempw0[ecx]                          ; move real mode DS from stack to temp
                pop     ds:tempw1[ecx]                          ; move FLAGS from stack to temp
                pop     ds:tempw2[ecx]                          ; real mode ES to temp
                str     word ptr [ecx + _systr]                 ;
                mov     ecx,cr0                                 ;
                test    ecx,CPU_CR0_PG                          ; paging enabled?
                jz      @@pmtormf0                              ;
                and     ecx,not (CPU_CR0_PG or CPU_CR0_WP)      ; clear PG and WP bits
                mov     cr0,ecx                                 ;
                xor     eax,eax                                 ; flush TLB
                mov     cr3,eax                                 ;
                mov     eax,cr4                                 ; do it on every call
                and     ax,not (CPU_CR4_PAE or CPU_CR4_PGE)     ; else we trap NTLDR on partition
                mov     cr4,eax                                 ; boot ;)))
@@pmtormf0:
                mov     ax,SELREAL                              ; load descriptors with real mode seg
                mov     ds,ax                                   ;  attributes
                mov     es,ax
                mov     fs,ax
                mov     gs,ax
                mov     ss,ax                                   ; load descriptor with real mode attr
                movzx   esp,bx                                  ; load real mode SP, high word 0
                lidt    fword ptr cs:rmidtlimit                 ; load real mode IDT
                and     cl,0FEh                                 ; switch to real mode
                mov     cr0,ecx
                db      0EAh                                    ; JMP FAR PTR PMODE_TEXT:$+4
                dw      $+4                                     ;  (clear prefetch que)
xr_pmtormsw_cs  label   near
                dw      0                                       ; cs
                mov     ss,dx                                   ; load real mode SS
                mov     ds,cs:tempw0                            ; load real mode DS
                mov     es,cs:tempw2                            ; load real mode ES
                xor     ax,ax
                mov     fs,ax                                   ; load real mode FS with NULL
                mov     gs,ax                                   ; load real mode GS with NULL
                push    cs:tempw1                               ; store old FLAGS
                push    si                                      ; store real mode target CS
                push    di                                      ; store real mode target IP
                iret                                            ; go to target addx in real mode

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
vxr_saverestorerm:                                              ; VCPI/XMS/raw save/restore status
                retf                                            ; no save/restore needed, 16bit RETF

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
vxr_saverestorepm:                                              ; VCPI/XMS/raw save/restore status
                db      66h,0CBh                                ; no save/restore needed, 32bit RETF

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
critical_error:                                                 ; some unrecoverable error
                cli                                             ; make sure we are not interrupted
                in      al,61h                                  ; beep
                or      al,3
                out     61h,al
                jmp     $                                       ; now hang

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
callback:                                                       ; real mode callback handler
                mov     ax,sp                                   ; preserve SS:SP for callback
                push    ss
                push    ax

                SaveReg <gs,fs,ds,es>                           ; preserve real mode regs for callback
                pushf                                           ; preserve FLAGS for callback

                cli
                cld

                mov     ebp,cs:pmstacktop                       ; EBP = ESP for protected mode

                mov     ebx,ebp                                 ; set EBX to next stack location
                sub     ebx,cs:pmstacklen
                mov     cs:pmstacktop,ebx                       ; update ptr for possible reenterancy

                cmp     ebx,cs:pmstackbase                      ; exceeded protected mode stack space?
                jb      critical_error                          ; if yes, critical error (hang)

                xor     eax,eax                                 ; EAX = base address of SS
                mov     ax,ss
                shl     eax,4

                movzx   ebx,sp                                  ; EBX = current linear SS:SP
                add     ebx,eax

                mov     es,cs:gdtseg                            ; set for protected mode callback DS
                or      eax,92000000h                           ;  base address in GDT
                mov     es:[SELCALLBACKDS+2],eax

                mov     ax,SELZERO                              ; DS selector for protected mode
                mov     dx,ax                                   ; SS selector = DS selector
                mov     si,SELCODE                              ; target protected mode CS:EIP
                mov     edi,offset @@callbackf0

                jmp     xr_rmtopmsw                             ; go to protected mode

@@callbackf0:
                mov     edi,[esp+14]                            ; EDI -> register structure from stack

                lea     esi,[esp+24]                            ; copy general registers from stack
                mov     ecx,8                                   ;  to register structure
                rep     movs dword ptr es:[edi],dword ptr ds:[esi]

                mov     esi,esp                                 ; copy FLAGS, ES, DS, FG, and GS
                movs    word ptr es:[edi],word ptr ds:[esi]
                movs    dword ptr es:[edi],dword ptr ds:[esi]
                movs    dword ptr es:[edi],dword ptr ds:[esi]

                lods    dword ptr ds:[esi]                      ; EAX = real mode SS:SP from stack
                add     ax,42                                   ; adjust SP for stuff on stack
                mov     es:[edi+4],eax                          ; put in register structure

                mov     ds,cs:_selcallbackds                    ; DS = callback DS selector
                sub     edi,42                                  ; EDI -> register structure
                movzx   esi,ax                                  ; ESI = old real mode SP
                xchg    esp,ebp                                 ; ESP = protected mode stack

                pushfd                                          ; push flags for IRETD from callback
                db      66h                                     ; push 32bit CS for IRETD
                push    cs
                dw      6866h,@@callbackf1,0                    ; push 32bit EIP for IRETD

                movzx   eax,word ptr [ebp+22]                   ; EAX = target CS of callback
                push    eax                                     ; push 32bit CS for RETF to callback
                push    dword ptr [ebp+18]                      ; push 32bit EIP for retf

                db      66h                                     ; 32bit RETF to callback
                retf

@@callbackf1:
                cli
                cld

                push    es                                      ; DS:ESI = register structure
                pop     ds
                mov     esi,edi

                mov     es,cs:_selzero                          ; ES -> 0 (beginning of memory)

                movzx   ebx,word ptr [esi+2Eh]                  ; EBX = real mode SP from structure
                movzx   edx,word ptr [esi+30h]                  ; EDX = real mode SS from structure
                sub     bx,42                                   ; subtract size of vars to be put

                mov     ebp,[esi+0ch]                           ; EBP = pushed ESP from real mode
                mov     bp,bx                                   ; EBP = old high & new low word of ESP

                lea     edi,[edx*4]                             ; EDI -> real mode base of stack
                lea     edi,[edi*4+ebx]                         ;  of vars to be stored

                mov     ecx,8                                   ; copy general registers to stack
                rep     movs dword ptr es:[edi],dword ptr ds:[esi]

                mov     eax,[esi+6]                             ; EAX = return FS and GS for real mode
                mov     es:[edi],eax                            ; store on real mode stack for return

                mov     ax,[esi]                                ; AX = return FLAGS for real mode
                mov     es:[edi+8],ax                           ; store on real mode stack for return
                mov     eax,[esi+10]                            ; EAX = return CS:IP for real mode
                mov     es:[edi+4],eax                          ; store on real mode stack for return

                mov     ax,[esi+4]                              ; AX = return DS for real mode
                mov     cx,[esi+2]                              ; CX = return ES for real mode

                mov     si,cs:_rm16code                         ; real mode target CS:IP
                mov     di,off @@callbackf2

                db      66h                                     ; JMP DWORD PTR, as in 32bit offset,
                jmp     word ptr cs:pmtormswrout                ;  not seg:16bit offset

@@callbackf2:
                mov     esp,ebp                                 ; restore total ESP, old high word

                mov     eax,cs:pmstacklen                       ; restore top of protected mode stack
                add     cs:pmstacktop,eax

                popad                                           ; get callback return general regs
                RestReg <fs,gs>                                 ; get callback return FS and GS values
                iret                                            ; go to callback return CS:IP

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
intr:                                                           ; general interrupt redirector
                cmp     word ptr [esp+4],SELCODE                ; INT redirection or internal INT 3?
                je      short @@intrf0                          ; if not INT 3, jump to redirection

                jmp     fword ptr cs:int3vector                 ; INT 3, jump to INT 3 vector

@@intrf0:
                mov     [esp+8],eax                             ; store EAX for later POPAD
                mov     eax,[esp]                               ; get address in redirection matrix
                add     esp,8                                   ; discard EIP and CS from INT 3
                SaveReg <ecx,edx,ebx,esp,ebp,esi,edi>           ; store rest of registers for POPAD
                SaveReg <ds,es,fs,gs>

                mov     ds,cs:_selzero                          ; DS -> 0 (beginning of memory)
                mov     edi,cs:codebase                         ; EDI = offset of PMODE_TEXT from 0

                mov     dx,cs:rmstacktop                        ; DX = SS for real mode redirection
                movzx   ebp,dx                                  ; EBP -> top of real mode stack
                shl     ebp,4

                mov     bx,cs:_pm_rmstacklen                    ; get size of real mode stack
                sub     dx,bx                                   ; adjust DX to next stack location
                mov     ds:rmstacktop[edi],dx                   ; update ptr for possible reenterancy
                shl     bx,4                                    ; set real mode SP to top of stack

                cmp     dx,cs:rmstackbase                       ; exceeded real mode stack space?
                jb      critical_error                          ; if yes, critical error (hang)

                mov     ds:[ebp-2],ss                           ; store SS:ESP on real mode stack
                mov     ds:[ebp-6],esp

                sub     ax,off intrmatrix+1                     ; AX = int number
                mov     ah,al                                   ; AH = high 5 bits of int number
                and     ah,0f8h

                cmp     ah,cs:picslave                          ; high IRQ?
                je      short intrirq                           ; if yes, do IRQ
                cmp     ah,cs:picmaster                         ; low IRQ?
                jne     short intrint                           ; if no, do INT (with general regs)

;-----------------------------------------------------------------------------
intrirq:                                                        ; an IRQ redirection
                mov     ds:@@intrirqintnum[edi],al              ; modify code with interrupt number

                mov     si,cs:_rm16code                         ; real mode target CS:IP
                mov     di,off @@intrirqf0
                sub     bx,6                                    ; adjust real mode SP for stored vars

                db      66h                                     ; JMP DWORD PTR, as in 32bit offset,
                jmp     word ptr cs:pmtormswrout                ;  not seg:16bit offset

@@intrirqf0:
                db      0cdh                                    ; INT @@intrirqintnum
@@intrirqintnum db      ?

                mov     ax,SELZERO                              ; DS selector value for protected mode
                mov     cx,ax                                   ; ES selector value for protected mode
                pop     ebx                                     ; get protected mode SS:ESP from stack
                pop     dx
                mov     si,SELCODE                              ; target CS:EIP in protected mode
                mov     edi,off @@intrirqf1

                jmp     xr_rmtopmsw                             ; go back to protected mode

@@intrirqf1:
                mov     edi,cs:codebase                         ; restore top of real mode stack
                mov     ax,cs:_pm_rmstacklen
                add     ds:rmstacktop[edi],ax

                RestReg <gs,fs,es,ds>                           ; restore all registers
                popad
                iretd

;-----------------------------------------------------------------------------
intrint:                                                        ; an INT redirection
                mov     ds:@@intrintintnum[edi],al              ; modify code with interrupt number

                mov     es,cs:_selzero                          ; copy registers from protected mode
                lea     edi,[ebp-26h]                           ;  stack to real mode stack
                lea     esi,[esp+8]
                mov     ecx,8
                cld
                rep     movs dword ptr es:[edi],dword ptr ss:[esi]

                mov     si,cs:_rm16code                         ; real mode target CS:IP
                mov     di,off @@intrintf0
                sub     bx,26h                                  ; adjust real mode SP for stored vars

                db      66h                                     ; JMP DWORD PTR, as in 32bit offset,
                jmp     word ptr cs:pmtormswrout                ;  not seg:16bit offset

@@intrintf0:
                popad                                           ; load regs with int call values

                db      0cdh                                    ; INT @@intrirqintnum
@@intrintintnum db      ?

                pushad                                          ; store registers on stack
                pushf                                           ; store flags on stack
                cli

                xor     eax,eax                                 ; EAX = linear ptr to SS
                mov     ax,ss
                shl     eax,4
                movzx   ebp,sp                                  ; EBP = SP

                mov     ebx,[bp+22h]                            ; get protected mode SS:ESP from stack
                mov     dx,[bp+26h]

                add     ebp,eax                                 ; EBP -> stored regs on stack

                mov     ax,SELZERO                              ; DS selector value for protected mode
                mov     cx,ax                                   ; ES selector value for protected mode
                mov     si,SELCODE                              ; target CS:EIP in protected mode
                mov     edi,off @@intrintf1

                jmp     xr_rmtopmsw                         ; go back to protected mode

@@intrintf1:
                mov     edi,cs:codebase                         ; restore top of real mode stack
                mov     ax,cs:_pm_rmstacklen
                add     ds:rmstacktop[edi],ax

                mov     ax,ds:[ebp]                             ; move return FLAGS from real mode
                and     ax,8d5h                                 ;  stack to protected mode stack
                mov     bx,[esp+30h]
                and     bx,not 8d5h
                or      ax,bx
                mov     [esp+30h],ax

                mov     edi,ds:[ebp+2]                          ; restore return registers from real
                mov     esi,ds:[ebp+6]                          ;  mode stack
                mov     ebx,ds:[ebp+18]
                mov     edx,ds:[ebp+22]
                mov     ecx,ds:[ebp+26]
                mov     eax,ds:[ebp+30]
                mov     ebp,ds:[ebp+10]

                RestReg <gs,fs,es,ds>                           ; restore segment regs
                add     esp,20h                                 ; skip old general registers on stack
                iretd

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; INT 31h INTERFACE
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

;ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
int31:                                                          ; protected mode INT 31h handler
                cli
                cld
                SaveReg <ds,es,fs,gs>                           ; push regs needed
                pushad
                mov     ds,cs:_selzero                          ; DS -> 0 (beginning of memory)

                push    bx
                mov     bx,(INT31FUNCNUM - 1) * 2               ; number of functions to check
@@int31l0:
                cmp     ax,cs:int31functbl[bx]                  ; found function value?
                jne     short @@int31l0c

                mov     bx,cs:int31routtbl[bx]                  ; yes, go to appropriate handler
                xchg    bx,[esp]
                ret

@@int31l0c:
                sub     bx,2                                    ; no, continue loop
                jnc     @@int31l0

                pop     bx                                      ; no function found
                jmp     int31fail8001                           ; error 8001h
;-----------------------------------------------------------------------------
int31fail8024:                                                  ; INT 31h return fail with error 8024h
                mov     word ptr [esp+28],8024h                 ; set AX on stack to 8024h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8023:                                                  ; INT 31h return fail with error 8023h
                mov     word ptr [esp+28],8023h                 ; set AX on stack to 8023h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8022:                                                  ; INT 31h return fail with error 8022h
                mov     word ptr [esp+28],8022h                 ; set AX on stack to 8022h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8021:                                                  ; INT 31h return fail with error 8021h
                mov     word ptr [esp+28],8021h                 ; set AX on stack to 8021h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8016:                                                  ; INT 31h return fail with error 8016h
                mov     word ptr [esp+28],8016h                 ; set AX on stack to 8016h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8015:                                                  ; INT 31h return fail with error 8015h
                mov     word ptr [esp+28],8015h                 ; set AX on stack to 8015h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8013:                                                  ; INT 31h return fail with error 8013h
                mov     word ptr [esp+28],8013h                 ; set AX on stack to 8013h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8012:                                                  ; INT 31h return fail with error 8012h
                mov     word ptr [esp+28],8012h                 ; set AX on stack to 8012h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8011:                                                  ; INT 31h return fail with error 8011h
                mov     word ptr [esp+28],8011h                 ; set AX on stack to 8011h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8010:                                                  ; INT 31h return fail with error 8010h
                mov     word ptr [esp+28],8010h                 ; set AX on stack to 8010h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31fail8001:                                                  ; INT 31h return fail with error 8001h
                mov     word ptr [esp+28],8001h                 ; set AX on stack to 8001h for POPAD
                jmp     short int31fail
;-----------------------------------------------------------------------------
int31failcx:                                                    ; INT 31h return fail with CX,AX
                mov     word ptr [esp+24],cx                    ; put CX onto stack for POPAD
;-----------------------------------------------------------------------------
int31failax:                                                    ; INT 31h return fail with AX
                mov     word ptr [esp+28],ax                    ; put AX onto stack for POPAD
;-----------------------------------------------------------------------------
int31fail:                                                      ; INT 31h return fail, pop all regs
                popad
                RestReg <gs,fs,es,ds>
;-----------------------------------------------------------------------------
int21:                                                          ; for someone who call int 21h ;)
int31failnopop:                                                 ; INT 31h return fail with carry set
                or      byte ptr [esp+8],1                      ; set carry in EFLAGS on stack
                iretd
;-----------------------------------------------------------------------------
int31okedx:                                                     ; INT 31h return ok with EDX,CX,AX
                mov     [esp+20],edx                            ; put EDX onto stack for POPAD
                jmp     short int31okcx
;-----------------------------------------------------------------------------
int31okdx:                                                      ; INT 31h return ok with DX,CX,AX
                mov     [esp+20],dx                             ; put DX onto stack for POPAD
                jmp     short int31okcx
;-----------------------------------------------------------------------------
int31oksinoax:                                                  ; INT 31h return ok SI,DI,BX,CX
                mov     ax,[esp+28]                             ; get old value of AX for restore
;-----------------------------------------------------------------------------
int31oksi:                                                      ; INT 31h return ok SI,DI,BX,CX,AX
                mov     [esp+4],si                              ; put SI onto stack for POPAD
                mov     [esp],di                                ; put DI onto stack for POPAD
                mov     [esp+16],bx                             ; put BX onto stack for POPAD
;-----------------------------------------------------------------------------
int31okcx:                                                      ; INT 31h return ok with CX,AX
                mov     [esp+24],cx                             ; put CX onto stack for POPAD
;-----------------------------------------------------------------------------
int31okax:                                                      ; INT 31h return ok with AX
                mov     [esp+28],ax                             ; put AX onto stack for POPAD
;-----------------------------------------------------------------------------
int31ok:                                                        ; INT 31h return ok, pop all regs
                popad
                RestReg <gs,fs,es,ds>
;-----------------------------------------------------------------------------
int31oknopop:                                                   ; INT 31h return ok with carry clear
                and     byte ptr [esp+8],0feh                   ; clear carry in EFLAGS on stack
                iretd

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; DESCRIPTOR FUNCTIONS
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

;-----------------------------------------------------------------------------
int31testsel:                                                   ; test for valid selector BX
                pop     bp                                      ; pop return address

                cmp     bx,cs:gdtlimit                          ; selector BX out of range?
                ja      int31fail8022                           ; if yes, fail with error 8022h

                mov     edi,cs:gdtbase                          ; get base of GDT
                and     bl,0f8h                                 ; mask offset table index and RPL
                movzx   ebx,bx                                  ; EBX = selector index
                test    byte ptr ds:[edi+ebx+6],10h             ; is descriptor used?
                jz      int31fail8022                           ; if descriptor not used, fail 8022h

                jmp     bp                                      ; return ok

;-----------------------------------------------------------------------------
int31testaccess:                                                ; test access bits in CX
                pop     bp                                      ; pop return address

                test    ch,20h                                  ; test MUST BE 0 bit in CH
                jnz     int31fail8021                           ; if not 0, error 8021h

                test    cl,80h                                  ; test present bit
                jz      int31fail8021                           ; if 0, error 8021h

                test    cl,10h                                  ;
                jz      @@int31testselok                        ; system? skip tests

                test    cl,8                                    ; if code, more tests needed
                jz      @@int31testselok                        ; if data, skip code tests

                test    cl,6                                    ; test conforming and readable bits
                jz      int31fail8021                           ; if both 0, error 8021h
                jpe     int31fail8021                           ; if equal, error 8021h

@@int31testselok:

                jmp     bp                                      ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310000:                                                      ; allocate descriptors
                or      cx,cx                                   ; if CX = 0, error 8021h
                jz      int31fail8021

                mov     edx,cs:gdtbase                          ; get base of GDT
                movzx   eax,cs:gdtlimit                         ; EAX = last selector index
                and     al,0f8h

                mov     bx,cx                                   ; BX = number of selectors to find
@@int310000l0:
                test    byte ptr ds:[edx+eax+6],10h             ; is descriptor used?
                jnz     short @@int310000l0f0

                dec     bx                                      ; found free descriptor, dec counter
                jnz     short @@int310000l0f1                   ; continue if need to find more

                mov     ebx,eax                                 ; found all descriptors requested
@@int310000l1:
                mov     dword ptr ds:[edx+ebx],0                ; set entire new descriptor
                mov     dword ptr ds:[edx+ebx+4],109200h
                add     bx,8                                    ; increment selector index
                dec     cx                                      ; dec counter of descriptors to mark
                jnz     @@int310000l1                           ; loop if more to mark

                jmp     int31okax                               ; return ok, with AX

@@int310000l0f0:
                mov     bx,cx                                   ; reset number of selectors to find

@@int310000l0f1:
                sub     ax,8                                    ; dec current selector counter
                cmp     ax,8*SYSSELECTORS                       ; more descriptors to go?
                jae     @@int310000l0                           ; if yes, loop

                jmp     int31fail8011                           ; did not find descriptors

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310001:                                                      ; free descriptor
                call    int31testsel                            ; test for valid selector BX

                and     byte ptr ds:[edi+ebx+6],0efh            ; mark descriptor as free

                mov     cx,4                                    ; zero any segregs loaded with BX
                lea     ebp,[esp+32]                            ; EBP -> selectors on stack
@@int310001l0:
                cmp     word ptr [ebp],bx                       ; selector = BX?
                jne     short @@int310001l0f0                   ; if no, continue loop

                mov     word ptr [ebp],0                        ; zero selector on stack

@@int310001l0f0:
                add     ebp,2                                   ; increment selector ptr
                loop    @@int310001l0                           ; loop

                jmp     int31ok                                 ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310002:                                                      ; segment to descriptor
                mov     cx,1                                    ; allocate descriptor
                xor     ax,ax                                   ;
                int     31h                                     ;
                jc      int31fail8011                           ; if failed, descriptor unavailable

                push    ax                                      ; preserve allocated selector

                mov     dx,bx                                   ; real mode segment to BX
                xor     cx,cx                                   ; clear CX
                shld    cx,dx,4                                 ; high 4 bits into DX
                shl     cx,4                                    ; segment to base address
                mov     bx,ax                                   ; allocated selector to BX
                push    bx                                      ; and preserve selector again

                mov     ax,7                                    ; set segment base address
                int     31h                                     ;

                pop     bx                                      ; restore selector
                xor     dx,dx                                   ; 64K segment limit in cx:dx
                mov     cx,1                                    ;
                mov     ax,8                                    ; set segment limit
                int     31h                                     ;

                pop     ax                                      ; restore allocated selector

                jmp     int31okax                               ; return ok, with AX

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310003:                                                      ; get selector increment value
                mov     ax,8                                    ; selector increment value is 8
                jmp     int31okax                               ; return ok, with AX

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310006:                                                      ; get segment base address
                call    int31testsel                            ; test for valid selector BX

                mov     dx,word ptr ds:[edi+ebx+2]              ; low word of 32bit linear address
                mov     cl,byte ptr ds:[edi+ebx+4]              ; high word of 32bit linear address
                mov     ch,byte ptr ds:[edi+ebx+7]

                jmp     int31okdx                               ; return ok, with DX, CX, AX

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310007:                                                      ; set segment base address
                call    int31testsel                            ; test for valid selector BX

                mov     word ptr ds:[edi+ebx+2],dx              ; low word of 32bit linear address
                mov     byte ptr ds:[edi+ebx+4],cl              ; high word of 32bit linear address
                mov     byte ptr ds:[edi+ebx+7],ch

                jmp     int31ok                                 ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310008:                                                      ; set segment limit
                call    int31testsel                            ; test for valid selector BX

                cmp     cx,0fh                                  ; limit greater than 1M?
                jbe     short @@int310008f0

                mov     ax,dx                                   ; yup, limit greater than 1M
                and     ax,0fffh
                cmp     ax,0fffh                                ; low 12 bits set?
                jne     int31fail8021                           ; if no, error 8021h

                shrd    dx,cx,12                                ; DX = low 16 bits of page limit
                shr     cx,12                                   ; CL = high 4 bits of page limit
                or      cl,80h                                  ; set granularity bit in CL

@@int310008f0:
                mov     word ptr ds:[edi+ebx],dx                ; put low word of limit
                and     byte ptr ds:[edi+ebx+6],70h             ; mask off G and high nybble of limit
                or      byte ptr ds:[edi+ebx+6],cl              ; put high nybble of limit

                jmp     int31ok                                 ; return ok
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310009:                                                      ; set descriptor access rights
                call    int31testsel                            ; test for valid selector BX

                call    int31testaccess                         ; test access bits in CX

                or      ch,10h                                  ; set AVL bit, descriptor used
                and     ch,0f0h                                 ; mask off low nybble of CH
                and     byte ptr ds:[edi+ebx+6],0fh             ; mask off high nybble access rights
                or      byte ptr ds:[edi+ebx+6],ch              ; or in high access rights byte
                mov     byte ptr ds:[edi+ebx+5],cl              ; put low access rights byte

                jmp     int31ok                                 ; return ok
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int31000a:                                                      ; create alias descriptor
                call    int31testsel                            ; test for valid selector BX

                mov     ax,0000h                                ; allocate descriptor
                mov     cx,1
                int     31h
                jc      int31fail8011                           ; if failed, descriptor unavailable

                push    ax                                      ; preserve allocated selector

                push    ds                                      ; copy descriptor and set type data
                pop     es
                movzx   edi,ax                                  ; EDI = target selector
                mov     esi,cs:gdtbase                          ; ESI -> GDT
                add     edi,esi                                 ; adjust to target descriptor in GDT
                add     esi,ebx                                 ; adjust to source descriptor in GDT

                movs    dword ptr es:[edi],dword ptr ds:[esi]      ; copy descriptor
                lods    dword ptr ds:[esi]
                mov     ah,92h                                  ; set descriptor type - R/W up data
                stos    dword ptr es:[edi]

                pop     ax                                      ; restore allocated selector

                jmp     int31okax                               ; return ok, with AX
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int31000b:                                                      ; get descriptor
                call    int31testsel                            ; test for valid selector BX

                lea     esi,[edi+ebx]                           ; ESI -> descriptor in GDT
                mov     edi,[esp]                               ; get EDI buffer ptr from stack
                movs    dword ptr es:[edi],dword ptr ds:[esi]      ; copy descriptor
                movs    dword ptr es:[edi],dword ptr ds:[esi]

                jmp     int31ok                                 ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int31000c:                                                      ; set descriptor
                call    int31testsel                            ; test for valid selector BX

                mov     esi,[esp]                               ; ESI = EDI buffer ptr from stack
                mov     cx,es:[esi+5]                           ; get access rights from descriptor
                call    int31testaccess                         ; test access bits in CX

                push    ds                                      ; swap DS and ES, target and source
                push    es
                pop     ds
                pop     es

                add     edi,ebx                                 ; adjust EDI to descriptor in GDT
                movs    dword ptr es:[edi],dword ptr ds:[esi]   ; copy descriptor
                lods    dword ptr ds:[esi]
                or      eax,100000h                             ; set descriptor AVL bit
                stos    dword ptr es:[edi]

                jmp     int31ok                                 ; return ok
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int31000e:                                                      ; get multiple descriptors
                mov     ax,000bh                                ; function 000bh, get descriptor
;-----------------------------------------------------------------------------
int31000ef:                                                     ; common to funcions 000eh and 000fh
                or      cx,cx                                   ; if CX = 0, return ok immediately
                jz      int31ok

                mov     dx,cx                                   ; DX = number of descriptors
                xor     cx,cx                                   ; CX = successful counter
@@int31000efl0:
                mov     bx,es:[edi]                             ; BX = selector to get
                add     edi,2

                int     31h                                     ; get/set descriptor
                jc      int31failcx                             ; if error, fail with AX and CX

                add     edi,8                                   ; increment descriptor ptr
                inc     cx                                      ; increment successful copy counter
                dec     dx                                      ; decrement loop counter
                jnz     @@int31000efl0                          ; if more descriptors to go, loop

                jmp     int31ok                                 ; return ok
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int31000f:                                                      ; set multiple descriptors
                mov     ax,000ch                                ; function 000ch, set descriptor

                jmp     int31000ef                              ; go to common function

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; INTERRUPT FUNCTIONS
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310200:                                                      ; get real mode interrupt vector
                movzx   ebx,bl                                  ; EBX = BL (interrupt number)
                mov     dx,ds:[ebx*4]                           ; load real mode vector offset
                mov     cx,ds:[ebx*4+2]                         ; load real mode vector segment

                jmp     int31okdx                               ; return ok, with AX, CX, DX

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310201:                                                      ; set real mode interrupt vector
                movzx   ebx,bl                                  ; EBX = BL (interrupt number)
                mov     ds:[ebx*4],dx                           ; set real mode vector offset
                mov     ds:[ebx*4+2],cx                         ; set real mode vector segment

                jmp     int31ok                                 ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310204:                                                      ; get protected mode interrupt vector
                cmp     bl,3                                    ; INT 3 vector?
                je      short @@int310204f00                    ; if yes, go to special INT 3 handling

                movzx   ebx,bl                                  ; EBX = BL (interrupt number)
                shl     ebx,3                                   ; adjust for location in IDT
                add     ebx,cs:idtbase                          ; add base of IDT

                mov     edx,dword ptr ds:[ebx+4]                ; get high word of offset
                mov     dx,word ptr ds:[ebx]                    ; get low word of offset
                mov     cx,word ptr ds:[ebx+2]                  ; get selector

                jmp     int31okedx                              ; return ok, with AX, CX, EDX

@@int310204f00:
                mov     edx,dword ptr cs:int3vector[0]          ; get offset of INT 3
                mov     cx,word ptr cs:int3vector[4]            ; get selector of INT 3

                jmp     int31okedx                              ; return ok, with AX, CX, EDX

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
; reset descriptor type here, because it can be changed by sys_intgate()
;
int310205:                                                      ; set protected mode interrupt vector
                xchg    bx,cx                                   ; swap int number with int selector
                call    int31testsel                            ; test for valid selector BX

                cmp     cl,3                                    ; INT 3 vector?
                je      short @@int310205f01                    ; if yes, go to special INT 3 handling

                movzx   ecx,cl                                  ; ECX = CL (interrupt number)
                mov     ax,cx                                   ;
                and     al,0F8h                                 ; isolate high 5 bits of int num

                test    cl,0F8h                                 ; one of the low 8 interrupts?
                jz      short @@int310205f00                    ; if yes, store as interrupt gate

                cmp     al,cs:picslave                          ; one of the high IRQs?
                je      short @@int310205f00                    ; if yes, store as interrupt gate
                cmp     al,cs:picmaster                         ; one of the low IRQs?
                je      short @@int310205f00                    ; if yes, store as interrupt gate

                or      ah,1                                    ; set to trap gate type
@@int310205f00:
                shl     ecx,3                                   ; adjust for location in IDT
                add     ecx,cs:idtbase                          ; add base of IDT

                mov     ds:[ecx],dx                             ; set low word of offset
                mov     dx,8E00h                                ; interrupt gate type
                or      dh,ah
                mov     ds:[ecx+2],bx                           ; set selector
                mov     ds:[ecx+4],edx                          ; set high word of offset + flags

                jmp     int31ok                                 ; return ok

@@int310205f01:
                mov     edi,cs:codebase                         ; EDI = offset of PMODE_TEXT from 0

                mov     dword ptr ds:int3vector[edi+0],edx      ; set offset of INT 3
                mov     word ptr ds:int3vector[edi+4],bx        ; set selector of INT 3

                jmp     int31ok                                 ; return ok

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310900:                                                      ; get and disable interrupt state
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                btc     word ptr [esp+8],9                      ; test and clear IF bit in EFLAGS
                setc    al                                      ; set AL = carry (IF flag from EFLAGS)

                jmp     int31oknopop                            ; return ok, dont pop registers

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310901:                                                      ; get and enable interrupt state
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                bts     word ptr [esp+8],9                      ; test and set IF bit in EFLAGS
                setc    al                                      ; set AL = carry (IF flag from EFLAGS)

                jmp     int31oknopop                            ; return ok, dont pop registers

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310902:                                                      ; get interrupt state
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                bt      word ptr [esp+8],9                      ; just test IF bit in EFLAGS
                setc    al                                      ; set AL = carry (IF flag from EFLAGS)

                jmp     int31oknopop                            ; return ok, dont pop registers

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; REAL/PROTECTED MODE TRANSLATION FUNCTIONS
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±

;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310300:                                                      ; simulate real mode interrupt
                movzx   ebx,bl                                  ; get real mode INT CS:IP
                mov     ebp,dword ptr ds:[ebx*4]

                jmp     short int3103                           ; go to common code
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310301:                                                      ; call real mode FAR procedure
                                                                ; same start as function 0302h
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310302:                                                      ; call real mode IRET procedure
                mov     ebp,dword ptr es:[edi+2ah]              ; get target CS:IP from structure
;-----------------------------------------------------------------------------
int3103:                                                        ; common to 0300h, 0301h, and 0302h
                mov     esi,cs:codebase                         ; ESI = offset of PMODE_TEXT from 0

                movzx   ebx,word ptr es:[edi+2eh]               ; EBX = SP from register structure
                movzx   edx,word ptr es:[edi+30h]               ; EDX = SS from register structure

                mov     ax,bx                                   ; check if caller provided stack
                or      ax,dx
                jnz     short @@int3103f3                       ; if yes, go on to set stack

                mov     dx,cs:rmstacktop                        ; DX = SS for real mode redirection
                mov     bx,cs:_pm_rmstacklen                    ; get size of real mode stack
                sub     dx,bx                                   ; adjust DX to next stack location

                cmp     dx,cs:rmstackbase                       ; exceeded real mode stack space?
                jb      int31fail8012                           ; if yes, error 8012h

                mov     ds:rmstacktop[esi],dx                   ; update ptr for possible reenterancy
                shl     bx,4                                    ; adjust BX from paragraphs to bytes

@@int3103f3:
                lea     edi,[edx*4]                             ; EDI -> top of real mode stack
                lea     edi,[edi*4+ebx]

                lea     ax,[bx-8]                               ; AX = top of stack parms
                xchg    ax,ds:rmstackparmtop[esi]               ; preserve and set new top of stack
                push    ax                                      ;  parms for possible reenterancy

                movzx   ax,byte ptr [esp+30]                    ; AX = AL of original INT 31h call
                and     al,1                                    ; if function 0301h, AL=0, else, AL=2
                xor     al,1
                shl     al,1
                sub     bx,ax                                   ; adjust BX for possible FLAGS

                movzx   eax,cx                                  ; EAX = length of stack parms
                shl     eax,1
                sub     bx,36h                                  ; adjust real mode SP for needed vars
                sub     bx,ax                                   ; adjust real mode SP for stack parms

                mov     ds:[edi-2],ss                           ; store SS:ESP on real mode stack
                mov     ds:[edi-6],esp
                mov     ds:[edi-8],es                           ; store ES on real mode stack

                push    ds                                      ; swap DS and ES
                push    es
                pop     ds
                pop     es

                std                                             ; string copy backwards

                sub     edi,10                                  ; copy stack parms from protected mode
                movzx   ecx,cx                                  ;  stack to real mode stack
                lea     esi,[esp+ecx*2+36h-2]
                rep     movs word ptr es:[edi],word ptr ss:[esi]

                mov     esi,[esp+2]                             ; ESI = offset of structure from stack
                mov     ax,[esi+20h]                            ; AX = FLAGS from register structure

                mov     es:[edi],ax                             ; store data for real mode return IRET

                cmp     byte ptr [esp+30],1                     ; check AL on stack for function code
                je      short @@int3103f4                       ; if function 0301h, go on

                and     ah,0fch                                 ; 0300h or 0302h, clear IF and TF flag

@@int3103f4:
                cld                                             ; string copy forward
                lea     edi,[edx*4]                             ; EDI -> bottom of stack
                lea     edi,[edi*4+ebx]

                mov     ecx,8                                   ; copy general regs to real mode stack
                rep     movs dword ptr es:[edi],dword ptr ds:[esi]

                add     esi,6                                   ; copy FS and GS to real mode stack
                movs    dword ptr es:[edi],dword ptr ds:[esi]

                mov     cx, cs:_rm16code
                mov     word ptr es:[edi+8],cx                  ; return address from call
                mov     word ptr es:[edi+6],off @@int3103f1

                mov     es:[edi+4],ax                           ; store FLAGS for real mode IRET maybe
                mov     dword ptr es:[edi],ebp                  ; put call address to real mode stack

                mov     ax,[esi-6]                              ; real mode DS from register structure
                mov     cx,[esi-8]                              ; real mode ES from register structure

                mov     si,cs:_rm16code                         ; real mode target CS:IP
                mov     di,off @@int3103f0

                db      66h                                     ; JMP DWORD PTR, as in 32bit offset,
                jmp     word ptr cs:pmtormswrout                ;  not seg:16bit offset

@@int3103f0:                                                    ; real mode INT, FAR, or IRET call
                popad                                           ; load regs with call values
                RestReg <fs,gs>

                iret                                            ; go to call address

@@int3103f1:
                mov     sp,cs:rmstackparmtop                    ; remove stack parameters

                SaveReg <gs,fs,ds,es>                           ; store registers on stack
                pushf                                           ; store flags on stack
                pushad
                call    enablea20                               ; ugly int15 calls can disable it again
                cli

                mov     ax,ss                                   ; EAX = linear ptr to SS
                movzx   eax,ax
                shl     eax,4
                movzx   ebp,sp                                  ; EBP = SP

                mov     cx,[bp+2ah]                             ; get protected mode ES from stack
                mov     ebx,[bp+2ch]                            ; get protected mode SS:ESP from stack
                mov     dx,[bp+30h]

                add     ebp,eax                                 ; EBP -> stored regs on stack

                mov     ax,SELZERO                              ; DS selector value for protected mode
                mov     si,SELCODE                              ; target CS:EIP in protected mode
                mov     edi,off @@int3103f2

                jmp     xr_rmtopmsw                             ; go back to protected mode

@@int3103f2:
                mov     edi,cs:codebase                         ; restore old stack parameter length
                pop     ds:rmstackparmtop[edi]

                mov     edi,[esp]                               ; get structure offset from stack
                mov     esi,ebp                                 ; copy return regs from real mode
                mov     ecx,15h                                 ;  stack to register structure
                cld
                rep     movs word ptr es:[edi],word ptr ds:[esi]

                cmp     dword ptr es:[edi+4],0                  ; stack provided by caller?
                jne     int31ok                                 ; if yes, done now

                mov     edi,cs:codebase                         ; restore top of real mode stack
                mov     ax,cs:_pm_rmstacklen
                add     ds:rmstacktop[edi],ax

                jmp     int31ok                                 ; return ok
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310303:                                                      ; allocate real mode callback address
                mov     bl,cs:_pm_callbacks                     ; CL = total number of callbacks
                or      bl,bl                                   ; are there any?
                jz      int31fail8015                           ; if no, error 8015h

                mov     edx,cs:callbackbase                     ; EDX -> base of callbacks
                mov     ecx,edx                                 ; for later use

@@int310303l0:
                cmp     word ptr [edx+3],0                      ; is this callback free?
                jz      short @@int310303f0                     ; if yes, allocate

                add     edx,25                                  ; increment ptr to callback
                dec     bl                                      ; decrement loop counter
                jnz     @@int310303l0                           ; if more callbacks to check, loop

                jmp     int31fail8015                           ; no free callback, error 8015h

@@int310303f0:
                mov     bx,[esp+38]                             ; BX = caller DS from stack
                mov     [edx+3],bx                              ; store callback parms in callback
                mov     [edx+7],esi
                mov     [edx+12],es
                mov     [edx+16],edi

                sub     edx,ecx                                 ; DX = offset of callback
                shr     ecx,4                                   ; CX = segment of callback

                jmp     int31okdx                               ; return ok, with DX, CX, AX
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310304:                                                      ; free real mode callback address
                cmp     cx,cs:callbackseg                       ; valid callback segment?
                jne     int31fail8024                           ; if no, error 8024h

                movzx   ebx,dx                                  ; EBX = offset of callback

                xor     ax,ax                                   ; check if valid offset
                xchg    dx,ax
                mov     cx,25
                div     cx

                or      dx,dx                                   ; is there a remainder
                jnz     int31fail8024                           ; if yes, not valid, error 8024h

                or      ah,ah                                   ; callback index too big?
                jnz     int31fail8024                           ; if yes, not valid, error 8024h

                cmp     al,cs:_pm_callbacks                     ; callback index out of range?
                jae     int31fail8024                           ; if yes, not valid, error 8024h

                add     ebx,cs:callbackbase                     ; EBX -> callback
                mov     word ptr [ebx+3],0                      ; set callback as free

                jmp     int31ok                                 ; return ok

;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
; MISC FUNCTIONS
;±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±±
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310305:                                                      ; get state save/restore addresses
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                xor     ax,ax                                   ; size needed is none
                mov     bx,_rm16code                            ; real mode seg of same RETF
                mov     cx,off vxr_saverestorerm                ; same offset of 16bit RETF
                mov     si,cs                                   ; selector of routine is this one
                mov     edi,off vxr_saverestorepm               ; offset of simple 32bit RETF

                jmp     int31oknopop                            ; return ok, dont pop registers
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310306:                                                      ; get raw mode switch addresses
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                mov     si,cs                                   ; selector of pmtorm rout is this one
                mov     edi,cs:pmtormswrout                     ; offset in this seg of rout
                mov     bx,_rm16code                            ; real mode seg of rmtopm rout
                mov     cx,offset xr_rmtopmsw                   ; offset of rout in real mode

                jmp     int31oknopop                            ; return ok, dont pop registers
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
int310400:                                                      ; get version
                add     esp,26h                                 ; adjust stack
                pop     ds                                      ; restore DS

                mov     ax,100h                                 ; return version 1.0
                mov     bx,3                                    ; capabilities
                mov     cl,cs:processortype                     ; processor type
                mov     dx,word ptr cs:picslave                 ; master and slave PIC values

                jmp     int31oknopop                            ; return ok, dont pop registers
;ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
PMODE_TEXT      ends
                end
