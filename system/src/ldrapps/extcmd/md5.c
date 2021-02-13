#include "stdlib.h"
#include "qsshell.h"
#include "qsio.h"
#include "vioext.h"
#include "direct.h"
#include "errno.h"

#define T_MASK               (FFFF)
#define T1  /* 0xD76AA478 */ (T_MASK^0x28955B87)
#define T2  /* 0xE8C7B756 */ (T_MASK^0x173848A9)
#define T3     0x242070DB
#define T4  /* 0xC1BDCEEE */ (T_MASK^0x3E423111)
#define T5  /* 0xF57C0FAF */ (T_MASK^0x0A83F050)
#define T6     0x4787C62A
#define T7  /* 0xA8304613 */ (T_MASK^0x57CFB9EC)
#define T8  /* 0xFD469501 */ (T_MASK^0x02B96AFE)
#define T9     0x698098D8
#define T10 /* 0x8B44F7AF */ (T_MASK^0x74BB0850)
#define T11 /* 0xFFFF5BB1 */ (T_MASK^0x0000A44E)
#define T12 /* 0x895CD7BE */ (T_MASK^0x76A32841)
#define T13    0x6B901122
#define T14 /* 0xFD987193 */ (T_MASK^0x02678E6C)
#define T15 /* 0xA679438E */ (T_MASK^0x5986BC71)
#define T16    0x49B40821
#define T17 /* 0xF61E2562 */ (T_MASK^0x09E1DA9D)
#define T18 /* 0xC040B340 */ (T_MASK^0x3FBF4CBF)
#define T19    0x265E5A51
#define T20 /* 0xE9B6C7AA */ (T_MASK^0x16493855)
#define T21 /* 0xD62F105D */ (T_MASK^0x29D0EFA2)
#define T22    0x02441453
#define T23 /* 0xD8A1E681 */ (T_MASK^0x275E197E)
#define T24 /* 0xE7D3FBC8 */ (T_MASK^0x182C0437)
#define T25    0x21E1CDE6
#define T26 /* 0xC33707D6 */ (T_MASK^0x3CC8F829)
#define T27 /* 0xF4D50D87 */ (T_MASK^0x0B2AF278)
#define T28    0x455A14ED
#define T29 /* 0xA9E3E905 */ (T_MASK^0x561C16FA)
#define T30 /* 0xFCEFA3F8 */ (T_MASK^0x03105C07)
#define T31    0x676F02D9
#define T32 /* 0x8D2A4C8A */ (T_MASK^0x72D5B375)
#define T33 /* 0xFFFA3942 */ (T_MASK^0x0005C6BD)
#define T34 /* 0x8771F681 */ (T_MASK^0x788E097E)
#define T35    0x6D9D6122
#define T36 /* 0xFDE5380C */ (T_MASK^0x021AC7F3)
#define T37 /* 0xA4BEEA44 */ (T_MASK^0x5B4115BB)
#define T38    0x4BDECFA9
#define T39 /* 0xF6BB4B60 */ (T_MASK^0x0944B49F)
#define T40 /* 0xBEBFBC70 */ (T_MASK^0x4140438F)
#define T41    0x289B7EC6
#define T42 /* 0xEAA127FA */ (T_MASK^0x155ED805)
#define T43 /* 0xD4EF3085 */ (T_MASK^0x2B10CF7A)
#define T44    0x04881D05
#define T45 /* 0xD9D4D039 */ (T_MASK^0x262B2FC6)
#define T46 /* 0xE6DB99E5 */ (T_MASK^0x1924661A)
#define T47    0x1FA27CF8
#define T48 /* 0xC4AC5665 */ (T_MASK^0x3B53A99A)
#define T49 /* 0xF4292244 */ (T_MASK^0x0BD6DDBB)
#define T50    0x432AFF97
#define T51 /* 0xAB9423A7 */ (T_MASK^0x546BDC58)
#define T52 /* 0xFC93A039 */ (T_MASK^0x036C5FC6)
#define T53    0x655B59C3
#define T54 /* 0x8F0CCC92 */ (T_MASK^0x70F3336D)
#define T55 /* 0xFFEFF47D */ (T_MASK^0x00100B82)
#define T56 /* 0x85845DD1 */ (T_MASK^0x7A7BA22E)
#define T57    0x6FA87E4F
#define T58 /* 0xFE2CE6E0 */ (T_MASK^0x01D3191F)
#define T59 /* 0xA3014314 */ (T_MASK^0x5CFEBCEB)
#define T60    0x4E0811A1
#define T61 /* 0xF7537E82 */ (T_MASK^0x08AC817D)
#define T62 /* 0xBD3AF235 */ (T_MASK^0x42C50DCA)
#define T63    0x2AD7D2BB
#define T64 /* 0xEB86D391 */ (T_MASK^0x14792C6E)

