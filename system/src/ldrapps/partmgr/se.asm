;
; QSINIT
; MBR / boot sector code
;
; 1. partition table code for MBR disk: _psect
;    * move self to 0:0600
;    * save I13X flag to 3000:0, but only for BM active partition (this will
;      make problems if we`re booting FAT with IBM os2boot/os2ldr. Not
;      _bsect16, nor qsinit os2ldr does not require this flag).
;    * if no active partitions - Boot Manager will be used if present.
;    * no fixups in code
;
; 2. boot sector: _bsect16
;    * can load and run IBM OS2LDR directly, without OS2BOOT, Windows XP NTLDR
;      can be loaded too ;)
;    * supports both fdd & hdd boot
;    * load FAT12/16 by FAT chains, not contigous files only.
;    * load file to 2000:0, use 0:7E00 for directory and 1000:0 for FAT cache
;    * no fixups in code, but assume 0:7C00 as location
;    * uses disk number from DL
;    * checks file attribute for Vol & Dir
;
; 3. boot sector: _bsect32
;    * load FAT32 by FAT chains, not contigous files only.
;    * load file to 2000:0, use 1000:0 for directory and FAT cache
;    * no fixups in code, but assume 0:7C00 as location
;    * uses disk number from DL
;    * checks file attribute for Vol & Dir
;    * checks "active FAT copy" bits as windows boot sector do.
;    * can read partition on 2Tb border ;) (i.e. only boot sector must have
;      32-bit number)
;
; 3. boot sector: _bsectexf
;    * load exFAT by FAT chains, not contigous files only
;    * load file to 2000:0, uses 1000:0 for directory and FAT cache
;    * no fixups in code, but assume 0:7C00 as location
;    * uses disk number from DL
;    * able to boot from partition beyond 2Tb border
;    * boot file size limited to 448k-1 (but no checks via int 12h!)
;    * ignores "active FAT copy" bit, because FasFs can`t work in two-FAT mode
;
; 4. boot sector: _bsectdb
;    * print DL from BIOS, disk number and "hidden sectors" from BPB and i13x
;      presence flag
;    * sector code is short and can be used in MBR too
;
; 5. partition table code for GPT disk: _gptsect
;    * move self to 0:0600
;    * panic if no i13x (CHS+GPT makes me ill, sorry)
;    * detects sector size via int 13h ah=48h call (to calc number of GPT
;      records per sector)
;    * assume 32bit sector number for 1st GPT copy
;    * get FIRST found UEFI in MBR as GPT main header
;    * searching in GPT records for "BIOS Bootable" attribute bit, gets first
;    * checks bytes per sector BPB field and if it 512/1024/2048/4096 - assume
;      BPB in boot sector and update "Hidden Sectors" value in it.
;    * no fixups in code
;
; tools\bootset.exe compilation assumes FAT sector at 200h, FAT32 at 400h
; and 2 exFAT sectors at 600h when makes headers with boot sector code.
;
                include inc/qstypes.inc
                include inc/parttab.inc
                include inc/filetab.inc

                NUM_RETRIES  = 4
                LOC_7C00h    = 7C00h
                LOC_7E00h    = 7E00h
                LOC_0600h    = 0600h
                I13X_SEG     = 3000h
                BOOT_OEM_LEN = 11                               ; jmp + label
                BOOT_F32INFO = BOOT_OEM_LEN + size Common_BPB
                BOOT_SEG     = 2000h
                FAT_LOAD_SEG = 1000h
                MIN_FAT16    = 4086

PARTMGR_CODE    segment byte public USE16 'CODE'
                assume  cs:PARTMGR_CODE,ds:PARTMGR_CODE,es:nothing,ss:nothing

                public  psect
psect:
                and     ax,5351h                                ; %QS% string,
                and     ax,2420h                                ; but zero ax too :)
                mov     si,LOC_7C00h                            ;
                mov     ss,ax                                   ;
                mov     sp,si                                   ;
                mov     es,ax                                   ;
                mov     ds,ax                                   ;
                cld                                             ;
                mov     di,LOC_0600h                            ;
                mov     cx,100h                                 ;
            rep movsw                                           ;
                JmpF16a 0,LOC_0600h + (@@pt_m_moved - psect)   ;
@@pt_m_moved:
                sti                                             ; walk through 4
                mov     cl,4                                    ; pt entries and
                sub     di,4 * size MBR_Record + 2              ; search for active
@@pt_m_loop:
                cmp     [di].PTE_Type,PTE_0A_BOOTMGR            ; if no active -
                jnz     @@pt_m_notbm                            ; try to boot
                mov     ax,di                                   ; Boot Manager
@@pt_m_notbm:
                cmp     [di].PTE_Active,ch                      ; if exists one
                jl      @@pt_m_active                           ; (as OS/2 mbr
                jnz     @@pt_m_badrec                           ; code do).
                add     di,size MBR_Record                      ;
                loop    @@pt_m_loop                             ;
                xor     di,di                                   ;
                or      di,ax                                   ;
                jnz     @@pt_m_active                           ;
                int     18h                                     ;
@@pt_m_active:
                mov     byte ptr [di-3],NUM_RETRIES             ;
                mov     byte ptr [di-4],0                       ;
                mov     [di],dl                                 ;
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                call    @@pt_m_i13                              ;
                jc      @@pt_m_errloop                          ;
                cmp     bx,0AA55h                               ;
                jnz     @@pt_m_errloop                          ;
                and     cl,1                                    ;
                mov     [di-4],cl                               ;
@@pt_m_errloop:
                mov     bx,sp                                   ; 7C00 here
                cmp     [di-4],bl                               ; cmp with 0
                jz      @@pt_m_noextread                        ;
                xor     ax,ax                                   ;
                push    ax                                      ;
                push    ax                                      ;
                push    dword ptr [di].PTE_LBAStart             ;
                push    ax                                      ;
                push    bx                                      ; 7C00h
                push    1                                       ;
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     dl,[di]                                 ;
                mov     si,sp                                   ;
                call    @@pt_m_i13                              ;
                mov     sp,LOC_7C00h                            ;
                jmp     @@pt_m_readdone                         ;
@@pt_m_noextread:
                mov     ax,201h                                 ;
                mov     dx,[di]                                 ;
                mov     cx,[di].PTE_CSStart                     ;
                call    @@pt_m_i13                              ;
@@pt_m_readdone:
                jnc     @@pt_m_goboot                           ;
                dec     byte ptr [di-3]                         ;
                jz      @@pt_m_check80h                         ;
                xor     ah,ah                                   ;
                mov     dl,[di]                                 ;
                call    @@pt_m_i13                              ;
                jmp     @@pt_m_errloop                          ;
@@pt_m_check80h:
                mov     dl,80h                                  ;
                cmp     byte ptr [di],dl                        ;
                jz      @@pt_m_errread                          ;
                jmp     @@pt_m_active                           ;
@@pt_m_goboot:
                movzx   dx,[di]                                 ;
                mov     bp,sp                                   ;
                cmp     word ptr [bp+510],0AA55h                ;
                jnz     @@pt_m_missing                          ;
                mov     si,di                                   ; ds:[si] - pt entry
                xor     edi,edi                                 ;
                cmp     [si].PTE_Type,PTE_0A_BOOTMGR            ;
                jz      @@pt_m_bootmgr                          ;
                jmp     @@pt_m_set_I13X                         ;
@@pt_m_bootmgr:
                mov     ax,[di + LOC_0600h + (@@pt_m_sign - psect)] ; use [di] to avoid wasm wrong op type
                mov     [bp+512+offset MBR_Reserved],ax         ; BootMGR check it here!
