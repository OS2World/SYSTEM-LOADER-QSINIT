#include "sp_defs.h"
#include <malloc.h>
#ifdef HRT
#include "sp_mem.h"
#endif
#ifndef SP_QSINIT
#include <math.h>
#if !defined(__GNUC__)&&!defined(KYLIX)
#include <io.h>
#else
#ifdef KYLIX
#include <io.h>
#endif
#include <unistd.h>
#include <sys/stat.h>
#define _fileno(fp) (fp->_fileno)
#endif // gcc
#endif // qsinit

#ifndef _MSC_VER
#include "minmax.h"
#else
#pragma warning(disable:4035)
#endif

#ifdef HRT
extern "C" {
Bool AlwaysCreateWidePalette= true;
Bool  CreateGrayscalePalette= true; // this must be 1
}
#endif

#ifdef __IBMCPP__
#include <direct.h>
#define makestupidiccop(op) {  \
  char *dir=strdup(Dir);       \
  int result=op(dir);          \
  free(dir);                   \
  return result;               \
}
#ifndef __IBMCPP_RN_ON__
int CHDIR(const char *Dir) { makestupidiccop(chdir) }
int MKDIR(const char *Dir) { makestupidiccop(mkdir) }
int RMDIR(const char *Dir) { makestupidiccop(rmdir) }
#elif defined(SP_OS2)
#define INCL_DOSFILEMGR
#include <os2.h>
int RMDIR(const char *Dir) { makestupidiccop(DosDeleteDir) }
int remove(const char*Dir) { makestupidiccop(DosDelete) }
#elif defined(SP_WIN32)
int CHDIR(const char *Dir) { return SetCurrentDirectory(Dir)==0; }
int MKDIR(const char *Dir) { return CreateDirectory(Dir,NULL)==0; }
int RMDIR(const char *Dir) { return RemoveDirectory(Dir)==0; }
int remove(const char*File) { return DeleteFile(File)==0; }
#endif
#endif

#if defined(__IBMC__)||defined(__IBMCPP__)

#ifndef HRT
#if defined(OS2)||defined(SP_OS2)
#define MEM_RELEASE        0
#define MEM_DECOMMIT  (0x20) // PAG_DECOMMIT
#define MEM_RESERVE        0
#define MEM_COMMIT    (0x10) // PAG_COMMIT
#endif

#ifdef SP_OS2
#define INCL_BASE
#include <os2.h>
#endif

static void FAST memVirtualAlloc(ptr* Ptr,d Size,d Flags) {
#ifdef SP_WIN32
  l PgAccess[2]={PAGE_EXECUTE_READWRITE,PAGE_NOACCESS};
  *Ptr=VirtualAlloc(NULL,Size,Flags,PgAccess[Flags&(MEM_RESERVE!=0&1)]);
#endif
#ifdef SP_OS2
  if (DosAllocMem(Ptr,Size,PAG_READ|PAG_WRITE|Flags)) *Ptr=NULL;
#endif
}

static Bool FAST memVirtualFree(ptr Ptr,d Size,d Flags) {
#ifdef SP_WIN32
  return VirtualFree(Ptr,(Flags&MEM_DECOMMIT)==0?0:Size,Flags);
#endif
#ifdef SP_OS2
  return Flags?!DosSetMem(Ptr,Size,PAG_DECOMMIT):!DosFreeMem(Ptr);
#endif
}
#endif //HRT

int _vsnprintf(char *buf,size_t count,const char *format,va_list arg) {
  d cnt=RoundN(4096,count);
  ptr bb;
  memVirtualAlloc(&bb,cnt+4096,MEM_COMMIT);     // alloc buffer
  memVirtualFree((b*)bb+cnt,4096,MEM_DECOMMIT); // decommit next page
  try {
    int cc=vsprintf((char*)bb,format,arg);
    if (cc<count) strcpy(buf,(char*)bb);
    memVirtualFree(bb,cnt+4096,MEM_RELEASE);
    return cc>=count?-1:cc;
  } catch(...) {
    memVirtualFree(bb,cnt+4096,MEM_RELEASE);
    return -1;
  }
}
#endif


#ifdef __IBMCPP_RN_ON__
int isdigit2(int cc) {
  return cc>='0'&&cc<='9';
}

