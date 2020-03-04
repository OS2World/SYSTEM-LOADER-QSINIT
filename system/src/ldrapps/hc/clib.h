//
// QSINIT
// 32bit clib functions
// ----------------------------------------------------
// the difference between clib.h & stdlib.h is internal only
// (clib.h enum QSINIT module exports & stdlib.h - START).
//
#ifndef QSINIT_CLIB32
#define QSINIT_CLIB32

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** snprintf function.
    There is no floating point support on early stages (START module provides
    FP support).
    @code
    %b modified in comparation with default syntax:
       %12Lb  - print 12 qwords from specified address
       %12lb  - print 12 dwords from specified address
       %12hb  - print 12 shorts from specified address
       %#12b  - print 12 0xbytes from specified address
    @endcode
*/
int   __cdecl   snprintf(char *buf, u32t count, const char *fmt, ...);

/** printf function.
    @see snprintf */
int   __cdecl   printf  (const char *fmt, ...);

void* __stdcall memset  (void *dst, int c, u32t length);

int   __stdcall memcmp (const void *s1, const void *s2, u32t length);

int   __stdcall strncmp (const char *s1, const char *s2, u32t n);

/** strnicmp function.
    note, that current code page affects this! */
int   __stdcall strnicmp(const char *s1, const char *s2, u32t n);

u32t  __stdcall strlen  (const char *s);

char* __stdcall strncpy (char *dst, const char *src, u32t n);

char* __stdcall strcpy  (char *dst, const char *src);

char* __stdcall strchr  (const char *src, int c);

void* __stdcall memcpy  (void *dst, const void *src, u32t length);

/** same as memcpy() but copy words, not dwords internally.
    Function is for some rare alignment cases.
    length here in bytes! */
void* __stdcall memcpyw (void *dst, const void *src, u32t length);

void* __stdcall memmove (void *dst, const void *src, u32t length);

int   __stdcall toupper (int cc);

int   __stdcall tolower (int cc);

char* __stdcall _itoa   (int      value, char *buffer, int radix);
char* __stdcall _utoa   (unsigned value, char *buffer, int radix);
char* __stdcall _itoa64 (s64t     value, char *buffer, int radix);
char* __stdcall _utoa64 (u64t     value, char *buffer, int radix);
#define _ltoa   _itoa
#define _ultoa  _utoa
#define _ltoa64 _itoa64

/* a set of functions for byte/word/dword/qword search and fill;
   note, that memrchrX available for reverse search in stdlib.h */
void* __stdcall memchr  (const void*mem, int  chr, u32t buflen);
u8t*  __stdcall memchrnb(const u8t* mem, u8t  chr, u32t buflen);
u16t* __stdcall memchrw (const u16t*mem, u16t chr, u32t buflen);
u16t* __stdcall memchrnw(const u16t*mem, u16t chr, u32t buflen);
u32t* __stdcall memchrd (const u32t*mem, u32t chr, u32t buflen);
u32t* __stdcall memchrnd(const u32t*mem, u32t chr, u32t buflen);
u64t* __stdcall memchrq (const u64t*mem, u64t chr, u32t buflen);
u64t* __stdcall memchrnq(const u64t*mem, u64t chr, u32t buflen);

u16t* __stdcall memsetw (u16t *dst, u16t value, u32t length);
u32t* __stdcall memsetd (u32t *dst, u32t value, u32t length);
u64t* __stdcall memsetq (u64t *dst, u64t value, u32t length);

// a set of functions for dword/qword safe +/-/or/and/xor
u32t  __stdcall mt_safedadd(u32t *src, u32t value);
u32t  __stdcall mt_safedand(u32t *src, u32t value);
u32t  __stdcall mt_safedor (u32t *src, u32t value);
u32t  __stdcall mt_safedxor(u32t *src, u32t value);
u64t  __stdcall mt_safeqadd(u64t *src, u64t value);
u64t  __stdcall mt_safeqand(u64t *src, u64t value);
u64t  __stdcall mt_safeqor (u64t *src, u64t value);
u64t  __stdcall mt_safeqxor(u64t *src, u64t value);
/** cmpxchg public api.
    Analogue of win`s InterlockedCompareExchange() */