@@pt_m_set_I13X:
                mov     eax,edi                                 ;
                cmp     [si-4],al                               ; I13X?
                mov     ecx,[si].PTE_LBAStart                   ;
                jz      @@pt_m_no_i13x                          ;
                mov     eax,58333149h                           ;
@@pt_m_no_i13x:
                push    I13X_SEG                                ; this is for IBM boot
                pop     es                                      ; sectors only, but I hope
                stosd                                           ; is not critical for any
                mov     es:[di],ecx                             ; other (2 dd at 3000:0)
                JmpF16a 0,LOC_7C00h                             ;
@@pt_m_i13:
                push    di                                      ;
                push    ds                                      ;
                int     13h                                     ;
                pop     ds                                      ;
                pop     di                                      ;
                ret                                             ;
@@pt_m_errread:
                mov     si,LOC_0600h + (@@pt_m_msg_io - psect)  ;
                jmp     @@pt_m_haltmsg                          ;
@@pt_m_missing:
                mov     si,LOC_0600h + (@@pt_m_msg_os - psect)  ;
                jmp     @@pt_m_haltmsg                          ;
@@pt_m_badrec:
                mov     si,LOC_0600h + (@@pt_m_msg_pt - psect)  ;
@@pt_m_haltmsg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@pt_m_halt                             ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@pt_m_haltmsg                          ;
@@pt_m_halt:
                hlt                                             ;
                jmp     @@pt_m_halt                             ;

@@pt_m_msg_pt:  db      '<error in partition table>',0          ;
@@pt_m_msg_os:  db      '<no operating system>',0               ;
@@pt_m_msg_io:  db      '<error loading operating system>',0    ;
psect_end:

pt_m_reserved   equ     size MBR_Code - (psect_end - psect)     ;
                db      pt_m_reserved dup (0)                   ;

                dd      0                                       ;
@@pt_m_sign:    dw      0                                       ; 0CC33h
                db      4 * size MBR_Record dup (0)             ;
                dw      0AA55h                                  ;

;================================================================
; FAT12/16 boot sector
;================================================================

                public  bsect16
                public  bsect16_name
@@bt_w_clshift  = byte ptr [bp-18]                              ; sect->cluster shift
@@bt_w_fat12    = byte ptr [bp-17]                              ;
@@bt_w_datapos  =          [bp-16]                              ;
@@bt_w_rootpos  = word ptr [bp-12]                              ; 1st root sector
@@bt_w_rootscnt =          [bp-10]                              ; sectors in root
@@bt_w_disknum  = byte ptr [bp- 8]                              ; byte->sect shift
@@bt_w_i13x     = byte ptr [bp- 7]                              ;
@@bt_w_fatpos   = word ptr [bp- 6]                              ;
@@bt_w_curclus  =          [bp- 4]                              ;

@@bt_w_physdisk = byte ptr [bp + BOOT_OEM_LEN + size Common_BPB]
@@bt_w_bootname = [bp + BOOT_OEM_LEN + size Common_BPB + size Extended_BPB + 3]

bsect16:
                jmp     @@bt_w_start                            ;
                nop                                             ; os2dasd get DOS version
                db      'QSBT 5.0'                              ; from OEM field and fail
                dw      200h                                    ; to boot on some variants
                db      1h                                      ;
                dw      1                                       ;
                db      2                                       ;
                dw      0E0h                                    ;
                dw      0B40h                                   ;
                db      0F0h                                    ;
                dw      9                                       ;
                dw      18                                      ;
                dw      2                                       ;
                dd      0                                       ;
                dd      0                                       ;
                db      0h                                      ; 80h
                db      0                                       ;
                db      29h                                     ;
                dd      0                                       ;
                db      'NO LABEL   '                           ;
                db      'FAT     '                              ;
@@bt_w_err1:    db      'No '
bsect16_name:
                db      'QSINIT     ',0                         ;
@@bt_w_err2:    db      'Read error!',0                         ;
@@bt_w_start:
                mov     bp,LOC_7C00h                            ;
                xor     esi,esi                                 ;
                mov     ds,si                                   ;
                mov     ss,si                                   ;
                mov     sp,bp                                   ;
                push    esi                                     ; space for vars
                push    esi                                     ;
                or      si,[bp+BOOT_OEM_LEN].BPB_TotalSec       ;
                jnz     @@bt_w_bigfat                           ;
                mov     esi,[bp+BOOT_OEM_LEN].BPB_TotalSecBig   ; total sectors
@@bt_w_bigfat:
                mov     ax,[bp+BOOT_OEM_LEN].BPB_RootEntries    ;
                shl     ax,5                                    ; x32
                mov     bx,[bp+BOOT_OEM_LEN].BPB_BytePerSect    ;
                bsf     cx,bx                                   ; shift
                mov     @@bt_w_disknum,dl                       ;
                add     ax,bx                                   ;
                dec     ax                                      ;
                shr     ax,cl                                   ; # of root sectors
                push    ax                                      ; @@bt_w_rootscnt
                cwde                                            ;
                mov     cl,[bp+BOOT_OEM_LEN].BPB_FATCopies      ; 0 in ch here
                imul    cx,[bp+BOOT_OEM_LEN].BPB_SecPerFAT      ;
                add     cx,[bp+BOOT_OEM_LEN].BPB_ResSectors     ; start of root
                push    cx                                      ; @@bt_w_rootpos
                add     ax,cx                                   ;
                push    eax                                     ; @@bt_w_datapos

                mov     bl,[bp+BOOT_OEM_LEN].BPB_SecPerClus     ; make SecPerClus
                bsf     cx,bx                                   ; shift
                sub     esi,eax                                 ;
                shr     esi,cl                                  ; is FAT12?
                cmp     si,MIN_FAT16                            ;
                setc    ch                                      ;
                push    cx                                      ; @@bt_w_clshift/fat12

                mov     dl,@@bt_w_disknum                       ;
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                int     13h                                     ;
                jc      @@bt_w_findname                         ;
                cmp     bx,0AA55h                               ;
                jnz     @@bt_w_findname                         ;
                and     cl,1                                    ;
                mov     @@bt_w_i13x,cl                          ;
@@bt_w_findname:
                push    LOC_7E00h SHR PARASHIFT                 ;
                pop     es                                      ;
                movzx   eax,@@bt_w_rootpos                      ;
                mov     ch,@@bt_w_rootscnt                      ;
                call    @@bt_w_read                             ;

                xor     ax,ax                                   ; root dir
                mov     dx,[bp+BOOT_OEM_LEN].BPB_RootEntries    ;
@@bt_w_namecmp:
                mov     di,ax                                   ;
                mov     cl,11                                   ; ch=0 here
                lea     si,@@bt_w_bootname                      ;
           repe cmpsb                                           ;
                jnz     @@bt_w_namenext                         ;
                test    byte ptr es:[di],18h                    ; check for vol
                jz      @@bt_w_namefound                        ; & dir
@@bt_w_namenext:
                add     ax,20h                                  ;
                dec     dx                                      ;
                jnz     @@bt_w_namecmp                          ;
                jmp     @@bt_w_nofile                           ;
@@bt_w_namefound:
                mov     dx,es:[di+15]                           ; 1st cluster
                mov     si,BOOT_SEG                             ;
                push    si                                      ; for retf below
                push    es                                      ;
@@bt_w_fileloop:
                mov     es,si                                   ;
                mov     @@bt_w_curclus,dx                       ;
                dec     dx                                      ; read
                dec     dx                                      ; cluster
                movzx   eax,dx                                  ;
                mov     cl,@@bt_w_clshift                       ;
                shl     eax,cl                                  ;
                add     eax,@@bt_w_datapos                      ;
                mov     ch,[bp+BOOT_OEM_LEN].BPB_SecPerClus     ;
                call    @@bt_w_read                             ;

                shr     bx,PARASHIFT                            ;
                add     si,bx                                   ;
