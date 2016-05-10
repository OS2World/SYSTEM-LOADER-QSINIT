//
// QSINIT "start" module
// volume functions
// -------------------------------------
//
// moved here from QSINIT binary

#include "qsutil.h"
#include "qsint.h"
#include "qcl/sys/qsvolapi.h"
#include "qcl/qslist.h"
#include "internal.h"
#include "stdlib.h"
#include "qsmodext.h"
#include "qsstor.h"
#include "qserr.h"
#include "sysio.h"

// additional attrs for sft_check_mode()
#define  IOFM_DEL       IOFM_CLOSE_DEL
#define  IOFM_REN       0x80
// alloc increment value
#define  SFT_INC        20
// increment for public fno (in io_handle_info/io_direntry_info structs)
#define  SFT_PUBFNO     16

static qs_sysvolume ve[DISK_COUNT];
static u8t       fsver[DISK_COUNT];     ///< FsType cache for hlp_volinfo()
extern u32t          fatio_classid;

static volatile u32t     v_mounted = 0; ///< mounted volumes (for fast check)

static
sft_entry* volatile       *sftable = 0;
static
io_handle_data* volatile   *ftable = 0;
static volatile u32t sftable_alloc = 0,
                      ftable_alloc = 0;
vol_data                  *_extvol = 0; ///< const pointer from QSINIT binary

#define is_mounted(vol) (v_mounted&1<<(u8t)vol?1:0)

void register_fatio(void);

/** volume by full path.
    Must be called in locked state (volume array access) */
static qserr vol_from_fp(const char *fullpath, qs_sysvolume *vol) {
   char ltr;
   if (!fullpath) return E_SYS_ZEROPTR;
   if (fullpath[1]!=':') return E_SYS_INVNAME;
   if (ltr>='0' && ltr<='9') *vol = ve[ltr-'0']; else {
      ltr = toupper(fullpath[0]);
      if (ltr>='A' && ltr<='A'+DISK_COUNT-1) *vol = ve[ltr-'A']; else
         return E_SYS_INVNAME;
   }
   return *vol?0:E_DSK_NOTMOUNTED;
}

static void sft_logerr(u32t ii) {
   log_it(0, "Damaged SFT entry %u (%s)\n", ii, sftable[ii]->fpath);
}

static void ioh_logerr(sft_entry *fe, io_handle_data *hd, u32t index) {
   log_it(0, "SFT damaged. File \"%s\", ioh %08X (%u)\n", fe->fpath, hd, index);
}

/** search in SFT by full path.
    Must be called in locked state (sft access) */
static u32t sft_find(const char *fullpath) {
   if (fullpath && *fullpath) {
      u32t ii;
      // skip entry 0
      for (ii=1; ii<sftable_alloc; ii++)
         if (sftable[ii])
            if (sftable[ii]->sign!=SFTe_SIGN) sft_logerr(ii); else {
               char *fp = sftable[ii]->fpath;
               /* stricmp takes in mind current codepage, so let it be in this
                  way for a while */
               if (fp)
                  if (stricmp(fullpath,fp)==0) return ii;
            }
   }
   return 0;
}

/** can this file be opened with "mode"?
    Must be called in locked state (sft access).
    Combined share value for sft_entry.file.sbits placed in newmode on exit.
    IOFM_DEL & IOFM_REN can be used here to check possibility of del/ren op. */
static qserr sft_check_mode(sft_entry *fe, u32t mode, u32t *newmode) {
   if (fe->sign!=SFTe_SIGN) return E_SYS_INVOBJECT;
   if (fe->type!=IOHT_FILE) return E_SYS_INVHTYPE; else {
      u32t smask = mode<<8 & IOFM_SHARE_MASK; // op type to share mask
      // something disabled? then check file by file
      if ((fe->file.sbits&smask)!=smask) {
         u32t  pid = mod_getpid(),
               idx = fe->ioh_first;
         while (idx) {
            io_handle_data *hd = ftable[idx];
            if (hd->sign!=IOHd_SIGN) {
               ioh_logerr(fe, hd, idx);
               return E_SYS_INVHTYPE;
            } else
            if (hd->pid!=pid)
               if ((hd->file.omode&smask)!=smask) return E_SYS_SHARE;
            idx = hd->next;
         }
      }
      if (newmode) *newmode = fe->file.sbits&(mode&IOFM_SHARE_MASK);
      return 0;
   }
}

static u32t common_alloc(void **ptr, volatile u32t *alloc, u32t recsize) {
   u32t  ii, oldsz, addsz;
   void *np;
   if (*alloc)
      for (ii=0; ii<*alloc; ii++)
         if (((ptrdiff_t*)(*ptr))[ii]==0) return ii;
   // expand array (realloc accepts zero, heap context here will be "DLL")
   oldsz = recsize*(ii=*alloc);
   addsz = recsize*SFT_INC;
   np    = realloc(*ptr, oldsz+addsz);
   if (np==0) return 0; else *ptr = np;
   memset((u8t*)*ptr+oldsz, 0, addsz);
   *alloc+=SFT_INC;
   return ii;
}


/** alloc new sft_entry entry.
    Must be called in locked state (sft access). */
static u32t sft_alloc(void) {
   return common_alloc((void**)&sftable, &sftable_alloc, sizeof(sft_entry*));
}

/** alloc new io_handle_data entry.
    Must be called in locked state (io_handle array access). */
static u32t ioh_alloc(void) {
   return common_alloc((void**)&ftable, &ftable_alloc, sizeof(io_handle_data*));
}

u8t _std io_curdisk(void) {
   mt_prcdata *pd = get_procinfo();
   return pd->piCurDrive;
}

qserr _std io_setdisk(u8t drive) {
   if (drive>DISK_COUNT-1) return E_SYS_INVPARM; else
   if (!ve[drive]) return E_DSK_NOTMOUNTED; else {
      mt_prcdata *pd = get_procinfo();
      // write byte must be atomic
      pd->piCurDrive = drive;
      return 0;
   }
}

qserr _std io_diskdir(u8t drive, char *buf, u32t size) {
   if (!buf) return E_SYS_ZEROPTR;
   if (size<4 || drive>'Z'-'A') return E_SYS_INVPARM; else {
      mt_prcdata  *pd = get_procinfo();
      u32t      cdlen, rc;
      char      *cdir;
      mt_swlock();
      cdir  = pd->piCurDir[drive];
      cdlen = cdir?strlen(cdir)+1:4;
      rc    = size>=cdlen?0:E_SYS_BUFSMALL;
      if (!rc)
         if (cdir) strcpy(buf, cdir); else
            sprintf(buf, "%c:\\", pd->piCurDrive+'A');
      mt_swunlock();
      return rc;
   }
}

