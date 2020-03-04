//
// QSINIT "start" module
// memory manager
// -------------------------------------
//
// * code was written in 1997 for own unfinished pascal compiler ;)
//   then it was ported to C and used on some real jobs (for 4 bln. users)
// * simplified version here, with 16 byte block header.
// * alloc/free pair speed is ~ the same with ICC/GCC runtime, but this
//   manager also provides file/line or id1/id2 pair for user needs in every
//   block as well as various debug listings.
//
// * mtlock is used here because of using this manager in MTLIB itself. I.e
//   system thread/fiber data is also stored in heap blocks.
//

#include "qsutil.h"
#include "memmgr.h"
#include "syslocal.h"

#define QSMCC_SIGN (0x484D5351) // QSMH
#define QSMCL_SIGN (0x4C4D5351) // QSML
#define CHECK_MASK (~0x4000000)

#define CALLER(x) (((u32t*)(&x))[-1])

#define _RET_BUFOVERFLOW    1
#define _RET_INVALIDPTR     2
#define _RET_MCBDESTROYD    3
#define _RET_NOMEM          4
#define _RET_CHECKFAILED    5
#define _RET_DOUBLEFREE     6

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
   QSMCC           *CPrev, // additional fields cannot use more than 16 bytes
                   *CNext;
   u32t               Sum; // MUST be equal to CPrev+CNext (just a crc)
   u16t           PoolIdx;
   u16t           PoolPos;
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

volatile static int     MMInited     = 0;
volatile static int     ParanoidalChk= 0; // check ALL at EVERY call (ULTRA slow)
volatile static int     ClearBlocks  = 0; // clear blocks on malloc
volatile static int     HaltOnNoMem  = 1; // abort() on out of memory
volatile static u32t    CurMemUsed   = 0; // total used mem size
volatile static u32t    MaxMemUsed   = 0; // max used mem size
extern "C"
volatile long           FirstFree    = 0;
volatile static long    LargeFree    = 0;
volatile static long    UniquePool   = 0;

#define FREEQSMCC       ((long)-1)
#define GETOWNERID      ((long)QSMEMOWNER_UNOWNER)
#define HeaderSize      long(sizeof(QSMCC))
// ** changeable constants
#define MAX_MEM_SIZE    (1<<28)          // 256Mb - total memory size (<=4Gb ;)
#define MaxLargePower2  31               // 2Gb - max large block size (must be >64k)
#define PoolBlocks      (1<<18)          // 256k - one pool size (64k..1Mb)
// ********
#define LargeShift      16               // sys alloc is always rounded to 64k
#define LargeRoundMask  ((1<<LargeShift)-1)
#define MaxPool         (MAX_MEM_SIZE/PoolBlocks) // max. number of small pools
#define ExtPool         256                       // number of ext. small variable
                                                  // size pools
#define MaxSmallCanBe   ((1<<16)-HeaderSize-16)   // max small block (abs.limit)
#define MaxSmallAlloc   ((56*1024)-HeaderSize)    // max small block
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
extern "C" void mem_init(void);

volatile static MemMgrError MemError=NULL;

static CacheArray Cache[2];

static void*      ExtPoolPtr[MaxPool+ExtPool];  // ext. pools pointers
static u16t      ExtPoolSize[MaxPool+ExtPool];  // ext. pool state (for stat)
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
   long  lg = PP->Size<TinySize16?0:1;
   PQSMCC *chpos = Cache[lg]+(PP->Size>>(lg?SZSHL_L-SZSHL_S:0));
   PQSMCC   next = *chpos;
   LC(PP)->CNext = next;
   LC(PP)->CPrev = NULL;
   LC(PP)->Sum   = (u32t)next;
   *chpos = PP;
   if (next) {
      LC(next)->CPrev = PP;
      LC(next)->Sum   = (u32t)LC(next)->CNext + (u32t)PP;
   }
}

