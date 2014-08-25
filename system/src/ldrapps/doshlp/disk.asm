;
; QSINIT
; raw disk info & read/write
; ---------------------------------------------------------------
; * must be placed in discardable part of doshlp (for readbuf_ptr usage)
; * was shared with QSINIT until rev 268
;
                include inc/qstypes.inc                         ;
                include inc/segdef.inc                          ; segments
                include inc/basemac.inc                         ;
                include inc/ioint13.inc                         ;
                include inc/debug.inc                           ;
                include inc/qsconst.inc                         ;

; this struct can`t be changed
DriveParams     struc                                           ; ancient drive parameters
DPCylcount      dw      ?                                       ;
DPSPT           dw      ?                                       ;
DPHeads         dw      ?                                       ;
DPPrecomp       dw      ?                                       ;
DPFlags         dw      ?                                       ;
DriveParams     ends

NUMBER_DISKS     =  4                                           ; 2 floppies, 2 hard disks
MAX_INT13_RETRY  = 10                                           ; maximum number of read retries
AF_SECTORS      equ     AF_ALIGN SHR SECTSHIFT                  ;
SECT_PER_READ    = 120                                          ; number of sectors per one r/w op

                public  _driveparams                            ;
                public  _edparmtable                            ;
                public  _eddparmtable                           ;
                public  _i13ext_table                           ;
                public  _BootBPB                                ;
                public  _useAFio                                ;

_BSS            segment                                         ;
                extrn   readbuf_ptr:word                        ; offset to int13 read buffer

                align   4                                       ;
_driveparams    label   byte                                    ;
REPT    NUMBER_DISKS                                            ;
                DriveParams <>                                  ; 4 common disks
ENDM                                                            ; parameters

_BSS            ends                                            ; data must be resident
OEMHLP_DATA     segment                                         ; in doshlp

_edparmtable    label   byte                                    ;
REPT    INT13E_TABLESIZE                                        ; extended parameters
                EDParmTable <>                                  ;
ENDM                                                            ;
_eddparmtable   label   byte                                    ;
REPT    INT13E_TABLESIZE                                        ;
                EDDParmTable <>                                 ;
ENDM                                                            ;

OEMHLP_DATA     ends                                            ;
_BSS            segment                                         ;

ext_packet      ExtReadPacket <>                                ;
                align   4                                       ;
_BootBPB        Disk_BPB <>                                     ; BPB
lasterror_i13   db      ?                                       ;
_BSS            ends                                            ;


cylinders_1_2M  equ     80                                      ; cylinders - 1.2M drive
sectors_1_2M    equ     15                                      ; sectors per track - 1.2M drive
heads_1_2M      equ     2                                       ; heads - 1.2M drive

cylinders_360k  equ     40                                      ; cylinders - 360K drive
sectors_360k    equ     9                                       ; sectors per track - 360K drive
heads_360k      equ     2                                       ; heads - 360K drive

CHANGELINE      equ     00000010b
NONREMOVABLE    equ     00000001b

_DATA           segment
_drivenumbers   db      0,1,80h,81h,0FFh                        ; drive list
_i13ext_table   db      INT13E_TABLESIZE dup (0)                ;
_useAFio        db      1                                       ;
_DATA           ends


_TEXT           segment
                assume  cs:DGROUP,ds:DGROUP,es:nothing,ss:nothing

                extrn   _dd_bootdisk:byte                       ;
                extrn   panic_initpm:near                       ;
                extrn   pae_diskio:near                         ;
;
; check disk configuration & save some data for OEMHLP handler
;----------------------------------------------------------------
;
                public int13check
int13check      proc    near

                mov     bx,offset _driveparams                  ; drive number & info tables
                mov     si,offset _drivenumbers                 ;
@@i13c_loop:
                push    bx                                      ; save pointers
                push    si                                      ;
                mov     dl,[si]                                 ; get drive number
                push    dx                                      ;
                push    bx                                      ;
                mov     ah,15h                                  ;
                call    int13                                   ; Read DASD type
                pop     bx                                      ;
                pop     dx                                      ;
                jc      @@i13c_skipdrv                          ; drive is not present
                or      ah,ah                                   ;
                jz      @@i13c_skipdrv                          ;

                cmp     ah,2                                    ; changeline ?
                jne     @@i13c_01                               ;
                or      [bx].DPFlags,CHANGELINE                 ; BSS filled by zero initially
@@i13c_01:
                cmp     dl,80h                                  ; set nonremoveable if a hard disk
                jb      @@i13c_02                               ;
                or      [bx].DPFlags,NONREMOVABLE               ;
@@i13c_02:
                push    dx                                      ; get the disk drive parameters
                push    bx                                      ;
                mov     ah,08h                                  ;
                call    int13                                   ;
                pop     bx                                      ;
                jc      @@i13c_noinfo                           ; failed?
                or      cl,cl                                   ;
                jne     @@i13c_filltab1                         ;
@@i13c_noinfo:
                test    [bx].DPFlags,CHANGELINE                 ; fix ancient bios & cmos bugs:
                jz      @@i13c_03                               ; all zeroes returned
                mov     ch,cylinders_1_2M-1                     ;
                mov     cl,sectors_1_2M                         ;
                mov     dh,heads_1_2M-1                         ;
                jmp     @@i13c_filltab1                         ;
@@i13c_03:
                mov     ch,cylinders_360K-1                     ;
                mov     cl,sectors_360K                         ;
                mov     dh,heads_360K-1                         ;
@@i13c_filltab1:
; put parameters into configuration data table
                movzx   ax,dh                                   ; heads
                ;dbg16print <"disk %x heads %x",10>,<ax,dx>
                inc     ax                                      ;
                mov     [bx].DPHeads,ax                         ;
                mov     al,cl                                   ; sectors
                and     al,3Fh                                  ;
                cbw                                             ;
                mov     [bx].DPSPT,ax                           ;
                shr     cl,6                                    ; cylinders
                xchg    cl,ch                                   ;
                inc     cx                                      ;
                mov     [bx].DPCylcount,cx                      ;
                pop     dx                                      ; restore drive

                cmp     dl,80h                                  ; hard disk?
                jc      @@i13c_skipdrv                          ;
                ja      @@i13c_hdd1                             ;
                mov     si,FDTAB_0                              ; who really need this?
                jmp     @@i13c_biosarea                         ;
@@i13c_hdd1:
                mov     si,FDTAB_1                              ;
@@i13c_biosarea:
                push    0                                       ;
                pop     es                                      ;
                les     si,dword ptr es:[si]                    ; read int 41h/46h

                mov     ax,es:[si+FDParmTable.FDWritePComp]     ; read precompensation factor
                mov     [bx+DriveParams.DPPrecomp],ax           ; from here
@@i13c_skipdrv: ; Restore and update table pointers
                pop     si                                      ;
                pop     bx                                      ;
                inc     si                                      ;
                cmp     byte ptr [si],0FFh                      ; check for the end of table
                jz      @@i13c_oldready                         ;
                add     bx,size DriveParams                     ; else continue
                jmp     Near Ptr @@i13c_loop                    ;
@@i13c_oldready:
                push    ds                                      ; restore es
                pop     es                                      ;
                mov     dl, 80h                                 ;
                mov     si, offset _edparmtable                 ;
                mov     di, offset _eddparmtable                ;
@@i13c_loop_ei:
                push    dx
                mov     ah,15h                                  ;
                call    int13                                   ; Read DASD type
                jc      @@i13c_noext                            ;
                ;dbg16print <"int13 15h: %x",10>,<ax>
                cmp     ah, 3                                   ; hdd?
                jz      @@i13c_hdd2                             ;
                cmp     ah, 2                                   ; and modern fdd type
                jz      @@i13c_hdd2                             ; (thanks to Gigabyte!)
                jmp     @@i13c_noext                            ;
@@i13c_hdd2:
                pop     dx                                      ;
                push    di                                      ;
                push    si                                      ;
                push    dx                                      ;
                mov     ah, 41h                                 ; check for int 13 ext.
                mov     bx, 55AAh                               ;
                call    int13                                   ;
                ;dbg16print <"int13 41h: %x %x %x",10>,<cx,bx,ax>
                jc      @@i13c_testread                         ; no support?
                cmp     ah, 21h                                 ; check version
                jb      @@i13c_testread                         ;
                cmp     bx, 0AA55h                              ; check signature
                jnz     @@i13c_testread                         ;
                and     cl, 1                                   ; check r/w support
                jz      @@i13c_testread                         ;
                pop     dx                                      ;
@@i13c_nonboot:
                mov     bl, dl                                  ;
                and     bx, 07Fh                                ; zero high bits
                or      cl, 2                                   ; drive present
                mov     _i13ext_table[bx], cl                   ; save flag
                mov     word ptr [si].ED_BufferSize, size EDParmTable
                mov     ah, 48h                                 ; get extended drive parameters
                push    dx                                      ;
                call    int13                                   ;
                jc      @@i13c_testread                         ;
                cmp     word ptr [si], size EDParmTable         ; too old table
                jb      @@i13c_testread                         ;
                xor     eax, eax                                ;
                cmp     [si].ED_EDDTable, eax                   ; compare EDD table address to
                jz      @@i13c_testread                         ; 0:0 and FFFF:FFFF
                dec     eax                                     ;
                cmp     [si].ED_EDDTable, eax                   ;
                jz      @@i13c_testread                         ;
                mov     cx, size EDDParmTable shr 2             ;
                push    ds                                      ;
                lds     si, dword Ptr [si].ED_EDDTable          ; copy EDD table
            rep movsd                                           ;
                pop     ds                                      ;
@@i13c_testread:                                                ; restore:
                pop     dx                                      ; drive
                pop     si                                      ; drive parameters ptr
                pop     di                                      ; edd info ptr

                xor     eax,eax                                 ; check fields
                cmp     [si].ED_SectorSize,ax                   ; and save default
                jnz     @@i13c_sszok                            ; if empty
                mov     [si].ED_SectorSize,512                  ;
@@i13c_sszok:
                cmp     [si].ED_Cylinders,eax                   ;
                jnz     @@i13c_chsok                            ;
                mov     [si].ED_Cylinders,16383                 ;
                mov     [si].ED_Heads,16                        ;
                mov     [si].ED_SectOnTrack,63                  ;
                dbg16print <"empty EDD for disk %x",10>,<dx>
@@i13c_chsok:
; read sector at the end of disk to test actual int13ext functionality,
; but for boot disk only (some bioses can hang on connected USB drives)
                push    dx                                      ;
                mov     ecx, dword ptr [si+4].ED_Sectors        ; number of sectors - hi dword
                jecxz   @@i13c_10                               ;
                mov     ecx, 0FFFFFFFFh                         ;
                jmp     @@i13c_11                               ;
@@i13c_10:
                mov     ecx, dword ptr [si].ED_Sectors          ; number of sectors - lo dword
                or      ecx,ecx                                 ; no disk size?
                jz      @@i13c_testend                          ; let the c code thinking about...
@@i13c_11:
                mov     bl, dl                                  ; flag i13x
                and     bx, 07Fh                                ; possibility
                or      _i13ext_table[bx], 4                    ;
@@i13c_gt8gb:
                cmp     dl, _dd_bootdisk                        ;
                jnz     @@i13c_testend                          ; boot disk?
                dec     ecx                                     ; skip test then
                jecxz   @@i13c_testend                          ;
                dec     ecx                                     ;
                jecxz   @@i13c_testend                          ;
                push    si                                      ;
                mov     si, offset ext_packet                   ; fill packer
                and     cl, not 7
                mov     dword ptr [si].ER_StartSect, ecx        ; sector number
                xor     ecx, ecx                                ;
                mov     dword ptr [si+4].ER_StartSect, ecx      ; hi part - 0
                mov     [si].ER_PktSize, 10h                    ;
                mov     [si].ER_Reserved, cl                    ;
                mov     [si].ER_SectorCount, 1                  ;
                mov     ax, readbuf_ptr                         ;
                mov     word ptr [si].ER_BufferAddr, ax         ; buffer addr
                mov     word ptr [si+2].ER_BufferAddr, ds       ;
                ;dbg16print <"int13ext: %x %lx %x:%x",10>,<bx,di,edx,ax>
                mov     ah, 42h                                 ; read it
                call    int13                                   ;
                pop     si                                      ;
                jnc     @@i13c_testend                          ; error?
@@i13c_noext:
                xor     eax, eax                                ; clear extended info
                mov     cx, size EDDParmTable shr 2             ;
                push    di                                      ;
            rep stosd                                           ;
                mov     di, si                                  ;
                mov     cx, size EDParmTable shr 1              ;
            rep stosw                                           ;
                pop     di                                      ;
@@i13c_testend:
                pop     dx                                      ;
                inc     dl                                      ;
                add     di, size EDDParmTable                   ;
                add     si, size EDParmTable                    ;
                cmp     dl, 80h+INT13E_TABLESIZE                ; loop all drives
                jc      @@i13c_loop_ei                          ;
                dbg16print <"int13check done",10>               ;
                ret                                             ;
int13check      endp

int13           proc    near                                    ; save registers
                SaveReg <si,di,ds,es>                           ; bios can destroy ANYTHING ;)
                int     13h                                     ;
                RestReg <es,ds,di,si>                           ;
                sti                                             ;
                cld                                             ;
                ret                                             ;
int13           endp                                            ;

;
; read sectors from boot partition/disk
;----------------------------------------------------------------
; IN: dx:ax - starting sector (partition relative)
;        cx - number of sectors to read
;        bx - number of bytes to be read from last sector (not included to cx!)
;     es:di - user buffer
; OUT:   CF - disk error
                public  int13read
int13read       proc    near
                push    es                                      ;
                push    edx                                     ; save high part of edx
                pusha                                           ; (does anyone need it?)
                cmp     bx,_BootBPB.BPB_BytesPerSec             ;
                jnz     @@i13r_partial                          ;
                xor     bl,bl                                   ;
                call    sector_io                               ; single read op
@@i13r_exit:
                popa                                            ;
                pop     edx                                     ;
                pop     es                                      ;
                ret                                             ;
@@i13r_partial:
                dec     cx                                      ;
                push    bx                                      ; save buffer
                push    di                                      ;
                push    es                                      ;
                jcxz    @@i13r_01                               ; one sector?

                push    cx                                      ;
                shl     cx, SECTSHIFT-PARASHIFT                 ; calculate last sector
                mov     si, sp                                  ;
                add     ss:[si+2], cx                           ; modify segment address
                pop     cx                                      ;
                xor     bl,bl                                   ;
                call    sector_io                               ; read the sectors
                jc      @@i13r_fail                             ;
@@i13r_01:                                                      ;
                mov     di,readbuf_ptr                          ; point to temp buffer
                push    cs                                      ;
                pop     es                                      ;
                mov     cx,1                                    ; read a single sector
                xor     bl,bl                                   ;
                call    sector_io                               ;
                jc      @@i13r_fail                             ; error?
                mov     si,readbuf_ptr                          ; reset tmp buffer to scratch buff
                pop     es                                      ; restore buffer address
                pop     di                                      ;
                pop     cx                                      ; restore byte count
            rep movsb                                           ; last sector
                jmp     @@i13r_exit                             ;
@@i13r_fail:
                add     sp,6                                    ; pop registers
                stc                                             ;
                jmp     @@i13r_exit                             ;
int13read       endp                                            ;

;
; boot disk I/O handler
;----------------------------------------------------------------
; IN:     bl - action: 0=read, 1=write
;         cx - number of sectors to read
;      dx:ax - first sector
;      es:di - buffer
; OUT:    CF - disk error
;      dx:ax - next sector to read
;      all registers destroyd
sector_io       proc    near
                push    dx                                      ; save sector number in 32 bit
                push    ax                                      ; word order
                mov     bh,2                                    ; 2 checks: start & end sector
@@rsec_check:
                cmp     _BootBPB.BPB_TotalSec,0                 ;
                jnz     @@rsec_01                               ; we have small total sectors
                cmp     dx,word ptr _BootBPB.BPB_TotalSecBig+2  ;
                ja      @@rsec_range                            ; beyond range of disk
                jb      @@rsec_02                               ;
                cmp     ax,word ptr _BootBPB.BPB_TotalSecBig    ;
                ja      @@rsec_range                            ; beyond range of disk
                jmp     @@rsec_02                               ;
@@rsec_01:
                or      dx,dx                                   ;
                jnz     @@rsec_range                            ; beyond range of disk
                cmp     ax,_BootBPB.BPB_TotalSec                ; compare with max sector
                jbe     @@rsec_02                               ;
@@rsec_range:
                dbg16print <"int13: out of range:%x%x",10>,<ax,dx>
                pop     ax                                      ; restore sector number
                pop     dx                                      ;
                stc                                             ; error
                ret                                             ;
@@rsec_02:
                add     ax,cx                                   ; check last sector in next
                adc     dx,0                                    ; iteration
                dec     bh                                      ;
                jnz     @@rsec_check                            ;
;----------------------------------------------------------------
@@rsec_read:
                mov     ax,bx                                   ; r/w op. code
                mov     bx,di                                   ; read buffer in es:bx
                movzx   si,_dd_bootdisk                         ;
                btr     si,7                                    ; check for 80h
                jnc     @@rsec_oldway                           ;
                test    _i13ext_table[si],1                     ; ext. on?
                jz      @@rsec_paeio                            ; no, use CHS
                pop     edx                                     ; start sector
                call    int13_rwext                             ; r/w via 42/43h and return
                ret                                             ; (dx:ax updated in call)
@@rsec_paeio:
                test    _i13ext_table[si],8                     ; the same call
                jz      @@rsec_oldway                           ; method as for
                pop     edx                                     ; int13_rwext
                add     edx, _BootBPB.BPB_HiddenSec             ;
                call    pae_diskio                              ;
                ret                                             ;
;----------------------------------------------------------------
@@rsec_oldway:                                                  ;

@@SectorBase    = dword ptr [bp+4]                              ; start sector
@@ActionCode    = byte ptr [bp+2]                               ; r/w op
@@SectorCount   = word ptr [bp-2]                               ; sector count
@@CurrentSector = byte ptr [bp-3]                               ;
@@CurrentHead   = byte ptr [bp-4]                               ;
                add     al,2                                    ;
                push    ax                                      ; int 13 ah value
                push    bp                                      ;
                mov     bp,sp                                   ;
                push    cx                                      ; sector count
                push    cx                                      ; reserve place for variables
; calculate head/sector
@@rsec_loop:
                mov     eax,@@SectorBase                        ; sector number
                ;dbg16print <"int13: s:%lx",10>,<eax>
                add     eax,_BootBPB.BPB_HiddenSec              ;
                xor     edx,edx                                 ;
                movzx   ecx,_BootBPB.BPB_SecPerTrack            ;
                div     ecx                                     ;
                mov     @@CurrentSector,dl                      ; start sector.
                xor     edx,edx                                 ;
                movzx   ecx,_BootBPB.BPB_Heads                  ;
                div     ecx                                     ;
                mov     @@CurrentHead,dl                        ; head
                mov     si,ax                                   ; cylinder
                shr     eax,16                                  ; number of cylinders > 64k
                mov     ax,7                                    ;
                jnz     panic_initpm                            ; panic
;----------------------------------------------------------------
                mov     ax,_BootBPB.BPB_SecPerTrack             ;
                sub     al,@@CurrentSector                      ; read sectors up to the end
                cbw                                             ; of track
                cmp     ax,@@SectorCount                        ;
                jb      @@rsec_03                               ;
                mov     ax,@@SectorCount                        ;
@@rsec_03:
; perform read
                push    ax                                      ;
                mov     ah,@@ActionCode                         ; int 13 read operation
                mov     dx,si                                   ; dx - cylinder
                mov     cl,6                                    ;
                shl     dh,cl                                   ;
                or      dh,@@CurrentSector                      ;
                mov     cx,dx                                   ;
                xchg    ch,cl                                   ;
                inc     cl                                      ; sector is 1 based.
                mov     dl,_dd_bootdisk                         ;
                mov     dh,@@CurrentHead                        ;

                mov     di,MAX_INT13_RETRY                      ; retry count
@@rsec_retry:
                push    ax                                      ;
                push    di                                      ;
                call    int13_action                            ; try to read
                jnc     @@rsec_ok                               ;
                mov     lasterror_i13,ah                        ;
                pop     di                                      ; reset controller
                cmp     di,MAX_INT13_RETRY                      ; only once here!
                jnz     @@rsec_onlyone                          ;
                push    di                                      ;
                xor     ah,ah                                   ; if no,
                call    int13_action                            ; reset the drive
                pop     di                                      ;
@@rsec_onlyone:
                pop     ax                                      ;
                dec     di                                      ; next try?
                jnz     @@rsec_retry                            ;
                mov     al,lasterror_i13                        ; last error
                cbw                                             ;
                movzx   cx,@@CurrentHead                        ; print to log
                movzx   di,@@CurrentSector                      ;
                dbg16print <"int13: t:%d h:%d s:%d error %x",10>,<ax,di,cx,si>
                stc                                             ;
                jmp     @@rsec_04                               ; exit
@@rsec_ok:
                pop     di                                      ;
                pop     ax                                      ;
@@rsec_04:
                pop     ax                                      ;
                jc      @@rsec_exit                             ; error?
                movzx   eax,al                                  ;
                sub     @@SectorCount,ax                        ; update sector count
                add     @@SectorBase,eax                        ;
                shl     ax,SECTSHIFT-PARASHIFT                  ; convert sectors to paragraphs
                mov     cx,es                                   ; and update destination address.
                add     cx,ax                                   ;
                mov     es,cx                                   ;
                cmp     @@SectorCount,0                         ; done?
                jnz     @@rsec_loop                             ;
                clc                                             ;
@@rsec_exit:
                mov     sp,bp                                   ;
                pop     bp                                      ;
                pop     ax                                      ;
                pop     ax                                      ; return updated sector pos
                pop     dx                                      ; (@@SectorBase) to dx:ax
                ret
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
;      ah = error code
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
                call    @@i13d_checkaddr                        ; check addr low 16 bit
                add     dx,511                                  ;
                jc      @@i13d_b64read                          ; CF - sector on 64k boundary
; number of sectors we can transfer before DMA boundary
                shr     dh,1                                    ; divide by 512
                mov     ah,128                                  ; # of sectors in 64k
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
                call    @@i13d_checkaddr                        ; zf - wrap on boundary
                jnz     @@i13d_b64read                          ;
                mov     ax,[bp].int13_ax                        ; yes?
                call    @@i13d_rwdma                            ; read last part and exit
                jmp     @@i13d_exit                             ;
; request wrapped on the 64k boundary
@@i13d_b64read:
                push    bx                                      ;
;                cmp     byte ptr [bp].int13_ax+1,3              ; write action?
                push    bx                                      ;
                push    es                                      ;
                push    cs                                      ;
                pop     es                                      ;
;                jz      @@i13d_b64write                         ;
                mov     bx,readbuf_ptr                          ; we are reading sector to
                mov     al,1                                    ; our buffer and copy to user space
                call    @@i13d_rwdma                            ; read one sector
                pop     es                                      ;
                pop     bx                                      ;
                jc      @@i13d_exit                             ; error?
                push    si                                      ;
                mov     di,bx                                   ; es:di - buffer address
                mov     si,readbuf_ptr                          ; ds:si - sector data
                call    @@i13d_sectcopy                         ;
                pop     si                                      ;
; write used in qsinit only, but qsinit disk buffer is always page-aligned,
; so remove this code
;                jmp     @@i13d_64brest                          ;
;@@i13d_b64write:
;                mov     di,readbuf_ptr                          ; copying sector to our`s
;                pop     ds                                      ; buffer in 9x00:xxxx
;                push    si                                      ;
;                mov     si,bx                                   ; es:di - internal buffer
;                call    @@i13d_sectcopy                         ; ds:si - write data
;                pop     si                                      ;
;                mov     bx,cs:readbuf_ptr                       ;
;                call    @@i13d_rwdma                            ;
;                pop     bx                                      ; restore es:bx
;                push    ds                                      ; and ds
;                pop     es                                      ;
;                push    cs                                      ;
;                pop     ds                                      ;
;                jc      @@i13d_exit                             ; error?
@@i13d_64brest:
; rest of sectors behind 64k boundary
                pop     bx                                      ; org bx value
                add     bh,2                                    ; + 512
                inc     cx                                      ;
                mov     al,byte ptr [bp].int13_ax               ; number of sectors left
                clc                                             ; clear error
                dec     al                                      ;
                jz      @@i13d_exit                             ; all done? Just return
                call    @@i13d_rwdma                            ; else read