qserr _std io_curdir(char *buf, u32t size) {
   if (!buf) return E_SYS_ZEROPTR;
   if (size<4) return E_SYS_INVPARM;
   return io_diskdir(get_procinfo()->piCurDrive, buf, size);
}

char* _std io_fullpath_int(char *dst, const char *path, u32t size, qserr *err) {
   char   tmp[QS_MAXPATH+1], *str;
   u32t  plen = path?strlen(path):0;
   *err = 0;
   if (plen>QS_MAXPATH) { *err = E_SYS_LONGNAME; return 0; }

   if (path && path[1]==':') {
      strcpy(tmp, path);
      if (tmp[0]>='0' && tmp[0]<='9') tmp[0]+='A'-'0'; else {
         tmp[0] = toupper(tmp[0]);
         if (tmp[0]<'A' || tmp[0]>'Z') { *err = E_SYS_INVNAME; return 0; }
      }
   } else
   if (!path || path[0]!='/' && path[0]!='\\') {
      u32t  mplen;
      qserr  dres = io_curdir(tmp, QS_MAXPATH+1);
      if (dres) { *err = dres; return 0; }
      mplen = strlen(tmp);
      if (tmp[mplen-1]!='\\') { strcat(tmp,"\\"); mplen++; }
      if (mplen+plen+1>QS_MAXPATH) { *err = E_SYS_LONGNAME; return 0; }
      strcat(tmp,path);
   } else {
      if (plen+3>QS_MAXPATH) { *err = E_SYS_LONGNAME; return 0; }
      tmp[0] = 'A' + io_curdisk();
      tmp[1] = ':';
      tmp[2] = '\\';
      strcpy(tmp+3, path+1);
   }
   if (!dst) dst = (char*)malloc_local(size = QS_MAXPATH+1);
   dst[0] = 0;

   str = tmp;
   do {
      str=strpbrk(str,"/.\\");
      if (!str) break;
      switch (*str) {
         case '.':
            if (*(str-1)=='\\') {
               u32t  idx=1, dbl;
               if ((dbl=str[1]=='.')!=0) idx++; //  /./  /.\ /../ /..\\ /. /..
               if (str[idx]=='/' || str[idx]=='\\' || !str[idx]) {
                  char *ps = str-1;
                  if (dbl && str>tmp+1) { // "/../path" work as "/path"
                     ps = str-2;
                     while (ps!=tmp&&*ps!='\\') ps--;
                  }
                  memmove(ps, str+idx, strlen(str+idx)+1);
                  str=ps-1;
               }
            } else
            if (str[1]==0) *str=0;
            break;
         case '/': *str='\\';
         case '\\':
            if (str!=tmp)
               if (*(str-1)=='\\')
                  memmove(str, str+1, strlen(str+1)+1);
            break;
      }
   } while (*++str);

   if (tmp[1]==':' && tmp[2]==0) strcat(tmp,"\\");

   if (size<strlen(tmp)+1) { *err = E_SYS_BUFSMALL; return 0; }
   strcpy(dst,tmp);
   return dst;
}

qserr _std io_chdir(const char *path) {
   if (!path) return E_SYS_ZEROPTR; else {
      qserr    res;
      char     *fp;

      mt_swlock();
      fp = io_fullpath_int(0, path, 0, &res);
      if (!res) {
         qs_sysvolume vol;
         res = vol_from_fp(fp, &vol);
         if (!res) {
            mt_prcdata  *pd = get_procinfo();
            int      volnum = vol->drive();

            if (volnum<0) res = E_DSK_NOTMOUNTED; else {
               dir_handle_int  dh;
               res = vol->diropen(fp, &dh);
               if (!res) {
                  if (!pd->piCurDir[volnum]) {
                     pd->piCurDir[volnum] = fp;
                     fp = 0;
                  } else
                     strcpy(pd->piCurDir[volnum], fp);

                  pd->piCurDrive = volnum;
                  vol->dirclose(dh);
               }
            }
         }
      }
      if (fp) free(fp);
      mt_swunlock();

      return res;
   }
}

qserr _std io_fullpath(char *buffer, const char *path, u32t size) {
   char  *rc;
   qserr res;
   if (!buffer) return E_SYS_ZEROPTR;

   rc = io_fullpath_int(buffer, path, size, &res);
   return rc?res:0;
}


