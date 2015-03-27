#include "qcl/qslist.h"
#include "qslog.h"
#include "classes.hpp"

/* we`re using bad methods of initing/clearing list fields here (without 
   constructor/destructor calling) */

#define LSTd_SIGN  MAKEID4('L','S','T','D')
#define LSTq_SIGN  MAKEID4('L','S','T','Q')

/// number of methods in list classes
#define LIST_M_COUNT  18

typedef List<u64t> TQList;

struct listd_ptr {
   u32t     sign;
   TUList   list;
};

struct listq_ptr {
   u32t     sign;
   TQList   list;
};

#define list_ptr listd_ptr

static const char *errmsg = "warning! invalid list ptr: %08X (%d)\n";

#define list_type(size) list##size##_ptr

#define makep_checkret_void(x,size)                          \
   list_type(size) *x=(list_type(size)*)lst;                 \
   if (!lst||x->sign!=LST##size##_SIGN) {                    \
      log_printf(errmsg,lst,__LINE__);                       \
      return;                                                \
   }                                                         
                                                             
#define makep_checkret_value(x,v,size)                       \
   list_type(size) *x=(list_type(size)*)lst;                 \
   if (!lst||x->sign!=LST##size##_SIGN) {                    \
      log_printf(errmsg,lst,__LINE__);                       \
      return v;                                              \
   }                                                         
                                                             
#define lst_create(size)                                     \
void _std lst##size##_create(void *inst,void *lst) {         \
   list_type(size) *lp = (list_type(size)*)lst;              \
   lp->sign = LST##size##_SIGN;                              \
   lp->list.Init();                                          \
}                                                            
                                                             
#define lst_free(size)                                       \
void _std lst##size##_free(void *inst,void *lst) {           \
   makep_checkret_void(lp,size);                             \
   lp->sign = 0;                                             \
   lp->list.Clear();                                         \
}                                                            
                                                             
#define lst_value(size,tpret)                                \
tpret _std lst##size##_value(void *lst, u32t index) {        \
   makep_checkret_value(lp,0,size);                          \
   return lp->list[index];                                   \
}

#define lst_max(size)                                        \
long _std lst##size##_max(void *lst) {                       \
   makep_checkret_value(lp,-1,size);                         \
   return lp->list.Max();                                    \
}

#define lst_count(size)                                      \
u32t _std lst##size##_count(void *lst) {                     \
   makep_checkret_value(lp,0,size);                          \
   return lp->list.Count();                                  \
}

#define lst_assign(size)                                     \
void _std lst##size##_assign(void *lst, void *src) {         \
   list_type(size) *lps=(list_type(size)*)src;               \
   makep_checkret_void(lpd,size);                            \
   if (!lps||lps->sign!=LST##size##_SIGN) return;            \
   lpd->list = lps->list;                                    \
}

#define lst_array(size,tpret)                                \
tpret*_std lst##size##_array(void *lst) {                    \
   makep_checkret_value(lp,0,size);                          \
   return (tpret*)lp->list.Value();                          \
}

#define lst_compact(size)                                    \
void _std lst##size##_compact(void *lst) {                   \
   makep_checkret_void(lp,size);                             \
   lp->list.Compact();                                       \
}

#define lst_clear(size)                                      \
void _std lst##size##_clear(void *lst) {                     \
   makep_checkret_void(lp,size);                             \
   lp->list.Clear();                                         \
}

#define lst_exchange(size)                                   \
void _std lst##size##_exchange(void *lst, u32t idx1, u32t idx2) { \
   makep_checkret_void(lp,size);                             \
   lp->list.Exchange(idx1,idx2);                             \
}

#define lst_insert(size,tpv)                                 \
void _std lst##size##_insert(void *lst, u32t pos, tpv value, u32t repeat) { \
   makep_checkret_void(lp,size);                             \
   lp->list.Insert(pos,value,repeat);                        \
}

#define lst_insert_l(size)                                   \
void _std lst##size##_insert_l(void *lst, u32t pos, void *src) { \
   list_type(size) *lps=(list_type(size)*)src;               \
   makep_checkret_void(lpd,size);                            \
   if (!lps||lps->sign!=LST##size##_SIGN) return;            \
   lpd->list.Insert(pos,lps->list);                          \
}

#define lst_del(size)                                        \
void _std lst##size##_del(void *lst, u32t pos, u32t count) { \
   makep_checkret_void(lp,size);                             \
   lp->list.Delete(pos,count);                               \
}

#define lst_equal(size)                                      \
int  _std lst##size##_equal(void *lst, void *src) {          \
   list_type(size) *lps=(list_type(size)*)src;               \
   makep_checkret_value(lpd,0,size);                         \
   if (!lps||lps->sign!=LST##size##_SIGN) return 0;          \
   return lpd->list.Equal(lps->list);                        \
}

#define lst_add(size,tpv)                                    \
u32t _std lst##size##_add(void *lst, tpv value) {            \
   makep_checkret_value(lp,0,size);                          \
   return lp->list.Add(value);                               \
}

#define lst_inccount(size)                                   \
void _std lst##size##_inccount(void *lst, u32t incvalue) {   \
   makep_checkret_void(lp,size);                             \
   lp->list.IncCount(incvalue);                              \
}

#define lst_setcount(size)                                   \
void _std lst##size##_setcount(void *lst, u32t count) {      \
   makep_checkret_void(lp,size);                             \
   lp->list.SetCount(count);                                 \
}