@@i13d_exit:
                mov     sp,bp                                   ; ax & c - result
                pop     bp                                      ;
                pop     bx                                      ; discard caller's AX, use error code instead
                pop     bx                                      ;
                pop     cx                                      ;
                pop     dx                                      ;
                ret                                             ;
@@i13d_checkaddr:
                mov     dx,es                                   ; calc low 16 bit of physical address
                shl     dx,PARASHIFT                            ;
                add     dx,bx                                   ; seg + buffer
                ret                                             ;
@@i13d_sectcopy:
                push    cx                                      ; copy one sector
                mov     cx,SECTSIZE/4                           ;
            rep movsd                                           ;
                pop     cx                                      ;
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

;
; read/write by int 13h ah=42/43h
; ---------------------------------------------------------------
; IN:    al - action: 0=read, 1=write
;       edx - start sector
;        cx - sector count
;     es:bx - buffer address
; OUT:  C=1 - failed
;     dx:ax - start sector + count
;
int13_rwext     proc    near
@@SectorCount   = word ptr  [bp+2]                              ;
@@SectorBase    = dword ptr [bp+6]                              ;
@@Readed        = word ptr  [bp-2]                              ;
@@RetryCount    = word ptr  [bp-4]                              ;
@@Action        = word ptr  [bp-6]                              ;
                push    edx                                     ;
                push    ecx                                     ;
                push    bp                                      ;
                mov     bp,sp                                   ;
                push    eax                                     ; @@Readed + @@RetryCount
                cbw                                             ; @@Action
                push    ax                                      ;
                movzx   ecx, cx                                 ; zero-extend sector count
                add     edx, _BootBPB.BPB_HiddenSec             ; adjust with partition's base sector
                mov     di, es                                  ;