qserr _std io_open(const char *name, u32t mode, io_handle *pfh, u32t *action) {
   static const s8t mode_tr[8] = { -1, SFOM_CREATE_NEW, -1, -1, SFOM_OPEN_EXISTING,
                                   SFOM_OPEN_ALWAYS, SFOM_OPEN_EXISTING, SFOM_CREATE_ALWAYS };
   qserr         rc;
   qs_sysvolume vol;
   int        openm = mode_tr[mode&IOFM_OPEN_MASK];
   sft_entry    *fe = 0;
   int        newfp = 0;
   u32t      tmpact;
   char         *fp;
   if (!name || !pfh) return E_SYS_ZEROPTR;
   // someone ORed it too ugly
   if (openm<0 || (mode&(IOFM_READ|IOFM_WRITE))==0) return E_SYS_INVPARM;
   // TRUNCATE_EXISTING requires write access (as in windows)
   if ((mode&IOFM_OPEN_MASK)==IOFM_TRUNCATE_EXISTING && (mode&IOFM_WRITE)==0)
      return E_SYS_INCOMPPARAMS;
   // unknown bits?
   if ((mode&~(IOFM_OPEN_MASK|IOFM_READ|IOFM_WRITE|IOFM_SHARE_MASK|IOFM_CLOSE_DEL)))
      return E_SYS_INVPARM;

   if (!action) action = &tmpact;

   *action = IOFN_EXISTED;
   *pfh    = 0;
   fp      = io_fullpath_int(0, name, 0, &rc);
   if (!fp) return rc;

   mt_swlock();
   rc = vol_from_fp(fp, &vol);
   if (!rc) {
      io_handle_int   fhi = 0;
      u32t            fno = sft_find(fp);
      u32t            pid = mod_getpid();
      u32t           ifno = ioh_alloc();

      if (!ifno) rc = E_SYS_NOMEM; else
      // file in sft already?
      if (fno) {
         fe  = sftable[fno];
         if (fe->type!=IOHT_FILE) rc = E_SYS_ACCESS; else {
            u32t  newmode;

            fhi = fe->file.fh;
            // check sharing & get new share mode;
            rc = sft_check_mode(fe, mode, &newmode);
            // update common share mode
            if (!rc) fe->file.sbits = newmode;
         }
      } else {
         rc = vol->open(fp, openm, &fhi, action);
         // log_it(2,"vol->open(%s) == %08X\n", fp, rc);

         // new file entries in sft & ioh
         if (rc==0) {
            fno = sft_alloc();
            if (!fno) rc = E_SYS_NOMEM;
         }
         if (rc==0) {
            fe = (sft_entry*)malloc(sizeof(sft_entry));
            fe->sign       = SFTe_SIGN;
            fe->type       = IOHT_FILE;
            fe->fpath      = fp;
            fe->pub_fno    = SFT_PUBFNO+fno;
            fe->file.vol   = vol;
            fe->file.fh    = fhi;
            fe->file.sbits = mode&IOFM_SHARE_MASK;
            fe->file.del   = 0;
            fe->file.renn  = 0;
            fe->ioh_first  = 0;
            fe->broken     = 0;
            // add new entries
            sftable[fno]   = fe;
            newfp = 1;
            __set_shared_block_info(fe, "sft data", fno-1);
            __set_shared_block_info(fp, "sft name", fno-1);
         }
      }
      if (!rc) {
         io_handle_data *fh = (io_handle_data*)malloc(sizeof(io_handle_data));
         fh->sign       = IOHd_SIGN;
         fh->file.pos   = 0;
         fh->file.omode = mode;
         fh->pid        = pid;
         fh->sft_no     = fno;
         fh->lasterr    = 0;
         // link it
         fh->next       = fe->ioh_first;
         fe->ioh_first  = ifno;
         fe->file.del  |= mode&IOFM_CLOSE_DEL?1:0;
         ftable[ifno]   = fh;

         // truncate bit on?
         if (mode&2) {
            rc = vol->setsize(fhi, 0);
            // truncate failed - close this file & return error
            if (rc) {
               mt_swunlock();
               io_close(IOH_HBIT+ifno);
               return rc;
            }
         }
         if (!rc) *pfh = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   // full path was not used in sft?
   if (!newfp) free(fp);
   return rc;
}

/** check user io_handle handle and return pointers to objects.
    @attention if no error returned, then MT lock IS ON!
    @param [in]  fh            handle to check
    @param [in]  accept_types  IOHT_* combination (allowed handle types)
    @param [out] pfh           pointer to io_handle_data, can be 0
    @param [out] pfe           pointer to sft_entry, can be 0
    @return error code */
static qserr io_check_handle(io_handle ifh, u32t accept_types,
                             io_handle_data **pfh, sft_entry **pfe)
{
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          res;
   if ((ifh^=IOH_HBIT)>=ftable_alloc) return E_SYS_INVPARM;

   mt_swlock();
   fh = ftable[ifh];

   if (!fh || fh->sign!=IOHd_SIGN) res = E_SYS_INVOBJECT; else
   if (fh->sft_no>=sftable_alloc) res = E_SYS_INVOBJECT; else {
      fe = sftable[fh->sft_no];
      if (!fe || fe->sign!=SFTe_SIGN) {
         log_it(3, "Warning! ioh %u points to missing SFT entry %u!\n",
            fh, fh->sft_no);
         res = E_SYS_INVOBJECT;
      } else
      if ((fe->type&accept_types)==0) res = E_SYS_INVHTYPE; else {
         u32t pid = mod_getpid();
         // file detached if fh->pid==0
         if (fh->pid && pid!=fh->pid) res = E_SYS_PRIVATEHANDLE; else {
            // lock is ON on successful exit!
            if (pfh) *pfh = fh;
            if (pfe) *pfe = fe;
            return 0;
         }
      }
   }
   mt_swunlock();
   return res;
}

static u32t io_common(io_handle ifh, void *buffer, u32t size, int read) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!size || !buffer) return 0;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (err) return 0;

   //log_it(2,"vol->iocommon(%LX, %u)\n", fh->file.pos, size);

   if (fe->broken) {
      err  = E_SYS_BROKENFILE;
      size = 0;
   } else
   if (read && (fh->file.omode&IOFM_READ)==0 ||
      !read && (fh->file.omode&IOFM_WRITE)==0)
   {
      err  = E_SYS_ACCESS;
      size = 0;
   } else
      err  = (read?fe->file.vol->read:fe->file.vol->write)
             (fe->file.fh, fh->file.pos, buffer, &size);
   // log_it(2,"vol->iocommon() == %u, err %08X\n", size, err);
   fh->file.pos+= size;
   fh->lasterr  = err;

   mt_swunlock();
   return size;
}

u32t _std io_read(io_handle fh, const void *buffer, u32t size) {
   return io_common(fh, (void*)buffer, size, 1);
}

u32t _std io_write(io_handle fh, const void *buffer, u32t size) {
   return io_common(fh, (void*)buffer, size, 0);
}

u64t _std io_seek(io_handle ifh, s64t offset, u32t origin) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   u64t          size;
   s64t           pos;
   if (origin>IO_SEEK_END) return FFFF64;
   if (origin==IO_SEEK_SET && offset<0) return FFFF64;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (err) return FFFF64;

   if (!fe->broken) err = fe->file.vol->size(fe->file.fh, &size); 
      else err = E_SYS_BROKENFILE;
   // hope we have no true 64-bit size of the file :)
   if (err) { fh->lasterr = err; pos = -1; } else {
      switch (origin) {
         case IO_SEEK_SET: pos = 0; break;
         case IO_SEEK_CUR: pos = fh->file.pos; break;
         case IO_SEEK_END: pos = size; break;
      }
      pos += offset;
      if (pos<0) { fh->lasterr = E_SYS_SEEKERR; pos = -1; } else
      if (pos>size) {
         if ((fh->file.omode&IOFM_WRITE)==0) {
            fh->lasterr = E_SYS_ACCESS; pos = -1;
         } else {
            err = fe->file.vol->setsize(fe->file.fh, pos);
            fh->lasterr = err;
            if (!err) fh->file.pos = pos;
         }
      } else {
         fh->lasterr  = 0;
         fh->file.pos = pos;
      }
   }
   mt_swunlock();
   return pos;

}

