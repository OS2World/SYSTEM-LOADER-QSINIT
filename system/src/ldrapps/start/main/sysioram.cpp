//
// QSINIT "start" module
// "ramfs" implementation for disk B:
// -------------------------------------
// * case insensitive
// * sorted directory list, unicode
// * expanded space after setsize is *always* zeroed
//
#include "qstypes.h"
#include "qserr.h"
#include "qsint.h"
#include "qslog.h"
#include "qsxcpt.h"
#include "stdlib.h"
#include "qcl/sys/qsvolapi.h"
#include "sysio.h"
#include "wchar.h"
#include "classes.hpp"

#define RAMFS_SIGN      0x444D4152   ///< RAMD string
#define RFSDIR_SIGN     0x45444D52   ///< RMDE string
#define RFSFH_SIGN      0x48464D52   ///< RMFH string
#define RFSFD_SIGN      0x44464D52   ///< RMFD string
#define RFS_DBLEVEL     0

#define FDCE_WAIT_TIME  10    ///< seconds to wait before resize memory
#define FSIZE_MAX       x7FFF ///< file size limit (malloc will limit us on 2Gb-64k)

struct file_data {
   u32t           sign;     ///< RFSFD_SIGN
   u32t           size;

   u32t         in_use;     ///< # of users, including all sub-trees

   u32t          alloc;     ///< size of data. Can be < than size
   u8t           *data;     ///< file data

   u32t          flags;     ///< FDF_*
   u16t          attrs;     ///< file attributes
   u16t        namelen;

   file_data   *parent;
   TPtrList       *dir;     ///< list of file_data for the directory (or 0)

   u32t          nhash;     ///< uppercased name hash
   wchar_t       *name,     ///< file name
                *uname;     ///< uppercased name (in the same buf with name)
   io_time       ctime;
   io_time       atime;
   io_time       wtime;

   TPtrList      users;     ///< file_handle + dir_enum_data pointers
};

struct volume_data {
   u32t           sign;     ///< RAMFS_SIGN
   int             vol;     ///< -1 or 0..9
   qs_sysvolume   self;     ///< ptr to self instance
   file_data     *root;
};

#define FDF_DCE   0x0001

struct dir_enum_data {
   u32t           sign;
   file_data       *dd;
   u32t            pos;     ///< index in dir list
   char        dirname[QS_MAXPATH+1];
};

struct file_handle {
   u32t           sign;
   file_data       *fd;
   char             fp[QS_MAXPATH+1];
};

#define volinst_ret(inst,err)              \
   volume_data *inst = (volume_data*)data; \
   if (inst->sign!=RAMFS_SIGN) return err;

#define volinst_void(inst)                 \
   volume_data *inst = (volume_data*)data; \
   if (inst->sign!=RAMFS_SIGN) return;

#define fdinst_ret(inst,err)             \
   file_data *inst = (file_data*)data;   \
   if (inst->sign!=RFSFD_SIGN) return err;

// should be called in MT lock
static u32t _std dce_callback(u32t code, void *data) {
   fdinst_ret(fd, DCNR_GONE);
   // shrink memory block
   if (code==DCN_TIMER || code==DCN_MEM)
      if (fd->alloc >= fd->size+_64KB)
         fd->data = (u8t*)realloc(fd->data, fd->alloc = fd->size + 512);
#if RFS_DBLEVEL>0
   log_it(3, "dce(%u,%S) = %08X, %u (alloc %u) \n", code, fd->name, fd, fd->size, fd->alloc);
#endif
   // drop DCE flag
   fd->flags &= ~FDF_DCE;
   return DCNR_GONE;
}

/** schedule memory releasing callback after 10 seconds.
    Actually, DCE timer is stable in MT mode only, but in non-MT it called
    with DCN_MEM, at least, which is also a good idea */
static void dce_push(file_data *fd) {
   if (fd->alloc - fd->size >= _64KB)
      if (sys_dcenew(fd, DCF_UNSAFE, FDCE_WAIT_TIME, dce_callback)==0)
         fd->flags|=FDF_DCE;
}

