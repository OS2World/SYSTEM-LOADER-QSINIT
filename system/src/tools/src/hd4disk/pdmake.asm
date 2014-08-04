;
; QSINIT OS/2 "ramdisk" basedev
; page tables and switch code prepation
;
ifdef DOSHLP_BUILD
                include segs.inc 
else
                include segdef.inc
endif
                include swpage.inc
                include inc/qstypes.inc
                include inc/seldesc.inc
                include inc/pagedesc.inc
                include inc/basemac.inc

PDSTEP          equ     1 shl PD2M_ADDRSHL

ifdef DOSHLP_BUILD
_DATA           segment dword public USE32 'DATA'
_FlatDS         dd      seg _DATA

make_esp        macro                                           ;
                endm
else
_DATA           segment
                extrn   _FlatDS:dword

make_esp        macro                                           ;
                movzx   esp,sp                                  ;
                endm
endif
                extrn   _paeppd:dword
                extrn   _swcode:dword
                extrn   _swcodelin:dword
gdt_limit       dw      0                                       ;
gdt_base        dd      0                                       ;
_DATA           ends

ifdef DOSHLP_BUILD
_TEXT16         segment para public USE16 'CODE'
                assume  cs:_TEXT16, ds:_DATA, es:nothing, ss:nothing
else
_TEXT           segment
                assume  cs:_TEXT, ds:DGROUP, es:nothing, ss:nothing
endif

; set selector fields
; es:[edi] - selector, bl - access, bh - attrs, eax - base, ecx - limit
setup_selector  proc    near
                push    eax                                     ;
                push    ecx                                     ;
                mov     es:[edi].d_access,bl                    ;
                mov     es:[edi].d_loaddr,ax                    ;
                bswap   eax                                     ;
                mov     es:[edi].d_extaddr,al                   ;
                mov     es:[edi].d_hiaddr,ah                    ;
                mov     es:[edi].d_limit,cx                     ;
                shr     ecx,16                                  ; setup limit
                and     cl,0Fh                                  ;
                or      cl,bh                                   ;
                mov     es:[edi].d_attr,cl                      ;
                pop     ecx                                     ;
                pop     eax                                     ;
                ret
setup_selector  endp

; void paemake(u16t pdsel);
                public  _paemake
_paemake        proc    near
@@pdm_pdsel     = dword ptr [esp+20]                            ;
                pusha                                           ; 16
                push    es                                      ;
                make_esp                                        ;
                mov     es,@@pdm_pdsel                          ;
                mov     edi,PAETAB_OFS                          ; skip 32bit tables
                mov     eax,_paeppd                             ; 
                lea     eax,[eax+edi+4000h]                     ; PAE PT located after 4 PDs
                cld                                             ;
                or      eax,PT_PRESENT or PT_WRITE              ; first 2Mbs mapped
                stosd                                           ; by page table
                xor     eax,eax                                 ;
                stosd                                           ;
                mov     eax,PD_BIGPAGE or PT_PRESENT or PT_WRITE or PDSTEP ; start from 2Mb
                xor     edx,edx                                 ;
                mov     cx,512*4 - 1                            ; number of 2Mb pages
@@paemk_pdloop:
                stosd                                           ; make plain phys==lin
                xchg    edx,eax                                 ; mapping for entire
                stosd                                           ; 4Gb space.
                xchg    edx,eax                                 ;
                add     eax,PDSTEP                              ;
                loop    @@paemk_pdloop                          ;

                mov     edi,PAETAB_OFS+4000h                    ; fill one PT
                mov     eax,PT_PRESENT or PT_WRITE              ;
                mov     cx,512                                  ;
@@paemk_ptloop:
                stosd                                           ;
                xchg    edx,eax                                 ;
                stosd                                           ;
                xchg    edx,eax                                 ;
                add     eax,PAGESIZE                            ;
                loop    @@paemk_ptloop                          ;

                mov     es,_FlatDS                              ; fill PDPTE table
                mov     edi,_swcodelin                          ; at the end of
                add     edi,offset sw_pdpte                     ; switch code page
                mov     eax,_paeppd                             ;
                add     eax,PAETAB_OFS or PT_PRESENT            ;
                xor     edx,edx                                 ;
                mov     cx,4                                    ;