u32t  __stdcall mt_cmpxchgd(u32t volatile *src, u32t value, u32t cmpvalue);
/** cmpxchg8b public api.
    note: since this is direct cmpxchg8b usage - it will hang on 486dx, which
    is still supported as lowest cpu for QSINIT ;) */
u64t  __stdcall mt_cmpxchgq(u64t volatile *src, u64t value, u64t cmpvalue);

/** delay function (in mks).
    Uses simple calibrated loop, up to 4294 sec (enough for common use).

    MTLIB replaces it by own variant, which makes the same if delay is
    smaller, than half of timer tick, else yields time via mt_waitobject().
    
    Note for system code: this, also, mean, that MT lock state is
    unpredictable after this call and must be reset before it! */
void  __stdcall usleep  (u32t usec);

/** string to int conversion.
    Accepts spaces before, -, dec & hex values.

    This is init time internal call basically, better use atoi & strtol.
    But it useful because of missing octal conversion (function skips leading
    zeroes).
    See also str2ulong(), str2int64() and str2uint64() in stdlib.h */
long  __stdcall str2long(const char *str);

u32t  __stdcall crc32   (u32t crc, const u8t* buf, u32t len);

typedef struct {
   u32t      count[2];
   u32t       abcd[4];
   u8t        buf[64];
} md5_state;

void __stdcall  md5_init(md5_state *pms);
void __stdcall  md5_next(md5_state *pms, const void *data, u32t len);
/// assume that result size is 16 bytes
void __stdcall  md5_done(md5_state *pms, u8t *result);


u32t get_esp(void);
#pragma aux get_esp = "mov eax,esp" value [eax] modify exact [eax];

u16t get_flatcs(void);
#pragma aux get_flatcs =   \
    "xor     eax,eax"      \
    "mov     eax,cs"       \
    value [ax] modify exact [eax];

u16t get_flatds(void);
#pragma aux get_flatds =   \
    "xor     eax,eax"      \
    "mov     eax,ds"       \
    value [ax] modify exact [eax];

u16t get_flatss(void);
#pragma aux get_flatss =   \
    "xor     eax,eax"      \
    "mov     eax,ss"       \
    value [ax] modify exact [eax];

u16t get_taskreg(void);
#pragma aux get_taskreg = "str ax" value [ax] modify exact [eax];

u32t ints_enabled(void);
#pragma aux ints_enabled = \
    "pushfd"               \
    "pop     eax"          \
    "shr     eax,9"        \
    "and     eax,1"        \
    value [eax] modify exact [eax];

u32t _bittest(u32t src, u32t bit);
#pragma aux _bittest =     \
    "bt      ecx,eax"      \
    "setc    al"           \
    parm [ecx] [eax] value [eax] modify exact [eax];

// return previous bit value
u32t _bitset(u32t *dst, u32t bit);
#pragma aux _bitset =      \
    "bts     [ecx],eax"    \
    "setc    al"           \
    parm [ecx] [eax] value [eax] modify exact [eax];

u32t _bitreset(u32t *dst, u32t bit);
#pragma aux _bitreset =    \
    "btr     [ecx],eax"    \
    "setc    al"           \
    parm [ecx] [eax] value [eax] modify exact [eax];

u32t inp(u32t port);
#pragma aux inp =          \
    "xor     eax,eax"      \
    "in      al,dx"        \
    parm [edx] value [eax] modify exact [eax];

u32t inpw(u32t port);
#pragma aux inpw =         \
    "xor     eax,eax"      \
    "in      ax,dx"        \
    parm [edx] value [eax] modify exact [eax];

u32t inpd(u32t port);
#pragma aux inpd =         \
    "in      eax,dx"       \
    parm [edx] value [eax] modify exact [eax];

u32t outp(u32t port, u32t value);
#pragma aux outp =         \
    "out     dx,al"        \
    parm [edx] [eax] value [eax] modify exact [];

u32t outpw(u32t port, u32t value);
#pragma aux outpw =        \
    "out     dx,ax"        \
    parm [edx] [eax] value [eax] modify exact [];

u32t outpd(u32t port, u32t value);
#pragma aux outpd =        \
    "out     dx,eax"       \
    parm [edx] [eax] value [eax] modify exact [];

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CLIB32
