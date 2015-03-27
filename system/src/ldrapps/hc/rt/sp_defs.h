//
//  primary definitions
//
//  (c) dixie/SP 1997-2013. Freeware.
//
#ifdef SP_PRIMARY_DEFS
#error sp_defs.h is incompatible with previous version of SP definition module!
#endif

#ifndef SP_PRIMARY_DEFS_VERSION_II
#define SP_PRIMARY_DEFS_VERSION_II

#ifdef __QSINIT__
#define SP_QSINIT
#endif

#if !defined(HRT)&&!defined(SRT)&&!defined(SP_QSINIT)
#define HRT
#endif

#ifdef SP_QSINIT
#define SRT
#define NO_SP_ASSERTS
#define NO_IOSTREAM_IO
#define NO_HUGELIST
#define NO_RANDOM
#define GLOBAL_MEM_SWITCH
#define NO_SP_POINTRECT
#include <clib.h>
#else  // SP_QSINIT
#if defined(SP_WIN32)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)
#define NOGDI
#define NOUSER
#define NO_COM
#define NOCRYPT
#define NOIME
#define NOMCX
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <windows.h>
#endif // WIN32
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#if defined(__IBMC__)||defined(__IBMCPP__)
#include <stdarg.h>
#endif
#if defined(SP_LINUX)
#include <unistd.h>
#endif // Linix
#endif // QSINIT

#if defined(NO_SP_ACCERTS) && !defined(NO_SP_ASSERTS)
#define NO_SP_ASSERTS
#elif defined(NO_SP_ASSERTS) && !defined(NO_SP_ACCERTS)
#define NO_SP_ACCERTS
#endif

#ifndef NO_SP_ASSERTS
#include <assert.h>
#endif

#define PRFADD(x)

