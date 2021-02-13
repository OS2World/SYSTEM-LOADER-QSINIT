//
// QSINIT
// sequential zip (stream) read.
// --------------------------------------------------------
// uses no memory in memory mode & 64k for cache + buffer for compressed file
// data in file mode.
//
#include "clib.h"
#include "qsutil.h"
#include "unzip.h"
#include "puff.h"
#include "qslocal.h"
#include "qsio.h"

//#define ZIP_DEBUG
#ifndef ZIP_DEBUG
  #define Message(x)
#else
  #define Message log_printf
#endif

#define ZIP_SIGN    0x5150495A    ///< ZIPQ

#define ST_INITNEXT        0x1    ///< initial zip_nextfile() done, to check is it a ZIP?
#define ST_NEXTREADY       0x2
#define ST_DATAREADY       0x3    ///< file data was recieved by zip_readfile() call
#define ST_ERROR           0x4

#define CEN_SIG     0x02014B50    ///< central directory
#define LOC_SIG     0x04034B50    ///< local file signature
#define ARC_SIG     0x08064B50    ///< archive extra data record
#define DAT_SIG     0x08074B50    ///< data descripror signature

#define CACHE_SIZE       _64KB

typedef struct {
   u16t        ver;
   u16t      flags;
   u16t     method;
   u32t    dostime;
   u32t        crc;
   u32t      csize;     ///< compressed size
   u32t       size;     ///< uncompressed size
   u16t    namelen;     ///< filename length
   u16t      extra;     ///< extra data length
} zip_filerec;

typedef struct {
   u32t       sign;
   u32t        crc;
   u32t      csize;     ///< compressed size
   u32t       size;     ///< uncompressed size
} zip_datarec;

qserr _std zip_open(ZIP *zip, void *in_data, u32t in_size) {
   if (!zip || !in_data || !in_size) return E_SYS_INVPARM;
   memset(zip, 0, sizeof(ZIP));
   zip->memdata = (char*)in_data;
   zip->srcsize = in_size;
   zip->sign    = ZIP_SIGN;
   zip->state   = ST_DATAREADY;

   if (zip_nextfile(zip,0,0)==0) {
      zip->state = ST_INITNEXT;
      return 0;
   } else {
      zip_close(zip);
      return E_SYS_BADFMT;
   }
}

qserr _std zip_openfile(ZIP *zip, const char *path, int bootvol) {
   qserr  res = 0;
   if (!zip || !path) return E_SYS_INVPARM;
   memset(zip, 0, sizeof(ZIP));

   if (bootvol) {
      zip->srcsize = hlp_fopen2(path,1);
      if (zip->srcsize==FFFF) return E_SYS_NOFILE;
      if (!pxemicro && !zip->srcsize) res = E_SYS_BADFMT;

      zip->isboot  = 1;
   } else {
      u32t  action;
      u64t    sz64;
      if (!mod_secondary) return E_SYS_UNSUPPORTED;

      res = mod_secondary->io_open(path, IOFM_OPEN_EXISTING|IOFM_READ|
         IOFM_SHARE_READ|IOFM_SHARE_DEL|IOFM_SHARE_REN, &zip->fh, &action);
      if (res) return res;

      res = mod_secondary->io_size(zip->fh, &sz64);
      if (!res) zip->srcsize = sz64;
   }
   zip->sign = ZIP_SIGN;
   // cache buffer
   zip->buf  = (char*)hlp_memalloc(CACHE_SIZE, QSMA_RETERR|QSMA_LOCAL|QSMA_NOCLEAR);
   zip->bpos = CACHE_SIZE;
   if (!zip->buf) res = E_SYS_NOMEM;

   if (!res) {
      zip->state = ST_DATAREADY;
      if (zip_nextfile(zip,0,0)==0) zip->state = ST_INITNEXT; else
         res = E_SYS_BADFMT;
   }
#ifdef ZIP_DEBUG
   Message("zip_openfile \"%s\" (%i) = %X\n", path, bootvol, res);
#endif     
   if (res) zip_close(zip);
   return res;
}

