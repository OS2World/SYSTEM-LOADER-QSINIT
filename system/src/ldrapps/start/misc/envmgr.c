//
// QSINIT "start" module
// environment C library functions
//
#include "stdlib.h"
#include "qsutil.h"
#include "errno.h"
#include "internal.h"
#include "qsconst.h"
#include "qsshell.h"
#include "direct.h"

// query environment length
u32t envlen2(process_context *pq, u32t *lines) {
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
char *envcopy(process_context *pq, u32t addspace) {
   u32t len=envlen(pq);
   char *rc=(char*)malloc(len+addspace);
   if (!rc) return 0;
   if (!pq) { rc[0]=0; rc[1]=0; } else memcpy(rc,pq->envptr,len);
   return rc;
}

static char *linepos(process_context *pq, const char *en) {
   char *ep = pq->envptr, enbuf[384];
   u32t len;
   // trim spaces in env key name
   while (*en==' ') en++;
   strncpy(enbuf,en,383);
   enbuf[383]=0;
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

char*  __stdcall getenv (const char *name) {
   process_context *pq = mod_context();
   char *rc;
   if (!pq) return 0;
   rc = linepos(pq, name);
   if (!rc) return 0;
   rc = strchr(rc, '=');
   if (rc) {
      do { rc++; } while (*rc==' ');
   }
   return rc;
}

int __stdcall setenv(const char *name, const char *newvalue, int overwrite) {
   process_context *pq = mod_context();
   char  *lp;
   u32t  len, newlen;
   if (!pq) { set_errno(ENOMEM); return 1; }
   lp = linepos(pq, name);
   if (lp&&newvalue&&!overwrite) { set_errno(EACCES); return 1; }
   if (!newvalue&&!lp) return 0;
   len = envlen(pq);

   // remove env var
   if (lp) {
      newlen = strlen(lp)+1;
      len   -= lp-pq->envptr;
      if (newlen<len-1) memmove(lp, lp+newlen, len); else { *lp++=0; *lp=0; }
   }
   if (!newvalue) return 0;
   // and add it again ;)
   len = envlen(pq);
   if (len==2) len--;

   newlen = len + strlen(name) + strlen(newvalue) + 2; // "=" & "\0"
   // realloc env buffer
   if ((pq->flags|PCTX_BIGMEM)!=0 || memBlockSize(pq->envptr)<newlen) {
      char *newenv = envcopy(pq, newlen - len);
      /* drop "large block" flag or free previously self-allocated pointer.
         we do not free original block, allocated in mod_exec */
      if (pq->flags&PCTX_BIGMEM) pq->flags&=~PCTX_BIGMEM; else
      if (pq->flags&PCTX_ENVCHANGED) memFree(pq->envptr);

      pq->envptr = newenv;
      pq->flags |= PCTX_ENVCHANGED;
   }
   // add string
   lp = pq->envptr + len - 1;
   strcpy(lp, name);
   strcat(lp, "=");
   strcat(lp, newvalue);
   pq->envptr[newlen-1] = 0;
   return 0;
}

// clears the process environment area
int __stdcall clearenv(void) {
   process_context *pq = mod_context();
   if (!pq) { set_errno(ENOMEM); return 1; }
   pq->envptr[0]=0;
   pq->envptr[1]=0;
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
      // file is in the current dir?
      if (_dos_stat(name, &fi)==0) {
         char *cd = hlp_curdir(hlp_curdisk());
         if (cd) { strcpy(pathname, cd); strcat(pathname,"\\"); }
         strcat(pathname, name);
         return;
      }
      path = env_var?getenv(env_var):0;
      if (!path) return; else
      { // searching in specifed directories
         char fpath[QS_MAXPATH+1];
         str_list *lst = str_split(path,";");
         u32t ii, len, namelen=strlen(name);

         for (ii=0;ii<lst->count;ii++) {
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
                  return;
               }
            }
         }
      }
   }
}

str_list* _std str_getenv(void) {
   process_context *pq = mod_context();
   u32t lines, ii, len = envlen2(pq, &lines);
   str_list *rc = (str_list*)malloc(len + sizeof(str_list) + lines*sizeof(char*));
   char   *cpos = (char*)&rc->item[lines], *cenv = pq->envptr;

   for (ii=0;ii<lines;ii++) {
      rc->item[ii]=cpos;
      strcpy(cpos,cenv);
      len = strlen(cenv)+1;
      cpos+=len; cenv+=len;
   }
   rc->count = lines;
   return rc;
}

/** check environment variable and return integer value.
    @param name     name of env. string
    @return 1 on TRUE/ON/YES, 0 on FALSE/OFF/NO, -1 if no string,
    else integer value of env. string */
int  __stdcall env_istrue(const char *name) {
   char *ev = getenv(name), *chp;
   int  len = strlen(ev);
   if (!ev) return -1;

   // use , or ; as string delimeter
   chp = strchr(ev,',');
   if (!chp) chp = strchr(ev,';');
   if (chp) len = chp-ev;
   // check names
   if (strnicmp(ev,"TRUE",len)==0||strnicmp(ev,"ON",len)==0||
      strnicmp(ev,"YES",len)==0) return 1;
   if (strnicmp(ev,"FALSE",len)==0||strnicmp(ev,"OFF",len)==0||
      strnicmp(ev,"NO",len)==0) return 0;
   return atoi(ev);
}