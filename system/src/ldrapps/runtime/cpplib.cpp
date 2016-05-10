//
// QSINIT
// C++ runtime code (both exe & dll)
// --------------------------------------------------------------
// for exe module malloc will return process owned memory,
// for dll module - DLL owned (i.e. shared, but until DLL unload).
#include "stdlib.h"

void *operator new(unsigned size) {
   return malloc(size);
}

void *operator new [](unsigned size) {
   return malloc(size);
}

void operator delete(void *ptr) {
   if (ptr) free(ptr);
}

void operator delete [](void *ptr) {
   if (ptr) free(ptr);
}
