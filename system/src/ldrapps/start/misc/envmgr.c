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
   u32t len=0, lncnt=0;
   char *cc;
   if (lines) *lines=0;
   if (!pq) return 0;
   cc = pq->envptr;
   do {
      u32t clen = strlen(cc)+1;
      len += clen;
      cc  += clen;
      lncnt++;
   } while (*cc);
   if (lines) *lines=lncnt;
   return len+1;
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
   char *ep = pq->envptr, enbuf[384];
   u32t len;
   // trim spaces in env key name
   while (*en==' ') en++;
   strncpy(enbuf,en,383); enbuf[383] = 0;
   len = strlen(enbuf);
   while (enbuf[len-1]==' ') len--;
   enbuf[len]=0;
   if (!len) return 0;

   do {
      char *npos=strchr(ep,'='), *aep=ep;
      while (*aep==' ') aep++;                       // trim spaces
      while (aep!=npos&&npos[-1]==' ') npos--;
      if (npos-aep==len)
         if (strnicmp(aep,enbuf,len)==0) return ep;
      ep += strlen(ep)+1;
   } while (*ep);
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
   char            *lp;
   u32t    len, newlen, rc = 0;

   env_lock();
   lp = linepos(pq, name);
   if (lp && newvalue && !overwrite) { set_errno(EACCES); rc = 1; } else
   if (!newvalue && !lp) { } else {
      len = envlen(pq);
      // remove env var
      if (lp) {
         newlen = strlen(lp)+1;
         len   -= lp-pq->envptr;
         if (newlen<len-1) memmove(lp, lp+newlen, len); else { *lp++=0; *lp=0; }
      }
      if (newvalue) {
         // and add it again ;)
         len = envlen(pq);
         if (len==2) len--;
         
         newlen = len + strlen(name) + strlen(newvalue) + 2; // "=" & "\0"
         // realloc env buffer
         if ((pq->flags|PCTX_BIGMEM)!=0 || mem_blocksize(pq->envptr)<newlen) {
            char *newenv = env_copy(pq, newlen - len);
            // make it exe-module onwed, as mod_exec do
            mem_modblockex(newenv, (u32t)pq->self);
            /* drop "large block" flag or free previously self-allocated pointer.
               we do not free original block, allocated in mod_exec */
            if (pq->flags&PCTX_BIGMEM) pq->flags&=~PCTX_BIGMEM; else
               if (pq->flags&PCTX_ENVCHANGED) mem_free(pq->envptr);
            pq->envptr = newenv;
            pq->flags |= PCTX_ENVCHANGED;
         }
         // add string
         lp = pq->envptr + len - 1;
         strcpy(lp, name);
         strcat(lp, "=");
         strcat(lp, newvalue);
         pq->envptr[newlen-1] = 0;
      }
   }
   env_unlock();
   return rc;
}

// clears the process environment area
int __stdcall clearenv(void) {
   process_context *pq = mod_context();
   env_lock();
   pq->envptr[0]=0; pq->envptr[1]=0;
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
   _utoa(value, vstr, 10);
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
      qshandle        mux = pq->rtbuf[RTBUF_ENVMUX];
      if (!mux) {
         mt_muxcreate(1, 0, &mux);
         pq->rtbuf[RTBUF_ENVMUX] = mux;
      } else 
         mt_muxcapture(mux);
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
