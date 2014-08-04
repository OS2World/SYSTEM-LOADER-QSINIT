;
; QSINIT OS/2 "ramdisk" basedev
; ram disk i/o in PAE mode
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
                include inc/cpudef.inc
                include inc/doshlp.inc

if size swpage_s NE 4096                                        ; check size
                .err                                            ;
endif                                                           ;

_DATA           segment
                public  _switch_codelen
_switch_codelen dw      switch_codeend - _switch_code           ;
_DATA           ends

ifndef DOSHLP_BUILD
; this empty object exist to get 16:16 pointer to 32bit code for strat.c
_REL16          segment
                public  _switch_handler
_switch_handler label   near
_REL16          ends
endif

; this code will be copied to single page in 1Mb. So it shouldn`t use
; any absolute relocations.
;
; ds,es - FLAT
; esi  physaddr/SGList
; edi  hi_page/sector
; ecx  sectors to copy | cSGList<<16
; edx  _swcodelin  - this page linear addr
; ebx  _swcode     - this page phys addr
; eax  _paeppd     - page tables in order: 32: PD+2PT, PAE: 4PD+PT
; al - bit 0:    0 = read, 1 = write
;      bit 1,2:  0 = addr, 1 = SGList, 2 = clear header
;
_REL32          segment
                public  _switch_code
                assume  cs:nothing, ds:nothing, es:nothing, ss:nothing
_switch_code    label   near
ifndef DOSHLP_BUILD
                sgdt    fword ptr [edx].sw_savegdt              ;
                mov     ebp,cr3                                 ; low bits in al
                mov     cr3,eax                                 ; must be ignored
                lgdt    fword ptr [edx].sw_gdtlimit             ;
                mov     word ptr [edx].sw_ss,ss                 ;
                mov     [edx].sw_esp,esp                        ;
                mov     [edx-4].sw_esp,ebp                      ; save old cr3
                lea     esp,[edx-4].sw_esp                      ;
                mov     bp,SWSEL_FLAT                           ;
                mov     ss,bp                                   ; FLAT selector,
                mov     ds,bp                                   ; we have stack! ;)
                mov     es,bp                                   ;
                push    SWSEL_CODEPHYS                          ; reload cs
                push    @@swc_farjmp1 - _switch_code            ;
                retf                                            ;
@@swc_farjmp1:
                mov     ebp,cr0                                 ; go out of 32bit
                push    ebp                                     ; paging
                and     ebp,not CR0_PG                          ;
                mov     cr0,ebp                                 ;
                and     esp,PAGEMASK                            ; fix stack ptr
                add     esp,ebx                                 ;
                push    edx                                     ; save base for out code
endif
                mov     [ebx].sw_paeppd,eax                     ; save paeppd
;
; here we are in plain non-paged mode with own GDT
; ss=ds=es - FLAT
; cs=non-zero based 32 bit segment (one page)
; GDT is valid
; IDT is INVALID
; Task Register is INVALID
; stack points to the end of this page
;
                cld                                             ;
                mov     dl,al                                   ;
                and     dl,6                                    ;
                mov     eax,[ebx].sw_headpage                   ;
                cmp     dl,4                                    ;
                jc      @@swc_notclean                          ; zero disk header?
                mov     edi,eax                                 ;
                mov     ecx,1                                   ; 512 bytes to zero
@@swc_notclean:
                or      dl,dl                                   ; addr?
                jnz     @@swc_notaddr                           ;
                mov     eax,edi                                 ; get it to check
@@swc_notaddr:
                cmp     eax,100000h                             ; header/page above 4Gb or not?
                setnc   [ebx].sw_paeon                          ;
                jc      switch_actflat                          ;
                jmp     switch_actPAE                           ; goto handler
switch_exit     label   near                                    ;
ifndef DOSHLP_BUILD
                mov     edx,[ebx].sw_paeppd                     ;
                mov     cr3,edx                                 ;
                pop     edx                                     ;
                pop     ebp                                     ; 32bit paging on
                mov     cr0,ebp                                 ;
                and     esp,PAGEMASK                            ; fix stack ptr
                add     esp,edx                                 ;
                push    SWSEL_CODELIN                           ; reload cs
                push    @@swc_farjmp2 - _switch_code            ;
                retf                                            ;
@@swc_farjmp2:
                pop     ebp                                     ; OS/2 page dir
                mov     cr3,ebp                                 ;
                lgdt    fword ptr [edx].sw_savegdt              ;
                pop     ebp                                     ; OS/2 stack
                pop     ss                                      ;
                mov     esp,ebp                                 ; return and set
endif
                retf                                            ; valid cs
;----------------------------------------------------------------
; disk use only memory below 4Gb - do not use PAE, operates in
; plain non-paged mode
;----------------------------------------------------------------
switch_actflat  label   near
                cmp     dl,2                                    ;
                push    switch_exit - _switch_code              ;
                jz      @@swc_SG_List                           ;
                shl     edi,PAGESHIFT                           ;
                jmp     @@swc_commoncopy                        ; call clear/copy code
@@swc_SG_List:
                push    esi                                     ;
                push    ecx                                     ; check SGList:
                shr     ecx,16                                  ; total size and
                xor     edx,edx                                 ; unaligned entries
                push    0                                       ;
@@swc_SG_CheckLoop:
                add     esi,4                                   ;
                lodsd                                           ; 
                add     edx,eax                                 ;
                and     eax,SECTMASK                            ;
                add     [esp],eax                               ;
                loop    @@swc_SG_CheckLoop                      ;
                pop     eax                                     ;
                pop     ecx                                     ;
                movzx   ecx,cx                                  ; do not need cSGList
                pop     esi                                     ;
                test    edx,SECTMASK                            ; SGList sum & 511 <> 0
                jnz     @@swc_SG_brokenList                     ;
                shr     edx,SECTSHIFT                           ; SGList sum <> # of
                cmp     ecx,edx                                 ; sectors
                jnz     @@swc_SG_brokenList                     ;
                or      eax,eax                                 ; unaligned sum
                jnz     @@swc_SG_BadBoys                        ;
;
; copy loop with sector-aligned SGList entries
;
@@swc_SG_GoodBoys:
                mov     [ebx].sw_sectors,ecx                    ; # of sectors to copy
                xor     eax,eax                                 ;
                mov     [ebx].sw_sgbcnt,eax                     ;

                mov     ebp,[ebx].sw_headcopy                   ;
                mov     dx,[ebp].h4_tabsize                     ; # of HD4Entry
                add     bp,[ebp].h4_tabofs                      ;
@@swc_SG_diskLoop:
                cmp     edi,[ebp].hde_sectors                   ; loop over list of
                jc      @@swc_flatSGRead                        ; HD4Entry to reach
                sub     edi,[ebp].hde_sectors                   ; our`s 1st sector pos
                add     ebp,size HD4Entry                       ;
                dec     dx                                      ; end of list???
                jnz     @@swc_SG_diskLoop                       ;
                mov     ax,0102h                                ; IOERR_CMD_SYNTAX
                retn                                            ;
