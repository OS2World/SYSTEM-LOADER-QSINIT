//
// QSINIT "start" module
// "common" devices
// -------------------------------------
//

#include "sysio.h"
#include "syslocal.h"
#include "vioext.h"
#include "qsxcpt.h"

#define CON_WBUF      64
#define HANDLE_AUX   (FFFF-1)
#define HANDLE_NUL   (FFFF-2)

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

static qserr _exicc stdio_read(EXI_DATA, st_handle_int fh, void *buf, u32t *size) {
   if (!size || !buf) return E_SYS_INVPARM;
   *size = 0;
   if (!fh) return E_SYS_INVPARM;

   if (fh!=HANDLE_AUX && fh!=HANDLE_NUL) {
      // session number should match!
      if (fh!=se_sesno()) return E_SYS_ACCESS;


   }
   return 0;
}

static u32t  _exicc stdio_avail(EXI_DATA, st_handle_int fh) {
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
   // order here is uses for pubfno: 1|2|3
   return str_split("CON|AUX|NUL","|");
}

static void *m_list[] = { stdio_open, stdio_read, stdio_avail, stdio_write,
   stdio_flush, stdio_close, stdio_devlist };

extern u32t stdio_classid;

void register_stdio(void) {
   if (sizeof(_qs_sysstream)!=sizeof(m_list)) _throwmsg_("stdio: size mismatch");
   // register private(unnamed) class
   stdio_classid = exi_register("qs_sys_stdio", m_list, sizeof(m_list)/sizeof(void*),
      0, EXIC_PRIVATE|EXIC_EXCLUSIVE, 0, 0, 0);
   if (!stdio_classid) _throwmsg_("stdio - reg.error");
}
