//
// QSINIT
// lite UNZIP, ugly and slow, but small :)
//
#ifndef QSINIT_UNZIP_LITE
#define QSINIT_UNZIP_LITE

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZIP_NAMELEN 256

#pragma pack(1)
typedef struct {
   /* service data, do not touch it! */
   u32t               sign;    ///< check signature
   char           *memdata;    ///< full memory buffer (0 in file mode)
   u32t            srcsize;
   u32t             srcpos;
   char               *buf;
   u32t               bpos;
   qshandle             fh;    ///< file handle
   int              isboot;    ///< boot file i/o is used
   int               state;
   void             *cdata;    ///< compressed file data, valid when local header present

   /* user data about current file (after zip_nextfile() call) */
   char  name[ZIP_NAMELEN];    ///< file name
   u32t         compressed;    ///< compressed file size
   u32t       uncompressed;    ///< file size
   int              method;    ///< 0 = store, >0 = zip
   u32t         stored_crc;    ///< file CRC value
   u32t                crc;    ///< REAL CRC value, after zip_readfile()
   u32t            dostime;    ///< file time (in dos format)
} ZIP;
#pragma pack()

/** open memory buffer as ZIP file.
    @param [out] zip      zip handle
    @param [in]  in_data  zip file data
    @param [in]  in_size  zip file size
    @return 0 on success or error code. */
qserr _std zip_open(ZIP *zip, void *in_data, u32t in_size);

/** open file as a ZIP file.
    Note, that function locks boot i/o until zip_close() call - like
    hlp_fopen() do.

    Also, note, that all zip functions are NOT thread safe, i.e. every ZIP
    instance should be used from one thread.

    @param [out] zip      zip handle
    @param [in]  path     zip file name
    @param [in]  bootvol  boot volume i/o flag (1/0). Non-zero means
                          micro-FSD file i/o (root of the boot volume).
    @return 0 on success or error code. */
qserr _std zip_openfile(ZIP *zip, const char *path, int bootvol);

/// close ZIP file.
void  _std zip_close(ZIP *zip);

/** seek to the next file and get it information.
    @param [in]  zip       zip handle
    @param [out] filename  next file name to unpack
    @param [out] filesize  next file`s size
    @return 0 on success or error code. E_SYS_EOF retuned when end of
            file reached without error. */
qserr _std zip_nextfile(ZIP *zip, char** filename, u32t *filesize);

/** read next file from ZIP.
    Call should be done only after success zip_nextfile(), else
    this forces E_SYS_NONINITOBJ error.

    @attention returning buffer is global shared memory (i.e. it will be lost
               if user forgot to release it).

    @param [in]  zip       zip handle
    @param [out] buffer    uncompressed data, must be freed via memFree().
    @return 0 on success or error code. */
qserr _std zip_readfile(ZIP *zip, void **buffer);

/** query total unpacked size.
    File must be re-opened after this function (it seeks file from start to the end).
    Note, that for PXE zip_openfile() this also mean second download!

    @param [in]  zip         zip handle
    @param [out] totalfiles  number of files in ZIP
    @return sum of unpacked file sizes */
u32t  _std zip_unpacked(ZIP *zip, u32t *totalfiles);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_UNZIP_LITE
