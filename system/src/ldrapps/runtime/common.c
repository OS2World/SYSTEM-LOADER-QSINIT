//
// QSINIT
// le/lx module common runtime code (both exe & dll)
//

#include "clib.h"
#include "malloc.h"
#include "qspdata.h"
#include "qsmodext.h"
#include "qstask.h"

#if QSMEMOWNER_COTHREAD+(1<<MAX_TID_BITS) != QSMEMOWNER_LINENUM
#error MAX_TID_BITS in not match to QSMEMOWNER_COTHREAD!
#endif

extern u32t _RandSeed;
extern u32t    _IsDll;
extern u32t   _Module;

u32t __stdcall random(u32t range);

int  __stdcall rand(void) { return random(32768); }

void __stdcall srand(u32t seed) { _RandSeed = seed; }

static mt_prcdata *get_procinfo(void) {
   process_context *pq = mod_context();
   return (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
}

void* __stdcall malloc(unsigned long size) {
   if (_IsDll) return mem_alloc(QSMEMOWNER_COLIB, _Module, size); else
      return mem_alloc(QSMEMOWNER_COPROCESS, (u32t)get_procinfo(), size);
}

#if 0 // removed to asm to be simple jmp (to save caller addr for memmgr error dump)
void  __stdcall free(void *ptr) {
   mem_free(ptr);
}
#endif

void* __stdcall calloc(unsigned long n, unsigned long size) {
   size *= n;
   if (_IsDll) return mem_allocz(QSMEMOWNER_COLIB, _Module, size); else
      return mem_allocz(QSMEMOWNER_COPROCESS, (u32t)get_procinfo(), size);
}

void* __stdcall realloc(void *old_blk, unsigned long size) {
   if (!old_blk) return malloc(size);
   return mem_realloc(old_blk, size);
}

void* __stdcall __realloc_shared_rt(void *old_blk, unsigned long size,
                                    const char *file, unsigned long line) 
{
   if (!old_blk)
      return mem_alloc((line&MAXFILELINEMASK)-1|QSMEMOWNER_LINENUM, (long)file, size);
   else {
      void *rc = mem_realloc(old_blk, size);
      if (rc) mem_setobjinfo(rc, (line&MAXFILELINEMASK)-1|QSMEMOWNER_LINENUM, (long)file);
      return rc;
   }
}

void* __stdcall malloc_thread(unsigned long size) {
   return mem_alloc(QSMEMOWNER_COTHREAD+mt_getthread()-1, mod_getpid(), size);
}

void* __stdcall calloc_thread(unsigned long n, unsigned long size) {
   return mem_allocz(QSMEMOWNER_COTHREAD+mt_getthread()-1, mod_getpid(), n*size);
}

void* __stdcall realloc_thread(void *old_blk, unsigned long size) {
   if (!old_blk)
      return mem_alloc(QSMEMOWNER_COTHREAD+mt_getthread()-1, mod_getpid(), size);
   else {
      void *rc = mem_realloc(old_blk, size);
      if (rc) mem_setobjinfo(rc, QSMEMOWNER_COTHREAD+mt_getthread()-1, mod_getpid());
      return rc;
   }
}

int __stdcall mem_modblock(void *block) {
   return mem_setobjinfo(block, QSMEMOWNER_COLIB, _Module);
}

// unpublished
int __stdcall mem_modblockex(void *block, u32t module) {
   return mem_setobjinfo(block, QSMEMOWNER_COLIB, module);
}

int __stdcall mem_threadblock(void *block) {
   return mem_setobjinfo(block, QSMEMOWNER_COTHREAD+mt_getthread()-1, mod_getpid());
}

// unpublished
int __stdcall mem_threadblockex(void *block, u32t pid, u32t tid) {
   return mem_setobjinfo(block, QSMEMOWNER_COTHREAD+tid-1, pid);
}

int __stdcall mem_localblock(void *block) {
   return mem_setobjinfo(block, QSMEMOWNER_COPROCESS, (u32t)get_procinfo());
}
