#ifndef UNZIP2_H
#define UNZIP2_H

#include "qstypes.h"

#define ZIP_MAXPATHLEN 256

typedef struct {
   char        *indata;
   char          *dptr;
   u32t         insize;
   char        *bufpos;
   char           name[ZIP_MAXPATHLEN];
   u32t     compressed;
   u32t   uncompressed;
   int          method;
   u32t     stored_crc;
   u32t            crc;
   u32t        dostime;
   u32t      extradata;
} ZIP;

/** open memory buffer as ZIP file.
    @param [out] zip      zip handle
    @param [in]  in_data  zip file data
    @param [in]  in_size  zip file size
    @return true if success, and setup the ZIP accordingly. */
int   _std zip_open(ZIP *zip, void *in_data, u32t in_size);

/// close ZIP file.
void  _std zip_close(ZIP *zip);

/** get next file info.
    @param [in]  zip       zip handle
    @param [out] filename  next file name to unpack
    @param [out] filesize  next file`s size
    @return true if next file exists in the ZIP */
int   _std zip_nextfile(ZIP *zip, char** filename, u32t *filesize);

/** read next file from ZIP.
    @param [in]  zip       zip handle
    @return buffer with file on 0 on error. Buffer must be freed via memFree() */
void* _std zip_readfile(ZIP *zip);

/** query last extracted result.
    @return true on CRC match */
int   _std zip_isok(ZIP *zip);

/** query total unpacked size.
    File must be re-opened after this function (it seek file from start to the end).
    @param [in]  zip         zip handle
    @param [out] totalfiles  number of files in ZIP
    @return sum of unpacked file sizes */
u32t  _std zip_unpacked(ZIP *zip, u32t *totalfiles);

#endif // UNZIP2_H
