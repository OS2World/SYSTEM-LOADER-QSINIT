;
; QSINIT OS/2 "ramdisk" basedev
; calling PAE i/o code
;
                include segdef.inc
                include swpage.inc
                include inc/qstypes.inc
                include inc/seldesc.inc
                include inc/basemac.inc
                include inc/lowports.inc 
                include inc/cpudef.inc
                include inc/doshlp.inc

if size swpage_s NE 4096                                        ; check size
                .err                                            ;
endif                                                           ;

_DATA           segment
                extrn   _swcode:dword                           ;
                extrn   _paeppd:dword                           ;
ifndef DOSHLP_BUILD
                extrn   _FlatDS:dword                           ;
                extrn   _swcodelin:dword                        ;
                public  _cp_sel, _dis_nmi                       ;
_cp_sel         dw      0                                       ; paged mode selector
spinlock        db      0                                       ; primitive spinlock
_dis_nmi        db      0                                       ;
else
saved_gdt       dw      3 dup (0)                               ;
saved_ss        dw      0                                       ;
saved_esp       dd      0                                       ;
saved_cr3       dd      0                                       ;
endif
_DATA           ends

_TEXT           segment
                assume  cs:_TEXT, ds:DGROUP, es:nothing, ss:nothing

ifdef DOSHLP_BUILD
; common i/o call for DOSHLP binary.
; we called here in real mode - so ignore 32bit paging parameters
; ---------------------------------------------------------------
; IN:    al - action: 0=read, 1=write (not used)
;       edx - start sector
;       edi - bpb`s hidden sectors
;        cx - sector count
;     es:bx - buffer address
; OUT:  C=1 - failed
;     dx:ax - start sector + count (without hidden sectors)
;     all registers destroyd
;
                public  pae_diskio
pae_diskio      proc    near                                    ;
                add     edi,edx                                 ;
                movzx   ecx,cx                                  ;
                add     edx,ecx                                 ;
                push    edx                                     ; out dx:ax
                pushfd                                          ;
                cli                                             ;
                or      al,2                                    ; SGList i/o
                movzx   eax,al                                  ;
                or      eax,_paeppd                             ;

                sgdt    fword ptr saved_gdt                     ; save (not required,
                mov     edx,cr3                                 ; but who knows)
                mov     saved_cr3,edx                           ;

                xor     esi,esi                                 ; phys addr of
                mov     si,es                                   ; buffer for i/o
                shl     esi,PARASHIFT                           ;
                movzx   ebx,bx                                  ;
                add     esi,ebx                                 ;

                mov     ebx,_swcode                             ;
                mov     edx,ebx                                 ;
                shr     edx,PARASHIFT                           ;
                mov     es,dx                                   ;
                lgdt    fword ptr es:[0].sw_gdtlimit            ;

                mov     saved_ss,ss                             ;
                mov     saved_esp,esp                           ;
                xor     ebp,ebp                                 ;
                mov     bp,ds                                   ; rm cs/ds

                mov     edx,cr0                                 ; PM
                or      edx,CPU_CR0_PE                          ;
                and     edx,not CPU_CR0_PG                      ;
                mov     cr0,edx                                 ;
                mov     dx,SWSEL_FLAT                           ;
                mov     es,dx                                   ;
                mov     ds,dx                                   ;
                mov     ss,dx                                   ;
                lea     esp,[ebx].sw_esp                        ;
                push    ebp                                     ; save rm CS/DS

                push    SWSEL_CODELIN                           ; validate CS
                push    offset @@pdhi_pm                        ;
                retf                                            ;
@@pdhi_pm:
                mov     edx,ecx                                 ;
                shl     edx,SECTSHIFT                           ;
                push    edx                                     ; XferBufLen
                push    esi                                     ; ppXferBuf
                mov     esi,esp                                 ;
                or      ecx,10000h                              ; cSGList = 1

                mov     edx,SWSEL_CODEPHYS                      ;
                push    edx                                     ;
                xor     dx,dx                                   ;
                push    edx                                     ;
                call    fword ptr [esp]                         ;

                mov     cx,[esp+16]                             ; real mode cs/ds
                mov     edx,cr0                                 ;
                and     edx,not CPU_CR0_PE                      ; RM
                mov     cr0,edx                                 ;
                mov     ds,cx                                   ;
                mov     es,cx                                   ;
                mov     ss,saved_ss                             ;
                mov     esp,saved_esp                           ; restore stack
                push    cx                                      ;
                push    offset @@pdhi_rm                        ;
                retf                                            ; reset CS
@@pdhi_rm:
                mov     edx,saved_cr3                           ; restore GDTR
                mov     cr3,edx                                 ; and cr3
                lgdt    fword ptr saved_gdt                     ;
                popfd                                           ;
                cmp     ax,1                                    ; error code?
                cmc                                             ;
                pop     ax                                      ;
                pop     dx                                      ;
                ret                                             ;
pae_diskio      endp                                            ;

else   ; !DOSHLP_BUILD

pae_common      proc    near                                    ;
                LockLogBuffer <spinlock>                        ;
                pushfd                                          ;
                cli                                             ;
                test    _dis_nmi,0FFh                           ;
                jz      @@pcom_nodis1                           ;
                push    eax                                     ;
                mov     al,DISABLE_NMI                          ;
                out     NMI_PORT,al                             ;
                nop                                             ;
                in      al,NMI_PORT+1                           ;
                pop     eax                                     ;
@@pcom_nodis1:
                SaveReg <ebx,ebp,ds>                            ;
                mov     es,_FlatDS                              ;
                mov     ebp,_swcodelin                          ;
                mov     ebx,_swcode                             ;
                or      eax,_paeppd                             ;
                movzx   ebp,_cp_sel                             ;
                mov     edx,_swcodelin                          ;
                push    ebp                                     ;
                xor     bp,bp                                   ;
                push    ebp                                     ;
                mov     ds,_FlatDS                              ;
                call    fword ptr [esp]                         ;
                add     esp,8                                   ;
                RestReg <ds,ebp,ebx>                            ;
                test    _dis_nmi,0FFh                           ;
                jz      @@pcom_nodis2                           ;
                push    eax                                     ;
                mov     al,ENABLE_NMI                           ;
                out     NMI_PORT,al                             ;
                nop                                             ;
                in      al,NMI_PORT+1                           ;
                pop     eax                                     ;
@@pcom_nodis2:
                popfd                                           ;
                UnlockLogBuffer <spinlock>                      ;
                ret                                             ;
pae_common      endp                                            ;

; int paecopy(u32t lo_physaddr, u32t hi_page, u16t sectors, u8t writehi);
                public  _paecopy
_paecopy        proc    near
@@paec_phys     = dword ptr [esp+10]                            ;
@@paec_hpage    = dword ptr [esp+14]                            ;
@@paec_scnt     = word  ptr [esp+18]                            ;
@@paec_write    = byte  ptr [esp+20]                            ;

                mov     eax,1                                   ; eax!!
                push    esi                                     ;
                push    edi                                     ;
                movzx   esp,sp                                  ;
                cmp     @@paec_write,0                          ;
                setnz   al                                      ;
                mov     esi,@@paec_phys                         ;
                mov     edi,@@paec_hpage                        ;
                movzx   ecx,@@paec_scnt                         ;
                call    pae_common                              ;
                pop     edi                                     ;
                pop     esi                                     ;
                ret     12                                      ;
_paecopy        endp

; void zero_header(void);
                public  _zero_header
_zero_header    proc    near                                    ;
                push    esi                                     ;
                push    edi                                     ;
                movzx   esp,sp                                  ;
                mov     eax,6                                   ;
                call    pae_common                              ;
                pop     edi                                     ;
                pop     esi                                     ;
                ret
_zero_header    endp

; u16t paeio(u32t SGList, u16t cSGList, u32t sector, u16t count, u8t write);
                public  _paeio
_paeio          proc    near
@@paei_sglist   = dword ptr [esp+10]                            ;
@@paei_csglist  = word  ptr [esp+14]                            ;
@@paei_sector   = dword ptr [esp+16]                            ;
@@paei_scnt     = word  ptr [esp+20]                            ;
@@paei_write    = byte  ptr [esp+22]                            ;
                xor     eax,eax                                 ;
                push    esi                                     ;
                push    edi                                     ;
                movzx   esp,sp                                  ;
                cmp     @@paei_write,0                          ;
                setnz   al                                      ;
                or      al,2                                    ;
                mov     esi,@@paei_sglist                       ;
                mov     edi,@@paei_sector                       ;
                mov     cx,@@paei_csglist                       ;
                shl     ecx,16                                  ;
                mov     cx,@@paei_scnt                          ;
                call    pae_common                              ;
                pop     edi                                     ;
                pop     esi                                     ;
                ret     14                                      ;
_paeio          endp                                            ;
endif  ; !DOSHLP_BUILD

_TEXT           ends
                end
