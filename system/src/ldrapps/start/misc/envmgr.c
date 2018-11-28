//
// QSINIT "start" module
// environment C library functions
//
#include "qsutil.h"
#include "errno.h"
#include "syslocal.h"
#include "qsconst.h"
#include "qsshell.h"

#define envlen(pq) envlen2(pq,0)

/** query environment length.
    In MT mode must be called inside RTBUF_ENVMUX environment mutex.
    @param [in]  pq     Process context.
    @param [out] lines  Number of lines, can be 0.
    @return full length, including trailing zero */
static u32t envlen2(process_context *pq, u32t *lines) {
   u32t  size = 0, 
         lcnt = 0;
   char  *pos = pq->envptr;
   while (*pos) {
      u32t len = strlen(pos) + 1;
      size+=len; pos+=len; lcnt++;
   }
   if (lines) *lines = lcnt;
   return size + 1;
}

// make copy of process environment (in malloc buffer)
char* _std env_copy(process_context *pq, u32t addspace) {
   u32t   len;
   char   *rc;

   env_lock();
   len = envlen(pq);
   rc  = (char*)malloc_local(len+addspace);
   if (rc) {
      if (addspace) memset(rc+len, 0, addspace);
      memcpy(rc, pq->envptr, len);
   }
   env_unlock();
   return rc;
}

static char *linepos(process_context *pq, const char *en) {
   char  *ep = pq->envptr, enbuf[384];
   u32t  len;
   if (!en) return 0;
   // trim spaces in env key name
   while (*en==' ') en++;
   strncpy(enbuf, en, 383); enbuf[383] = 0;
   trimright(enbuf, " \t");
   len = strlen(enbuf);
   if (!len) return 0;

   while (*ep) {
      char *npos = strchr(ep,'='), *aep = ep;
      if (npos) {
         while (*aep==' ') aep++; // trim spaces
         while (aep!=npos && npos[-1]==' ') npos--;
         if (npos-aep==len)
            if (strnicmp(aep,enbuf,len)==0) return ep;
      }
      ep += strlen(ep)+1;
   }
   return 0;
}

char*  __stdcall getenv(const char *name) {
   process_context *pq = mod_context();
   char            *rc;
   env_lock();
   rc = linepos(pq, name);
   if (rc) {
      rc = strchr(rc, '=');
      if (rc)
         do { rc++; } while (*rc==' ');
   }
   env_unlock();
   return rc;
}

int __stdcall setenv(const char *name, const char *newvalue, int overwrite) {
   process_context *pq = mod_context();
   u32t             rc = 0;
   char            *lp;
   // just to not broke the logic somewhere
   if (!name || strchr(name,'=') || !name[0]) {
      set_errno(EINVAL);
      return 1; 
   }
   // lock mutex
   env_lock();
   lp = linepos(pq, name);
   if (lp && newvalue && !overwrite) { set_errno(EACCES); rc = 1; } else
   if (newvalue || lp) {
      u32t  len = envlen(pq),
           clen = lp ? strlen(lp)+1 : 0,
           nlen = newvalue ? strlen(name)+strlen(newvalue)+2 : 0;
      int  diff = nlen - clen;
      // delete value if no size match
      if (lp && diff) {
         u32t rem = len - (lp - pq->envptr) - clen;
         if (rem<=1) *lp = 0; else memmove(lp, lp+clen, rem); 
         len = envlen(pq);
      }
      if (nlen) {
         // reallocate environment to fit new variable
         if (diff>0) {
            /* realloc env buffer:
               - pid 1 (QSINIT) has env. initially allocated in the large memory
                 block - copy is mandatory (heap function will panic on this block)
               - for any other - just check available block size */
            if (pq->pid==1 && (pq->flags&PCTX_ENVCHANGED)==0 ||
                mem_blocksize(pq->envptr) < len+nlen)
            {
               // env_copy calc length again - with deleted variable!
               char *newenv = env_copy(pq, nlen);
               // make it exe-module onwed for auto-release on exit
               mem_modblockex(newenv, (u32t)pq->self);
               // free previously self-allocated pointer
               if (pq->flags&PCTX_ENVCHANGED) free(pq->envptr);
               pq->envptr = newenv;
               pq->flags |= PCTX_ENVCHANGED;
            }
         }
         if (diff) {
            lp = pq->envptr + len - 1;
            lp[nlen] = 0;
         }
         strcpy(lp, name);
         strcat(lp, "=");
         strcat(lp, newvalue);
      }
   }
   env_unlock();
   return rc;
}

// clears the process environment area
int __stdcall clearenv(void) {
   process_context *pq = mod_context();
   env_lock();
   pq->envptr[0] = 0;
   env_unlock();
   return 0;
}