static wchar_t char_to_wu(char cc) {
   return (u8t)cc>=0x80 ? ff_wtoupper(ff_convert((u8t)cc,1)) : toupper(cc);
}

/** calculate name hash for UPPERcased name.
    @param   name         name to calc
    @param   len          # of chars from name to use (or 0)
    @return hash value */
static u32t calc_hash(const char *name, u32t len = 0) {
   u32t res = 0;
   if (!len) len = x7FFF;
   while (*name && len--) {
      res += char_to_wu(*name++);
      res = (res>>2) + (res<<30);
   }
   return res;
}

/// the same as calc_hash() but for *uppercased* unicode string
static u32t calc_hash_wu(const wchar_t *name, u32t len = 0) {
   u32t res = 0;
   if (!len) len = x7FFF;
   while (*name && len--) {
      res += *name++;
      res = (res>>2) + (res<<30);
   }
   return res;
}

static void usage_modify(file_data *fd, int incr) {
   while (fd) {
      if (incr) fd->in_use++; else fd->in_use--;
      fd = fd->parent;
   }
}

static int unlink_user(file_data *fd, void *user) {
   int idx = fd->users.IndexOf(user);
   if (idx<0) return 0;
   fd->users.Delete(idx);
   usage_modify(fd,0);
   return 1;
}

/// inc/dec directory enumeration indices after adding/removing file in it
static void fix_user_index(file_data *dir, u32t index, int incr) {
   for (u32t ii=0; ii<dir->users.Count(); ii++) {
      dir_enum_data *de = (dir_enum_data*)dir->users[ii];
      // both file_handle & dir_enum_data are listed here
      if (de->sign==RFSDIR_SIGN)
         if (de->pos>=index)
            if (incr) de->pos++; else de->pos--;
   }
}

/// link file to a directory
static qserr file_link(file_data *dir, file_data *fd) {
   // skip .. for subdirectories
   u32t pos = dir->parent==0?0:1;
   // dumb enumeration, still should be fast because this is RAM fs
   while (pos<dir->dir->Count()) {
      file_data *de = (file_data*)(*dir->dir)[pos];
      if (de->uname[0] < fd->uname[0]) { pos++; continue; }
      if (de->uname[0] > fd->uname[0]) break;

      int rc = wcscmp(de->uname, fd->uname);
      if (rc<0) { pos++; continue; }
      if (rc==0) return E_SYS_EXIST;
      break;
   }
   dir->dir->Insert(pos, fd);
   fix_user_index(dir, pos, 1);
   fd->parent = dir;
   return 0;
}

/** unlink file from the parent directory.
    file data is untouched after this call */
static qserr file_unlink(file_data *fd) {
   file_data *pd = fd->parent;
   if (!pd) return E_SYS_SOFTFAULT;
   int idx = pd->dir->IndexOf(fd);
   if (idx<0) return E_SYS_SOFTFAULT;
   pd->dir->Delete(idx);
   fix_user_index(pd, idx, 0);
   return 0;
}

static void file_delete(file_data *fd) {
   if (fd->sign!=RFSFD_SIGN) return;
   // unschedule DCE callback first!
   if (fd->flags&FDF_DCE) {
      sys_dcedel(fd);
      fd->flags &= ~FDF_DCE;
   }
   if (fd->dir) {
      for (int ii=fd->dir->Max(); ii>=0; ii--) {
         file_delete((file_data*)((*fd->dir)[ii]));
         fd->dir->Delete(ii);
      }
      delete fd->dir;
      fd->dir = 0;
   }
   if (fd->users.Count()) {
      for (u32t ii=0; ii<fd->users.Count(); ii++) {
         u32t *ptr = (u32t*)fd->users[ii];
         // clean up signature to be safe
         if (*ptr==RFSDIR_SIGN || *ptr==RFSFH_SIGN) { *ptr = 0; free(ptr); } else
            log_it(3, "unlink(%S,%X) user data: %X = %X\n", fd->name, fd, ptr, *ptr);
      }
      fd->users.Clear();
   }
   if (fd->data) { free(fd->data); fd->data = 0; fd->alloc = 0; }
   if (fd->name) { free(fd->name); fd->name = 0; fd->namelen = 0; }
   fd->parent = 0;
   fd->uname  = 0;
   fd->size   = 0;
   fd->sign   = 0;
   delete fd;
}