int toupper2(int cc) {
  return ((cc)>0x60&&(cc)<=0x7A?(cc)-0x20:(cc));
}
int tolower2(int cc) {
  return ((cc)>0x40&&(cc)<=0x5A?(cc)+0x20:(cc));
}
int stricmp(const char*s1,const char*s2) {
  char *cc1=strdup(s1),
       *cc2=strdup(s2);
  int   res=strcmp(strlwr(cc1),strlwr(cc2));
  free(cc2);
  free(cc1);
  return res;
}
#endif
#if defined(__IBMCPP_RN_ON__)
char *_gcvt(double value,int digits,char*str) {
  char fmt[24]; fmt[0]='%'; fmt[1]='#';
  sprintf(fmt+2,"%d.%dg",digits,digits-1); /* "%#(digits).(digits-1)g" */
  sprintf(str,fmt,value);
  return str;
}
#endif
#if defined(__IBMCPP_RN_ON__)||defined(__GNUC__)
char *strupr(char *str) {
  char *cc=str-1;
  while (*(++cc))
    if (*cc>0x60&&*cc<=0x7A) *cc-=0x20;
  return str;
}
char *strlwr(char *str) {
  char *cc=str-1;
  while (*(++cc))
    if (*cc>0x40&&*cc<=0x5A) *cc+=0x20;
  return str;
}
#endif

#ifdef __GNUC__
int MKDIR(const char *dir) {
  return mkdir(dir,S_IRWXU|S_IRWXO|S_IRWXG);
}

static void cc_cdecl xtoa (d val, char *buf, unsigned radix, int is_neg) {
  char *p, *firstdig, temp;
  unsigned digval;
  p = buf;
  if (is_neg) { *p++='-'; val=(d)(-(l)val); }
  firstdig = p;
  do {
    digval=(unsigned)(val % radix);
    val/=radix;
    if (digval>9) *p++=(char)(digval-10+'a'); else
      *p++=(char)(digval+'0');
  } while (val > 0);
  *p--='\0';
  do {
    temp=*p;
    *p=*firstdig;
    *firstdig=temp;
    --p;
    ++firstdig;
  } while (firstdig<p);
}

char* cc_cdecl itoa(int val,char *buf,int radix) {
  if (radix==10&&val<0) xtoa((d)val,buf,radix,1); else
    xtoa((d)(unsigned int)val,buf,radix,0);
  return buf;
}

char* cc_cdecl _ltoa(l val, char *buf, int radix) {
  xtoa((d)val,buf,radix,(radix==10&&val<0));
  return buf;
}

char* cc_cdecl _utoa(d val,char *buf,int radix) {
  xtoa(val, buf, radix, 0);
  return buf;
}
#endif

#if !defined(__IBMCPP_RN_ON__)&&!defined(NO_FSIZE)&&!defined(__QSINIT__)
d cc_stdcall fsize(FILE *fp) {
  d save_pos, size_of_file;
  save_pos=ftell(fp);
  fseek(fp,0,SEEK_END);
  size_of_file=ftell(fp);
  fseek(fp,save_pos,SEEK_SET);
  return size_of_file;
}

#ifndef __WATCOMC__
void cc_stdcall setfsize(FILE *fp,d newsize) {
  CHSIZE(_fileno(fp),newsize);
}
#endif
#endif

l FAST RoundN(l N,l I) {
  l J;
  J=I/N;if (I%N) J++;
  return J*N;
}

#ifndef SP_QSINIT
#if defined(_MSC_VER)
// ECX and EDX
void*FAST memsetw(void*dst,w value,d _ln) {
  __asm {
    push  edi
    mov   eax,edx
    mov   edi,ecx
    cld
    mov   edx,ecx
    mov   ecx,dword ptr _ln
    rep   stosw
    mov   eax,edx
    pop   edi
  }
}

void*FAST memsetd(void*dst,d value,d _ln) {
  __asm {
    push  edi
    mov   eax,edx
    mov   edi,ecx
    cld
    mov   edx,ecx
    mov   ecx,_ln
    rep   stosd
    mov   eax,edx
    pop   edi
  }
}
#elif defined(__BCPLUSPLUS__)
// EAX EDX ECX
void*FAST memsetw(void*dst,w value,d length) {
  __asm {
    push  edi
    xchg  eax,edx
    cld
    mov   edi,edx
    rep   stosw
    mov   eax,edx
    pop   edi
  }
}

void*FAST memsetd(void*dst,d value,d length) {
  __asm {
    push  edi
    xchg  eax,edx
    cld
    mov   edi,edx
    rep   stosd
    mov   eax,edx
    pop   edi
  }
}
#elif !defined(__WATCOMC__)
void*cc_cdecl memsetw(void*dst,w value,d length) {
  w*Dst=(w*)dst; while (length--) *Dst++=value; return dst;
}
void*cc_cdecl memsetd(void*dst,d value,d length) {
  d*Dst=(d*)dst; while (length--) *Dst++=value; return dst;
}
#endif

