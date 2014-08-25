#ifndef QS_BASE_TYPES
#define QS_BASE_TYPES

#ifdef DOXYGEN_GEN
#include "qsxdoc.h"
#endif

#define TRUE   (1)
#define FALSE  (0)
#ifndef NULL
#define NULL   (0)
#endif

typedef unsigned __int64   u64t;
typedef __int64            s64t;
typedef unsigned long      u32t;
typedef long               s32t;
typedef unsigned short     u16t;
typedef short              s16t;
typedef unsigned char       u8t;
typedef signed char         s8t;
typedef void             *pvoid;

#ifndef _STDDEF_H_INCLUDED
typedef unsigned long ptrdiff_t;
#endif

#define PAGESHIFT    (12)
#define PAGESIZE     (0x1000)
#define PAGEMASK     (0x0FFF)
#define PARASHIFT    (4)

#define   _1KB       (  1UL* 1024)       // 1 kb
#define   _2KB       (  2UL* _1KB)       // 2 kb
#define   _4KB       (  4UL* _1KB)       // 4 kb
#define  _16KB       ( 16UL* _1KB)       // 32 kb
#define  _32KB       ( 32UL* _1KB)       // 32 kb
#define  _64KB       ( 64UL* _1KB)       // 64 kb
#define _128KB       (  2UL*_64KB)       // 128 kb
#define _256KB       (  4UL*_64KB)       // 256 kb
#define _512KB       (  8UL*_64KB)       // 512 kb
#define _640KB       ( 10UL*_64KB)       // 640 kb
#define   _1MB       ( _1KB* _1KB)       // 1 mb
#define   _2MB       (  2UL* _1MB)       // 2 mb
#define   _4MB       (  4UL* _1MB)       // 4 mb
#define   _8MB       (  8UL* _1MB)       // 8 mb
#define  _15MB       ( 15UL* _1MB)       // 15 mb
#define  _16MB       ( 16UL* _1MB)       // 16 mb
#define  _64MB       ( 64UL* _1MB)       // 64 mb
#define _512MB       (512UL* _1MB)       // 512 mb
#define   _1GB       ( 16UL*_64MB)       // 1 gb
#define   _4GB       (  0UL)             // 4 gb (32 bit)

#define FFFF         (0xFFFFFFFF)
#define x7FFF        (0x7FFFFFFF)
#define x7FFF64      (((s64t)(0x7FFFFFFF)<<32)+0xFFFFFFFF)
#define FFFF64       (((u64t)(0xFFFFFFFF)<<32)+0xFFFFFFFF)
#define _4GBLL       ((u64t)0xFFFFFFFF+1)    // 4 gb (64 bit)
#define _2TBLL       ((u64t)0xFFFFFFFF+1<<9) // 2 tb (64 bit)

#define PAGEROUND(p)    (((p) + PAGEMASK) & ~PAGEMASK)
#define PAGEROUNDLL(p)  (((p) + PAGEMASK) & ~(u64t)PAGEMASK)

#define Round2(ii)   (((ii)+1)&0xFFFFFFFE)
#define Round4(ii)   (((ii)+3)&0xFFFFFFFC)
#define Round8(ii)   (((ii)+7)&0xFFFFFFF8)
#define Round16(ii)  (((ii)+0x0F)&0xFFFFFFF0)
#define Round32(ii)  (((ii)+0x1F)&0xFFFFFFE0)
#define Round64(ii)  (((ii)+0x3F)&0xFFFFFFC0)
#define Round128(ii) (((ii)+0x7F)&0xFFFFFF80)
#define Round256(ii) (((ii)+0xFF)&0xFFFFFF00)
#define Round512(ii) (((ii)+0x1FF)&0xFFFFFE00)
#define Round1k(ii)  (((ii)+0x3FF)&0xFFFFFC00)
#define Round2k(ii)  (((ii)+0x7FF)&0xFFFFF800)
#define Round4k(ii)  (((ii)+0xFFF)&0xFFFFF000)
#define Round64k(ii) (((ii)+0xFFFF)&0xFFFF0000)

#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

#define _std  __stdcall
#ifdef __WATCOMC__
#define _fast __watcall
#else
#define _fast
#endif

#define OFFSETOF(x)   (((u16t*)&(x))[0])
#define SELECTOROF(x) (((u16t*)&(x))[1])
#define MAKEFAR16(seg,ofs) ((u32t)(seg)<<16|(ofs))
#define MAKEFAR32(sel,ofs) ((u64t)(sel)<<32|(ofs))
#define MAKEID4(c1,c2,c3,c4)  ((u32t)((s8t)(c4))<<24|(u32t)((s8t)(c3))<<16|(u32t)((s8t)(c2))<<8|((s8t)(c1)))
#define Xor(v1,v2)    (((v1)?1:0)^((v2)?1:0))

#endif // QS_BASE_TYPES
