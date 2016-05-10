#include "qstypes.h"
#include "qserr.h"
#include "qsint.h"
#include "qslog.h"
#include "qsstor.h"
#include "stdlib.h"
#include "parttab.h"
#include "qsxcpt.h"
#include "qcl/sys/qsvolapi.h"
#include "fat/ff.h"
#include "sysio.h"

#define VFAT_SIGN        0x54414656   ///< VFAT string
#define VFATDIR_SIGN     0x45444656   ///< VFDE string
#define VFATFH_SIGN      0x48464656   ///< VFFH string

typedef struct {
   u32t           sign;
   int             vol;     ///< -1 or 0..9
   qs_sysvolume   self;     ///< ptr to self instance
   int        mount_ok;     ///< FatFs mount was ok
} volume_data;

typedef struct {
   u32t           sign;
   DIR             dir;
   FILINFO          fi;
   char            lfn[QS_MAXPATH+1];
   char        dirname[QS_MAXPATH+1];
} dir_enum_data;

typedef struct {
   u32t           sign;
   FIL              fh;
   char             fp[QS_MAXPATH+1];
} file_handle;

u32t fatio_classid = 0;

#define instance_ret(type,inst,err)      \
   type *inst = (type*)data;             \
   if (inst->sign!=VFAT_SIGN) return err;

#define instance_void(type,inst)         \
   type *inst = (type*)data;             \
   if (inst->sign!=VFAT_SIGN) return;

static qserr errcvt(FRESULT err) {
   static u32t codes[FR_INVALID_PARAMETER+1] = { 0, E_DSK_IO, E_SYS_INVPARM,
      E_DSK_NOTREADY, E_SYS_NOFILE, E_SYS_NOPATH, E_SYS_INVNAME, E_SYS_ACCESS,
      E_SYS_EXIST, E_SYS_INVOBJECT, E_DSK_WP, E_SYS_INVPARM, E_DSK_NOTMOUNTED,
      E_DSK_UNCKFS, E_SYS_INCOMPPARAMS, E_SYS_TIMEOUT, E_SYS_SHARE, E_SYS_NOMEM,
      E_SYS_FILES, E_SYS_INVPARM};
   return err>FR_INVALID_PARAMETER? E_SYS_INVOBJECT : codes[err];
}