#if defined(_MSC_VER) //||defined(__WATCOMC__)
b* cc_stdcall memchrnb(b*mem,b chr,d buflen) {
  __asm {
    push  edi
    mov   edi,mem
    mov   eax,dword ptr chr
    mov   ecx,buflen
    repe  scasb
    mov   eax,ecx
    jz    bye
    lea   eax,[edi-1]
bye:
    pop   edi
  }
}

w* cc_stdcall memchrw(w*mem,w chr,d buflen) {
  __asm {
    push  edi
    mov   edi,mem
    mov   eax,dword ptr chr
    mov   ecx,buflen
    repne scasw
    mov   eax,ecx
    jnz   bye
    lea   eax,[edi-2]
bye:
    pop   edi
  }
}

w* cc_stdcall memchrnw(w*mem,w chr,d buflen) {
  __asm {
    push  edi
    mov   edi,mem
    mov   eax,dword ptr chr
    mov   ecx,buflen
    repe  scasw
    mov   eax,ecx
    jz    bye
    lea   eax,[edi-2]
bye:
    pop   edi
  }
}

d* cc_stdcall memchrd(d*mem,d chr,d buflen) {
  __asm {
    push  edi
    mov   edi,mem
    mov   eax,chr
    mov   ecx,buflen
    repne scasd
    mov   eax,ecx
    jnz   bye
    lea   eax,[edi-4]
bye:
    pop   edi
  }
}

d* cc_stdcall memchrnd(d*mem,d chr,d buflen) {
  __asm {
    push  edi
    mov   edi,mem
    mov   eax,chr
    mov   ecx,buflen
    repe  scasd
    mov   eax,ecx
    jz    bye
    lea   eax,[edi-4]
bye:
    pop   edi
  }
}
#else
b* cc_stdcall memchrnb(b*mem,b chr,d length) {
  while (length--) if (*mem!=chr) return mem; else mem++;
  return NULL;
}
w* cc_stdcall memchrw(w*mem,w chr,d length) {
  while (length--) if (*mem==chr) return mem; else mem++;
  return NULL;
}
w* cc_stdcall memchrnw(w*mem,w chr,d length) {
  while (length--) if (*mem!=chr) return mem; else mem++;
  return NULL;
}
d* cc_stdcall memchrd(d*mem,d chr,d length) {
  while (length--) if (*mem==chr) return mem; else mem++;
  return NULL;
}
d* cc_stdcall memchrnd(d*mem,d chr,d length) {
  while (length--) if (*mem!=chr) return mem; else mem++;
  return NULL;
}
#endif
#endif // SP_QSINIT

d WCSLEN(w*str) {
  if (!str) return 0;
  w* rc=memchrw(str,0,FFFF-1);
  return rc-str;
}


#if defined(_MSC_VER)
l NAKED FAST BSF(d Value) {
  __asm {
    bsf eax,ecx
    jnz bye
    mov eax,-1
bye:
    ret
  }
}
l NAKED FAST BSR(d Value) {
  __asm {
    bsr eax,ecx
    jnz bye
    mov eax,-1
bye:
    ret
  }
}
#elif !defined(__WATCOMC__)
l FAST BSF(d Value) {
  l cnt=-1;
  if (Value)
    while (++cnt>=0&&(Value&1)==0) Value>>=1;
  return cnt;
}
l FAST BSR(d Value) {
  l cnt=31;
  if (!Value) return -1; else
    while ((Value&0x80000000)==0) { Value<<=1; cnt--; }
  return cnt;
}
#endif

l FAST RoundP2(l I) { //Round to nearest power of 2
  l result;
  result=30;
  while (I<=1<<result) result--;
#ifdef _MSC_VER
  return 1<<(result+1); // Microsoft are always know what we want to do....
#else
  return 1<<result+1;
#endif
}

#ifdef __WATCOMC__
#ifndef NO_RANDOM
d   RandSeed  =1;
f32 _RndCDelta=2147483648.0;
s16 _RndCScale=-32;
#endif
#elif defined(_MSC_VER)||defined(__BCPLUSPLUS__)

#ifndef NO_INTLOCK
// eax cdx ecx ->BCC
// ecx edx     ->VC
l NAKED FAST SafeGetInt(l*ptr) {
  __asm {
#ifdef __BCPLUSPLUS__
lock mov     eax,dword ptr [eax]
#else
lock mov     eax,dword ptr [ecx]
#endif
     ret
  }
}

