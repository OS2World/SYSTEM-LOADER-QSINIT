//
// QSINIT "start" module
// system i/o and handle processing
// -------------------------------------
//

#include "qsutil.h"
#include "qsint.h"
#include "qcl/sys/qsvolapi.h"
#include "qcl/qslist.h"
#include "syslocal.h"
#include "qsstor.h"
#include "qserr.h"
#include "sysio.h"

// additional attrs for sft_check_mode()
#define  IOFM_DEL       IOFM_CLOSE_DEL
#define  IOFM_REN       0x80
// alloc increment value
#define  SFT_INC        20
#define  FS_COUNT       16
static qs_sysvolume ve[DISK_COUNT];
static u8t       fsver[DISK_COUNT];     ///< FsType cache for hlp_volinfo()
static u32t   fs_classid[FS_COUNT];     ///< registered filesystems
static u32t     fs_avail[FS_COUNT];     ///< avail() function result

static volatile u32t     v_mounted = 0; ///< mounted volumes (for fast check)
sft_entry* volatile       *sftable = 0;
io_handle_data* volatile   *ftable = 0;
static volatile u32t sftable_alloc = 0,
                      ftable_alloc = 0;
vol_data                  *_extvol = 0; ///< const pointer from QSINIT binary
u32t                 ramfs_classid = 0, ///< ram fs class for volume b:
                     nulfs_classid = 0; ///< null fs class (fs stub)

#define is_mounted(vol) (v_mounted&1<<(u8t)vol?1:0)

void register_fatio(void);
void register_ramfs(void);
void register_nulfs(void);

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


void sft_setblockowner(sft_entry *fe, u32t fno) {
   __set_shared_block_info(fe, "sft data", fno-1);
   if (fe->fpath)
      __set_shared_block_info(fe->fpath, "sft name", fno-1);
}

/** search in SFT by full path.
    Must be called in locked state (sft access) */