static void md5_process(md5_state *pms, const u8t *data /*[64]*/) {
    u32t a = pms->abcd[0], b = pms->abcd[1],
         c = pms->abcd[2], d = pms->abcd[3];
    u32t t;
    u32t xbuf[16];
    const u32t *X;
    // data is properly aligned?
    if (((ptrdiff_t)data & 3)==0) X = (const u32t *)data; else {
       memcpy(xbuf, data, 64);
       X = xbuf;
    }
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

     /* Round 2. */
     /* Let [abcd k s i] denote the operation
          a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

     /* Round 3. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

     /* Round 4. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void __stdcall md5_init(md5_state *pms) {
   pms->count[0] = 0;
   pms->count[1] = 0;
   pms->abcd[0]  = 0x67452301;
   pms->abcd[1]  = T_MASK^0x10325476; // 0xEFCDAB89
   pms->abcd[2]  = T_MASK^0x67452301; // 0x98BADCFE
   pms->abcd[3]  = 0x10325476;
}

void __stdcall md5_next(md5_state *pms, const void *data, u32t len) {
   const u8t *src = (const u8t *)data;
   u32t      left = len,
           offset = pms->count[0]>>3 & 63,
            nbits = len<<3;
   if (!len) return;
   // update message length
   pms->count[1] += len >> 29;
   pms->count[0] += nbits;
   if (pms->count[0]<nbits) pms->count[1]++;
   // process leading partial block
   if (offset) {
      u32t copy = offset+len>64 ? 64-offset : len;
      memcpy(pms->buf + offset, src, copy);
      if (offset+copy<64) return;
      src += copy;
      left-= copy;
      md5_process(pms, pms->buf);
   }
   // process full blocks
   for (; left>=64; src+=64, left-=64) md5_process(pms, src);
   // process final partial block
   if (left) memcpy(pms->buf, src, left);
}

void __stdcall md5_done(md5_state *pms, u8t *result) {
   static const u32t pad[16] = {0x80,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
   u8t              data[8];
   int                ii;
   // save length before padding
   for (ii=0; ii<8; ii++)
      data[ii] = (u8t)(pms->count[ii>>2] >> ((ii&3) << 3));
   // pad to 56 bytes mod 64
   md5_next(pms, pad, (55 - (pms->count[0]>>3) & 63) + 1);
   // add length
   md5_next(pms, data, 8);
   for (ii=0; ii<16; ii++)
      result[ii] = (u8t)(pms->abcd[ii>>2] >> ((ii&3) << 3));
}

static int file_print(const char *name, qserr err, const char *msg) {
   char *emsg = err ? cmd_shellerrmsg(EMSG_QS, err) : 0;
   int    esc = cmd_printseq("%s : %s", 0, 0, name, emsg?emsg:msg);
   if (emsg) free(emsg);
   return esc;
}

/** calc file md5/crc32.
   @param       path        file path, size can be >4Gb
   @param [out] sumstr      result string
   @param       crc         type (crc/md5)
   @param       sumsfn      sum file SFN to compare and skip
   @return error code, 0 on success or FFFF if file skipped (sum file itself) */
static qserr file_md5(const char *path, char *sumstr, int crc, u32t sumsfn) {
   u8t        sum[16];
   io_handle  ifh;
   u32t      crcv;
   qserr      err = io_open(path, IOFM_OPEN_EXISTING|IOFM_READ|IOFM_SHARE, &ifh, 0);

   if (!err) {
      io_direntry_info  fi;
      md5_state      state;
      u64t            rpos = 0, size;
      void            *buf = 0;
      // get file information (size & SFN)
      err = io_info(ifh, &fi);
      // skip sum file
      if (!err && sumsfn && fi.fileno==sumsfn) err=FFFF;

      if (!err) {
         if (crc) crcv = crc32(0,0,0); else md5_init(&state);
         buf  = malloc(_32KB);
         size = fi.size;
      }
      // read loop
      while (!err && rpos<size) {
         u32t rds = size-rpos>_32KB ? _32KB : size-rpos;

         if (io_read(ifh,buf,rds)!=rds) err = io_lasterror(ifh);
         if (rds) {
            if (crc) crcv = crc32(crcv, buf, rds); else
               md5_next(&state, buf, rds);
            rpos+=rds;
         }
      }
      if (!err) md5_done(&state, sum);
      if (buf) free(buf);
      io_close(ifh);
   }
   if (!err)
      if (crc) sprintf(sumstr, "%08X", crcv); else {
         int  ii;
         for (ii=0; ii<16; ii++) sprintf(sumstr+ii*2, "%02X", sum[ii]);
      }
   return err;
}

/// get unique system file number
u32t getsfn(FILE *ff) {
   io_direntry_info  fi;
   qshandle         ioh = _os_handle(fileno(ff));
   if (!ioh) return 0;
   if (io_info(ioh, &fi)) return 0;
   return fi.fileno;
}

u32t _std shl_md5(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, check=0, crc=0;
   static char *argstr   = "np|check|c|crc|crc32";
   static short argval[] = { 1,  1,  1, 1,     1};
   char            *arg0 = 0;
   // help overrides any other keys
   if (str_findkey(args, "/?", 0)) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   args = str_parseargs(args, 0, SPA_RETLIST|SPA_NOSLASH, argstr, argval,
                        &nopause, &check,
      &check, &crc, &crc);
   // process command
   if (args->count>0) {
      cmd_printseq(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT, 0);

      if (check) {
         int   quiet = 0;
         if (stricmp(args->item[quiet],"/q")==0) quiet++;

         if (quiet+1==args->count) {
            u32t   cflen = 0;
            void *cfdata = freadfull(args->item[quiet], &cflen);
            // should be al least ENOENT & ENOMEM
            if (!cfdata) rc = errno; else {
               str_list *cf = str_settext((char*)cfdata, cflen);
               hlp_memfree(cfdata);

               if (cf) {
                  u32t ii;
                  for (ii=0; ii<cf->count; ii++) {
                     char *ln = cf->item[ii],
                          sum[36], sum2[36];
                     u32t  jj = 0,
                          len = strlen(ln);
                     // allow comment in MD5 file too
                     if (*ln==';') continue;
                     if (crc) {
                        // get SFV string: name with spaces  01234567\n
                        if (len<10) rc = EINVAL; else {
                           char *sp = strrchr(ln, ' ');
                           if (!sp) rc = EINVAL; else {
                              *sp++ = 0;
                              trimleft(sp, " \t");
                              trimright(sp, " \t");
                              if (strlen(sp)!=8) rc = EINVAL;
                                 else strcpy(sum, sp);
                           }
                        }
                     } else
                     if (len<34) rc = EINVAL; else {
                        // check for xxxx-xxxx-xxxx-xxxx ... 
                        if (len>=42)
                           for (jj=0; jj<7; jj++)
                              if (ln[4+jj*5]!='-') break;
                        if (jj!=7) {
                           memcpy(&sum, ln, 32); 
                           ln += 32;
                        } else {
                           for (jj=0; jj<8; jj++)
                              memcpy(sum+jj*4, ln+jj*5, 4);
                           ln += 39;
                        }
                        sum[32] = 0;
                        if (memrchr(sum,' ',32)) rc = EINVAL; else
                           if (*ln++!=' ') rc = EINVAL; else
                              if (*ln==' ' || *ln=='*') ln++;
                     }
                     if (rc<=0) {
                        qserr ioerr;
                        trimright(ln, " \t");
                        trimleft(ln, " \t");

                        ioerr = file_md5(ln, sum2, crc, 0);
                        if (ioerr) {
                           if (quiet) {
                              rc = qserr2errno(ioerr);
                              break;
                           } else
                           if (file_print(ln,ioerr,0)) { rc = EINTR; break; }
                        } else {
                           int ok = stricmp(sum,sum2)==0;

                           if (quiet && !ok) { rc = EBADF; break; } else
                           if (!quiet)
                              if (file_print(ln,0,ok?"OK":"FAILED"))
                                 { rc = EINTR; break; }
                        }
                        rc = 0;
                     }
                     if (rc>0)
                        if (quiet) break; else {
                           cmd_printseq("Invalid line %u", 0, 0, ii+1);
                           rc = 0;
                        }
                  }
                  free(cf);
               } else
                  rc = ENOMEM;
            }
            if (rc>=0 && quiet) {
               env_setvar("ERRORLEVEL", rc);
               rc = 0;
            }
         }
      } else {
         int     next = 0;
         char  *sumfn = 0;
         FILE  *sumfl = 0;
         u32t  sumsfn = 0;
         // /q name file [file]
         if (stricmp(args->item[next],"/q")==0) {
            next+=2;
            if (args->count>next) sumfn = args->item[next-1];
         }
         while (next<args->count) {
            char   *src = args->item[next++];
            long    pos = 0;
            dir_t    fi;
            qserr   err;
            // first time file creation
            if (sumfn && !sumfl) {
               /* select append here to prevent killing of randomly typed
                  binary file */
               sumfl = _fsopen(sumfn, "ab", SH_DENYWR);
               if (!sumfl) {
                  printf("Unable to open output file \"%s\"\n", sumfn);
                  rc = 0;
                  break;
               }
               // ftell will return -1 if file longer than 2Gb
               pos = ftell(sumfl);
               if (pos<0) {
                  printf("Wrong output file \"%s\"\n", sumfn);
                  rc = 0;
                  break;
               } else
               if (!pos) pos=-1;
               // exclude sum file by sfn, not by name
               sumsfn = getsfn(sumfl);
            }
            err = _dos_findfirst(src, _A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_ARCH, &fi);
            if (err) {
               // no quiet?
               if (!sumfn)
                  if (file_print(src,err,0)) { rc = EINTR; break; }
            } else
            while (!err) {
               // ".." can be returned here
               if ((fi.d_attr&_A_SUBDIR)==0) {
                  char      *fp = strcat_dyn(strdup(fi.d_openpath), fi.d_name),
                         sumstr[36];
                  err = file_md5(fp, sumstr, crc, sumsfn);
                  free(fp);
                  
                  if (err) {
                     if (!sumfn && err!=FFFF)
                        if (file_print(fi.d_name,err,0)) { rc = EINTR; break; }
                  } else {
                     char *line = crc?sprintf_dyn("%-32s %s", fi.d_name, sumstr):
                                      sprintf_dyn("%s *%s", sumstr, fi.d_name);
                     int    esc = 0;
                     if (sumfl) {

                        if (pos) {
                           /* check for \n at the end of file and add one, but only
                              here, when we have other data to write */
                           if (pos>0) {
                              fseek(sumfl, -1, SEEK_CUR);
                              if (getc(sumfl)!='\n') putc('\n',sumfl);
                           } else
                           if (crc) {
                              /* this SFV file, comment with ; supported, so
                                 write date/time mark here */
                              time_t    now = time(0);
                              struct tm tme;
                              char      str[80];
                              localtime_r(&now, &tme);
                              strftime(str, 80, "; File created %d %b %Y at %T\n", &tme);
                              fputs(str, sumfl);
                           }
                           pos = 0;
                        }
                        fputs(line, sumfl);
                        fputs("\n", sumfl);
                     } else
                        esc = cmd_printseq("%s", 0, 0, line);
                     free(line);
                     if (esc) { rc = EINTR; break; }
                  }
               }
               err = _dos_findnext(&fi);
            }
            _dos_findclose(&fi);
            // go to the error message below
            if (rc>0) break;
            // at least one file entry processed
            rc = 0;
         }
         if (sumfl) fclose(sumfl);
      }
   }
   // free args clone, returned by str_parseargs() above
   free(args);
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}
