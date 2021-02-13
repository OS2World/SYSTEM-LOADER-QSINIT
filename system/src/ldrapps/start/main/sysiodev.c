//
// QSINIT "start" module
// "common" devices
// -------------------------------------
// console handling is actually session-based, so open/close functions
// are fake here.
//

#include "sysio.h"
#include "syslocal.h"
#include "vioext.h"
#include "qsxcpt.h"
#include "qcl/qslist.h"

#define CON_WBUF      64
#define HANDLE_AUX   (FFFF-1)
#define HANDLE_NUL   (FFFF-2)

static ptr_list sesbuf = 0;

// called in MT lock, so it is safe to modify list in it
static void _std sesexit_cb(sys_eventinfo *info) {
   u32t sno = info->info;

   // free queue for this session
   if (sesbuf && sesbuf->count()>sno) {
      char **sl = (char**)sesbuf->array();
      if (sl[sno]) {
         free(sl[sno]);
         sl[sno] = 0;
      }
   }
}

static qserr _exicc stdio_open(EXI_DATA, const char *name, st_handle_int *pfh) {
   // duplicate handle call
   if (!name) {
      if (*pfh==SESN_DETACHED) return E_SYS_INVPARM;
      // duplicate call will fail in another session
      if (*pfh!=HANDLE_AUX && *pfh!=HANDLE_NUL && *pfh!=se_sesno())
         return E_SYS_ACCESS;
   } else {
      int len = strlen(name);
      if (len!=3) return E_SYS_NOFILE;
      if (strnicmp(name,"CON",3)==0) {
         u32t sno = se_sesno();
      
         if (sno==SESN_DETACHED) return E_CON_DETACHED;
         // use session number as "internal handle"
         *pfh = sno;
      } else
      if (strnicmp(name,"AUX",3)==0) *pfh = HANDLE_AUX; else
         if (strnicmp(name,"NUL",3)==0) *pfh = HANDLE_NUL;
            else return E_SYS_NOFILE;
   }
   return 0;
}

// this value is accessible for user via io_pathinfo() call
static u32t _exicc stdio_avail(EXI_DATA, st_handle_int fh) {
   u32t avail = 0;
   if (fh && fh==se_sesno()) {
      // query # of chars in buffer for this session
      mt_swlock();
      if (sesbuf && sesbuf->count()>fh) {
         char *que = sesbuf->value(fh);
         if (que) avail = strlen(que);
      }
      mt_swunlock();
   }
   return avail;
}

static qserr _exicc stdio_read(EXI_DATA, st_handle_int fh, char *buf, u32t *size) {
   u32t toread;
   if (!size || !buf) return E_SYS_INVPARM;
   toread = *size;
   *size  = 0;
   if (!fh) return E_SYS_INVPARM;

   if (fh!=HANDLE_AUX && fh!=HANDLE_NUL) {
      // session number should match!
      if (fh!=se_sesno()) return E_SYS_ACCESS;
      if (!sesbuf) return E_SYS_SOFTFAULT;

      while (toread) {
         mt_swlock();
         if (sesbuf->count()>fh) {
            char **sl = (char**)sesbuf->array(),
                 *sep = sl[fh];
            if (sep) {
               u32t avail = strlen(sep);
               if (avail>toread) {
                  memcpy(buf, sep, toread);
                  memmove(sep, sep+toread, avail-toread+1);
                  toread = 0;
               } else {
                  memcpy(buf, sep, avail);
                  free(sep);
                  sl[fh] = 0;
                  toread -= avail;
                  buf    += avail;
               }
            }
         } else // grow list if session number is new
           while (sesbuf->count()<fh+1) sesbuf->add(0);
         mt_swunlock();
         if (toread) {
            char *str = key_getstr(0);
            u32t  len = str ? strlen(str) : 0;

            if (len) {
               char **sl, *sep;
               mt_swlock();
               sl  = (char**)sesbuf->array();
               sep = sl[fh];
               if (!sep) {
                  mem_shareblock(str);
                  sl[fh] = str;
               } else {
                  // block owner type will be preserved by realloc()
                  sl[fh] = strcat_dyn(sep, str);
                  free(str);
               }
               mt_swunlock();
            } else
            if (str) free(str);
         }
      }
   }
   return 0;
}

static qserr _exicc stdio_write(EXI_DATA, st_handle_int fh, void *buf, u32t *size) {
   if (!size || !buf || !fh) return E_SYS_INVPARM;

   if (fh==HANDLE_NUL) {} else
   if (fh!=HANDLE_AUX && fh!=se_sesno()) {
      *size = 0;
      return E_SYS_ACCESS;
   } else {
      char *cpb = buf;
      u32t  len = *size;

      while (len>0) {
         u32t      cpy;

         if (fh!=HANDLE_AUX) {
            char     pbuf[CON_WBUF+1];
            cpy = len>CON_WBUF?CON_WBUF:len;
      
            memcpy(pbuf, cpb, cpy);
            pbuf[cpy] = 0;
            ansi_strout(pbuf);
         } else {
            char *eolp = strchr(cpb, '\n');
            u32t   ii;
            if (eolp) cpy = eolp-cpb+1; else cpy = len;
            
            // print to log and debug COM port separately
            log_pushb(LOG_MISC, cpb, cpy, 0);
            for (ii=0; ii<cpy; ii++) {
               char ch = cpb[ii];
               if (ch=='\n' && ii && cpb[ii-1]!='\r') hlp_seroutstr("\n");
                  else hlp_seroutchar(ch);
            }
         }
         len -= cpy;
         cpb += cpy;
      }
   }
   return 0;
}

static qserr _exicc stdio_flush(EXI_DATA, st_handle_int fh) { return 0; }

static qserr _exicc stdio_close(EXI_DATA, st_handle_int fh) {
   if (fh==HANDLE_AUX || fh==HANDLE_NUL) return 0;
   if (!fh) return E_SYS_INVPARM;
   if (fh!=se_sesno()) return E_SYS_ACCESS;
   return 0;
}

static str_list* _exicc stdio_devlist(EXI_DATA) {
   // order here is used for pubfno: 1|2|3
   return str_split("CON|AUX|NUL","|");
}

static void *m_list[] = { stdio_open, stdio_read, stdio_avail, stdio_write,
   stdio_flush, stdio_close, stdio_devlist };

extern u32t stdio_classid;

void register_stdio(void) {
   // session exit notification
   sys_notifyevent(SECB_SEEXIT|SECB_GLOBAL, sesexit_cb);
   // session input buffers
   sesbuf = NEW_G(ptr_list);

   if (sizeof(_qs_sysstream)!=sizeof(m_list)) _throwmsg_("stdio: size mismatch");
   // register private(hidden) class
   stdio_classid = exi_register("qs_sys_stdio", m_list, sizeof(m_list)/sizeof(void*),
      0, EXIC_PRIVATE|EXIC_EXCLUSIVE, 0, 0, 0);
   if (!stdio_classid) _throwmsg_("stdio - reg.error");
}