@@swc_flatSGRead:
                mov     ecx,[ebp].hde_1stpage                   ; disk memory pos
                shl     ecx,PAGESHIFT - SECTSHIFT               ; in 512 byte-blocks
                add     ecx,edi                                 ; (vdisk will never send
                mov     [ebx].sw_ephys,ecx                      ;  addr above 2Tb to us)
; u32t toread = cde[ii].hde_sectors - sector, cdeofs = sector;
                mov     ecx,[ebp].hde_sectors                   ; sectors to read in
                sub     ecx,edi                                 ; this HD4Entry
                cmp     [ebx].sw_sectors,ecx                    ;
; if (toread>count) toread = count;
                jnc     @@swc_SG_sliceLoop                      ;
                mov     ecx,[ebx].sw_sectors                    ;
                align   4
@@swc_SG_sliceLoop:
                or      ecx,ecx                                 ;
                jz      @@swc_SG_fin                            ;
                cmp     [ebx].sw_sgbcnt,eax                     ; 0?
                jnz     @@swc_SG_ok                             ;
                lodsd                                           ;
                mov     [ebx].sw_sgphys,eax                     ;
                test    dword ptr [esi],SECTMASK                ;
                jnz     @@swc_SG_brokenList                     ; 
                lodsd                                           ; update
                shr     eax,SECTSHIFT                           ; SGList pos/cnt
                mov     [ebx].sw_sgbcnt,eax                     ;
                xor     eax,eax                                 ;
@@swc_SG_ok:
                push    ecx                                     ;
                cmp     [ebx].sw_sgbcnt,ecx                     ; SGList < toread?
                jnc     @@swc_SG_2                              ;
                mov     ecx,[ebx].sw_sgbcnt                     ;
@@swc_SG_2:
                test    [ebx].sw_paeon,1                        ; limit PAE transfers
                jz      @@swc_SGR_no2MbLimit                    ; to 2Mb
                cmp     ecx,_2MB shr SECTSHIFT                  ; (this code is void,
                jc      @@swc_SGR_no2MbLimit                    ; but required formally)
                mov     ecx,_2MB shr SECTSHIFT                  ;
