;
; QSINIT
; CDFS (El Torito) boot code (micro-FSD)
;
;  * ignore multi-extent files
;  * assumes Primary Volume Descriptor as the first on disk
;  * ignore Joliet (i.e. only ISO names up to 31 char is possible)
;  * incoming names must be in UPPER case. This bug was made in IBM HPFS
;    micro, so no reason to upper it anywhere in micro-FSDs.
;  * there is no mini-FSD at all. I.e. direct boot is impossible, only
;    switching to ram disk boot, for example.
;

                include inc/qstypes.inc
                include inc/filetab.inc
                include inc/seldesc.inc

                PRINT_FNAME  = 0                                ; print file name (debug)

                NUM_RETRIES  = 8                                ; sometimes it helps ;)
                STACK_SIZE   = 4096                             ;
                SECTOR_SIZE  = 2048                             ;
                CD_SECTSHIFT = 11                               ;
                SELF_SEG_LEN = (SECTOR_SIZE*3 + STACK_SIZE) shr 10 ;
                BOOT_SEG     = 1000h                            ; OS2LDR location
                DIRBUF_OFS   = SECTOR_SIZE                      ;
                DATABUF_OFS  = (SECTOR_SIZE*2)                  ;
                SECTOR_STEP  = (0FFFFh shr CD_SECTSHIFT)        ; < 64k

cdfs_dir_entry  struc                                           ; CDFS dir entry
cdd_DirLen      db     ?                                        ;
cdd_XARLen      db     ?                                        ;
cdd_FileLoc     dd     ?                                        ;
                dd     ?                                        ;
cdd_DataLen     dd     ?                                        ;
                dd     ?                                        ;
cdd_RecTime     db     6 dup (?)                                ;
cdd_FlagsHSG    db     ?                                        ;
cdd_FlagsISO    db     ?                                        ;
cdd_IntLvSize   db     ?                                        ;
cdd_IntLvSkip   db     ?                                        ;
cdd_VSSN        dw     ?                                        ;
                dw     ?                                        ;
cdd_FileIdLen   db     ?                                        ;
cdfs_dir_entry  ends                                            ;

ISO_ATTR_MULTI  equ    80h                                      ;
ISO_ATTR_DIR    equ    02h                                      ;


DGROUP GROUP    _TEXT,_DATA,_STACK                              ;

_STACK          segment word public USE16 'STACK'
_STACK          ends

_DATA           segment para public USE16 'DATA'
_DATA           ends

_TEXT           segment byte public USE16 'CODE'
                assume  cs:_TEXT, ds:_TEXT, es:nothing, ss:_TEXT

                org     0h

                public  cdb_start                               ;
cdb_start:
                cli                                             ;
                call    @@get_offs                              ;
@@get_offs:
                pop     di                                      ;
                lea     si,[di+SECTOR_SIZE+STACK_SIZE-(@@get_offs-cdb_start)]  ;
                cmp     si,di                                   ;
                jnc     @@offset_ok
@@err_loop:
                sti                                             ; we want 4k
                int     19h                                     ; above sector
                hlt                                             ; for stack
                jmp     @@err_loop                              ;
@@offset_ok:
                mov     ax,cs                                   ;
                mov     ds,ax                                   ;
                mov     ss,ax                                   ;
                mov     sp,si                                   ;
                sti
                push    di                                      ;
                push    dx                                      ;
                int     12h                                     ; get memory for
                pop     dx                                      ; OS2LDR +  micro
                sub     ax,64 + SELF_SEG_LEN                    ;
                and     ax,0FFF0h                               ; round down
                shl     ax,6                                    ; kb -> para
                mov     es,ax                                   ;
                cld                                             ;
                xor     di,di                                   ;
                pop     si                                      ;
                sub     si,@@get_offs-cdb_start                 ;
                mov     cx,SECTOR_SIZE shr 1                    ;
            rep movsw                                           ;
                push    ax                                      ;
                push    offset @@offset_code                    ;
                retf                                            ;
