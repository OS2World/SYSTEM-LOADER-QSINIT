//
// QSINIT "start" module
// memory manager
// -------------------------------------
//
// todo! some things still not fixed after porting! ;)
//
// this memmgr was written in 1997 for own unfinished pascal compiler ;)
// then it was ported to C and used on some real jobs (for 4 bln. users)
// here used simplified version, with 16 bytes block header
// alloc/free pair speed is ~ the same as in ICC/GCC runtime.
//

#include "clib.h"
#include "qsutil.h"
#include "memmgr.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "vio.h"

#define QSMCC_SIGN (0x484D5351) // QSMH
#define QSMCL_SIGN (0x4C4D5351) // QSML
#define CHECK_MASK (~0x4000000)

#define CALLER(x) (((u32t*)(&x))[-1])

#define sprintf(buf,fmt,...) snprintf(buf,256,fmt,__VA_ARGS__)

#define RET_BUFOVERFLOW    1
#define RET_INVALIDPTR     2
#define RET_MCBDESTROYD    3
#define RET_NOMEM          4
#define RET_CHECKFAILED    5
#define RET_DOUBLEFREE     6

typedef unsigned long  u32t;
typedef unsigned short u16t;
typedef unsigned char   u8t;

typedef struct {
   u32t         Signature; // "QSMM"
   u16t              Size;
   u16t              Link;
   long          GlobalID; // ID1
   long          ObjectID; // ID2
} QSMCC;

typedef struct {
   QSMCC            block;
   QSMCC           *CPrev,
                   *CNext;
   u32t           PoolIdx;
   u32t           PoolPos;
} QSMCCF;

#define QSMCCMEM(PP) ((long*)((QSMCC*)PP+1))

typedef QSMCC *PQSMCC;

class qsmcc {
protected:
   union {
      QSMCC*SPt;
      char *CHt;
   };
public:
   qsmcc(QSMCC*PP=NULL) { SPt=PP; }
   qsmcc& operator= (qsmcc&PP) { SPt=PP.SPt; return *this; }
   qsmcc& operator= (QSMCC*PP) { SPt=PP; return *this; }

   friend long operator- (qsmcc const &p1,qsmcc const &p2)
      { return p2.CHt-p1.CHt; }

   operator QSMCC* () { return SPt; }
   operator QSMCCF*() { return (QSMCCF*)SPt; }
   operator char*  () { return (char*)SPt; }
   operator void*  () { return (void*)SPt; }
   operator u8t*() { return (u8t*)SPt; }
   QSMCC& operator*()  {
      return *SPt;
   }
   QSMCC** operator&() {
      return &SPt;
   }
   QSMCC* operator->() {
      return SPt;
   }
   int operator!() { return !SPt; }
   qsmcc& operator+=(long Count) {
      CHt+=Count;
      return *this;
   }
   int operator==(QSMCC*PP) { return SPt==PP; }
   int operator!=(QSMCC*PP) { return SPt!=PP; }
   int operator==(qsmcc PP) { return SPt==PP.SPt; }
   int operator!=(qsmcc PP) { return SPt!=PP.SPt; }
   qsmcc& operator+=(u32t Count) { return operator+=(long(Count)); }
   qsmcc& operator-=(long Count)    { return operator+=(-Count); }
   qsmcc& operator-=(u32t Count) { return operator+=(-long(Count)); }
   qsmcc& operator++(int) { return operator+=((u32t)SPt->Size<<4); }
   qsmcc& operator--(int) { return operator-=((u32t)SPt->Link<<4); }
};

volatile static int     MMInited     =0;
volatile static int     ParanoidalChk=0; // check ALL at EVERY call (ULTRA slow)
volatile static int     ClearBlocks  =0; // clear blocks on malloc
volatile static int     HaltOnNoMem  =1; // abort() on out of memory
volatile static u32t    CurMemUsed   =0; // total used mem size
volatile static u32t    MaxMemUsed   =0; // max used mem size
extern "C" volatile  long    FirstFree    =0;
volatile static long    LargeFree    =0;
volatile static long    UniquePool   =0;

#define FREEQSMCC       ((long)-1)
#define GETOWNERID      ((long)-2)
#define HeaderSize      long(sizeof(QSMCC))
// ** changeable constants
#define MAX_MEM_SIZE    (1<<28)          // 256Mb - total memory size (<=4Gb ;)
#define MaxLargePower2  25               // 32Mb - max large block size (must be >64k)
#define PoolBlocks      (1<<18)          // 256k - one pool size (64k..1Mb)
// ********
#define LargeShift      (MaxLargePower2-16)
#define LargeRoundMask  ((1<<LargeShift)-1)
#define MaxPool         (MAX_MEM_SIZE/PoolBlocks) // max. number of small pools
#define ExtPool         256                       // number of ext. small variable
                                                  // size pools
#define MaxSmallCanBe   ((1<<16)-HeaderSize-16)   // max small block
#define MaxSmallAlloc   ((48*1024)-HeaderSize)    // max small block
#define MaxLargeAlloc   ((1<<MaxLargePower2)-(LargeRoundMask+1))
#define MaxLargePool    (MAX_MEM_SIZE/MaxSmallAlloc)
                                         // max. number of large pools
#define RoundLarge(ii)  (((ii)+LargeRoundMask)&~LargeRoundMask)

#define MaxTinySize     (4096)
#define SZSHL_S         (4)
#define SZSHL_L         (12)
#define TinySize16      (MaxTinySize>>SZSHL_S)
#define HeaderSize16    (HeaderSize>>SZSHL_S)
typedef QSMCC*CacheArray[TinySize16];

typedef void (*MemMgrError)(u32t ErrType, const char *FromHere,
                            u32t Caller,void *Pointer);