u64t _std io_pos(io_handle ifh) {
   io_handle_data *fh;
   qserr          err;
   u64t           res;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, 0);
   if (err) return FFFF64;
   res = fh->file.pos;
   mt_swunlock();
   return res;
}

qserr _std io_flush(io_handle ifh) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (!err) {
      if (fe->broken) err = E_SYS_BROKENFILE; else
         if ((fh->file.omode&IOFM_WRITE)==0) err = E_SYS_ACCESS; else
            err = fe->file.vol->flush(fe->file.fh);
      fh->lasterr = err;
      mt_swunlock();
   }
   return err;
}

/// unlink ioh from sft and zero ftable entry
static void ioh_unlink(sft_entry *fe, u32t ioh_index) {
   io_handle_data *fh = ftable[ioh_index];
   // unlink it from sft entry
   if (fe->ioh_first==ioh_index) fe->ioh_first = fh->next; else {
      io_handle_data *eh = ftable[fe->ioh_first];
      while (eh->next && eh->next!=ioh_index) eh = ftable[eh->next];
      if (eh->next==ioh_index) {
         eh->next = fh->next;
      } else {
         log_it(0, "Warning! ioh %u is not listed in SFT entry %u!\n",
            fh, fh->sft_no);
      }
   }
   ftable[ioh_index] = 0;
}

qserr _std io_close(io_handle ifh) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (!err) {
      ifh ^= IOH_HBIT;
      // unlink it from sft entry & ioh array
      ioh_unlink(fe, ifh);

      fh->sign = 0;
      // we reach real close here!
      if (fe->ioh_first==0) {
         sftable[fh->sft_no] = 0;

         if (!fe->broken) {
            err = fe->file.vol->close(fe->file.fh);
            // closed without error -> process delayed ren/del ops
            if (!err) {
               if (fe->file.del) {
                  err = fe->file.vol->remove(fe->fpath);
               } else
               if (fe->file.renn) {
                  err = fe->file.vol->renmove(fe->fpath, fe->file.renn);
               }
            }
         }
         if (fe->file.renn) { free(fe->file.renn); fe->file.renn = 0; }
         if (fe->fpath) free(fe->fpath);
         fe->sign  = 0;
         fe->fpath = 0;
         free(fe);
      } else {
         u32t  ehn = fe->ioh_first;
         // rebuild sbits
         fe->file.sbits = IOFM_SHARE_MASK;
         while (ehn) {
            fe->file.sbits &= ftable[ehn]->file.omode;
            ehn = ftable[ehn]->next;
         }
      }
      free(fh);

      mt_swunlock();
   }
   return err;
}

qserr _std io_size(io_handle ifh, u64t *size) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!size) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (err) return 0;
   if (!fe->broken) err = fe->file.vol->size(fe->file.fh, size);
      else err = E_SYS_BROKENFILE; 
   mt_swunlock();
   return err;
}

qserr _std io_setsize(io_handle ifh, u64t newsize) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (!err) {
      if (fe->broken) err = E_SYS_BROKENFILE; else
         if ((fh->file.omode&IOFM_WRITE)==0) err = E_SYS_ACCESS; else
            err = fe->file.vol->setsize(fe->file.fh, newsize);
      fh->lasterr = err;
      mt_swunlock();
   }
   return err;
}

qserr _std io_setstate(io_handle ifh, u32t flags, u32t value) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!flags) return E_SYS_INVPARM;
   // invalid combinations
   if ((flags&IOFS_DETACHED) && !value ||
       (flags&IOFS_RENONCLOSE) && value) return E_SYS_INVPARM;
   // just one known flag now!
   if ((flags&~(IOFS_DETACHED|IOFS_DELONCLOSE|IOFS_RENONCLOSE)))
      return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO, &fh, &fe);
   if (err) return E_SYS_INVPARM;
   // some options is for file only
   if ((flags&(IOFS_DELONCLOSE|IOFS_RENONCLOSE)) && fe->type!=IOHT_FILE)
      err = E_SYS_INVHTYPE; else
   // and not broken file!
   if ((flags&(IOFS_DELONCLOSE|IOFS_RENONCLOSE)) && fe->broken)
      err = E_SYS_BROKENFILE; else
   {
      if (flags&IOFS_DETACHED) {
         fh->pid     = 0;
         fh->lasterr = 0;
      } else
      if (flags&IOFS_RENONCLOSE) {
         if (fe->file.renn) {
            free(fe->file.renn);
            fe->file.renn = 0;
         }
      }
      if (flags&IOFS_DELONCLOSE) fe->file.del = value?1:0;
   }
   mt_swunlock();
   return err;
}

qserr _std io_getstate(io_handle ifh, u32t *flags) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!flags) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO, &fh, &fe);
   if (err) return E_SYS_INVPARM;

   *flags = 0;
   if (fh->pid==0) *flags|=IOFS_DETACHED;
   if (fe->broken) *flags|=IOFS_BROKEN;

   if (fe->type==IOHT_FILE) {
      if (fe->file.renn) *flags|=IOFS_RENONCLOSE;
      if (fe->file.del)  *flags|=IOFS_DELONCLOSE;
   }
   mt_swunlock();
   return err;
}

qserr _std io_lasterror(io_handle ifh) {
   io_handle_data *fh;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, 0);
   if (!err) {
      err = fh->lasterr;
      mt_swunlock();
   }
   return err;
}

qserr _std io_duphandle(io_handle src, io_handle *dst, int priv) {
   io_handle_data *fho;
   sft_entry       *fe;
   qserr           err;
   if (!dst) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(src, IOHT_FILE, &fho, &fe);
   if (err) return E_SYS_INVPARM;

   if (fe->broken) err = E_SYS_BROKENFILE; else {
      u32t  ifno = ioh_alloc();

      if (!ifno) err = E_SYS_NOMEM; else {
         io_handle_data *fh = (io_handle_data*)malloc(sizeof(io_handle_data));
         fh->sign       = IOHd_SIGN;
         fh->file.pos   = fho->file.pos;
         fh->file.omode = fho->file.omode;
         fh->pid        = fho->pid;
         if (priv && !fh->pid) fh->pid = mod_getpid();
         fh->sft_no     = fho->sft_no;
         fh->lasterr    = 0;
         // link it
         fh->next       = fe->ioh_first;
         fe->ioh_first  = ifno;
         ftable[ifno]   = fh;
         
         *dst = IOH_HBIT+ifno;
      }
   }
   mt_swunlock();
   return err;
}