@@offset_code:
                mov     ds,ax                                   ; here we have
                mov     ss,ax                                   ; normal env. in
                mov     sp,SELF_SEG_LEN shl 10                  ; the top of memory
                mov     disk_number,dl                          ;

                mov     es,ax                                   ; read primary
                mov     eax,10h                                 ; volume descriptor
                mov     cl,1                                    ;
                mov     bx,DIRBUF_OFS                           ; just ignore any
                push    bx                                      ; "CD001" checks, user

                call    read_sector                             ; should care of it
                pop     bx                                      ;
                mov     eax,[bx+9Eh]                            ; root dir loc
                mov     rootdir_loc,eax                         ;
                mov     eax,[bx+0A6h]                           ; root dir size

                add     eax,SECTOR_SIZE-1                       ;
                shr     eax,CD_SECTSHIFT                        ;
                mov     rootdir_len,ax                          ; up to 64k sectors

                or      ax,ax                                   ; just a check
                jz      panic_nofile                            ;

                mov     di,offset cd_bsname                     ;
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
                mov     byte ptr fsd_mode,1                     ; switch i/o to FSD mode

                xor     eax,eax                                 ;
                mov     cf_size,eax                             ; "close file"

                push    BOOT_SEG                                ; loader address
                push    ax                                      ;
                mov     dh,BF_NOMFSHVOLIO or BF_MICROFSD        ; boot flags
                mov     dl,disk_number                          ;
                xor     si,si                                   ; ds:si - NO BPB
                push    cs                                      ;
                pop     es                                      ;
                mov     di,offset ftab                          ; es:di - filetable
                retf                                            ;

;----------------------------------------------------------------
; read sector
; in    : eax - start, cl - count, es:bx - target
;         function cannot cross segment border!
; saves : edx, esi, edi, ebp
; return: es:bx - points to the end of data
;         CF set on error (in micro-FSD mode only, else panic)
read_sector     label   near
@@cdb_sloop:
                mov     ch,NUM_RETRIES                          ;
@@cdb_rloop:
                pushad                                          ;
                xor     edx,edx                                 ;
                push    edx                                     ;
                mov     di,sp                                   ;
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
                mov     dl,disk_number                          ;
                int     13h                                     ;
                mov     sp,di                                   ;
                pop     ecx                                     ;
                jc      @@cdb_read_err                          ;
                mov     di,sp                                   ;
                sub     word ptr [di].pa_ecx,cx                 ; upd. sector count
                add     [di].pa_eax,ecx                         ; and pos
                shl     cx,CD_SECTSHIFT                         ; and offset in bx
                add     word ptr [di].pa_ebx,cx                 ;
                popad                                           ; esp restored here
                or      cl,cl                                   ;
                jnz     @@cdb_sloop                             ;
                ret                                             ; CF cleared after "or"
