;cc #ifndef QS_CONSOLE_GRAPHDEF
;cc #define QS_CONSOLE_GRAPHDEF

con_drawinfo_s  struc                           ;
cdi_FontX       db      ?                       ; font width
cdi_FontY       db      ?                       ; font height
cdi_FontMult    db      ?                       ; 1,2,3,4
cdi_Reserved    db      ?                       ;
cdi_CurMask     dd      ?                       ; cursor bit mask (by lines)
cdi_CurColor    dd      ?                       ; cursor color
cdi_vbpps       dd      ?                       ; bytes per pixel
con_drawinfo_s  ends                            ;

;cc typedef struct con_drawinfo_s   con_drawinfo;

;cc #endif // QS_CONSOLE_GRAPHDEF