/** read/skip zip stream data.
    Note, that for PXE source file size is UNKNOWN.
    Smart code, which uses memory, file, micro-FSD file & moveton`s unhandy
    streams as a source.

    @param [in]     zip      instance data
    @param [in]     buffer   buffer for data or 0 to skip it
    @param [in,out] bytes    number of bytes to read, real value on exit
    @return 0 on success or error code. Function returns E_SYS_EOF when
            read after the end of file. Function returns NO error when reaches
            the end of file on PXE boot. In this case out bytes value will be
            smaller, than in! */
static qserr zip_getdata(ZIP *zip, void *buffer, u32t *bytes) {
   u64t      rest = 0;
   u32t   readlen = *bytes;
   qserr     errv = 0;
   // we should have size even for PXE, after we reach the end of file
   if (zip->srcsize && zip->srcpos>=zip->srcsize) return E_SYS_EOF;
   if (zip->srcsize) {
      rest = zip->srcsize - zip->srcpos;
      if (readlen>rest) /*readlen = rest;*/ return E_SYS_EOF;
   }

   if (zip->memdata) {
      if (buffer) memcpy(buffer, zip->memdata+zip->srcpos, readlen);
      zip->srcpos += readlen;
      readlen = 0;
   } else 
   while (readlen) {
      // get/update cache position until cache is empty
      if (zip->bpos<CACHE_SIZE) {
         u32t clen = CACHE_SIZE - zip->bpos;
         if (clen>readlen) clen = readlen;
         if (buffer) {
            memcpy(buffer, zip->buf+zip->bpos, clen);
            buffer = (u8t*)buffer + clen;
         }
         zip->bpos+= clen;
         readlen  -= clen;
         zip->srcpos += clen;
         if (rest) rest = zip->srcsize - zip->srcpos;
      }
      if (!readlen) break;
      // still have something to read/skip?
      if (zip->isboot) {
         // moveton`s stream read has no offset at all, i.e. we MUST read it all
         if (pxemicro || buffer) {
            int tocache = !buffer || readlen<CACHE_SIZE;
            u32t  rsize = tocache ? (rest && rest<CACHE_SIZE?rest:CACHE_SIZE) : readlen,
                  asize = hlp_fread(zip->srcpos, tocache?zip->buf:buffer, rsize);
            // in PXE mode we just discover EOF by uncomplete read
            if (asize<rsize)
               if (pxemicro) zip->srcsize = zip->srcpos + asize;
                  else { errv = E_DSK_ERRREAD; break; }
            // loop it to get data from cache
            if (tocache) { zip->bpos = 0; continue; }
         }
      } else {
         if (!buffer) {
            // cache is empty, i.e. file position should be in the right place
            if (mod_secondary->io_seek(zip->fh, readlen, IO_SEEK_CUR)==FFFF64) {
               errv = mod_secondary->io_lasterror(zip->fh);
               break;
            }
         } else {
            int tocache = readlen<CACHE_SIZE;
            u32t  rsize = tocache ? (rest<CACHE_SIZE?rest:CACHE_SIZE) : readlen;
#if 0
            log_printf("zip->srcsize %Lu zip->srcpos %Lu(%LX) rsize %u readlen %u\n",
               zip->srcsize, zip->srcpos, zip->srcpos, rsize, readlen);
#endif
            if (mod_secondary->io_read(zip->fh, tocache?zip->buf:buffer, rsize)!=rsize) {
               errv = mod_secondary->io_lasterror(zip->fh);
               if (!errv) errv = E_DSK_ERRREAD;
               break;
            }
            // loop it to get data from cache
            if (tocache) { zip->bpos = 0; continue; }
         }
      }
      zip->srcpos += readlen;
      readlen = 0;
   }
   *bytes -= readlen;
   return errv;
}