/** walk over tree to the selected path.
    @param   vd             volume data
    @param   path           full path (accepts '\' only)
    @param   [out] fd       file_data object
    @param   [out] failpos  if ptr is non-zero, then last reached fd value
                            will be returned (directory) and failpos set to
                            offset in the path buffer, where non-existing
                            path begin.
    @return error code or 0 */
static qserr follow_path(volume_data *vd, const char *path, file_data **fd,
                         u32t *failpos = 0)
{
   *fd = 0;
   // path MUST be full
   if (path[1]!=':' || path[0]-'A'!=vd->vol) return E_SYS_SOFTFAULT;

   const char *cp = path+2;
   // is this a root?
   while (*cp=='\\') cp++;
   if (*cp==0) { *fd = vd->root; return 0; }

   file_data *cdd = vd->root;
   qserr      res = 0;
   // directory read
   do {
      const char   *ce = cp;
      while (*ce && *ce!='\\') ce++;

      u32t nmlen = ce-cp, *npos,
           nhash = calc_hash(cp, nmlen);
      // loop over directory
      res = E_SYS_NOPATH;
      for (u32t idx = 0; idx<cdd->dir->Count(); idx++) {
         file_data *de = (file_data*)(*cdd->dir)[idx];

         if (nmlen!=de->namelen) continue;
         if (nhash==de->nhash) {
            wchar_t    *fnp = de->uname;
            const char *cpp = cp;
            while (cpp!=ce)
               if (char_to_wu(*cpp++) != *fnp++) break;
            // name match!
            if (cpp==ce) {
               cp = ce;
               while (*cp=='\\') cp++;
               // reach the end?
               if (!*cp) { *fd = de; res = 0; } else
                  if (de->attrs&IOFA_DIR) { cdd = de; res = 0; }
               break;
            }
         }
      }
   } while (!res && *cp);
   // only a part of path exists (for mkdir below)
   if (res==E_SYS_NOPATH && failpos) {
      *failpos = cp - path;
      *fd      = cdd;
   }
#if RFS_DBLEVEL>1
   log_it(3, "follow_path(%s, %X) = %X\n", path, *fd, res);
#endif
   return res;
}

static void fd_getname(file_data *fd, char *dst) {
   for (u32t ii=0; ii<fd->namelen; ii++) {
      u16t ch = ff_convert(fd->name[ii],0);
      dst[ii] = ch?ch:'?';
   }
   dst[fd->namelen] = 0;
}

/* function should save original name until complete (re)naming */
static qserr fd_setname(file_data *fd, const char *name) {
   u32t    len = strlen(name), ii;
   wchar_t *rc = (wchar_t*)malloc(len+1<<2),
          *urc = rc + (len+1);
   if (!rc) return E_SYS_NOMEM;
   rc [len] = 0;
   urc[len] = 0;

   for (ii=0; ii<len; ii++) {
      if ((rc[ii] = ff_convert(name[ii],1))==0) {
         free(rc);
         return E_SYS_CPLIB;
      }
      urc[ii] = ff_wtoupper(rc[ii]);
   }
   if (fd->name) free(fd->name);
   fd->name    = rc;
   fd->uname   = urc;
   fd->namelen = len;
   fd->nhash   = calc_hash_wu(urc);
   return 0;
}

