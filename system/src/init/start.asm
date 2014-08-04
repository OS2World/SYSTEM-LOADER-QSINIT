;
; QSINIT
; init process
;

                .386p
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ;
                include inc/basemac.inc                         ;
                include inc/filetab.inc                         ;
                include inc/debug.inc                           ;
                include inc/ioint13.inc                         ; for BPB
                include inc/iopic.inc                           ;
                include inc/qsconst.inc                         ;
                include inc/cpudef.inc

PMODE_TEXT      segment                                         ;
                extrn _pm_info:near                             ;
                extrn _pm_init:near                             ;
PMODE_TEXT      ends                                            ;

_TEXT           segment                                         ;
                extrn _calibrate_delay:near                     ; io delay calibrate call
                extrn settimerirq:near                          ; set int 50h handler
                extrn pm16set:near                              ; 16bit pm init part
                extrn int13check:near                           ; int13 support check
                extrn _checkresetmode:near                      ;
_TEXT           ends                                            ;

_STACK          segment
                public  bssend                                  ;
                align   2                                       ;
bssend          label word                                      ; end of BSS seg
                dw      1536 dup (?)                            ;
stacktop        dw      $                                       ; top of stack
_STACK          ends

_DATA           segment                                         ;
                public  _filetable                              ;
                public  _minifsd_ptr                            ;
                public  _DiskBufRM_Seg                          ;
                ; ----> must be in the same order and place,
                ;       exported to bootos2.exe <----------------
_minifsd_ptr    dd      0                                       ; minifsd copied here
minifsd_flags   dw      0                                       ; boot flags
_DiskBufRM_Seg  dw      0                                       ; RM far disk buffer segment
_filetable      filetable_s <>                                  ;
                ; -----------------------------------------------
filetabptr      dd      0                                       ;
_DATA           ends                                            ;

_ABOUT          segment
                public  _aboutstr                               ;
_aboutstr       label   near                                    ;
                include inc/version.inc                         ;
                db      10,0                                    ;
_ABOUT          ends   

STARTBSS        segment                                         ;
                align 2                                         ;
bssstart        label   word                                    ;
                public  _pmdataseg                              ;
                public  _logbufseg                              ;
                public  _physmem                                ;
                public  intrmatrix                              ;
init_print_attr db      ?                                       ;
                align 2                                         ;
_pmdataseg      dw      ?                                       ; pm data segment
_logbufseg      dw      ?                                       ; rm log buffer segment
intrmatrix      db      100h dup(?)                             ; INT redirectors for all INTs
_physmem        db      100h dup (?)                            ; 32 * 8 (!!)
STARTBSS        ends                                            ;

_BSS            segment                                         ;
                public  _rm16code                               ;
                public  _safeMode                               ;
                public  _BootBPB                                ;
_rm16code       dw      ?                                       ;
_safeMode       db      ?                                       ;
_BootBPB        Disk_BPB <>                                     ; BPB
_BSS            ends                                            ;

;
; ENTRY
;    (CS:IP) = entry point address
;            = 2000:0000H for FAT boot system
;    (SS:SP) = 0030:00FAH for FAT boot system (BM/MBR dependent)
;    (BX)    = First data sector on the disk (0 based)
;    (DH)    = boot flags:
;              0x00 - FAT boot
;              BF_NOMFSHVOLIO  0x01    // mini-FSD does not use mFSH_DOVOLIO
;              BF_RIPL         0x02    // remote IPL flag
;              BF_MINIFSD      0x04    // mini-FSD flag
;              BF_NOPICINIT    0x08    // do not initialize the PIC
;              BF_MICROFSD     0x10    // micro-FSD flag
;              BF_RESERVED     0xE0    // reserved, must be 0
;    (DL)    = drive number for the boot disk (ignored if no BF_NOMFSHVOLIO or BF_MINIFSD)
;    (DS:SI) = address of BOOT MEDIA'S bpb (ignored if no BF_NOMFSHVOLIO or BF_MINIFSD)
;    (ES:DI) = address of root directory / micro-fsd file table
;
;  FAT32 boot sector:
;    (DH)    = boot flags
;    (DL)    = drive number for the boot disk
;    (DS:SI) = address of BOOT MEDIA'S bpb
;
INITCODE        segment
                assume cs:DGROUP,ds:DGROUP,es:nothing,ss:DGROUP