u32t sft_find(const char *fullpath, int name_space) {
   if (fullpath && *fullpath) {
      u32t ii;
      // skip entry 0
      for (ii=1; ii<sftable_alloc; ii++)
         if (sftable[ii])
            if (sftable[ii]->sign!=SFTe_SIGN) sft_logerr(ii); else {
               char *fp = sftable[ii]->fpath;
               /* stricmp takes in mind current codepage, so let it be in this
                  way for a while */
               if (fp && sftable[ii]->name_space==name_space)
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
      // internal handle is r/o? deny write op
      if ((mode&IOFM_WRITE) && fe->file.ro) return E_SYS_ACCESS;
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
u32t sft_alloc(void) {
   return common_alloc((void**)&sftable, &sftable_alloc, sizeof(sft_entry*));
}

/** alloc new io_handle_data entry.
    Must be called in locked state (io_handle array access). */
u32t ioh_alloc(void) {
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
                  /* always do realloc here, because original piCurDir buffer
                     has space only for path in it */
                  char *prev = pd->piCurDir[volnum];
                  pd->piCurDir[volnum] = fp;
                  pd->piCurDrive = volnum;
                  fp = 0;
                  if (prev) free(prev);

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

/// common local handle creation part, must be called inside lock
io_handle_data *ioh_setuphandle(u32t ifno, u32t fno, u32t pid) {
   io_handle_data *fh = (io_handle_data*)malloc(sizeof(io_handle_data));
   sft_entry      *fe = sftable[fno];

   fh->sign       = IOHd_SIGN;
   fh->pid        = pid;
   fh->sft_no     = fno;
   fh->lasterr    = 0;
   // link it
   fh->next       = fe->ioh_first;
   fe->ioh_first  = ifno;
   ftable[ifno]   = fh;
   return fh;
}

qserr _std START_EXPORT(io_open)(const char *name, u32t mode, io_handle *pfh, u32t *action) {
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
   if ((mode&~(IOFM_OPEN_MASK|IOFM_READ|IOFM_WRITE|IOFM_SHARE_MASK|
      IOFM_CLOSE_DEL|IOFM_SECTOR_IO))) return E_SYS_INVPARM;

   if (!action) action = &tmpact;

   *action = IOFN_EXISTED;
   *pfh    = 0;
   fp      = io_fullpath_int(0, name, 0, &rc);
   if (!fp) return rc;

   mt_swlock();
   rc = vol_from_fp(fp, &vol);
   if (!rc) {
      io_handle_int   fhi = 0;
      u32t            fno = sft_find(fp, NAMESPC_FILEIO);
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
         disk_volume_data vi;
         int         ro_open = 0;
         rc = vol->open(fp, openm, &fhi, action);
         // log_it(2,"vol->open(%s) == %08X\n", fp, rc);

         // ugly way to inform about r/o file/disk
         if (rc==E_SYS_READONLY) {
            if ((mode&IOFM_WRITE)) {
               vol->close(fhi);
               rc = E_SYS_ACCESS;
            } else {
               ro_open = 1;
               rc = 0;
            }
         }
         if (rc==0) rc = vol->volinfo(&vi,1);
         // new file entries in sft & ioh
         if (rc==0) {
            fno = sft_alloc();
            if (!fno) rc = E_SYS_NOMEM;
         }
         if (rc==0) {

            fe = (sft_entry*)malloc(sizeof(sft_entry));
            fe->sign        = SFTe_SIGN;
            fe->type        = IOHT_FILE;
            fe->fpath       = fp;
            fe->name_space  = NAMESPC_FILEIO;
            fe->pub_fno     = SFT_PUBFNO+fno;
            fe->file.vol    = vol;
            fe->file.fh     = fhi;
            fe->file.sbits  = mode&IOFM_SHARE_MASK;
            fe->file.del    = 0;
            fe->file.renn   = 0;
            fe->file.ro     = ro_open;
            fe->file.bshift = bsr32(vi.SectorSize);
            fe->ioh_first   = 0;
            fe->broken      = 0;
            // add new entries
            sftable[fno]    = fe;
            newfp = 1;
            sft_setblockowner(fe, fno);
         }
      }
      if (!rc) {
         io_handle_data *fh = ioh_setuphandle(ifno, fno, pid);
         fh->file.pos   = 0;
         fh->file.omode = mode;
         fe->file.del  |= mode&IOFM_CLOSE_DEL?1:0;

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
    @param [in]  ifh           handle to check
    @param [in]  accept_types  IOHT_* combination (allowed handle types)
    @param [out] pfh           pointer to io_handle_data, can be 0
    @param [out] pfe           pointer to sft_entry, can be 0
    @return error code */
qserr io_check_handle(io_handle ifh, u32t accept_types, io_handle_data **pfh,
                      sft_entry **pfe)
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

/** special version of io_check_handle(), should not be used by anyone!
    Called from thread switching code (timer interrupt).

    Since thread switching occurs only when MT lock is off - it is safe
    to access file table arrays here.

    Handle owning is ignored! */
qserr io_check_handle_spec(io_handle ifh, u32t accept_types, io_handle_data **pfh,
                           sft_entry **pfe)
{
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          res;
   if ((ifh^=IOH_HBIT)>=ftable_alloc) return E_SYS_INVPARM;

   fh = ftable[ifh];

   if (!fh || fh->sign!=IOHd_SIGN) res = E_SYS_INVOBJECT; else
   if (fh->sft_no>=sftable_alloc) res = E_SYS_INVOBJECT; else {
      fe = sftable[fh->sft_no];
      if (!fe || fe->sign!=SFTe_SIGN) res = E_SYS_INVOBJECT; else
      if ((fe->type&accept_types)==0) res = E_SYS_INVHTYPE; else {
         if (pfh) *pfh = fh;
         if (pfe) *pfe = fe;
         return 0;
      }
   }
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
   if (fh->file.omode&IOFM_SECTOR_IO) size<<=fe->file.bshift;

   if (fe->broken) {
      err  = E_SYS_BROKENFILE;
      size = 0;
   } else
   if (read && (fh->file.omode&IOFM_READ)==0 ||
      !read && (fh->file.omode&IOFM_WRITE)==0)
   {
      err  = E_SYS_ACCESS;
      size = 0;
   } else {
      err  = (read?fe->file.vol->read:fe->file.vol->write)
             (fe->file.fh, fh->file.pos, buffer, &size);
   }
   // log_it(2,"vol->iocommon() == %u, err %08X\n", size, err);
   fh->file.pos+= size;
   fh->lasterr  = err;

   if (fh->file.omode&IOFM_SECTOR_IO) size>>=fe->file.bshift;

   mt_swunlock();
   return size;
}

u32t _std START_EXPORT(io_read)(io_handle fh, const void *buffer, u32t size) {
   return io_common(fh, (void*)buffer, size, 1);
}

u32t _std START_EXPORT(io_write)(io_handle fh, const void *buffer, u32t size) {
   return io_common(fh, (void*)buffer, size, 0);
}

u64t _std START_EXPORT(io_seek)(io_handle ifh, s64t offset, u32t origin) {
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
      if (fh->file.omode&IOFM_SECTOR_IO) offset<<=fe->file.bshift;
      pos += offset;
      if (pos<0) { fh->lasterr = E_SYS_SEEKERR; pos = -1; } else
      if (pos>size) {
         if ((fh->file.omode&IOFM_WRITE)==0) {
            fh->lasterr = E_SYS_ACCESS; pos = -1;
         } else {
            err = fe->file.vol->setsize(fe->file.fh, pos);
            fh->lasterr = err;
            if (!err) fh->file.pos = pos; else pos = -1;
         }
      } else {
         fh->lasterr  = 0;
         fh->file.pos = pos;
      }
      if ((fh->file.omode&IOFM_SECTOR_IO) && pos!=FFFF64)
         pos>>=fe->file.bshift;
   }
   mt_swunlock();
   return pos;
}

u64t _std io_pos(io_handle ifh) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   u64t           res;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, &fh, &fe);
   if (err) return FFFF64;
   res = fh->file.pos;
   if (fh->file.omode&IOFM_SECTOR_IO) res>>=fe->file.bshift;
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

u32t _std io_blocksize(io_handle ifh) {
   sft_entry      *fe;
   qserr          err;
   u32t           res = 0;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE, 0, &fe);
   if (!err) {
      if (!fe->broken) res = 1<<fe->file.bshift;
      mt_swunlock();
   }
   return res;
}

/// unlink ioh from sft and zero ftable entry
void ioh_unlink(sft_entry *fe, u32t ioh_index) {
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

/// is sft has only one object?
int ioh_singleobj(sft_entry *fe) {
   io_handle_data *fh = ftable[fe->ioh_first];
   return fh->next?0:1;
}

qserr _std START_EXPORT(io_close)(io_handle ifh) {
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
   if (!fe->broken) {
      err = fe->file.vol->size(fe->file.fh, size);
      // shift size if sector i/o is on
      if (!err && (fh->file.omode&IOFM_SECTOR_IO))
        *size>>=fe->file.bshift;
   } else err = E_SYS_BROKENFILE;
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
         if ((fh->file.omode&IOFM_WRITE)==0) err = E_SYS_ACCESS; else {
            if (fh->file.omode&IOFM_SECTOR_IO) newsize<<=fe->file.bshift;
            err = fe->file.vol->setsize(fe->file.fh, newsize);
         }
      fh->lasterr = err;
      mt_swunlock();
   }
   return err;
}

qserr _std io_setstate(qshandle ifh, u32t flags, u32t value) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!flags) return E_SYS_INVPARM;
   // invalid combinations
   if ((flags&IOFS_DETACHED) && !value ||
       (flags&IOFS_RENONCLOSE) && value) return E_SYS_INVPARM;
   // just one known flag now!
   if ((flags&~(IOFS_DETACHED|IOFS_DELONCLOSE|IOFS_RENONCLOSE|IOFS_SECTORIO)))
      return E_SYS_INVPARM;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO|IOHT_MUTEX|
                         IOHT_QUEUE|IOHT_EVENT, &fh, &fe);
   if (err) return E_SYS_INVPARM;
   // some options is for file only
   if ((flags&(IOFS_DELONCLOSE|IOFS_RENONCLOSE|IOFS_SECTORIO)) && fe->type!=IOHT_FILE)
      err = E_SYS_INVHTYPE; else
   // and not broken file!
   if ((flags&(IOFS_DELONCLOSE|IOFS_RENONCLOSE|IOFS_SECTORIO)) && fe->broken)
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
      if (flags&IOFS_SECTORIO)
         if (value) fh->file.omode|=IOFM_SECTOR_IO; else
            fh->file.omode&=~IOFM_SECTOR_IO;
   }
   mt_swunlock();
   return err;
}