#define lst_indexof(size,tpv)                                \
long _std lst##size##_indexof(void *lst, tpv value, u32t startpos) { \
   makep_checkret_value(lp,-1,size);                         \
   return lp->list.IndexOf(value,startpos);                  \
}

#define lst_sort(size,tu,tl)                                 \
void _std lst##size##_sort(void *lst, int is_signed, int forward) { \
   makep_checkret_void(lp,size);                             \
                                                             \
   if (lp->list.Count()>1) {                                 \
      l iCnt=lp->list.Count(),iOffset=iCnt/2;                \
      while (iOffset) {                                      \
         l iLimit=iCnt-iOffset, iSwitch;                     \
         do {                                                \
            iSwitch=false;                                   \
            for (l iRow=0;iRow<iLimit;iRow++) {              \
               const tu* vptr = lp->list.Value();            \
               if (forward) {                                \
                  if (is_signed && (tl)vptr[iRow]>(tl)vptr[iRow+iOffset] ||      \
                     !is_signed && vptr[iRow]>vptr[iRow+iOffset])                \
                        { lp->list.Exchange(iRow,iRow+iOffset); iSwitch=iRow; }  \
               } else {                                                          \
                  if (is_signed && (tl)vptr[iRow]<(tl)vptr[iRow+iOffset] ||      \
                     !is_signed && vptr[iRow]<vptr[iRow+iOffset])                \
                        { lp->list.Exchange(iRow,iRow+iOffset); iSwitch=iRow; }  \
               }                                             \
            }                                                \
         } while (iSwitch);                                  \
         iOffset=iOffset/2;                                  \
      }                                                      \
   }                                                         \
}

#define lst_delvalue(size,tpv)                               \
u32t _std lst##size##_delvalue(void *lst, tpv value) {       \
   l idx, count = 0;                                         \
   makep_checkret_value(lp,0,size);                          \
   do {                                                      \
      if ((idx = lp->list.IndexOf(value))>=0) {              \
         count++;                                            \
         lp->list.Delete(idx);                               \
      }                                                      \
   } while(idx>=0);                                          \
   return count;                                             \
}

#define lst_freeitems(size)                                  \
void _std lst##size##_freeitems(void *lst, u32t first, u32t last) { \
   makep_checkret_void(lp,size);                             \
   if (!lp->list.Count()) return;                            \
   if (last>=lp->list.Count()) last=lp->list.Count()-1;      \
   for (d ii=first; ii<last; ii++)                           \
      free((void*)lp->list[ii]);                             \
}

// dword list implementation
lst_create(d) lst_free(d) lst_value(d,u32t) lst_max(d) lst_count(d)
lst_assign(d) lst_array(d,u32t) lst_compact(d) lst_clear(d) lst_exchange(d)
lst_insert(d,u32t) lst_insert_l(d) lst_del(d) lst_equal(d) lst_add(d,u32t)
lst_inccount(d) lst_setcount(d) lst_indexof(d,u32t) lst_sort(d,u32t,long)
lst_delvalue(d,u32t) lst_freeitems(d)

// qword list implementation
lst_create(q) lst_free(q) lst_value(q,u64t) lst_max(q) lst_count(q)
lst_assign(q) lst_array(q,u64t) lst_compact(q) lst_clear(q) lst_exchange(q)
lst_insert(q,u64t) lst_insert_l(q) lst_del(q) lst_equal(q) lst_add(q,u64t)
lst_inccount(q) lst_setcount(q) lst_indexof(q,u64t) lst_sort(q,u64t,s64t)
lst_delvalue(q,u64t)

// exported function lists
void *f_dd_list[LIST_M_COUNT+1] = { lstd_value, lstd_max, lstd_count, 
   lstd_assign, lstd_array, lstd_compact, lstd_clear, lstd_exchange, 
   lstd_insert, lstd_insert_l, lstd_del, lstd_equal, lstd_add, lstd_inccount,
   lstd_setcount, lstd_indexof, lstd_sort, lstd_delvalue, lstd_freeitems };

void *f_dq_list[LIST_M_COUNT] = { lstq_value, lstq_max, lstq_count, 
   lstq_assign, lstq_array, lstq_compact, lstq_clear, lstq_exchange, 
   lstq_insert, lstq_insert_l, lstq_del, lstq_equal, lstq_add, lstq_inccount,
   lstq_setcount, lstq_indexof, lstq_sort, lstq_delvalue };

// register
void register_lists(void) {
   u32t check = exi_register("dd_list", f_dd_list, LIST_M_COUNT, 
                sizeof(listd_ptr), lstd_create, lstd_free);
   if (check!=EXID_dd_list) {
      exi_unregister(check);
      log_printf("register lists err (%d)\n",check);
      return;
   }
   exi_register("dds_list", f_dd_list, LIST_M_COUNT, sizeof(listd_ptr), 
      lstd_create, lstd_free);
   exi_register("ptr_list", f_dd_list, LIST_M_COUNT+1, sizeof(listd_ptr), 
      lstd_create, lstd_free);
   exi_register("dq_list" , f_dq_list, LIST_M_COUNT, sizeof(listq_ptr), 
      lstq_create, lstq_free);
   exi_register("dqs_list", f_dq_list, LIST_M_COUNT, sizeof(listq_ptr), 
      lstq_create, lstq_free);
}