;----------------------------------------------------------------
; next cluster in chain
; in  : @@bt_w_curclus - cluster
; out : dx  - cluster, !CF - end of file
; save & return : the same as read sector + es, bx destroyd
@@bt_w_next_in_chain:
                mov     eax,@@bt_w_curclus                      ;
                shl     eax,1                                   ;
                test    @@bt_w_fat12,1                          ;
                jz      @@bt_w_nic_not12                        ;
                add     eax,@@bt_w_curclus                      ;
                shr     eax,1                                   ;
@@bt_w_nic_not12:
                xor     edx,edx                                 ;
                movzx   ecx,[bp+BOOT_OEM_LEN].BPB_BytePerSect   ;
                div     ecx                                     ;
                add     ax,[bp+BOOT_OEM_LEN].BPB_ResSectors     ;
                push    FAT_LOAD_SEG                            ;
                pop     es                                      ;
                cmp     ax,@@bt_w_fatpos                        ;
                push    dx                                      ;
                jz      @@bt_w_nic_ready                        ;
                mov     @@bt_w_fatpos,ax                        ;
                mov     ch,2                                    ;
                call    @@bt_w_read                             ;
@@bt_w_nic_ready:
                pop     bx                                      ;
                mov     edx,es:[bx]                             ;  dx??
                mov     cl,@@bt_w_fat12                         ;
                and     cl,1                                    ;
                jnz     @@bt_w_nic_parse12                      ;
                cmp     dx,0FFF8h                               ;
                jmp     @@bt_w_nic_end                          ;
;                ret                                             ;
;----------------------------------------------------------------
@@bt_w_nic_parse12:
                and     cl,@@bt_w_curclus                       ;
                jz      @@bt_w_nic_odd12
                shr     edx,4                                   ;
@@bt_w_nic_odd12:
                and     dh,0Fh                                  ;
                cmp     dx,0FF8h                                ;
;----------------------------------------------------------------
;                ret
;                call    @@bt_w_next_in_chain                    ;
@@bt_w_nic_end:
                jc      @@bt_w_fileloop                         ;

                lea     si,[bp+BOOT_OEM_LEN]                    ;
                mov     bx,@@bt_w_datapos                       ;
                movzx   dx,@@bt_w_disknum                       ;
                xor     di,di                                   ;
                pop     es                                      ; es:[di] - root dir
                push    di                                      ; done!
                retf                                            ;
;----------------------------------------------------------------
@@bt_w_readerr:
                add     bp,offset @@bt_w_err2 - offset @@bt_w_err1
@@bt_w_nofile:
                lea     si,[bp+BOOT_OEM_LEN+size Common_BPB+size Extended_BPB]
@@bt_w_err_msg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@bt_w_err                              ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@bt_w_err_msg                          ;
@@bt_w_err:
                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
;----------------------------------------------------------------
; read sector
; in    : eax - start, ch - count, es - seg to place data
; save  : si, di, ds, ch
; return: ch=0, es=seg, bx=offset to the end of data
@@bt_w_read:
                add     eax,[bp+BOOT_OEM_LEN].BPB_HiddenSec     ;
                xor     bx,bx                                   ;
@@bt_w_sloop:
                mov     cl,NUM_RETRIES                          ;
@@bt_w_rloop:
                pushad                                          ;
                mov     di,sp                                   ;
                xor     edx,edx                                 ;
                cmp     @@bt_w_i13x,dh                          ; cmp with 0
                jz      @@bt_w_no_ext_read                      ;
                push    edx                                     ;
                push    eax                                     ; pos to read
                push    es                                      ; target addr
                push    bx                                      ;
                push    1                                       ; # of sectors
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     si,sp                                   ;
                jmp     @@bt_w_read_call                        ;
@@bt_w_read_done:
                add     bx,[bp+BOOT_OEM_LEN].BPB_BytePerSect    ;
                inc     eax                                     ;
                dec     ch                                      ; count--
                jnz     @@bt_w_sloop                            ;
                ret                                             ;
@@bt_w_no_ext_read:
                movzx   ecx,[bp+BOOT_OEM_LEN].BPB_SecPerTrack   ;
                div     ecx                                     ;
                inc     dl                                      ;
                mov     cl,dl                                   ;
                mov     edx,eax                                 ;
                shr     edx,10h                                 ;
                div     [bp+BOOT_OEM_LEN].BPB_Heads             ;
                xchg    dl,dh                                   ;
                mov     ch,al                                   ;
                shl     ah,6                                    ;
                or      cl,ah                                   ;
                mov     ax,201h                                 ;
@@bt_w_read_call:
                mov     dl,@@bt_w_disknum                       ;
                int     13h                                     ;
                mov     sp,di                                   ;
                popad                                           ; esp restored here
                jnc     @@bt_w_read_done                        ;
@@bt_w_read_err:
                dec     cl                                      ; # retries
                jnz     @@bt_w_rloop                            ;
                jmp     @@bt_w_readerr                          ;
@@bt_w_end:
bt_w_reserved   equ     510 - (@@bt_w_end - bsect16)            ;
if bt_w_reserved GT 0
                db      bt_w_reserved dup (0)                   ;
endif
                dw      0AA55h                                  ;


;================================================================
; FAT32 boot sector
;================================================================

                public  bsect32
                public  bsect32_name
@@bt_d_clshift  = byte ptr [bp-16]                              ; sect->cluster shift
@@bt_d_i13x     = byte ptr [bp-15]                              ; i13x present?
@@bt_d_datapos  =          [bp-14]                              ; cluster data
@@bt_d_disknum  = byte ptr [bp-10]                              ; disk number from DL
@@bt_d_reserved = byte ptr [bp- 9]                              ; not used
@@bt_d_fatpos   =          [bp- 8]                              ; current fat sector
@@bt_d_curclus  =          [bp- 4]                              ; current cluster

;@@bt_d_physdisk = byte ptr [bp + BOOT_F32INFO + size ExtF32_BPB]
@@bt_d_physdisk = @@bt_d_disknum
@@bt_d_bootname = [bp + BOOT_F32INFO + size ExtF32_BPB + size Extended_BPB + 3]

bsect32:
                jmp     @@bt_d_start                            ;
                nop                                             ; os2dasd get DOS version
                db      'QSBT 5.0'                              ; from OEM field and fail
                dw      200h                                    ; to boot on some variants
                db      1h                                      ;
                dw      1                                       ;
                db      2                                       ;
                dw      0E0h                                    ;
                dw      0B40h                                   ;
                db      0F0h                                    ;
                dw      9                                       ;
                dw      18                                      ;
                dw      2                                       ;
                dd      0                                       ;
                dd      0                                       ;
                dd      0                                       ; number of sectors in one FAT copy
                dw      0                                       ; active copy of FAT (bit 0..3)
                dw      0                                       ; version (should be 0)
                dd      0                                       ; first cluster of root directory
                dw      0                                       ; logical sector with FSInfo
                dw      0                                       ; logical sector of 3 boot sectors copy
                db      12 dup (0)                              ; reserved
                db      0h                                      ; 80h
                db      0                                       ;
                db      29h                                     ;
                dd      0                                       ;
                db      'NO LABEL   '                           ;
                db      'FAT32   '                              ;
@@bt_d_err1:    db      'No '
bsect32_name:
                db      'QSINIT     ',0                         ;
