;
; QSINIT
; low level vio output api
;

;cc #ifndef QSINIT_VIO_INT
;cc #define QSINIT_VIO_INT

;cc #include "qstypes.h"

;cc #ifdef __cplusplus
;cc extern "C" {
;cc #endif

SESDATA_SIGN    equ     4D424453h  ; SDBM
SESDATA_KBSIZE  equ     16

vio_session_s   struc
vs_sign         dd      ?
vs_x            dd      ?
vs_y            dd      ?
vs_cx           dd      ?
vs_cy           dd      ?
vs_color        dw      ?
vs_shape        dw      ?
vs_lines        dd      ?
vs_flags        dd      ?
;ct void *
vs_data         dd      ?          ;/< mode handler`s data
vs_keybuf       dw      SESDATA_KBSIZE dup (?)
vs_keywp        db      ?          ;/< write pos
vs_keyrp        db      ?          ;/< read pos
vio_session_s   ends

VSF_GRAPHIC     equ     1          ;/< graphic mode
VSF_INTENSITY   equ     2          ;/< blink is off

;cc typedef struct vio_session_s  vio_session_data;

;cc // process info
vio_handler_s   struc
;ct char
vh_name         db      8 dup (?)
;cr    void  _std (*vh_init     )(u32t sesno);
vh_init         dd      ?
;cr    void  _std (*vh_fini     )(void);
vh_fini         dd      ?
;cr    vio_session_data* _std (*vh_savestate)(void);
vh_savestate    dd      ?
;cr    void  _std (*vh_reststate)(vio_session_data *state);
vh_reststate    dd      ?
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
;cr    void  _std (*vh_setshape )(u8t start, u8t end);
vh_setshape     dd      ?
;cr    u16t  _std (*vh_getshape )(void);
vh_getshape     dd      ?
;cr    void  _std (*vh_defshape )(u8t shape);
vh_defshape     dd      ?

;cr    void  _std (*vh_setcolor )(u16t color);
vh_setcolor     dd      ?
;cr    void  _std (*vh_clearscr )(void);
vh_clearscr     dd      ?
;cr    void  _std (*vh_setpos   )(u8t line, u8t col);
vh_setpos       dd      ?
;cr    void  _std (*vh_getpos   )(u32t *line, u32t *col);
vh_getpos       dd      ?
;cr    u32t* _std (*vh_ttylines )(void);
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
;cr    u16t  _std (*vh_kwait    )(u32t seconds);
vh_kwait        dd      ?
;cr    u32t  _std (*vh_kstatus  )(void);
vh_kstatus      dd      ?
;cr    u8t   _std (*vh_kpush    )(u16t code);
vh_kpush        dd      ?
vio_handler_s   ends

;cc typedef struct vio_handler_s  vio_handler;

;cc #ifdef __cplusplus
;cc }
;cc #endif
;cc #endif // QSINIT_VIO_INT