static  void CheckMgr(u32t Caller);
static  void DumpNewReport(QSMCC*P,u32t idx);

volatile static MemMgrError MemError=NULL;

static CacheArray Cache[2];

static void*      ExtPoolPtr[MaxPool+ExtPool];  // ext. pools pointers
static QSMCC*      SmallPool[MaxPool+ExtPool];  // small pools
static QSMCC*         LargePool[MaxLargePool];  // large pools

static void* mget(u32t Size) {
   void *rc = hlp_memallocsig(Size, "heap", QSMA_RETERR);
   return rc;
}
static inline void mfree(void *PP) { hlp_memfree(PP); }

#define LC(pp) ((QSMCCF*)(pp))

static inline u32t actsize(QSMCC *pp) {
   return pp->Size<<(pp->Signature==QSMCL_SIGN?LargeShift:4);
}

static void AddToCache(QSMCC*PP) {
   long lg=PP->Size<TinySize16?0:1;
   PQSMCC*chpos=Cache[lg]+(PP->Size>>(lg?SZSHL_L-SZSHL_S:0));
   PQSMCC next=*chpos;
   LC(PP)->CNext =next;
   LC(PP)->CPrev =NULL;
   *chpos    =PP;
   if (next) LC(next)->CPrev=PP;
}

static void RmvFromCache(QSMCC*PP) {
   QSMCC*prev=LC(PP)->CPrev,
        *next=LC(PP)->CNext;
   if (prev) LC(prev)->CNext=LC(PP)->CNext; else {
      long lg=PP->Size<TinySize16?0:1;
      QSMCC**chpos=Cache[lg]+(PP->Size>>(lg?SZSHL_L-SZSHL_S:0));
      *chpos=next;
   }
   if (next) LC(next)->CPrev=LC(PP)->CPrev;
   LC(PP)->CPrev=0; LC(PP)->CNext=0;
}

static inline void FillQSMCC(QSMCC*PP,u32t sz,long Owner,long Pool,u32t Back) {
   PP->Signature =QSMCC_SIGN;
   PP->GlobalID  =Owner;
   PP->ObjectID  =Pool;
   PP->Size      =sz>>4;
   PP->Link      =Back>>4;
}

static inline void FillQSMCC16(QSMCC*PP,u32t sz,long Owner,long Pool,u32t Back) {
   PP->Signature =QSMCC_SIGN;
   PP->GlobalID  =Owner;
   PP->ObjectID  =Pool;
   PP->Size      =sz;
   PP->Link      =Back;
}

typedef struct {
   void      *memPtr;
   u32t     usedSize;
} ExtPoolInfo;

//must be called inside SynchroL mutex
static int NewSmallPool(u32t Size16,u32t Caller) {
   ExtPoolInfo info[ExtPool];
   // reading r/o blocks
   int cnt=hlp_memqconst((u32t*)&info,ExtPool);

   while (cnt--&&FirstFree<MaxPool+ExtPool) {
      // block is not used already
      if (!memchrd((u32t*)&ExtPoolPtr,(u32t)info[cnt].memPtr,MaxPool+ExtPool)) {
         u32t usedSize = Round16(info[cnt].usedSize);
         u8t *blockPtr = (u8t*)info[cnt].memPtr + usedSize;
         usedSize = _64KB - usedSize;
         // too small block
         if (usedSize<_4KB) continue;
#if 1
         log_printf("* %08X %d\n",info[cnt].memPtr,info[cnt].usedSize);
#endif
         qsmcc Block=(QSMCC*)blockPtr;
         ExtPoolPtr[FirstFree] = info[cnt].memPtr;
         FillQSMCC(Block,usedSize-HeaderSize,FREEQSMCC,FREEQSMCC,0);
         SmallPool [FirstFree++] = Block;
         AddToCache(Block);
         Block++;
         // size 0 is a special entry!
         FillQSMCC(Block,0,FREEQSMCC,FREEQSMCC,usedSize-HeaderSize);
         // this block can help us ;)
         if (Size16<=usedSize-HeaderSize*2>>4) return 1;
      }
   }
   CurMemUsed+=PoolBlocks;
   if (MaxMemUsed<CurMemUsed) MaxMemUsed=CurMemUsed;
   // there is no suitable const block, allocating full size pool
   void *newBlock=mget(PoolBlocks);
   if (!newBlock)
      if (HaltOnNoMem) (*MemError)(RET_NOMEM,"NewSmallPool()",Caller,0);
         else return 0;
   if (FirstFree>=MaxPool+ExtPool) {
      (*MemError)(RET_BUFOVERFLOW,"NewSmallPool()",Caller,0);
      return 0;
   } else {
      qsmcc Block=(QSMCC*)newBlock;
      FillQSMCC(Block,PoolBlocks-HeaderSize,FREEQSMCC,FREEQSMCC,0);
      ExtPoolPtr[FirstFree]  =0;
      SmallPool [FirstFree++]=Block;
      AddToCache(Block);
      Block++;
      // size 0 is a special entry!
      FillQSMCC(Block,0,FREEQSMCC,FREEQSMCC,PoolBlocks-HeaderSize);
      return 1;
   }
}

static long AllocLargeSlot(u32t Caller) {
   for (long ll=0;ll<LargeFree;ll++) if (!LargePool[ll]) return ll;
   if (LargeFree+1>=MaxLargePool) (*MemError)(RET_BUFOVERFLOW,"AllocLarge()",Caller,0);
   return LargeFree++;
}

