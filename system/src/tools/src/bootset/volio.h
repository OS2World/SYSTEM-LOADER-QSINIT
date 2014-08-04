#ifndef bootset_volio_h
#define bootset_volio_h

#include "sp_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *volh;

/// open volume
Bool volume_open (char letter, volh *handle);

/// return volume size in sectors
u32  volume_size (volh handle, u32 *sectorsize);

/// read sectors
u32  volume_read (volh handle, u32 sector, u32 count, void *data);

/// write sectors
u32  volume_write(volh handle, u32 sector, u32 count, void *data);

/// close volume
void volume_close(volh handle);

#ifdef __cplusplus
}
#endif
#endif // bootset_volio_h