static qserr _std fat_initialize(void *data, u8t vol, u32t flags, void *bootsec) {
   FATFS  **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (vol>=_VOLUMES) return E_DSK_BADVOLNAME;

   if (extdrv) {
      vol_data  *vdta = _extvol+vol;
      FATFS     *fdta = extdrv[vol];
      qserr       err = E_DSK_NOTMOUNTED;
      if (flags&SFIO_APPEND) {
         if (vol<=DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
            if ((vdta->flags&VDTA_FAT) || (flags&SFIO_FORCE)) {
               vd->vol = vol;
               err = 0;
            } else err = E_DSK_UNCKFS;
         } else err = E_DSK_NOTMOUNTED;
      } else {
         struct Boot_Record *br = (struct Boot_Record*)bootsec;
         char             dp[8];
         FRESULT           mres;
         u32t           *fsname = 0;

         snprintf(dp, 8, "%u:\\", (u32t)vol);
         // FatFs mount
         mres = f_mount(fdta, dp, 1);

         if (br->BR_BPB.BPB_SecPerFAT)
            if (br->BR_BPB.BPB_RootEntries) fsname = (u32t*)br->BR_EBPB.EBPB_FSType;
               else fsname = (u32t*)((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_FSType;
         // if it is not FAT really - fix error code to acceptable
         if (mres!=FR_OK && (!fsname || (*fsname&0xFFFFFF)!=0x544146))
            mres = FR_NO_FILESYSTEM;
         if (mres==FR_OK) vdta->flags|=VDTA_FAT;
         if (mres==FR_OK || mres==FR_NO_FILESYSTEM && (flags&SFIO_FORCE)) vd->vol = vol;
         err = errcvt(mres);
      }
      if (!err && (vdta->flags&VDTA_FAT)) {
         strcpy(vdta->fsname, fdta->fs_type<=FS_FAT16?"FAT":"FAT32");
         vdta->clsize  = fdta->csize;
         vdta->cltotal = (vdta->length - fdta->database)/fdta->csize;
         vd->mount_ok  = 1;
      }
      return err;
   }
   return E_SYS_INVOBJECT;
}

/// unmount volume
static qserr _std fat_finalize(void *data) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (vd->vol<0) return E_DSK_NOTMOUNTED; else {
      char          dp[8];
      FRESULT      res = 0;
      vol_data   *vdta = _extvol+vd->vol;

      if (vd->mount_ok) {
         snprintf(dp, 8, "%d:", vd->vol);
         res = f_mount(0, dp, 0);
      }
      vdta->flags &= ~(VDTA_FAT|VDTA_ON);
      vd->vol      = -1;
      vd->mount_ok = 0;
      return errcvt(res);
   }
}

static int _std fat_drive(void *data) {
   instance_ret(volume_data, vd, -1);
   return vd->vol;
}

static char *fat_cvtpath(volume_data *vd, const char *fp) {
   if (fp[1]!=':' || fp[0]-'A'!=vd->vol) return 0; else {
      char *rc = strdup(fp);
      rc[0] -= 'A'-'0';
      mem_shareblock(rc);
      return rc;
   }
}

static qserr _std fat_open(void *data, const char *name, u32t mode,
                           io_handle_int *pfh, u32t *action)
{
   FRESULT       res;
   static u32t mf[4] = { FA_OPEN_EXISTING, FA_OPEN_ALWAYS, FA_CREATE_NEW,
                         FA_CREATE_ALWAYS };
   file_handle   *fh = 0;
   char       *cpath;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (mode>SFOM_CREATE_ALWAYS) return E_SYS_INVPARM;
   if (!pfh || !name || !action) return E_SYS_ZEROPTR;
   if (strlen(name)>QS_MAXPATH) return E_SYS_LONGNAME;

   cpath = fat_cvtpath(vd, name);
   if (!cpath) return E_SYS_INVPARM;
   *pfh  = 0;
   fh    = (file_handle*)malloc(sizeof(file_handle));
   res   = f_open(&fh->fh, cpath, FA_READ|FA_WRITE|mf[mode]);
   free(cpath);
   if (res) { free(fh); return errcvt(res); }

   // FatFs set it if file was actually created
   if ((fh->fh.flag&FA_CREATE_ALWAYS)) *action = IOFN_CREATED; else
      if ((mode&FA_CREATE_ALWAYS)) *action = IOFN_TRUNCATED; else
         *action = IOFN_EXISTED;
   strcpy(fh->fp, name);

   fh->sign = VFATFH_SIGN;
   *pfh     = (io_handle_int)fh;
   return 0;
}

static qserr _std fat_read(void *data, io_handle_int file, u64t pos,
                           void *buffer, u32t *size)
{
   file_handle   *fh = (file_handle*)file;
   FRESULT       res = 0;
   UINT           br = 0;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!buffer || !size) return E_SYS_ZEROPTR;
   if (fh->sign!=VFATFH_SIGN || pos>=fh->fh.fsize) return E_SYS_INVPARM;
   if (!*size) return 0;

   if (pos!=fh->fh.fptr) res = f_lseek(&fh->fh, pos);
   if (!res) res = f_read(&fh->fh, buffer, *size, &br);
   *size = br;

   return errcvt(res);
}

static qserr _std fat_write(void *data, io_handle_int file, u64t pos,
                            void *buffer, u32t *size)
{
   file_handle   *fh = (file_handle*)file;
   FRESULT       res = 0;
   UINT           bw = 0;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!buffer || !size) return E_SYS_ZEROPTR;
   if (fh->sign!=VFATFH_SIGN) return E_SYS_INVPARM;
   if (pos>=FFFF || pos+*size>=FFFF) return E_SYS_FSLIMIT;
   if (!*size) return 0;

   if (pos!=fh->fh.fptr) res = f_lseek(&fh->fh, pos);
   if (!res) res = f_write(&fh->fh, buffer, *size, &bw);
   *size = bw;

   return errcvt(res);
}

static qserr _std fat_flush(void *data, io_handle_int file) {
   file_handle   *fh = (file_handle*)file;
//   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=VFATFH_SIGN) return E_SYS_INVPARM;

   return errcvt(f_sync(&fh->fh));
}

static qserr _std fat_close(void *data, io_handle_int file) {
   FRESULT       res;
   file_handle   *fh = (file_handle*)file;
//   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=VFATFH_SIGN) return E_SYS_INVPARM;

   res = f_close(&fh->fh);
   fh->sign = 0;
   free(fh);
   return errcvt(res);
}

static qserr _std fat_size(void *data, io_handle_int file, u64t *size) {
   file_handle   *fh = (file_handle*)file;
//   instance_ret(volume_data, vd, 0);
   if (fh->sign!=VFATFH_SIGN) return E_SYS_INVPARM;
   if (!size) return E_SYS_ZEROPTR;
   *size = fh->fh.fsize;
   return 0;
}