@@bt_d_err2:    db      'Read error!',0                         ;
@@bt_d_start:
                mov     bp,LOC_7C00h                            ;
                xor     eax,eax                                 ;
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                mov     sp,bp                                   ;
                push    eax                                     ; space for vars
                push    eax                                     ;
                mov     al,[bp+BOOT_OEM_LEN].BPB_FATCopies      ;
                push    dx                                      ; disk number
                mov     edx,[bp+BOOT_F32INFO].FBPB_SecPerFAT    ;
                mul     edx                                     ;
                mov     dx,[bp+BOOT_OEM_LEN].BPB_ResSectors     ;
                add     eax,edx                                 ;
                push    eax                                     ; @@bt_d_datapos

                mov     bl,[bp+BOOT_OEM_LEN].BPB_SecPerClus     ; make SecPerClus
                bsf     cx,bx                                   ; shift
                push    cx                                      ; @@bt_d_clshift/fat12

                mov     dl,@@bt_d_physdisk                      ;
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                int     13h                                     ;
                jc      @@bt_d_findname                         ;
                cmp     bx,0AA55h                               ;
                jnz     @@bt_d_findname                         ;
                and     cl,1                                    ;
                mov     @@bt_d_i13x,cl                          ;
@@bt_d_findname:
                mov     si,BOOT_SEG                             ;
                push    si                                      ;
                mov     edx,[bp+BOOT_F32INFO].FBPB_RootClus     ;
                call    @@bt_d_fileloop                         ;
                mov     bx,si                                   ; last segment
                pop     dx                                      ; start segment
@@bt_d_namecmp:
                xor     di,di                                   ;
                mov     es,dx                                   ;
                mov     cx,11                                   ;
                lea     si,@@bt_d_bootname                      ;
           repe cmpsb                                           ;
                jnz     @@bt_d_namenext                         ;
                test    byte ptr es:[di],18h                    ; check for vol
                jz      @@bt_d_namefound                        ; & dir
@@bt_d_namenext:
                inc     dx                                      ; + dir size
                inc     dx                                      ;
                cmp     bx,dx                                   ; last segment?
                jnz     @@bt_d_namecmp                          ;
                jmp     @@bt_d_nofile                           ;
@@bt_d_namefound:
                mov     edx,es:[di+7]                           ; hi bytes \ 1st
                mov     dx,es:[di+15]                           ; lo bytes / cluster
                mov     si,BOOT_SEG                             ;
                push    si                                      ; for retf below

                call    @@bt_d_fileloop                         ;

                lea     si,[bp+BOOT_OEM_LEN]                    ;
                movzx   dx,@@bt_d_physdisk                      ;
                push    0
                retf                                            ;
;----------------------------------------------------------------
; read file / root dir loop
; in  : edx - start cluster, si - start segment
; out : si - end segment
; save & return : the same as read sector + es, bx destroyd
@@bt_d_fileloop:
                mov     es,si                                   ;
                mov     @@bt_d_curclus,edx                      ;
                dec     edx                                     ; read
                dec     edx                                     ; cluster
                mov     eax,edx                                 ;
                mov     cl,@@bt_d_clshift                       ;
                shl     eax,cl                                  ;
                add     eax,@@bt_d_datapos                      ;
                mov     cl,[bp+BOOT_OEM_LEN].BPB_SecPerClus     ;
                call    @@bt_d_read                             ;

                shr     bx,PARASHIFT                            ;
                add     si,bx                                   ;
                call    @@bt_d_next_in_chain                    ;
                jc      @@bt_d_fileloop                         ;
                ret
;----------------------------------------------------------------
; next cluster in chain
; in  : @@bt_d_curclus - cluster
; out : edx  - cluster, !CF - end of file
; save & return : the same as read sector + es, bx destroyd, edx - cluster
@@bt_d_next_in_chain:
                mov     eax,@@bt_d_curclus                      ;
                shl     eax,2                                   ;
                xor     edx,edx                                 ;
                movzx   ecx,[bp+BOOT_OEM_LEN].BPB_BytePerSect   ;
                div     ecx                                     ;
                push    dx                                      ;
                mov     dx,[bp+BOOT_OEM_LEN].BPB_ResSectors     ; add it here to prevent
                add     eax,edx                                 ; 0 in @@bt_d_fatpos
                push    FAT_LOAD_SEG                            ;
                pop     es                                      ;
                cmp     eax,@@bt_d_fatpos                       ;
                jz      @@bt_d_nic_ready                        ;
                mov     @@bt_d_fatpos,eax                       ;
                mov     dx,[bp+BOOT_F32INFO].FBPB_ActiveCopy    ;
                and     dx,0Fh                                  ;
                jz      @@bt_d_nic_fcopy0                       ;
                mov     ecx,eax                                 ;
                mov     eax,[bp+BOOT_F32INFO].FBPB_SecPerFAT    ;
                mul     edx                                     ;
                add     eax,ecx                                 ;
@@bt_d_nic_fcopy0:
                mov     cl,1                                    ;
                call    @@bt_d_read                             ;
@@bt_d_nic_ready:
                pop     bx                                      ;
                and     byte ptr es:[bx+3], 0Fh                 ; saves 2 bytes
                mov     edx,es:[bx]                             ;
;                and     edx,0FFFFFFFh                           ;
                cmp     edx,0FFFFFF8h                           ;
                ret                                             ;
;----------------------------------------------------------------
@@bt_d_err:
                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
@@bt_d_readerr:
                add     bp,offset @@bt_d_err2 - offset @@bt_d_err1
@@bt_d_nofile:
                lea     si,[bp+BOOT_F32INFO+size ExtF32_BPB+size Extended_BPB]
@@bt_d_err_msg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@bt_d_err                              ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@bt_d_err_msg                          ;
;----------------------------------------------------------------
; read sector
; in    : eax - start, cl - count, es - seg to place data
; save  : si, di, ds
; return: cl=0, es=seg, bx=offset to the end of data
@@bt_d_read:
                push    edx                                     ;
                xor     edx,edx                                 ;
                add     eax,[bp+BOOT_OEM_LEN].BPB_HiddenSec     ;
                adc     dl,dl                                   ; 2Tb ;)
                xor     bx,bx                                   ;
@@bt_d_sloop:
                mov     ch,NUM_RETRIES                          ;
@@bt_d_rloop:
                pushad                                          ;
                mov     di,sp                                   ;
                cmp     @@bt_d_i13x,dh                          ; cmp with 0
                jz      @@bt_d_no_ext_read                      ;
                push    edx                                     ;
                push    eax                                     ; pos to read
                push    es                                      ; target addr
                push    bx                                      ;
                push    1                                       ; # of sectors
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     si,sp                                   ;
                jmp     @@bt_d_read_call                        ;
@@bt_d_read_done:
                add     bx,[bp+BOOT_OEM_LEN].BPB_BytePerSect    ;
                inc     eax                                     ;
                setz    ch                                      ; 2Tb border
                add     dl,ch                                   ; inc
                dec     cl                                      ;
                jnz     @@bt_d_sloop                            ;
                pop     edx                                     ;
                ret                                             ;
@@bt_d_no_ext_read:
                movzx   ecx,[bp+BOOT_OEM_LEN].BPB_SecPerTrack   ;
                div     ecx                                     ;
                inc     dl                                      ;
                mov     cl,dl                                   ;
                mov     edx,eax                                 ;
                shr     edx,10h                                 ;
                div     [bp+BOOT_OEM_LEN].BPB_Heads             ;
                xchg    dl,dh                                   ;
                mov     ch,al                                   ;
                shl     ah,6                                    ;
                or      cl,ah                                   ;
                mov     ax,201h                                 ;
@@bt_d_read_call:
                mov     dl,@@bt_d_physdisk                      ;
                int     13h                                     ;
                mov     sp,di                                   ;
                popad                                           ; esp restored here
                jnc     @@bt_d_read_done                        ;