#ifdef __cplusplus
extern "C" {
#ifndef SP_QSINIT
#include <string.h>
#endif
#endif

#if defined(__GNUC__)                        // GNU C++ 3.95.1
#define impdll extern
#define expdll
#elif defined(__IBMCPP__)||defined(__IBMC__) // IBM C++ 3.6.5
#define impdll __declspec(dllimport)
#define expdll __declspec(dllexport)
#else                                        // other
#define impdll __declspec(dllimport)
#define expdll __declspec(dllexport)
#endif

#define TOUPPER toupper
#define TOLOWER tolower
#define MODF    modf
#define SIN     sin

#if defined(__WATCOMC__)      // Watcom C/C++
#define __WATCOM_LONGLONG_SUPPORT
#define CHSIZE        chsize
#define FAST
#define ITOA          _itoa
#define UTOA          _utoa
#define GCVT          _gcvt
#define ALLOCA        alloca
#define NAKED
#define threadvar     _declspec(thread)
#define cc_cdecl      __cdecl
#define cc_stdcall    __stdcall
#define cc_sighandler __cdecl
#define cc_face       __stdcall
#define STRTOUL64     strtoull
#define STRTOL64      strtoll
#define ATOI64        atoll
#elif defined(__BCPLUSPLUS__) // BC++ 5.5
#define __WATCOM_LONGLONG_SUPPORT
#define FAST          _fastcall
#define CHSIZE        chsize
#define ITOA          itoa
#define UTOA          ultoa
#define GCVT          _gcvt
#define ALLOCA        alloca
#define NAKED         _declspec(naked)
#define threadvar     _declspec(thread)
#define cc_cdecl      __cdecl
#define cc_stdcall    __stdcall
#define cc_sighandler __cdecl
#define cc_face       __stdcall
#if (__BCPLUSPLUS__>=0x0570)&&defined(SP_LINUX)
#define KYLIX
#endif
#elif defined(__IBMC__)||defined(__IBMCPP__)    // IBM VAC++ 3.6.5
#define __LONGLONG_SUPPORT
#define FAST
#define CHSIZE        chsize
#define ITOA          _itoa
#define UTOA          ultoa
#define GCVT          _gcvt
#define ALLOCA        alloca
#define NAKED         _declspec(naked)
#define cc_sighandler _Optlink
#define NO_INLINE_ASM
#ifdef SP_OS2
#define threadvar
#else
#define threadvar     _declspec(thread)
#endif
#define cc_cdecl      __cdecl
#define cc_stdcall    __stdcall
#define cc_face       __stdcall
#define STRTOUL64     strtoull
#define STRTOL64      strtoll
#define ATOI64        atoll
#elif defined(__GNUC__)      // GNU C++ 3.95.1
#define __LONGLONG_SUPPORT
#define FAST          __attribute__ ((regparm(3)))
#define CHSIZE        ftruncate
#define ITOA          itoa
#define UTOA          _utoa
#define GCVT          gcvt
#define ALLOCA        alloca
#define threadvar         // ??
#define NAKED         _declspec(naked)
#define cc_cdecl
#define cc_stdcall
#define cc_sighandler __cdecl
#define cc_face
#define NO_INLINE_ASM
#define STRTOUL64     strtoull
#define STRTOL64      strtoll
#define ATOI64        atoll
#elif defined(_MSC_VER)      // MS VC 5.0
#define __WATCOM_LONGLONG_SUPPORT
#define FAST          _fastcall
#define CHSIZE        _chsize
#define ITOA          _itoa
#define UTOA          _ultoa
#define GCVT          _gcvt
#define ALLOCA        _alloca
#define threadvar     _declspec(thread)
#define NAKED         _declspec(naked)
#define cc_cdecl      __cdecl
#define cc_stdcall    __stdcall
#define cc_sighandler __cdecl
#define cc_face       __stdcall
#if _MSC_VER >= 1300
#define STRTOUL64     _strtoui64
#define STRTOL64      _strtoi64
#define ATOI64        _atoi64
#define NO_IOSTREAM_IO
#define stricmp(s1,s2) (_stricmp(s1,s2))
#define strdup(s1)     (_strdup(s1))
#endif
#ifndef SP_WIN32
#define SP_WIN32 // vc is for win32 only ;)
#endif
#else                        // other hypotetic
#error unknown compiler!
#define FAST          _fastcall
#define CHSIZE        chsize
#define ITOA          _itoa
#define UTOA          _utoa
#define GCVT          _gcvt
#define ALLOCA        alloca
#define threadvar
#define NAKED
#define cc_cdecl      __cdecl
#define cc_stdcall    __stdcall
#define cc_sighandler __cdecl
#define cc_face       __stdcall
#endif

#define cc_fast            FAST
#if defined(__IBMC__)||defined(__IBMCPP__)    // IBM VAC++ 3.6.5
#ifdef __IBMCPP_RN_ON__
#define cc_threadcall __stdcall
#else
#define cc_threadcall _Optlink
#endif
#elif defined(__GNUC__)
#define cc_threadcall
#else
#define cc_threadcall __stdcall
#endif

#ifdef SP_WIN32
#define SP_OSSTR "Win32"
#elif defined(SP_OS2)
#define SP_OSSTR "OS/2"
#elif defined(SP_LINUX)
#define SP_OSSTR "Linux"
#else
#define SP_OSSTR ""
#endif

#define _swapdword(x) (((x)<<24)|(((x)&0x0000FF00)<<8)|(((x)&0x00FF0000)>>8)| \
                  (((unsigned long)((x)&0xFF000000))>>24))
#define _swapword(x) ((((x)&0xFF)<<8)|(((x)>>8)&(0xFF)))

#ifdef SP_BEBO
#define _ihibyte(x) ((x)&0xFF)
#define _ilobyte(x) (((x)>>8)&0xFF)
#define _dword(x)   _swapdword(x)
#define _word(x)    _swapword(x)
#else   // SP_BEBO
#define _ihibyte(x) (((x)>>8)&0xFF)
#define _ilobyte(x) ((x)&0xFF)
#define _dword(x)   (x)
#define _word(x)    (x)
#endif  // SP_BEBO

//#pragma pack(8)    // must be the same for all sources!!

typedef unsigned long  d;
typedef long           l;
typedef unsigned char  b;
typedef unsigned short w;
typedef short          i;
typedef float          f;

typedef l *plongint;
typedef b *pbyte;
typedef w *psmallword;
typedef char *pchar;
typedef d time_t_4;