static QSMCC*SmallAlloc(long Owner,long Pool,u32t Size16,u32t Caller) {
   // if (MultiThread) MutexGrab(SynchroL);

   u32t lg=Size16<TinySize16?0:1,
           bp=Size16>>(lg?SZSHL_L-SZSHL_S:0);
   QSMCC*res=Cache[lg][bp+lg];
   if (!res)                 // search in same size blocks
      while (++bp<TinySize16)
         if ((res=Cache[lg][bp])!=NULL) break;
   if (!res) {
      if (!lg) bp=0;
      while (++bp<TinySize16) // search in large blocks
         if ((res=Cache[1][bp])!=NULL) break;
      if (!res) {             // alloc new pool
         int HaltSv=HaltOnNoMem;
         if (Owner==500) HaltOnNoMem=0;
         int rc=NewSmallPool(Size16,Caller);
         HaltOnNoMem=HaltSv;
         if (!rc) {
            // if (MultiThread) MutexRelease(SynchroL);
            return 0;
         }
         res=SmallPool[FirstFree-1];
      }
   }
   if (res->Signature!=QSMCC_SIGN)
      (*MemError)(RET_MCBDESTROYD,"SmallAlloc()",Caller,res);
   RmvFromCache(res);
   res->GlobalID  =Owner;
   res->ObjectID  =Pool;
   if (res->Size>Size16+HeaderSize16) {
      // Build Next MCB only if at least free 16 bytes present
      long bs=res->Size-Size16;
      res->Size=Size16;

      qsmcc PP(res); PP++;
      FillQSMCC16(PP,bs,FREEQSMCC,FREEQSMCC,Size16);
      qsmcc next(PP); next++;
      if (next->Size&&next->GlobalID==FREEQSMCC) {
         if (next->Signature!=QSMCC_SIGN)
            (*MemError)(RET_MCBDESTROYD,"SmallAlloc()",Caller,next);
         next->Signature=0; // clean up signature
         RmvFromCache(next);
         PP->Size+=next->Size;
         next++;
      }
      next->Link=PP->Size;
      AddToCache(PP);
   }
   // if (MultiThread) MutexRelease(SynchroL);
   return res;
}

extern "C" void* memAlloc(long Owner,long Pool,u32t Size) {
   if (!Size) return NULL;
   if (!MMInited) memInit();
   u32t Caller=CALLER(Owner);
   Size=Round16(Size+HeaderSize);
   qsmcc result;
   if (ParanoidalChk) CheckMgr(Caller);
   if (Size<MaxSmallAlloc) result=SmallAlloc(Owner,Pool,Size>>4,Caller); else {
      Size=RoundLarge(Size);
      if (Size>MaxLargeAlloc) result=0; else {
         CurMemUsed+=Size;
         if (MaxMemUsed<CurMemUsed) MaxMemUsed=CurMemUsed;
         result=(QSMCC*)mget(Size);
      }
      if (!result) {
         if (Owner!=500&&HaltOnNoMem) (*MemError)(RET_NOMEM,"AllocLarge()",Caller,0);
            else return 0;
      } else {
         // if (MultiThread) MutexGrab(SynchroG);
         FillQSMCC16(result,Size>>LargeShift,Owner,Pool,AllocLargeSlot(Caller));
         LargePool[result->Link]=result;
         result->Signature=QSMCL_SIGN;
         // if (MultiThread) MutexRelease(SynchroG);
      }
   }
   if (ParanoidalChk) CheckMgr(Caller);
   if (!result) return NULL;
   result+=HeaderSize;
   if (ClearBlocks) memZero(result);
   return result;
}

extern "C" void* memAllocZ(long Owner,long Pool,u32t Size) {
   void *rc = memAlloc(Owner,Pool,Size);
   /* clear block if no "clear all" flags and block is small (QSINIT large
      alloc is always cleared) */
   if (!ClearBlocks && rc && Size<MaxSmallAlloc) memZero(rc);
   return rc;
}

static inline QSMCC*TryToRefBlock(void*MM,const char *FromWhere,u32t Caller) {
   QSMCC*mm=(QSMCC*)((char*)MM-HeaderSize);
   if ((mm->Signature&CHECK_MASK)!=QSMCC_SIGN) (*MemError)(RET_INVALIDPTR,FromWhere,Caller,MM);
   return mm;
}

static inline QSMCC*TryToRefNonFatal(void*MM) {
   QSMCC*mm=(QSMCC*)((char*)MM-HeaderSize);
   if (!MM||(mm->Signature&CHECK_MASK)!=QSMCC_SIGN) mm=0;
   return mm;
}

#define ChkMergeStr "CheckMerge()"

