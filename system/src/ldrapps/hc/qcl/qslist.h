//
// QSINIT API
// dword list implementation
//
#ifndef QSINIT_LISTEXPORT
#define QSINIT_LISTEXPORT

#include "qstypes.h"
#include "qsclass.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @name predefined class types (for exi_createid)
//@{
#define EXID_dd_list    0x0001    ///< create dd_list (list of dword values)
#define EXID_dds_list   0x0002    ///< create dds_list (list of long values)
#define EXID_ptr_list   0x0003    ///< create ptr_list (list of void* values)
#define EXID_dq_list    0x0004    ///< create dq_list (list of u64t values)
#define EXID_dqs_list   0x0005    ///< create dqs_list (list of s64t values)
//@}

/** @fn dd_list_s.sort
    Sort list.
    @param  is_signed  List with signed values
    @param  forward    Sort direction */

/** @fn dd_list_s.delvalue
    Delete all entries with specified value.
    @param  value      Value to search
    @return number of deleted entries */

/// note! sort & delvalue functions are not present in classes.hpp

#define LIST_IMPL(struct_name,type_name)                                  \
   type_name  _exicc (*value   )(u32t index);                             \
   long       _exicc (*max     )(void);                                   \
   u32t       _exicc (*count   )(void);                                   \
   void       _exicc (*assign  )(struct struct_name *src);                \
   type_name* _exicc (*array   )(void);                                   \
   void       _exicc (*compact )(void);                                   \
   void       _exicc (*clear   )(void);                                   \
   void       _exicc (*exchange)(u32t idx1, u32t idx2);                   \
   void       _exicc (*insert  )(u32t pos, type_name value, u32t repeat); \
   void       _exicc (*insert_l)(u32t pos, struct struct_name *src);      \
   void       _exicc (*del     )(u32t pos, u32t count);                   \
   int        _exicc (*equal   )(struct struct_name *lst2);               \
   u32t       _exicc (*add     )(type_name value);                        \
   void       _exicc (*inccount)(u32t incvalue);                          \
   void       _exicc (*setcount)(u32t count);                             \
   long       _exicc (*indexof )(type_name value, u32t startpos);         \
   void       _exicc (*sort    )(int is_signed, int forward);             \
   u32t       _exicc (*delvalue)(type_name value);

/// list of ulongs
typedef struct dd_list_s {
   LIST_IMPL(dd_list_s,u32t)
} _dd_list, *dd_list;

/// list of longs
typedef struct dds_list_s {
   LIST_IMPL(dds_list_s,s32t)
} _dds_list, *dds_list;

/// list of pointers
typedef struct ptr_list_s {
   LIST_IMPL(ptr_list_s,pvoid)
   // call free for first..last items in list
   void _exicc (*freeitems)(u32t first, u32t last);
} _ptr_list, *ptr_list;

/// list of unsigned __int64
typedef struct dq_list_s {
   LIST_IMPL(dq_list_s,u64t)
} _dq_list, *dq_list;

/// list of __int64
typedef struct dqs_list_s {
   LIST_IMPL(dqs_list_s,s64t)
} _dqs_list, *dqs_list;

#ifdef __cplusplus
}
#endif

#endif // QSINIT_LISTEXPORT