#ifdef __WATCOM_LONGLONG_SUPPORT
typedef unsigned __int64   u64;
typedef __int64            s64;
#define __LONGLONG_SUPPORT  // make single flag define
#elif defined(__LONGLONG_SUPPORT)
typedef unsigned long long u64;
typedef long long          s64;
#endif
typedef unsigned long      u32;
typedef long               s32;
typedef unsigned short     u16;
typedef short              s16;
typedef unsigned char       u8;
typedef signed char         s8;
typedef float              f32;
typedef double             f64;

typedef void* ptr;
typedef ptr*  pptr;

typedef u32 spHANDLE;

typedef l Bool;
typedef b ByteBool;

/* Palette types */
typedef u8 Plt256[256][3];
typedef u8 VGAPal[768];
typedef struct {
  b red,green,blue;
} SP_RGB;
typedef SP_RGB VGAPlt[256];

typedef Plt256 *PPlt256;
typedef VGAPal *PVGAPal;
typedef VGAPlt *PVGAPlt;

typedef b LetterWdtArray[256];

#define false 0
#define true  1
#ifndef _TV_VERSION
#define False 0
#define True  1
#endif

#define x7FFF     (0x7FFFFFFF)
#define FFFF      (0xFFFFFFFF)
#ifndef x7FFF64
#define x7FFF64   (((s64)(0x7FFFFFFF)<<32)+0xFFFFFFFF)
#define FFFF64    (((u64)(0xFFFFFFFF)<<32)+0xFFFFFFFF)
#endif
#define _16k      (16384)
#define _32k      (32768)
#define _48k      (49152)
#define _64k      (65536)
#define _128k     (131072)
#define _256k     (262144)
#define _384k     (393216)
#define _512k     (524288)
#define _640k     (655360)
#define _768k     (786432)
#define _896k     (917504)
#define _1024k    (1048576)
#define _1Mb      (_1024k)
#define _1Gb      (_1024k*1024)

/* Return types */
#define RET_OK           0  //  all ok
#define RET_OPENERR      1  //  file open error
#define RET_INVALIDFMT   2  //  unknown/invalid format
#define RET_IOERR        3  //  file read/write error
#define RET_NOMEM        4  //  no free memory
#define RET_END          5  //  end of file reached
#define RET_NOSUPPORT    6  //  operation is not supported
#define RET_BUFOVERFLOW  7  //  internal static buffer overflow
#define RET_INVALIDPTR   8  //  invalid pointer supplied
#define RET_MCBDESTROYD  9  //  SPCC structure headers destroyd
#define RET_BUSY        10  //  object is busy for other operation
#define RET_NEEDRESTORE 11  //  object must be restore before next operation
#define RET_INVALIDOP   12  //  invalid operation requested
#define RET_NOMESSAGES  13  //  no more messages in queue
#define RET_DUPLICATE   14  //  duplication is not allowed
#define RET_BREAK       15  //  operation break
#define RET_BADPATH     16  //  bad or invalid path
#define RET_NOACCESS    17  //  access denied
#define RET_BADCRC      18  //  invalid CRC - file damaged
#define RET_NOTFOUND    19  //  something was not founded
#define RET_SYNTAXERROR 20  //  syntax error in script or text file
#define RET_FAILED      21  //  operation failed
#define RET_ZEROPTR     22  //  NULL pointer supplied
#define RET_INVPARAM    23  //  invalid parameter
// note: Entries 0x100-0x140 defined in flib.hpp
// note: Entries 0x970-0x97F defined in sp_msgen.h
// note: Entries 0x980-0x98F defined in sp_sysw32.h
typedef l Error;

#define WordMask(wrd) ((d)0xFFFFFFFF>>8*(4-wrd))

#define SPCC_CONTENTS                            \
  d       Signature;  /* "SPCC" signature" */    \
  d       ObjectID;   /* object ID */            \
  d       GlobalID;   /* Global ID */            \
  d       BlockSize;  /* object size */          \
  d       ObjectType; /* object type */          \
  l       UseCnt;     /* object usage count */   \
  w       LockCnt;    /* object locking count */ \
  w       Flags;      /* object flags */         \
  s16     ObjectPool; /* memmgr cache */         \
  u16     BackLink;   /* memmgr internal */

