;
; QSINIT EFI loader
; 64-bit part function index
;

;cc #ifndef QS_FN64_INDEX
;cc #define QS_FN64_INDEX

;u16t tm_calibrate(void);
          EFN_TMCALIBRATE   =  0                                ;
;u16t key_read(void);
          EFN_KEYREAD       =  1                                ;
;u16t key_wait(u32t seconds);
          EFN_KEYWAIT       =  2                                ;
;u32t tm_counter(void);
          EFN_TMCOUNTER     =  3                                ; not used
;u64t tm_getdate(void);
          EFN_TMGETDATE     =  4                                ;
;void tm_setdate(u32t dostime);
          EFN_TMSETDATE     =  5                                ;
;u16t mfs_open (u32t plen) {
          EFN_MFSOPEN       =  6                                ;
;u32t mfs_read (u32t offset, u32t buf, u32t readsize);
          EFN_MFSREAD       =  7                                ;
;u32t mfs_close(void);
          EFN_MFSCLOSE      =  8                                ;
;u32t mfs_term (void);
          EFN_MFSTERM       =  9                                ;
;u32t key_status(void);
          EFN_KEYSTATUS     = 10                                ;
;u8t  key_pressed(void);
          EFN_KEYPRESSED    = 11                                ;
;u32t vio_charout(char ch);
          EFN_VIOCHAROUT    = 12                                ;
;u32t vio_strout(const char *str);
          EFN_VIOSTROUT     = 13                                ;
;void vio_clearscr(void);
          EFN_VIOCLEARSCR   = 14                                ;
;void vio_setpos(u32t line, u32t col);
          EFN_VIOSETPOS     = 15                                ;
;void vio_getpos(u32t *line, u32t *col);
          EFN_VIOGETPOS     = 16                                ;
;void vio_setcolor(u16t color);
          EFN_VIOSETCOLOR   = 17                                ;
;u8t  vio_getmode(u32t *cols, u32t *lines);
          EFN_VIOGETMODE    = 18                                ;
;void vio_setmode(u32t lines);
          EFN_VIOSETMODE    = 19                                ;
;void init15mem(void);
          EFN_INIT15MEM     = 20                                ;
;void vio_showcursor(u32t state);
          EFN_VIOSHOWCURSOR = 21                                ;
;void exit_poweroff(u32t actiontype);
          EFN_EXITPOWEROFF  = 22                                ;
;u32t vio_enummodes(struct console_mode **);
          EFN_VIOENUMMODES  = 23                                ;
;u32t grf_enummodes(struct graphic_mode **mi, u32t *vmemptr, u32t *vmemsize);
          EFN_GRFENUMMODES  = 24                                ;
;int  vio_setmodeex(u32t cols, u32t lines);
          EFN_VIOSETMODEEX  = 25                                ;
;u32t grf_setmode(u32t mode);
          EFN_GRFSETMODE    = 26                                ;
;u32t grf_blit(u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch);
          EFN_GRFBLIT       = 27                                ;
;u32t grf_clear(u32t x, u32t y, u32t dx, u32t dy, u32t color);
          EFN_GRFCLEAR      = 28                                ;
;u32t dsk_io(struct qs_diskinfo *di, u64t start, u32t sectors, int write);
          EFN_DSKIO         = 29                                ;
;u64t hlp_setreg(u32t idx, u64t value);
;use FFFF64 to query value only (so, no way to send value -1 to control registers ;))
          EFN_HLPSETREG     = 30                                ;
;u32t XcptOn(void);
; return 0 in safe mode (not supported), else 1 (success)
          EFN_XCPTON        = 31                                ;
;void hlp_memmove(void *dst, void *src, u64t length, u32t usecli);
          EFN_MEMMOVE       = 32                                ;
;u32t TimerIrqsOn(void *apic, u8t tmr_vector, u8t spr_vector);
          EFN_TIMERIRQON    = 33                                ;

          EFN_FUNCCOUNT     = 34                                ;


console_mode    struc
conmode_x       dd      ?                 ; # of columns
conmode_y       dd      ?                 ; # of lines
conmode_number  dd      ?                 ; EFI mode number
conmode_res     dd      ?                 ; reserved
console_mode    ends

graphic_mode    struc
grfmode_x       dd      ?                 ; mode width
grfmode_y       dd      ?                 ; mode height
grfmode_number  dd      ?                 ; EFI mode number
grfmode_bpp     dd      ?                 ; mode bits per pixel
grfmode_mred    dd      ?                 ; color mask - red
grfmode_mgreen  dd      ?                 ; color mask - green
grfmode_mblue   dd      ?                 ; color mask - blue
grfmode_mres    dd      ?                 ; color mask - reserved
grfmode_pitch   dd      ?                 ; mode pitch
graphic_mode    ends

SETREG64_CR2    equ     2                 ;
SETREG64_CR3    equ     3                 ;
SETREG64_CR4    equ     4                 ;
SETREG64_CR8    equ     8                 ;

EFI_RESETCOLD   equ     0                 ;
EFI_RESETWARM   equ     1                 ;
EFI_SHUTDOWN    equ     2                 ;

xcpt64_data     struc
x64_cr0         dq      ?
x64_cr2         dq      ?
x64_cr3         dq      ?
x64_cr4         dq      ?
x64_ds          dq      ?
x64_es          dq      ?
x64_fs          dq      ?
x64_gs          dq      ?
x64_rax         dq      ?
x64_rbx         dq      ?
x64_rcx         dq      ?
x64_rdx         dq      ?
x64_rsi         dq      ?
x64_rdi         dq      ?
x64_rbp         dq      ?
x64_r8          dq      ?
x64_r9          dq      ?
x64_r10         dq      ?
x64_r11         dq      ?
x64_r12         dq      ?
x64_r13         dq      ?
x64_r14         dq      ?
x64_r15         dq      ?
x64_number      dq      ?                 ; exception number
x64_code        dq      ?                 ; exception error code
x64_rip         dq      ?
x64_cs          dq      ?
x64_rflags      dq      ?
x64_rsp         dq      ?
x64_ss          dq      ?
xcpt64_data     ends

;cc #endif // QS_FN64_INDEX
