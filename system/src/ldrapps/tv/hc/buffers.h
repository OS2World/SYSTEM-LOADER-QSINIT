/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   BUFFERS.H                                                             */
/*                                                                         */
/*   defines the functions getBufMem() and freeBufMem() for use            */
/*   in allocating and freeing viedo buffers                               */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#include <tvvo.h>

#if defined( Uses_TVMemMgr ) && !defined( __TVMemMgr )
#define __TVMemMgr

const int DEFAULT_SAFETY_POOL_SIZE = 4096;

class TBufListEntry {

private:

   TBufListEntry(void *&);
   ~TBufListEntry();

   void *operator new(size_t, size_t);
   void *operator new(size_t);
   void operator delete(void *);

   TBufListEntry *next;
   TBufListEntry *prev;
   void *&owner;

   static TBufListEntry *bufList;
   static Boolean freeHead();

   friend class TVMemMgr;
#if defined(__BORLANDC__) && !defined(__OS2__)
   friend void *cdecl operator new(size_t);
#else
   friend void *operator new(size_t);
#endif
   friend void *allocBlock( size_t );

};

class TVMemMgr {

public:

   TVMemMgr();

   static void resizeSafetyPool(size_t = DEFAULT_SAFETY_POOL_SIZE);
   static int safetyPoolExhausted();
   static void clearSafetyPool();

   static void allocateDiscardable(void *&, size_t);
   static void freeDiscardable(void *);
   static void suspend(void);

private:

   static void *safetyPool;
   static size_t safetyPoolSize;
   static int inited;
   static int initMemMgr();

};

#endif  // Uses_TVMemMgr

#include <tvvo2.h>