@@bt_d_read_err:
                dec     ch                                      ; # retries
                jnz     @@bt_d_rloop                            ;
                jmp     @@bt_d_readerr                          ;
@@bt_d_end:
bt_d_reserved   equ     510 - (@@bt_d_end - bsect32)            ;
if bt_d_reserved GT 0
                db      bt_d_reserved dup (0)                   ;
endif
                dw      0AA55h                                  ;

;================================================================
; exFAT boot sector
;================================================================

                public  bsectexf
                public  bsectexf_name
                public  exf_bsacount
@@bt_e_numret   = byte ptr [bp-34]                              ; # of retries
@@bt_e_wofat    = byte ptr [bp-33]                              ; w/o fat mode (1/0)
@@bt_e_fatpos   =          [bp-32]                              ;
@@bt_e_cluspos  =          [bp-28]                              ;
@@bt_e_fileclus =          [bp-24]                              ;
@@bt_e_filelen  = word ptr [bp-20]                              ; file len in sectors
@@bt_e_fnlen    = byte ptr [bp-18]                              ; file name len
@@bt_e_isfile   = byte ptr [bp-17]
@@bt_e_heads    = word ptr [bp-16]
@@bt_e_spt      = word ptr [bp-14]
@@bt_e_physdisk = byte ptr [bp-12]                              ; disk number from DL
@@bt_e_i13x     = byte ptr [bp-11]                              ; i13x present?
@@bt_e_sectsz   = word ptr [bp-10]                              ; sector size in bytes
@@bt_e_clussz   =          [bp- 8]                              ; cluster size in sectors
@@bt_e_curclus  = dword ptr [bp- 4]                             ; file cluster

bsectexf:
                jmp     @@bt_e_start                            ;
                nop                                             ;
                db      'EXFAT   '                              ;
                db      53 dup (0)                              ;
                dq      0                                       ; partition pos
                dq      0                                       ; partition size
                dd      0                                       ; FAT first sector
                dd      0                                       ; # of FAT sectors
                dd      0                                       ; first data sector
                dd      0                                       ; # of clusters on disk
                dd      0                                       ; root dir first cluster
                dd      0                                       ; serial number
                dw      100h                                    ; version
                dw      0                                       ; volume state
                db      9                                       ; sector size bit
                db      0                                       ; sectors per cluster bit
                db      1                                       ;
                db      80h                                     ;
                db      0                                       ; percentage of allocated space
                db      6 dup (0)                               ; reseved? (to get EB 76 90 in _bsectexf)
exf_bsacount:
                db      1                                       ; # of additional sectors to read
@@bt_e_start:
                mov     bp,LOC_7C00h                            ;
                xor     eax,eax                                 ;
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                mov     sp,bp                                   ;
                push    eax                                     ; @@bt_e_curclus
                push    eax                                     ;
                xor     dh,dh                                   ; @@bt_e_i13x
                push    edx                                     ;
                push    eax                                     ;
                push    eax                                     ;
                push    eax                                     ; @@bt_e_fileclus
                push    eax                                     ; @@bt_e_cluspos
                push    eax                                     ; @@bt_e_fatpos
                push    ax                                      ;

                mov     dl,@@bt_e_physdisk                      ;
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                int     13h                                     ;
                jc      @@bt_e_noi13x                           ;
                cmp     bx,0AA55h                               ;
                jnz     @@bt_e_noi13x                           ;
                and     cl,1                                    ;
                mov     @@bt_e_i13x,cl                          ;
                jnz     @@bt_e_ldnext                           ;
@@bt_e_noi13x:
                mov     dl,@@bt_e_physdisk                      ;
                mov     ah,8                                    ;
                int     13h                                     ; no LBA & no CHS?
                jc      @@bt_e_readerr                          ; -> read error
                and     cx,3Fh                                  ;
                mov     @@bt_e_spt,cx                           ;
                movzx   cx,dh                                   ;
                inc     cx                                      ;
                mov     @@bt_e_heads,cx                         ;
@@bt_e_ldnext:
                mov     cl,[bp].BR_ExF_SecSize                  ;
                mov     ax,1                                    ;
                push    ax                                      ;
                shl     ax,cl                                   ;
                mov     @@bt_e_sectsz,ax                        ;
                pop     ax
                mov     cl,[bp].BR_ExF_ClusSize                 ;
                shl     ax,cl                                   ;
                mov     @@bt_e_clussz,ax                        ;

                mov     di,LOC_7C00h SHR PARASHIFT              ;
                xor     eax,eax                                 ;
                inc     al                                      ;
                movzx   si,byte ptr [bp+(exf_bsacount - bsectexf)] ;
@@bt_e_loadmore:
                add     di,512 shr PARASHIFT                    ; read sector by sector
                mov     cx,1                                    ; with 512b offset
                push    eax                                     ; for case of 1/2/4k
                call    @@bt_e_read                             ; sector size
                pop     eax                                     ;
                inc     eax                                     ; next sector
                dec     si                                      ;
                jnz     @@bt_e_loadmore                         ; until end of exf_bsacount

                jmp     bsectexf_2nd_sector                     ;

@@bt_e_readerr:
                lea     si,[bp+(@@bt_e_str1 - bsectexf)]        ;
                jmp     @@bt_e_msg                              ;
@@bt_e_exiterr:
                lea     si,[bp+(@@bt_e_str2 - bsectexf)]        ;
@@bt_e_msg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@bt_e_err                              ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@bt_e_msg                              ;
@@bt_e_str1:    db      '<disk read error>',0                   ;
@@bt_e_str2:    db      '<press any key to reboot>',0           ;
@@bt_e_err:
                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
;----------------------------- -----------------------------------
; read sectors
; in    : eax - start, cx - count, di - seg to place data
; save  : si, di, ds, edx
; return: cx=0, es:bx - offset to the end of data
@@bt_e_read:
                mov     es,di                                   ;
                push    edx                                     ;
                xor     ebx,ebx                                 ;
                mov     edx,dword ptr [bp].BR_ExF_VolStart+4    ; we can boot
                add     eax,dword ptr [bp].BR_ExF_VolStart      ; above 2Tb border!
                adc     edx,ebx                                 ;
@@bt_e_sloop:
                mov     @@bt_e_numret,NUM_RETRIES               ;
@@bt_e_rloop:
                pushad                                          ;
                mov     di,sp                                   ;
                cmp     @@bt_e_i13x,bl                          ; cmp with 0
                jz      @@bt_e_no_ext_read                      ;
                push    edx                                     ;
                push    eax                                     ; pos to read
                push    es                                      ; target addr
                push    bx                                      ;
                push    1                                       ; # of sectors
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     si,sp                                   ;
                jmp     @@bt_e_read_call                        ;
@@bt_e_read_done:
                add     bx,@@bt_e_sectsz                        ;
                jnz     @@bt_e_read_noseg                       ; end of segment?
                mov     bx,es                                   ;
                add     bx,1000h                                ;
                mov     es,bx                                   ;
                xor     bx,bx                                   ;
@@bt_e_read_noseg:
                inc     eax                                     ;
                jnz     @@bt_e_read_noovf                       ;
                inc     edx                                     ; 2Tb border inc
@@bt_e_read_noovf:
                loop    @@bt_e_sloop                            ;
                pop     edx                                     ;
                ret                                             ;
@@bt_e_no_ext_read:
                movzx   ecx,@@bt_e_spt                          ;
                div     ecx                                     ;
                inc     dl                                      ;
                mov     cl,dl                                   ;
                mov     edx,eax                                 ;
                shr     edx,10h                                 ;
                div     @@bt_e_heads                            ;
                xchg    dl,dh                                   ;
                mov     ch,al                                   ;
                shl     ah,6                                    ;
                or      cl,ah                                   ;
                mov     ax,201h                                 ;