static int CheckMerge(QSMCC*MM,u32t Caller) {
   /* free block and check for merging with next free block */
   if ((MM->Signature&CHECK_MASK)!=QSMCC_SIGN) (*MemError)(RET_INVALIDPTR,ChkMergeStr,Caller,MM);
      else
   if (MM->GlobalID==FREEQSMCC) (*MemError)(RET_DOUBLEFREE,ChkMergeStr,Caller,MM); else {
      if (MM->Signature==QSMCL_SIGN) { // free global block
         long ii=MM->Link;
         if (LargePool[ii]==MM) {
            // if (MultiThread) MutexGrab(SynchroG);
            MM->Signature=0; // clean up signature
            CurMemUsed-=MM->Size<<LargeShift;
            mfree(MM);
            LargePool[ii]=NULL;
            while (!LargePool[LargeFree-1]&&LargeFree) LargeFree--;
            // if (MultiThread) MutexRelease(SynchroG);
         } else (*MemError)(RET_INVALIDPTR,ChkMergeStr,Caller,MM);
      } else {                // free small block
         // if (MultiThread) MutexGrab(SynchroL);
         MM->GlobalID=FREEQSMCC;
         MM->ObjectID=FREEQSMCC;
         qsmcc next(MM);
         next++;
         while (next->Size&&next->GlobalID==FREEQSMCC) {
            next->Signature=0; // clean up signature
            RmvFromCache(next);
            MM->Size+=next->Size; next++;
         }
         qsmcc prev(MM);
         while (prev->Link) {
            prev--;
            if (prev->GlobalID==FREEQSMCC) {
               RmvFromCache(prev);
               prev->Size+=MM->Size;
               MM->Signature=0;
               MM=prev;
            } else break;
         }
         next->Link=MM->Size;
         AddToCache(MM);
         int gone=0;
         if (MM->Size<<4==PoolBlocks-HeaderSize) {
            while (FirstFree&&SmallPool[FirstFree-1]->Size<<4==PoolBlocks-HeaderSize&&
                 SmallPool[FirstFree-1]->GlobalID==FREEQSMCC)
            {
               RmvFromCache(SmallPool[--FirstFree]);
               SmallPool[FirstFree]->Signature=0;
               if ((u32t)MM-(u32t)SmallPool[FirstFree]<PoolBlocks) gone=1;
               if (!ExtPoolPtr[FirstFree]) mfree(SmallPool[FirstFree]);
                  else ExtPoolPtr[FirstFree]=0;
               SmallPool[FirstFree]=0;
               CurMemUsed-=PoolBlocks;
            }
         }
         // if (MultiThread) MutexRelease(SynchroL);
         return gone;
      }
   }
   return 0;
}


// free memory. Can be used both pointer(s) & QSMCC(s)
extern "C" void memFree(void* M) {
   if (!MMInited) return;
   if (!M) return; //!!warning! added for clib free() compatibility

   u32t Caller=CALLER(M);
   if (ParanoidalChk) CheckMgr(Caller);
   QSMCC*mm=(QSMCC*)((char*)M-HeaderSize);
   CheckMerge(mm,Caller);
   if (ParanoidalChk) CheckMgr(Caller);
}

#define memDupStr "memDup()"

extern "C" void* memDup(void *M) {
   if (!MMInited) return 0;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefBlock((u8t*)M,memDupStr,Caller);
   u32t size=actsize(mm);
   void *mb=memAlloc(mm->GlobalID,mm->ObjectID,size-HeaderSize);
   if (!mb) return NULL;
   memcpy(mb,M,size-HeaderSize);
   return mb;
}

#define memReallocStr "memRealloc()"

// realloc memory block.
extern "C" void *memRealloc(void *M,u32t NewSize) {
   if (!NewSize) NewSize=16;
   if (!M) return memAlloc(0,0,NewSize);
   if (!MMInited) return 0;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefBlock((u8t*)M,memReallocStr,Caller),result=NULL;

   NewSize=Round16(NewSize+HeaderSize);
   u32t size=actsize(mm);
   if (NewSize==size) return M;

   if (ParanoidalChk) CheckMgr(Caller);

   if (mm->Signature==QSMCL_SIGN) {
      NewSize=RoundLarge(NewSize);
      if (NewSize==size) return M;
      if (NewSize>=MaxSmallAlloc) {
         if (mm!=LargePool[mm->Link]) (*MemError)(RET_MCBDESTROYD,memReallocStr,Caller,mm);
         // if (MultiThread) MutexGrab(SynchroG);
         // hlp_memrealloc() will zero the end of new block for as
         result=(QSMCC*)hlp_memrealloc(mm,NewSize);
         if (result!=(QSMCC*)NULL) {
            LargePool[result->Link]=result;
            result->Size=NewSize>>LargeShift;
         }
         // if (MultiThread) MutexRelease(SynchroG);
      }
   }
   if (!result) {
      // if (MultiThread) MutexGrab(SynchroL);
      u32t NewSize16=NewSize>>4;
      if (NewSize>size) {
         if (mm->Signature==QSMCC_SIGN) {  // small block processing
            qsmcc next=mm;
            next++;                         // try to enlarge small block
            long usize16=next->Size+mm->Size-NewSize16;
            if (next->Size && next->GlobalID==FREEQSMCC && usize16>=0) {
               qsmcc npsave=next;
               RmvFromCache(next);
               if (usize16>HeaderSize16) {
                  mm->Size=NewSize16;
                  next=mm; next++;
                  FillQSMCC16(next,usize16,FREEQSMCC,FREEQSMCC,NewSize16);
                  AddToCache(next);
                  next++;
                  next->Link=usize16;
               } else {
                  mm->Size+=next->Size;
                  next->Signature=0;
                  next++;
                  next->Link=mm->Size;
               }
               // zero additional part
               if (ClearBlocks) memset(npsave,0,NewSize-size);
               result=mm;
            }
         }
      } else
      if (NewSize16<mm->Size) {
         if (mm->Signature==QSMCC_SIGN) { // truncate small block
            if (NewSize16<mm->Size-(u32t)HeaderSize16) { // create free space if place avail.
               long usize16=mm->Size-NewSize16;
               qsmcc next(mm);
               mm->Size=NewSize16;
               next++;                    // create fake block & free it
               FillQSMCC16(next,usize16,0,0,NewSize16);
               qsmcc nxt2(next);
               nxt2++;
               nxt2->Link=usize16;
               CheckMerge(next,Caller);
            }
            result=mm;
         }
      }
      // if (MultiThread) MutexRelease(SynchroL);
   }
   if (!result) {                               // common stupid processing
      void *mb=memAlloc(mm->GlobalID,mm->ObjectID,NewSize-HeaderSize);
      if (!mb) return NULL;
      result=(QSMCC*)mb-1;
      u32t copysz = actsize(mm)-HeaderSize;

      if (copysz>NewSize-HeaderSize) copysz=NewSize-HeaderSize; else
      if (ClearBlocks && NewSize-HeaderSize>copysz)
         memset((u8t*)mb+copysz,0, NewSize-HeaderSize-copysz);

      memcpy(mb,M,copysz);
      memFree(M);
   }
   return result+=HeaderSize;
}