qserr _std io_info(io_handle ifh, io_direntry_info *info) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!info) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DIR, &fh, &fe);
   if (err) return err;
   memset(info, 0, sizeof(io_direntry_info));

   if (fe->broken) err = E_SYS_BROKENFILE; else {
      io_handle_info hi;
      qs_sysvolume  vol = fe->type==IOHT_FILE?fe->file.vol:fe->dir.vol;

      err = vol->pathinfo(fe->fpath, &hi);
      if (err==0) {
         strcpy(info->name, fe->fpath);
         info->attrs  = hi.attrs;
         info->ctime  = hi.ctime;
         info->atime  = hi.atime;
         info->wtime  = hi.wtime;
         info->fileno = fe->pub_fno;
         info->size   = hi.size;
      }
   }
   if (err) fh->lasterr = err;
   mt_swunlock();
   return err;
}

u32t _std io_filetype(io_handle ifh) {
   sft_entry      *fe;
   qserr          err;
   u32t           res;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO, 0, &fe);
   if (err) return IOFT_BADHANDLE;

   switch (fe->type) {
      case IOHT_FILE : res = IOFT_FILE; break;
      case IOHT_STDIO: res = IOFT_CHAR; break;
      default:
         res = IOFT_UNKNOWN;
   }
   mt_swunlock();
   return res;
}

/** check path and return pointers to full path, volume and sft entry (if exist).
    @attention if no error returned, then MT lock IS ON!
    @param [in]  path          source path
    @param [in]  typemask      handle type to accept (IOHT_*)
    @param [out] pvol          pointer to qs_sysvolume, can be 0
    @param [out] pfp           pointer to full path, can be 0, must be free()-ed
    @param [out] pfe           pointer to sft_entry, can be 0
    @param [out] pfe_index     pointer index of sft_entry, can be 0
    @return error code */
static qserr io_check_path(const char *path, u32t typemask, qs_sysvolume *pvol,
                           char **pfp, sft_entry **pfe, u32t *pfe_index)
{
   qs_sysvolume vol;
   qserr        res;
   char         *fp = io_fullpath_int(0, path, 0, &res);
   if (pvol) *pvol = 0;
   if (pfp)  *pfp = 0;
   if (pfe)  *pfe = 0;
   if (pfe_index) pfe_index = 0;
   if (!fp) return res;
   // drop finishing \ if dir accepted here
   if (typemask&IOHT_DIR) {
      u32t len = strlen(fp);
      if (len>3 && fp[len-1]=='\\') fp[len-1] = 0;
   }

   mt_swlock();
   res = vol_from_fp(fp, &vol);
   if (!res) {
      u32t fno = sft_find(fp);

      if (fno && (sftable[fno]->type&typemask)==0) res = E_SYS_INVNAME; else {
         if (pvol) *pvol = vol;
         if (pfe) *pfe = fno?sftable[fno]:0;
         if (pfp) *pfp = fp; else free(fp);
         if (pfe_index && fno) *pfe_index = fno;
      }
   }
   if (res) {
      mt_swunlock();
      free(fp);
   }
   return res;
}

qserr _std io_setexattr(const char *path, const char *aname, void *data, u32t size) {
   return E_SYS_UNSUPPORTED;
}

qserr _std io_getexattr(const char *path, const char *aname, void *buffer, u32t *size) {
   return E_SYS_UNSUPPORTED;
}

str_list* _std io_lstexattr(const char *path) {
   return 0;
}

qserr _std io_remove(const char *path) {
   qs_sysvolume vol;
   qserr        res;
   char         *fp;
   sft_entry    *fe;
   if (!path) return E_SYS_ZEROPTR;
   res = io_check_path(path, IOHT_FILE, &vol, &fp, &fe, 0);
   if (res) return res;

   if (fe) {
      // file in use. can we delete it on close?
      res = sft_check_mode(fe, IOFM_DEL, 0);
      if (!res) { fe->file.del = 1; res = E_SYS_DELAYED; }
   } else {
      res = vol->remove(fp);
   }
   free(fp);

   mt_swunlock();
   return res;
}

qserr _std io_pathinfo(const char *path, io_handle_info *info) {
   qs_sysvolume vol;
   qserr        res;
   char         *fp;
   sft_entry    *fe;

   if (!path || !info) return E_SYS_ZEROPTR;
   res = io_check_path(path, IOHT_FILE|IOHT_DIR, &vol, &fp, &fe, 0);
   if (res) return res;

   if (strlen(fp)==3 && fp[2]=='\\') {
      memset(info, 0, sizeof(info));
      info->attrs  = IOFA_DIR;
   } else
      res = vol->pathinfo(fp, info);
   if (!res) info->fileno = fe?fe->pub_fno:-1;
   free(fp);

   mt_swunlock();
   return res;
}

qserr _std io_setinfo(const char *path, io_handle_info *info, u32t flags) {
   if (!path || !info) return E_SYS_ZEROPTR;
   if (!flags) return E_SYS_INVPARM;

   if ((flags&IOSI_CTIME) && !io_checktime(&info->ctime) ||
       (flags&IOSI_ATIME) && !io_checktime(&info->atime) ||
       (flags&IOSI_WTIME) && !io_checktime(&info->wtime)) return E_SYS_INVTIME;

   if ((flags&IOSI_ATTRS) && (info->attrs&~(IOFA_READONLY|IOFA_HIDDEN|IOFA_SYSTEM|IOFA_ARCHIVE)))
      return E_SYS_INVPARM;
   else {
      qs_sysvolume vol;
      qserr        res;
      char         *fp;
      sft_entry    *fe;
      if (!path || !info) return E_SYS_ZEROPTR;
      res = io_check_path(path, IOHT_FILE|IOHT_DIR, &vol, &fp, &fe, 0);
      if (res) return res;

      if (fe && fe->type==IOHT_FILE) res = sft_check_mode(fe, IOFM_REN, 0);
      if (!res) res = vol->setinfo(fp, info, flags);
      free(fp);

      mt_swunlock();
      return res;
   }
}