void _std zip_close(ZIP *zip) {
   // make it thread safe ;)
   if (mt_cmpxchgd(&zip->sign,0,ZIP_SIGN) != ZIP_SIGN) return;

   if (zip->isboot) hlp_fclose(); else
      if (zip->fh) mod_secondary->io_close(zip->fh);

   if (zip->buf) hlp_memfree(zip->buf);
   if (zip->cdata) hlp_memfree(zip->cdata);
}

static qserr _zip_nextdata(ZIP *zip, void *buffer, u32t toread
#ifdef ZIP_DEBUG
                           , const char *message
#endif
) {
   u32t  rds = toread;
   qserr res = zip_getdata(zip, buffer, &rds);
   if (res || rds<toread) {
      Message(message);
      return res?res:E_SYS_EOF;
   }
   return 0;
}

#ifdef ZIP_DEBUG
#define zip_next(z,buf,rds,msg) _zip_nextdata(z,buf,rds,msg)
#else
#define zip_next(z,buf,rds,msg) _zip_nextdata(z,buf,rds)
#endif

qserr _std zip_nextfile(ZIP *zip, char** filename, u32t *filesize) {
   qserr        res = 0;
   u32t   sign, rds;
   if (zip->sign!=ZIP_SIGN) return E_SYS_INVOBJECT;
   // we have a file from zip_open, just return it here
   if (zip->state==ST_INITNEXT) {
      zip->state = zip->cdata?ST_DATAREADY:ST_NEXTREADY;
      if (filename) *filename = zip->name;
      if (filesize) *filesize = zip->uncompressed;
      return 0;
   }
   /* it should be the sequence of ST_NEXTREADY || ST_DATAREADY, any other
      value is error */
   if (zip->state==ST_NEXTREADY) {
      res = zip_next(zip, 0, zip->compressed, "ZIP error: skip file\n");
      if (res) return res;
      zip->state = ST_DATAREADY;
   }
   if (zip->state!=ST_DATAREADY) return E_SYS_BADFMT;
   // error until the success in header parse
   zip->state = ST_ERROR;
   // release possible lost cdata
   if (zip->cdata) { hlp_memfree(zip->cdata); zip->cdata = 0; }

   while (!res) {
      res = zip_next(zip, &sign, 4, "ZIP error: no signature\n");
      if (res) return res;

      switch (sign) {
         case CEN_SIG:
            Message("Central directory\n");
            return E_SYS_EOF;
         case LOC_SIG: {
            zip_filerec hdr;
            Message("Local file signature\n");

            res = zip_next(zip, &hdr, sizeof(hdr), "ZIP error: broken LOC\n");
            if (res) return res;
#ifdef ZIP_DEBUG
            Message("flags         %04X\n", hdr.flags);
            Message("method        %u\n"  , hdr.method);
            Message("crc           %08X\n", hdr.crc);
            Message("compressed    %u\n"  , hdr.csize);
            Message("uncompressed  %u\n"  , hdr.size);
            Message("name length   %u\n"  , hdr.namelen);
            Message("extra         %u\n"  , hdr.extra);
#endif
            if (hdr.namelen>ZIP_NAMELEN-1) {
               Message("ZIP error: file name too long\n");
               return E_SYS_BADFMT;
            }
            res = zip_next(zip, &zip->name, hdr.namelen, "ZIP error: broken file name\n");
            if (res) return res;

            if (hdr.extra) {
               u16t elen = hdr.extra;
               while (elen>4) {
                  u32t   hdrid, hdrlen;
                  res = zip_next(zip, &hdrid, 4, "ZIP error: extra\n");
                  if (res) return res; else elen-=4;

                  hdrlen = hdrid>>16;
                  hdrid &= 0xFFFF;
                  if (elen<hdrlen) {
                     Message("ZIP error: missing extra data\n");
                     return E_SYS_BADFMT;
                  } else
                     elen -= hdrlen;
                  //log_printf("%u bytes ext id %u\n", hdrlen, hdrid);

                  // ZIP64 extra (I saw this header for a file of 2.2Gb size)
                  if (hdrid==1) {
                     u64t tmp;
                     if (hdr.size==FFFF && hdrlen>=8) {
                        res = zip_next(zip, &tmp, 8, "ZIP error: extra usize\n");
                        if (res) return res;
                        hdrlen-=8;
                        if (tmp>=_4GBLL) return E_SYS_TOOLARGE;
                        hdr.size = (u32t)tmp;
                     }
                     if (hdr.csize==FFFF && hdrlen>=8) {
                        res = zip_next(zip, &tmp, 8, "ZIP error: extra csize\n");
                        if (res) return res;
                        hdrlen-=8;
                        if (tmp>=_4GBLL) return E_SYS_TOOLARGE;
                        hdr.csize = (u32t)tmp;
                     }
                  }
                  if (hdrlen) {
                     res = zip_next(zip, 0, hdrlen, "ZIP error: broken extra\n");
                     if (res) return res;
                  }
               }
               if (elen) {
                  res = zip_next(zip, 0, elen, "ZIP error: unk extra?\n");
                  if (res) return res;
               }
            }
            /* DUMB option. It requires slow forward searching with unknown
               size and, at least, JAR archives use it */
            if (hdr.flags & 8) {
               /* field can be set and Info-ZIP trusts to this value, so we
                  can too */
               if (hdr.csize) {
                  zip_datarec dh;
                  zip->cdata = hlp_memallocsig(hdr.csize, "zip", QSMA_RETERR|QSMA_LOCAL|QSMA_NOCLEAR);
                  // no memory?
                  if (!zip->cdata) return E_SYS_NOMEM;
                  res = zip_next(zip, zip->cdata, hdr.csize, "ZIP error: data (pt.1)\n");
                  if (res) return res;
                  res = zip_next(zip, &dh, sizeof(dh), "ZIP error: data descriptor\n");
                  if (res) return res;
                  // check signature, at least
                  if (dh.sign!=DAT_SIG) {
                     Message("ZIP error: missing data descriptor\n");
                     return E_SYS_BADFMT;
                  }
               } else {
                  u32t asize = _64KB, aofs = 0, ddc, rp;
                  zip->cdata = hlp_memallocsig(asize, "zip", QSMA_RETERR|QSMA_LOCAL|QSMA_NOCLEAR);

                  if (!zip->cdata) return E_SYS_NOMEM;

                  ddc = 16;
                  rp  = 0;
                  while (!hdr.csize) {
                     // realloc compressed file buffer
                     if (aofs+32>=asize) {
                        zip->cdata = hlp_memrealloc(zip->cdata, asize+=_64KB);
                        if (!zip->cdata) return E_SYS_NOMEM;
                     }
                     res  = zip_next(zip, (u8t*)zip->cdata+aofs, ddc, "ZIP error: data (pt.2)\n");
                     if (res) return res;
                     aofs+= ddc;
                     // first call is 16, next is 12 to be less, than zip_datarec
                     ddc  = 12;

                     while (rp <= aofs-4)
                        if (*(u32t*)((u8t*)zip->cdata+rp)==DAT_SIG) {
                           u32t rem = 16 - (aofs-rp);
                           // remaining part of header
                           if (rem) res = zip_next(zip, (u8t*)zip->cdata+aofs, 
                              rem, "broken data (pt.3)\n");
                           if (res) return res; else {
                              zip_datarec *zd = (zip_datarec*)((u8t*)zip->cdata+rp);
                              aofs += rem;
                              // check that it is not an accident match
                              if (zd->csize==rp) {
                                 hdr.crc   = zd->crc;
                                 hdr.csize = zd->csize;
                                 hdr.size  = zd->size;
                                 break;
                              } else
                                 rp++;
                           }
                        } else
                           rp++;
                  }
               }
            }
            if (!res) {
               zip->method       = hdr.method;
               zip->compressed   = hdr.csize;
               zip->uncompressed = hdr.size;
               zip->stored_crc   = hdr.crc;
               zip->dostime      = hdr.dostime;
               zip->state        = hdr.flags&8?ST_DATAREADY:ST_NEXTREADY;
               zip->crc          = 0;
               
               zip->name[hdr.namelen] = 0;
#ifdef ZIP_DEBUG
               Message("name          \"%s\"\n", zip->name);
#endif     
               if (filename) *filename = zip->name;
               if (filesize) *filesize = zip->uncompressed;

               return 0;
            }
            break;
         }
         case ARC_SIG: {
            Message("Extra data record\n");
            res = zip_next(zip, &sign, 4, "ZIP error: no extra data info\n");
            if (res) return res;
            res = zip_next(zip, 0, sign, "ZIP error: no extra data\n");
            if (res) return res;
            break;
         }
         default:
            return E_SYS_BADFMT;
      }
   }
   Message("zip_nextfile() done\n");
   return res;
}

