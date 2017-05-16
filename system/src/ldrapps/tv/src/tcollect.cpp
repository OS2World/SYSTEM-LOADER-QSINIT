// --------------------------------------------------------------
// Очень подлая ошибка заключается в том, что у TNSCollection
// был пустой деструктор и он естественно не освобождал массив указателей
// на элементы коллекции items ( void **items ).
// Пока TNSCollection и все что от нее
// происходит создается с помощью new - все OK т.к. механизм destroy
// вызывает shutDown() которая вызывает freeAll(), что приводит к
// установке count=0, ну а потом setLimit(0) вытирает items ( setLimit(0)
// освобождает items только при условии, что count равен нулю !!!). Механизм
// конечно не из наглядных, но, как говорится хозяин - барин, работало бы !
// Но позабыв о деструкторе, "товарищи" из Borland позабыли также о том, что
// объект ipstream имеет своим членом TPReadObjs objs, а TPReadObjs через два
// поколения происходит от злополучной TNSCollection и при уничтожении
// ipstream память под items в TPReadObjs objs не освобождается. Далее эта
// ошибка благополучно передается по дереву в iopstream и fpstream ( ох уж
// это объектно-ориентированное программирование !!!). Короче плохо, просто
// неприлично ошибаться в базовых классах.
//                                                        Рязанов Д.В.
// -----------------------------------------------------------------------
/*------------------------------------------------------------*/
/* filename -       tcollect.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TNSCollection member functions            */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TNSCollection
#define Uses_opstream
#define Uses_ipstream
#define Uses_TCollection
#include <tv.h>

#include <stdlib.h>
#if !defined(__MSVC__)&&!defined(__IBMCPP__)&&!defined(__QSINIT__)
#include <mem.h>
#else
#include <string.h>
#endif

#ifdef __WATCOMC__
#pragma disable_message (919);
#endif

TNSCollection::TNSCollection(ccIndex aLimit, ccIndex aDelta) :
   count(0),
   items(0),
   limit(0),
   delta(aDelta),
   shouldDelete(True) 
{
   setLimit(aLimit);
}

TNSCollection::TNSCollection() :
   count(0),
   items(0),
   limit(0),
   delta(0),
   shouldDelete(True) 
{
   items = 0;
}

TNSCollection::~TNSCollection() {
   delete items;
}

void TNSCollection::shutDown() {
   if (shouldDelete)
      freeAll();
   else
      removeAll();
   setLimit(0);
   TObject::shutDown();
}

void *TNSCollection::at(ccIndex index) {
   if (index < 0 || index >= count)
      error(1,0);
   return items[index];
}

void TNSCollection::atRemove(ccIndex index) {
   if (index < 0 || index >= count)
      error(1,0);

   count--;
   memmove(&items[index], &items[index+1], (count-index)*sizeof(void *));
}

void TNSCollection::atFree(ccIndex index) {
   void *item = at(index);
   atRemove(index);
   freeItem(item);
}

void TNSCollection::atInsert(ccIndex index, void *item) {
   if (index < 0)
      error(1,0);
   if (count == limit)
      setLimit(count + delta);

   memmove(&items[index+1], &items[index], (count-index)*sizeof(void *));
   count++;

   items[index] = item;
}

void TNSCollection::atPut(ccIndex index, void *item) {
   if (index >= count)
      error(1,0);

   items[index] = item;
}

void TNSCollection::remove(void *item) {
   atRemove(indexOf(item));
}

void TNSCollection::removeAll() {
   count = 0;
}

void TNSCollection::error(ccIndex code, ccIndex) {
   exit(212 - code);
}

void *TNSCollection::firstThat(ccTestFunc Test, void *arg) {
   for (ccIndex i = 0; i < count; i++) {
      if (Test(items[i], arg) == True)
         return items[i];
   }
   return 0;
}

void *TNSCollection::lastThat(ccTestFunc Test, void *arg) {
   for (ccIndex i = count; i > 0; i--) {
      if (Test(items[i-1], arg) == True)
         return items[i-1];
   }
   return 0;
}

void TNSCollection::forEach(ccAppFunc action, void *arg) {
   for (ccIndex i = 0; i < count; i++)
      action(items[i], arg);
}

void TNSCollection::free(void *item) {
   remove(item);
   freeItem(item);
}

void TNSCollection::freeAll() {
   for (ccIndex i =  0; i < count; i++)
      freeItem(at(i));
   count = 0;
}

void TNSCollection::freeItem(void *item) {
   delete item;
}

ccIndex TNSCollection::indexOf(void *item) {
   for (ccIndex i = 0; i < count; i++)
      if (item == items[i])
         return i;

   error(1,0);
   return 0;
}

ccIndex TNSCollection::insert(void *item) {
   ccIndex loc = count;
   atInsert(count, item);
   return loc;
}

void TNSCollection::pack() {
   void **curDst = items;
   void **curSrc = items;
   void **last = items + count;
   while (curSrc < last) {
      if (*curSrc != 0)
         *curDst++ = *curSrc;
      curSrc++;
   }
}

void TNSCollection::setLimit(ccIndex aLimit) {
   if (aLimit < count)
      aLimit =  count;
   if (aLimit > maxCollectionSize)
      aLimit = maxCollectionSize;
   if (aLimit != limit) {
      void **aItems;
      if (aLimit == 0)
         aItems = 0;
      else {
         aItems = new void *[aLimit];

         if (!aItems) return; // BF:TNSCollection-2
         if (count!=0  && items!=0)
            memcpy(aItems, items, count*sizeof(void *));
      }
      delete items;
      items =  aItems;
      limit =  aLimit;
   }
}

#ifndef NO_TV_STREAMS
void TCollection::write(opstream &os) {
   os << count << limit << delta;
   for (ccIndex idx = 0; idx < count; idx++)
      writeItem(items[idx], os);
}

void *TCollection::read(ipstream &is) {
   int limit;
   is >> count >> limit >> delta;
   // limit = 0; ?
   setLimit(limit);
   for (ccIndex idx = 0; idx < count; idx++)
      items[idx] = readItem(is);
   return this;
}


TCollection::TCollection(StreamableInit) {
}
#endif  // ifndef NO_TV_STREAMS
