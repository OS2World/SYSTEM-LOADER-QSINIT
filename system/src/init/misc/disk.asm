;
; QSINIT
; raw disk info & read/write
; ---------------------------------------------------------------
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ; segments
                include inc/basemac.inc                         ;
                include inc/ioint13.inc                         ;
                include inc/debug.inc                           ;
                include inc/qsconst.inc                         ;
                include inc/dskinfo.inc                         ;
                include inc/qsinit.inc                          ;

                .486p

                MAX_INT13_RETRY = 10                            ; maximum number of read retries
                SECT_PER_READ   = 120                           ; number of sectors per one r/w op
AF_SECTORS      equ     AF_ALIGN SHR SECTSHIFT                  ;

_BSS16          segment                                         ;
ext_packet      ExtReadPacket <>                                ;
                align   4                                       ;
lasterror_i13   db      ?                                       ;
                align   4                                       ;

                public  _qd_array, _qd_fdds, _qd_hdds
_qd_array       label   byte                                    ;
REPT MAX_QS_DISK                                                ;
                qs_diskinfo <>                                  ;
ENDM                                                            ;
_qd_fdds        db      ?                                       ; # fdds in qs_diskinfo
_qd_hdds        db      ?                                       ; # hdds in qs_diskinfo
                extrn   _BootBPB:Disk_BPB                       ;
_BSS16          ends                                            ;

DATA16          segment
                extrn   _DiskBufRM_Seg:word                     ;
                public  _useAFio, _qd_bootidx                   ;
_useAFio        db      1                                       ;
_qd_bootidx     dd      -1                                      ;
DATA16          ends


TEXT16          segment                                         ;
                assume  cs:G16, ds:G16, es:nothing, ss:nothing  ;

                extrn   _dd_bootdisk:byte

; read disks configuration 
;----------------------------------------------------------------
; on start _qd_array is zero-filled in BSS
;
                public int13check
int13check      proc    near
@@i13c_counter  = byte  ptr [bp-1]                              ;
@@i13c_index    = byte  ptr [bp-2]                              ;
                mov     bp,sp                                   ;
                mov     si,offset _qd_array                     ;
                xor     dx,dx                                   ;
                push    0                                       ; vars
@@i13c_loop:
                push    dx                                      ;
                mov     ah,15h                                  ;
                call    int13                                   ; Read DASD type
                pop     dx                                      ;
                jc      @@i13c_nodrv                            ; cx:dx here is incorrect,
                or      ah,ah                                   ; at least on Gigabyte MBs
                jz      @@i13c_nodrv                            ;
@@i13c_force_boot_fdd:
                mov     [si].qd_biosdisk,dl                     ; BIOS disk number
                mov     [si].qd_mediatype,ah                    ; DASD type
                or      [si].qd_flags,HDDF_PRESENT              ;
                inc     @@i13c_counter                          ;

                push    dx                                      ; get CHS for all
                xor     eax,eax                                 ; types to correct
                mov     ah,08h                                  ; CHS i/o
                call    int13                                   ;
                jc      @@i13c_fn08err                          ;
                ;dbg16print <"int13 08h: %x %x %x %x",10>,<dx,cx,bx,ax>
                or      cl,cl                                   ;
                jnz     @@i13c_fn08ok                           ;
@@i13c_fn08err:
                pop     dx                                      ;
                cmp     [si].qd_mediatype,3                     ;
                jnc     @@i13c_fn08skip                         ; is it boot FDD
                cmp     dl,_dd_bootdisk                         ; without CHS?
                jnz     @@i13c_fn08skip                         ;
                mov     cx,_BootBPB.BPB_SecPerTrack             ; then copy it
                mov     [si].qd_chsspt,cx                       ; from BPB!
                jcxz    @@i13c_fn08skip                         ;
                mov     ax,cx                                   ;
                mov     cx,_BootBPB.BPB_Heads                   ;
                mov     [si].qd_chsheads,cx                     ;
                jcxz    @@i13c_fn08skip                         ;
                push    dx                                      ;
                mul     cx                                      ; heads * spt
                push    dx                                      ; in ecx
                push    ax                                      ;
                pop     ecx                                     ;
                mov     eax,_BootBPB.BPB_TotalSecBig            ; calc # of cyls
                or      eax,eax                                 ;
                jnz     @@i13c_fn08err_1                        ;
                mov     ax,_BootBPB.BPB_TotalSec                ;