static int RmvFromCache(QSMCC*PP) {
   QSMCC *prev = LC(PP)->CPrev,
         *next = LC(PP)->CNext;
   // control sum mismatch - header damaged!
   if ((u32t)prev + (u32t)next != LC(PP)->Sum) return 0;

   if (prev) {
      LC(prev)->CNext = LC(PP)->CNext;
      LC(prev)->Sum   = (u32t)LC(prev)->CPrev + (u32t)LC(prev)->CNext;
   } else {
      long lg = PP->Size<TinySize16?0:1;
      QSMCC**chpos=Cache[lg]+(PP->Size>>(lg?SZSHL_L-SZSHL_S:0));
      *chpos = next;
   }
   if (next) {
      LC(next)->CPrev = LC(PP)->CPrev;
      LC(next)->Sum   = (u32t)LC(next)->CPrev + (u32t)LC(next)->CNext;
   }
   LC(PP)->CPrev=0; LC(PP)->CNext=0; LC(PP)->Sum=0;
   return 1;
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

// must be called inside SynchroL mutex
static int NewSmallPool(u32t Size16, u32t Caller) {
   static ExtPoolInfo info[ExtPool];  // 1k, can use static inside lock
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
#if 0
         log_printf("* %08X %d\n",info[cnt].memPtr,info[cnt].usedSize);
#endif
         qsmcc Block=(QSMCC*)blockPtr;
         ExtPoolPtr [FirstFree] = info[cnt].memPtr;
         ExtPoolSize[FirstFree] = usedSize;
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
      if (HaltOnNoMem) (*MemError)(_RET_NOMEM,"NewSmallPool()",Caller,0);
         else return 0;
   if (FirstFree>=MaxPool+ExtPool) {
      (*MemError)(_RET_BUFOVERFLOW,"NewSmallPool()",Caller,0);
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
   if (LargeFree+1>=MaxLargePool) (*MemError)(_RET_BUFOVERFLOW,"AllocLarge()",Caller,0);
   return LargeFree++;
}

static QSMCC* SmallAlloc(long Owner, long Pool, u32t Size16, u32t Caller) {
   mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);

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
         int rc = NewSmallPool(Size16,Caller);
         if (!rc) {
            mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
            return 0;
         }
         res = SmallPool[FirstFree-1];
      }
   }
   if (res->Signature!=QSMCC_SIGN || !RmvFromCache(res))
      (*MemError)(_RET_MCBDESTROYD,"SmallAlloc()",Caller,res);
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
         if (next->Signature!=QSMCC_SIGN || !RmvFromCache(next))
            (*MemError)(_RET_MCBDESTROYD,"SmallAlloc()",Caller,next);
         next->Signature = 0; // clean up signature
         PP->Size+=next->Size;
         next++;
      }
      next->Link=PP->Size;
      AddToCache(PP);
   }
   mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
   return res;
}

extern "C" void* mem_alloc(long Owner, long Pool, u32t Size) {
   if (!Size) return NULL;
   if (!MMInited) mem_init();
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
         if (HaltOnNoMem) (*MemError)(_RET_NOMEM,"AllocLarge()",Caller,0);
            else return 0;
      } else {
         mt_swlock(); // if (MultiThread) MutexGrab(SynchroG);
         FillQSMCC16(result,Size>>LargeShift,Owner,Pool,AllocLargeSlot(Caller));
         LargePool[result->Link]=result;
         result->Signature=QSMCL_SIGN;
         mt_swunlock(); // if (MultiThread) MutexRelease(SynchroG);
      }
   }
   if (ParanoidalChk) CheckMgr(Caller);
   if (!result) return NULL;
   result+=HeaderSize;
   if (ClearBlocks) mem_zero(result);
   return result;
}

extern "C" void* mem_allocz(long Owner, long Pool, u32t Size) {
   void *rc = mem_alloc(Owner,Pool,Size);
   /* clear block if no "clear all" flags and block is small (QSINIT large
      alloc is always cleared) */
   if (!ClearBlocks && rc && Size<MaxSmallAlloc) mem_zero(rc);
   return rc;
}

static inline QSMCC* TryToRefBlock(void*MM, const char *FromWhere, u32t Caller) {
   QSMCC *mm = (QSMCC*)((char*)MM-HeaderSize);
   if (!MM || (mm->Signature&CHECK_MASK)!=QSMCC_SIGN)
      (*MemError)(_RET_INVALIDPTR,FromWhere,Caller,MM);
   return mm;
}

static inline QSMCC* TryToRefNonFatal(void*MM) {
   QSMCC *mm = (QSMCC*)((char*)MM-HeaderSize);
   if (!MM || (mm->Signature&CHECK_MASK)!=QSMCC_SIGN) mm=0;
   return mm;
}

#define ChkMergeStr "CheckMerge()"

/** internal free.
    returns 0 for large pool and for completly released small pool and
    ptr to this FREE block, enlarged to all free blocks around it (i.e.,
    result ptr can be smaller, than source) */