void NAKED FAST SafeIncInt(l*ptr) {
  __asm {
#ifdef __BCPLUSPLUS__
lock inc dword ptr [eax]
#else
lock inc dword ptr [ecx]
#endif
     ret
  }
}

void NAKED FAST SafeDecInt(l*ptr) {
  __asm {
#ifdef __BCPLUSPLUS__
lock dec dword ptr [eax]
#else
lock dec dword ptr [ecx]
#endif
     ret
  }
}

void NAKED FAST SafePutInt(l*ptr,d value) {
  __asm {
#ifdef __BCPLUSPLUS__
lock mov     dword ptr [eax],ecx
#else
lock mov     dword ptr [ecx],edx
#endif
     ret
  }
}
#endif // NO_INTLOCK

#ifndef NO_RANDOM
extern "C" d RandSeed=1;
static void NAKED NextRandom() {
  __asm {
    mov     eax,8088405h
    mul     RandSeed
    inc     eax
    mov     RandSeed,eax
    ret
  }
}
d NAKED FAST RandInt(d Range) {
  __asm {
#ifdef __BCPLUSPLUS__
    mov     ecx,eax
#endif
    call    NextRandom
    mul     ecx                     // Random * Range / 1 0000 0000h
    mov     eax,edx                 // 0 <= eax < Range
    ret
  }
}
static f32 ConstDelta=2147483648.0;
static s16 ConstScale=-32;
f32 NAKED FAST RandFlt(void) {
 __asm {
    call    NextRandom              // Compute next random number
    fild    ConstScale              // Load -32
    fild    RandSeed                // Load 32-bit random integer
    fadd    ConstDelta              // Scale to 32-bit positive integer
    fscale                          // Scale so 0 <= ST < 1
    fstp    st(1)                   // Remove scaling factor
    ret
  }
}
#endif // NO_RANDOM
#endif // MSVC && BCC

void FAST memswap(void*mem,d size,d len) {
  if (len<1) return;
  switch (size) {
    case 1: {
        b *x1=(b*)mem,*x2=(b*)mem+(len-1); len>>=1;
        while (len--) { register b swp=*x1; *x1++=*x2; *x2--=swp; }
      }
      break;
    case 2: {
        w *x1=(w*)mem,*x2=(w*)mem+(len-1); len>>=1;
        while (len--) { register w swp=*x1; *x1++=*x2; *x2--=swp; }
      }
      break;
    case 4: {
        d *x1=(d*)mem,*x2=(d*)mem+(len-1); len>>=1;
        while (len--) { register d swp=*x1; *x1++=*x2; *x2--=swp; }
      }
      break;
    default: {
#ifdef SP_QSINIT
        void *sbuf=MALLOC(size);
#else
        void *sbuf=size>512?MALLOC(size):ALLOCA(size);
#endif
        for (l ii=0;ii<len/2;ii++) {
          memcpy(sbuf,(b*)mem+ii*size,size);
          memcpy((b*)mem+ii*size,(b*)mem+(len-1-ii)*size,size);
          memcpy((b*)mem+(len-1-ii)*size,sbuf,size);
        }
#ifndef SP_QSINIT
        if (size>512) 
#endif
           FREE(sbuf);
      }
      break;
  }
}

#ifndef SRT
l FAST roundfd(f64 value) {
  f64 intv;
  f64 fp=MODF(value,&intv);
  return l(fabs(fp)>=.5?(value>=0?intv+1:intv-1):intv);
}
#endif

#if !defined(__IBMCPP_RN_ON__)&&!defined(NO_RANDOM)
l FAST randomG(l max) { //random via gauss
  f64 a,b,q,sq;
  l result;
  do {
    a=2*RandFlt()-1;
    b=2*RandFlt()-1;
    q=a*a+b*b;
  } while (q>=1);
  sq=sqrt(-2.0*log(q)/q);
  result=(l)(max*fabs(a*sq/3));
  if (result>max-1) result=max-1;
  return result;
}
#endif

#ifdef __cplusplus

#ifdef HRT

cSPCC& cSPCC::UniqueObject() {
  if (data->UseCnt>1) {
#ifndef NO_SP_ASSERTS
     assert(!(data->Flags&SPCCOBJECT_LOCKED));
#endif
     SPCC *olddata=data;
     data=memDupCC(data);
#ifndef NO_SP_ASSERTS
     assert(data);  // check for memory allocation failed
#endif
     olddata->UseCnt--;
  }
  return *this;
}