@@i13c_fn08err_1:
                xor     edx,edx                                 ;
                div     ecx                                     ;
                cmp     edx,1                                   ;
                cmc                                             ;
                adc     eax,0                                   ;
                mov     [si].qd_cyls,eax                        ;
                jmp     @@i13c_fn08skip                         ;
@@i13c_fn08ok:
                movzx   eax,dh                                  ; heads
                inc     ax                                      ;
                mov     [si].qd_chsheads,ax                     ;
                mov     al,cl                                   ; sectors
                and     al,3Fh                                  ;
                cbw                                             ;
                mov     [si].qd_chsspt,ax                       ;
                shr     cl,6                                    ; cylinders
                xchg    cl,ch                                   ;
                inc     cx                                      ; ah=08 cyls in cx
                mov     word ptr [si].qd_cyls,cx                ; zero must be in hi part
                pop     dx                                      ;
@@i13c_fn08skip:
                mov     al,[si].qd_mediatype                    ; hdd?
                cmp     al, 3                                   ;
                jz      @@i13c_hddinfo                          ;
                cmp     al, 2                                   ; and modern fdd type
                jz      @@i13c_hddinfo                          ; (thanks to Gigabyte!)
                cmp     dl,80h                                  ;
                jnc     @@i13c_hddinfo                          ; and any HDD
                jmp     @@i13c_fddinfo                          ; (thanks to QEMU!)
@@i13c_hddinfo:
                push    dx
                mov     ah, 41h                                 ; check for int 13 ext.
                mov     bx, 55AAh                               ;
                call    int13                                   ;
                pop     dx                                      ;
                mov     al,cl                                   ;
                jc      @@i13c_fddinfo                          ;
                ;dbg16print <"int13 41h: %x %x %x",10>,<cx,bx,ax>
                cmp     bx, 0AA55h                              ; check signature
                jnz     @@i13c_fddinfo                          ;
                and     cl, 1                                   ; check r/w support
                jz      @@i13c_fddinfo                          ;
                sub     sp,size EDParmTable                     ;
                push    si                                      ;
                lea     esi,[esp+2]                             ; DS==ES==SS !!!
                push    dx                                      ;

                mov     cx,size EDParmTable                     ;
                mov     di,si                                   ;
                xor     ax,ax                                   ;
                cld                                             ;
            rep stosb                                           ;
                mov     [si].ED_BufferSize, size EDParmTable    ;
                mov     ah, 48h                                 ;
                call    int13                                   ;
;stc
                mov     di,si                                   ;
                pop     dx                                      ;
                pop     si                                      ;
                jc      @@i13c_eddempty                         ;
                cmp     [di].ED_BufferSize, 1Ah                 ;
                jnc     @@i13c_eddready                         ;
@@i13c_eddempty:
                xor     eax,eax                                 ;
                mov     [di].ED_SectorSize,ax                   ;
                mov     [di].ED_Cylinders,eax                   ;
@@i13c_eddready:
                mov     eax,[di].ED_Cylinders                   ; copy from edd
                mov     [si].qd_cyls,eax                        ;
                mov     ax,word ptr [di].ED_Heads               ;
                mov     [si].qd_heads,ax                        ;
                mov     ax,word ptr [di].ED_SectOnTrack         ;
                mov     [si].qd_spt,ax                          ;
                mov     ax,[di].ED_SectorSize                   ;
                mov     [si].qd_sectorsize,ax                   ;
                mov     eax,dword ptr [di].ED_Sectors           ;
                mov     ecx,dword ptr [di].ED_Sectors + 4       ;
                cmp     ecx,-1                                  ; FFFF:FFFF
                jz      @@i13c_eddcheck                         ;
                mov     dword ptr [si].qd_sectors,eax           ;
                mov     dword ptr [si].qd_sectors + 4,ecx       ;
@@i13c_eddcheck:
                dbg16print <"%x [+] %lx%lx",10>,<eax,ecx,dx>    ;
                mov     eax,[si].qd_cyls                        ;
                or      eax,eax                                 ;
                jz      @@i13c_chsbad                           ; check for 0 & -1
                inc     eax                                     ; and save default 
                jnz     @@i13c_chsok                            ; in this case
@@i13c_chsbad:
                mov     [si].qd_cyls,16383                      ;
                mov     [si].qd_heads,16                        ;
                mov     [si].qd_spt,63                          ;
                dbg16print <"empty EDD for disk %x",10>,<dx>
