;
; QSINIT "start" module
; system exception handler
;
                .486p

                include inc/dpmi.inc
                include inc/qsinit.inc
                include inc/cpudef.inc
                include inc/qspdata.inc
                include inc/basemac.inc

                extrn   _mt_swlock:near                         ;
                extrn   _mt_swunlock:near                       ;
                extrn   _fpu_updatets:near                      ;
                extrn   _mt_exechooks:mt_proc_cb_s              ;
                extrn   _log_dumpregs_int:near                  ;

xcpt_rec_sign   = 54504358h                                     ;
xcpt_cmask      = 0FFFFh                                        ;
xcpt_cinit      = 10000h                                        ; not catched

xcpt_signerr    = 0FFFEh                                        ; sign err xcpt

xcpt_rec        struc
xcpt_sign       dd      ?                                       ; signature
xcpt_catch      dd      ?                                       ; number & flags
xcpt_file       dd      ?                                       ; source name
xcpt_line       dd      ?                                       ; source line
xcpt_nextrec    dd      ?                                       ; next in chain
xcpt_jmp        rmcallregs_s  <?>                               ; setjmp regs
xcpt_rec        ends

if size xcpt_rec GT 80                           ; look at sys_xcpt in qsxcpt.h
.err
endif

_BSS            segment dword public USE32 'BSS'
trap_data       tss_s <?>
_BSS            ends

_DATA           segment dword public USE32 'DATA'
trap_table      dd      offset trap_0                           ; exception handler
                dd      offset trap_1                           ; table
                dd      0                                       ;
                dd      offset trap_3                           ;
                dd      offset trap_4                           ;
                dd      offset trap_5                           ;
                dd      offset trap_6                           ;
                dd      0                                       ;
                dd      offset trap_8                           ;
                dd      0                                       ;
                dd      offset trap_10                          ;
                dd      offset trap_11                          ;
                dd      offset trap_12                          ;
                dd      offset trap_13                          ;
                dd      offset trap_14                          ;
                dd      0                                       ;
                dd      offset trap_16                          ;
                dd      offset trap_17                          ;
                dd      offset trap_18                          ;
                dd      offset trap_19                          ;
                dd      0FFFFFFFFh                              ;

xcpt_top        dd      0                                       ;
exf4_file       dd      0                                       ;
exf4_line       dd      0                                       ;
                public  xcpt_broken                             ;
xcpt_broken     dd      0                                       ; one of handler broken
                extrn   main_tss:word                           ;
_DATA           ends

CODE32          segment dword public USE32 'CODE'
                assume cs:FLAT, ds:FLAT, es:FLAT, ss:FLAT

                extrn   _trap_screen:near
                extrn   _trap_screen_64:near
                extrn   _reipl:near
                extrn   _exit_pm32:near
                extrn   __longjmp:near
                extrn   _key_read:near
                extrn   _sys_idtdump:near

                public  _except_init
;----------------------------------------------------------------
; install PM exception handlers for entire code (BIOS host only)
_except_init    proc    near                                    ;
                push    ebx                                     ;
                push    edi                                     ; setup exception
                push    esi                                     ; handlers
                mov     eax,offset xcpt_top                     ;
                mov     _mt_exechooks.mtcb_pxcpttop,eax         ; save ptr into mtdata
                mov     esi,offset trap_table                   ;
                xor     edi,edi                                 ;
@@excpt_set_loop:
                lodsd                                           ; offset
                or      eax,eax                                 ; skip it?
                jz      @@excpt_skip                            ;
                mov     edx,eax                                 ;
                inc     eax                                     ; end of table?
                jz      @@excpt_end                             ;
                mov     ebx,edi                                 ; int number
                mov     cx,cs                                   ; selector or CS
                mov     ax,0205h                                ;
                int     31h                                     ; set interrupt
@@excpt_skip:
                inc     edi                                     ; loop until end
                jmp     @@excpt_set_loop                        ;
@@excpt_end:
                pop     edi                                     ;
                pop     edi                                     ;
                pop     ebx                                     ;
                ret                                             ;
_except_init    endp

IRP             x,<8,10,11,12,13,14,17>                         ; exceptions with
                ALIGN 4                                         ; error code
trap_&x         label   near                                    ;
                push    x                                       ;
                jmp     common_trap                             ;
ENDM                                                            ;