#define SPCCOBJECT_ARCHIVE  0x01  /* object was modified - not used */
#define SPCCOBJECT_LOCKED   0x02  /* object is locked */
#define SPCCOBJECT_USER1    0x40  /* user flag */
#define SPCCOBJECT_USER2    0x80  /* user flag */

#define SPCCOBJTYPE_GENERAL          0
#define SPCCOBJTYPE_IMAGE            1  // object type for SPCC structure
#define SPCCOBJTYPE_TRANSPORTCLIENT  2  // transport client info
#define SPCCOBJTYPE_SPREXECUTABLE    3
#define SPCCOBJTYPE_SPHTML           4
#define SPCCOBJTYPE_SPGRLHANDLER     5
#define SPCCOBJTYPE_SPIFCSEQ         6
#define SPCCOBJTYPE_SPIFCSEQLIST     7

extern char SIGNATURE[5];

#ifdef __GNUC__
char* cc_cdecl _utoa(d val,char *buf,int radix);
char* cc_cdecl itoa(int val,char *buf,int radix);
char* cc_cdecl _ltoa(l val, char *buf,int radix);
#define stricmp(s1,s2) (strcasecmp(s1,s2))
#endif

#if defined(__GNUC__)
#define _vsnprintf(buf,count,format,arg) (vsprintf(buf,format,arg))
#endif

#if defined(__IBMC__)||defined(__IBMCPP__)
int _vsnprintf(char *buf,size_t count,const char *format,va_list arg);
#endif

#if defined(__IBMC__)||defined(__IBMCPP__)
int CHDIR(const char *);
int MKDIR(const char *);
int RMDIR(const char *);
#ifdef __IBMCPP_RN_ON__
int remove(const char*File);
// drop fflush(stdout) calls
#define fflush(x)
//int rand();
//void srand(unsigned int seed);
//#define RAND_MAX x7FFF
#endif
#elif defined(__GNUC__)
#define CHDIR chdir
int MKDIR(const char *);
#define RMDIR rmdir
#elif defined(SP_QSINIT)
#define CHDIR chdir
#define MKDIR mkdir
#define RMDIR rmdir
#else
#define CHDIR f_chdir
#define MKDIR f_mkdir
#define RMDIR f_unlink
#endif

#ifdef PROFILER
#include "sp_profile.h"
#endif

#define SPCC_SIGN (0x43435053)

#ifdef __cplusplus
}

#define MEMORY_CONTENTS                                        \
  union {                                                      \
    u8    Mem[1];   /* access via byte(s) for compatibility */ \
    u8  dbMem[1];   /* access via byte(s)                   */ \
    u16 dwMem[1];   /* access via word(s)                   */ \
    u32 ddMem[1];   /* access via long(s)                   */ \
  };