// zero entire memory block by it pointer.
extern "C" void memZero(void *M) {
   if (!MMInited) return;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefBlock((u8t*)M,"memZero()",Caller);
   memset(M,0,actsize(mm)-HeaderSize);
}

extern "C" unsigned long memBlockSize(void *M) {
   if (!MMInited) return 0;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefBlock((u8t*)M,"memBlockSize()",Caller);
   return actsize(mm)-HeaderSize;
}

extern "C" unsigned long memGetObjInfo(void *M,long *Owner,long *Pool) {
   if (!MMInited) return 0;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefNonFatal((u8t*)M);
   if (!mm) return 0;
   if (Owner) *Owner=mm->GlobalID;
   if (Pool)  *Pool =mm->ObjectID;
   return actsize(mm)-HeaderSize;
}

extern "C" int __stdcall memSetObjInfo(void *M,long Owner,long Pool) {
   if (!MMInited||Owner==FREEQSMCC||Pool==FREEQSMCC) return 0;
   u32t Caller=CALLER(M);
   qsmcc mm=TryToRefNonFatal((u8t*)M);
   if (!mm) return 0;
   mm->GlobalID = Owner;
   mm->ObjectID = Pool;
   return 1;
}

void __stdcall memGetUniqueID(long *Owner,long *Pool) {
   //EnterUniqueSection(&SynchroPool);
   *Owner=GETOWNERID;
   *Pool =UniquePool++;
   //LeaveUniqueSection(&SynchroPool);
}


// alias for memset()
extern "C" void memZeroBlock(void* P,u32t Size) {
   memset(P,0,Size);
}

// free by Owner/Pool
extern "C" unsigned long memFreePool(long Owner,long Pool) {
   if (!MMInited) return 0;
   long ii;
   u32t Caller=CALLER(Owner), blockcnt=0;
   // if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->GlobalID==Owner&&PP->ObjectID==Pool) {
            blockcnt++;
            if (CheckMerge(PP,Caller)) break;
         }
         PP++;
      }
   }
   //if (MultiThread) LeaveUniqueSection(&SynchroL);
   //if (MultiThread) EnterUniqueSection(&SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii])
         if (LargePool[ii]->GlobalID==Owner&&LargePool[ii]->ObjectID==Pool) {
            CheckMerge(LargePool[ii],Caller); blockcnt++;
         }
   //if (MultiThread) LeaveUniqueSection(&SynchroG);
   return blockcnt;
}

extern "C" unsigned long memFreeByOwner(long Owner) {
   if (!MMInited) return 0;
   long ii;
   u32t Caller=CALLER(Owner), blockcnt=0;
   //if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->GlobalID==Owner) {
            blockcnt++;
            if (CheckMerge(PP,Caller)) break;
         }
         PP++;
      }
   }
   //if (MultiThread) LeaveUniqueSection(&SynchroL);
   //if (MultiThread) EnterUniqueSection(&SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii])
         if (LargePool[ii]->GlobalID==Owner) {
            CheckMerge(LargePool[ii],Caller);
            blockcnt++;
         }
   //if (MultiThread) LeaveUniqueSection(&SynchroG);
   return blockcnt;
}

extern "C" unsigned long memFreeByPool(long Pool) {
   if (!MMInited) return 0;
   long ii;
   u32t Caller=CALLER(Pool), blockcnt=0;
   //if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->ObjectID==Pool) {
            blockcnt++;
            if (CheckMerge(PP,Caller)) break;
         }
         PP++;
      }
   }
   //if (MultiThread) LeaveUniqueSection(&SynchroL);
   //if (MultiThread) EnterUniqueSection(&SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii])
         if (LargePool[ii]->ObjectID==Pool) {
            CheckMerge(LargePool[ii],Caller);
            blockcnt++;
         }
   //if (MultiThread) LeaveUniqueSection(&SynchroG);
   return blockcnt;
}

extern "C" void memFreeAll() {
   if (!MMInited) return;
   long ii;
   u32t Caller=(u32t)&memFreeAll;
   // if (MultiThread) MutexGrab(SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      if (!ExtPoolPtr[ii]) mfree(SmallPool[ii]); else ExtPoolPtr[ii]=0;
      SmallPool[ii]=0;
   }
   FirstFree=0;
   for (ii=0;ii<TinySize16;ii++) { Cache[0][ii]=NULL; Cache[1][ii]=NULL; }
   // if (MultiThread) MutexRelease(SynchroL);
   // if (MultiThread) MutexGrab(SynchroG);
   CurMemUsed-=FirstFree*PoolBlocks;
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) {
         CurMemUsed-=LargePool[ii]->Size<<LargeShift;
         mfree(LargePool[ii]);
         LargePool[ii]=0;
      }
   LargeFree=0;
   // if (MultiThread) MutexRelease(SynchroG);
}

extern "C" void memStat() {
   u32t sp=0,lp=0;
   int ii;
   char buf[512],*ep;
   u32t Caller=(u32t)&memStat;
   // if (MultiThread) MutexGrab(SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) lp+=LargePool[ii]->Size<<LargeShift;
   // if (MultiThread) MutexRelease(SynchroG);
   // if (MultiThread) MutexGrab(SynchroL);
   sp=FirstFree*PoolBlocks;
   ep=buf+sprintf(buf,"memStat(): %ld:%ld /",sp>>10,lp>>10);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      u32t maxsize=0,totsize=0;
      while (true) {
         if (!PP->Size) break;
         if (PP->Signature!=QSMCC_SIGN) TryToRefBlock((char*)PP+HeaderSize,"memStat()",Caller); // abort
         auto u32t size=PP->Size<<4;
         if (PP->GlobalID==FREEQSMCC) {
            if (size>maxsize) maxsize=size;
         } else totsize+=size;
         PP++;
      }
      if ((ep+=sprintf(ep,"%ld:%ld/",totsize*100/(PoolBlocks-32),maxsize>>10))-buf>=480) break;
   }
   strcpy(ep," \n");
   // if (MultiThread) MutexRelease(SynchroL);
   log_printf(buf);
}