qserr _std io_mkdir(const char *path) {
   if (!path || !*path) return E_SYS_ZEROPTR; else {
      qs_sysvolume vol;
      qserr        res;
      char         *fp;
      sft_entry    *fe;
      res = io_check_path(path, IOHT_DIR|IOHT_FILE, &vol, &fp, &fe, 0);
      if (res) return res;

      if (fe) res = E_SYS_EXIST; else res = vol->makepath(fp);
      free(fp);

      mt_swunlock();
      return res;
   }
}

qserr _std io_rmdir(const char *path) {
   if (!path || !*path) return E_SYS_ZEROPTR; else {
      qs_sysvolume vol;
      qserr        res;
      char         *fp;
      sft_entry    *fe;
      res = io_check_path(path, IOHT_DIR, &vol, &fp, &fe, 0);
      if (res) return res;

      if (fe) res = E_SYS_ACCESS; else res = vol->rmdir(fp);
      free(fp);

      mt_swunlock();
      return res;
   }
}

qserr _std io_diropen(const char *path, dir_handle *pdh) {
   qs_sysvolume    vol;
   qserr           res;
   char            *fp;
   sft_entry       *fe;
   u32t            fno;
   int           newfp = 0;
   dir_handle_int  idh = 0;
   if (!path || !pdh) return E_SYS_ZEROPTR;
   res = io_check_path(path, IOHT_DIR, &vol, &fp, &fe, &fno);
   if (res) return res;

   res = vol->diropen(fp, &idh);
   if (!res) {
      u32t  ifno = ioh_alloc();
      if (!ifno) res = E_SYS_NOMEM; else
      if (!fe) {
         fno = sft_alloc();
         if (!fno) res = E_SYS_NOMEM; else {
            fe = (sft_entry*)malloc(sizeof(sft_entry));
            fe->sign       = SFTe_SIGN;
            fe->type       = IOHT_DIR;
            fe->fpath      = fp;
            fe->pub_fno    = SFT_PUBFNO+fno;
            fe->dir.vol    = vol;
            fe->dir.renn   = 0;
            fe->ioh_first  = 0;
            fe->broken     = 0;
            // add new entries
            sftable[fno]   = fe;
            newfp = 1;
            __set_shared_block_info(fe, "sft data", fno-1);
            __set_shared_block_info(fp, "sft name", fno-1);
         }
      }
      if (!res) {
         io_handle_data *fh = (io_handle_data*)malloc(sizeof(io_handle_data));
         u32t           pid = mod_getpid();
         fh->sign       = IOHd_SIGN;
         fh->dir.dh     = idh;
         fh->pid        = pid;
         fh->sft_no     = fno;
         fh->lasterr    = 0;
         // link it
         fh->next       = fe->ioh_first;
         fe->ioh_first  = ifno;
         ftable[ifno]   = fh;

         if (!res) *pdh = IOH_HBIT+ifno;
      }
      if (res) vol->dirclose(idh);
   }
   mt_swunlock();
   // full path was not used in sft?
   if (!newfp) free(fp);
   return res;
}

qserr _std io_dirnext(dir_handle dh, io_direntry_info *de) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!de) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(dh, IOHT_DIR, &fh, &fe);
   if (err) return E_SYS_INVPARM;

   if (!fe->broken) err = fe->dir.vol->dirnext(fh->dir.dh, de);
      else err = E_SYS_BROKENFILE;

   mt_swunlock();
   return err;
}

qserr _std io_dirclose(dir_handle dh) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(dh, IOHT_DIR, &fh, &fe);
   if (err) return E_SYS_INVPARM;

   if (!fe->broken) err = fe->dir.vol->dirclose(fh->dir.dh);
   dh ^= IOH_HBIT;
   // unlink it from sft entry & ioh array
   ioh_unlink(fe, dh);
   fh->sign = 0;
   // we reach real close here!
   if (fe->ioh_first==0) {
      sftable[fh->sft_no] = 0;

      if (!fe->broken && !err && fe->dir.renn)
         err = fe->dir.vol->renmove(fe->fpath, fe->dir.renn);
      if (fe->dir.renn) { free(fe->dir.renn); fe->dir.renn = 0; }
      if (fe->fpath) free(fe->fpath);
      fe->sign  = 0;
      fe->fpath = 0;
      free(fe);
   }
   free(fh);

   mt_swunlock();
   return err;
}


qserr _std io_lock(io_handle fh, u64t start, u64t len) {
   return E_SYS_UNSUPPORTED;
}

qserr _std io_unlock(io_handle fh, u64t start, u64t len) {
   return E_SYS_UNSUPPORTED;
}

qserr _std io_move(const char *src, const char *dst) {
   qs_sysvolume vol_s, vol_d;
   qserr        res;
   char         *fp_s, *fp_d;
   sft_entry    *fe_s, *fe_d;
   if (!src || !dst) return E_SYS_ZEROPTR;
   res = io_check_path(src, IOHT_FILE|IOHT_DIR, &vol_s, &fp_s, &fe_s, 0);
   if (res) return res;
   res = io_check_path(dst, IOHT_FILE|IOHT_DIR, &vol_d, &fp_d, &fe_d, 0);
   // dec second mp lock
   mt_swunlock();
   if (res) { free(fp_s); return res; }

   if (vol_s!=vol_d) res = E_SYS_DISKMISMATCH; else
   if (fe_d) res = E_SYS_ACCESS; else
   if (fe_s) {
      // file in use. can we rename it after close?
      if (fe_s->type==IOHT_FILE) {
         res = sft_check_mode(fe_s, IOFM_REN, 0);
         if (!res) {
            if (fe_s->file.renn) free(fe_s->file.renn);
            fe_s->file.renn = fp_d;
            res = E_SYS_DELAYED;
         }
      } else
      if (fe_s->type==IOHT_DIR) {
         if (fe_s->dir.renn) free(fe_s->dir.renn);
         fe_s->dir.renn = fp_d;
         res = E_SYS_DELAYED;
      } else
         res = E_SYS_INVHTYPE;
   } else {
      res = vol_s->renmove(fp_s, fp_d);
      free(fp_d);
   }
   free(fp_s);

   mt_swunlock();
   return res;
}

/** process open files and directories on removing volume.

    Function close all internal qs_sysvolume handles and mark SFT entries
    as "broken", but leave all user handles in usable mode (i.e. user should
    close it in common way).

    Must be used inside MT lock only! */