d MemoryManangerVersion=0;
 
struct VersionCheck {
  VersionCheck() { 
    MemoryManangerVersion=memVersion(); 
  }
} *LinkVersion=NULL;

Bool cSPCC::AllocObject(d GlobalID,d ObjectID,d NewSize) {
  if (!LinkVersion) LinkVersion=new VersionCheck;
  SPCC* newdata=memAllocCC(GlobalID,ObjectID,NewSize);
  if (newdata) {
    Free();
    data=newdata;
    return true;
  }
  return false;
}

Bool cSPCC::ResizeObject(d NewSize) {
#ifndef NO_SP_ASSERTS
  assert(!(data->Flags&SPCCOBJECT_LOCKED));
#endif
  SPCC* newdata=memReallocCC(UniqueObject().data,NewSize);
  if (newdata) {
    data=newdata;
    return true;
  }
  return false;
}

SPCC *cSPCC::LockPtr() {
  if (!(data->Flags&SPCCOBJECT_LOCKED)) {
    UniqueObject();
    data->LockCnt=1;
    data->Flags|=SPCCOBJECT_LOCKED;
  } else data->LockCnt++;
  return data;
}

void cSPCC::UnlockPtr() {
  if ((data->Flags&SPCCOBJECT_LOCKED)&&!(--data->LockCnt)) {
    data->Flags&=~SPCCOBJECT_LOCKED;
  }
}

SPCC *cSPCC::Free() {
  //if (data) if (--data->UseCnt<=0) memFree(data); data=NULL;
  SPCC *res=NULL;
  if (data) {
    res=data->UseCnt>1?data:NULL;
    memFree(data);
    data=NULL;
  }
  return res;
}

#endif // HRT

#ifndef NO_SP_POINTRECT

TPoint TPoint::operator + (const TPoint &P) {
  TPoint res(*this);
  res+=P;
  return res;
}

TPoint TPoint::operator - (const TPoint &P) {
  TPoint res(*this);
  res-=P;
  return res;
}

Bool operator == (const TRect &R1, const TRect &R2) {
  return (R1.xLeft==R2.xLeft&&R1.xRight==R2.xRight&&
    R1.yTop==R2.yTop&&R1.yBottom==R2.yBottom);
}

Bool operator != (const TRect &R1, const TRect &R2) {
  return (R1.xLeft!=R2.xLeft||R1.xRight!=R2.xRight||
    R1.yTop!=R2.yTop||R1.yBottom!=R2.yBottom);
}

TRect operator + (const TRect &R1, const TRect &R2) {
  return R1.IsEmpty()?R2:(R2.IsEmpty()?R1:TRect(min(R1.xLeft,R2.xLeft),
    min(R1.yTop,R2.yTop),max(R1.xRight,R2.xRight),max(R1.yBottom,R2.yBottom)));
}

TRect operator + (const TRect&R, const TPoint&P) {
  TRect RR(R);
  RR+=P;
  return RR;
}

TRect operator - (const TRect&R, const TPoint&P) {
  TRect RR(R);
  RR-=P;
  return RR;
}

TRect& TRect::operator += (const TPoint &Offset) {
  xLeft+=Offset.x; xRight +=Offset.x;
  yTop +=Offset.y; yBottom+=Offset.y;
  return *this;
}

TRect& TRect::operator -= (const TPoint &Offset) {
  xLeft-=Offset.x; xRight -=Offset.x;
  yTop -=Offset.y; yBottom-=Offset.y;
  return *this;
}

TRect& TRect::Trunc(const TRect &By) {
  if (*this&&By) {
    xLeft =max(xLeft,By.xLeft);
    xRight=min(xRight,By.xRight);
    yTop  =max(yTop,By.yTop);
    yBottom=min(yBottom,By.yBottom);
  } else { xLeft=0; xRight=-1; yTop=0; yBottom=-1; }
  return *this;
}

TRect operator & (const TRect &r1, const TRect &r2) {  // intersection
  return r1.xLeft>r2.xRight||r2.xLeft>r1.xRight||r1.yTop>r2.yBottom||
    r2.yTop>r1.yBottom?TRect(0,0,-1,-1):
      TRect(max(r1.xLeft,r2.xLeft),max(r1.yTop,r2.yTop),
        min(r1.xRight,r2.xRight),min(r1.yBottom,r2.yBottom));
}

#endif //NO_SP_POINTRECT

#endif
