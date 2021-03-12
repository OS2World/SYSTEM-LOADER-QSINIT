#include "qs_rt.h"

str_list* str_getlist(TStringVector &lst) {
   l len=sizeof(str_list)+lst.Count()*sizeof(char*), dlen=0, ii;

   for (ii=0;ii<=lst.Max();ii++) dlen+=lst[ii].trim().length()+1;
   str_list *rc=(str_list*)malloc(len+dlen);
   if (!rc) return 0;
   rc->count=lst.Count();
   char *pos=(char*)rc+len;
   for (ii=0;ii<=lst.Max();ii++) {
      /* trim spaces around '='. 
         pure "C" code in bootos2/console.dll assume this */
      int vpos=lst[ii][0]!=';'?lst[ii].cpos('='):-1;
      if (vpos>1) lst[ii]=spstr(lst[ii],0,vpos).trim()+'='+
         spstr(lst[ii],vpos+1,lst[ii].length()).trim();
      // copying into buffer
      rc->item[ii]=pos;
      strcpy(pos,lst[ii]());
      pos+=lst[ii].length()+1;
   }
   return rc;
}

str_list* str_getlistpure(TStringVector &lst) {
   l len=sizeof(str_list)+lst.Count()*sizeof(char*), dlen=0, ii;

   for (ii=0;ii<=lst.Max();ii++) dlen+=lst[ii].length()+1;
   str_list *rc=(str_list*)malloc(len+dlen);
   if (!rc) return 0;
   rc->count=lst.Count();
   char *pos=(char*)rc+len;
   for (ii=0;ii<=lst.Max();ii++) {
      rc->item[ii]=pos;
      strcpy(pos,lst[ii]());
      pos+=lst[ii].length()+1;
   }
   return rc;
}