start:
                mov     word ptr cs:[_dd_bpbseg],ds             ;
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                mov     word ptr [LdrRstSS],ss                  ; saving start values for restart
                mov     word ptr [LdrRstSP],sp                  ; loader function
                mov     word ptr [_dd_bootdisk],dx              ;
                mov     minifsd_flags,dx                        ;
                mov     word ptr [_dd_bpbofs],si                ;
                mov     word ptr [_dd_firstsector],bx           ;
                mov     word ptr [_dd_rootseg],es               ;
                mov     word ptr [_dd_rootofs],di               ;
                mov     word ptr [_LdrRstCS],cs                 ;
                mov     word ptr [filetabptr+2],es              ; save filetable/rootdir pointer
                mov     word ptr [filetabptr],di                ;
                mov     es,ax                                   ;

                mov     di,offset bssstart                      ; clear BSS segment
                mov     cx,offset bssend                        ;
                sub     cx,di                                   ;
                shr     cx,1                                    ;
                xor     ax,ax                                   ;
                cld                                             ;
            rep stosw                                           ;
                mov     di,offset intrmatrix                    ; fill int routers
                mov     cl,80h                                  ;
                mov     ax,0CCCCh                               ;
            rep stosw                                           ;
                mov     ax,offset stacktop                      ; length of ldr image
                add     ax,PAGESIZE - 1                         ; align to page
                and     ax,not PAGEMASK                         ;
                mov     _qsinit_size, ax                        ; own segment size (page aligned)
                mov     cx,cs                                   ;
                mov     ss,cx                                   ;
                movzx   esp,ax                                  ; switch stack

                mov     ax,1003h                                ; blink off
                xor     bl,bl                                   ;
                int     10h                                     ;
                mov     ah,2                                    ; check left shift state
                int     16h                                     ;
                and     al,2                                    ;
                setnz   _safeMode                               ;
                add     byte ptr mhdr_about,al                  ; change color of |LOADING|

                xor     ax,ax                                   ;
                mov     dx,ax                                   ; left corner
                call    init_print                              ; hello world!

ifdef INITDEBUG
                mov     ax,word ptr _dd_bootdisk
                dbg16print <10,"bootdisk: %x",10>,<ax>          ;
endif
                int     12h                                     ; get int 12 value
ifdef INITDEBUG
                dbg16print <"lowmem: %d kb",10>,<ax>            ;
endif
                mov     dx,ax                                   ; check for sufficient
                mov     ax,3                                    ; place to to put self
                cmp     dx,512                                  ; in the top of low memory
                jc      panic_initpm                            ; <512kb -> error
                shl     dx,10-PARASHIFT                         ; kb->paragraphs
                mov     ax,_qsinit_size                         ;
                shr     ax,PARASHIFT                            ; use 9100:0 by default,
                sub     dx,ax                                   ; else try to calculate
                mov     cx,QSINIT_SEGREAL                       ;
                cmp     dx,cx                                   ;
                jnc     @@init_9100ok                           ;
                mov     cx,not ((1 shl (PAGESHIFT-PARASHIFT)) - 1) ; round to page mask
                and     cx,dx                                   ; own target segment
@@init_9100ok:
ifdef INITDEBUG
                dbg16print <"new cs: %x",10>,<cx>               ;
endif
                mov     _rm16code, cx                           ;

                call    _pm_info                                ; check cpu type
                jc      panic_initpm                            ; exit on error
                ;dbg16print <"Need %x paragraphs",10>,bx         ;
                neg     bx                                      ;
                mov     _pmdataseg,bx                           ;
                cmp     cl, 4                                   ;
                mov     ax, 1                                   ; if 486?
                jc      panic_initpm                            ; no, exit too

                mov     bx,ds                                   ; save
                mov     dh,_dd_bootflags                        ;
                mov     ah,dh                                   ; copy flags to ah
                and     dh,not BF_NOPICINIT                     ;

                movzx   cx,ah                                   ;
                mov     es,bx                                   ;