; Advanced Format aligning
                test    _useAFio, 0FFh                          ;
                jz      @@i13rw_mainloop                        ;
                lea     eax, [edx + AF_ALIGN_SECT - 1]          ;
                and     eax, not (AF_ALIGN_SECT - 1)            ;
                sub     eax, edx                                ;
                jz      @@i13rw_mainloop                        ; split op for AF only if
                lea     esi, [eax + AF_ALIGN_SECT - 1]          ; another part is full-size
                cmp     cx, si                                  ; with 8 sectors
                jbe     @@i13rw_mainloop                        ;
                sub     cx, ax                                  ; read/write up to 4k
                push    cx                                      ; boundary first
                mov     cx, ax                                  ;
                ;dbg16print <"int13ext: align %d",10>,<cx>       ;
                jmp     @@i13rw_read                            ;
;----------------------------------------------------------------
@@i13rw_mainloop:
                cmp     cx, SECT_PER_READ                       ; no more 64k in one read
                jbe     @@i13rw_less64k                         ;
                sub     cx, SECT_PER_READ                       ;
                push    cx                                      ; save sector counter
                mov     cx, SECT_PER_READ                       ;
                jmp     @@i13rw_read                            ;
@@i13rw_less64k:
                push    0                                       ; < 64ª - no more to read