static QSMCC* CheckMerge(QSMCC*MM, u32t Caller) {
   /* free block and check for merging with next free block */
   if ((MM->Signature&CHECK_MASK)!=QSMCC_SIGN) (*MemError)(_RET_INVALIDPTR,ChkMergeStr,Caller,MM);
      else
   if (MM->GlobalID==FREEQSMCC) (*MemError)(_RET_DOUBLEFREE,ChkMergeStr,Caller,MM); else {
      if (MM->Signature==QSMCL_SIGN) { // free global block
         long ii=MM->Link;
         if (LargePool[ii]==MM) {
            mt_swlock(); // if (MultiThread) MutexGrab(SynchroG);
            MM->Signature=0; // clean up signature
            CurMemUsed-=MM->Size<<LargeShift;
            mfree(MM);
            LargePool[ii]=NULL;
            while (!LargePool[LargeFree-1]&&LargeFree) LargeFree--;
            mt_swunlock(); // if (MultiThread) MutexRelease(SynchroG);
         } else (*MemError)(_RET_INVALIDPTR,ChkMergeStr,Caller,MM);
      } else {                // free small block
         mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);
         MM->GlobalID=FREEQSMCC;
         MM->ObjectID=FREEQSMCC;
         qsmcc next(MM);
         next++;
         while (next->Size&&next->GlobalID==FREEQSMCC) {
            // unlink from free stack and hang on damaged header
            if (!RmvFromCache(next))
               (*MemError)(_RET_MCBDESTROYD, ChkMergeStr, Caller, next);
            // clean up signature
            next->Signature=0;
            MM->Size+=next->Size; next++;
         }
         qsmcc prev(MM);
         while (prev->Link) {
            prev--;
            if (prev->GlobalID==FREEQSMCC) {
               if (!RmvFromCache(prev))
                  (*MemError)(_RET_MCBDESTROYD, ChkMergeStr, Caller, prev);
               prev->Size+=MM->Size;
               MM->Signature=0;
               MM=prev;
            } else break;
         }
         next->Link=MM->Size;
         AddToCache(MM);
         if (MM->Size<<4==PoolBlocks-HeaderSize) {
            while (FirstFree&&SmallPool[FirstFree-1]->Size<<4==PoolBlocks-HeaderSize&&
                 SmallPool[FirstFree-1]->GlobalID==FREEQSMCC)
            {
               QSMCC *fp = SmallPool[FirstFree-1];
               if (!RmvFromCache(fp))
                  (*MemError)(_RET_MCBDESTROYD, ChkMergeStr, Caller, fp);
               fp->Signature = 0;
               if (MM && (u32t)MM-(u32t)fp<PoolBlocks) MM = 0;

               SmallPool[--FirstFree] = 0;
               if (!ExtPoolPtr[FirstFree]) mfree(fp); else ExtPoolPtr[FirstFree] = 0;
               CurMemUsed-=PoolBlocks;
            }
         }
         mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
         return MM;
      }
   }
   return 0;
}


// free memory. Can be used both pointer(s) & QSMCC(s)
extern "C" void mem_free(void* M) {
   if (!MMInited) return;
   if (!M) return; //!!warning! added for clib free() compatibility

   u32t Caller=CALLER(M);
   if (ParanoidalChk) CheckMgr(Caller);
   QSMCC*mm=(QSMCC*)((char*)M-HeaderSize);
   CheckMerge(mm,Caller);
   if (ParanoidalChk) CheckMgr(Caller);
}

#define memDupStr "mem_dup()"

extern "C" void* mem_dup(void *M) {
   if (!MMInited) return 0;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefBlock((u8t*)M,memDupStr,Caller);
   u32t   size = actsize(mm);
   void    *mb = mem_alloc(mm->GlobalID,mm->ObjectID,size-HeaderSize);
   if (!mb) return NULL;
   memcpy(mb, M, size-HeaderSize);
   return mb;
}

#define memReallocStr "mem_realloc()"