static qserr file_alloc(const char *name, int dir, file_data **pfd) {
   file_data *fd = new file_data;
   time_t    now = time(0);
   qserr     err = 0;
   fd->sign      = RFSFD_SIGN;
   fd->size      = 0;
   fd->in_use    = 0;
   fd->flags     = 0;
   fd->alloc     = 0;
   fd->data      = 0;
   fd->attrs     = 0;
   fd->name      = 0;
   fd->uname     = 0;
   fd->parent    = 0;
   fd->dir       = 0;
   fd->nhash     = 0;
   fd->namelen   = 0;
   *pfd = 0;
   // can fail on missing CPLIB and char code >=128
   if (name) err = fd_setname(fd, name);
   if (!err) {
      io_timetoio (&fd->ctime, now);
      fd->atime = fd->ctime;
      fd->wtime = fd->ctime;

      if (dir) {
         file_data *ddot;
         err = file_alloc("..", 0, &ddot);
         if (!err) {
            fd->dir      = new TPtrList;
            fd->attrs    = IOFA_DIR;
            ddot->parent = fd;
            ddot->attrs  = IOFA_DIR;
            fd->dir->Add(ddot);
         }
      }
   }
   if (err) file_delete(fd); else *pfd = fd;
   return err;
}

static qserr _exicc ramfs_initialize(EXI_DATA, u8t vol, u32t flags, void *bootsec) {
   volinst_ret(vd, E_SYS_INVOBJECT);

   vd->vol = vol;
   file_alloc(".", 0, &vd->root);

   vd->root->dir   = new TPtrList;
   vd->root->attrs = IOFA_DIR;
   vol_data  *vdta = _extvol + vol;
   vdta->flags    |= VDTA_VFS;
   strcpy(vdta->fsname, "RAMFS");

   return 0;
}

/// unmount volume
static qserr _exicc ramfs_finalize(EXI_DATA) {
   volinst_ret(vd, E_SYS_INVOBJECT);

   if (vd->vol<0) return E_DSK_NOTMOUNTED; else {
      file_delete(vd->root);
      vd->root =  0;
      vd->vol  = -1;
      return 0;
   }
}

static int _exicc ramfs_drive(EXI_DATA) {
   volinst_ret(vd, -1);
   return vd->vol;
}

static qserr _exicc ramfs_open(EXI_DATA, const char *name, u32t mode,
                             io_handle_int *pfh, u32t *action)
{
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!pfh || !name || !action) return E_SYS_ZEROPTR;
   *pfh = 0;
   if (mode>SFOM_CREATE_ALWAYS) return E_SYS_INVPARM;
   if (strlen(name)>QS_MAXPATH) return E_SYS_LONGNAME;

   file_data *fd, *dd;
   u32t     fpos;
   char   *cpath = strdup(name);
   char   *cname = 0;  // name to create
   qserr     res = follow_path(vd, cpath, &fd, &fpos);

   if (!res && (fd->attrs&IOFA_DIR)) res = E_SYS_ISDIR; else
      if (res==E_SYS_NOPATH) {
         cname = cpath+fpos;
         res   = strchr(cname,'\\') ? E_SYS_NOPATH : 0;
      }
   if (!res)
      if (cname && mode==SFOM_OPEN_EXISTING) res = E_SYS_NOFILE; else
         if (!cname && mode==SFOM_CREATE_NEW) res = E_SYS_EXIST;
   if (!res) {
      if (cname) {
         dd  = fd;
         res = file_alloc(cname, 0, &fd);
         if (!res)
            if ((res = file_link(dd, fd)) != 0) file_delete(fd);
      } else
         dd  = fd->parent;
   }
   free(cpath);
   if (res) return res;

   // trunc file and schedule memory release
   if (mode==SFOM_CREATE_ALWAYS && !cname) {
      fd->size = 0;
      *action  = IOFN_TRUNCATED;
      dce_push(fd);
   } else
      *action  = cname ? IOFN_CREATED : IOFN_EXISTED;

   file_handle *fh = (file_handle*)malloc(sizeof(file_handle));
   strcpy(fh->fp, name);
   fh->sign = RFSFH_SIGN;
   fh->fd   = fd;
   *pfh     = (io_handle_int)fh;
   // file & all of parents are in use
   usage_modify(fd, 1);
   fd->users.Add(fh);