typedef struct {
   long      owner;
   long       pool;
   u32t       size;
   u32t     blocks;
} StatMaxInfo;

static StatMaxInfo *StatMaxUpdate(StatMaxInfo *stat,int &cnt,int &max,QSMCC*block) {
   for (int ii=0;ii<cnt;ii++)
      if (block->GlobalID==stat[ii].owner&&block->ObjectID==stat[ii].pool) {
         stat[ii].size+=actsize(block);
         stat[ii].blocks++;
         return stat;
      }
   if (cnt+1==max) {
      StatMaxInfo *statnew=(StatMaxInfo*)hlp_memrealloc(stat,max*2*sizeof(StatMaxInfo));
      if (!statnew) {
         log_printf("No mem for memStatMax()");
         mfree(stat);
         return 0;
      }
      stat=statnew;
      max*=2;
   }
   stat[cnt].owner =block->GlobalID;
   stat[cnt].pool  =block->ObjectID;
   stat[cnt].size  =actsize(block);
   stat[cnt].blocks=1;
   cnt++;
   return stat;
}

extern "C" void memStatMax(int topcount) {
   int ii,alloc=4096,ca=0;
   u32t Caller=CALLER(topcount);
   log_printf("memStatMax():\n");
   StatMaxInfo *stat=(StatMaxInfo*)mget(alloc*sizeof(StatMaxInfo));
   // if (MultiThread) MutexGrab(SynchroG);
   for (ii=0;ii<LargeFree&&stat;ii++)
      if (LargePool[ii]) {
         if (LargePool[ii]->Signature!=QSMCL_SIGN)
            TryToRefBlock((char*)LargePool[ii]+HeaderSize,"memStatMax()",Caller);
         stat=StatMaxUpdate(stat,ca,alloc,LargePool[ii]);
      }
   // if (MultiThread) MutexRelease(SynchroG);
   // if (MultiThread) MutexGrab(SynchroL);

   for (ii=0;ii<FirstFree&&stat;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (stat) {
         if (!PP->Size) break;
         if (PP->Signature!=QSMCC_SIGN)
            TryToRefBlock((char*)PP+HeaderSize,"memStatMax()",Caller); // abort
         if (PP->GlobalID!=FREEQSMCC) stat=StatMaxUpdate(stat,ca,alloc,PP);
         PP++;
      }
   }
   // if (MultiThread) MutexRelease(SynchroL);
   if (!stat) {
      log_printf("No mem for memStatMax()");
      return;
   } else {
      if (ca>1) {
         int iCnt=ca,iOffset=iCnt/2;
         while (iOffset) {
            int iLimit=iCnt-iOffset, iSwitch;
            do {
               iSwitch=false;
               for(int iRow=0;iRow<iLimit;iRow++)
                  if (stat[iRow].size<stat[iRow+iOffset].size) {
                     StatMaxInfo swp;
                     swp=stat[iRow]; stat[iRow]=stat[iRow+iOffset]; stat[iRow+iOffset]=swp;
                     iSwitch=iRow;
                  }
            } while (iSwitch);
            iOffset=iOffset/2;
         }
      }
      if (topcount<=0) topcount=16;
      if (topcount>ca) topcount=ca;

      for (ii=0;ii<topcount;ii++) {
         if ((u32t)stat[ii].owner>(u32t)0xFFFFE000&&(u32t)stat[ii].pool<(u32t)FREEQSMCC) {
            char *Line=(char*)stat[ii].pool;
            long  line=stat[ii].owner&0x1FFF;
            log_printf("%4d. %8u bytes, %4u blocks, #%s %u\n",ii+1,stat[ii].size,
               stat[ii].blocks,(u32t)Line>0x1000?Line:"(null)",line);
         } else {
            log_printf("%4d. %8u bytes, %4u blocks, %u %u\n",ii+1,stat[ii].size,
               stat[ii].blocks,stat[ii].owner,stat[ii].pool);
         }
      }
      mfree(stat);
   }
}

static void PrintBlock(QSMCC *ptr,QSMCC *prev,QSMCC *pool) {
   char     buf[64];
   u8t    *addr[2];
   int     size[2];
   if (!prev)
      if ((u32t)ptr-(ptr->Link<<4)-(u32t)pool<PoolBlocks-32)
         prev=(QSMCC*)((u8t*)ptr-(ptr->Link<<4)); else
            log_printf("Unable to get prev - BackLink field destroyd\n");
   addr[0]=(u8t*)prev; if (prev) size[0]=prev->Size>16?16:prev->Size;
   addr[1]=(u8t*)ptr; size[1]=16;

   for (int pass=0;pass<2;pass++) {
      if (addr[pass]) {
         log_printf(pass?"Block start>>>\n":"Previous block start>>>\n");
         for (int lines=0;lines<size[pass];lines++) {
            u8t* eline=addr[pass]+(lines<<4);
            sprintf(buf,"%8.8X: ",(unsigned int)eline);
            for (int bytes=0;bytes<16;bytes++)
               sprintf(buf+10+bytes*3,"%2.2X ",eline[bytes]);
            buf[58]='\n';
            buf[59]=0;
            log_printf(buf);
         }
      }
   }
}