@@swc_SGR_no2MbLimit:
                sub     [esp],ecx                               ; toread -= cnt;
                sub     [ebx].sw_sectors,ecx                    ; count  -= cnt;
                add     edi,ecx                                 ; cdeofs += cnt;

                sub     [ebx].sw_sgbcnt,ecx                     ;
                push    edi                                     ;
                mov     edi,ecx                                 ;
                shl     ecx,SECTSHIFT                           ;
                push    esi                                     ;
                mov     esi,[ebx].sw_sgphys                     ; SG ptr
                add     [ebx].sw_sgphys,ecx                     ;

                xadd    [ebx].sw_ephys,edi                      ; disk ptr
                test    [ebx].sw_paeon,1                        ;
                jz      @@swc_SG_plain                          ;
                mov     eax,edi                                 ;
                and     eax,(1 shl (PAGESHIFT - SECTSHIFT)) - 1 ; just 7
                shl     eax,SECTSHIFT                           ; offset in page
                shr     edi,PAGESHIFT - SECTSHIFT               ;
                call    @@swc_paemap                            ;
                add     edi,eax                                 ;
                xor     eax,eax                                 ; restore 0
                jmp     @@swc_SG_copy                           ;
@@swc_SG_plain:
                shl     edi,SECTSHIFT                           ;
@@swc_SG_copy:
                shr     ecx,2                                   ;
                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jnz     @@swc_SG_3                              ;
                xchg    esi,edi                                 ;
@@swc_SG_3:
;                div     al
            rep movsd                                           ;
                pop     esi                                     ;
                pop     edi                                     ;
                pop     ecx                                     ;
                jmp     @@swc_SG_sliceLoop                      ;
@@swc_SG_fin:
                cmp     [ebx].sw_sectors,eax                    ; count==0?
                jz      @@swc_flatdone                          ;
                xor     edi,edi                                 ;
                add     ebp,size HD4Entry                       ;
                jmp     @@swc_SG_diskLoop                       ;
@@swc_SG_brokenList:
                mov     ax,0103h                                ; shot SGList - IOERR_CMD_SGLIST_BAD
@@swc_flatdone:
                retn                                            ;

;
;----------------------------------------------------------------
; at least disk header - located above 4Gb - turn on PAE and
; continue
;----------------------------------------------------------------
switch_actPAE   label   near                                    ;
                lea     eax,[ebx].sw_pdpte                      ; turn on
                mov     cr3,eax                                 ; PAE paging
                mov     eax,cr4                                 ;
                or      eax,CPU_CR4_PAE                         ;
                mov     cr4,eax                                 ;
ifdef DOSHLP_BUILD
                mov     ebp,cr0                                 ;
endif
                push    ebp                                     ;
                or      ebp,CPU_CR0_PG                          ;
                mov     cr0,ebp                                 ;

                cmp     dl,2                                    ;
                push    @@swc_paedone - _switch_code            ; for retn
                jz      @@swc_SG_List                           ;

                cmp     ecx, _2MB shr SECTSHIFT                 ; copy <= 2Mb
                jc      @@swc_fixcopylen                        ;
                mov     ecx, _2MB shr SECTSHIFT                 ;
@@swc_fixcopylen:
                call    @@swc_paemap                            ;
                jmp     @@swc_commoncopy                        ;
@@swc_paedone:
                pop     ebp                                     ; paging off
                mov     cr0,ebp                                 ;
                mov     ebp,cr4                                 ;
                and     ebp,not CPU_CR4_PAE                     ; drop PAE flag
                mov     cr4,ebp                                 ;
                jmp     switch_exit
;
; map page to address space:
; in : edi - page
; out: edi - address, up to 2Mb for safe copying
@@swc_paemap:
                push    edx                                     ;
                push    eax                                     ;
                mov     edx,edi                                 ;
                mov     eax,not ((1 shl (PD2M_ADDRSHL - PAGESHIFT)) - 1) ;
                and     eax,edx                                 ;

                sub     edx,eax                                 ; offset in big page
                shl     edx,PAGESHIFT                           ;
                lea     edi,[COPYPAGE_ADDR+edx]                 ;

                xor     edx,edx                                 ; update page dir
                shld    edx,eax,PAGESHIFT                       ;
                shl     eax,PAGESHIFT                           ;
                or      eax,PD_BIGPAGE or PT_PRESENT or PT_WRITE ;

                push    ebp                                     ;
                mov     ebp,[ebx].sw_paeppd                     ; point to the end
                and     ebp,not 7                               ;
                add     ebp,PAETAB_OFS+4000h-16                 ; of 4th page dir
                cmp     [ebp],eax                               ;
                jnz     @@swc_paemapNotEQ                       ; the same addr?
                cmp     [ebp+4],edx                             ; skip invlpg
                jz      @@swc_paemapSkipInv                     ;
@@swc_paemapNotEQ:
                mov     [ebp],eax                               ; update page dir
                mov     [ebp+4],edx                             ;
                add     eax,_2MB                                ;
                adc     edx,0                                   ;
                mov     [ebp+8],eax                             ;
                mov     [ebp+12],edx                            ;

                invlpg  [edi]                                   ; invalidate last
                invlpg  [edi+_2MB]                              ; two big pages