u32t sft_volumebroke(u8t vol, int enumonly) {
   u32t    ii, ccnt = 0;
   qs_sysvolume  vl;
   if (vol>=DISK_COUNT || !ve[vol]) return 0;
   vl = ve[vol];
   // skip entry 0
   for (ii=1; ii<sftable_alloc; ii++)
      if (sftable[ii]) {
         sft_entry *fe = sftable[ii];
         if (fe->sign!=SFTe_SIGN) sft_logerr(ii); else {
            u32t sel = 0;
            /* close all "qs_sysvolume" handles for files and directories
               and mark SFT entries as broken */
            if (fe->type==IOHT_DIR && fe->dir.vol==vl) {
               if (!enumonly) {
                  u32t idx = fe->ioh_first;
                  while (idx) {
                     io_handle_data *hd = ftable[idx];
                     if (hd->sign!=IOHd_SIGN) {
                        ioh_logerr(fe, hd, idx); 
                        break;
                     } else
                        fe->dir.vol->dirclose(hd->dir.dh);
                     idx = hd->next;
                  }
                  fe->dir.vol = 0;
               }
               sel = 1;
            } else
            if (fe->type==IOHT_FILE && fe->file.vol==vl) {
               if (!enumonly) {
                  fe->file.vol->close(fe->file.fh);
                  fe->file.vol = 0;
               }
               sel = 1;
            }
            if (sel) { 
               ccnt++;
               if (!enumonly) fe->broken = 1; 
            }
         }
      }
   return ccnt;
}

u32t io_close_as(u32t pid, u32t htmask) {
   u32t ccnt = 0;
   htmask   &= IOHT_FILE|IOHT_DIR;
   if (htmask && pid) {
      dd_list clist = 0;
      u32t       ii,
           pidmatch = pid==mod_getpid();
      mt_swlock();
      // skip entry 0
      for (ii=1; ii<ftable_alloc; ii++)
         if (ftable[ii]) {
            io_handle_data *fh = ftable[ii];
            sft_entry      *fe = sftable[fh->sft_no];

            if (fh->sign!=IOHd_SIGN) ioh_logerr(fe, fh, ii); else 
            if ((fe->type&htmask) && fh->pid==pid) {
               // detach it to close!
               if (!pidmatch) fh->pid = 0;
               // create instance & disable trace for it
               if (!clist) { clist = NEW(dd_list); exi_chainset(clist, 0); }
               clist->add(ii);
            }
         }
      // close collected entires
      if (clist)
         for (ii=0; ii<clist->count(); ii++) {
            u32t         fhidx = clist->value(ii);
            io_handle_data *fh = ftable[fhidx];
            sft_entry      *fe = sftable[fh->sft_no];

            fhidx|=IOH_HBIT;
            if (fe->type==IOHT_DIR) io_dirclose(fhidx); else
            if (fe->type==IOHT_FILE) io_close(fhidx);

         }
      mt_swunlock();

      if (clist) DELETE(clist);
   }
   return ccnt;
}

qserr _std io_vollabel(u8t drive, const char *label) {
   if (drive>DISK_COUNT-1) return E_SYS_INVPARM; else
   if (!is_mounted(drive)) return E_DSK_NOTMOUNTED; else {
      u32t res;
      mt_swlock();
      res = ve[drive]->setlabel(label);
      mt_swunlock();
      return res;
   }
}

qserr _std io_volinfo(u8t drive, disk_volume_data *info) {
   qserr  res = 0;
   if (!info) return E_SYS_ZEROPTR;
   if (drive>DISK_COUNT-1) res = E_SYS_INVPARM; else
   if (!is_mounted(drive)) res = E_DSK_NOTMOUNTED; else {
      mt_swlock();
      res = ve[drive]->volinfo(info);
      fsver[drive] = info->FsVer;
      mt_swunlock();
   }
   if (res) memset(info, 0, sizeof(disk_volume_data));
   return res;
}

u32t _std hlp_volinfo(u8t drive, disk_volume_data *info) {
   if (drive<DISK_COUNT && is_mounted(drive)) {
      disk_volume_data  vi,
                      *pvi = info?info:&vi;
      /* cache volume type to prevent too ofter full info calls */
      if (info || fsver[drive]==0xFF) {
         qserr res = io_volinfo(drive, pvi);
         if (!res) return fsver[drive] = pvi->FsVer; else return 0;
      }
      return fsver[drive];
   }
   if (info) memset(info,0,sizeof(disk_volume_data));
   return FST_NOTMOUNTED;
}

qserr _std io_mount(u8t drive, u32t disk, u64t sector, u64t count) {
   u32t rc = E_DSK_BADVOLNAME;
   // no curcular references allowed!!! ;)
   if (disk==QDSK_VOLUME+(u32t)drive) return E_DSK_DISKNUM;
   // no 2Tb partitions!
   if (count>=_4GBLL) return E_DSK_PT2TB;
   // mount it
   if (drive>DISK_LDR && drive<DISK_COUNT) {
      struct Boot_Record *br = 0;
      u32t            sectsz;
      u64t               tsz = hlp_disksize64(disk, &sectsz);
      vol_data         *vdta = _extvol + drive;
      // check for init, size and sector count overflow
      if (!tsz||sector>=tsz||sector+count>tsz||sector+count<sector) return E_SYS_INVPARM;
      /* lock it!
         bad way, but at least now its guarantee atomic mount */
      mt_swlock();
      // unmount current
      hlp_unmountvol(drive);
      // get buffer
      br = (struct Boot_Record*)malloc(sectsz);
      // read boot sector
      if (br && hlp_diskread(disk|QDSK_DIRECT, sector, 1, br)) {
         qs_sysvolume vol = (qs_sysvolume)exi_createid(fatio_classid);
         if (vol) {
            qserr  eres;
            // disk parameters for fs i/o
            vdta->disk       = disk;
            vdta->start      = sector;
            vdta->length     = count;
            vdta->sectorsize = sectsz;
            vdta->serial     = 0;
            vdta->badclus    = 0;
            vdta->clsize     = 1;
            vdta->clfree     = 0;
            vdta->cltotal    = 0;
            // mark as mounted else disk i/o deny disk access
            vdta->flags      = VDTA_ON;

            eres = vol->init(drive, SFIO_FORCE, br);
            rc   = eres==E_DSK_UNCKFS?0:eres;
            if (!rc) {
               mt_safedor((u32t*)&v_mounted, 1<<drive);
               fsver[drive] = 0xFF;
               ve[drive] = vol;
               cache_ctrl(CC_MOUNT,drive);
            }
            if (eres) log_it(3, "Mount %c - rc %X\n", 'A'+drive, eres);
         }
      }
      // clear state
      if (rc) vdta->flags = 0;
      // unlock it!
      mt_swunlock();
      // sector buffer
      if (br) free(br);
   }
   return rc;
}