extern "C" {
#else

#define MEMORY_CONTENTS                                        \
  u8    Mem[4];     /* access via byte(s) */

#endif

/// basic structure
typedef struct {
  SPCC_CONTENTS
  MEMORY_CONTENTS
} SPCC;

typedef struct {
  SPCC_CONTENTS
} SPCC_clean_contents;

#define SPCC_SIZE (sizeof(SPCC_clean_contents))

typedef SPCC* PSPCC;


#ifdef GLOBAL_MEM_SWITCH
#include "sp_new.hpp"
#define MALLOC spmalloc
#define REALLOC sprealloc
#define FREE spfree
#elif defined(SP_EMBEDDED_RUNTIME) && defined(__IBMCPP_RN_ON__) && defined(HRT)
#include "sp_mem.h"
#define MALLOC spmalloc
#define REALLOC sprealloc
#define FREE spfree
#else
#define MALLOC malloc
#define REALLOC realloc
#define FREE free
#endif

#ifndef NO_FSIZE
#ifdef SP_QSINIT
#define setfsize(fp,pos) _chsize((int)(fp),pos)
#define fsize(fp) filelength((int)(fp))
#else
d cc_stdcall fsize(FILE *fp);
void cc_stdcall setfsize(FILE *fp,d newsize);
#endif
#endif

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
#define Round4k(ii)  (((ii)+0xFFF)&0xFFFFF000)
#define Round64k(ii) (((ii)+0xFFFF)&0xFFFF0000)

l FAST BSF(d Value);  // bit scan forward (0..31)
l FAST BSR(d Value);  // bit scan reversed (31..0)

#if defined(_MSC_VER)||defined(__BCPLUSPLUS__)
#define RAND_CCONV FAST
#elif defined(__WATCOMC__)
#define RAND_CCONV
#else
#define RAND_CCONV cc_cdecl
#endif

#ifndef NO_RANDOM
extern d    RAND_CCONV RandInt(d Range);
extern f32  RAND_CCONV RandFlt(void);
#ifndef __IBMCPP_RN_ON__
l FAST randomG(l max);    // there is no math functions... :(
#endif
#endif

#ifndef NO_INTLOCK
// thread safe put/get int
extern l    RAND_CCONV SafeGetInt(l*ptr);
extern void RAND_CCONV SafePutInt(l*ptr,d value);
extern void RAND_CCONV SafeIncInt(l*ptr);
extern void RAND_CCONV SafeDecInt(l*ptr);
#endif

#ifdef __WATCOMC__
extern d    RandSeed;
extern f32 _RndCDelta;
extern s16 _RndCScale;

#ifndef NO_INTLOCK
#pragma aux SafeGetInt =            \
    "mov     eax,dword ptr [eax]"   \
    parm [eax]                      \
    value [eax];

#pragma aux SafeIncInt =            \
"lock inc     dword ptr [eax]"      \
    parm [eax];

#pragma aux SafeDecInt =            \
"lock dec     dword ptr [eax]"      \
    parm [eax];

#pragma aux SafePutInt =            \
    "mov     dword ptr [eax],ecx"   \
    parm [eax ecx];
#endif // NO_INTLOCK

#ifndef NO_RANDOM
#pragma aux RandInt =     \
    "mov     eax,8088405h"\
    "mul     RandSeed"    \
    "inc     eax"         \
    "mov     RandSeed,eax"\
    "mul     ecx"         \
    "mov     eax,edx"     \
    parm [ecx]            \
    modify [eax edx];

#pragma aux RandFlt =     \
    "mov     eax,8088405h"\
    "mul     RandSeed"    \
    "inc     eax"         \
    "mov     RandSeed,eax"\
    "fild    _RndCScale"  \
    "fild    RandSeed"    \
    "fadd    _RndCDelta"  \
    "fscale"              \
    "fstp    st(1)"       \
    modify [eax edx 8087] \
    value [8087];
#endif // NO_RANDOM

#pragma aux BSF =         \
    "bsf     eax,ecx"     \
    "jnz     bye"         \
    "mov     eax,-1"      \
"bye:"                    \
    parm [ecx]            \
    value [eax];

#pragma aux BSR =         \
    "bsr     eax,ecx"     \
    "jnz     bye"         \
    "mov     eax,-1"      \
"bye:"                    \
    parm [ecx]            \
    value [eax];

#endif

l FAST RoundN (l N,l I);
l FAST RoundP2(l I);

l FAST roundfd(f64 value);

#define ROUNDF roundfd

#ifdef SRT
#define NOOLDXOR
#endif

#define range(c,a,b) (((c)<(a)?(a):((c)>(b)?(b):(c))))
#define spswp(x,y) ((x)^=(y)^=(x)^=(y))
#define Xor(v1,v2) (((v1)?1:0)^((v2)?1:0))
#ifndef NOOLDXOR
#define xor Xor
#endif

#ifdef __IBMCPP_RN_ON__
int toupper2(int cc);
int tolower2(int cc);
char *strupr(char *str);
char *strlwr(char *str);
int isdigit2(int cc);
#ifdef isdigit
#undef isdigit
#endif
#define isdigit isdigit2
#ifndef SRT
double cc_cdecl cc_modf(double value,double *iptr);
double cc_cdecl cc_sin(double value);
#endif
#undef TOUPPER
#undef TOLOWER
#undef MODF
#undef SIN
#define MODF cc_modf
#define SIN cc_sin
#define TOUPPER toupper2 //((cc)>0x60&&(cc)<=0x7A?(cc)-0x20:(cc))
#define TOLOWER tolower2 //((cc)>0x40&&(cc)<=0x5A?(cc)+0x20:(cc))
char *_gcvt(double,int,char*);
int stricmp(const char*s1,const char*s2);
#elif defined(SP_QSINIT)
char *_gcvt(double,int,char*);
#else
#ifdef __GNUC__
char *strupr(char *str);
char *strlwr(char *str);
#endif
#include <ctype.h> //toupper
#endif
d WCSLEN(w*);

/* Apply value - rec with operation and value. */
#define APPLY_SET 0
#define APPLY_ADD 1
#define APPLY_SUB 2
#define APPLY_MUL 3
#define APPLY_DIV 4

typedef struct {
  f32         Value; // value
  u8             Op; // operation
  u8    Reserved[3]; // alignment & reserve
} ApplyValue;

typedef ApplyValue a;

#ifndef SRV_COORD  // old Direction definition is present
#define NW       0xFFFF
#define SW       0xFF01
#define NE       0x01FF
#define SE       0x0101
#endif
#define STOP     0x0000
#define NORTH    0x00FE
#define SOUTH    0x0002
#define WEST     0xFE00
#define EAST     0x0200

typedef l Direction;
#pragma pack(1)
typedef struct {s8 WE,NS;} DirectionRec;
#pragma pack()

#define CoordIsNorth(dir) ((s8)(dir)<0)
#define CoordIsSouth(dir) ((s8)(dir)>0)
#define CoordIsWest(dir) ((s16)(dir)<0)
#define CoordIsEast(dir) ((s16)(dir)>>8>0)
#define CoordNSAndWE(NS,WE) ((NS)&&(WE)?(s8)(NS)/2&(s16)(WE)/0x200:NS&WE)
#define CoordTurnAround(dir) (-(s8)(dir>>8)<<8|-(s8)(dir))

// swap order of "len" datas with size "size"
void FAST memswap(void*mem,d size,d len);

#ifndef SP_QSINIT
extern void*RAND_CCONV memsetw(void*dst,w value,d length);
extern void*RAND_CCONV memsetd(void*dst,d value,d length);

w* cc_stdcall memchrw(w*,w chr,d length);
d* cc_stdcall memchrd(d*,d chr,d length);

/* search while mem*==chr */
b* cc_stdcall memchrnb(b*,b chr,d length);
w* cc_stdcall memchrnw(w*,w chr,d length);
d* cc_stdcall memchrnd(d*,d chr,d length);

#ifdef __WATCOMC__
#pragma aux memsetw =     \
    "cld"                 \
    "mov     edx,edi"     \
    "rep     stosw"       \
    parm [edi eax ecx]    \
    modify [edi edx ecx]  \
    value [edx];

#pragma aux memsetd =     \
    "cld"                 \
    "mov     edx,edi"     \
    "rep     stosd"       \
    parm [edi eax ecx]    \
    modify [edi edx ecx]  \
    value [edx];
#endif
#endif // SP_QSINIT

#ifdef __cplusplus
}