@@i13c_chsok:
                add     sp,size EDParmTable
                or      [si].qd_flags,HDDF_LBAON or HDDF_LBAPRESENT
                jmp     @@i13c_infodone
@@i13c_nodrv:
                test    dl,80h                                  ; if this is boot
                jnz     @@i13c_nodrv_hdd                        ; FDD, then force it
                cmp     dl,_dd_bootdisk                         ; presence with CHS
                mov     ah,1                                    ; from BPB
                jz      @@i13c_force_boot_fdd                   ;
                jmp     @@i13c_nodrv_force                      ; force floppy entry presence
@@i13c_fddinfo:
                mov     ecx,[si].qd_cyls                        ;
                mov     ax,[si].qd_chsheads                     ;
                mov     [si].qd_heads,ax                        ;
                movzx   eax,[si].qd_chsspt                      ; zero high part
                mov     [si].qd_spt,ax                          ;

                push    dx                                      ;
                mul     cx                                      ; disk size
                shl     edx,16                                  ; in sectors
                or      eax,edx                                 ;
                mov     cx,[si].qd_heads                        ;
                mul     ecx                                     ;
                mov     dword ptr [si].qd_sectors,eax           ;
                mov     dword ptr [si].qd_sectors+4,edx         ;
                pop     dx                                      ;
                dbg16print <"%x [-] %lx",10>,<eax,dx>           ;
; - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
@@i13c_infodone:
                xor     eax,eax                                 ; check sector size
                cmp     [si].qd_sectorsize,ax                   ; and save default
                jnz     @@i13c_sszok                            ; if empty
                mov     [si].qd_sectorsize,512                  ;
@@i13c_sszok:
                bsf     ax,[si].qd_sectorsize                   ; sector shift
                mov     [si].qd_sectorshift,al                  ;
@@i13c_nodrv_force:
                add     si,size qs_diskinfo                     ;
                inc     @@i13c_index                            ;
                cmp     @@i13c_index,MAX_QS_DISK                ;
                jnc     @@i13c_done                             ;
@@i13c_nodrv_hdd:
                inc     dl                                      ;
                cmp     dl,0F1h                                 ; for boot cd-rom
                jz      @@i13c_done                             ;
                cmp     dl,MAX_QS_FLOPPY                        ;
                jnz     @@i13c_loop                             ;
                mov     dl,80h                                  ;
                mov     cl,@@i13c_counter                       ;
                mov     _qd_fdds,cl                             ; # of fdds
                jmp     @@i13c_loop                             ;
@@i13c_done:
                mov     cl,@@i13c_counter                       ;
                sub     cl,_qd_fdds                             ;
                mov     _qd_hdds,cl                             ; # of hdds
ifdef INITDEBUG
                xor     ch,ch                                   ;
                movzx   ax,_qd_fdds                             ;
                dbg16print <"int13check done (fd:%d,hd:%d)",10>,<cx,ax>
endif
                mov     sp,bp                                   ;
                ret                                             ;
int13check      endp

int13           proc    near                                    ; save registers
                SaveReg <esi,edi,ds,es>                         ; bios can destroy ANYTHING ;)
                int     13h                                     ;
                RestReg <es,ds,edi,esi>                         ;
                sti                                             ;
                cld                                             ;
                ret                                             ;
int13           endp                                            ;

;
; boot disk I/O handler
;----------------------------------------------------------------
; IN:     bl - action: 0=read, 1=write
;         cx - number of sectors to read
;         si - qs_diskinfo
;    edx:eax - first sector
; OUT:    CF - disk error
;      all registers destroyd
                public  sector_io
sector_io       proc    far
                xor     bh,bh                                   ; bl->bx
                push    bp                                      ;
                mov     bp,sp                                   ;
                push    edx                                     ; @@SectorBase
                push    eax                                     ;
                push    cx                                      ; @@SectorCount
; this is walking over the minefield
; int13ext can be incorrecly reported on some bioses, but my own
; return USB HDD both as fdd & hdd and stops on CHS reading.
; just leave it in this way for a while
;                test    [si].qd_biosdisk,80h                    ; check for hdd
;                jz      @@rsec_oldway                           ;
                test    [si].qd_flags,HDDF_LBAPRESENT           ;
                jz      @@rsec_oldway                           ;
                test    [si].qd_flags,HDDF_LBAON                ; ext. on?
                jz      @@rsec_oldway                           ; no, use CHS
                jmp     int13_rwext                             ; r/w via 42/43h and return
