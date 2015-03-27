;
; QSINIT
; HPFS boot code (micro-FSD)
;
;  * NOT able to load OS/2 1.x kernels (IBM code supports it until now ;))
;  * uses disk number from DL, not from own BPB!
;  * can read partition on 2Tb border ;) (i.e. only boot sector must have
;    32-bit number)
;  * incoming names must be in UPPER case. This is bug in original micro-FSD,
;    was reproduced here for compatibility ;)
;  * size made the same with original micro (12kb, but actually code uses
;    only 10 - 2kb for self, 4kb - i/o buffer & 4kb - stack space)
;
                include inc/qstypes.inc
                include inc/parttab.inc
                include inc/filetab.inc
                include inc/seldesc.inc

                NUM_RETRIES  = 4
                LOC_7C00h    = 7C00h
                BOOT_OEM_LEN = 11                               ; jmp + label
                BOOT_SEG     = 1000h                            ; OS2LDR location
                SELF_SEG_LEN = 12                               ; len of micro-FSD in kb
                BUFFER_POS   = 1000h                            ;
                MFSD_SEG     = 7Ch                              ; mini-FSD target seg
                CODE_SECTORS = (hpfs_boot_total-main_code)/512  ; len of micro-FSD

_DATA           segment byte public USE32 'DATA'
                public  _hpfs_bscount
_hpfs_bscount   dd      CODE_SECTORS + 1
_DATA           ends

; force this segment to separate object (to make offsets zero-based and use it
; freely over code!)
HPFSBOOT_CODE   segment byte public USE16 'HPFS'
                assume  cs:HPFSBOOT_CODE,ds:HPFSBOOT_CODE,es:nothing,ss:HPFSBOOT_CODE

                org     0h

                public  _hpfs_bsect
                public  _hpfs_bsname
_hpfs_bsect:
                jmp     @@bt_h_start                            ;
                nop                                             ; os2dasd get DOS version
                db      'QSBT 5.0'                              ; from OEM field and fail
                dw      200h                                    ; to boot on some variants
                db      0                                       ;
                dw      0                                       ;
                db      0                                       ;
                dw      0                                       ;
                dw      0                                       ;
                db      0F0h                                    ;
                dw      0                                       ;
bpb_spt         dw      18                                      ; sector per track
bpb_numheads    dw      2                                       ;
bpb_ptstart     dd      0                                       ;
                dd      0                                       ;
bpb_disk        db      0h                                      ; 80h
                db      0                                       ;
                db      28h                                     ;
                dd      0                                       ;
                db      'NO LABEL   '                           ;
                db      'HPFS    '                              ;
user_ds         dw      0                                       ;
user_sp         dw      0                                       ;
user_ss         dw      0                                       ;
stack_pos       dw      0                                       ;
fsd_mode        db      0                                       ;
i13x_on         db      0                                       ;
stack_rest      db      0                                       ; restore stack on exit
root_dirblk     dd      0                                       ;
cb_lsn          dd      0                                       ; current 4 sectors
cf_size         dd      0                                       ; current file size
cf_readlen      dd      0                                       ;
@@bt_h_start:
                mov     si,LOC_7C00h                            ;
                xor     ax,ax                                   ;
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                mov     sp,si                                   ;
                mov     [si+bpb_disk],dl                        ; update BPB

                int     12h                                     ; get memory for
                sub     ax,64 + SELF_SEG_LEN                    ; OS2LDR + micro
                and     ax,0FFF0h                               ; round down
                shl     ax,6                                    ; kb -> para
                mov     es,ax                                   ;
                cld                                             ;
                xor     di,di                                   ;
                mov     cx,100h                                 ;
            rep movsw                                           ;
                push    es                                      ;
                push    offset @@bt_h_home                      ;
                retf                                            ;
@@bt_h_home:
                mov     ds,ax                                   ;
                mov     ss,ax                                   ; point stack to the
                mov     sp,SELF_SEG_LEN shl 10                  ; end of own seg

                mov     ah,41h                                  ;
                mov     bx,55AAh                                ;
                mov     dl,bpb_disk                             ;
                int     13h                                     ;
                jc      @@bt_h_findname                         ;
                cmp     bx,0AA55h                               ;
                jnz     @@bt_h_findname                         ;
                and     cl,1                                    ;
                mov     i13x_on,cl                              ;