@@i13rw_read:
                mov     @@Readed, cx                            ; update counters
                sub     @@SectorCount, cx                       ;
                add     @@SectorBase, ecx                       ;
                mov     @@RetryCount, 0                         ;
; - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
@@i13rw_loop:                                                   ;
                push    edx                                     ;
                xor     eax,eax                                 ;
                mov     si, offset ext_packet                   ;
                mov     [si].ER_PktSize, 10h                    ; size = 10h
                mov     [si].ER_Reserved, al                    ; zero
                mov     [si].ER_SectorCount, cx                 ; sector count
                mov     word ptr [si].ER_BufferAddr, bx         ; buffer adrress
                mov     word ptr [si].ER_BufferAddr+2, di       ;
                mov     dword ptr [si].ER_StartSect, edx        ; start sector
                mov     dword ptr [si].ER_StartSect+4, eax      ;
                mov     ax,@@Action                             ;
                xchg    al,ah                                   ;
                add     ah,42h                                  ;
                ;dbg16print <"int13ext: %x %lx %x %x:%x",10>,<bx,di,cx,edx,ax>
                mov     dl,_dd_bootdisk                         ;
                push    bx                                      ; save registers
                push    cx                                      ; to prevent bios damage
                call    int13                                   ; make it!
                pop     cx                                      ;
                pop     bx                                      ;
                pop     edx                                     ;

                jc      @@i13rw_error                           ; error?
                pop     cx                                      ;
                jcxz    @@i13rw_end                             ;
                movzx   eax, @@Readed                           ;
                add     edx, eax                                ; update segment and
                shl     ax, SECTSHIFT - PARASHIFT               ; sector for next read
                add     di, ax                                  ;
                jmp     near ptr @@i13rw_mainloop               ;
; - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
@@i13rw_error:                                                  ;
                inc     @@RetryCount                            ; next retry
                cmp     @@RetryCount, MAX_INT13_RETRY           ;
                jle     near ptr @@i13rw_loop                   ;
                pop     cx                                      ; bios error code in ax
                mov     bx, ext_packet.ER_SectorCount           ;
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
                pop     ecx                                     ;
                pop     ax                                      ; modified start sector
                pop     dx                                      ; value
                ret                                             ;
int13_rwext     endp

;
; DHReadSectors doshlp call
; ---------------------------------------------------------------
; damage OF flag ;)
                public  DHReadSectors                           ;
DHReadSectors   proc    far                                     ;
                push    ds                                      ;
                push    ax                                      ;
                pushf                                           ;
                push    cs                                      ;
                pop     ds                                      ; set dgroup
                ;dbg16print <"DHReadSectors es:di=%x:%x, dx:ax=%x:%x cx=%x bx=%x",10>,<bx,cx,ax,dx,di,es>
                call    int13read                               ; read
                setc    al                                      ;
                popf                                            ;
                rcr     al,1                                    ; save all flags except OF ;)
                pop     ax                                      ;
                pop     ds                                      ;
                ret                                             ;
DHReadSectors   endp                                            ;

_TEXT           ends
                end