qserr _std zip_readfile(ZIP *zip, void **buffer) {
   void *res;
   qserr  rc = 0;
   if (buffer) *buffer = 0; else return E_SYS_ZEROPTR;
   if (zip->sign!=ZIP_SIGN) return E_SYS_INVOBJECT;
   // wrong state?
   if (!(zip->state==ST_NEXTREADY || zip->state==ST_DATAREADY && zip->cdata))
      return E_SYS_NONINITOBJ;
   // actually, after this error operation can be restarted
   res = hlp_memallocsig(zip->uncompressed, "zip", QSMA_RETERR|QSMA_NOCLEAR);
   if (!res) return E_SYS_NOMEM;
#ifdef ZIP_DEBUG
   Message("zip_readfile \"%s\" -> %X (cdata:%08X, %u->%u)\n", zip->name, *buffer,
      zip->cdata, zip->compressed, zip->uncompressed);
#endif
   zip->crc = crc32(0,0,0); 
   do {
      if (!zip->method) { // no compression
         if (zip->cdata) memcpy(res, zip->cdata, zip->compressed); else
            rc = zip_next(zip, res, zip->compressed, "ZIP error: data (pt.3)\n");
         zip->state = rc?ST_ERROR:ST_DATAREADY;
         if (rc) break;
         zip->uncompressed = zip->compressed;
      } else {
         u32t dlen = zip->uncompressed,
              slen = zip->compressed;
         int   err;
   
         if (!zip->cdata) { 
            zip->cdata = hlp_memallocsig(zip->compressed, "zip", QSMA_RETERR|QSMA_NOCLEAR);
            if (!zip->cdata) { rc = E_SYS_NOMEM; break; }
            rc = zip_next(zip, zip->cdata, zip->compressed, "ZIP error: data (pt.4)\n");
            if (rc) break;
         }
         err = puff((u8t*)res, &dlen, (u8t*)zip->cdata, &slen);
   
         if (err || dlen!=zip->uncompressed) {
#ifdef ZIP_DEBUG
            Message("zip_readfile: inflate error %i (%d!=%d)\n", err, zip->uncompressed, dlen);
#endif
            rc = E_SYS_CRC;
         }
      }
   } while (0);

   if (!rc) {
      zip->crc = crc32(zip->crc, res, zip->uncompressed);
#ifdef ZIP_DEBUG
      Message("zip_readfile: %08X, %u bytes, crc: %08X\n", res, zip->uncompressed, zip->crc);
#endif
      zip->state = ST_DATAREADY;
      *buffer = res;
      /* this may occur only for uncompressed data (for compressed puff() has
         returned error earlier) */
      if (zip->crc!=zip->stored_crc) rc = E_SYS_CRC;
   } else
   if (res) hlp_memfree(res);

   if (zip->cdata) { hlp_memfree(zip->cdata); zip->cdata = 0; }

   return rc;
}

u32t _std zip_unpacked(ZIP *zip, u32t *totalfiles) {
   u32t rcsize, fsz, files;
   if (zip->sign!=ZIP_SIGN) return 0;
   rcsize = 0;
   files  = 0;
   while (zip_nextfile(zip,0,&fsz)==0) { rcsize += fsz; files++; }

   if (totalfiles) *totalfiles = files;
   return rcsize;
}