#if RFS_DBLEVEL>0
   log_it(3, "open(%s) fd:%X, in_use:%u, sz:%u, alloc:%u, data:%08X\n",
      name, fd, fd->in_use, fd->size, fd->alloc, fd->data);
#endif
   return 0;
}

static qserr _exicc ramfs_read(EXI_DATA, io_handle_int file, u64t pos,
                             void *buffer, u32t *size)
{
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!buffer || !size) return E_SYS_ZEROPTR;

   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   if (!*size) return 0;
   file_data   *fd = fh->fd;
   u32t     toread = *size,
            tofill = 0;
   if (pos>=fd->size) return E_SYS_EOF;
   if (pos+toread > fd->size) toread = fd->size - pos;
   *size = toread;
   // take in mind unallocated space
   if (pos>fd->alloc) { tofill = toread; toread = 0; } else
      if (pos+toread > fd->alloc) {
         tofill = pos + toread - fd->alloc;
         toread-= tofill;
      }
   if (toread) memcpy(buffer, fd->data + (u32t)pos, toread);
   if (tofill) memset((u8t*)buffer + toread, 0, tofill);
   io_timetoio (&fd->atime, time(0));
   return 0;
}

static qserr _exicc ramfs_write(EXI_DATA, io_handle_int file, u64t pos,
                              void *buffer, u32t *size)
{
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!buffer || !size) return E_SYS_ZEROPTR;
   if (pos>FSIZE_MAX) return E_SYS_FSLIMIT;

   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   if (!*size) return 0;
   file_data   *fd = fh->fd;
   u32t    towrite = *size;
   if (pos+towrite > FSIZE_MAX) towrite = FSIZE_MAX + 1 - pos;
   if (pos+towrite > fd->alloc) {
      // try to eliminate op at all (because realloc below is much worse)
      if (pos>=fd->alloc && *(u8t*)buffer==0)
         if (memchrnb((u8t*)buffer, 0, towrite)==0) return 0;
      u32t newsize = pos+towrite;
      if (newsize>_16KB) newsize = Round2k(newsize);
      *size = 0;
      // deny huge reallocs on no memory
      if (newsize>=_128KB) {
         u32t maxsize = 0;
         hlp_memavail(&maxsize, 0);
         if (maxsize<_2MB || newsize>maxsize) return E_SYS_NOMEM;
      }
      u8t *dnew = (u8t*)realloc(fd->data, newsize);
      // let it be here, even if our realloc() never returns on error
      if (!dnew) return E_SYS_NOMEM;
      // zero added memory
      memset(dnew + fd->alloc, 0, newsize - fd->alloc);
      fd->data  = dnew;
      fd->alloc = newsize;
   }
   memcpy(fd->data + (u32t)pos, buffer, towrite);
   if (pos+towrite > fd->size) fd->size = pos+towrite;
   *size = towrite;
   io_timetoio (&fd->atime, time(0));
   fd->wtime = fd->atime;
   return 0;
}

static qserr _exicc ramfs_flush(EXI_DATA, io_handle_int file) { return 0; }

static qserr _exicc ramfs_close(EXI_DATA, io_handle_int file) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   unlink_user(fh->fd, fh);
   fh->sign = 0;
   free(fh);
   return 0;
}

static qserr _exicc ramfs_size(EXI_DATA, io_handle_int file, u64t *size) {
   if (!size) return E_SYS_ZEROPTR;
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   *size = fh->fd->size;
   return 0;
}