@@bt_h_findname:
                mov     bx,200h                                 ;
                xor     eax,eax                                 ; 2nd sector
                inc     al                                      ;
                mov     cl,CODE_SECTORS                         ; append own code
                call    read_sector                             ; from next sectors


                jmp     main_code                               ;
;----------------------------------------------------------------
panic_nomini    label   near                                    ; no OS2BOOT
                mov     si,offset miniFSD_name                  ;
                mov     byte ptr miniFSD_nzero,' '              ;
                jmp     @@bt_h_err_msg                          ;
panic_ioerr     label   near                                    ; read errror
                mov     si,offset errmsg_ioerr                  ;
                jmp     @@bt_h_err_msg                          ;
panic_nofile    label   near                                    ; no OS2LDR
                mov     si,offset errmsg_nofile                 ;
@@bt_h_err_msg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@bt_h_err                              ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@bt_h_err_msg                          ;
@@bt_h_err:
                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
;----------------------------------------------------------------
; strlen
; in : es:di - string
; out: ax - len, all regs saved
strlen          label   near
                push    cx                                      ;
                push    di                                      ;
                xor     cx,cx                                   ;
                or      di,di                                   ;
                jz      @@strlen_null                           ;
                dec     cx                                      ;
                xor     al,al                                   ;
                cld                                             ;
          repne scasb                                           ;
                not     cx                                      ;
                dec     cx                                      ;
@@strlen_null:
                mov     ax,cx                                   ;
                pop     di                                      ;
                pop     cx                                      ;
                ret                                             ;
;----------------------------------------------------------------
; read sector
; in    : eax - start, cl - count, es:bx - target
;         function cannot cross segment border!
; saves : dx, si, di, bp
; return: es:bx - points to the end of data
;         CF set on error (in micro-FSD mode only, else panic)
read_sector     label   near
@@bt_h_sloop:
                mov     ch,NUM_RETRIES                          ;
@@bt_h_rloop:
                pushad                                          ;
                mov     esi,bpb_ptstart                         ;
                xor     edx,edx                                 ;
                push    edx                                     ;
                mov     di,sp                                   ;
                cmp     i13x_on,dh                              ; cmp with 0
                jz      @@bt_h_no_ext_read                      ;
                add     eax,esi                                 ;
                rcl     edx,1                                   ; ;)
                push    edx                                     ;
                push    eax                                     ; pos to read
                push    es                                      ; target addr
                push    bx                                      ;
                xor     ch,ch                                   ;
                push    cx                                      ; # of sectors
                push    10h                                     ;
                mov     ah,42h                                  ;
                mov     si,sp                                   ;
                mov     [di],cl                                 ; read cl sectors
                jmp     @@bt_h_read_call                        ;
@@bt_h_no_ext_read:
                inc     byte ptr [di]                           ; read 1 sector
                add     eax,esi                                 ; sector #
                movzx   ecx,bpb_spt                             ;
                div     ecx                                     ;
                inc     dl                                      ;
                mov     cl,dl                                   ;
                mov     edx,eax                                 ;
                shr     edx,10h                                 ;
                div     bpb_numheads                            ;
                xchg    dl,dh                                   ;
                mov     ch,al                                   ;
                shl     ah,6                                    ;
                or      cl,ah                                   ;
                mov     ax,201h                                 ;
@@bt_h_read_call:
                mov     dl,bpb_disk                             ;
                int     13h                                     ;
                mov     sp,di                                   ;
                pop     ecx                                     ;
                jc      @@bt_h_read_err                         ;
                mov     di,sp                                   ;
                sub     word ptr [di].pa_ecx,cx                 ; upd. sector count
                add     [di].pa_eax,ecx                         ; and pos
                shl     cx,SECTSHIFT                            ; and offset in bx
                add     word ptr [di].pa_ebx,cx                 ;
                popad                                           ; esp restored here
                or      cl,cl                                   ;
                jnz     @@bt_h_sloop                            ;
                ret                                             ; CF cleared after "or"