static qserr _std fat_setsize(void *data, io_handle_int file, u64t newsize) {
   FRESULT       res = 0;
   file_handle   *fh = (file_handle*)file;
//   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=VFATFH_SIGN) return E_SYS_INVPARM;

   if (newsize>=FFFF) return E_SYS_FSLIMIT;
   if (newsize!=fh->fh.fptr) res = f_lseek(&fh->fh, newsize);
   if (!res && newsize<fh->fh.fsize) res = f_truncate(&fh->fh);
   if (!res) res = f_sync(&fh->fh);

   return errcvt(res);
}

static qserr _std fat_pathinfo(void *data, const char *path, io_handle_info *info) {
   FILINFO    *fi;
   FRESULT    res;
   time_t     tmv;
   char    *cpath;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;
   // do not use stack for such large alloc
   fi = (FILINFO*)malloc(sizeof(FILINFO)+QS_MAXPATH+1);
   if (!fi) return E_SYS_NOMEM;
   mem_zero(fi);
   fi->lfname = (char*)(fi+1);
   fi->lfsize = QS_MAXPATH+1;
   fi->lfname[0] = 0;

   cpath = fat_cvtpath(vd, path);
   if (!cpath) return E_SYS_INVPARM;
   res   = f_stat(cpath, fi);
   free(cpath);
   if (res==FR_OK) {
      info->attrs  = fi->fattrib;
      info->fileno = 0;
      info->size   = fi->fsize;

      tmv = dostimetotime((u32t)fi->fdate<<16|fi->ftime);
      io_timetoio(&info->atime, tmv);
      io_timetoio(&info->wtime, tmv);
      tmv = dostimetotime((u32t)fi->cdate<<16|fi->ctime);
      io_timetoio(&info->ctime, tmv);
   }
   free(fi);
   return res?errcvt(res):0;
}

static qserr _std fat_setinfo(void *data, const char *path, io_handle_info *info, u32t flags) {
   FRESULT       res;
   char       *cpath;
   qserr         err = 0;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;

   cpath = fat_cvtpath(vd, path);
   if (!cpath) return E_SYS_INVPARM;

   if (flags&IOSI_ATTRS) {
      res = f_chmod(cpath, info->attrs, AM_RDO|AM_ARC|AM_SYS|AM_HID);
      if (res!=FR_OK) err = errcvt(res);
   }
   if (!err && (flags&(IOSI_CTIME|IOSI_WTIME|IOSI_ATIME))) {
      io_handle_info   ci;
      u32t             dt;
      io_time  *pct, *pwt;
      FILINFO          fi;
      // combine two times into one
      if ((flags&IOSI_WTIME) && (flags&IOSI_ATIME)==0) pwt = &info->wtime;
         else pwt = flags&IOSI_ATIME?&info->atime:0;
      pct = flags&IOSI_CTIME?&info->ctime:0;
      // read missing one
      if (!pct || !pwt) {
         qserr rc = fat_pathinfo(data, path, &ci);
         if (rc) return rc;
         if (!pct) pct = &ci.ctime;
         if (!pwt) pwt = &ci.atime;
      }
      // a bit of paranoya
      fi.lfname = 0;
      fi.lfsize = 0;

      dt = timetodostime(io_iototime(pwt));
      fi.fdate = dt>>16;
      fi.ftime = (WORD)dt;
      dt = timetodostime(io_iototime(pct));
      fi.cdate = dt>>16;
      fi.ctime = (WORD)dt;

      res = f_utime(cpath, &fi);
      if (res!=FR_OK) err = errcvt(res);
   }
   free(cpath);
   return err;
}

static qserr _std fat_setattr(void *data, const char *path, u32t attr) {
   char       *cpath;
   qserr          rc;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path) return E_SYS_ZEROPTR;
   if ((attr&~(IOFA_READONLY|IOFA_HIDDEN|IOFA_SYSTEM|IOFA_ARCHIVE)))
      return E_SYS_INVPARM;

   cpath = fat_cvtpath(vd, path);
   if (!cpath) return E_SYS_INVPARM;
   rc    = errcvt(f_chmod(cpath, attr, AM_RDO|AM_ARC|AM_SYS|AM_HID));
   free(cpath);

   return rc;
}

static qserr _std fat_getattr(void *data, const char *path, u32t *attr) {
   io_handle_info pd;
   qserr          rc;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !attr) return E_SYS_ZEROPTR;

   *attr = 0;
   rc    = fat_pathinfo(data, path, &pd);
   if (rc) return rc;
   *attr = pd.attrs;
   return 0;
}