#define STATIC_CAST(T)      static_cast<T>
#define DYNAMIC_CAST(T)     dynamic_cast<T>
#define REINTERPRET_CAST(T) reinterpret_cast<T>
#define CONST_CAST(T)       const_cast<T>


#ifdef HRT // disabled in light version because of lacking runtime dll...

class cSPCC {
protected:
  SPCC *data;
public:
  // constructors
  cSPCC(d GlobalID,d ObjectID,d BlockSize,d ObjectType=SPCCOBJTYPE_GENERAL) {
    data=NULL; AllocObject(GlobalID,ObjectID,BlockSize);
    data->ObjectType=ObjectType;
  }
  cSPCC(cSPCC& from) { data=NULL; Assign(from.data); }
  cSPCC(SPCC *from=NULL) { data=NULL; Assign(from); }
  // assignment
  cSPCC& operator= (const cSPCC& from) { Free(); Assign(from.data); return *this; }
  // comparation
  Bool operator==(const cSPCC& from) {
    return data==from.data||data->BlockSize==from.data->BlockSize&&
      memcmp(&data->Mem,&from.data->Mem,data->BlockSize)==0;
  }
  // destructor
  virtual ~cSPCC() { Free(); }
  // handling
  cSPCC& UniqueObject();                               // make object unique
  Bool   AllocObject(d GlobalID,d ObjectID,d NewSize); // alloc instance
  Bool   ResizeObject(d NewSize);                      // resize
  SPCC*  LockPtr();                                    // get writable ptr
  void   UnlockPtr();                                  // free writable ptr
  Bool   Valid() { return data!=NULL; }
  d      Size() { return data?data->BlockSize:0; }