// realloc memory block.
extern "C" void *mem_realloc(void *M, u32t NewSize) {
   if (!NewSize) NewSize=16;
   if (!M) return mem_alloc(0,0,NewSize);
   if (!MMInited) return 0;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefBlock((u8t*)M,memReallocStr,Caller), result=NULL;

   NewSize     = Round16(NewSize+HeaderSize);
   u32t   size = actsize(mm);
   if (NewSize==size) return M;

   if (ParanoidalChk) CheckMgr(Caller);

   if (mm->Signature==QSMCL_SIGN) {
      NewSize=RoundLarge(NewSize);
      if (NewSize==size) return M;
      if (NewSize>=MaxSmallAlloc) {
         if (mm!=LargePool[mm->Link]) (*MemError)(_RET_MCBDESTROYD,memReallocStr,Caller,mm);
         mt_swlock(); // if (MultiThread) MutexGrab(SynchroG);
         // hlp_memrealloc() will zero the end of new block for as
         result=(QSMCC*)hlp_memrealloc(mm,NewSize);
         if (result!=(QSMCC*)NULL) {
            LargePool[result->Link]=result;
            result->Size=NewSize>>LargeShift;
         }
         mt_swunlock(); // if (MultiThread) MutexRelease(SynchroG);
      }
   }
   if (!result) {
      mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);
      u32t NewSize16=NewSize>>4;
      if (NewSize>size) {
         if (mm->Signature==QSMCC_SIGN) {  // small block processing
            qsmcc next=mm;
            next++;                         // try to enlarge small block
            long usize16=next->Size+mm->Size-NewSize16;
            if (next->Size && next->GlobalID==FREEQSMCC && usize16>=0) {
               qsmcc npsave=next;
               if (!RmvFromCache(next))
                  (*MemError)(_RET_MCBDESTROYD,memReallocStr,Caller,next);
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
      mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
   }
   if (!result) {                               // common stupid processing
      void *mb = mem_alloc(mm->GlobalID, mm->ObjectID, NewSize-HeaderSize);
      if (!mb) return NULL;
      result=(QSMCC*)mb-1;
      u32t copysz = actsize(mm)-HeaderSize;

      if (copysz>NewSize-HeaderSize) copysz=NewSize-HeaderSize; else
      if (ClearBlocks && NewSize-HeaderSize>copysz)
         memset((u8t*)mb+copysz,0, NewSize-HeaderSize-copysz);

      memcpy(mb,M,copysz);
      mem_free(M);
   }
   return result+=HeaderSize;
}

// zero entire memory block by it pointer.
extern "C" void mem_zero(void *M) {
   if (!MMInited) return;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefBlock((u8t*)M,"mem_zero()",Caller);
   memset(M,0,actsize(mm)-HeaderSize);
}

extern "C" u32t mem_blocksize(void *M) {
   if (!MMInited) return 0;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefBlock((u8t*)M,"mem_blocksize()",Caller);
   return actsize(mm)-HeaderSize;
}

extern "C" u32t mem_getobjinfo(void *M, long *Owner, long *Pool) {
   if (!MMInited) return 0;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefNonFatal((u8t*)M);
   if (!mm) return 0;
   if (Owner) *Owner=mm->GlobalID;
   if (Pool)  *Pool =mm->ObjectID;
   return actsize(mm)-HeaderSize;
}

extern "C" int __stdcall mem_setobjinfo(void *M, long Owner, long Pool) {
   if (!MMInited||Owner==FREEQSMCC||Pool==FREEQSMCC) return 0;
   u32t Caller = CALLER(M);
   qsmcc    mm = TryToRefNonFatal((u8t*)M);
   if (!mm) return 0;
   mm->GlobalID = Owner;
   mm->ObjectID = Pool;
   return 1;
}

void __stdcall mem_uniqueid(long *Owner, long *Pool) {
   mt_swlock(); //EnterUniqueSection(&SynchroPool);
   *Owner = GETOWNERID;
   *Pool  = UniquePool++;
   mt_swunlock(); //LeaveUniqueSection(&SynchroPool);
}

// free by Owner/Pool
extern "C" u32t mem_freepool(long Owner, long Pool) {
   if (!MMInited) return 0;
   long ii;
   u32t Caller=CALLER(Owner), blockcnt=0;
   mt_swlock(); // if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->GlobalID==Owner && PP->ObjectID==Pool) {
            blockcnt++;
            PP = CheckMerge(PP,Caller);
            if (!PP) break;
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
   mt_swunlock(); //if (MultiThread) LeaveUniqueSection(&SynchroG);
   return blockcnt;
}

extern "C" u32t mem_freeowner(long Owner) {
   if (!MMInited) return 0;
   long ii;
   u32t Caller=CALLER(Owner), blockcnt=0;
   mt_swlock(); //if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0;ii<FirstFree;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->GlobalID==Owner) {
            blockcnt++;
            PP = CheckMerge(PP,Caller);
            if (!PP) break;
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
   mt_swunlock(); //if (MultiThread) LeaveUniqueSection(&SynchroG);
   return blockcnt;
}

extern "C" void mem_stat() {
   u32t     sp = 0, lp = 0;
   int      ii;
#define MSTAT_BSIZE 512
   char    buf[MSTAT_BSIZE], *ep;
   u32t Caller = (u32t)&mem_stat;

   mt_swlock(); // if (MultiThread) MutexGrab(SynchroG);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) lp+=LargePool[ii]->Size<<LargeShift;
   // if (MultiThread) MutexRelease(SynchroG);
   // if (MultiThread) MutexGrab(SynchroL);
   // calc real size in small pools
   for (ii=0; ii<FirstFree; ii++)
      sp += ExtPoolSize[ii]?ExtPoolSize[ii]:PoolBlocks;
   ep = buf + snprintf(buf, MSTAT_BSIZE, "mem_stat(): %u:%u /", sp>>10, lp>>10);
   for (ii=0; ii<FirstFree; ii++) {
      qsmcc PP(SmallPool[ii]);
      u32t  maxsize = 0, totsize = 0,
           poolsize = ExtPoolSize[ii]?ExtPoolSize[ii]:PoolBlocks;
      while (true) {
         if (!PP->Size) break;
         if (PP->Signature!=QSMCC_SIGN)
            TryToRefBlock((char*)PP+HeaderSize, "memStat()", Caller); // abort
         auto u32t size=PP->Size<<4;
         if (PP->GlobalID==FREEQSMCC) {
            if (size>maxsize) maxsize = size;
         } else totsize+=size;
         PP++;
      }
      if ((ep += snprintf(ep, 32, "%u:%u%%:%u/", poolsize>>10, totsize*100/(poolsize-32),
         maxsize>>10))-buf >= MSTAT_BSIZE-32) break;
   }
   strcpy(ep," \n");
   mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
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
         log_printf("No mem for mem_statmax()");
         mfree(stat);
         return 0;
      }
      stat=statnew;
      max*=2;
   }
   stat[cnt].owner = block->GlobalID;
   stat[cnt].pool  = block->ObjectID;
   stat[cnt].size  = actsize(block);
   stat[cnt].blocks= 1;
   cnt++;
   return stat;
}

