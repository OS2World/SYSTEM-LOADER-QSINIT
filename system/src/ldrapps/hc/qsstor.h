//
// QSINIT
// tiny storage implementation (some kind of ini/registry for system vars)
//
#ifndef QSINIT_STORAGE
#define QSINIT_STORAGE

#include "qstypes.h"
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

/** save user data. 
    @param  entry  Key name.
    @param  data   Pointer to data, use 0 to delete key.
    @param  len    Length to data.
    @param  copy   Make copy of data or save pointer (boolean) */
void  _std sto_save(const char *entry, void *data, u32t len, int copy);

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