  /// attach, with effect on UseCount
  SPCC* Assign(SPCC *from) {
    SPCC* res=Free(); data=from;
    if (data) {
#ifndef NO_SP_ASSERTS
      assert(!(from->Flags&SPCCOBJECT_LOCKED));
#endif
      data->UseCnt++;
    }
    return res;
  }
  SPCC* Deassign() { return Free(); }
  /// attach, without effect on UseCount
  SPCC* Attach(SPCC *from) {
    SPCC *res=data;
    data=from;
    return res;
  }
  SPCC* Detach() { return Attach(NULL); }
  /// return old value or NULL,if it freed
  SPCC *Free();
};

class pspcc {
protected:
  union {
    PSPCC SPt;
    pchar CHt;
  };
public:
  pspcc(PSPCC PP=NULL) { SPt=PP; }
  pspcc& operator= (pspcc&PP) { SPt=PP.SPt; return *this; }
  pspcc& operator= (PSPCC PP) { SPt=PP; return *this; }

  friend l operator- (pspcc const &p1,pspcc const &p2) { return p2.CHt-p1.CHt; }

  operator SPCC*() { return SPt; }
  operator char*() { return (char*)SPt; }
  operator void*() { return (ptr)SPt; }
  operator b*()    { return (b*)SPt; }
  SPCC& operator*()  {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    return *SPt;
  }
  SPCC** operator&() {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    return &SPt;
  }
  SPCC* operator->() {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    return SPt;
  }
  Bool  operator!() { return !SPt; }
  pspcc& operator+=(l Count) {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    CHt+=Count;
    return *this;
  }
  Bool operator==(PSPCC PP) { return SPt==PP; }
  Bool operator!=(PSPCC PP) { return SPt!=PP; }
//  Bool operator==(ptr PP) { return SPt==PP; }
//  Bool operator!=(ptr PP) { return SPt!=PP; }
  Bool operator==(pspcc PP) { return SPt==PP.SPt; }
  Bool operator!=(pspcc PP) { return SPt!=PP.SPt; }
  pspcc& operator+=(d Count) { return operator+=(l(Count)); }
  pspcc& operator-=(l Count) { return operator+=(-Count); }
  pspcc& operator-=(d Count) { return operator+=(-l(Count)); }
  pspcc& operator++(int) {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    return operator+=(SPt->BlockSize);
  }
  pspcc& operator--(int) {
#ifndef NO_SP_ASSERTS
    assert(SPt);
#endif
    return operator-=((l)(SPt->BackLink)<<4);
  }
};

#endif // HRT

#ifndef NO_SP_POINTRECT

/// Point(x,y) class
struct TPoint {
  l x,y;
  TPoint(l xx=0,l yy=0) { x=xx; y=yy; }
  TPoint(const TPoint &P) { x=P.x; y=P.y; }
  TPoint& operator += (const TPoint &P) { x+=P.x;y+=P.y;return *this; }
  TPoint& operator -= (const TPoint &P) { x-=P.x;y-=P.y;return *this; }
  TPoint operator + (const TPoint &);
  TPoint operator - (const TPoint &);
};


