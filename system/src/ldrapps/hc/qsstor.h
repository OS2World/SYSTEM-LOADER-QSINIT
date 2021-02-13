//
// QSINIT
// tiny storage implementation (some kind of ini/registry for system vars)
//
#ifndef QSINIT_STORAGE
#define QSINIT_STORAGE

#include "qstypes.h"
#include "qsshell.h"
#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

/// read first dword of stored data
u32t  _std sto_dword(const char *entry);

/// get pointer to stored data
void *_std sto_data(const char *entry);

/// get size of to stored data
u32t  _std sto_size(const char *entry);

/** return a copy of stored data in application owned heap block.
    @param        entry  Key name.
    @param  [out] psize  Ptr to size of returned data, can be 0.
    @return data copy in heap block or 0 if no such key */
void *_std sto_datacopy(const char *entry, u32t *psize);

/** save user data. 
    @param  entry  Key name.
    @param  data   Pointer to data, use 0 to delete key.
    @param  len    Length to data.
    @param  copy   Make copy of data or save pointer (boolean) */
void  _std sto_save(const char *entry, void *data, u32t len, int copy);

/** save user data (dword). 
    @param  entry  Key name.
    @param  value  Dword value. */
void  _std sto_savedword(const char *entry, u32t value);

/** list storage keys. 
    @param  match  Match pattern (in ren syntax, i.e., "cmd_h_*"), empty/0
                   means full list
    @param  type   Return variable type in 1st name character (name starts from
                   2nd in this case). @ means pointer to a value, * - regular
                   value.
    @return string list with existing key names in the application owned heap
            block, 0 can be returned if list is empty */
str_list* _std sto_list(const char *match, int type);

/// delete key
#define sto_del(entry) sto_save(entry, 0, 0, 0)

#ifdef STORAGE_INTERNAL

#pragma pack(1)
typedef struct {
   u32t      data;
   u32t       len;
   u8t      isptr;
   char  name[11];
} stoinit_entry;
#pragma pack()

stoinit_entry *_std sto_init(void);

/// flush delayed/realmode storage entries to actual storage.
void  _std sto_flush(void);

#endif // STORAGE_INTERNAL

#ifdef __cplusplus
}
#endif
#pragma pack()
#endif // QSINIT_STORAGE
