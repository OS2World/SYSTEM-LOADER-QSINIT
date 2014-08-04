#ifndef HM_DISK_IO_HEADER
#define HM_DISK_IO_HEADER

#define INCL_BASE
#include <os2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STDCALL _stdcall

/** Opens high memory.
    @param        exclusive   Lock driver until own exit to be exclusive user.
    @param  [out] handle      Handle.
    @return 0 or OS/2 error code. ERROR_WRITE_PROTECT returned if memory
            already locked to exclusive mode by someone. */
APIRET STDCALL hm_open(int exclusive, PULONG handle);


/** Query memory size.
    @param        handle      Handle.
    @return available memory size in sectors (512 bytes) or 0 on error */
ULONG  STDCALL hm_getsize(ULONG handle);

/** Read high memory.
    I/O is sector based (i.e. by 512 bytes only).

    It is recommended to use only PAGE ALIGNED buffer for read and write ops, else
    it will get a large overhead.

    @param        handle   Handle.
    @param        sector   Start sector.
    @param        count    Number of sectors (do not use too long values, because
                           copying disables current CPU interrups for a long time).
    @param        data     Buffer for data
    @param  [out] actual   Pointer to number of actual transfered sectors (can be 0)
    @return 0 or OS/2 error code */
APIRET STDCALL hm_read (ULONG handle, ULONG sector, ULONG count, PVOID data, PULONG actual);


/** Write to high memory.
    All hm_read() notes applied here too.
    Write op will return error if another one app open driver in "exclusive" mode.

    Be careful - this is direct write to mounted ram disk (if HD4DISK.ADD used
    without /i option)

    @param        handle   Handle.
    @param        sector   Start sector.
    @param        count    Number of sectors.
    @param        data     Buffer for data
    @param  [out] actual   Pointer to number of actual transfered sectors (can be 0)
    @return 0 or OS/2 error code */
APIRET STDCALL hm_write(ULONG handle, ULONG sector, ULONG count, PVOID data, PULONG actual);


typedef struct {
   ULONG     physpage;          ///< physical memory page (i.e ADDR >> 12)
   ULONG      sectors;          ///< number of sectors (512 bytes)
} HM_LOCATION_INFO;

/** Query information about memory.

    Function can be called in "exclusive" mode only.

    Because "emulated disk can use multiple memory blocks (including memory
    below 4Gb), information about all its memory stored in a table.

    This function queries it. Function can be called with 0 entries to query
    table size.

    @param        handle   Handle.
    @param  [out] memloc   Destination buffer with number of "entries", can be 0
    @param       entries   Number of entries in "memloc" table, can be 0
    @return 0 on error or number of entries in table */
ULONG STDCALL hm_getinfo(ULONG handle, HM_LOCATION_INFO *memloc, ULONG entries);


/** Query emulated HDD geometry.
    Any of out parameters can be 0.

    @param        handle   Handle.
    @param  [out] active   Disk is active (if active==0 - "HD4DISK.ADD /i" used,
                           i.e. disk memory available only to this API
    @param  [out] cyls     Number of cylinders
    @param  [out] heads    Number of heads
    @param  [out] spt      Number of sectors per track
    @return 0 or OS/2 error code */
APIRET STDCALL hm_chsgeo(ULONG handle, PULONG active,
                         PULONG cyls, PULONG heads, PULONG spt);


/** Close high memory.
    @param  handle         Handle.
    @return 0 or OS/2 error code */
APIRET STDCALL hm_close(ULONG handle);

#ifdef __cplusplus
}
#endif

#endif // HM_DISK_IO_HEADER