@@paemk_pdpteloop:
                stos    dword ptr [edi]                         ;
                xchg    edx,eax                                 ;
                stos    dword ptr [edi]                         ;
                xchg    edx,eax                                 ;
                add     eax,PAGESIZE                            ;
                loop    @@paemk_pdpteloop                       ;

                pop     es                                      ;
                popa                                            ;
                ret     2                                       ;
_paemake        endp

ifndef DOSHLP_BUILD
; void selsetup(u16 sel, u32t base, u16t size, u8t access, u8t attrs);
                public  _selsetup
_selsetup       proc    near
@@s_sel         = word  ptr [bp+4]                              ;
@@s_base        = dword ptr [bp+6]                              ;
@@s_size        = word  ptr [bp+10]                             ;
@@s_access      = byte  ptr [bp+12]                             ;
@@s_attrs       = byte  ptr [bp+14]                             ;
                push    bp                                      ;
                mov     bp,sp                                   ;
                SaveReg <es,edi,ebx,ecx>                        ;
                mov     es,_FlatDS                              ;
                sgdt    fword ptr gdt_limit                     ;
                movzx   edi,@@s_sel                             ;
                add     edi,gdt_base                            ;
                mov     bl,@@s_access                           ;
                mov     bh,@@s_attrs                            ;
                movzx   ecx,@@s_size                            ;
                dec     ecx                                     ;
                mov     eax,@@s_base                            ;
                call    setup_selector                          ;
                RestReg <ecx,ebx,edi,es>                        ;
                mov     sp,bp                                   ;
                pop     bp                                      ;
                ret     12                                      ;
_selsetup       endp

;void linmake(u16t pdsel, u32t ppd, u32t swlin, u32t swphys);
;
; make page tables for switching out of paging mode:
;  * one page table for current linear location of switching code
;  * one page table for linear==physical map of switching code
;  * one page dir ;)
;
                public  _linmake
_linmake        proc    near
@@ldm_pdsel     = word  ptr [esp+20]                            ;
                pusha                                           ; 16
                push    es                                      ;
                make_esp                                        ;
                mov     es,@@ldm_pdsel                          ;
                xor     di,di                                   ;
                xor     cx,cx                                   ;
                cld                                             ;
                mov     ebx,_swcodelin                          ; fill page dir
                mov     edx,_swcode                             ; with 2 entries
                shr     ebx,PD4M_ADDRSHL                        ;
                shr     edx,PD4M_ADDRSHL                        ;
@@linmk_pdloop: ;------------------------------------------------
                xor     eax,eax                                 ;
                cmp     cx,bx                                   ;
                jnz     @@linmk_pd1                             ;
                mov     ax,1000h                                ; PT for lin adr
                jmp     @@linmk_pd2                             ;
@@linmk_pd1:
                cmp     cx,dx                                   ;
                jnz     @@linmk_pd3                             ;
                mov     ax,2000h                                ; PT for phys addr
@@linmk_pd2:
                add     eax,_paeppd                             ;
                or      ax,PT_PRESENT or PT_WRITE               ;
@@linmk_pd3:
                stosd                                           ;
                inc     cx                                      ;
                cmp     cx,1024                                 ;
                jnz     @@linmk_pdloop                          ;

                mov     ebx,_swcodelin                          ; PD ready, fill
                mov     si,2                                    ; 2 PTs
@@linmk_ptloopset:
                and     ebx,not PD4M_ADDRMASK                   ;
                shr     ebx,PAGESHIFT                           ;
                xor     cx,cx                                   ;
@@linmk_ptloop: ;------------------------------------------------
                xor     eax,eax                                 ;
                cmp     cx,bx                                   ;
                jnz     @@linmk_ptloop1                         ;
                mov     eax,_swcode                             ;
                or      ax,PT_PRESENT or PT_WRITE               ;
@@linmk_ptloop1:
                stosd                                           ;
                inc     cx                                      ;
                cmp     cx,1024                                 ;
                jnz     @@linmk_ptloop                          ;
                mov     ebx,_swcode                             ;
                dec     si                                      ;
                jnz     @@linmk_ptloopset                       ;
                pop     es                                      ;
                popa                                            ;
                ret     2
_linmake        endp
endif

