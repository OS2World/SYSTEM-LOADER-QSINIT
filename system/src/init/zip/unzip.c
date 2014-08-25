//
// QSINIT
// unzip code
//
#include "clib.h"
#include "qsutil.h"
#include "unzip.h"
#include "puff.h"

//#define ZIP_DEBUG
#ifndef ZIP_DEBUG
  #define Message(x)
#else
  #define Message log_printf
#endif

#define CEN_SIG 0x02014B50 // central directory
#define LOC_SIG 0x04034B50 // local file signature
#define ARC_SIG 0x08064B50 // archive extra data record
#define DAT_SIG 0x08074B50 // data descripror signature

static inline unsigned int read16_ex(char **bufptr) {
    unsigned int  rc = *((u16t*)(*bufptr));
    *bufptr += 2;
    return rc;
}

static inline u32t read32_buf(char *buf) {
    return *((u32t*)buf);
}

static inline u32t read32_ex(char **bufptr) {
    u32t  rc = *((u32t*)(*bufptr));
    *bufptr += 4;
    return rc;
}

static inline int search_sig(unsigned int *distance, char *buffer, int bufsize, unsigned int sig)
{
    char s1 = (char)((sig)       & 0xff);
    char s2 = (char)((sig >> 8)  & 0xff);
    char s3 = (char)((sig >> 16) & 0xff);
    char s4 = (char)((sig >> 24) & 0xff);

    char *bufptr = buffer;

    while (bufptr - buffer + 4 <= bufsize) {
        if(s1 == bufptr[0] && s2 == bufptr[1] && s3 == bufptr[2] && s4 == bufptr[3]) {
            *distance = bufptr - buffer;
            return true;
        }
        ++bufptr;
    }
    return false;
}

static inline u32t zip_read32(ZIP *zip) {
    return read32_ex(&zip->dptr);
}


static inline unsigned int zip_read16(ZIP *zip) {
    return read16_ex(&zip->dptr);
}


static inline unsigned int buf_rest(ZIP *zip) {
    return zip->insize - (zip->dptr - zip->indata);
}

static int read_desc(ZIP *zip, u32t *c_size, u32t *u_size, u32t *stored_crc) {
   unsigned int rest, distance = 0;

   Message("read_desc() entry\n");
   rest = buf_rest(zip);

#ifdef ZIP_DEBUG
   Message("indata = 0x%08X, dptr = 0x%08X, rest = %d\n",
      zip->indata, zip->dptr, rest);
#endif
   for (;;) {
      unsigned int sub_distance = 0;

      if (search_sig(&sub_distance, zip->dptr + distance,
         rest - distance, DAT_SIG) && distance + sub_distance + 16 < rest)
      {
         u32t real_crc, real_compressed, real_uncompressed;
         distance += sub_distance;
#ifdef ZIP_DEBUG
         Message("distance = %u\n", distance);
#endif
         real_crc = read32_buf(zip->dptr + distance + 4);
         real_compressed   = read32_buf(zip->dptr + distance + 8);
         real_uncompressed = read32_buf(zip->dptr + distance + 12);

#ifdef ZIP_DEBUG
         Message("crc 0x%08x\n", real_crc);
         Message("compressed   %d\n", real_compressed);
         Message("uncompressed %d\n", real_uncompressed);
#endif
         // check that it is not an accident match
         if (distance == real_compressed) {
            *c_size = real_compressed;
            *u_size = real_uncompressed;
            *stored_crc = real_crc;

            Message("read_desc() done\n");

            return true;
         }
         distance += 4;
      } else {
          // the rare case when no data descripror signature is provided
          // is not supported
          Message("read_desc() failed\n");
          return false;
      }
   }
}


int zip_open(ZIP *zip, void *in_data, u32t in_size) {
   if (!zip||!in_data||!in_size) return false;
   memset(zip, 0, sizeof(ZIP));
   zip->indata  = (char*)in_data;
   zip->dptr    = (char*)in_data;
   zip->insize  = in_size;
   return true;
}

void zip_close(ZIP *zip) {
}