qserr _std io_getstate(qshandle ifh, u32t *flags) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!flags) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO|IOHT_MUTEX|
                         IOHT_QUEUE|IOHT_EVENT, &fh, &fe);
   if (err) return E_SYS_INVPARM;

   *flags = 0;
   if (fh->pid==0) *flags|=IOFS_DETACHED;
   if (fe->broken) *flags|=IOFS_BROKEN;

   if (fe->type==IOHT_FILE) {
      if (fe->file.renn) *flags|=IOFS_RENONCLOSE;
      if (fe->file.del)  *flags|=IOFS_DELONCLOSE;
      if (fh->file.omode&IOFM_SECTOR_IO) *flags|=IOFS_SECTORIO;
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

qserr _std io_duphandle(qshandle src, qshandle *dst, int priv) {
   io_handle_data *fho;
   sft_entry       *fe;
   qserr           err;
   if (!dst) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(src, IOHT_FILE|IOHT_MUTEX|IOHT_EVENT|IOHT_QUEUE, &fho, &fe);
   if (err) return E_SYS_INVPARM;

   if (fe->broken) err = E_SYS_BROKENFILE; else {
      u32t  ifno = ioh_alloc();

      if (!ifno) err = E_SYS_NOMEM; else {
         io_handle_data *fh;
         u32t           pid = fho->pid;

         if (priv && !pid) pid = mod_getpid();
         fh = ioh_setuphandle(ifno, fho->sft_no, pid);

         if (fe->type==IOHT_FILE) {
            fh->file.pos   = fho->file.pos;
            fh->file.omode = fho->file.omode;
         }
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
      // use finfo() function if it available
      if (fe->type==IOHT_FILE && (vol->avail()&SFAF_FINFO))
         err = vol->finfo(fe->file.fh, info);
      else
         err = E_SYS_UNSUPPORTED;

      if (err==E_SYS_UNSUPPORTED) {
         err = vol->pathinfo(fe->fpath, &hi);
         if (err==0) {
            strcpy(info->name, fe->fpath);
            info->attrs  = hi.attrs;
            info->ctime  = hi.ctime;
            info->atime  = hi.atime;
            info->wtime  = hi.wtime;
            info->size   = hi.size;
            info->vsize  = hi.size;
         }
      }
      if (!err) {
         info->fileno = fe->pub_fno;
         if (fh->file.omode&IOFM_SECTOR_IO) {
            info->size >>= fe->file.bshift;
            info->vsize>>= fe->file.bshift;
         }
      }
   }
   if (err) fh->lasterr = err;
   mt_swunlock();
   return err;
}

u32t _std io_handletype(qshandle ifh) {
   sft_entry      *fe;
   qserr          err;
   u32t           res;
   // this call LOCKs us if err==0
   err = io_check_handle(ifh, IOHT_FILE|IOHT_DISK|IOHT_STDIO|IOHT_DIR|
                         IOHT_MUTEX|IOHT_QUEUE|IOHT_EVENT, 0, &fe);
   if (err) return IOFT_BADHANDLE;

   switch (fe->type) {
      case IOHT_FILE : res = IOFT_FILE;  break;
      case IOHT_STDIO: res = IOFT_CHAR;  break;
      case IOHT_DIR  : res = IOFT_DIR;   break;
      case IOHT_MUTEX: res = IOFT_MUTEX; break;
      case IOHT_QUEUE: res = IOFT_QUEUE; break;
      case IOHT_EVENT: res = IOFT_EVENT; break;
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
      u32t fno = sft_find(fp, NAMESPC_FILEIO);

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
   if (!res) {
      info->fileno = fe?fe->pub_fno:-1;
      info->vol    = vol->drive();
   }
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

qserr _std START_EXPORT(io_diropen)(const char *path, dir_handle *pdh) {
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
            fe->name_space = NAMESPC_FILEIO;
            fe->pub_fno    = SFT_PUBFNO+fno;
            fe->dir.vol    = vol;
            fe->dir.renn   = 0;
            fe->ioh_first  = 0;
            fe->broken     = 0;
            // add new entries
            sftable[fno]   = fe;
            newfp = 1;
            sft_setblockowner(fe, fno);
         }
      }
      if (!res) {
         u32t           pid = mod_getpid();
         io_handle_data *fh = ioh_setuphandle(ifno, fno, pid);
         fh->dir.dh     = idh;
         if (!res) *pdh = IOH_HBIT+ifno;
      }
      if (res) vol->dirclose(idh);
   }
   mt_swunlock();
   // full path was not used in sft?
   if (!newfp) free(fp);
   return res;
}

qserr _std START_EXPORT(io_dirnext)(dir_handle dh, io_direntry_info *de) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   if (!de) return E_SYS_ZEROPTR;
   // this call LOCKs us if err==0
   err = io_check_handle(dh, IOHT_DIR, &fh, &fe);
   if (err) return err;

   if (!fe->broken) err = fe->dir.vol->dirnext(fh->dir.dh, de);
      else err = E_SYS_BROKENFILE;

   mt_swunlock();
   return err;
}

