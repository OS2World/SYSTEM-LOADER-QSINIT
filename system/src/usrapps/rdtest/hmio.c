#include "hmio.h"
#include <stdlib.h>

#define CHECK_SIGN    0x6F694D48    // HMio
#define FFFF          0xFFFFFFFF

#define CHECK_VERSION          1
#define IOCTL_CAT           0xC0
#define IOCTL_F_CHECK        100
#define IOCTL_F_SIZE         101
#define IOCTL_F_READ         102
#define IOCTL_F_WRITE        103
#define IOCTL_F_CHS          104
#define IOCTL_F_INFO         105

typedef struct {
   ULONG    sector;
   ULONG     count;
   ULONG   lindata;   // flat address
   int       write;
} RW_PKT;

typedef struct {
   ULONG      cyls;
   ULONG     heads;
   ULONG       spt;
   ULONG    active;
} GEO_PKT;

typedef struct {
   ULONG    linloc;   // flat address
   ULONG   entries;
} INFO_PKT;

typedef struct {
   ULONG      sign;
   HFILE       drv;
   int        excl;
} OPENINFO;

#define HANDLE_CHECK(err)                 \
   OPENINFO* poi = (OPENINFO*)handle;     \
   if (!poi) return err;                  \
   if (poi->sign!=CHECK_SIGN) return err;

/** Opens high memory.
    @param        exclusive   Lock driver until own exit to be exclusive user.
    @param  [out] handle      Handle.
    @return 0 or OS/2 error code. ERROR_WRITE_PROTECT returned if memory
            already locked to exclusive mode by someone. */
APIRET STDCALL hm_open(int exclusive, PULONG handle) {
   if (!handle) return ERROR_INVALID_PARAMETER; else {
      HFILE    drvh = 0;
      ULONG  action = 0,
            version = exclusive ? FFFF : 0, 
              vsize = 4;
      APIRET     rc = DosOpen("\\DEV\\HD4DISK$", &drvh, &action, 0, FILE_NORMAL,
         OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE|OPEN_ACCESS_READONLY, NULL);
      OPENINFO *poi;

      *handle = 0;
      if (rc) return rc;

      /* check driver for IOCTL support and inform about exclusive mode for
         this sfn (send version=-1). If driver will return the same -1 - exclusive
         mode already used by someone */
      rc = DosDevIOCtl(drvh, IOCTL_CAT, IOCTL_F_CHECK, 0, 0, 0, &version, 4, &vsize);
      if (!rc)
         if (version!=CHECK_VERSION) rc = FFFF;
      if (rc) {
         DosClose(drvh);
         return version==FFFF && rc==FFFF ? ERROR_WRITE_PROTECT : ERROR_BAD_COMMAND;
      }
      // return handle
      poi = malloc(sizeof(OPENINFO));
      poi->sign = CHECK_SIGN;
      poi->drv  = drvh;
      poi->excl = exclusive;

      *handle = (ULONG)poi;
      return 0;
   }
}


/** Query memory size.
    @param        handle      Handle.
    @return available memory size in sectors (512 bytes) or 0 on error */
ULONG STDCALL hm_getsize(ULONG handle) {
   ULONG rc, size = 0, vsize = 4;
   HANDLE_CHECK(0)

   rc = DosDevIOCtl(poi->drv, IOCTL_CAT, IOCTL_F_SIZE, 0, 0, 0, &size, 4, &vsize);
   if (rc) return 0;

   return size;
}


static APIRET iocommon(OPENINFO* poi, ULONG sector, ULONG count, PVOID data,
                       PULONG actual, int write)
{
   RW_PKT     cmd;
   ULONG   cmdlen = sizeof(RW_PKT);
   APIRET      rc;

   cmd.sector  = sector;
   cmd.count   = count;
   cmd.lindata = (ULONG)data;
   cmd.write   = write;

   rc = DosDevIOCtl(poi->drv, IOCTL_CAT, IOCTL_F_READ + write, 0, 0, 0, &cmd,
      cmdlen, &cmdlen);

   if (actual) *actual = cmd.count;

   return rc;
}

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
APIRET STDCALL hm_read (ULONG handle, ULONG sector, ULONG count, PVOID data, PULONG actual) {
   HANDLE_CHECK(ERROR_INVALID_PARAMETER)
   return iocommon(poi, sector, count, data, actual, 0);
}


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
APIRET STDCALL hm_write(ULONG handle, ULONG sector, ULONG count, PVOID data, PULONG actual) {
   HANDLE_CHECK(ERROR_INVALID_PARAMETER)
   return iocommon(poi, sector, count, data, actual, 1);
}


/** Query emulated HDD geometry.
    Any of out parameters can be 0.

    @param        handle   Handle.
    @param  [out] active   Disk is active (if active==0 - "HD4DISK.ADD /i" used,
                           i.e. disk memory available only to this API
    @param  [out] cyls     Number of cylinders
    @param  [out] heads    Number of heads
    @param  [out] spt      Number of sectors per track
    @return 0 or OS/2 error code */
APIRET STDCALL hm_chsgeo(ULONG handle, PULONG active, PULONG cyls, PULONG heads, 
   PULONG spt)
{
   GEO_PKT    cmd;
   ULONG   cmdlen = sizeof(GEO_PKT);
   APIRET      rc;

   HANDLE_CHECK(ERROR_INVALID_PARAMETER)

   rc = DosDevIOCtl(poi->drv, IOCTL_CAT, IOCTL_F_CHS, 0, 0, 0, &cmd, cmdlen, &cmdlen);

   if (cyls)   *cyls   = rc ? 0 : cmd.cyls;
   if (heads)  *heads  = rc ? 0 : cmd.heads;
   if (spt)    *spt    = rc ? 0 : cmd.spt;
   if (active) *active = rc ? 1 : cmd.active;

   return rc;
}


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
ULONG STDCALL hm_getinfo(ULONG handle, HM_LOCATION_INFO *memloc, ULONG entries) {
   INFO_PKT   cmd;
   ULONG   cmdlen = sizeof(INFO_PKT);
   APIRET      rc;
   HANDLE_CHECK(0)
   // check it here (but driver deny request too)
   if (!poi->excl) return 0;

   cmd.entries = entries;
   cmd.linloc  = (ULONG)memloc;

   rc = DosDevIOCtl(poi->drv, IOCTL_CAT, IOCTL_F_INFO, 0, 0, 0, &cmd, cmdlen, &cmdlen);

   return rc ? 0 : cmd.entries;
}


/** Close high memory.
    @param  handle         Handle.
    @return 0 or OS/2 error code */
APIRET STDCALL hm_close(ULONG handle) {
   APIRET  rc;
   HANDLE_CHECK(ERROR_INVALID_PARAMETER)

   rc = DosClose(poi->drv);
   poi->sign = 0;
   free(poi);

   return rc;
}