ifdef INITDEBUG
                dbg16print <"boot flags: %x",10>,<cx>           ;
endif
                cmp     dh,al                                   ; FSD mode?
                jc      @@init_noft                             ;
                lds     si,[filetabptr]                         ; (ds:si) = filetable pointer
                mov     di,offset _filetable                    ;
                mov     cx,(size filetable_s) shr 1             ;
            rep movsw                                           ; copying filetable
                mov     ds,bx                                   ; restore

; checking file table ...
                mov     al,5                                    ; message code
                mov     cx,[_filetable.ft_loaderseg]            ; check micro-fsd report
                cmp     bx,cx                                   ; table wrong
                jnz     panic_initpm                            ;
                cmp     bx,OS2LDR_SEGREAL                       ; we are loaded not at 1000:0
                jnz     panic_initpm                            ;

                and     dh,BF_MICROFSD or BF_RESERVED           ;
                cmp     dh,BF_MICROFSD                          ; check flags
                jnz     panic_initpm                            ;

                mov     bx,_rm16code                            ; our's next location
                xor     si,si                                   ; zero
                cmp     si,word ptr [_filetable.ft_loaderlen+2] ; loader > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_mulen+2]     ; microfsd > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_mfsdlen+2]   ; minifsd > 64k
                jnz     panic_initpm                            ;
                cmp     si,word ptr [_filetable.ft_loaderlen]   ; loader len field == 0?
                jz      panic_initpm                            ;
                mov     cx,word ptr [_filetable.ft_mulen]       ; microfsd len field == 0?
                or      cx,cx                                   ;
                jz      panic_initpm                            ;
                shr     cx,PARASHIFT                            ;
                add     cx,[_filetable.ft_museg]                ; microfsd use memory
                sub     cx,bx                                   ; above 9100:0 or
                jnc     panic_initpm                            ; below 8100:0?
                add     cx,_64KB SHR PARASHIFT                  ;
                jnc     panic_initpm                            ;

                mov     cx,[_filetable.ft_museg]                ; segment for pm_init data
                add     _pmdataseg,cx                           ;

                mov     cx,[_filetable.ft_cfiles]               ; check ft_cfiles
                cmp     cx,8                                    ;
                jnc     panic_initpm                            ; accept all of 3..7
                cmp     cl,3                                    ;
                jc      panic_initpm                            ;

                mov     cx,word ptr [_filetable.ft_mfsdlen]     ; minifsd check
                test    ah,BF_MINIFSD                           ;
                jz      @@init_nomini_1                         ;
                or      cx,cx                                   ; minifsd size == 0?
                jnz     @@init_nomini_1                         ;
                and     ah,not BF_MINIFSD                       ; drop BF_MINIFSD flag
                and     byte ptr _dd_bootflags,ah               ;
                and     byte ptr minifsd_flags+1,ah             ; 
@@init_nomini_1:
                test    ah,BF_RIPL                              ; ripl on?
                jz      @@init_noripl                           ;
                cmp     si,word ptr [_filetable.ft_ripllen+2]   ; ripl data
                jnz     panic_initpm                            ; >64k
                mov     dx,word ptr [_filetable.ft_ripllen]     ;
                cmp     si,dx                                   ; ignore empty
                jz      @@init_noripl                           ; ripl data
                add     cx,dx                                   ; minifsd + ripl len
                jc      panic_initpm                            ; >64k
                mov     si,[_filetable.ft_riplseg]              ;
                cmp     si,MFSDBASE SHR PARASHIFT               ; check ripl seg location:
                jc      panic_initpm                            ; > RIPLBASE and < 1000:0
                shr     dx,PARASHIFT                            ; NOTE: replaced to MFSDBASE
                add     si,dx                                   ; for incorrect DToD PXE boot
                cmp     si,OS2LDR_SEGREAL                       ;
                ja      panic_initpm                            ;