@@swc_paemapSkipInv:
                pop     ebp                                     ;
                pop     eax                                     ;
                pop     edx                                     ;
                retn                                            ;
;
; common pae/flat page copy/zero code
;
@@swc_commoncopy:
                shl     ecx,SECTSHIFT-2                         ; size in dwords

                xor     eax,eax                                 ;
; vbox break (no IDT and VM stops ;)
;                div     al
                test    byte ptr [ebx].sw_paeppd,4              ; clear?
                jz      @@swc_paecopy                           ;
            rep stosd                                           ;
                retn                                            ;
@@swc_paecopy:
                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jnz     @@swc_paenoswap                         ;
                xchg    esi,edi                                 ;
@@swc_paenoswap:
            rep movsd                                           ;
                retn                                            ;
;
; call with not aligned SGList entries.... I'm not so smart to
; resolve this case in crazy loop above, so just get plain 64k
; buffer and use it as temporary SGList for this op
;
@@swc_SG_BadBoys:
                mov     [ebx].sw_sgn_size,0                     ;
@@swc_SG_BB_Loop:
                mov     eax,128                                 ; 64k
                cmp     ecx,eax                                 ;
                jnc     @@swc_SG_BB_GE64k                       ;
                mov     eax,ecx                                 ;
@@swc_SG_BB_GE64k:
                sub     ecx,eax                                 ;
                push    ecx                                     ;
                mov     ecx,eax                                 ;

                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jz      @@swc_SG_BB_nWrite                      ;
                call    @@swc_SG_CopyBuffer                     ;
@@swc_SG_BB_nWrite:
                push    ecx                                     ;
                push    esi                                     ;
                push    edi                                     ;
                lea     esi,[ebx].sw_buf64k                     ;
                call    @@swc_SG_GoodBoys                       ;
                pop     edi                                     ;
                pop     esi                                     ;
                pop     ecx                                     ;
                add     edi,ecx                                 ; update sector
                or      eax,eax                                 ;
                jnz     @@swc_SG_BB_err                         ;

                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jnz     @@swc_SG_BB_nRead                       ;
                call    @@swc_SG_CopyBuffer                     ;
@@swc_SG_BB_nRead:
                pop     ecx                                     ;
                or      ecx,ecx                                 ;
                jnz     @@swc_SG_BB_Loop                        ;
                retn                                            ;
@@swc_SG_BB_err:
                pop     ecx                                     ;
                retn                                            ;
;
; in  - ecx = sectors
;       esi = sglist
; out - esi = updated sglist
;       other preserved
;            
@@swc_SG_CopyBuffer:
                push    eax                                     ;
                push    ecx                                     ;
                push    edi                                     ;
                mov     edi,[ebx].sw_buf64k                     ;
                shl     ecx,SECTSHIFT                           ;
@@swc_SG_CB_Loop:
                mov     eax,[ebx].sw_sgn_size                   ;
                or      eax,eax                                 ;
                jnz     @@swc_SG_CB_SameEntry                   ;
                lodsd                                           ;
                mov     [ebx].sw_sgn_phys,eax                   ;
                lodsd                                           ;
                mov     [ebx].sw_sgn_size,eax                   ;
@@swc_SG_CB_SameEntry:
                push    esi                                     ;
                mov     esi,[ebx].sw_sgn_phys                   ;
                cmp     eax,ecx                                 ; bytes to copy
                jc      @@swc_SG_CB_NF                          ; in this iter
                mov     eax,ecx                                 ;
@@swc_SG_CB_NF:
                sub     [ebx].sw_sgn_size,eax                   ;
                sub     ecx,eax                                 ;
                xchg    ecx,eax                                 ;

                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jnz     @@swc_SG_CB_nSwap1                      ;
                xchg    esi,edi                                 ;
@@swc_SG_CB_nSwap1:
                push    ecx                                     ;
                shr     ecx,2                                   ;
            rep movsd                                           ;
                pop     ecx                                     ;
                and     ecx,3                                   ;
            rep movsb                                           ;
                test    byte ptr [ebx].sw_paeppd,1              ; write?
                jnz     @@swc_SG_CB_nSwap2                      ;
                xchg    esi,edi                                 ;
@@swc_SG_CB_nSwap2:
                mov     [ebx].sw_sgn_phys,esi                   ;
                pop     esi                                     ;
                or      ecx,eax                                 ;
                jnz     @@swc_SG_CB_Loop                        ;
                pop     edi                                     ;
                pop     ecx                                     ;
                pop     eax                                     ;
                retn                                            ;

switch_codeend  label   near

_REL32          ends
                end