static qserr _exicc ramfs_setsize(EXI_DATA, io_handle_int file, u64t newsize) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (newsize>FSIZE_MAX) return E_SYS_FSLIMIT;
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   file_data   *fd = fh->fd;
   u32t nsize = newsize;
   if (nsize<fd->size) {
      fd->size = nsize;
      dce_push(fd);
   } else
   if (nsize>fd->size) {
      // zero tail of allocated block
      if (fd->alloc>fd->size) {
         u32t diff = fd->alloc - fd->size,
              grow = nsize - fd->size;
         memset(fd->data+fd->size, 0, diff>grow?grow:diff);
      }
      fd->size = nsize;
   }
   return 0;
}

static void fd2dirinfo(file_data *de, io_direntry_info *info) {
   info->attrs   = de->attrs;
   info->fileno  = 0;
   info->size    = de->size;
   info->vsize   = de->alloc<de->size?de->alloc:de->size;
   info->dir     = 0;
   info->atime   = de->atime;
   info->wtime   = de->wtime;
   info->ctime   = de->ctime;

   fd_getname(de, info->name);
}

static qserr _exicc ramfs_finfo(EXI_DATA, io_handle_int file, io_direntry_info *info) {
   if (!info) return E_SYS_ZEROPTR;
   volinst_ret(vd, E_SYS_INVOBJECT);
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=RFSFH_SIGN) return E_SYS_INVPARM;
   // a bit faster than function below
   fd2dirinfo(fh->fd, info);
   return 0;
}

static qserr _exicc ramfs_pathinfo(EXI_DATA, const char *path, io_handle_info *info) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;
   file_data *de;
   qserr     res = follow_path(vd, path, &de);
   if (!res) {
      info->attrs   = de->attrs;
      info->fileno  = 0;
      info->size    = de->size;
      info->vsize   = de->alloc<de->size?de->alloc:de->size;
      info->atime   = de->atime;
      info->wtime   = de->wtime;
      info->ctime   = de->ctime;
   }
   return res;
}

static qserr _exicc ramfs_setinfo(EXI_DATA, const char *path, io_handle_info *info, u32t flags) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;
   file_data *de;
   qserr     res = follow_path(vd, path, &de);
   if (!res) {
      if (flags&IOSI_ATTRS) de->attrs = info->attrs;
      if (flags&IOSI_ATIME) de->atime = info->atime;
      if (flags&IOSI_CTIME) de->ctime = info->ctime;
      if (flags&IOSI_WTIME) de->wtime = info->wtime;
   }
   return res;
}

static qserr _exicc ramfs_setexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t size) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static qserr _exicc ramfs_getexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t *size) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static str_list* _exicc ramfs_lstexattr(EXI_DATA, const char *path) {
   volinst_ret(vd, 0);
   return 0;
}

// warning! it acts as rmdir too!!
static qserr _exicc ramfs_remove(EXI_DATA, const char *path) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!path) return E_SYS_ZEROPTR;
   file_data *fd;
   qserr     res = follow_path(vd, path, &fd);
   if (!res)
      if (fd->in_use || fd==vd->root) res = E_SYS_ACCESS; else
         if (fd->dir && fd->dir->Count()>1) res = E_SYS_DIRNOTEMPTY; else 
            if (fd->attrs&IOFA_READONLY) res = E_SYS_READONLY; else {
               res = file_unlink(fd);
               if (!res) file_delete(fd);
            }
   return res;
}

static qserr _exicc ramfs_renmove(EXI_DATA, const char *oldpath, const char *newpath) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!oldpath || !newpath) return E_SYS_ZEROPTR;

   char *cpatho = strdup(oldpath),
        *cpathn = strdup(newpath);
   qserr     rc = 0;

   if (!cpathn) rc = E_SYS_INVPARM; else
   do {
      file_data *src, *dst, *psrc;
      char *cpname = strrchr(cpathn,'\\');
      // path _must_ be full here, so the next line is void
      if (!cpname) { rc = E_SYS_INVPARM; break; }
      // source object
      rc = follow_path(vd, cpatho, &src);
      if (rc) break;
      // check for the destination
      if (follow_path(vd, cpathn, &dst)==0) rc = dst==src?0:E_SYS_EXIST; else {
         *cpname++ = 0;
         rc = follow_path(vd, cpathn, &dst);
         if (rc) break;
         // root cannot be renamed
         psrc = src->parent;
         if (psrc==0) { rc = E_SYS_INVNAME; break; }
         // unlink it in source dir
         rc = file_unlink(src);
         if (rc) break;
         // rename it and try to move/link back
         rc = fd_setname(src, cpname);
         if (!rc) rc = file_link(dst, src);
         if (rc) rc = file_link(psrc, src);
         // file is lost if both attempts fail
         if (rc) file_delete(src);
      }
   } while (0);
   free(cpathn);
   free(cpatho);
   return rc;
}