// warning! buffer here is static. But this may distort mem_statmax() only
static const char *get_file_name(u32t address) {
   static char fnbuf[64];
   u32t   len = hlp_copytoflat(&fnbuf, address, get_flatds(), 63);
   fnbuf[len] = 0;
   if (!len) snprintf(fnbuf, 64, "(addr violation: %08X)", address);
   return fnbuf;
}

extern "C" void mem_statmax(int topcount) {
   int     alloc = 4096, ca = 0, ii;
   u32t   Caller = CALLER(topcount);
   log_printf("memStatMax():\n");
   StatMaxInfo *stat=(StatMaxInfo*)mget(alloc*sizeof(StatMaxInfo));
   mt_swlock(); // if (MultiThread) MutexGrab(SynchroG);
   for (ii=0;ii<LargeFree&&stat;ii++)
      if (LargePool[ii]) {
         if (LargePool[ii]->Signature!=QSMCL_SIGN)
            TryToRefBlock((char*)LargePool[ii]+HeaderSize,"mem_statmax()",Caller);
         stat=StatMaxUpdate(stat,ca,alloc,LargePool[ii]);
      }
   // if (MultiThread) MutexRelease(SynchroG);
   // if (MultiThread) MutexGrab(SynchroL);

   for (ii=0;ii<FirstFree&&stat;ii++) {
      qsmcc PP(SmallPool[ii]);
      while (stat) {
         if (!PP->Size) break;
         if (PP->Signature!=QSMCC_SIGN)
            TryToRefBlock((char*)PP+HeaderSize,"mem_statmax()",Caller); // abort
         if (PP->GlobalID!=FREEQSMCC) stat=StatMaxUpdate(stat,ca,alloc,PP);
         PP++;
      }
   }
   mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
   if (!stat) {
      log_printf("No mem for mem_statmax()");
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
         u32t owner = stat[ii].owner,
               pool = stat[ii].pool;
         if (owner>=QSMEMOWNER_LINENUM && owner<FREEQSMCC) {
            long  line = (owner&~QSMEMOWNER_LINENUM)+1;
            log_printf("%4d. %8u bytes, %4u blocks, #%s %u\n", ii+1, stat[ii].size,
               stat[ii].blocks, pool>0x1000?get_file_name(pool):"(null)", line);
         } else
         if (owner==QSMEMOWNER_COTHREAD) {
            mt_thrdata *th = (mt_thrdata*)pool;
            log_printf("%4d. %8u bytes, %4u blocks, PID %u, TID %u\n", ii+1,
               stat[ii].size, stat[ii].blocks, th->tiPID, th->tiTID);
         } else
         if (owner==QSMEMOWNER_COPROCESS) {
            mt_prcdata *pd = (mt_prcdata*)pool;
            log_printf("%4d. %8u bytes, %4u blocks, PID %u\n", ii+1,
               stat[ii].size, stat[ii].blocks, pd->piPID);

         } else
         if (owner==QSMEMOWNER_COLIB) {
            module *mh = (module*)pool;
            log_printf("%4d. %8u bytes, %4u blocks, module %s (%s)\n", ii+1,
               stat[ii].size, stat[ii].blocks, mh->name, mh->mod_path);
         } else {
            log_printf("%4d. %8u bytes, %4u blocks, %u %u\n", ii+1, stat[ii].size,
               stat[ii].blocks, owner, pool);
         }
      }
      mfree(stat);
   }
}

static void statinfo_add(u32t itype, QSMCC*ptr, mem_statdata *info) {
   if (itype!=QSMEMINFO_ALL) {
      u32t owner = ptr->GlobalID;
      switch (itype) {
         case QSMEMINFO_MODULE:
            if (owner!=QSMEMOWNER_COLIB || ptr->ObjectID!=info->ph) return;
            break;
         case QSMEMINFO_GLOBAL:
            if (owner==QSMEMOWNER_COLIB || owner==QSMEMOWNER_COPROCESS ||
               owner==QSMEMOWNER_COTHREAD) return;
            break;
         case QSMEMINFO_PROCESS:
            if (owner!=QSMEMOWNER_COPROCESS) return; else
               if (((mt_prcdata*)ptr->ObjectID)->piPID!=info->ph) return;
            break;
         case QSMEMINFO_THREAD:
            if (owner!=QSMEMOWNER_COTHREAD) return; else {
               mt_thrdata *td = (mt_thrdata*)ptr->ObjectID;
               if (td->tiPID!=info->ph || td->tiTID!=info->tid) return;
            }
            break;
         default:
            return;
      }
   }
   info->mem += (u32t)ptr->Size<<4;
   info->nblk++;
}