@@rsec_oldway:
@@SectorBaseHi  = dword ptr [bp- 4]                             ; start sector (hi dd)
@@SectorBaseLo  = dword ptr [bp- 8]                             ; start sector (lo dd)
@@SectorCount   = word  ptr [bp-10]                             ; sector count
@@RetryCount    = byte  ptr [bp-11]                             ;
@@ActionCode    = byte  ptr [bp-12]                             ; r/w op
@@CurrentSector = byte  ptr [bp-13]                             ;
@@CurrentHead   = byte  ptr [bp-14]                             ;

                mov     es,_DiskBufRM_Seg                       ; buffer
                add     bl,2                                    ;
                push    bx                                      ; int 13 ah value
                push    bx                                      ; reserve place for variables
if 0 ;def INITDEBUG
                movzx   di,[si].qd_biosdisk                     ;
                lea     edx,[ebx-2]                             ;
                dbg16print <"int13 %d %x %lx %x",10>,<cx,eax,di,dx>
endif
; calculate head/sector
@@rsec_loop:
                cmp     @@SectorBaseHi,1                        ; check for zero
                cmc                                             ; high dword of #sector
                jc      @@rsec_exit                             ;
                mov     eax,@@SectorBaseLo                      ; sector number
                xor     edx,edx                                 ;
                movzx   ecx,[si].qd_chsspt                      ;
                div     ecx                                     ;
                mov     @@CurrentSector,dl                      ; start sector.
                xor     edx,edx                                 ;
                movzx   ecx,[si].qd_chsheads                    ;
                div     ecx                                     ;
                mov     @@CurrentHead,dl                        ; head
                movzx   edi,ax                                  ; # of cylinder > 64k?
                cmp     edi,eax                                 ;
                jc      @@rsec_exit                             ; di = cylinder
@@rsec_goodcyl:
;----------------------------------------------------------------
                mov     ax,[si].qd_chsspt                       ;
                sub     al,@@CurrentSector                      ; read sectors up to the
                cbw                                             ; end of track
                cmp     ax,@@SectorCount                        ;
                jb      @@rsec_03                               ;
                mov     ax,@@SectorCount                        ;
@@rsec_03:
; perform read
                push    ax                                      ;
                mov     ah,@@ActionCode                         ; int 13 read operation
                mov     dx,di                                   ; dx - cylinder
                mov     cl,6                                    ;
                shl     dh,cl                                   ;
                or      dh,@@CurrentSector                      ;
                mov     cx,dx                                   ;
                xchg    ch,cl                                   ;
                inc     cl                                      ; sector is 1 based.
                mov     dl,[si].qd_biosdisk                     ;
                mov     dh,@@CurrentHead                        ;

                mov     @@RetryCount,MAX_INT13_RETRY            ; retry count
@@rsec_retry:
                push    ax                                      ;
                xor     bx,bx                                   ;
                call    int13_action                            ; try to read
                jnc     @@rsec_ok                               ;
                mov     lasterror_i13,ah                        ;
                cmp     @@RetryCount,MAX_INT13_RETRY            ; only once here!
                jnz     @@rsec_once                             ;
                xor     ah,ah                                   ; if no,
                call    int13_action                            ; reset the drive
@@rsec_once:
                pop     ax                                      ;
                dec     @@RetryCount                            ; next try?
                jnz     @@rsec_retry                            ;
                mov     al,lasterror_i13                        ; last error
                cbw                                             ;
                movzx   cx,@@CurrentHead                        ; print to log
                movzx   di,@@CurrentSector                      ;
                dbg16print <"int13: t:%d h:%d s:%d error %x",10>,<ax,di,cx,si>
                stc                                             ;
                jmp     @@rsec_04                               ; exit
@@rsec_ok:
                pop     ax                                      ;
@@rsec_04:
                pop     ax                                      ;
                jc      @@rsec_exit                             ; error?
                movzx   eax,al                                  ;
                sub     @@SectorCount,ax                        ; update sector count
                add     @@SectorBaseLo,eax                      ;
                mov     cl,[si].qd_sectorshift                  ; convert sectors to
                sub     cl,PARASHIFT                            ; paragraphs and update
                shl     ax,cl                                   ; destination address.
                mov     cx,es                                   ; 
                add     cx,ax                                   ;
                mov     es,cx                                   ;
                cmp     @@SectorCount,0                         ; done?
                jnz     @@rsec_loop                             ;
                clc                                             ;
@@rsec_exit:
                mov     sp,bp                                   ;
                pop     bp                                      ;
                ret
