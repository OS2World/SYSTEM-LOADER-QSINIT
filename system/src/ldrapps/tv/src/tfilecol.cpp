/*------------------------------------------------------------*/
/* filename -       tfilecol.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TFileCollection member functions          */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*                                                            */
/*                                                            */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*------------------------------------------------------------*/


#define Uses_TFileCollection
#define Uses_TSearchRec
#include <tv.h>
#include <string.h>

inline const char *getName(void *k) {
   return ((TSearchRec *)k)->name;
}

inline int attr(void *k) {
   return ((TSearchRec *)k)->attr;
}

int TFileCollection::compare(void *key1, void *key2) {
   const char *name1 = getName(key1);
   const char *name2 = getName(key2);
   int code = stricmp(name1, name2);
   if (code == 0) return 0;

   if (strcmp(name1, "..") == 0)
      return 1;
   if (strcmp(name2, "..") == 0)
      return -1;

   if ((attr(key1) & FA_DIREC) != 0 && (attr(key2) & FA_DIREC) == 0)
      return 1;
   if ((attr(key2) & FA_DIREC) != 0 && (attr(key1) & FA_DIREC) == 0)
      return -1;

   return code;
}

#ifndef NO_TV_STREAMS
TStreamable *TFileCollection::build() {
   return new TFileCollection(streamableInit);
}

void TFileCollection::writeItem(void *obj, opstream &os) {
   TSearchRec *item = (TSearchRec *)obj;
   os << item->attr << item->time << item->size;
   os.writeString(item->name);
}

void *TFileCollection::readItem(ipstream &is) {
   TSearchRec *item = new TSearchRec;
   is >> item->attr >> item->time >> item->size;
   is.readString(item->name, sizeof(item->name));
   return item;
}
#endif  // ifndef NO_TV_STREAMS