@@bt_e_read_call:
                mov     dl,@@bt_e_physdisk                      ;
                int     13h                                     ;
                mov     sp,di                                   ;
                popad                                           ; esp restored here
                jnc     @@bt_e_read_done                        ;
@@bt_e_read_err:
                dec     @@bt_e_numret                           ; # retries
                jnz     @@bt_e_rloop                            ;
                jmp     @@bt_e_readerr                          ;

@@bt_e_end:
bt_e_reserved   equ     510 - (@@bt_e_end - bsectexf)           ;
if bt_e_reserved GT 0
                db      bt_e_reserved dup (0)                   ;
endif
                dw      0AA55h                                  ;
;
; second sector of exFAT boot code
;
bsectexf_2nd_sector:
                push    ds                                      ;
                pop     es                                      ; calc source
                lea     di,[bp+(bsectexf_name - bsectexf)]      ; file name len
                mov     cx,-1                                   ;
                xor     al, al                                  ;
          repne scasb                                           ;
                not     cl                                      ;
                dec     cl                                      ;
                mov     @@bt_e_fnlen,cl                         ;

                mov     eax,[bp].BR_ExF_RootClus                ; 1st cluster
                mov     @@bt_e_curclus,eax                      ;
@@bt_e_dirmain:
                mov     di,FAT_LOAD_SEG + (4096 shr PARASHIFT)  ;
                mov     cx,1                                    ; read next sector
                call    @@bt_e_clread                           ; from cluser chain
                or      cx,cx                                   ;
                jnz     @@bt_e_exiterr                          ; cx!=0 -> end of dir, exit

                mov     cx,@@bt_e_sectsz                        ;
                shr     cx,5                                    ;
                xor     bx,bx                                   ;
@@bt_e_direnum:
                push    cx                                      ;
                mov     al,es:[bx]                              ;
                or      al,al                                   ; end of dir?
                jz      @@bt_e_exiterr                          ;
                cmp     al,085h                                 ;
                jnz     @@bt_e_direnum_2                        ;
                test    byte ptr es:[bx+4],10h                  ; is this a dir?
                setz    @@bt_e_isfile                           ;
                jmp     @@bt_e_dirnext                          ;
@@bt_e_direnum_2:
                test    @@bt_e_isfile,1                         ; checks file only
                jz      @@bt_e_dirnext                          ;
                cmp     al,0C0h                                 ;
                jnz     @@bt_e_direnum_3                        ;
                mov     al,@@bt_e_fnlen                         ;
                cmp     es:[bx+3],al                            ; name len match?
@@bt_e_direnum_mis:
                setz    @@bt_e_isfile                           ;
                jnz     @@bt_e_dirnext                          ;
                xor     eax,eax                                 ;
                cmp     es:[bx+12],eax                          ;
                jnz     @@bt_e_direnum_mis                      ;
                mov     ax,es:[bx+10]                           ;
                cmp     ax,6                                    ; up to 448k-1!
                ja      @@bt_e_direnum_mis                      ;
                mov     eax,es:[bx+8]                           ;
                movzx   ecx,@@bt_e_sectsz                       ;
                lea     eax,[eax+ecx-1]                         ; calc file len
                mov     cl,[bp].BR_ExF_SecSize                  ; in sectors
                shr     eax,cl                                  ;
                mov     @@bt_e_filelen,ax                       ;
                mov     eax,es:[bx+20]                          ; 1st cluster
                mov     @@bt_e_fileclus,eax                     ;
                mov     al,2                                    ; no FAT flag
                and     al,es:[bx+1]                            ;
                or      @@bt_e_isfile,al                        ;
                jmp     @@bt_e_dirnext                          ;
@@bt_e_direnum_3:
                cmp     al,0C1h                                 ;
                jnz     @@bt_e_dirnext                          ;
                movzx   cx,@@bt_e_fnlen                         ;
                lea     si,[bp+(bsectexf_name - bsectexf)]      ; file name len
                xor     di,di                                   ;
@@bt_e_direnum_nameloop:
                inc     di                                      ; compare name
                lodsb                                           ; (up to 15 chars)
                inc     di                                      ;
                mov     dx,es:[bx+di]                           ;
                or      dh,dh                                   ;
                jnz     @@bt_e_dirnext                          ;
                cmp     dl,61h                                  ; 'A'..'Z'
                jc      @@bt_e_direnum_up                       ;
                cmp     dl,7Bh                                  ;
                jnc     @@bt_e_direnum_up                       ;
                sub     dl,20h                                  ;
@@bt_e_direnum_up:
                cmp     al,dl                                   ;
                jnz     @@bt_e_dirnext                          ;
                loop    @@bt_e_direnum_nameloop                 ;

; yes! we have file, len in # of sectors & start cluster
                test    @@bt_e_isfile,2                         ; flag for cluster
                setnz   @@bt_e_wofat                            ; read
                mov     eax,@@bt_e_fileclus                     ;
                mov     @@bt_e_curclus,eax                      ;
                xor     eax,eax                                 ;
                mov     @@bt_e_cluspos,eax                      ;

                mov     cx,@@bt_e_filelen                       ;
                mov     di,BOOT_SEG                             ;
                push    di                                      ;
                call    @@bt_e_clread                           ;
                or      cx,cx                                   ;
                jnz     @@bt_e_readerr                          ;

                mov     dl,@@bt_e_physdisk                      ;
                mov     dh,BF_NEWBPB                            ;

                push    (LOC_7E00h+512) shr PARASHIFT           ; 20 bytes behind
                pop     es                                      ; self 2 sectors
                lea     si,[bp].BR_ExF_VolStart                 ;
                mov     di,2                                    ; fill Disk_NewPB
                mov     cx,8                                    ;
                cld                                             ;
            rep movsw                                           ;
                xor     si,si                                   ;
                push    es                                      ;
                pop     ds                                      ;
                mov     ax,@@bt_e_sectsz                        ;
                mov     [si],ax                                 ;
                mov     [si+18],si                              ; NPB_Reserved = 0
                push    si                                      ;
                retf                                            ;
@@bt_e_dirnext:
                pop     cx                                      ;
                add     bx,32                                   ;
                dec     cx                                      ;
                jnz     @@bt_e_direnum                          ;
                jmp     @@bt_e_dirmain                          ;

;----------------------------- -----------------------------------
; read cluster data
; in    : cx - sector count, di - seg to place data
; save  : si, ds
; out   : cx = 0 if ok, else - end of file data (only when FAT chain used)
@@bt_e_clread:
                mov     eax,@@bt_e_clussz                       ;
                mov     edx,@@bt_e_cluspos                      ;
                sub     eax,edx                                 ; remain in cluster
                movzx   ecx,cx                                  ;
                cmp     eax,ecx                                 ;
                jnc     @@bt_e_clread_fit                       ;
                sub     cx,ax                                   ;
                push    cx                                      ;
                mov     cx,ax                                   ;
                jcxz    @@bt_e_clread_eoclus                    ; already the end?
                jmp     @@bt_e_clread_loop                      ;
@@bt_e_clread_fit:
                push    0                                       ;
@@bt_e_clread_loop:
                lea     ebx,[edx+ecx]                           ;
                mov     @@bt_e_cluspos,ebx                      ;
                mov     bx,cx                                   ;
                push    cx                                      ;
                mov     cl,[bp].BR_ExF_SecSize                  ;
                sub     cl,PARASHIFT                            ;
                shl     bx,cl                                   ;
                pop     cx                                      ;
                add     bx,di                                   ;
                push    bx                                      ; seg for next cluster
                push    cx                                      ;
                mov     eax,@@bt_e_curclus                      ;
                sub     eax,2                                   ;
                mov     cl,[bp].BR_ExF_ClusSize                 ;
                shl     eax,cl                                  ;
                add     eax,edx                                 ;
                add     eax,[bp].BR_ExF_DataPos                 ;
                pop     cx                                      ;
                call    @@bt_e_read                             ;
                pop     di                                      ;