int zip_nextfile(ZIP *zip, char** filename, u32t *filesize) {
   int finish = false;
   Message("zip_nextfile() entry\n");

   while (!finish && buf_rest(zip) >= 4) {
      switch (zip_read32(zip)) {
         case CEN_SIG:
            Message("Central directory\n");
            return false;
         case LOC_SIG: {
            u32t  bitflag, method;
            u32t  stored_crc, dostime;
            u32t  compressed, uncompressed;
            u32t  name_size, extra_size;
            char *name;
         
            Message("Local file signature\n");
         
            if (buf_rest(zip) < 28) {
               Message("ZIP corrupted: broken LOC\n");
               return false;
            }
            // version needed to extract
            zip->dptr += 2;
            // general purpose bit flag
            bitflag      = zip_read16(zip);
            // compression method
            method       = zip_read16(zip);
            // last modified file date in DOS format
            dostime      = zip_read32(zip);
            // crc32
            stored_crc   = zip_read32(zip);
            // compressed size
            compressed   = zip_read32(zip);
            // uncompressed size
            uncompressed = zip_read32(zip);
            // filename length
            name_size    = zip_read16(zip);
            // extra field length
            extra_size   = zip_read16(zip);
            // file name
            name = zip->dptr;
#ifdef ZIP_DEBUG
            Message("bitflag       0x%08x\n", bitflag);
            Message("method        %d\n", method);
            Message("stored_crc    0x%08x\n", stored_crc);
            Message("compressed    %d\n", compressed);
            Message("uncompressed  %d\n", uncompressed);
            Message("name_size     %d\n", name_size);
            Message("extra_size    %d\n", extra_size);
#endif    
            if (buf_rest(zip) < name_size + extra_size) {
               Message("ZIP corrupted: no file name\n");
               return false;
            }
            // skip file name & skip extra data
            zip->dptr += name_size + extra_size;
          
            // check whether compressed/uncompressed size fields are set
            // if not, search for the end of the current file data manually
            if (bitflag & 8) {
               if (!read_desc(zip,&compressed,&uncompressed,&stored_crc)) {
                  Message("ZIP corrupted: no descriptor\n");
                  return false;
               }
            }
          
            zip->bufpos       = zip->dptr;
            zip->method       = method;
            zip->compressed   = compressed;
            zip->uncompressed = uncompressed;
            zip->stored_crc   = stored_crc;
            zip->dostime      = dostime;
          
            strncpy(zip->name, name, name_size);
            zip->name[name_size] = 0;
#ifdef ZIP_DEBUG
            Message("name '%s'\n", zip->name);
#endif     
            *filename = zip->name;
            *filesize = zip->uncompressed;
            // a normal file found
            finish = true;
            
            if (buf_rest(zip) < zip->compressed) {
                Message("ZIP corrupted: no file data\n");
                return false;
            }
            // skip compressed data
            zip->dptr += zip->compressed;
           
            // skip data descriptor
            if (bitflag & 8) {
               if (buf_rest(zip)<16) {
                  Message("ZIP corrupted: no data descriptor\n");
                  return false;
               }
               zip->dptr += 16;
            }
            Message("skipped to the next file/eof\n");
            break;
         }
         case ARC_SIG: {
            u32t extra;
            Message("Extra data record\n");
         
            if (buf_rest(zip) < 4) {
               Message("ZIP corrupted: no extra data info\n");
               return false;
            }
            extra = zip_read32(zip);
         
            if (buf_rest(zip) < extra) {
               Message("ZIP corrupted: no extra data\n");
               return false;
            }
            zip->dptr += extra;
            break;
         }
         default:
            return false;
      }
   }
   Message("zip_nextfile() done\n");
   return true;
}

void *zip_readfile(ZIP *zip) {
   void *rc = hlp_memalloc(zip->uncompressed,0);
   zip->crc = crc32(0,0,0); 

   if (!zip->method) { // no compression
      Message("zip_readfile: no compression\n");

      memcpy(rc, zip->bufpos, zip->compressed);
      zip->crc = crc32(zip->crc, rc, zip->compressed);
#ifdef ZIP_DEBUG
      Message("zip_readfile: %d bytes, crc:0x%08X\n", zip->compressed, zip->crc);
#endif
   } else {
      u32t dlen = zip->uncompressed,
           slen = zip->compressed;
      int result;

      Message("zip_readfile: opening compressed ZIP file\n");

      result = puff((u8t*)rc, &dlen, (u8t*)zip->bufpos, &slen);

      if (result || dlen!=zip->uncompressed) {
#ifdef ZIP_DEBUG
         Message("zip_readfile: inflate error %d (%d!=%d)\n",result, zip->uncompressed, dlen);
#endif
         hlp_memfree(rc);
         return 0;
      }
      zip->crc = crc32(zip->crc, rc, zip->uncompressed);
#ifdef ZIP_DEBUG
      Message("zip_readfile: %d bytes, crc:0x%08X\n", zip->uncompressed, zip->crc);
#endif
   }
   return rc;
}


int zip_isok(ZIP *zip) {
    int rc = zip->crc == zip->stored_crc;
#ifdef ZIP_DEBUG
    Message("zip_isok (%s, crc 0x%08X, stored crc 0x%08X)\n", 
       zip->name, zip->crc, zip->stored_crc);
#endif
    return rc;
}

u32t zip_unpacked(ZIP *zip, u32t *totalfiles) {
   unsigned long rc = 0;
   char *filename;
   u32t  filesize, files = 0;

   if (!zip) return 0;

   while (zip_nextfile(zip,&filename,&filesize)) { rc+=filesize; files++; }

   if (totalfiles) *totalfiles=files;
   return rc;
}