@@init_noripl:
                test    ah,BF_MINIFSD                           ; minifsd present?
                jz      @@init_nomini_2                         ;
                mov     si,[_filetable.ft_mfsdseg]              ;
                cmp     si,MFSDBASE SHR PARASHIFT               ; check minifsd seg location:
                jc      panic_initpm                            ; > MFSDBASE
                shr     cx,PARASHIFT                            ; and
                add     si,cx                                   ; < 1000:0
                cmp     si,OS2LDR_SEGREAL                       ;
                ja      panic_initpm                            ;
@@init_nomini_2:
                clc                                             ; skip _pmdataseg FAT boot setup
@@init_noft:
                mov     bx,_rm16code                            ; our's next location
                jnc     @@init_nofsd_pm                         ;
                add     _pmdataseg,bx                           ; _pmdataseg in FAT boot
@@init_nofsd_pm:
; migrating to the top of low memory ...
                mov     es,bx                                   ; copy self to 9100:0
                xor     di,di                                   ;
                mov     cx,_qsinit_size                         ;
                mov     si,di                                   ;
                shr     cx,2                                    ;
            rep movsd                                           ;
                push    bx                                      ; jump to 9100:0
                push    offset @@init_9100                      ;
                mov     dx,cs                                   ;
                retf                                            ;
@@init_9100:
                mov     ss,bx                                   ; home, sweet home ;)
                mov     ds,bx                                   ;

                mov     cx,_pmdataseg                           ; DMPI data (GDT, etc)
                and     cx,NOT ((PAGESIZE SHR PARASHIFT) - 1)   ; round it to page
                mov     _pmdataseg,cx                           ; and allocate 4k log buffer
                sub     cx,LOGBUF_SIZE SHR PARASHIFT            ; for logging debug output
                mov     _logbufseg,cx                           ; in RM callbacks

                test    ah,BF_NOMFSHVOLIO                       ; BPB available?
                jnz     @@init_noBPB                            ;
                mov     si,word ptr _dd_bpbofs                  ; yes, copy it
                mov     di,offset _BootBPB                      ;
                mov     ds,word ptr _dd_bpbseg                  ;
                mov     cx,size Disk_BPB                        ;
            rep movsb
                mov     ds,bx                                   ; restore ds
                jmp     @@init_copyMFSD
@@init_noBPB:
                mov     byte ptr _dd_bootdisk, 0FFh             ; guarantee invalid disk
                mov     byte ptr minifsd_flags, 0FFh            ; number