@@bt_e_clread_eoclus:
                pop     cx                                      ;
                jcxz    @@bt_e_clread_done                      ;
@@bt_e_clread_nextclus:
; seek to next cluster
                xor     eax,eax                                 ;
                mov     @@bt_e_cluspos,eax                      ;
                test    @@bt_e_wofat,1                          ;
                jz      @@bt_e_ncl_readfat                      ;
                inc     @@bt_e_curclus                          ;
                jmp     @@bt_e_clread                           ;
@@bt_e_clread_done:
                ret                                             ;
@@bt_e_ncl_readfat:
                push    cx                                      ;
                mov     eax,@@bt_e_curclus                      ;
                shl     eax,2                                   ;
                xor     edx,edx                                 ;
                movzx   ecx,@@bt_e_sectsz                       ;
                div     ecx                                     ;

                push    dx                                      ;
                mov     edx,[bp].BR_ExF_FATPos                  ; add it here to prevent
                add     eax,edx                                 ; 0 in @@bt_e_fatpos
                push    FAT_LOAD_SEG                            ;
                pop     es                                      ;
                cmp     eax,@@bt_e_fatpos                       ;
                jz      @@bt_e_ncl_fatready                     ;
                mov     @@bt_e_fatpos,eax                       ;
                mov     cx,1                                    ;
                push    di                                      ;
                mov     di,es                                   ;
                call    @@bt_e_read                             ;
                pop     di                                      ;
@@bt_e_ncl_fatready:
                pop     bx                                      ;
                mov     edx,es:[bx]                             ;
                mov     @@bt_e_curclus,edx                      ;
                or      edx,edx                                 ; small check
                jz      @@bt_e_readerr                          ;
                add     edx,9                                   ; should be < FFFFFFF7
                pop     cx                                      ;
                jc      @@bt_e_clread_done                      ; cx = unreaded # of sectors
                jmp     @@bt_e_clread                           ;

bsectexf_name:
                db      'QSINIT',0,0,0,0,0,0,0,0,0,0            ; 15 chars + 0
@@bt_e2_end:
bt_e2_reserved  equ     510 - (@@bt_e2_end - bsectexf_2nd_sector) ;
if bt_e2_reserved GT 0
                db      bt_e2_reserved dup (0)                  ;
endif
                dw      0AA55h                                  ;

;================================================================
; debug boot sector (for printing only)
;================================================================

                public  bsectdb
@@bt_p_disknum  = byte ptr [bp- 2]                              ; disk number from DL
@@bt_p_fat32    = byte ptr [bp- 1]                              ;

;@@bt_p_physdisk = byte ptr [bp + BOOT_F32INFO + size ExtF32_BPB]
@@bt_p_physdisk = @@bt_p_disknum
@@bt_p_msgbase  = [bp + @@bt_p_msg - bsectdb]

bsectdb:
                jmp     @@bt_p_start                            ;
                nop                                             ; os2dasd get DOS version
                db      'QSBT 5.0'                              ; from OEM field and fail
                dw      200h                                    ; to boot on some variants
                db      1h                                      ;
                dw      1                                       ;
                db      2                                       ;
                dw      0E0h                                    ;
                dw      0B40h                                   ;
                db      0F0h                                    ;
                dw      9                                       ;
                dw      18                                      ;
                dw      2                                       ;
                dd      0                                       ;
                dd      0                                       ;
                dd      0                                       ; number of sectors in one FAT copy
                dw      0                                       ; active copy of FAT (bit 0..3)
                dw      0                                       ; version (should be 0)
                dd      0                                       ; first cluster of root directory
                dw      0                                       ; logical sector with FSInfo
                dw      0                                       ; logical sector of 3 boot sectors copy
                db      12 dup (0)                              ; reserved
                db      0h                                      ; 80h
                db      0                                       ;
                db      29h                                     ;
                dd      0                                       ;
                db      19 dup (0)                              ;
@@bt_p_start:
                mov     bp,LOC_7C00h                            ;
                xor     eax,eax                                 ;
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                mov     sp,bp                                   ;
                push    dx                                      ; @@bt_p_disknum
                lea     si,@@bt_p_msgbase                       ;
                call    @@bt_p_print                            ;
                mov     al,@@bt_p_physdisk                      ;
                call    @@bt_p_hex                              ;
                call    @@bt_p_print                            ;
                inc     si                                      ;
                call    @@bt_p_print                            ;

                lea     di,[bp+ BOOT_OEM_LEN + size Common_BPB] ;
                mov     al,[di]                                 ;
                call    @@bt_p_hex                              ;
                call    @@bt_p_print                            ;
                add     di,size ExtF32_BPB                      ;
                mov     al,[di]                                 ;
                call    @@bt_p_hex                              ;
                call    @@bt_p_print                            ;

                mov     dl,@@bt_p_physdisk                      ;
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                int     13h                                     ;
                jc      @@bt_p_noi13x                           ;
                cmp     bx,0AA55h                               ;
                jnz     @@bt_p_noi13x                           ;
                and     cl,1                                    ;
                jmp     @@bt_p_i13xprn                          ;
@@bt_p_noi13x:
                xor     cl,cl                                   ;
@@bt_p_i13xprn:
                add     cl,30h                                  ;
                mov     [si],cl                                 ;
                call    @@bt_p_print                            ;
                mov     ax,word ptr [bp+BOOT_OEM_LEN].BPB_HiddenSec + 2 ;
                call    @@bt_p_word                             ;
                call    @@bt_p_print                            ;
                mov     ax,word ptr [bp+BOOT_OEM_LEN].BPB_HiddenSec ;
                call    @@bt_p_word                             ;
                call    @@bt_p_print                            ;
                mov     ah,8                                    ;
                mov     dl,@@bt_p_physdisk                      ;
                int     13h                                     ; max head for
                mov     al,dh                                   ; CHS i/o
                inc     al                                      ;
                call    @@bt_p_hex                              ;
                call    @@bt_p_print                            ;

                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
@@bt_p_print:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@bt_p_err                              ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@bt_p_print                            ;
@@bt_p_err:
                dec     si                                      ;
                ret                                             ;
@@bt_p_word:
                push    ax                                      ; print word
                mov     al,ah                                   ;
                call    @@bt_p_hex                              ;
                pop     ax                                      ;
                inc     bx                                      ;
                jmp     @@bt_p_hex1                             ;
@@bt_p_hex:
                xor     bx,bx                                   ; print byte
@@bt_p_hex1:
                push    ax                                      ;
                shr     al,4                                    ;
                call    @@bt_p_hex_sym                          ;
                pop     ax                                      ;
                and     al,0Fh                                  ;
                inc     bx                                      ;
@@bt_p_hex_sym:
                add     al,30h                                  ;
                cmp     al,3Ah                                  ;
                jc      @@bt_p_hex_done                         ;
                add     al,41h-3Ah                              ;
@@bt_p_hex_done:
                mov     [bx][si],al                             ;
                ret                                             ;
@@bt_p_msg:
                db      'dl=',0,0,0                             ;
                db      ' bpb.disk=',0,0,',',0,0                ;
                db      ' i13x=',0                              ;
                db      ' hidden=',0,0,0,0,0,0,0,0              ;
                db      ' heads=',0,0                           ;
                db      13,10,'press any key to reboot',0       ;
@@bt_p_end:
bt_p_reserved   equ     510 - (@@bt_p_end - bsectdb)            ;
if bt_p_reserved GT 0
                db      bt_p_reserved dup (0)                   ;