static void DumpBlock(QSMCC *ptr) {
   if (ptr) {
      char buf[64];
      log_printf("Block dump>>>\n");
      for (int lines=0;lines<10;lines++) {
         u8t* eline=(u8t*)ptr+(lines<<4);
         sprintf(buf,"%8.8X: ",(unsigned int)eline);
         for (int bytes=0;bytes<16;bytes++)
           sprintf(buf+10+bytes*3,"%2.2X ",eline[bytes]);
         buf[58]='\n';
         buf[59]=0;
         log_printf(buf);
      }
   }
}

static void PrintMCBErr(QSMCC*ptr,int type,QSMCC *pool) {
   int ii;
   log_printf("memCheckMgr() ERROR: 0x%8.8X TYPE %d\n",ptr,type);
   log_printf("FirstFree %d: [",FirstFree);
   for (ii=0;ii<FirstFree;ii++) log_printf(" 0x%08X",SmallPool[ii]);
   log_printf(" ]\n");
   if (ptr) PrintBlock(ptr,0,pool);
}

#define MCBError(PP,TYPE) { PrintMCBErr(PP,TYPE,SmallPool[ii]); result=0; break; }

// Check structures...
extern "C" int memCheckMgr() {
   if (!MMInited) return 0;
   long ii,jj,result=1;
   // if (MultiThread) MutexGrab(SynchroL);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc P=SmallPool[ii];
      u32t cnt=PoolBlocks-HeaderSize;
      while (true) {
         // checking for valid signature
         if (P->Signature!=QSMCC_SIGN) MCBError(P,1);
         // checking for invalid combination inside the free block
         if ((P->GlobalID==FREEQSMCC||P->ObjectID==FREEQSMCC)) {
            if (P->GlobalID!=P->ObjectID) MCBError(P,2);
            if (P->Size<TinySize16) {
               if (LC(P)->CNext)
                  if (LC(P)->CNext->Signature!=QSMCC_SIGN) MCBError(P,3);
               if (LC(P)->CPrev)
                  if (LC(P)->CPrev->Signature!=QSMCC_SIGN) MCBError(P,4);
            }
         }
         // checking for invalid size
         if ((u32t)P->Size<<4>cnt) MCBError(P,5);
         // adjusting size
         cnt-=P->Size<<4;
         if (P->Link) {
            qsmcc bk(P);
            bk--;
            if (bk->Size!=P->Link) MCBError(P,7);
            if (bk->Signature!=QSMCC_SIGN) MCBError(P,8);
         }
         P++;
         if (!P->Size) { if (cnt&&!ExtPoolPtr[ii]) MCBError(P,9); break; }
      }
   }
   for (jj=0;jj<2;jj++)
      for (ii=0;ii<TinySize16;ii++) {
         QSMCC*cc=Cache[jj][ii];
         while (cc) {
            if ((u32t)cc->Size>>(jj?SZSHL_L-SZSHL_S:0)!=(u32t)ii) MCBError(cc,10);
            cc=LC(cc)->CNext;
         }
      }
   // if (MultiThread) MutexRelease(SynchroL);
   // if (MultiThread) MutexGrab(SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) {
         if (LargePool[ii]->Signature!=QSMCL_SIGN) MCBError(LargePool[ii],12);
         // checking for pool field validity
         if (LargePool[ii]->Link!=ii) MCBError(LargePool[ii],11);
      }
   // if (MultiThread) MutexRelease(SynchroG);
   return result;
}

static void CheckMgr(u32t Caller) {
   if (!memCheckMgr()) (*MemError)(RET_CHECKFAILED,"CheckMgr()",Caller,0);
}

static void DumpNewReport(QSMCC*P,u32t idx) {
   if ((u32t)P->GlobalID>0xFFFFE000&&P->GlobalID<FREEQSMCC&&P->GlobalID!=GETOWNERID) {
      char *Line=(char*)P->ObjectID;
      long  line=P->GlobalID&0x1FFF;
      log_printf("%4d.[%8.8X] #%s %u - %u\n",idx,P,(u32t)Line>0x1000?Line:"(null)",line,actsize(P));
   } else {
      const char *cc=P->GlobalID==GETOWNERID?"<memGetUniqueID>":
         (P->GlobalID==FREEQSMCC?"free":"");
      log_printf("%4d.[%8.8X] % 8d % 8d - % 8d %s\n",idx,P,P->GlobalID,P->ObjectID,actsize(P),cc);
   }
}

#define LOG_PRN_BUFLEN    80