@@init_copyMFSD:
                test    ah,BF_MINIFSD                           ; mini-fsd exists?
                jz      @@init_nominifsd                        ;
                mov     es,dx                                   ; copy mini-fsd to
                xor     di,di                                   ; our`s old place to prevent
                mov     cx,word ptr [_filetable.ft_mfsdlen]     ; cleaning it by disk i/o
                xor     si,si                                   ;
                mov     ds,[_filetable.ft_mfsdseg]              ;
            rep movsb                                           ;
                mov     ds,bx                                   ;
                mov     word ptr _minifsd_ptr + 2,dx            ;
                mov     es,bx                                   ;
@@init_nominifsd:
                call    int13check                              ; check int 13h
                push    cs                                      ;
                call    _calibrate_delay                        ; calibrate io delay
                call    init_memory                             ; get system memory info
                call    remap_pic                               ; remap PIC1 to int 50h
                call    settimerirq                             ; set own irq0 handler

                xor     di,di                                   ; fill all of space
                mov     cx,offset @@init_nominifsd              ; above with int 3
                mov     es,_rm16code                            ; to catch goto 0 :)
                mov     al,0CCh                                 ; 
            rep stosb                                           ;
; protected mode ...
                mov     es,_pmdataseg                           ;
                call    _pm_init                                ; init
                jc      panic_initpm                            ;
                call    pm16set                                 ; go to 16 bit pm code

                public  panic_initpm                            ;
                public  panic_initpm_np                         ;
panic_initpm:
                mov     dx,200h                                 ; 3nd line, 0 col
panic_initpm_np:
                call    init_print                              ; print error
halt_sys:
                sti                                             ; allow C-A-D
                hlt                                             ; and loop forever
                jmp     short halt_sys                          ;
; ===============================================================
; initial (real mode) message print.
; IN: al = message number
;     dx = pos
;
                public init_print
init_print      proc  near                                      ;
                xor     ah,ah                                   ; save pos & msg
                push    ax                                      ;
                push    dx                                      ;
                xor     edx,edx                                 ; far call to reset mode
                or      ax,ax                                   ; but clear screen on
                setz    dl                                      ; msg #0 only
                push    edx                                     ;
                push    cs                                      ;
                call    _checkresetmode                         ;
                pop     dx                                      ;
                pop     bx                                      ;
                push    cs                                      ; check ds
                pop     ds                                      ;
                shl     bx,1                                    ;
                mov     si,[bx+embedded_errs]                   ;
                lodsb                                           ;
                push    si                                      ;
                cbw                                             ;
                mov     bx,ax                                   ;
                shl     bx,1                                    ;
                mov     si,[bx+embedded_hdrs]                   ;
                lodsb                                           ; load color attribute
                mov     init_print_attr,al                      ; from first byte
@@msgloop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@msgloop_end                           ;
                mov     ah,09h                                  ;
                push    ds                                      ; save all when we call bios
                push    si                                      ;
                                                                ;
                push    dx                                      ; update position
                push    ax                                      ;
                mov     ah,2                                    ;
                xor     bh,bh                                   ;
                int     10h                                     ;
                pop     ax                                      ;

                xor     bh,bh                                   ;
                mov     bl,cs:init_print_attr                   ; get attribute
                mov     cx,1                                    ;
                int     10h                                     ;
                pop     dx                                      ;
                inc     dx                                      ; update cursor position
                pop     si                                      ;
                pop     ds                                      ;
                jmp     @@msgloop                               ;
@@msgloop_end:
                cmp     si,offset msg_no486                     ; this was a
                jnc     @@msgloop_exit                          ; header print?
                pop     si                                      ;
                jmp     @@msgloop                               ; yes, print msg
@@msgloop_exit:
                ret                                             ;
init_print      endp                                            ;

INITCODE        ends

                include misc/ldrrst.inc
                include misc/meminit.inc
                include misc/mappic.inc

_DATA           segment
embedded_errs   dw      offset msg_loading, offset msg_no486, offset msg_notrm,
                        offset msg_memlow, offset msg_a20, offset msg_filetab,
                        offset msg_dpmierr, offset msg_int13chs, offset msg_no32mb,
                        offset msg_nothing, offset msg_mcberr, offset msg_faterr,
                        offset msg_diskerr, offset msg_noextdta, offset msg_runextdta,
                        offset msg_disknomem
embedded_hdrs   dw      offset mhdr_warning, offset mhdr_fatal, offset mhdr_about
mhdr_warning    db      0Eh, 'Warning: ',0
mhdr_fatal      db      0Ch, 'Fatal: ', 0
mhdr_about      db      0Ah, 0, 0
msg_loading     db      2, 0DBh,0DBh,' LOADING ',0DBh,0DBh,0       ; 00
msg_no486       db      1,'no 80486+ detected.',0                  ; 01
msg_notrm       db      1,'system already in protected mode.',0    ; 02
msg_memlow      db      1,'invalid low memory size.',0             ; 03
msg_a20         db      0,0                                        ; 04
msg_filetab     db      1,'invalid microfsd data.',0               ; 05
msg_dpmierr     db      1,'protected mode interface error.',0      ; 06
msg_int13chs    db      1,0                                        ; 07
msg_no32mb      db      1,'at least 24Mb required.',0              ; 08
msg_nothing     db      0,'nothing to do.',0                       ; 09
msg_mcberr      db      1,'memory allocation error.',0             ; 10
msg_faterr      db      1,'FAT processing error.',0                ; 11
msg_diskerr     db      1,'disk read error.',0                     ; 12
msg_noextdta    db      1,'QSINIT.LDI is missing or invalid.',0    ; 13
msg_runextdta   db      1,'unable to unpack secondary code.',0     ; 14
msg_disknomem   db      1,'no memory for virtual disk.',0          ; 15
                align 4
                public  _qsinit_size
_qsinit_size    dw      0                                       ; own size, rounded up to 4k

_DATA           ends
                end start