extern "C" void mem_statinfo(u32t itype, mem_statdata *info) {
   if (!MMInited) return;
   if (!info) return;
   info->mem  = 0;
   info->nblk = 0;
   if (itype<0 || itype>QSMEMINFO_THREAD) return;
   // zero cannot be handle or pid
   if (!info->ph && itype>=QSMEMINFO_MODULE) return;
   if (itype==QSMEMINFO_THREAD && !info->tid) return;

   u32t Caller = CALLER(itype), ii;
   mt_swlock(); // if (MultiThread) EnterUniqueSection(&SynchroL);
   if (ParanoidalChk) CheckMgr(Caller);
   for (ii=0; ii<FirstFree; ii++) {
      qsmcc PP(SmallPool[ii]);
      while (true) {
         if (!PP->Size) break;
         if (PP->Signature!=QSMCC_SIGN)
            TryToRefBlock((u8t*)PP+HeaderSize, "mem_statinfo()", Caller); // abort
         if (PP->GlobalID!=FREEQSMCC) statinfo_add(itype, PP, info);
         PP++;
      }
   }
   //if (MultiThread) LeaveUniqueSection(&SynchroL);
   //if (MultiThread) EnterUniqueSection(&SynchroG);
   for (ii=0; ii<LargeFree; ii++)
      if (LargePool[ii]) {
         if (LargePool[ii]->Signature!=QSMCL_SIGN)
            TryToRefBlock((u8t*)LargePool[ii]+HeaderSize, "mem_statinfo()", Caller);
         if (LargePool[ii]->GlobalID!=FREEQSMCC)
            statinfo_add(itype, LargePool[ii], info);
      }
   mt_swunlock(); //if (MultiThread) LeaveUniqueSection(&SynchroG);
}

static void PrintBlock(printf_function pfn, QSMCC *ptr, QSMCC *prev, QSMCC *pool) {
   u8t    *addr[2];
   u32t    size[2];
   if (!pfn) pfn = log_printf;
   if (!prev)
      if ((u32t)ptr-(ptr->Link<<4)-(u32t)pool<PoolBlocks-32)
         prev=(QSMCC*)((u8t*)ptr-(ptr->Link<<4)); else
            pfn("Unable to get prev - BackLink field destroyd\n");
   addr[0]=(u8t*)prev; if (prev) size[0]=prev->Size>16?16:prev->Size;
   addr[1]=(u8t*)ptr; size[1]=16;

   for (int pass=0; pass<2; pass++) {
      if (addr[pass]) {
         pfn(pass?"Block start>>>\n":"Previous block start>>>\n");
         for (int lines=0; lines<size[pass]; lines++) {
            u8t* lp = addr[pass]+(lines<<4);
            pfn("%8.8X: %16b\n", lp, lp);
         }
      }
   }
}

static void DumpBlock(printf_function pfn, QSMCC *ptr) {
   if (!pfn) pfn = log_printf;
   if (ptr) {
      pfn("Block dump>>>\n");
      for (int lines=0; lines<10; lines++) {
         u8t* lp = (u8t*)ptr+(lines<<4);
         pfn("%8.8X: %16b\n", lp, lp);
      }
   }
}

static void PrintMCBErr(QSMCC*ptr, int type, QSMCC *pool) {
   log_printf("memCheckMgr() ERROR: %08X TYPE %d\n", ptr, type);
   if (pool) {
      int ii;
      log_printf("FirstFree %d: [",FirstFree);
      for (ii=0;ii<FirstFree;ii++) log_printf(" %08X",SmallPool[ii]);
      log_printf(" ]\n");
      if (ptr) PrintBlock(0,ptr,0,pool);
   } else {
      if (ptr) DumpBlock(0,ptr);
   }
}

#define MCBError(PP,TYPE) { PrintMCBErr(PP,TYPE,SmallPool[ii]); result=0; break; }
#define MCBLErr(PP,TYPE) { PrintMCBErr(PP,TYPE,0); result=0; break; }

// Check structures...
extern "C" int mem_checkmgr() {
   if (!MMInited) return 0;
   long ii,jj,result=1;
   mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);
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
               QSMCCF *lp = LC(P);
               // check sum and only then links (both may be broken)
               if ((u32t)lp->CPrev+(u32t)lp->CNext!=lp->Sum) MCBError(P,13);
               if (lp->CNext)
                  if (lp->CNext->Signature!=QSMCC_SIGN) MCBError(P,3);
               if (lp->CPrev)
                  if (lp->CPrev->Signature!=QSMCC_SIGN) MCBError(P,4);
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
         if (LargePool[ii]->Signature!=QSMCL_SIGN) MCBLErr(LargePool[ii],12);
         // checking for pool field validity
         if (LargePool[ii]->Link!=ii) MCBLErr(LargePool[ii],11);
      }
   mt_swunlock(); // if (MultiThread) MutexRelease(SynchroG);
   return result;
}

static void CheckMgr(u32t Caller) {
   if (!mem_checkmgr()) (*MemError)(_RET_CHECKFAILED,"mem_checkmgr()",Caller,0);
}

