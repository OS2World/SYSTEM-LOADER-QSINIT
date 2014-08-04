#ifdef __cplusplus

#ifndef SP_NEW_REPLACEMENT
#define SP_NEW_REPLACEMENT

#include "malloc.h"
#include "stdlib.h"
#define spmalloc  malloc
#define sprealloc realloc
#define spfree    free

//extern void *operator new( size_t );
//extern void *operator new( size_t, void * );
//extern void *operator new []( size_t );
//extern void *operator new []( size_t, void * );
//extern void operator delete(void *p);
//extern void operator delete [](void *);

#endif

#endif
