//
// QSINIT
// 32bit clib functions
//
#ifndef QSINIT_CLIB32
#define QSINIT_CLIB32

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** snprintf function.
    there is no floating point support.
    @code
    %b modified in default syntax:
       %12Lb  - print 12 qwords from specified address
       %12lb  - print 12 dwords from specified address
       %12hb  - print 12 shorts from specified address
       %#12b  - print 12 0xbytes from specified address
    @endcode
*/
int   __cdecl   snprintf(char *buf, u32t count, const char *fmt, ...);

//#ifdef __WATCOMC__       // ugly hack ;)
//#define sprintf(buf,fmt,...) snprintf(buf,256,fmt,__VA_ARGS__)
//#endif

/** printf function.
    @see snprintf */
int   __cdecl   printf  (const char *fmt, ...);

void* __stdcall memset  (void *dst, int c, u32t length);

int   __stdcall strncmp (const char *s1, const char *s2, u32t n);

int   __stdcall strnicmp(const char *s1, const char *s2, u32t n);

u32t  __stdcall strlen  (const char *s);

char* __stdcall strncpy (char *dst, const char *src, u32t n);

char* __stdcall strcpy  (char *dst, const char *src);

char* __stdcall strchr  (const char *src, int c);

void* __stdcall memcpy  (void *dst, const void *src, u32t length);

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

/* a set of functions for byte/word/dword search and fill;
   note, what memrchrX are available for reverse search in stdlib.h */
void* __stdcall memchr  (const void*mem, int  chr, u32t buflen);
u8t*  __stdcall memchrnb(const u8t* mem, u8t  chr, u32t buflen);
u16t* __stdcall memchrw (const u16t*mem, u16t chr, u32t buflen);
u16t* __stdcall memchrnw(const u16t*mem, u16t chr, u32t buflen);
u32t* __stdcall memchrd (const u32t*mem, u32t chr, u32t buflen);
u32t* __stdcall memchrnd(const u32t*mem, u32t chr, u32t buflen);

u16t* __stdcall memsetw (u16t *dst, u16t value, u32t length);
u32t* __stdcall memsetd (u32t *dst, u32t value, u32t length);

void  __stdcall usleep  (u32t usec);

/** string to int conversion.
    This is qsinit init time internal call basically, better use atoi & strtol.
    Accept spaces before, -, dec & hex values */
long  __stdcall str2long(const char *str);

u32t  __stdcall crc32(u32t crc, const u8t* buf, u32t len);

u32t get_esp(void);
#pragma aux get_esp = "mov eax,esp" value [eax];

u32t ints_enabled(void);
#pragma aux ints_enabled = \
    "pushfd"               \
    "pop     eax"          \
    "shr     eax,9"        \
    "and     eax,1"        \
    value [eax];

#ifdef __cplusplus
}
#endif

#endif // QSINIT_CLIB32