static void DumpNewReport(printf_function pfn, QSMCC*P, u32t idx) {
   u32t owner = (u32t)P->GlobalID,
         pool = (u32t)P->ObjectID;
   if (!pfn) pfn = log_printf;
   if (owner>=QSMEMOWNER_LINENUM && owner<FREEQSMCC) {
      long line = (owner&~QSMEMOWNER_LINENUM)+1;
      pfn("%4d.[%8.8X] #%s %u - %u\n", idx, P,  pool>0x1000?
         get_file_name(pool):"(null)", line, actsize(P));
   } else
   if (owner==QSMEMOWNER_COTHREAD) {
      mt_thrdata *th = (mt_thrdata*)pool;
      if ((u32t)th<0x1000 || th->tiSign!=THREADINFO_SIGN)
         pfn("%4d.[%8.8X] !!! wrong tcb = %08X !!!\n", idx, P, th);
      else
         pfn("%4d.[%8.8X] pid %5d  tid %4d   - % 8d %s\n", idx, P,
            th->tiPID, th->tiTID, actsize(P), th->tiParent->piModule->mod_path);
   } else
   if (owner==QSMEMOWNER_COPROCESS) {
      mt_prcdata *pd = (mt_prcdata*)pool;
      pfn("%4d.[%8.8X] pid %5d             - % 8d %s\n", idx, P, pd->piPID,
         actsize(P), pd->piModule->mod_path);
   } else
   if (owner==QSMEMOWNER_COLIB) {
      module *mh = (module*)pool;
      pfn("%4d.[%8.8X] %-21s - %8d\n", idx, P, mh->name, actsize(P));
   } else {
      const char *cc = owner==GETOWNERID?"<memGetUniqueID>": (owner==FREEQSMCC?"free":"");
      pfn("%4d.[%8.8X] %10d %10d - %8d %s\n", idx, P, owner, pool, actsize(P), cc);
   }
}

#define LOG_PRN_BUFLEN    80

static void _memDumpLog(printf_function pfn, const char *Title, int DumpAll) {
   if (!MMInited) return;
   long ii,nbb;
   qsmcc  P;
   QSMCC *prev;
   if (!pfn) pfn = log_printf;
   if (Title) pfn("%s\n", Title);
   mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);
   for (ii=0;ii<FirstFree;ii++) {
      /* some assumes used here:
         QSINIT always return memory aligned to 64k from own alloc. So if we
         have 64k aligned block - this is a common 256k pool, else - a part of
         64k block from r/o allocation */
      pfn("**** Small Pool %d (0x%08X,%d)\n",ii,SmallPool[ii],
         (u32t)SmallPool[ii]&0xFFFF?_64KB-((u32t)SmallPool[ii]&0xFFFF):_256KB);

      P=SmallPool[ii]; nbb=0; prev=0;
      while (true) {
         if (P->Signature!=QSMCC_SIGN) {
            if (!DumpAll) DumpNewReport(pfn,prev,nbb);
            pfn("Block header destroyd!\n");
            PrintBlock(pfn,P,prev,SmallPool[ii]);
            break;
         }
         nbb++;
         if (DumpAll) DumpNewReport(pfn,P,nbb);
         if (P->GlobalID==FREEQSMCC&&P->Size>0) {
            LC(P)->PoolIdx = ii;
            LC(P)->PoolPos = nbb;
         }
         if (!P->Size) break;
         prev=P;
         P++;
      }
   }
   // if (MultiThread) MutexRelease(SynchroL);
   // if (MultiThread) MutexGrab(SynchroG);
   pfn("**** Large Pool (%d blocks)\n",LargeFree);
   for (ii=0;ii<LargeFree;ii++)
      if (LargePool[ii]) {
         P=LargePool[ii];
         DumpNewReport(pfn,P,ii+1);
         if (P->Signature!=QSMCL_SIGN) {
            pfn("Block header destroyd!\n");
            DumpBlock(pfn,P);
         }
      }
   mt_swunlock(); // if (MultiThread) MutexRelease(SynchroG);

   if (DumpAll) {
      mt_swlock(); // if (MultiThread) MutexGrab(SynchroL);
      pfn("**** Free Block Stack\n",LargeFree);

      for (long jj=0;jj<2;jj++)
         for (ii=jj==0?3:0;ii<TinySize16;ii++) {
            QSMCC*bbl=Cache[jj][ii];
            if (bbl) {
               static const char *badtype[]={"","(!!!)","<-!!","->!!","(sum)"};
               char linebuf[LOG_PRN_BUFLEN], bstr[48];
               int   slen = LOG_PRN_BUFLEN;

               slen-=snprintf(linebuf, slen, "%4d->", ii<<(jj?SZSHL_L:SZSHL_S));

               while (bbl) {
                  QSMCCF *bBL = LC(bbl);
                  int     err = 0;
                  if (bbl->GlobalID!=FREEQSMCC) err=1; else
                     if ((u32t)bBL->CPrev+(u32t)bBL->CNext!=bBL->Sum) err=4;
                  if (!err && bBL->CNext)
                    if (bBL->CNext->Signature!=QSMCC_SIGN) err=3;
                  if (err && bBL->CPrev)
                    if (bBL->CPrev->Signature!=QSMCC_SIGN) err=2;

                  int len = snprintf(bstr, sizeof(bstr), "%u.%u%s%s", bBL->PoolIdx,
                     bBL->PoolPos, badtype[err], err>=3?",<<broken>>":(bBL->CNext?",":""));

                  if (slen<++len) {
                     pfn("%s\n",linebuf);
                     linebuf[0] = 0;
                     slen = LOG_PRN_BUFLEN;
                  }
                  // do not use strcat() to prevent stdlib.h include
                  memcpy(linebuf+strlen(linebuf), bstr, len);
                  slen -= len;
                  // is CNext valid?
                  if (err>=3) break; else bbl = bBL->CNext;
               }
               if (linebuf[0]) pfn("%s\n",linebuf);
            }
         }
      mt_swunlock(); // if (MultiThread) MutexRelease(SynchroL);
   }

   pfn("*************************************\n");
   pfn("* Total used            : %d\n",CurMemUsed);
   pfn("* Peak memory usage     : %d\n",MaxMemUsed);
}