static qserr _std fat_setexattr(void *data, const char *path, const char *aname, void *buffer, u32t size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static qserr _std fat_getexattr(void *data, const char *path, const char *aname, void *buffer, u32t *size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static str_list* _std fat_lstexattr(void *data, const char *path) {
   instance_ret(volume_data, vd, 0);
   return 0;
}

// warning! it acts as rmdir too!!
static qserr _std fat_remove(void *data, const char *path) {
   char       *cpath;
   qserr          rc;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path) return E_SYS_ZEROPTR;

   cpath = fat_cvtpath(vd, path);
   if (!cpath) return E_SYS_INVPARM;
   rc    = errcvt(f_unlink(cpath));
   free(cpath);

   return rc;
}

static qserr _std fat_renmove(void *data, const char *oldpath, const char *newpath) {
   char      *cpatho,
             *cpathn;
   qserr          rc;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!oldpath || !newpath) return E_SYS_ZEROPTR;

   cpatho = fat_cvtpath(vd, oldpath);
   if (!cpatho) return E_SYS_INVPARM;
   cpathn = fat_cvtpath(vd, newpath);
   if (!cpathn) rc = E_SYS_INVPARM; else {
      rc  = errcvt(f_rename(cpatho,cpathn));
      free(cpathn);
   }
   free(cpatho);

   return rc;
}

static qserr _std fat_makepath(void *data, const char *path) {
   FRESULT    res;
   char  *fp, *cp;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   fp = fat_cvtpath(vd, path);
   if (!fp) return E_SYS_INVPARM;
   // create path.
   cp = strchr(fp,'\\');
   while (cp) {
      *cp  = 0;
      res   = f_mkdir(fp);
      *cp++= '\\';
      if (res!=FR_EXIST && res!=FR_OK) break;
      cp = strchr(cp,'\\');
   }
   // final target
   if (!cp) res = f_mkdir(fp);
   free(fp);
   return errcvt(res);
}

static qserr _std fat_diropen(void *data, const char *path, dir_handle_int *pdh) {
   dir_enum_data  *dd;
   char        *cpath = 0;
   qserr           rc;
   FRESULT        res;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !pdh) return E_SYS_ZEROPTR;
   *pdh = 0;
   // dir handle storage
   dd   = (dir_enum_data*)malloc(sizeof(dir_enum_data));
   if (!dd) return E_SYS_NOMEM;
   if (strlen(path)>QS_MAXPATH) rc = E_SYS_LONGNAME; else {
      strcpy(dd->dirname, path);
      cpath = fat_cvtpath(vd, path);
      if (!cpath) rc = E_SYS_INVPARM; else {
         rc = errcvt(f_opendir(&dd->dir, cpath));
         if (!rc) {
            dd->sign = VFATDIR_SIGN;
            *pdh     = (dir_handle_int)dd;
         }
      }
   }
   if (rc && dd) free(dd);
   if (cpath) free(cpath);
   return rc;
}

static qserr _std fat_dirnext(void *data, dir_handle_int dh, io_direntry_info *de) {
   dir_enum_data *dd = (dir_enum_data*)dh;
   FRESULT       res;
   time_t        tmv;
   char     *nameptr;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!dd || !de) return E_SYS_ZEROPTR;
   if (dd->sign!=VFATDIR_SIGN) return E_SYS_INVPARM;

   dd->fi.lfname = dd->lfn;
   dd->fi.lfsize = sizeof(dd->lfn);
   dd->lfn[0]    = 0;

   res     = f_readdir(&dd->dir,&dd->fi);
   nameptr = *dd->fi.lfname?dd->fi.lfname:dd->fi.fname;
   // no file or error?
   if (res!=FR_OK || !*nameptr) {
      if (res==FR_OK) res=FR_NO_FILE;
      return errcvt(res);
   }
   de->attrs  = dd->fi.fattrib;
   de->fileno = 0;
   de->size   = dd->fi.fsize;
   de->dir    = dd->dirname;

   if (*dd->fi.lfname) {
      strncpy(de->name,dd->fi.lfname,QS_MAXPATH+1);
   } else {
      strncpy(de->name,dd->fi.fname,13);
      de->name[12] = 0;
   }
   de->name[QS_MAXPATH] = 0;

   tmv = dostimetotime((u32t)dd->fi.fdate<<16|dd->fi.ftime);
   io_timetoio(&de->atime, tmv);
   io_timetoio(&de->wtime, tmv);
   tmv = dostimetotime((u32t)dd->fi.cdate<<16|dd->fi.ctime);
   io_timetoio(&de->ctime, tmv);

   return 0;
}