static qserr _exicc ramfs_makepath(EXI_DATA, const char *path) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   // follow_path() walks until last existing dir in path for us
   u32t     fpos;
   file_data *dd, *fd;
   char      *fp = strdup(path);
   qserr     res = follow_path(vd, fp, &dd, &fpos);

   if (!res) res = E_SYS_EXIST; else
   if (res==E_SYS_NOPATH) {
      char *cp = fp + fpos;
      // should be executed at least once
      while (cp && *cp) {
         char *ce = strchr(cp,'\\');
         if (ce) {
            *ce++ = 0;
            while (*ce=='\\') *ce++;
         }
         res = file_alloc(cp, 1, &fd);
#if RFS_DBLEVEL>0
         log_it(3, "mkdir(%s) = %08X, parent %08X, res = %X\n", cp, fd, dd, res);
#endif
         if (res) break;
         res = file_link(dd, fd);
         if (res) {
            file_delete(fd);
            break;
         }
         dd = fd;
         cp = ce;
      }
   }
   free(fp);
   return res;
}

static qserr _exicc ramfs_diropen(EXI_DATA, const char *path, dir_handle_int *pdh) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   if (!path || !pdh) return E_SYS_ZEROPTR;
   *pdh = 0;
   if (strlen(path)>QS_MAXPATH) return E_SYS_LONGNAME;
   // dir handle storage
   dir_enum_data *ed = (dir_enum_data*)malloc(sizeof(dir_enum_data));
   if (!ed) return E_SYS_NOMEM;
   strcpy(ed->dirname, path);
   file_data     *dd;
   qserr         res = follow_path(vd, path, &dd);

   if (!res)
      if (dd->attrs&IOFA_DIR) {
         ed->sign = RFSDIR_SIGN;
         ed->dd   = dd;
         ed->pos  = 0;
         *pdh     = (dir_handle_int)ed;
         usage_modify(dd,1);
         dd->users.Add(ed);
      } else
         res = E_SYS_NOPATH;

   if (res && ed) free(ed);
   return res;
}

static qserr _exicc ramfs_dirnext(EXI_DATA, dir_handle_int dh, io_direntry_info *info) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   dir_enum_data *ed = (dir_enum_data*)dh;
   if (!ed || !info) return E_SYS_ZEROPTR;
   if (ed->sign!=RFSDIR_SIGN) return E_SYS_INVPARM;
   /* pos can be modified by fix_user_index() above - if file added/removed
      *before* it during enumeration process */
   if (ed->pos>=ed->dd->dir->Count()) return E_SYS_NOFILE; else {
      fd2dirinfo((file_data*)((*ed->dd->dir)[ed->pos++]), info);
      info->dir = ed->dirname;
   }
   return 0;
}

static qserr _exicc ramfs_dirclose(EXI_DATA, dir_handle_int dh) {
   volinst_ret(vd, E_SYS_INVOBJECT);
   dir_enum_data *ed = (dir_enum_data*)dh;
   if (!ed) return E_SYS_ZEROPTR;
   if (ed->sign!=RFSDIR_SIGN) return E_SYS_INVPARM;

   ed->sign = 0;
   unlink_user(ed->dd, ed);
   free(ed);
   return 0;
}

