#include "stdlib.h"
#include "memmgr.h"

void *operator new(unsigned size) {
   return memAlloc(2,0,size);
}

void *operator new [](unsigned size) {
   return memAlloc(2,0,size);
}

void operator delete(void *ptr) {
   if (ptr) memFree(ptr);
}

void operator delete [](void *ptr) {
   if (ptr) memFree(ptr);
}
