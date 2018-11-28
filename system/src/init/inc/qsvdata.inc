;
; QSINIT
; low level vio output definitions
;

;cc #ifndef QSINIT_VIO_INT
;cc #define QSINIT_VIO_INT

;cc #include "qstypes.h"
;cc #include "qcl/sys/qsvioint.h"
;cc #include "qcl/qslist.h"

;cc #ifdef __cplusplus
;cc extern "C" {
;cc #endif

SESDATA_SIGN    equ  4D424453h  ; SDBM
SESDATA_KBSIZE  equ        16   ;/< session keyboard buffer length
NUM_VIO_ROUTERS equ         4   ;/< number of system VIO routers
SYS_HANDLERS    equ        32   ;/< limited by vs_devmask width in session data

vio_session_s   struc
vs_sign         dd      ?
vs_x            dd      ?
vs_y            dd      ?
vs_devmask      dd      ?                       ;/< active on devices (0..31)
vs_flags        dd      ?                       ;/< VSF_* in qscon.h
vs_selfno       dd      ?                       ;/< session number
;ct qs_vh
vs_tb           dd      ?                       ;/< text mode buffer
;ct char*
vs_title        dd      ?                       ;/< title, in system heap block
;ct dd_list
vs_focus        dd      ?                       ;/< focus applications
vs_keybuf       dd      SESDATA_KBSIZE dup (?)
vs_keywp        db      ?                       ;/< write pos
vs_keyrp        db      ?                       ;/< read pos
vs_devdata      dw      SYS_HANDLERS dup (?)    ;/< word storage for every device (mode id actually)
vio_session_s   ends

;cc typedef struct vio_session_s        se_sysdata;

;cc // process info
vio_handler_s   struc
;ct char
vh_name         db      8 dup (?)
;cr    void  _std (*vh_setmode  )(u32t lines);
vh_setmode      dd      ?
;cr    int   _std (*vh_setmodeex)(u32t cols, u32t lines);
vh_setmodeex    dd      ?
;cr    void  _std (*vh_resetmode)(void);
vh_resetmode    dd      ?
;cr    u8t   _std (*vh_getmode  )(u32t *cols, u32t *lines);
vh_getmode      dd      ?
;cr    void  _std (*vh_getmfast )(u32t *cols, u32t *lines);
vh_getmfast     dd      ?

;cr    void  _std (*vh_intensity)(u8t value);
vh_intensity    dd      ?
;cr    void  _std (*vh_setshape )(u16t shape);
vh_setshape     dd      ?
;cr    u16t  _std (*vh_getshape )(void);
vh_getshape     dd      ?

;cr    void  _std (*vh_setcolor )(u16t color);
vh_setcolor     dd      ?
;cr    u16t  _std (*vh_getcolor )(void);
vh_getcolor     dd      ?
;cr    void  _std (*vh_clearscr )(void);
vh_clearscr     dd      ?
;cr    void  _std (*vh_setpos   )(u8t line, u8t col);
vh_setpos       dd      ?
;cr    void  _std (*vh_getpos   )(u32t *line, u32t *col);
vh_getpos       dd      ?
;cr    u32t  _std (*vh_ttylines )(s32t newvalue);
vh_ttylines     dd      ?

;cr    u32t  _std (*vh_charout  )(char ch);
vh_charout      dd      ?
;cr    u32t  _std (*vh_strout   )(const char *str);
vh_strout       dd      ?
;cr    void  _std (*vh_writebuf )(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
vh_writebuf     dd      ?
;cr    void  _std (*vh_readbuf  )(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch);
vh_readbuf      dd      ?

;cr    u16t  _std (*vh_kread    )(void);
vh_kread        dd      ?
;cr    u8t   _std (*vh_kpressed )(void);
vh_kpressed     dd      ?
;cr    u16t  _std (*vh_kwaitex  )(u32t ms, u32t *status);
vh_kwaitex      dd      ?
;cr    u32t  _std (*vh_kstatus  )(void);
vh_kstatus      dd      ?
;cr    u8t   _std (*vh_kpush    )(u16t code);
vh_kpush        dd      ?
vio_handler_s   ends

;cc typedef struct vio_handler_s  vio_handler;

;cc extern vio_handler* VHTable[NUM_VIO_ROUTERS];
;cc #pragma aux VHTable       "_*";

VHI_ACTIVE      equ     0          ;/< active VIO handler
VHI_CVIO        equ     1          ;/< start time plain vio output
VHI_BVIO        equ     2          ;/< base vio (router) with session support

;cc se_sysdata* _std se_dataptr(u32t sesno);

;cc #ifdef __cplusplus
;cc }
;cc #endif
;cc #endif // QSINIT_VIO_INT