IRP             x,<0,1,3,4,5,6,16,18,19>                        ; exceptions without
                ALIGN 4                                         ; error code
trap_&x         label   near                                    ;
                push    0                                       ; simulate zero code
                push    x                                       ;
                jmp     common_trap                             ;
ENDM

IRP             x,<8,12,13>                                     ; exceptions with
                ALIGN 4                                         ; error code, called
                public  traptask_&x                             ; via task gate
                local   traptask_iret                           ;
traptask_iret   label   near                                    ;
                iretd                                           ; loop TSS execution
traptask_&x     label   near                                    ;
                push    offset traptask_iret                    ;
                push    x                                       ;
                jmp     task_trap                               ;
ENDM                                                            ;
;----------------------------------------------------------------
task_trap       label   near                                    ;
                mov     ax,cs                                   ; assume ds = cs+8
                add     ax,SEL_INCR                             ;
                mov     es,ax                                   ;
                clts                                            ;

                str     ax                                      ; we have a writable TSS
                add     ax,SEL_INCR                             ; in next selector
                mov     fs,ax                                   ;
                mov     dx,fs:[0].tss_backlink                  ;
                lea     eax,[edx+SEL_INCR]                      ;
                mov     ds,ax                                   ;
                xor     esi,esi                                 ; copy task segment
                mov     edi,offset trap_data                    ; to trap_data struct
                mov     ecx,size tss_s                          ;
                mov     eax,edi                                 ;
                cld                                             ;
            rep movsb                                           ;
                push    es                                      ;
                pop     ds                                      ;
                xor     ecx,ecx                                 ;
                mov     fs,cx                                   ;
                mov     gs,cx                                   ;

                call    _mt_swlock                              ; we have ds, call lock

                pop     ecx                                     ; exception number
                mov     [eax+tss_s.tss_reservdbl],cx            ;
                pop     ecx                                     ;
                xchg    [esp],ecx                               ; error code
                mov     [eax+tss_s.tss_cr3],ecx                 ;
                mov     [eax+tss_s.tss_backlink],dx             ; interrupted TSS
                mov     [eax+tss_s.tss_esp0],esp                ; stack pointer for ret to iretd
                cmp     dx,main_tss                             ;
                jnz     @@trap_nocatch                          ; unable to restore from double
                jmp     _trap_handle                            ; task switch (must be in #8 only)
;----------------------------------------------------------------
common_trap     label   near                                    ;
                push    eax                                     ;
                mov     ax,cs                                   ; we assume ds = cs+8
                add     ax,SEL_INCR                             ; QSINIT make flat
                push    ds                                      ; selector in this way
                mov     ds,ax                                   ;

                call    _mt_swlock                              ; we have ds, call lock

                mov     [trap_data.tss_es],es                   ;
                pop     es                                      ;
                mov     [trap_data.tss_ds],es                   ;
                mov     es, ax                                  ;
                pop     eax                                     ; storing trap data
                mov     [trap_data.tss_eax],eax                 ;
                pop     eax                                     ; exception number
                mov     [trap_data.tss_reservdbl],ax            ;
                pop     eax                                     ; error code
                mov     [trap_data.tss_cr3],eax                 ;
                mov     eax, offset trap_data                   ;
                mov     [eax+tss_s.tss_ebx],ebx                 ;
                mov     [eax+tss_s.tss_ecx],ecx                 ;
                mov     [eax+tss_s.tss_edx],edx                 ;
                mov     [eax+tss_s.tss_esi],esi                 ;
                mov     [eax+tss_s.tss_edi],edi                 ;
                mov     [eax+tss_s.tss_ebp],ebp                 ;
                lea     ecx,[esp+12]                            ;
                mov     [eax+tss_s.tss_esp],ecx                 ;
                mov     ecx,[esp+8]                             ;
                mov     [eax+tss_s.tss_eflags],ecx              ;
                mov     ecx,[esp]                               ;
                mov     [eax+tss_s.tss_eip],ecx                 ;
                mov     cx,[esp+4]                              ;
                mov     [eax+tss_s.tss_cs],cx                   ;
                mov     [eax+tss_s.tss_ss],ss                   ;
                mov     [eax+tss_s.tss_fs],fs                   ;
                mov     [eax+tss_s.tss_gs],gs                   ;
                mov     [eax+tss_s.tss_backlink],0              ;

                public  _trap_handle
_trap_handle    label   near
                mov     ecx,dr6                                 ;
                mov     [eax+tss_s.tss_esp1],ecx                ;
                mov     ecx,dr7                                 ;
                mov     [eax+tss_s.tss_esp2],ecx                ;
                str     cx                                      ;
                mov     [eax+tss_s.tss_ss2],cx                  ;

                mov     ecx,xcpt_top                            ;
                jecxz   @@trap_nocatch                          ;
                push    eax                                     ;
                call    walk_xcpt                               ;
                pop     eax                                     ;
@@trap_nocatch:
                push    exf4_line                               ;
                push    exf4_file                               ;
                push    eax                                     ;
                call    _trap_screen                            ;
                push    4                                       ; QERR_SOFTFAULT
                call    _reipl                                  ;
                UD2                                             ; just to guarantee

; ---------------------------------------------------------------
; this point called with disabled interrupts and foreign stack
; (special buffer, allocated by EFI part)
;
                public  _trap_handle_64
_trap_handle_64 label   near
                call    _mt_swlock                              ; lock it
                mov     ecx,dr6                                 ;
                mov     [eax+tss_s.tss_esp1],ecx                ;
                mov     ecx,dr7                                 ;
                mov     [eax+tss_s.tss_esp2],ecx                ;
                str     cx                                      ;
                mov     [eax+tss_s.tss_ss2],cx                  ;
                push    edx                                     ; xcpt64_data
                push    eax                                     ;
                mov     ecx,xcpt_top                            ;
                jecxz   @@trap64_nocatch                        ;
                call    walk_xcpt                               ;
@@trap64_nocatch:
                call    _trap_screen_64                         ;
                push    4                                       ; QERR_SOFTFAULT
                call    _reipl                                  ;
                UD2                                             ;

; walk exception stack
; ---------------------------------------------------------------
; in: eax = trap_data*, ecx = sys_xcpt*
; function must be called after mt_lock++, it dec it on success
; exception handling
walk_xcpt       proc    near
                movzx   edx,[eax].tss_reservdbl                 ;
                mov     edi,ecx                                 ;
                xor     ecx,ecx                                 ; zero broken
                mov     xcpt_broken,ecx                         ; flag
                jmp     @@walk_xcpt_start                       ;
@@walk_xcpt_loop:
                cmp     [edi].xcpt_sign,xcpt_rec_sign           ; check sign
                jnz     @@walk_xcpt_broken                      ;
                test    [edi].xcpt_catch,xcpt_cinit             ; check init flag
                jz      @@walk_xcpt_parent                      ;
                mov     [edi].xcpt_catch,edx                    ; drop flag & save number
                mov     exf4_line,ecx                           ;
                mov     exf4_file,ecx                           ; zero file/line
                lea     edx,[edi].xcpt_jmp                      ;
                or      cx,[eax].tss_backlink                   ; was it task gate?
                jnz     @@walk_xcpt_taskret                     ;
@@walk_xcpt_longjmp:
                push    1                                       ; args for longjmp
                push    edx                                     ;
                ;clts                                            ;
                call    _fpu_updatets                           ; update TS flag
                call    _mt_swunlock                            ; unlock mt, it calls yeild
                call    __longjmp                               ; so we must be safe here!
@@walk_xcpt_parent:
                mov     edi,[edi].xcpt_nextrec                  ; next record
@@walk_xcpt_start:
                or      edi,edi                                 ; end of list?
                jnz     @@walk_xcpt_loop                        ;
@@walk_xcpt_broken:
                setnz   byte ptr xcpt_broken                    ; flag to trap
                ret                                             ; screen
@@walk_xcpt_taskret:
                add     cx,SEL_INCR                             ;
                mov     fs,cx                                   ;
                mov     fs:[0].tss_eip,offset @@walk_xcpt_longjmp
                mov     fs:[0].tss_edx,edx                      ;
                mov     fs:[0].tss_cs,cs                        ;
                mov     fs:[0].tss_ds,ds                        ; here we get ss:esp
                mov     fs:[0].tss_es,ds                        ; from longjmp buffer,
                mov     cx,[edx].r_ss                           ; this is ugly, but thread safe
                mov     fs:[0].tss_ss,cx                        ; (we unlocked BEFORE longjmp,
                mov     ecx,[edx].r_eax                         ; so we cannot use static
                mov     fs:[0].tss_esp,ecx                      ; stack as it was earlier)
                xor     ecx,ecx                                 ;
                mov     [eax].tss_backlink,cx                   ; zero task flag
                mov     fs,cx                                   ;
                mov     esp,[eax].tss_esp0                      ;
                pushfd                                          ; someone can drop it
                or      dword ptr [esp],CPU_EFLAGS_NT           ;
                popfd                                           ;
                ret                                             ; ret to iretd for this task
walk_xcpt       endp

; check xcpt signature in caller`s first parameter and shows trap on mismatch
; in: [esp+8] = sys_xcpt*
; out: trap screen or eax = sys_xcpt*
check_sign      proc    near                                    ;
                push    eax                                     ;
                mov     eax,[esp+8+4]                           ;
                pushfd                                          ;
                cmp     [eax].xcpt_sign,xcpt_rec_sign           ;
                jz      @@chk_xcs_ok                            ;
                lea     eax,[esp+8+8]                           ;
                xchg    eax,[esp]                               ; caller of _sys func
                push    eax                                     ; eflags
                mov     eax,[esp+8]                             ; restore eax
                push    dword ptr [esp+4+12]                    ; eip
                push    cs                                      ;
                push    offset _exit_pm32                       ;
                push    0                                       ; simulate trap
                push    xcpt_signerr                            ; stack
                jmp     common_trap                             ;
@@chk_xcs_ok:
                add     esp,8                                   ;
                ret                                             ;
check_sign      endp

;----------------------------------------------------------------
; void _std sys_exfunc1(sys_xcpt* except, const char* file, u32t line);
                public  _sys_exfunc1
_sys_exfunc1    proc    near
                mov     eax,[esp+4]                             ;
                mov     ecx,[esp+8]                             ;
                mov     [eax].xcpt_file,ecx                     ;
                mov     ecx,[esp+12]                            ;
                mov     [eax].xcpt_line,ecx                     ;
                mov     [eax].xcpt_sign,xcpt_rec_sign           ;
                mov     [eax].xcpt_catch,xcpt_cinit             ;
                pushfd                                          ;
                cli                                             ;
                mov     ecx,xcpt_top                            ;
                mov     [eax].xcpt_nextrec,ecx                  ;
                mov     xcpt_top,eax                            ;
                popfd                                           ;
                ret     12                                      ;
_sys_exfunc1    endp

; pop exception from stack
;----------------------------------------------------------------
; void _std sys_exfunc3(sys_xcpt* except);
                public  _sys_exfunc3
_sys_exfunc3    proc    near
                call    check_sign                              ;
                pushfd                                          ;
                cli                                             ;
                cmp     eax,xcpt_top                            ; current top of
                jnz     @@fxcpt3_broken                         ; stack?
                mov     ecx,[eax].xcpt_nextrec                  ;
                jecxz   @@fxcpt3_endoflist                      ; next in chain
                cmp     [ecx].xcpt_sign,xcpt_rec_sign           ;
                jnz     @@fxcpt3_broken                         ;
@@fxcpt3_endoflist:
                mov     xcpt_top,ecx                            ;
                popfd                                           ;
                ret     4                                       ;
@@fxcpt3_broken:
                popfd                                           ;
                xor     eax,eax                                 ;
                call    check_sign                              ; trap screen
                ret     4                                       ;
_sys_exfunc3    endp

; throw exception
;----------------------------------------------------------------
; void _std sys_exfunc4(u32t xcptype, const char* file, u32t line);
                public  _sys_exfunc4
_sys_exfunc4    proc    near
                push    eax                                     ; save eax
                pushfd                                          ;
                mov     eax,[esp+12+8]                          ; save file/line
                mov     exf4_line,eax                           ;
                mov     eax,[esp+8+8]                           ;
                mov     exf4_file,eax                           ;
                lea     eax,[esp+16+8]                          ; stack before call
                xchg    eax,[esp+4]                             ;
                push    cs                                      ;
                push    [esp+12]                                ; address
                push    0                                       ; except.code
                push    [esp+4+20]                              ; and number
                jmp     common_trap                             ;
_sys_exfunc4    endp

; rethrow exception to parent handler / screen trap
;----------------------------------------------------------------
; void _std sys_exfunc5(sys_xcpt* except, const char* file, u32t line);
                public  _sys_exfunc5
_sys_exfunc5    proc    near
                call    check_sign                              ;
                call    _mt_swlock                              ; mt lock
                mov     ecx,xcpt_top                            ; go to parent
                mov     eax,offset trap_data                    ;
                call    walk_xcpt                               ;

                push    dword ptr [esp+12]                      ; failed?
                push    dword ptr [esp+12]                      ; trap it!
                push    offset trap_data                        ;
                call    _trap_screen                            ;
                push    4                                       ; QERR_SOFTFAULT
                call    _reipl                                  ;
                UD2                                             ;
_sys_exfunc5    endp

; return pointer to setjmp buffer
;----------------------------------------------------------------
; jmp_buf* _std sys_exfunc6(sys_xcpt* except);
                public  _sys_exfunc6
_sys_exfunc6    proc    near
                call    check_sign                              ;
                add     eax,offset xcpt_jmp                     ;
                ret     4                                       ;
_sys_exfunc6    endp

; return current exception number
;----------------------------------------------------------------
; u32t _std sys_exfunc7(sys_xcpt* except);
                public  _sys_exfunc7
_sys_exfunc7    proc    near
                call    check_sign                              ;
                mov     eax,[eax].xcpt_catch                    ;
                test    eax,xcpt_cinit                          ;
                jz      @@fxcpt7_ok                             ;
                mov     eax,xcpt_signerr                        ;
@@fxcpt7_ok:
                ret     4                                       ;
_sys_exfunc7    endp


; dump registers into log/serial port
;----------------------------------------------------------------
; void  _std  log_dumpregs(int level);
; void  _std  hlp_dumpregs(void);

                public  _hlp_dumpregs, _log_dumpregs            ;
_hlp_dumpregs   proc    near                                    ;
                xchg    eax,[esp]                               ;
                push    eax                                     ; make log_dumpregs(-1)
                mov     eax,-1                                  ; stack 
                xchg    eax,[esp+4]                             ; 
_log_dumpregs   label   near                                    ; flags should be
                push    ebp                                     ; preserved until
                mov     ebp,esp                                 ; pushfd below!
                push    ds                                      ;
                push    es                                      ;
                push    ss                                      ; who knows?
                mov     es,[esp]                                ;
                pop     ds                                      ;
                lea     esp,[esp - size tss_s]                  ;
                pushad                                          ; esp = regs!
                lea     esi,[esp + size pushad_s]               ; esi = struct
                mov     [esi].tss_eax,eax                       ;
                pushfd                                          ;
                pop     eax                                     ;
                mov     [esi].tss_eflags,eax                    ;
                mov     eax,[esp].pa_esi                        ;
                mov     [esi].tss_esi,eax                       ;
                mov     eax,[ebp]                               ; simulate stack
                mov     [esi].tss_ebp,eax                       ; state before call
                lea     eax,[ebp+12]                            ;
                mov     [esi].tss_esp,eax                       ;
                mov     eax,[ebp+4]                             ;
                mov     [esi].tss_eip,eax                       ;
                mov     [esi].tss_ss,ss                         ;
                mov     [esi].tss_fs,fs                         ;
                mov     [esi].tss_gs,gs                         ;
                mov     [esi].tss_cs,cs                         ;
                mov     ax,[ebp-4]                              ;
                mov     [esi].tss_ds,ax                         ;
                mov     ax,[ebp-8]                              ;
                mov     [esi].tss_es,ax                         ;
                mov     [esi].tss_ebx,ebx                       ;
                mov     [esi].tss_ecx,ecx                       ;
                mov     [esi].tss_edx,edx                       ;
                mov     [esi].tss_edi,edi                       ;
                mov     eax,dr6                                 ;
                mov     [esi].tss_esp1,eax                      ;
                mov     eax,dr7                                 ;
                mov     [esi].tss_esp2,eax                      ;
                str     ax                                      ;
                mov     [esi].tss_ss2,ax                        ;

                push    [esi].tss_eflags                        ; function may alter it
                push    [ebp+8]                                 ;
                push    esi                                     ;
                call    _log_dumpregs_int                       ;
                popfd                                           ;
                popad                                           ;
                mov     es,[ebp-8]                              ;
                mov     ds,[ebp-4]                              ;
                mov     esp,ebp                                 ;
                pop     ebp                                     ;
                ret     4                                       ;
_hlp_dumpregs   endp                                            ;

CODE32          ends                                            ;

                end