@@cdb_read_err:
                popad                                           ; esp restored here
                dec     ch                                      ; # retries
                jnz     @@cdb_rloop                             ;
                or      ch,fsd_mode                             ; we`re in micro-FSD mode?
                jz      panic_ioerr                             ; error if still not
                stc                                             ;
                ret                                             ;

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
                cmp     cx,SECTOR_STEP                          ; sectors in 64k-16
                jc      @@rl_less64k                            ;
                mov     cx,SECTOR_STEP                          ;
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
                shl     cx,CD_SECTSHIFT-PARASHIFT               ;
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
; find file
; in    : [di] - file name
; out   : CF=1 - error/no file, CF=0 - cf_sector/cf_size is valid
; saves : dx,di,bp
;
file_find       label   near                                    ;
                push    di                                      ;
                push    bp                                      ;
                push    cs                                      ; update es
                pop     es                                      ; for strlen
                call    strlen                                  ;
                mov     an_len,ax                               ;

                mov     ax,rootdir_len                          ; loop var
                mov     rd_len,ax                               ;
                mov     ebp,rootdir_loc                         ;
                cmp     ebp,dir_sector                          ;
                jz      @@ff_loaded                             ;
@@ff_loop:
                mov     eax,ebp                                 ; es=cs here
                mov     cl,1                                    ;
                mov     bx,DIRBUF_OFS                           ;
                call    read_sector                             ;
                jc      @@ff_exit                               ; dir read error
                mov     dir_sector,ebp                          ;
@@ff_loaded:
                mov     si,DIRBUF_OFS                           ;
                cld                                             ;
@@ff_nameloop:
                mov     al,[si].cdd_DirLen                      ;
                or      al,al                                   ; zero len?
                jz      @@ff_nextsector                         ; then next sector

                test    [si].cdd_FlagsISO,ISO_ATTR_MULTI or ISO_ATTR_DIR
                jnz     @@ff_nextrec                            ;
                movzx   cx,[si].cdd_FileIdLen                   ;
                jcxz    @@ff_nextrec                            ;
                cmp     cx,an_len                               ; asked name is
                jc      @@ff_nextrec                            ; longer
                lea     bx,[si+1].cdd_FileIdLen                 ;
                jz      @@ff_cmpstart                           ;
                push    di                                      ; name is longer,
                mov     di,an_len                               ; check for ; and .
                cmp     byte ptr [bx][di],';'                   ;
                jz      @@ff_length_ok                          ;
                cmp     byte ptr [bx][di],'.'                   ; trailing dot,
                jnz     @@ff_nextrec_di                         ; should be the last
                inc     di                                      ; character
                cmp     di,cx                                   ;
                jz      @@ff_length_ok                          ; NAME.
                cmp     byte ptr [bx][di],';'                   ;
                jnz     @@ff_nextrec_di                         ; NAME.;1
@@ff_length_ok:
                pop     di                                      ;
@@ff_cmpstart:
;cli
;@@llll:
;jmp @@llll
                push    di                                      ;
                mov     cx,an_len                               ;
@@ff_cmploop:
                mov     al,[bx]                                 ;
                cmp     al,'a'                                  ;
                jc      @@ff_cmp2                               ;
                cmp     al,'z'                                  ;
                ja      @@ff_cmp2                               ;
                sub     al,20h                                  ;
@@ff_cmp2:
                inc     bx                                      ;
                scasb                                           ;
                loopz   @@ff_cmploop                            ;
                pop     di                                      ;
                jnz     @@ff_nextrec                            ;

                mov     eax,[si].cdd_FileLoc                    ; file location
                movzx   ecx,[si].cdd_XARLen                     ;
                add     eax,ecx                                 ;
                mov     cf_sector,eax                           ;
                mov     eax,[si].cdd_DataLen                    ;
                mov     cf_size,eax                             ;
                clc                                             ;
                jmp     @@ff_exit                               ;
@@ff_nextrec_di:
                pop     di                                      ;
@@ff_nextrec:
                movzx   ax,[si].cdd_DirLen                      ;
                add     si,ax                                   ;
                cmp     si,DIRBUF_OFS+SECTOR_SIZE               ; overflow?
                jc      @@ff_nameloop                           ;
@@ff_nextsector:
                inc     ebp                                     ;
                dec     word ptr rd_len                         ; loop dir
                jnz     @@ff_loop                               ;
                stc                                             ; no file
@@ff_exit:
                pop     bp                                      ;
                pop     di                                      ;
                ret                                             ;

; ---------------------------------------------------------------
; current file offset to sector
; in    : eax - offset
; out   : CF - error, else eax - sector, edx - run length
; saves : all
;
fofs2sector     label   near
                shr     eax,CD_SECTSHIFT                        ;

                mov     edx,cf_size                             ;
                add     edx,SECTOR_SIZE-1                       ;
                shr     edx,CD_SECTSHIFT                        ;
                sub     edx,eax                                 ; set CF here on wrong ofs
                jc      @@f2s_exit                              ;
                add     eax,cf_sector                           ; CF=0
@@f2s_exit:
                ret

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
                mov     bx,DATABUF_OFS                          ;
                push    cx                                      ;
                mov     cx,1                                    ;
                call    read_long                               ;
                pop     cx                                      ;
                pop     bx                                      ;
                jc      @@rdp_exit                              ;
                mov     es,si                                   ;
                mov     si,DATABUF_OFS                          ;
                cld                                             ;
                or      cx,cx                                   ;
                jns     @@rdp_checklen                          ;
                neg     cx                                      ;
                add     si,SECTOR_SIZE                          ;
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
; in    : [di] - file name
; out   : CF - error, else ax - result, cf_size - file size, cf_sector - 1st sector
; saves : none
;
open_file       label   near
if PRINT_FNAME gt 0
                call    name_print                              ;
endif
                call    file_find                               ;
                jc      @@opnf_err                              ;
                xor     ax,ax                                   ; CF is 0
                ret                                             ;
@@opnf_err:
                mov     ax,5                                    ; CF is 1
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
; in    : eax - offset, ecx - length, si:di - target location
; out   : eax - number of actually readed, CF set on error
; saves : none
;
; function just copied from HPFS micro (PARTMGR module), so it has
; run-length support, which is void (since we disallow multi-extent files)
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
                and     ecx,SECTOR_SIZE-1                       ; start is aligned
                jz      @@rdf_longread                          ; to sector?
                sub     cx,SECTOR_SIZE                          ;
                call    read_partial                            ;
                jc      @@rdf_errexit                           ;
                dec     edx                                     ; runlen--
                inc     eax                                     ; sector++
@@rdf_longread:
                cmp     ebx,SECTOR_SIZE                         ;
                jc      @@rdf_remain                            ;
                or      edx,edx                                 ;
                jnz     @@rdf_lr_runok                          ;
                mov     eax,ebp                                 ; get next run
                call    fofs2sector                             ;
                jc      @@rdf_errexit                           ;
@@rdf_lr_runok:
                mov     ecx,ebx                                 ;
                shr     ecx,CD_SECTSHIFT                        ; # of sectors
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
                shl     ecx,CD_SECTSHIFT                        ;
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

; ---------------------------------------------------------------

micro_open:
                mov     ax,offset mfsd_open                     ;
                jmp     micro_start                             ;
micro_read:
                mov     ax,offset mfsd_read                     ;
                jmp     micro_start                             ;
micro_close:
                mov     ax,offset mfsd_close                    ;
                jmp     micro_start                             ;
micro_terminate:
                retf                                            ;
micro_start:
                mov     dx,cs                                   ;
                mov     cs:user_ds,ds                           ;
                mov     ds,dx                                   ;
                mov     user_ss,ss                              ;
                mov     user_sp,sp                              ;
                cmp     user_ss,dx                              ; these censored calls
                setnz   stack_rest                              ; us with OUR stack?
                jz      @@entry_no_stack_set                    ;
                mov     ss,dx                                   ; if not -
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
                test    byte ptr stack_rest,1                   ;
                jz      @@entry_no_stack_rest                   ;
                lss     sp,dword ptr user_sp                    ;
@@entry_no_stack_rest:
                mov     ds,user_ds                              ;
                retf                                            ;

if PRINT_FNAME gt 0
name_print      label   near
                mov     ah,2                                    ;
                mov     bh,0                                    ;
                mov     dh,24                                   ;
                mov     dl,4                                    ;
                int     10h                                     ;
                push    di                                      ;
                pop     si                                      ;
@@npr_loop:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@npr_ret                               ;
                mov     ah,14                                   ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@npr_loop                              ;
@@npr_ret:
                ret                                             ;
endif ; PRINT_FNAME
; ---------------------------------------------------------------
panic_ioerr     label   near                                    ; read errror
                mov     si,offset errmsg_ioerr                  ;
                jmp     @@cdb_err_msg                           ;
panic_nofile    label   near                                    ; no OS2LDR
                mov     si,offset errmsg_nofile                 ;
@@cdb_err_msg:
                lodsb                                           ;
                or      al,al                                   ;
                jz      @@cdb_err                               ;
                mov     ah,0Eh                                  ;
                mov     bx,7                                    ;
                int     10h                                     ;
                jmp     @@cdb_err_msg                           ;
@@cdb_err:
                xor     ax,ax                                   ;
                int     16h                                     ;
                int     19h                                     ;
; ---------------------------------------------------------------
; micro-fsd export table                                        ;
ftab            label   near                                    ;
                dw      3                                       ;
                dw      BOOT_SEG                                ;
et_ldrlen       dd      0                                       ;
et_museg        dw      0                                       ;
et_mulen        dd      SELF_SEG_LEN shl 10                     ;
                dw      0                                       ;
et_mfsdlen      dd      0                                       ;
                dw      0                                       ;
                dd      0                                       ;
                dw      offset micro_open                       ;
et_muOpenSeg    dw      0                                       ;
                dw      offset micro_read                       ;
et_muReadSeg    dw      0                                       ;
                dw      offset micro_close                      ;
et_muCloseSeg   dw      0                                       ;
                dw      offset micro_terminate                  ;
et_muTermSeg    dw      0                                       ;
                dd      0                                       ;
                dd      0                                       ;
; ---------------------------------------------------------------
; variables                                                     ;

rootdir_len     dw      0                                       ; in sectors!
an_len          dw      0                                       ;
rd_len          dw      0                                       ;
cf_readlen      dd      0                                       ;
dir_sector      dd      0                                       ; loaded dir sector
rootdir_loc     dd      0                                       ;
cf_size         dd      0                                       ; current file size
cf_sector       dd      0                                       ;
user_ds         dw      0                                       ;
user_sp         dw      0                                       ;
user_ss         dw      0                                       ;
stack_pos       dw      0                                       ;
fsd_mode        db      0                                       ;
stack_rest      db      0                                       ; restore stack on exit
disk_number     db      0                                       ;
errmsg_ioerr    db      'CD boot: disk read error.',0           ;
errmsg_nofile   db      'CD boot: unable to find '              ;
cd_bsname:
                db      'OS2LDR',0                              ; boot file name
@@cdb_end:
cdb_reserved    equ     SECTOR_SIZE - 2 - (@@cdb_end - cdb_start)   ;
if cdb_reserved GT 0
                db      cdb_reserved dup (0)                    ;
endif
                dw      0AA55h                                  ;

_TEXT           ends
                end