qserr _std START_EXPORT(io_dirclose)(dir_handle dh) {
   io_handle_data *fh;
   sft_entry      *fe;
   qserr          err;
   // this call LOCKs us if err==0
   err = io_check_handle(dh, IOHT_DIR, &fh, &fe);
   if (err) return err;

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

    Function closes all internal qs_sysvolume handles and mark SFT entries
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
   htmask   &= IOHT_FILE|IOHT_DIR|IOHT_MUTEX|IOHT_QUEUE|IOHT_EVENT;
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
            switch (fe->type) {
               case IOHT_DIR  : io_dirclose(fhidx);
                                break;
               case IOHT_FILE : io_close(fhidx);
                                break;
               case IOHT_EVENT:
               case IOHT_MUTEX: mt_closehandle_int(fhidx,1);
                                break;
               case IOHT_QUEUE: qe_close(fhidx);
                                break;
            }
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

qserr _std START_EXPORT(io_volinfo)(u8t drive, disk_volume_data *info) {
   qserr  res = 0;
   if (!info) return E_SYS_ZEROPTR;
   if (drive>DISK_COUNT-1) res = E_SYS_INVPARM; else
   if (!is_mounted(drive)) res = E_DSK_NOTMOUNTED; else {
      mt_swlock();
      res = ve[drive]->volinfo(info,0);
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

#define FINDFS_NOFORCE    1   /// skip NUL driver mounting
#define FINDFS_NOLIB      2   /// skip FS library loading

/* select FS handler for the volume.
   @param drive       Volume to check
   @param flags       FINDFS_* flags
   @param [out] pvol  Inited instance or 0
   @return error code or 0 */
static qserr io_findfs(u8t drive, u32t flags, qs_sysvolume *pvol) {
   qs_sysvolume vol = 0;
   qserr        res = 0;
   // invalid call
   if (!pvol) return E_SYS_SOFTFAULT;
   *pvol = 0;
   if (drive>=DISK_COUNT || drive==DISK_LDR) res = E_DSK_BADVOLNAME; else {
      struct Boot_Record *br = 0;
      vol_data         *vdta = _extvol + drive;
      // lock MT to prevent any possible mount/unmount activity
      mt_swlock();
      if ((vdta->flags&VDTA_ON)==0) res = E_DSK_NOTMOUNTED; else
      while (1) {
         u32t ii;
         br = (struct Boot_Record*) malloc(vdta->sectorsize);
         if (!br) { res = E_SYS_NOMEM; break; }
         if (!hlp_diskread(drive|QDSK_VOLUME, 0, 1, br)) {
            res = E_DSK_ERRREAD;
            break;
         } else
         if ((flags&FINDFS_NOLIB)==0) {
            // try to load even more FS handlers ;)
            fs_detect_list *dl = ecmd_readfsdetect(), *dp;
            if (dl) {
               for (dp=dl; dp->cmpdata; dp++)
                  if (dp->offset + dp->size <= vdta->sectorsize)
                     if (!memcmp((u8t*)br+dp->offset, dp->cmpdata, dp->size))
                        ecmd_loadfslib(dp->fsname);
               free(dl);
            }
         }
         // loop over existing FS handlers
         for (ii=0; ii<FS_COUNT + (flags&FINDFS_NOFORCE?0:1); ii++) {
            int force = ii==FS_COUNT;
            u32t  cid = force ? nulfs_classid : fs_classid[ii];
            // virtual fs cannot be mounted to a volume
            if (!force && cid && (fs_avail[ii]&SFAF_VFS)) cid = 0;

            if (cid) {
               vol = (qs_sysvolume)exi_createid(cid, EXIF_SHARED);
               if (vol) {
                  res = vol->init(drive, 0, br);
                  if (force && res==E_DSK_UNKFS) res = 0;
                  if (!res) break; else { DELETE(vol); vol = 0; }
               }
            }
         }
         break;
      }
      mt_swunlock();
      if (br) free(br);
   }
   *pvol = vol;
   return res;
}

qserr _std io_mount(u8t drive, u32t disk, u64t sector, u64t count) {
   u32t rc = E_DSK_BADVOLNAME;
   // no curcular references allowed!!! ;)
   if (disk==QDSK_VOLUME+(u32t)drive) return E_DSK_DISKNUM;
   // no 2Tb partitions!
   if (count>=_4GBLL) return E_DSK_PT2TB;
   // mount it
   if (drive>DISK_LDR && drive<DISK_COUNT) {
      u32t      sectsz;
      u64t         tsz = hlp_disksize64(disk, &sectsz);
      vol_data   *vdta = _extvol + drive;
      qs_sysvolume vol;
      // check for init, size and sector count overflow
      if (!tsz||sector>=tsz||sector+count>tsz||sector+count<sector)
         return E_SYS_INVPARM;
      /* lock it!
         bad way, but at least now it guarantees atomic mount */
      mt_swlock();
      // unmount current
      hlp_unmountvol(drive);
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

      rc = io_findfs(drive, 0, &vol);

      if (!rc) {
         mt_safedor((u32t*)&v_mounted, 1<<drive);
         fsver[drive] = 0xFF;
         ve[drive] = vol;
         hlp_cachenotify(drive, CC_MOUNT);
      } else
         log_it(3, "Mount %c - rc %X\n", 'A'+drive, rc);
      // clear state
      if (rc) vdta->flags = 0;
      // unlock it!
      mt_swunlock();
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
         rc = 0;
         // we have open files on volume
         if (efl && numc) {
            efl = flags&IOUM_NOASK?1:0;
            if (!efl) {
               char *emsg = sprintf_dyn("Volume %c is in use, removing it will "
                  "broke %u open file(s).^Do you want to continue?", drive+'A', numc);
               /* vio_msgbox() will unlock it any way, because of keyboard waiting.
                  but we should check if volume still mounted after popup */
               mt_swunlock();
               efl = vio_msgbox("SYSTEM WARNING!", emsg,
                  MSG_POPUP|MSG_LIGHTRED|MSG_DEF2|MSG_YESNO, 0)==MRES_YES?0:1;
               free(emsg);
               mt_swlock();
               if ((_extvol[drive].flags&VDTA_ON)==0) rc = E_DSK_NOTMOUNTED;
            }
            if (!efl) sft_volumebroke(drive, 0); else rc = E_SYS_ACCESS;
         }
         if (rc==0) {
            // change current disk to 1: (for current process only!)
            if (io_curdisk()==drive) io_setdisk(DISK_LDR);

            mt_safedand((u32t*)&v_mounted, ~(1<<drive));
            fsver[drive] = 0xFF;

            if (ve[drive]) {
               qserr eres = ve[drive]->done();
               if (eres) log_it(3, "Unmount %c - rc %X\n", 'A'+drive, eres);
               DELETE(ve[drive]);
               ve[drive] = 0;
            }
            hlp_cachenotify(drive, CC_UMOUNT);
            _extvol[drive].flags = 0;
         }
      }
      mt_swunlock();
      return rc;
   }
}

u32t _std hlp_unmountvol(u8t drive) {
   return io_unmount(drive, IOUM_FORCE)?0:1;
}

static u32t _std dsk_chkwrite(u32t disk, u64t sector, u32t count) {
#if 0
   if ((disk&QDSK_VOLUME)==0) {
      u8t   drv;
      disk &= ~(QDSK_DIRECT|QDSK_IAMCACHE|QDSK_IGNACCESS);

      mt_swlock();
      for (drv=0; drv<DISK_COUNT; drv++) {
         vol_data *vdta = _extvol + drv;
         if (vdta->flags&VDTA_ON)
            if (vdta->disk==disk) {
               u64t  vol_end = vdta->start+vdta->length;
               if (sector>=vol_end || vdta->start>=sector+count) continue; else
               if (sector>=vdta->start) { count=0; break; } else {
                  count = vdta->start - sector;
                  if (!count) break;
               }
            }
      }
      mt_swunlock();
   } else {

   }
#endif
   return count;
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
   if (src->dsec>99 || src->sec>59 || src->min>59 || src->hour>23 || !src->day ||
      src->mon>11) return 0;
   if (src->day>30+is_31[src->mon]) return 0;
   return 1;
}

mt_prcdata *get_procinfo(void) {
   process_context *pq = mod_context();
   return (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
}

void _std io_dumpsft(printf_function pfn) {
   if (!pfn) pfn = log_printf;
   pfn("===== SFT list =====\n");
   if (sftable_alloc) {
      u32t ii = 0;

      pfn("     file name          |   fno   | brk | type  | ptr      |\n");
      mt_swlock();

      for (ii=0; ii<sftable_alloc; ii++)
         if (sftable[ii]) {
#define ASTRLEN 96
            static const char *tname[] = { "FILE", "DISK", "STD", "DUMMY",
                                           "DIR", "MUTEX", "QUEUE", "EVENT" };
            char          astr[ASTRLEN];
            sft_entry      *fe = sftable[ii];
            int           tpos = bsf32(fe->type);
            u32t           idx;
            if (tpos >= sizeof(tname)/sizeof(char*)) tpos = -1;
            astr[0] = 0;

            if (fe->type==IOHT_MUTEX) {
               qs_sysmutex_state  mx;
               int   mstate = mtmux->state(fe->muxev.mh, &mx);
               if (mstate<=0) snprintf(astr, ASTRLEN, mstate<0?"broken":"free"); else
                  snprintf(astr, ASTRLEN, "pid %u, tid %u, cnt %i, wait %u",
                     mx.pid, mx.tid, mx.state, mx.waitcnt);
               if (ii!=mx.sft_no) snprintf(astr, ASTRLEN, "sft_no mismatch!!");
            } else
            if (fe->type==IOHT_QUEUE) {
               u32t n_sched;
               int evnum = qe_available_info(fe->que.qi, &n_sched);
               snprintf(astr, ASTRLEN, "%i events, %u scheds", evnum, n_sched);
            }

            pfn("%3d. %-18s |%8i |  %i  | %-5s | %08X | %s\n", ii, fe->fpath,
               fe->pub_fno, fe->broken, tpos<0?"??":tname[tpos], fe, astr);

            astr[0] = 0;
            idx = fe->ioh_first;
            while (idx) {
               char     hmode[8];
               io_handle_data *hd = ftable[idx];
               if (hd->sign!=IOHd_SIGN) { ioh_logerr(fe, hd, idx); break; }
               if (!astr[0]) strcpy(astr, "      ");

               if (fe->type==IOHT_FILE) {
                  static char *oms = "?N??EATC", // New-Existing-Always-Trunc-Create
                              *sms = "-wr+",     // deny all-write-read-none
                              *xms = "-rd+";     // deny all-ren-del-none
                  u32t  om = hd->file.omode;
                  char *mp = hmode;
                  *mp++ = ':';
                  if (om&IOFM_READ) *mp++ = 'r';
                  if (om&IOFM_WRITE) *mp++ = 'w';
                  *mp++ = oms[om&IOFM_OPEN_MASK];
                  *mp++ = sms[om>>12&3];
                  *mp++ = xms[om>>14&3];
                  *mp++ = 0;  // up to 6 chars
               } else
                  hmode[0] = 0;

               // (handle[:mode]:pid) ..
               sprintf(astr+strlen(astr), hd->pid?"(%u%s:%u) ":"(%u%s:*) ",
                  idx, hmode, hd->pid);


               idx = hd->next;
               if (strlen(astr)>ASTRLEN-24 || !idx)
                  { pfn( "%s\n", astr); astr[0] = 0; }
            }
         }
      mt_swunlock();
   }
   pfn("====================\n");
}

qserr _std io_registerfs(u32t classid) {
   u32t ncnt = exi_methods(classid), *pos, ii;
   // check number of methods
   if (ncnt!=sizeof(_qs_sysvolume)/sizeof(void*)) return E_SYS_INVOBJECT;

   mt_swlock();
   pos = memchrd(fs_classid, 0, FS_COUNT);
   if (pos) *pos = classid;
   mt_swunlock();

   if (pos) {
      // create test instance & get avail() result from it
      u32t   fs_index = pos - fs_classid, avail;
      qs_sysvolume ci = (qs_sysvolume)exi_createid(classid, 0);
      if (!ci) {
         *pos = 0;
         return E_SYS_INVOBJECT;
      }
      fs_avail[fs_index] = avail = ci->avail();
#if 0
      log_it(3, "%s %sfs handler installed (%u)\n", exi_classname(ci),
         avail&SFAF_VFS?"virtual ":"", classid);
#endif
      DELETE(ci);
      /* enum volumes and try to remount every one this unknown filesystem,
         but only if installed FS is not a VFS */
      if ((avail&SFAF_VFS)==0)
      for (ii=0; ii<DISK_COUNT; ii++) {
         vol_data  *vdta = _extvol + ii;
         qs_sysvolume vl = ve[ii], vn;

         if (ii!=DISK_LDR && (vdta->flags&VDTA_ON) && exi_classid(vl)==nulfs_classid) {
            // lock it here to prevent changes in disk configuration
            mt_swlock();

            if (hlp_volinfo(ii,0)==FST_NOTMOUNTED)
               if (!io_findfs(ii, FINDFS_NOFORCE|FINDFS_NOLIB, &vn)) {
                  u32t nbrk = sft_volumebroke(ii,0);
                  // should never occur, because NUL handler was forced
                  if (nbrk) log_it(3, "Re-mount %c - %u handles lost", 'A'+ii, nbrk);

                  fsver[ii] = 0xFF;
                  ve[ii]    = vn;
                  // delete NUL handler
                  vl->done();
                  DELETE(vl);
               }
            mt_swunlock();
         }
      }
   }
   return pos?0:E_SYS_FILES;
}

qserr io_mount_ramfs(u8t vol) {
   u32t res = E_DSK_BADVOLNAME;
   if (!ramfs_classid) return E_DSK_MOUNTERR;

   if (vol>=DISK_LDR && vol<DISK_COUNT) {
      vol_data *vdta;
      mt_swlock();
      vdta = _extvol + vol;
      if (vdta->flags&VDTA_ON) res = E_DSK_UMOUNTERR; else {
         qs_sysvolume  vl;
         u32t       total,
                    msize = hlp_memavail(0,&total);
         vdta->sectorsize = 512;
         vdta->length     = total>>9;
         vdta->start      = 0;
         vdta->disk       = FFFF;
         vdta->flags      = VDTA_ON|VDTA_VFS;
         vdta->serial     = tm_getdate();
         vdta->badclus    = 0;
         vdta->clsize     = 1;
         vdta->label[0]   = 0;
         vdta->clfree     = msize - (msize>>3) >> 9;
         vdta->cltotal    = vdta->length;

         vl  = (qs_sysvolume)exi_createid(ramfs_classid, EXIF_SHARED);
         res = vl->init(vol, 0, 0);
         if (res) {
            vdta->flags = 0;
            DELETE(vl);
         } else {
            ve[vol]    = vl;
            v_mounted |= 1<<vol;
            fsver[vol] = 0xFF;
            hlp_cachenotify(vol, CC_MOUNT);
         }
      }
      mt_swunlock();
   }
   return res;
}

void setup_fio() {
   u32t      ii;
   memset(&ve, 0, sizeof(ve));
   // nulfs must be 1st of all
   register_nulfs();

   memset(&fsver, 0xFF, sizeof(fsver));
   memset(&fs_classid, 0, sizeof(fs_classid));
   v_mounted = 0;
   _extvol   = (vol_data*)sto_data(STOKEY_VOLDATA);

   register_ramfs();
   io_mount_ramfs(DISK_LDR);
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

/* this is smart mount, which called at the end of initialization.
   here we have extcmd.ini & ready module support so we can try to detect
   FS by signature and load FS lib for it */
void mount_vol0(void) {
   vol_data *vdta = _extvol + DISK_BOOT;
   // we can mount FAT on EFI host because volume is mounted as r/o
   if ((vdta->flags&VDTA_ON)!=0 /*&& hlp_hosttype()!=QSHT_EFI*/) {
      qs_sysvolume vol = 0;
      qserr        res = io_findfs(DISK_BOOT, 0, &vol);

      if (res) log_it(3, "v0 findfs = %X\n", res); else {
         v_mounted       |= 1<<DISK_BOOT;
         fsver[DISK_BOOT] = 0xFF;
         ve[DISK_BOOT]    = vol;
         hlp_cachenotify(DISK_BOOT, CC_MOUNT);

         /* replace MFS functions for FAT only, but leave HPFS as it is.
            It is more safe to have two copies of kernel read code */
         if (vdta->flags&VDTA_FAT) mod_secondary->mfs_replace = 1;
      }
   }
}

void setup_fio_mt(dsk_access_cbf *rcb, dsk_access_cbf *wcb) {
   *wcb = dsk_chkwrite;
}

/** query list of installed file systems.
    @return list of qwords with class id in low dword and result of
    class avail() function in high dword. */
dq_list io_fslist(void) {
   if (!nulfs_classid) return 0; else {
      dq_list rc = NEW(dq_list);
      u32t    ii;
      mt_swlock();
      rc->add(nulfs_classid);

      for (ii=0; ii<FS_COUNT; ii++)
         if (fs_classid[ii]) rc->add((u64t)fs_avail[ii]<<32 | fs_classid[ii]);
      mt_swunlock();
      return rc;
   }
}

void fs_list_int(printf_function pfn) {
   dq_list fslist = io_fslist();
   u32t        ii;
   pfn(" id  name              opts     module\n"
       "--- ---------------- --------- -----------\n");
   for (ii=0; ii<fslist->count(); ii++) {
      u64t    id = fslist->value(ii);
      void *inst = exi_createid(id,0);
      if (inst) {
         char *cname = exi_classname(inst), mname[129];
         DELETE(inst);
         mod_getname(exi_provider(id), mname);
         pfn("%3u %-16s %9s  %s\n", (u32t)id, cname, id>>32&SFAF_VFS?
            " virtual ":"", mname);
         free(cname);
      }
   }
   DELETE(fslist);
}