@@bt_h_read_err:
                popad                                           ; esp restored here
                dec     ch                                      ; # retries
                jnz     @@bt_h_rloop                            ;
                or      ch,fsd_mode                             ; we`re in micro-FSD mode?
                jz      panic_ioerr                             ; error if still not
                stc                                             ;
                ret                                             ;

; ---------------------------------------------------------------
; read service sectors
; in    : eax - sector number
; out   : CF - error
; saves : all, except ax

; read 4 sectors
read_four       label   near                                    ;
                push    cx                                      ;
                mov     cl,4                                    ;
                mov     cb_lsn,eax                              ; save it!!
                jmp     @@read_ofs0                             ;
; read sector to BUF + 512
read_one_1      label   near                                    ;
                push    cx                                      ;
                mov     cl,1                                    ;
                push    bx                                      ;
                mov     bx,512                                  ;
                jmp     @@read_proc                             ;
; read sector to BUF
read_one        label   near                                    ;
                push    cx                                      ;
                mov     cl,1                                    ;
@@read_ofs0:
                push    bx                                      ;
                xor     bx,bx                                   ;
@@read_proc:
                push    es                                      ;
                add     bx,BUFFER_POS                           ;
                push    cs                                      ;
                pop     es                                      ;
                call    read_sector                             ;
                pop     es                                      ; CF must be
                pop     bx                                      ; preserved
                pop     cx                                      ;
                ret                                             ;

;----------------------------------------------------------------
; normalize pointer in si:di to seg:000x form (i.e. 64k-16 adressing)
normalize       label   near                                    ;
                push    ax                                      ;
                mov     ax,di                                   ;
                and     di,PARAMASK                             ;
                shr     ax,PARASHIFT                            ;
                add     si,ax                                   ;
                pop     ax                                      ;
                ret                                             ;

; ---------------------------------------------------------------

errmsg_ioerr    db      'Disk i/o error.',0                     ;
miniFSD_name    db      'OS2BOOT'                               ;
miniFSD_nzero   db      0                                       ;
errmsg_nomini   db      'is missing.',0                         ;
errmsg_nofile   db      'Unable to find '                       ;
_hpfs_bsname:
                db      'OS2LDR',0,0,0,0,0,0,0,0,0,0            ; 15 chars + 0

@@bt_h_end:
bt_h_reserved   equ     510 - (@@bt_h_end - _hpfs_bsect)        ;
if bt_h_reserved GT 0
                db      bt_h_reserved dup (0)                   ;
endif
                dw      0AA55h                                  ;
; ---------------------------------------------------------------
;   2nd sector
; ---------------------------------------------------------------
main_code       label   near
                mov     bx,BUFFER_POS                           ;
                xor     eax,eax                                 ;
                mov     al,16                                   ; read super block
                call    read_one                                ;
                mov     eax,[bx+12]                             ; read root fnode
                call    read_one                                ;

                mov     eax,[bx+48h]                            ;
                mov     root_dirblk,eax                         ;

                mov     di,offset miniFSD_name                  ;
                mov     si,MFSD_SEG                             ;
                call    load_file                               ;
                jc      panic_nomini                            ;
                mov     eax,cf_size                             ; and save size to
                mov     et_mfsdlen,eax                          ; filetable

                mov     di,offset _hpfs_bsname                  ;
                mov     si,BOOT_SEG                             ; loads it
                call    load_file                               ;
                jc      panic_nofile                            ;
                mov     eax,cf_size                             ; and save size to
                mov     et_ldrlen,eax                           ; filetable

                mov     et_museg,cs                             ; update filetable
                mov     et_muOpenSeg,cs                         ;
                mov     et_muReadSeg,cs                         ;
                mov     et_muCloseSeg,cs                        ;
                mov     et_muTermSeg,cs                         ;

                mov     stack_pos,sp                            ; save stack pos
                mov     fsd_mode,1                              ; switch i/o to FSD mode

                xor     eax,eax                                 ;
                mov     cf_size,eax                             ; "close file"

                push    BOOT_SEG                                ; loader address
                push    ax                                      ;
                mov     dh,BF_MINIFSD or BF_MICROFSD            ; boot flags
                mov     dl,bpb_disk                             ;
                mov     si,BOOT_OEM_LEN                         ; ds:si - BPB
                push    cs                                      ;
                pop     es                                      ;
                mov     di,offset ftab                          ; es:di - filetable
                retf                                            ;

;----------------------------------------------------------------
; read sectors (can be > 64k)
; in : es:bx - target, eax - start sector, cx - number of sectors
; out: CF - error, es:bx updated (if CF=0), other regs saved
read_long       label   near                                    ;
                pushad                                          ;
                mov     si,es                                   ;
                mov     di,bx                                   ;
                call    normalize                               ;
                mov     es,si                                   ;
                mov     bx,di                                   ;

                mov     dx,cx                                   ;
@@rl_loop:
                mov     cx,dx                                   ;
                cmp     cx,128-1                                ; sectors in 64k-16
                jc      @@rl_less64k                            ;
                mov     cx,128-1                                ;
@@rl_less64k:
                push    eax                                     ;
                push    bx                                      ;
                push    cx                                      ;
                call    read_sector                             ;
                pop     cx                                      ;
                pop     bx                                      ;
                pop     eax                                     ;
                jc      @@rl_errexit                            ;
                sub     dx,cx                                   ;
                movzx   ecx,cx                                  ;
                add     eax,ecx                                 ;
                shl     cx,SECTSHIFT-PARASHIFT                  ;
                mov     si,es                                   ;
                add     si,cx                                   ;
                mov     es,si                                   ;
                or      dx,dx                                   ;
                jnz     @@rl_loop                               ;
                mov     bp,sp                                   ; update bx
                mov     word ptr [bp].pa_ebx,bx                 ;
@@rl_ok:
                clc                                             ;
@@rl_errexit:
                popad                                           ;
                ret                                             ;

; ---------------------------------------------------------------
; find file
; in    : [di] - file name
; out   : CF - error, [bx] - dirent of req. file
; saves : dx,di,bp
;
file_find       label   near                                    ;
                mov     eax,root_dirblk                         ;
                call    read_four                               ;
                jc      @@ff_errexit                            ;
                push    cs                                      ; update es
                pop     es                                      ; for strlen
                call    strlen                                  ;
                mov     cx,ax                                   ;
@@ff_start:
                mov     bx,BUFFER_POS + 14h                     ; hpfs_dirblk.startb
                jmp     @@ff_cmpname
@@ff_nextrec:
                add     bx,[bx]                                 ; hpfs_dirent.recsize
@@ff_cmpname:
                cmp     cl,[bx+1Eh]                             ; hpfs_dirent.namelen
                jnz     @@ff_notmatch                           ; cmp length
                push    cx                                      ;
                push    di                                      ;
                lea     si,[bx+1Fh]                             ; hpfs_dirent.name
@@ff_cmploop:
                lodsb                                           ;
                cmp     al,'a'                                  ;
                jc      @@ff_cmp2                               ;
                cmp     al,'z'                                  ;
                ja      @@ff_cmp2                               ;
                sub     al,20h                                  ;
@@ff_cmp2:
                scasb                                           ;
                loopz   @@ff_cmploop                            ;
                pop     di                                      ;
                pop     cx                                      ;
                jnz     @@ff_notmatch                           ;
                clc                                             ;
                ret                                             ;
@@ff_notmatch:
                test    byte ptr [bx+2],4                       ; hpfs_dirent.flags
                jz      @@ff_nodp                               ;  & HPFS_DF_BTP
                add     bx,[bx]                                 ; next entry
                mov     eax,[bx-4]                              ;
                call    read_four                               ;
                jc      @@ff_errexit                            ;
                jmp     @@ff_start                              ;
@@ff_nodp:
                test    byte ptr [bx+2],8                       ; HPFS_DF_END?
                jz      @@ff_nextrec                            ;

                mov     bx,BUFFER_POS                           ;
                test    byte ptr [bx+8],1                       ; hpfs_dirent.change
                jz      @@ff_retparent                          ;
@@ff_errexit:
                stc                                             ; top block -> no file
                ret                                             ;
@@ff_retparent:
                mov     esi,cb_lsn                              ; save current lsn
                mov     eax,[bx+12]                             ; hpfs_dirent.parent
                call    read_four                               ;
                jc      @@ff_errexit                            ;
                push    di                                      ;
                mov     bx,BUFFER_POS + 14h                     ;
                jmp     @@ff_srchstart                          ;
@@ff_srchpos:
                add     bx,di                                   ; next dirent
@@ff_srchstart:
                mov     di,[bx]                                 ; hpfs_dirent.recsize
                cmp     esi,[bx+di-4]                           ;
                jnz     @@ff_srchpos                            ;
                pop     di                                      ;
                jmp     @@ff_nodp                               ;

; ---------------------------------------------------------------
; current file offset to sector
; in    : eax - offset
; out   : CF - error, else eax - sector, edx - run length
; saves : all
;
fofs2sector     label   near
                push    bx                                      ;
                push    cx                                      ;
                push    si                                      ;
                shr     eax,SECTSHIFT                           ;
                mov     bx,BUFFER_POS + 38h                     ; hpfs_fnode.fst.alb
@@f2s_start:
                test    byte ptr [bx],80h                       ; hpfs_alblk.flag & HPFS_ABF_NODE
                jnz     @@f2s_donode                            ;
                mov     cl,[bx+5]                               ; hpfs_alblk.used
                xor     ch,ch                                   ;
                jcxz    @@f2s_err_exit                          ;
                dec     cx                                      ;
                add     bx,8                                    ; size hpfs_alblk
                jcxz    @@f2s_found                             ;
@@f2s_searchloop:
                lea     si,[bx+12]                              ; size hpfs_alleaf
                cmp     eax,[si]                                ; hpfs_alleaf.log
                jc      @@f2s_found                             ;
                add     bx,12                                   ; size hpfs_alleaf
                loop    @@f2s_searchloop
@@f2s_found:
                sub     eax,[bx]                                ; hpfs_alleaf.log
                mov     edx,[bx+4]                              ; hpfs_alleaf.runlen
                sub     edx,eax                                 ;
                add     eax,[bx+8]                              ; hpfs_alleaf.phys
                clc                                             ;
@@f2s_exit:
                pop     si                                      ;
                pop     cx                                      ;
                pop     bx                                      ;
                ret                                             ;
@@f2s_err_exit:
                stc                                             ;
                jmp     @@f2s_exit                              ;
@@f2s_donode:
                mov     cl,[bx+5]                               ; hpfs_alblk.used
                xor     ch,ch                                   ;
                add     bx,8                                    ; size hpfs_alblk
@@f2s_nodeloop:
                cmp     eax,[bx]                                ; hpfs_alnode.log
                jc      @@f2s_nodefound                         ;
                add     bx,8                                    ; size hpfs_alnode
                loop    @@f2s_nodeloop                          ;
                jmp     @@f2s_err_exit                          ; didn't find it!
@@f2s_nodefound:
                push    eax                                     ;
                mov     eax,[bx+4]                              ; hpfs_alnode.phys
                call    read_one_1                              ;
                pop     eax                                     ;
                jc      @@f2s_err_exit                          ;
                mov     bx,BUFFER_POS + 512 + 12                ; hpfs_alsec.alb
                jmp     @@f2s_start                             ;

; ---------------------------------------------------------------
; read/copy partial sector
; in    : eax - sector, cx - length (neg value - from the end),
;         si:di - normalized target, ebx - total length (limit for cx too),
;         ebp - offset
; out   : ebx, ebp, si:di - updated, CF set on error
; saves : all
read_partial    label   near                                    ;
                push    es                                      ;
                push    ecx                                     ;
                push    cs                                      ;
                pop     es                                      ;
                push    bx                                      ;
                mov     bx,BUFFER_POS + 512*2                   ;
                push    cx                                      ;
                mov     cx,1                                    ;
                call    read_long                               ;
                pop     cx                                      ;
                pop     bx                                      ;
                jc      @@rdp_exit                              ;
                mov     es,si                                   ;
                mov     si,BUFFER_POS + 512*2                   ;
                cld                                             ;
                or      cx,cx                                   ;
                jns     @@rdp_checklen                          ;
                neg     cx                                      ;
                add     si,512                                  ;
                sub     si,cx                                   ;
@@rdp_checklen:
                movzx   ecx,cx                                  ; check total len
                cmp     ebx,ecx                                 ;
                jnc     @@rdp_copy                              ;
                mov     ecx,ebx                                 ;
@@rdp_copy:
                sub     ebx,ecx                                 ; upd. total len
                add     ebp,ecx                                 ; & offset
            rep movsb                                           ;
                mov     si,es                                   ;
                clc                                             ;
@@rdp_exit:
                pop     ecx                                     ;
                pop     es                                      ;
                ret                                             ;

; ---------------------------------------------------------------
; in    : eax - offset, ecx - length, si:di - target location
; out   : eax - number of actually readed, CF set on error
; saves : none
;
read_file       label   near
                cmp     eax,cf_size                             ; check pos &
                jae     @@rdf_errexit                           ; length
                lea     ebx,[eax+ecx]                           ;
                sub     ebx,cf_size                             ;
                jbe     @@rdf_lenok                             ;
                sub     ecx,ebx                                 ; fix length
@@rdf_lenok:
                or      ecx,ecx                                 ;
                jz      @@rdf_zeroexit                          ; zero len?

                mov     ebp,eax                                 ; offset
                mov     ebx,ecx                                 ; length
                mov     cf_readlen,ecx                          ;
                call    normalize                               ;
                call    fofs2sector                             ;
                jc      @@rdf_errexit                           ;
; out   : CF - error, else eax - sector, edx - run length
                mov     ecx,ebp                                 ;
                and     ecx,SECTMASK                            ; start is aligned
                jz      @@rdf_longread                          ; to sector?
                sub     cx,SECTSIZE                             ;
                call    read_partial                            ;
                jc      @@rdf_errexit                           ;
                dec     edx                                     ; runlen--
                inc     eax                                     ; sector++
@@rdf_longread:
                cmp     ebx,SECTSIZE                            ;
                jc      @@rdf_remain                            ;
                or      edx,edx                                 ;
                jnz     @@rdf_lr_runok                          ;
                mov     eax,ebp                                 ; get next run
                call    fofs2sector                             ;
                jc      @@rdf_errexit                           ;
@@rdf_lr_runok:
                mov     ecx,ebx                                 ;
                shr     ecx,SECTSHIFT                           ; # of sectors
                cmp     edx,ecx                                 ; to read at time
                jnc     @@rdf_lr_runsize                        ;
                mov     ecx,edx                                 ;
@@rdf_lr_runsize:
                push    ebx                                     ;
                push    ecx                                     ;
                mov     es,si                                   ;
                mov     bx,di                                   ;
                call    read_long                               ;
                mov     si,es                                   ;
                mov     di,bx                                   ;
                pop     ecx                                     ;
                pop     ebx                                     ;
                jc      @@rdf_errexit                           ;
                sub     edx,ecx                                 ; runlen-=cnt
                add     eax,ecx                                 ; sector+=cnt
                shl     ecx,SECTSHIFT                           ;
                sub     ebx,ecx                                 ; total len
                add     ebp,ecx                                 ; & offset
                jmp     @@rdf_longread                          ;
@@rdf_remain:
                or      ebx,ebx                                 ; end is sector
                jz      @@rdf_exit                              ; aligned?
                or      edx,edx                                 ;
                jnz     @@rdf_rem_runok                         ;
                mov     eax,ebp                                 ; get next run
                call    fofs2sector                             ;
                jc      @@rdf_errexit                           ;
@@rdf_rem_runok:
                mov     cx,bx                                   ;
                call    normalize                               ;
                call    read_partial                            ;
                jc      @@rdf_errexit                           ;
@@rdf_exit:
                mov     eax,cf_readlen                          ;
                clc                                             ;
                ret                                             ;
@@rdf_zeroexit:
                xor     eax,eax                                 ; valid zero read
                ret                                             ;
@@rdf_errexit:
                xor     eax,eax                                 ; exit with CF set
                stc                                             ;
                ret                                             ;

; ---------------------------------------------------------------
; in    : [di] - file name, si - segment
; out   : CF - error
; saves : none
;
load_file       label   near                                    ;
                push    si                                      ;
                call    open_file                               ;
                pop     si                                      ;
                jc      @@lf_exit                               ;
                xor     eax,eax                                 ;
                xor     di,di                                   ;
                mov     ecx,cf_size                             ;
                call    read_file                               ;
@@lf_exit:
                ret                                             ;

; ---------------------------------------------------------------
; in    : [di] - file name
; out   : CF - error, else ax - result, cf_size - file size
; saves : none
;
open_file       label   near
                call    file_find                               ;
                jc      @@opnf_err                              ;
                mov     eax,[bx+12]                             ; hpfs_dirent.fsize
                mov     cf_size,eax                             ;
                mov     eax,[bx+4]                              ; hpfs_dirent.fnode
                call    read_one                                ;
                jc      @@opnf_err                              ;
                xor     ax,ax                                   ; CF is 0
                ret                                             ;
@@opnf_err:
                mov     ax,5                                    ; CF is 1
                ret                                             ;

; ---------------------------------------------------------------
; open();
;
mfsd_open       label   near
                xor     eax,eax                                 ; zero file size
                mov     cf_size,eax                             ;
                les     di,dword ptr user_sp                    ;
                lfs     si,dword ptr es:[di+4]                  ;
                les     di,dword ptr es:[di+4]                  ; length or file
                call    strlen                                  ; name
                push    cs                                      ;
                pop     es                                      ;
                mov     bp,sp                                   ;
                inc     ax                                      ;
                cmp     ah,1                                    ; name too long?
                cmc                                             ; (send CF=1 to exit)
                jc      @@mfs_open_exit                         ;
                sub     sp,ax                                   ;
                mov     cx,ax                                   ;
                mov     di,sp                                   ; copy name to own
                cld                                             ; stack (we have 4k
            rep movs    byte ptr es:[di],byte ptr fs:[si]       ; in it)
                mov     di,sp                                   ;

                call    open_file                               ;
                jc      @@mfs_open_exit                         ;

                les     di,dword ptr user_sp                    ;
                les     di,dword ptr es:[di+8]                  ; length or file
                mov     eax,cf_size                             ;
                stosd                                           ;
                xor     ax,ax                                   ;
@@mfs_open_exit:
                mov     sp,bp                                   ;
                ret                                             ;
; ---------------------------------------------------------------
; read();
;
mfsd_read       label   near
                lfs     si,dword ptr user_sp                    ;
                cld                                             ;
                lods    dword ptr fs:[si]                       ; skip ret addr
                lods    dword ptr fs:[si]                       ; file pos
                mov     ecx,fs:[si+4]                           ; # of bytes to read
                mov     di,fs:[si]                              ; si:di - target
                mov     si,fs:[si+2]                            ; pointer
                call    read_file                               ;
                push    eax                                     ;
                pop     ax                                      ;
                pop     dx                                      ;
                ret                                             ;

; ---------------------------------------------------------------
; close();
;
mfsd_close      label   near                                    ;
                xor     eax,eax                                 ; zero size to
                mov     cf_size,eax                             ; prevent read-after-close
                ret                                             ;

micro_open:
                mov     ax,offset mfsd_open                     ;
                jmp     micro_start
micro_read:
                mov     ax,offset mfsd_read                     ;
                jmp     micro_start
micro_close:
                mov     ax,offset mfsd_close
                jmp     micro_start
micro_terminate:
                retf
micro_start:
                mov     dx,cs                                   ;
                mov     cs:user_ds,ds                           ;
                mov     ds,dx                                   ;
                mov     user_ss,ss                              ;
                mov     user_sp,sp                              ;
                cmp     user_ss,dx                              ; these censored calls
                setnz   stack_rest                              ; us with OUR stack?
                jz      @@entry_no_stack_set                    ;
                mov     ss,dx                                   ; it not -
                mov     sp,stack_pos                            ; switching to it
@@entry_no_stack_set:
                push    es                                      ;
                push    fs                                      ;
                mov     es,dx                                   ;
                pushad                                          ; ds=es=ss -> self

                call    ax
micro_exit:
                mov     bp,sp                                   ;
                mov     word ptr [bp].pa_edx,dx                 ; save result
                mov     word ptr [bp].pa_eax,ax                 ;
                popad                                           ;
                pop     fs                                      ;
                pop     es                                      ;
                test    stack_rest,1                            ;
                jz      @@entry_no_stack_rest                   ;
                lss     sp,dword ptr user_sp                    ;
@@entry_no_stack_rest:
                mov     ds,user_ds                              ;
                retf                                            ;

; ---------------------------------------------------------------
; micro-fsd export table
ftab            label   near
                dw      3
                dw      BOOT_SEG
et_ldrlen       dd      0
et_museg        dw      0
et_mulen        dd      SELF_SEG_LEN shl 10
                dw      MFSD_SEG
et_mfsdlen      dd      0
                dw      0
                dd      0
                dw      offset micro_open
et_muOpenSeg    dw      0
                dw      offset micro_read
et_muReadSeg    dw      0
                dw      offset micro_close
et_muCloseSeg   dw      0
                dw      offset micro_terminate
et_muTermSeg    dw      0
                dd      0
                dd      0
; ---------------------------------------------------------------

@@bt_h2_end:
bt_h2_reserved  equ     512 - ((@@bt_h2_end - main_code) and 511) ;
if bt_h2_reserved GT 0
                db      bt_h2_reserved dup (0)                  ;
endif
hpfs_boot_total label   near


HPFSBOOT_CODE   ends
                end
