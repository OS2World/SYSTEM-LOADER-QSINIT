//
// QSINIT runtime
// rt<->qs conversion funcs
//
#ifndef QSINIT_RTCVT
#define QSINIT_RTCVT

#ifdef __cplusplus
#include "classes.hpp"
#include "qsshell.h"

/// convert TStrings to string list for usage in pure C code
str_list* str_getlist(TStringVector &lst);

/// convert string list to TStrings
template <class F> 
void str_getstrs(const str_list* lst, F &strs) {
   strs.Clear();
   if (!lst||!lst->count) return;
   for (u32t ll=0;ll<lst->count;ll++) strs.Add(lst->item[ll]);
}

/// convert string list to TStrings
template <class F> 
void str_getstrs2(const str_list* lst, F &strs, u32t start) {
   strs.Clear();
   if (!lst||!lst->count) return;
   for (u32t ll=start;ll<lst->count;ll++) strs.Add(lst->item[ll]);
}

#endif // __cplusplus
#endif // QSINIT_RTCVT