extern "C" void mem_dumplog(printf_function pfn, const char *TitleString) {
   _memDumpLog(pfn,TitleString,true);
}

extern "C" void mem_setopts(u32t Options) {
   ParanoidalChk = (Options&QSMEMMGR_PARANOIDALCHK)!=0;
   ClearBlocks   = (Options&QSMEMMGR_ZEROMEM)!=0;
   HaltOnNoMem   = (Options&QSMEMMGR_HALTONNOMEM)!=0;
}

extern "C" u32t mem_getopts(void) {
   return (ParanoidalChk?QSMEMMGR_PARANOIDALCHK:0) |
          (ClearBlocks?QSMEMMGR_ZEROMEM:0) |
          (HaltOnNoMem?QSMEMMGR_HALTONNOMEM:0);
}

void DefMemError(u32t ErrType,const char *FromHere, u32t Caller, void *Pointer) {
   static const char *errmsg[7]={"", "Out of internal buffers", "Invalid pointer value",
      "Block header destroyd", "Out of memory", "Check error", "Double block free"};
   char     msg[256];
   u32t  object, offset;
   module   *mi;
   int      len = snprintf(msg, 256, "\nMEMMGR FATAL: %s. Function %s.\n              Caller ",
                           errmsg[ErrType],FromHere);
   int  locaddr = ErrType==_RET_INVALIDPTR || ErrType==_RET_MCBDESTROYD || ErrType==_RET_DOUBLEFREE;
   // additional lock for safeness (forewer - we never return)
   mt_swlock();
   mi = mod_by_eip(Caller, &object, &offset, 0);
   if (mi) len+=snprintf(msg+len, 256-len, "\"%s\" %u:%08X.", mi->name, object+1, offset);
      else len+=snprintf(msg+len, 256-len, "unknown (%08X).", Caller);
   if (locaddr) snprintf(msg+len, 256-len, " Location %08X.", Pointer);
   _memDumpLog(0,msg,0);

   /* a kind of trap screen,
      all output should not use memory manager, at least it looks so. */
   mt_swlock();
   trap_screen_prepare();

   u32t px = 3, py = 3, hdt = locaddr?6:2;

   vio_drawborder(1, 2, 78, hdt+5, 0x4F);
   vio_setpos(py, px); vio_strout(" \xFE HEAP FATAL ERROR \xFE");
   py+=2;
   snprintf(msg, 256, "%s (%08X). Caller - %s", errmsg[ErrType], Pointer, FromHere);
   vio_setpos(py++, px); vio_strout(msg);
   if (mi) snprintf(msg, 256, "Module \"%s\" %u:%08X", mi->name, object+1, offset);
      else snprintf(msg, 256, "Source location unknown (%08X)", Caller);
   vio_setpos(py, px); vio_strout(msg);
   py+=2;

   if (locaddr) {
      u32t addr = (u32t)Pointer - 16;
      for (int ii=0; ii<4; ii++) {
         u8t   mb[16];
         u32t len = hlp_copytoflat(&mb, addr, get_flatds(), 16),
             mpos = snprintf(msg, 256, "%08X:", addr);
         // memory may be unreadable
         for (u32t lp=0; lp<len; lp++)
            mpos += snprintf(msg+mpos, 256-mpos, " %02X", mb[lp]);
         while (len++<16) strncat(msg, " ??", 256);

         vio_setpos(py++, px);
         vio_strout(msg);
         addr+=16;
      }
   }
   _memDumpLog(0,"Full memory dump",1);
   reipl(QERR_MCBERROR);
}

extern "C" void mem_init(void) {
   if (MMInited) return;
   MMInited   = 1;
   MemError   = DefMemError;
   memset(&Cache, 0, sizeof(Cache));
   memset(&SmallPool, 0, sizeof(SmallPool));
   memset(&LargePool, 0, sizeof(LargePool));
   memset(&ExtPoolPtr, 0, sizeof(ExtPoolPtr));
   CurMemUsed=0; MaxMemUsed=0; FirstFree=0;  LargeFree=0;
}