static qserr _exicc ramfs_volinfo(EXI_DATA, disk_volume_data *info, int fast) {
   volinst_ret(vd, E_SYS_INVOBJECT);

   if (!info) return E_SYS_ZEROPTR;
   memset(info, 0, sizeof(disk_volume_data));

   if (vd->vol>=0) {
      vol_data *vdta = _extvol+vd->vol;
      // mounted?
      if (vdta->flags&VDTA_ON) {
         u32t       memsize = hlp_memavail(0,0);
         info->StartSector  = FFFF64;
         info->Disk         = FFFF;
         info->DataSector   = FFFF;
         info->TotalSectors = vdta->length;
         info->SectorSize   = vdta->sectorsize;
         info->FSizeLim     = FSIZE_MAX;
         info->ClBad        = 0;
         info->SerialNum    = vdta->serial;
         info->ClSize       = 1;
         info->ClAvail      = memsize - (memsize>>3) >> 9;
         info->ClTotal      = vdta->length;
         info->FsVer        = FST_OTHER;
         info->InfoFlags    = VIF_VFS;
         //info->Reserved     = 0;
         //info->OemData      = 0;
         strcpy(info->Label,vdta->label);
         strcpy(info->FsName,vdta->fsname);
         return 0;
      }
   }
   return E_DSK_NOTMOUNTED;
}

static qserr _exicc ramfs_setlabel(EXI_DATA, const char *label) {
   volinst_ret(vd, E_SYS_INVOBJECT);

   if (vd->vol>=0) {
      vol_data  *vdta = _extvol+vd->vol;
      char     cn[12];
      u32t        len = strlen(label);
      if (len>11) return E_SYS_LONGNAME;

      if (len<11) memset(&cn, ' ', 11);
      cn[11] = 0;
      while (len--) {
         cn[len] = toupper(label[len]);
         if (strchr("+.,;=[]\"*:<>\?|\x7F",cn[len]) || cn[len]>=128)
            return E_SYS_INVNAME;
      }
      // update own saved string
      strcpy(vdta->label, cn);
      trimright(vdta->label, " ");
      return 0;
   }
   return E_DSK_NOTMOUNTED;
}

static u32t _exicc ramfs_avail(EXI_DATA) { return SFAF_VFS|SFAF_FINFO; }

static void _std ramfs_init(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   vd->sign        = RAMFS_SIGN;
   vd->vol         = -1;
   vd->self        = (qs_sysvolume)instance;
}

static void _std ramfs_done(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   if (vd->sign==RAMFS_SIGN)
      if (vd->vol>=0) {
         log_printf("Volume %d fini???\n", vd->vol);
         // force it to release memory
         ramfs_finalize(vd, 0);
      }
   memset(vd, 0, sizeof(volume_data));
}

static void *m_list[] = { ramfs_initialize, ramfs_finalize, ramfs_avail,
   ramfs_volinfo, ramfs_setlabel, ramfs_drive, ramfs_open, ramfs_read,
   ramfs_write, ramfs_flush, ramfs_close, ramfs_size, ramfs_setsize,
   ramfs_finfo, ramfs_setexattr, ramfs_getexattr, ramfs_lstexattr, ramfs_remove,
   ramfs_renmove, ramfs_makepath, ramfs_remove, ramfs_pathinfo, ramfs_setinfo,
   ramfs_diropen, ramfs_dirnext, ramfs_dirclose };

extern "C" u32t ramfs_classid;

extern "C"
void register_ramfs(void) {
   // register private(unnamed) class
   ramfs_classid = exi_register("qs_sys_ramfs", m_list, sizeof(m_list)/sizeof(void*),
      sizeof(volume_data), EXIC_PRIVATE, ramfs_init, ramfs_done, 0);
   if (!ramfs_classid) _throwmsg_("ramfs - class reg.error");
   qserr res = io_registerfs(ramfs_classid);
   if (res) {
      log_printf("ramfs: regfs err %08X\n", res);
      _throwmsg_("ramfs - registration error");
   }
}