/// Rect(x1,y1,x2,y2) class
struct TRect {
  l xLeft,xRight,yTop,yBottom;
  TRect(l x1=0,l y1=0,l x2=0,l y2=0)
    { xLeft=x1; xRight =x2; yTop=y1; yBottom=y2; }
  TRect(const TRect& Source) { operator=(Source); }
  TRect& operator=(const TRect& Source) {
    xLeft  =Source.xLeft;
    xRight =Source.xRight;
    yTop   =Source.yTop;
    yBottom=Source.yBottom;
    return *this;
  }
  TRect& operator += (const TPoint&);                     // rect offset
  TRect& operator -= (const TPoint&);                     // rect offset
  TRect& operator &= (const TRect&rr) { return Trunc(rr); }
  TRect& operator += (const TRect&rr) { return *this=*this+rr; }
  Bool operator () (const TRect &R) const {               // R1 include R2
    return (R.xLeft>=xLeft&&R.xRight<=xRight&&R.yTop>=yTop&&R.yBottom<=yBottom);
  }
  Bool operator () (const TPoint &R) const {              // R1 include Point
    return (R.x>=xLeft&&R.x<=xRight&&R.y>=yTop&&R.y<=yBottom);
  }
  friend Bool operator == (const TRect&, const TRect&);   // comparation
  friend Bool operator != (const TRect&, const TRect&);   // comparation
  friend Bool operator && (const TRect&R1,const TRect&R2) { // is_intersection?
    return !(R1.xLeft>R2.xRight||R2.xLeft>R1.xRight||R1.yTop>R2.yBottom||
      R2.yTop>R1.yBottom);
  }
  friend TRect operator & (const TRect&, const TRect&);   // intersection
  friend TRect operator + (const TRect&, const TRect&);   // additition
  friend TRect operator + (const TRect&, const TPoint&);
  friend TRect operator - (const TRect&, const TPoint&);
  l dx(void) const { return xRight-xLeft+1;}
  l dy(void) const { return yBottom-yTop+1;}
  Bool IsEmpty() const { return xRight<xLeft||yBottom<yTop; }
  TRect& Clear() { xLeft=0; xRight=-1; yTop=0; yBottom=-1; return *this; }

  TRect& OffsetBy(const TRect&RR) {
    xLeft  +=RR.xLeft;
    xRight +=RR.xRight;
    yTop   +=RR.yTop;
    yBottom+=RR.yBottom;
    return *this;
  }
  // move rect to (0,0,width,height)
  TRect& To00() {
    xRight-=xLeft; xLeft=0;
    yBottom-=yTop; yTop =0;
    return *this;
  }

  TRect& ExpandBy(l dx,l dy) {
    xLeft-=dx; xRight+=dx; yTop-=dy; yBottom+=dy;
    return *this;
  }

  TRect& Trunc(const TRect &By); // trunc by this rect
  void MakeRect(l x1,l y1,l x2,l y2) {
    xLeft  =x1;
    xRight =x2;
    yTop   =y1;
    yBottom=y2;
  }
  TPoint LeftUp()    const { return TPoint(xLeft,yTop); }
  TPoint LeftDown()  const { return TPoint(xLeft,yBottom); }
  TPoint RightUp()   const { return TPoint(xRight,yTop); }
  TPoint RightDown() const { return TPoint(xRight,yBottom); }
  TPoint Size()      const { return TPoint(dx(),dy()); }

#ifdef SP_WIN32
  TRect& operator= (RECT const&rr) {
    xLeft=rr.left; xRight=rr.right-1; yTop=rr.top; yBottom=rr.bottom-1;
    return *this;
  }
  operator RECT() const {
    RECT rr;
    rr.left=xLeft; rr.right=xRight+1; rr.top=yTop; rr.bottom=yBottom+1;
    return rr;
  }
#endif
};

#endif //NO_SP_POINTRECT

template <class F> F apply(F value,const a &toapply) {
  switch (toapply.Op) {
    case APPLY_ADD: return F(value+toapply.Value);
    case APPLY_SUB: return F(value-toapply.Value);
    case APPLY_MUL: return F(value*toapply.Value);
    case APPLY_DIV: return F(value/toapply.Value);
    case APPLY_SET:
    default:        return F(toapply.Value);
  }
}

#else // cplusplus
#ifndef NO_SP_POINTRECT
typedef struct { l x,y; } TPoint;
typedef struct { l xLeft,xRight,yTop,yBottom; } TRect;
#endif
#endif

#endif // SP_PRIMARY_DEFS_VERSION_II