;
; read/write by int 13h ah=42/43h
; ---------------------------------------------------------------
; IN:     bx - action: 0=read, 1=write
;         cx - number of sectors to read
;         si - qs_diskinfo
;    edx:eax - first sector
; OUT:    CF - disk error, registers destroyd
;
int13_rwext     label   near
@@SectorBaseHi  = dword ptr [bp- 4]                             ;
@@SectorBaseLo  = dword ptr [bp- 8]                             ;
@@SectorCount   = word  ptr [bp-10]                             ;
@@Readed        = word  ptr [bp-12]                             ;
@@RetryCount    = word  ptr [bp-14]                             ;
@@Action        = word  ptr [bp-16]                             ;
@@WriteSeg      = word  ptr [bp-18]                             ;
                push    eax                                     ; @@Readed + @@RetryCount
                push    bx                                      ; @@Action
if 0 ;def INITDEBUG
                movzx   di,[si].qd_biosdisk                     ;
                dbg16print <"int13 %d %x %lx%lx %d",10>,<cx,eax,edx,di,bx>
endif
                movzx   ebx,cx                                  ; zero-extend sector count
                push    _DiskBufRM_Seg                          ;
; Advanced Format aligning
                test    _useAFio,0FFh                           ;
                jz      @@i13rw_mainloop                        ;
                lea     edi,[eax + AF_ALIGN_SECT - 1]           ;
                and     edi,not (AF_ALIGN_SECT - 1)             ;
                sub     edi,eax                                 ;
                jz      @@i13rw_mainloop                        ; split op for AF only if
                lea     ecx,[edi + AF_ALIGN_SECT - 1]           ; another part is full-size
                cmp     bx,cx                                   ; with 8 sectors
                jbe     @@i13rw_mainloop                        ;
                sub     bx,di                                   ; read/write up to 4k
                push    bx                                      ; boundary first
                mov     bx,di                                   ;
                ;dbg16print <"int13ext: align %d",10>,<bx>       ;
                jmp     @@i13rw_read                            ;
;----------------------------------------------------------------
@@i13rw_mainloop:
                cmp     bx,SECT_PER_READ                        ; no more 64k in one read
                jbe     @@i13rw_less64k                         ;
                sub     bx,SECT_PER_READ                        ;
                push    bx                                      ; save sector counter
                mov     bx,SECT_PER_READ                        ;
                jmp     @@i13rw_read                            ;
@@i13rw_less64k:
                push    0                                       ; < 64ª - no more to read
@@i13rw_read:
                mov     @@Readed,bx                             ; update counters
                sub     @@SectorCount,bx                        ;
                xor     edi,edi                                 ;
                mov     @@RetryCount,di                         ;
                add     @@SectorBaseLo,ebx                      ;
                adc     @@SectorBaseHi,edi                      ;
; - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
@@i13rw_loop:                                                   ;
                push    edx                                     ;
                push    eax                                     ;
                mov     di,offset ext_packet                    ;
                mov     dword ptr [di].ER_StartSect+4,edx       ; start sector
                mov     dword ptr [di].ER_StartSect,eax         ;
                xor     eax,eax                                 ;
                mov     [di].ER_PktSize,10h                     ; size = 10h
                mov     [di].ER_Reserved,al                     ; zero
                mov     [di].ER_SectorCount,bx                  ; sector count
                mov     word ptr [di].ER_BufferAddr,ax          ; buffer adrress
                mov     ax,@@WriteSeg                           ;
                mov     word ptr [di].ER_BufferAddr+2,ax        ;
                mov     ax,@@Action                             ;
                xchg    al,ah                                   ;
                add     ah,42h                                  ;
                mov     dl,[si].qd_biosdisk                     ;
                push    esi                                     ;
                mov     si,di                                   ;
                ;dbg16print <"int13ext: %x %lx %x %x:%x",10>,<bx,di,cx,edx,ax>
                push    bx                                      ;
                call    int13                                   ; make it!
                pop     bx                                      ;
                pop     esi                                     ;
                pop     eax                                     ;
                pop     edx                                     ;

                jc      @@i13rw_error                           ; error?
                pop     bx                                      ;
                or      bx,bx                                   ; # to read
                jz      @@i13rw_end                             ;
                movzx   edi,@@Readed                            ;
                add     eax,edi                                 ; update sector and
                adc     edx,0                                   ; segment for next read
                mov     cl,[si].qd_sectorshift                  ;
                sub     cl,PARASHIFT                            ;
                shl     di,cl                                   ;
                add     @@WriteSeg,di                           ;
                jmp     @@i13rw_mainloop                        ;
; - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
@@i13rw_error:                                                  ;
                inc     @@RetryCount                            ; next retry
                cmp     @@RetryCount,MAX_INT13_RETRY            ;
                jle     @@i13rw_loop                            ;
                pop     bx                                      ; bios error code in ax
                mov     bx,ext_packet.ER_SectorCount            ;
                xchg    ah,al                                   ; error code ah->ax
                cbw                                             ;
ifdef INITDEBUG
                dbg16print <"int13ext: %d err=%x %d sec from %lx",10>,<edx,bx,ax,@@Action>
                save_dword <"int13err">,<eax>
endif
                stc                                             ; signal error
@@i13rw_end:
                mov     sp,bp                                   ;
                pop     bp                                      ;
                ret                                             ;
sector_io       endp

int13regs       struc
int13_bp        dw      ?                                       ; stack frame for int 13h
int13_ax        dw      ?                                       ; registers
int13_bx        dw      ?                                       ;
int13_cx        dw      ?                                       ;
int13_dx        dw      ?                                       ;
int13regs       ends

;
; process int 13h call
; ---------------------------------------------------------------
; IN:  registers for int 13h
;      ah value supported: 0, 2, 3
; OUT: CF = 1 - failed
;      ah = error code, save other regs
;
int13_action    proc    near
                push    ax                                      ;
                push    dx                                      ; save registers to prevent
                push    cx                                      ; damage from bios
                push    bx                                      ;
                call    int13                                   ;
                pop     bx                                      ;
                pop     cx                                      ;
                pop     dx                                      ;
                setc    al                                      ;
                jnc     @@i13d_ok                               ; no error?
                cmp     ah,09h                                  ; error is a DMA violation?
                jz      @@i13d_dma                              ; yes, fix it
@@i13d_ok:
                add     sp,2                                    ; skip ax
                rcr     al,1                                    ; restore CF
                ret                                             ;
; DMA violation
; ---------------------------------------------------------------
@@i13d_dma:                                                     ;
                pop     ax                                      ; pop caller ax
                push    dx                                      ;
                push    cx                                      ; save regs to stack
                push    bx                                      ;
                push    ax                                      ; al - numver of sectors
                push    bp                                      ;
                mov     bp,sp                                   ;
                mov     dx,es                                   ; low 16 bit of physical address
                shl     dx,PARASHIFT                            ;
; number of sectors we can transfer before DMA boundary
                mov     cl,[si].qd_sectorshift                  ;
                shr     dx,cl                                   ; / sector size
                sub     cl,16                                   ;
                neg     cl                                      ;
                mov     ah,1                                    ;
                shl     ah,cl                                   ; # of sectors in 64k
                mov     cx,[bp].int13_cx                        ;
                sub     ah,dh                                   ; # of sectors left in 64k!
                cmp     ah,al                                   ;
                jl      @@i13d_splitread                        ;
                call    @@i13d_rwdma                            ; fit in sinlge call?
                jmp     @@i13d_exit                             ;
; we can't fit request into the entire block
@@i13d_splitread:
                push    ax                                      ;
                xchg    al,ah                                   ; al - sectors left in 64k
                call    @@i13d_rwdma                            ;
                pop     ax                                      ;
                jc      @@i13d_exit                             ;
                sub     byte ptr [bp].int13_ax,ah               ; decrement by read count
                add     cl,ah                                   ;
                shl     ah,1                                    ; update sector # & buffer addr
                add     bh,ah                                   ;

                mov     ax,[bp].int13_ax                        ; yes?
                call    @@i13d_rwdma                            ; read last part and exit
                jmp     @@i13d_exit                             ;
@@i13d_exit:
                mov     sp,bp                                   ; ax & c - result
                pop     bp                                      ;
                pop     bx                                      ; discard caller's AX, use error code instead
                pop     bx                                      ;
                pop     cx                                      ;
                pop     dx                                      ;
                ret                                             ;
@@i13d_rwdma:
                or      al,al                                   ; read sector from disk
                jz      @@i13d_rdret                            ; number of sectors to read
                mov     dx,[bp].int13_dx                        ;
                mov     ah,byte ptr [bp].int13_ax+1             ; command
                call    int13                                   ;
@@i13d_rdret:                                                   ;
                ret                                             ;
int13_action    endp                                            ;

TEXT16          ends
                end