endif
                dw      0AA55h                                  ;

;================================================================
; partition table code for GPT disks
;================================================================
                public  gptsect

@@pt_g_disknum  = byte ptr [bp- 2]                              ; disk number from DL
@@pt_g_rps      = word ptr [bp- 4]                              ; # gpt recs per sector

gptsect:
                and     ax,5351h                                ; %QS% string,
                and     ax,2420h                                ; but zero ax too :)
                mov     si,LOC_7C00h                            ;
                mov     ss,ax                                   ;
                mov     sp,si                                   ;
                mov     bp,si                                   ; 7C00h const
                mov     es,ax                                   ;
                mov     ds,ax                                   ;
                cld                                             ;
                mov     di,LOC_0600h                            ;
                mov     cx,100h                                 ;
            rep movsw                                           ;
                JmpF16a 0,LOC_0600h + (@@pt_g_moved - gptsect)  ;
@@pt_g_moved:
                sti                                             ; walk through 4
                mov     cl,4                                    ; pt entries and
                sub     di,4 * size MBR_Record + 2              ; search for UEFI
@@pt_g_loop:
                cmp     [di].PTE_Type,PTE_EE_UEFI               ;
                jz      @@pt_g_uefi                             ;
                add     di,size MBR_Record                      ;
                loop    @@pt_g_loop                             ;
                int     18h                                     ;
@@pt_g_uefi:
                push    dx                                      ; save disk number
                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                int     13h                                     ;
                jc      @@pt_g_noi13x                           ;
                cmp     bx,0AA55h                               ;
                jnz     @@pt_g_noi13x                           ;
                and     cl,1                                    ;
                jz      @@pt_g_noi13x                           ;

                mov     ah,48h                                  ; get sector size
                mov     dl,@@pt_g_disknum                       ;
                mov     si,bp                                   ;
                mov     dword ptr [si],1Eh                      ; zero flags
                int     13h                                     ;
                jc      @@pt_g_noinfo                           ;
                mov     ax,[si+18h]                             ;
                bsr     cx,ax                                   ; sector size
                bsf     cx,ax                                   ; must have only
                jnz     @@pt_g_noinfo                           ; ONE bit
                sub     cl,7                                    ;
                jc      @@pt_g_noinfo                           ; <128?
                mov     ax,1                                    ;
                shl     ax,cl                                   ;
                push    ax                                      ;
                jmp     @@pt_g_gptread                          ;
@@pt_g_noinfo:
                push    4                                       ; @@pt_g_rps
@@pt_g_gptread:
                mov     eax,[di].PTE_LBAStart                   ;
                call    @@pt_g_read                             ;
                cmp     dword ptr [bp],GPT_SIGNDDLO             ;
                jnz     @@pt_m_badpt                            ;
                cmp     word ptr [bp].GPT_PtEntrySize, GPT_RECSIZE
                jnz     @@pt_m_badpt                            ;

                bsf     cx,@@pt_g_rps                           ;
                mov     dx,word ptr [bp].GPT_PtCout             ;
                shr     dx,cl                                   ; # of sectors to read
                jz      @@pt_m_badpt                            ;
                mov     eax,dword ptr [bp].GPT_PtInfo           ;
@@pt_g_ptloop:
                call    @@pt_g_read                             ;

                mov     cx,@@pt_g_rps                           ; recs per sector
                mov     di,bp                                   ;
@@pt_g_checkloop:
                mov     ebx,dword ptr [di].PTG_FirstSec + 4     ;
                or      ebx,dword ptr [di].PTG_FirstSec         ; zero? skip it
                jz      @@pt_g_checknext                        ;

                test    byte ptr [di].PTG_Attrs, 1 shl GPTATTR_BIOSBOOT
                jnz     @@pt_g_goboot                           ;
@@pt_g_checknext:
                add     di,GPT_RECSIZE                          ;
                loop    @@pt_g_checkloop                        ;

                inc     eax                                     ;
                dec     dx                                      ;
                jnz     @@pt_g_ptloop                           ;
                jmp     @@pt_g_missing                          ;
@@pt_g_goboot:
                mov     edx,dword ptr [di].PTG_FirstSec + 4     ; read boot sector
                mov     eax,dword ptr [di].PTG_FirstSec         ;
                call    @@pt_g_read_q                           ;
                cmp     word ptr [bp+510],0AA55h                ;
                jnz     @@pt_g_missing                          ;
                or      edx,edx                                 ; start above 2Tb?
                movzx   dx,@@pt_g_disknum                       ; then skip BPB check
                jnz     @@pt_g_notbpb                           ;
                bsr     ax,[bp].Boot_Record.BR_BPB.BPB_BytePerSect ; check bps
                bsf     cx,[bp].Boot_Record.BR_BPB.BPB_BytePerSect ; is it 1 bit
                cmp     ax,cx                                   ; in range 512..4096?
                jnz     @@pt_g_notbpb                           ; if yes - update
                cmp     ax,9                                    ; Hidden Sectors value
                jc      @@pt_g_notbpb                           ; in BPB.
                cmp     ax,12                                   ;
                jg      @@pt_g_notbpb                           ;
                mov     [bp].Boot_Record.BR_BPB.BPB_HiddenSec,ebx ;
@@pt_g_notbpb:
                JmpF16a 0,LOC_7C00h                             ;

;----------------------------------------------------------------
; read sector
; in    : eax - sector  [edx - high dword]
; return: save all except cx, cx = 0
@@pt_g_read:
                xor     edx,edx                                 ;
@@pt_g_read_q:
                mov     cx,NUM_RETRIES                          ;
@@pt_g_rloop:
                pushad                                          ;
                mov     di,sp                                   ;
                push    edx                                     ;
                push    eax                                     ; pos to read
                push    0                                       ; target addr
                push    bp                                      ;
                push    1                                       ; # of sectors
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     si,sp                                   ;
                mov     dl,@@pt_g_disknum                       ;
                int     13h                                     ;
                mov     sp,di                                   ;
                popad                                           ; esp restored here
                jc      @@pt_g_read_err                         ;
                ret
@@pt_g_read_err:
                loop    @@pt_g_rloop                            ; # retries
                ; no ret here
@@pt_g_errread:
                mov     si,LOC_0600h + (@@pt_g_msg_io - gptsect) ;
                jmp     @@pt_g_haltmsg                          ;
@@pt_g_missing:
                mov     si,LOC_0600h + (@@pt_g_msg_os - gptsect) ;
                jmp     @@pt_g_haltmsg                          ;
@@pt_m_badpt:
                mov     si,LOC_0600h + (@@pt_g_msg_pt - gptsect) ;
                jmp     @@pt_g_haltmsg                          ;
@@pt_g_noi13x:
                mov     si,LOC_0600h + (@@pt_g_msg_xr - gptsect) ;
@@pt_g_haltmsg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@pt_g_halt                             ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@pt_g_haltmsg                          ;
@@pt_g_halt:
                hlt                                             ;
                jmp     @@pt_g_halt                             ;

@@pt_g_msg_xr:  db      '<no i13x!>',0                          ;
@@pt_g_msg_os:  db      '<no operating system>',0               ;
@@pt_g_msg_io:  db      '<disk read error>',0                   ;
@@pt_g_msg_pt:  db      '<error in partition table>',0          ;
gptsect_end:

pt_g_reserved   equ     size MBR_Code - (gptsect_end - gptsect) ;
                db      pt_g_reserved dup (0)                   ;

                dd      0                                       ;
                dw      0                                       ;
                db      4 * size MBR_Record dup (0)             ;
                dw      0AA55h                                  ;


PARTMGR_CODE    ends
                end