u32t _std hlp_mountvol(u8t drive, u32t disk, u64t sector, u64t count) {
   return io_mount(drive, disk, sector, count)?0:1;
}

qserr _std io_unmount(u8t drive, u32t flags) {
   if (drive<=DISK_LDR || drive>=DISK_COUNT) return E_DSK_BADVOLNAME; else {
      u32t   rc = E_DSK_NOTMOUNTED;
      mt_swlock();
      if (_extvol[drive].flags&VDTA_ON) {
         u32t efl = flags&IOUM_FORCE?0:1,
             numc = sft_volumebroke(drive, efl);
         // we have open files on volume
         if (efl && numc) {
            efl = flags&IOUM_NOASK?1:0;
            if (!efl) {
               char *emsg = sprintf_dyn("Volume %c is in use, removing it will "
                  "broke %u open file(s).^Do you want to continue?", drive+'A', numc);
               efl = vio_msgbox("SYSTEM WARNING!", emsg,
                  MSG_LIGHTRED|MSG_DEF2|MSG_YESNO, 0)==MRES_YES?0:1;
               free(emsg);
            }
            if (!efl) sft_volumebroke(drive, 0); else
               return E_SYS_ACCESS;
         }

         // change current disk to 1: (for current process only!)
         if (io_curdisk()==drive) io_setdisk(DISK_LDR);

         mt_safedand((u32t*)&v_mounted, ~(1<<drive));
         fsver[drive] = 0xFF;

         if (ve[drive]) {
            qserr eres = ve[drive]->done();
            if (eres) log_it(3, "Unmount %c - rc %X", 'A'+drive, eres);
            DELETE(ve[drive]);
            ve[drive] = 0;
         }
         cache_ctrl(CC_UMOUNT,drive);
         _extvol[drive].flags = 0;
         rc = 0;
      }
      mt_swunlock();
      return rc;
   }
}

u32t _std hlp_unmountvol(u8t drive) {
   return io_unmount(drive, IOUM_FORCE)?0:1;
}

/** is drive letter mounted to something?
    @param  [in]  drive    drive letter to check
    @param  [out] lavail   last mounted drive letter (can be 0)
    @return boolean flag (1/0) */
u32t _std io_ismounted(u8t drive, u8t *lavail) {
   u32t  res = 0;
   // atomic checks
   if (lavail) *lavail = (u8t)bsr64(v_mounted);
   if (drive<DISK_COUNT) res = is_mounted(drive);
   return res;
}

void  _std io_timetoio(io_time *dst, time_t src) {
   struct tm tme;
   if (!dst) return;
   localtime_r(&src, &tme);
   dst->sec  = tme.tm_sec;
   dst->min  = tme.tm_min;
   dst->hour = tme.tm_hour;
   dst->day  = tme.tm_mday;
   dst->mon  = tme.tm_mon;
   dst->year = tme.tm_year + (1900-1781);
   dst->dsec = 0;
}

time_t _std io_iototime(io_time *src) {
   struct tm tme;
   if (!src) return 1275228311;
   tme.tm_sec  = src->sec;
   tme.tm_min  = src->min;
   tme.tm_hour = src->hour;
   tme.tm_mday = src->day;
   tme.tm_mon  = src->mon;
   tme.tm_year = src->year - (1900-1781);

   return mktime(&tme);
}

int io_checktime(io_time *src) {
   static const s8t is_31[12] = { 1,-1,1,0,1,0,1,1,0,1,0,1 };
   if (!src) return 0;
   if (src->dsec>99 || src->dsec>59 || src->min>59 || src->hour>23 || src->day ||
      src->mon>11) return 0;
   if (src->day>30+is_31[src->mon]) return 0;
    return 1;
}

mt_prcdata *get_procinfo(void) {
   process_context *pq = mod_context();
   return (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
}

void _std io_dumpsft(void) {
   log_it(2,"===== SFT list =====\n");
   if (sftable_alloc) {
      u32t ii = 0;

      log_it(2, "     file name          |   fno   | brk | type  | ptr\n");
      mt_swlock();

      for (ii=0; ii<sftable_alloc; ii++) 
         if (sftable[ii]) {
            static const char *tname[] = { "FILE", "DISK", "STD", "DUMMY", "DIR" };
            sft_entry *fe = sftable[ii];
            int      tpos = bsf32(fe->type);
            if (tpos>4) tpos = -1;

            //ioh_first

            log_it(2, "%3d. %-18s |%8i |  %i  | %-5s | %08X\n", ii, fe->fpath, fe->pub_fno,
               fe->broken, tpos<0?"??":tname[tpos], fe);
         }
      mt_swunlock();
   }
   log_it(2,"====================\n");
}

void setup_fio() {
   u32t      ii;
   memset(&ve, 0, sizeof(ve));
   // FAT i/o class
   register_fatio();

   memset(&fsver, 0xFF, sizeof(fsver));
   v_mounted = 0;
   _extvol   = (vol_data*)sto_data(STOKEY_VOLDATA);

   for (ii=0; ii<DISK_COUNT; ii++) {
      vol_data *vdta = _extvol+ii;
      // for disk 0 we checks VDTA_ON too (this can be PXE or EFI host)
      if (ii==DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
         qs_sysvolume vl = (qs_sysvolume)exi_createid(fatio_classid);

         if (vl) {
            vl->init(ii, SFIO_APPEND|SFIO_FORCE, 0);
            ve[ii] = vl;
            v_mounted|=1<<ii;
         }
      }
   }
   // reserve zero indices in both arrays - this allows using 0 as error code
   ii = sft_alloc();
   if (ii) log_it(0, "SFT init error!\n"); else {
      sftable[0] = (sft_entry*)calloc_shared(sizeof(sft_entry),1);
      sftable[0]->sign = SFTe_SIGN;
      sftable[0]->type = IOHT_DUMMY;
   }
   ii = ioh_alloc();
   if (ii) log_it(0, "IOH init error!\n"); else {
      ftable[0] = (io_handle_data*)calloc_shared(sizeof(io_handle_data),1);
      ftable[0]->sign  = IOHd_SIGN;
      ftable[0]->next  = 0;
   }
}