/** searches for the file specified by name in the list of directories assigned
    to the environment variable specified by env_var.
    Common values for env_var are PATH, LIB and INCLUDE. */
void __stdcall _searchenv(const char *name, const char *env_var, char *pathname) {
   pathname[0]=0;
   if (!name) return; else {
      char  *path;
      dir_t    fi;
      // file is relative to current dir?
      if (_dos_stat(name, &fi)==0) {
         _fullpath(pathname, name, _MAX_PATH+1);
         return;
      }
      // lock it to be safe
      env_lock();
      path = env_var?getenv(env_var):0;
      if (!path) { env_unlock(); return; } else
      { // searching in specifed directories
         char fpath[QS_MAXPATH+1];
         str_list *lst = str_split(path,";");
         u32t  namelen = strlen(name), ii, len;
         // should be no use of this string after unlock
         path = 0;
         env_unlock();

         for (ii=0; ii<lst->count; ii++) {
            strncpy(fpath,lst->item[ii],QS_MAXPATH);
            fpath[QS_MAXPATH]=0;
            len = strlen(fpath);
            while (len&&fpath[len-1]==' ') len--;
            if (len&&(fpath[len-1]=='\\'||fpath[len-1]=='/')) len--;
            fpath[len++]='\\';
            if (len+namelen<=QS_MAXPATH) {
               strcpy(fpath+len,name);
               if (_dos_stat(fpath, &fi)==0) {
                  strcpy(pathname, fpath);
                  break;
               }
            }
         }
         free(lst);
      }
   }
}

str_list* _std str_getenv(void) {
   process_context    *pq = mod_context();
   u32t    lines, ii, len;
   str_list           *rc;
   char      *cpos, *cenv;

   env_lock();
   len  = envlen2(pq, &lines);
   rc   = (str_list*)malloc_local(len + sizeof(str_list) + lines*sizeof(char*));
   cpos = (char*)&rc->item[lines];
   cenv = pq->envptr;

   for (ii=0; ii<lines; ii++) {
      rc->item[ii]=cpos;
      strcpy(cpos,cenv);
      len = strlen(cenv)+1;
      cpos+=len; cenv+=len;
   }
   rc->count = lines;
   env_unlock();
   return rc;
}

/** check environment variable and return integer value.
    @param name     name of env. string
    @return 1 on TRUE/ON/YES, 0 on FALSE/OFF/NO, -1 if no string,
    else integer value of env. string */
int  __stdcall env_istrue(const char *name) {
   char   *ev;
   int    res = -1;

   env_lock();
   ev = getenv(name);
   if (ev) {
      int   len = strlen(ev);
      char *chp = strchr(ev,',');
      // use , or ; as string delimeter
      if (!chp) chp = strchr(ev,';');
      if (chp) len = chp-ev;
      // check names
      if (strnicmp(ev,"TRUE",len)==0 || strnicmp(ev,"ON",len)==0 ||
         strnicmp(ev,"YES",len)==0) res = 1; else
      if (strnicmp(ev,"FALSE",len)==0 || strnicmp(ev,"OFF",len)==0 ||
         strnicmp(ev,"NO",len)==0) res = 0; 
      else res = atoi(ev);
   }
   env_unlock();
   return res;
}

/// set environment variable (integer) value
void _std env_setvar(const char *name, int value) {
   char  vstr[12];
   itoa(value, vstr, 10);
   setenv(name, vstr, 1);
}

/** get environment variable.
    Unlike getenv() - function is safe in MT mode.
    @param name     name of env. string 
    @return string in heap block (must be free()-ed) or 0. */
char* _std env_getvar(const char *name) {
   char *rc;
   if (!name) return 0;
   env_lock();
   rc = getenv(name);
   if (rc) mem_localblock(rc = strdup(rc));
   env_unlock();
   return rc;
}

/// lock environment access mutex for this process (in MT mode)
void env_lock(void) {
   if (!in_mtmode) return; else {
      process_context *pq = mod_context();
      qshandle        mux;

      mt_swlock();
      mux = pq->rtbuf[RTBUF_ENVMUX];
      if (!mux) {
         mt_muxcreate(1, 0, &mux);
         pq->rtbuf[RTBUF_ENVMUX] = mux;
      } else
         mt_muxcapture(mux);
      mt_swunlock();
   }
}

/// unlock environment access mutex
void env_unlock(void) {
   if (!in_mtmode) return; else {
      process_context *pq = mod_context();
      qshandle        mux = pq->rtbuf[RTBUF_ENVMUX];
      if (mux) mt_muxrelease(mux);
   }
}