static void _memDumpLog(const char *TitleString,int DumpAll) {
   if (!MMInited) return;
   long ii,nbb;
   qsmcc  P;
   QSMCC *prev;
   log_printf("%s\n",TitleString);
   // if (MultiThread) MutexGrab(SynchroL);
   for (ii=0;ii<FirstFree;ii++) {
      /* some assumes used here:
         QSINIT always return memory aligned to 64k from own alloc. So if we
         have 64k aligned block - this is a common 256k pool, else - a part of
         64k block from r/o allocation */
      log_printf("**** Small Pool %d (0x%08X,%d)\n",ii,SmallPool[ii],
         (u32t)SmallPool[ii]&0xFFFF?_64KB-((u32t)SmallPool[ii]&0xFFFF):_256KB);

      P=SmallPool[ii]; nbb=0; prev=0;
      while (true) {
         if (P->Signature!=QSMCC_SIGN) {
            if (!DumpAll) DumpNewReport(prev,nbb);
            log_printf("Block header destroyd!\n");
            PrintBlock(P,prev,SmallPool[ii]);
            break;
         }
         nbb++;
         if (DumpAll) DumpNewReport(P,nbb);
         if (P->GlobalID==FREEQSMCC&&P->Size>0) {
            LC(P)->PoolIdx=ii;
            LC(P)->PoolPos=nbb;
         }
         if (!P->Size) break;
         prev=P;
         P++;
      }
   }
   // if (MultiThread) MutexRelease(SynchroL);
   // if (MultiThread) MutexGrab(SynchroG);
   log_printf("**** Large Pool (%d blocks)\n",LargeFree);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) {
         P=LargePool[ii];
         DumpNewReport(P,ii+1);
         if (P->Signature!=QSMCL_SIGN) {
            log_printf("Block header destroyd!\n");
            DumpBlock(P);
         }
      }
   // if (MultiThread) MutexRelease(SynchroG);

   if (DumpAll) {
      // if (MultiThread) MutexGrab(SynchroL);
      log_printf("**** Free Block Stack\n",LargeFree);

      for (long jj=0;jj<2;jj++)
         for (ii=jj==0?3:0;ii<TinySize16;ii++) {
            QSMCC*bbl=Cache[jj][ii];
            if (bbl) {
               static const char *badtype[]={"","(!!!)","<-!!","->!!","(pos!:"};
               char linebuf[LOG_PRN_BUFLEN], bstr[48];
               int   slen = LOG_PRN_BUFLEN;

               slen-=snprintf(linebuf, slen, "%4d->", ii<<(jj?SZSHL_L:SZSHL_S));

               while (bbl) {
                  int err=0;
                  if (bbl->GlobalID!=FREEQSMCC) err=1; else
                  if (!err&&LC(bbl)->CNext)
                    if (LC(bbl)->CNext->Signature!=QSMCC_SIGN) err=3;
                  if (err&&LC(bbl)->CPrev)
                    if (LC(bbl)->CPrev->Signature!=QSMCC_SIGN) err=2;

                  int len = snprintf(bstr, 48, "%d.%d%s",LC(bbl)->PoolIdx,
                                     LC(bbl)->PoolPos, badtype[err]);
                  if (err==4) len+=snprintf(bstr+len, 48-len, "%#8.8x)", bbl);

                  bstr[len++] = LC(bbl)->CNext?',':' ';
                  bstr[len++] = 0;
                  if (slen<len) {
                     log_printf("%s\n",linebuf);
                     linebuf[0] = 0;
                     slen = LOG_PRN_BUFLEN;
                  } else {
                     // do not use strcat() to prevent stdlib.h include
                     memcpy(linebuf+strlen(linebuf), bstr, len);
                     slen-=len;
                  }
                  bbl=LC(bbl)->CNext;
               }
               if (linebuf[0]) log_printf("%s\n",linebuf);
            }
         }
      // if (MultiThread) MutexRelease(SynchroL);
   }

   log_printf("*************************************\n");
   log_printf("* Total used            : %d\n",CurMemUsed);
   log_printf("* Peak memory usage     : %d\n",MaxMemUsed);
}

extern "C" void memDumpLog(const char *TitleString) {
   _memDumpLog(TitleString,true);
}

extern "C" void memSetOptions(long Options) {
   ParanoidalChk = (Options&QSMEMMGR_PARANOIDALCHK)!=0;
   ClearBlocks   = (Options&QSMEMMGR_ZEROMEM)!=0;
   HaltOnNoMem   = (Options&QSMEMMGR_HALTONNOMEM)!=0;
}

extern "C" u32t memGetOptions(void) {
  return (ParanoidalChk?QSMEMMGR_PARANOIDALCHK:0) |
         (ClearBlocks?QSMEMMGR_ZEROMEM:0) |
         (HaltOnNoMem?QSMEMMGR_HALTONNOMEM:0);
}

void DefMemError(u32t ErrType,const char *FromHere,u32t Caller,void *Pointer) {
   static const char *errmsg[7]={"", "Out of internal buffers", "Invalid pointer value",
      "Block header destroyd", "Out of memory", "Check error", "Double block free"};
   char msg[256];
   u32t object,offset;
   int len=snprintf(msg,256,"MEMMGR FATAL: %s. Function %s.\n              Caller ",
      errmsg[ErrType],FromHere);

   module *mi=mod_by_eip(Caller,&object,&offset,0);
   if (mi) len+=snprintf(msg+len, 256-len, "\"%s\" %d:%8.8X.",mi->name,object+1,offset);
      else len+=snprintf(msg+len, 256-len, "unknown.");

   if (ErrType==RET_INVALIDPTR||ErrType==RET_MCBDESTROYD)
      snprintf(msg+len, 256-len, " Location %8.8X.", Pointer);

   _memDumpLog(msg,0);
   vio_clearscr();
   vio_setcolor(VIO_COLOR_LRED);
   vio_charout('\n');
   vio_strout(msg);
   vio_charout('\n');
   _memDumpLog("Full memory dump",1);
   exit_pm32(QERR_MCBERROR);
}

extern "C" void memInit(void) {
   //if (MultiThrd&&!MultiThread) MultiThread=1;
   if (MMInited) return;
   MMInited   =1;
   MemError   =DefMemError;
   //MultiThread  =MultiThrd;
   memZeroBlock(&Cache,sizeof(Cache));
   memZeroBlock(&SmallPool,sizeof(SmallPool));
   memZeroBlock(&LargePool,sizeof(LargePool));
   memZeroBlock(&ExtPoolPtr,sizeof(ExtPoolPtr));
   CurMemUsed=0; MaxMemUsed=0; FirstFree=0;  LargeFree=0;
}

extern "C" void memDone(void) {
   if (!MMInited) return;
   memDumpLog("Memory stack before memDone");
   memFreeAll();
   MMInited=0;
}