static qserr _std fat_dirclose(void *data, dir_handle_int dh) {
   dir_enum_data *dd = (dir_enum_data*)dh;
   FRESULT       res;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!dd) return E_SYS_ZEROPTR;
   if (!dd->sign!=VFATDIR_SIGN) return E_SYS_INVPARM;

   dd->sign = 0;
   res = f_closedir(&dd->dir);
   free(dd);

   return errcvt(res);
}

static qserr _std fat_volinfo(void *data, disk_volume_data *info) {
   FATFS   **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!info) return E_SYS_ZEROPTR;
   memset(info, 0, sizeof(disk_volume_data));

   if (vd->vol>=0) {
      vol_data  *vdta = _extvol+vd->vol;
      FATFS     *fdta = extdrv[vd->vol];
      // mounted?
      if (vd->vol==DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
         info->StartSector  = vdta->start;
         info->TotalSectors = vdta->length;
         info->Disk         = vdta->disk;
         info->SectorSize   = vdta->sectorsize;
         // was it really mounted?
         if ((vdta->flags&VDTA_FAT)!=0) {
            FATFS *pfat = 0;
            DWORD nclst = 0;
            char     cp[3];
            cp[0] = '0'+vd->vol; cp[1]=':'; cp[2]=0;

            //info->ClSize      = fdta->csize;
            info->RootDirSize = fdta->n_rootdir;
            info->FatCopies   = fdta->n_fats;
            info->DataSector  = fdta->database;
            info->FsVer       = fdta->fs_type;
            // update info to actual one
            if (f_getfree(cp,&nclst,&pfat)==FR_OK) vdta->clfree = nclst;
            if (!vdta->serial) f_getlabel(cp, vdta->label, &vdta->serial);
         } else {
            /* Not a FAT.
               HPFS format still can fill vdta in way, compatible with
               "Format" command printing and this function will return
               it, because nobody touch it until unmount */
            info->RootDirSize = 0;
            info->FatCopies   = 0;
            info->DataSector  = 0;
         }
         info->ClBad     = vdta->badclus;
         info->SerialNum = vdta->serial;
         info->ClSize    = vdta->clsize;
         info->ClAvail   = vdta->clfree;
         info->ClTotal   = vdta->cltotal;
         strcpy(info->Label,vdta->label);
         strcpy(info->FsName,vdta->fsname);

         return 0;
      }
   }
   return E_DSK_NOTMOUNTED;
}

static qserr _std fat_setlabel(void *data, const char *label) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (vd->vol>=0) {
      vol_data  *vdta = _extvol+vd->vol;
      FATFS     *fdta = ((FATFS  **)sto_data(STOKEY_FATDATA))[vd->vol];

      // mounted & FAT?
      if (vd->vol<=DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
         FRESULT  res;
         char      lb[14];
         lb[0] = vd->vol+'0';
         lb[1] = ':';
         if (label) {
            strncpy(lb+2,label,12);
            lb[13] = 0;
         } else
            lb[2] = 0;

         res = f_setlabel(lb);
         // drop file label cache to force its reading in volinfo
         if (res==FR_OK) vdta->serial = 0;
         return errcvt(res);
      }
   }
   return E_DSK_NOTMOUNTED;
}

static void _std fat_init(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   vd->sign        = VFAT_SIGN;
   vd->vol         = -1;
   vd->self        = (qs_sysvolume)instance;
   vd->mount_ok    = 0;
}

static void _std fat_done(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   if (vd->sign==VFAT_SIGN)
      if (vd->vol>=0) log_printf("Volume %d fini???\n", vd->vol);
   memset(vd, 0, sizeof(volume_data));
}

static void *methods_list[] = { fat_initialize, fat_finalize, fat_volinfo,
   fat_setlabel, fat_drive, fat_open, fat_read, fat_write, fat_flush,
   fat_close, fat_size, fat_setsize, fat_setattr, fat_getattr, fat_setexattr,
   fat_getexattr, fat_lstexattr, fat_remove, fat_renmove, fat_makepath,
   fat_remove, fat_pathinfo, fat_setinfo, fat_diropen, fat_dirnext,
   fat_dirclose };

void register_fatio(void) {
   // something forgotten! interface part is not match to implementation
   if (sizeof(_qs_sysvolume)!=sizeof(methods_list)) {
      log_printf("Function list mismatch\n");
      _throw_(xcpt_align);
   }
   // register private(unnamed) class
   fatio_classid = exi_register(0 /*"qs_common_fatio"*/, methods_list,
      sizeof(methods_list)/sizeof(void*), sizeof(volume_data),
         fat_init, fat_done, 0);
   if (!fatio_classid)
      log_printf("unable to register FAT i/o class!\n");
}