;void gdtmake(u32t selbase, u16t sellim);
                public  _gdtmake
_gdtmake        proc    near
@@gdm_selbase   = dword ptr [esp+16]                            ;
@@gdm_sellim    = word  ptr [esp+20]                            ;
                SaveReg <es,edi,ebx,ecx>                        ;
                mov     es,_FlatDS                              ;
                mov     edi,_swcodelin                          ;
                mov     ecx,offset sw_gdt                       ;
                mov     es:[edi].sw_gdtlimit,SWSEL_LIMIT        ;
                mov     eax,_swcode                             ;
                add     eax,ecx                                 ;
                mov     es:[edi].sw_gdtbase,eax                 ;
                add     edi,ecx                                 ;

                xor     eax,eax                                 ; ZERO SEL
                stos    dword ptr [edi]                         ;
                stos    dword ptr [edi]                         ;
                mov     bl,D_CODE0                              ; SWSEL_CODELIN
ifndef DOSHLP_BUILD
                mov     bh,D_DBIG                               ;
else
                xor     bh,bh                                   ;
endif
                movzx   ecx,@@gdm_sellim                        ;
                mov     eax,@@gdm_selbase                       ;
                call    setup_selector                          ;
                add     edi,SEL_INCR                            ; SWSEL_CODEPHYS
                mov     bh,D_DBIG                               ;
                mov     eax,_swcode                             ;
                call    setup_selector                          ;
                add     edi,SEL_INCR                            ;
                mov     bl,D_DATA0                              ; SWSEL_FLAT
                mov     bh,D_GRAN4K or D_DBIG                   ;
                mov     ecx,0FFFFFh                             ;
                xor     eax,eax                                 ;
                call    setup_selector                          ;

                RestReg <ecx,ebx,edi,es>                        ;
                ret     6                                       ;
_gdtmake        endp

;void setdisk(u32t hdrpage, u32t hdrcopy, u32t dwbuf);
                public  _setdisk
_setdisk        proc    near
@@sd_hpage      = dword ptr [esp+4]                             ;
@@sd_hpcopy     = dword ptr [esp+8]                             ;
@@sd_dwbuf      = dword ptr [esp+12]                            ;
                push    es                                      ;
                make_esp                                        ;
                mov     es,_FlatDS                              ;
                mov     eax,_swcodelin                          ;
                push    @@sd_hpage                              ;
                pop     dword ptr es:[eax].sw_headpage          ;
                push    @@sd_hpcopy                             ;
                pop     dword ptr es:[eax].sw_headcopy          ;
                push    @@sd_dwbuf                              ;
                pop     dword ptr es:[eax].sw_buf64k            ;
                mov     es:[eax].sw_buf64k_len, _64KB           ;
                pop     es                                      ;
                ret     12                                      ;
_setdisk        endp

ifdef DOSHLP_BUILD
;void pae_setup(u32t hdrpage, u32t hdrcopy, u32t dwbuf, u16t pdsel, u16t bhseg);
                public  _pae_setup
_pae_setup      proc    far
@@pbs_hdrpage   = dword ptr [ebp+12]                            ;
@@pbs_hdrcopy   = dword ptr [ebp+16]                            ;
@@pbs_dwbuf     = dword ptr [ebp+20]                            ;
@@pbs_pdsel     = word ptr  [ebp+24]                            ;
@@pbs_bhseg     = word ptr  [ebp+28]                            ;
                push    ebp                                     ;
                mov     ebp,esp                                 ;
                SaveReg <esi,edi,ebx,ecx,edx,es>                ;
                movzx   eax,@@pbs_bhseg                         ;
                shl     eax,PARASHIFT                           ;
                push    0FFFFh                                  ;
                push    eax                                     ;
                call    _gdtmake                                ;

                push    @@pbs_dwbuf                             ;
                push    @@pbs_hdrcopy                           ;
                push    @@pbs_hdrpage                           ;
                call    _setdisk                                ;

                push    @@pbs_pdsel                             ;
                call    _paemake                                ;

                RestReg <es,edx,ecx,ebx,edi,esi>                ;
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                db      66h                                     ;
                retf                                            ;
_pae_setup      endp

_TEXT16         ends
else
_TEXT           ends
endif
                end
