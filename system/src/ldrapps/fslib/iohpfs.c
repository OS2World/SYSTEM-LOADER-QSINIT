//
// QSINIT "fslib" module
// HPFS filesystem r/o support
//
#include "fslib.h"
#include "sys/fs/hpfs.h"

#define HPFS_SIGN        0x4F495048   ///< HPIO string
#define HPFSDIR_SIGN     0x45445048   ///< HPDE string
#define HPFSFH_SIGN      0x48465048   ///< HPFH string
#define SPB                       4   ///< sectors per buffer
#define SECTSHIFT                 9   ///< bit shift for 512

struct _dir_data;

/// 32 bytes
typedef struct {
   u32t               size;  ///< file size
   u32t              vsize;  ///< size of valid data in the file
   u32t              ctime;
   u32t              wtime;
   u32t              fnode;
   struct _dir_data    *dd;  ///< directory data (if available & directory is)
   char              *name;
   u8t               attrs;
   u8t               nmlen;
   u16t               res2;
} dir_entry;

struct _dir_data {
   u32t               sign;
   u32t            entries;
   struct _dir_data   *pdd;  ///< parent directory data
   u32t             pindex;  ///< index in the parent directory
   u32t              usage;  ///< usage counter
   u32t              *hash;  ///< name hash array
   dir_entry         de[1];
};

typedef struct _dir_data dir_data;

typedef struct {
   u32t               sign;
   u32t               vdsk;  ///< 0 or volume|QDSK_VOLUME
   qs_sysvolume       self;  ///< ptr to self instance
   int            mount_ok;  ///< mount was ok
   int               fs_ok;  ///< filesystem is ok (or it was forced to mount)
   u32t        root_dirblk;
   u32t           codepage;  ///< codepage or 0
   hpfs_superblock    *sup;
   hpfs_spareblock    *spr;
   dir_data          *root;
   u8t         uprtab[128];
} volume_data;

typedef struct {
   u32t               sign;
   dir_data            *dd;
   u32t                pos;
   char            dirname[QS_MAXPATH+1];
} dir_enum_data;

typedef struct {
   u32t               sign;
   dir_entry           *de;
   dir_data            *dd;
   qs_filecc            fd;
   char                 fp[QS_MAXPATH+1];
} file_handle;

#define instance_ret(type,inst,err)      \
   type *inst = (type*)data;             \
   if (inst->sign!=HPFS_SIGN) return err;

#define instance_void(type,inst)         \
   type *inst = (type*)data;             \
   if (inst->sign!=HPFS_SIGN) return;

/** read codepage data.
    Just works as linux driver do - get 1st found codepage and use
    it for everything. This is wrong, but mulipage support broken in HPFS.IFS
    itself, any way */
static qserr hpfs_load_cp(volume_data *vd) {
   u8t        sbuffer[512];
   const hpfs_cpblock *cpi = (hpfs_cpblock*)&sbuffer;
   const hpfs_cpdata  *cpd = (hpfs_cpdata*) &sbuffer;
   hpfs_cpdataentry   *cde;
   u32t              index;
   // read codepage directory
   if (!hlp_diskread(vd->vdsk, vd->spr->cp_dir, 1, sbuffer)) return E_DSK_ERRREAD;
   // check signature & too large index at least
   if (cpi->header!=HPFS_CPDIR_SIG || cpi->cpinfo[0].index>=HPFS_CPDATAPERSEC) {
      log_it(3, "vol %X, bad cp dir\n", vd->vdsk);
      return E_DSK_FSSTRUCT;
   }
   if (cpi->cpinfo[0].dbcsranges) return E_SYS_UNSUPPORTED;
   index = cpi->cpinfo[0].index;
   // buffer is shared, so cpi overwritten below
   if (!hlp_diskread(vd->vdsk, cpi->cpinfo[0].cpdata, 1, sbuffer)) return E_DSK_ERRREAD;
   // check offset (as well as linux driver)
   if (cpd->header!=HPFS_CPDAT_SIG || cpd->dataofs[index] > 512-sizeof(hpfs_cpdataentry)) {
      log_it(3, "vol %X, bad cp data\n", vd->vdsk);
      return E_DSK_FSSTRUCT;
   }

   cde = (hpfs_cpdataentry*)(sbuffer+cpd->dataofs[index]);

   vd->codepage = cde->codepage;
   memcpy(&vd->uprtab, &cde->uprtab, 128);

   return 0;
}

/** try to free directory and all its children.
    call free only for directories with zero usage count.
    @return 1 on success */
static int hpfs_free_dir(dir_data *dd) {
   u32t ii, rc = 1;
   if (!dd) return 1;

   for (ii=0; ii<dd->entries; ii++) {
      dir_entry *de = dd->de + ii;
      if (de->attrs&IOFA_DIR) {
         int rrc = hpfs_free_dir(de->dd);
         if (rrc) de->dd = 0;
         rc &= rrc;
      }
   }
   if (rc)
      if (dd->usage) rc = 0; else {
         dd->sign = 0;
         free(dd);
      }
   return rc;
}

static u32t _std data_cache_notify(u32t code, void *usr) {
   volume_data *vd = (volume_data*)usr;
   if (vd->sign!=HPFS_SIGN) return DCNR_GONE;

   if (vd->root) {
      log_it(3, "vol %X mem free\n", vd->vdsk);
      hpfs_free_dir(vd->root);
      vd->root = 0;
   }
   return DCNR_SAFE;
}

static qserr _exicc hpfs_initialize(EXI_DATA, u8t vol, u32t flags, void *bootsec) {
   vol_data         *vdta = _extvol+vol;
   qserr              err = E_DSK_UNKFS;
   struct Boot_Record *br = (struct Boot_Record*)bootsec;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   vd->mount_ok = 0;
   vd->fs_ok    = 0;
   vd->codepage = 0;
   vd->root     = 0;

   if (vdta->sectorsize!=512) err = E_DSK_SSIZE; else
   if (vdta->length) {
      vd->vdsk     = vol|QDSK_VOLUME;
      if (memcmp(br->BR_EBPB.EBPB_FSType,"HPFS",4)==0) {
         vd->sup = (hpfs_superblock*)malloc(512*2);
         vd->spr = (hpfs_spareblock*)((u8t*)vd->sup + 512);
         // read super block & spare block
         if (hlp_diskread(QDSK_VOLUME|vol,HPFSSEC_SUPERB,2,vd->sup)!=2)
            err = E_DSK_ERRREAD;
         if (vd->sup->header1==HPFS_SUP_SIGN1 && vd->sup->header2==HPFS_SUP_SIGN2 &&
             vd->spr->header1==HPFS_SPR_SIGN1 && vd->spr->header2==HPFS_SPR_SIGN2)
         {
            hpfs_fnode *root_fn = (hpfs_fnode*)malloc_thread(512);
            // read root fnode
            if (!hlp_diskread(QDSK_VOLUME|vol,vd->sup->rootfn,1,root_fn))
               err = E_DSK_ERRREAD; else
            if (vdta->length<vd->sup->sectors) err = E_DSK_MOUNTERR; else {
               vdta->cltotal   = vd->sup->sectors;
               vdta->serial    = br->BR_EBPB.EBPB_VolSerial;
               vd->fs_ok       = 1;
               memcpy(vdta->label, br->BR_EBPB.EBPB_VolLabel, 11);
               vdta->label[11] = 0;
               trimright(vdta->label, " ");
               vd->root_dirblk = root_fn->fst.aall[0].phys;
               // TRY to load codepage information
               if (vd->spr->codepages) hpfs_load_cp(vd);
               err = 0;

               log_it(3, "vol %X, cp %u, rootblk %LX\n", vd->vdsk, vd->codepage,
                  vdta->start+vd->root_dirblk);
            }
            free(root_fn);
         }
      }
   }
   if (!err) {
      vdta->clsize = 1;
      vd->mount_ok = 1;
      strcpy(vdta->fsname, "HPFS");
      sys_dcenew(vd, DCF_NOTIMER, 0, data_cache_notify);
   } else
      vd->vdsk = 0;
   return err;
}

static void hpfs_freedata(volume_data *vd) {
   if (vd->root) {
      if (!hpfs_free_dir(vd->root))
         log_printf("Vol %X free dir failed\n", vd->vdsk);
      vd->root = 0;
   }

   sys_dcedel(vd);

   if (vd->sup) { free(vd->sup); vd->sup = 0; }
   vd->vdsk     = 0;
   vd->mount_ok = 0;
   vd->fs_ok    = 0;
}

/// unmount volume
static qserr _exicc hpfs_finalize(EXI_DATA) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!vd->vdsk) return E_DSK_NOTMOUNTED; else {
      hpfs_freedata(vd);
      return 0;
   }
}

static int _exicc hpfs_drive(EXI_DATA) {
   instance_ret(volume_data, vd, -1);
   return vd->vdsk ? vd->vdsk&QDSK_DISKMASK : -1;
}

static char *hpfs_cvtpath(volume_data *vd, const char *fp) {
   if (fp[1]!=':' || fp[0]-'A'!=(vd->vdsk&QDSK_DISKMASK)) return 0; else {
      char *rc = strdup(fp);
      mem_shareblock(rc);
      return rc;
   }
}

static inline u8t hpfs_toupper(volume_data *vd, u8t ch) {
   if (ch<128) ch = toupper(ch); else
      if (vd->codepage) ch = vd->uprtab[ch-128];
   return ch;
}

/** calculate name hash for UPPERcased name.
    @param   vd           volume data (for uppercase table)
    @param   name         name to calc
    @param   len          # of chars from name to use (or 0)
    @return hash value */
static u32t calc_hash(volume_data *vd, char *name, u32t len) {
   u32t res = 0;
   if (!len) len = FFFF;
   while (*name && len--) {
      res += hpfs_toupper(vd, *name++);
      res = (res>>2) + (res<<30);
   }
   return res;
}

/** read entire FNODE allocation into array of qwords.
    @param   vd           volume data
    @param   loc          target array (dword order compatible with cc_loc)
    @param   secnext      service dword for recursive processing
    @param   src          file storage header from FNODE
    @return error code or 0 */
static qserr read_node(volume_data *vd, dq_list loc, u32t *secnext,
                       hpfs_filestorage *src)
{
   u32t  cnt = src->alb.used, ii;
   qserr res = 0;

   if (src->alb.flag & HPFS_ABF_NODE) {
      hpfs_alsec *als = (hpfs_alsec*)malloc_thread(512);
      if (!als) return E_SYS_NOMEM;

      for (ii=0; !res && ii<cnt; ii++) {
         u32t sector = src->aaln[ii].phys;
         // ASUS BIOS can read nothing without error from USB hdd above 128Gb
         als->header = 0;
         // read sector
         if (!hlp_diskread(vd->vdsk, sector, 1, als))
            { res = E_DSK_ERRREAD; break; }
         // check signature, at least
         if (als->header!=HPFS_ANODE_SIG) {
            log_it(3, "vol %X, AL sector %0X bad sig\n", vd->vdsk, sector);
            res = E_DSK_FSSTRUCT;
         }
         if (!res)
            res = read_node(vd, loc, secnext, (hpfs_filestorage*)&als->alb);
      }
      free(als);
   } else {
      for (ii=0; !res && ii<cnt; ii++) {
         hpfs_alleaf *al = src->aall + ii;
         u64t      value = (u64t)al->runlen<<32 | al->phys;
         // logical sector should be valid
         if (al->log!=*secnext) return E_DSK_FILEALLOC;
         *secnext += al->runlen;
         loc->add(value);
      }
   }
   return res;
}

#define next_dirent(de)  de = (hpfs_dirent*)((u8t*)de + de->recsize)
#define first_dirent(db) (hpfs_dirent*)((u8t*)(db) + offsetof(hpfs_dirblk,startb))

/** read entire directory into the linear list.
    @param   vd           volume data
    @param   dirblk       top DIRBLK sector of this directory
    @param   [out] dlist  directory data (in the single heap block) in module
                          owned heap block
    @return error code or 0 */
static qserr read_dir(volume_data *vd, u32t dirblk, dir_data **dlist) {
   ptr_list     pl = NEW(ptr_list);
   hpfs_dirblk *db = (hpfs_dirblk*)malloc_thread(SPB*512);
   dir_data    *dd = 0;
   qserr       res = 0;
   u32t    allocsz = 0,
          alloccnt = 0;
   int        step;

//log_it(3, "read_dir (,%X,)\n", dirblk);
   pl->add(db);

   if (hlp_diskread(vd->vdsk,dirblk,SPB,db)!=SPB) res = E_DSK_ERRREAD; else
      if (db->sig!=HPFS_DNODE_SIG) res = E_DSK_FSSTRUCT;
   // just to be safe in check below
   db->change|=1;

   for (step=0; !res && step<2; step++) {
      hpfs_dirent *de = 0;
      int         eof = 0;
      char  *namedata = 0;
      u32t   resindex = 0;
      //log_it(3, "read_dir (,%X,%X): pass %u\n", dirblk, dlist, step);
      if (step) {
         u32t    len = sizeof(dir_data) + sizeof(dir_entry)*alloccnt + 4*alloccnt + allocsz;
         dd          = (dir_data*)malloc(len);
         dd->sign    = HPFSDIR_SIGN;
         dd->entries = alloccnt;
         dd->usage   = 0;
         dd->pdd     = 0;
         dd->pindex  = 0;
         dd->hash    = (u32t*)((u8t*)(dd+1) + sizeof(dir_entry)*(alloccnt-1));
         namedata    = (char*)(dd->hash + alloccnt);
      }
      while (!res) {
         if (!de) de = first_dirent(db);
         if (de->namelen && (de->flags&HPFS_DF_END)==0)
            if (!step) { allocsz+=de->namelen+1; alloccnt++; } else {
               dir_entry *dne = dd->de + resindex;
               dne->size  = de->fsize;
               dne->vsize = de->fsize;
               dne->ctime = de->time_create;
               dne->wtime = de->time_mod;
               dne->fnode = de->fnode;
               dne->attrs = de->attrs;
               dne->nmlen = de->namelen;
               dne->dd    = 0;
               dne->name  = namedata;

               memcpy(namedata, &de->name, de->namelen);
               namedata[de->namelen] = 0;
               // change \1 to '.'
               if (de->flags&HPFS_DF_SPEC) replacechar(namedata, 1, '.');
               // hash saved for uppercased name
               dd->hash[resindex] = calc_hash(vd, namedata, 0);

               namedata  += de->namelen+1;
               resindex++;
            }
         if (de->flags&HPFS_DF_BTP) {
            u32t *ptr = (u32t*)((u8t*)de + de->recsize - 4);
            if (step) db = *(hpfs_dirblk**)ptr; else {
               hpfs_dirblk *ndb = (hpfs_dirblk*)malloc_thread(SPB*512);
               u32t        ldtp = *ptr;
               *ptr = (u32t)ndb;
               if (hlp_diskread(vd->vdsk,ldtp,SPB,ndb)!=SPB) res = E_DSK_ERRREAD; else
                  if (db->sig!=HPFS_DNODE_SIG) res = E_DSK_FSSTRUCT; else {
                     ndb->parent = (u32t)db;
                     pl->add(db = ndb);
                  }
               //log_it(3, "BTP: read %X to %X = %X\n", ldtp, ndb, res);
            }
            if (res) break;
            de = 0;
            continue;
         } else
         while (de->flags&HPFS_DF_END)
            // the end of topmost block
            if (db->change&1) { eof=1; break; } else {
               u32t pv = (u32t)db;
               db = (hpfs_dirblk*)db->parent;
               de = first_dirent(db);
               // walk to continue point in parent
               while (*(u32t*)((u8t*)de+de->recsize-4)!=pv) next_dirent(de);
            }
         if (eof) break;
         next_dirent(de);
      }
   }
   // release DIRBLK list
   for (step=0; step<pl->count(); step++) free(pl->value(step));
   DELETE(pl);

   if (!res) *dlist = dd; else
      if (dd) free(dd);

   return res;
}

static void usage_modify(dir_data *dd, int incr) {
   while (dd) {
      if (incr) dd->usage++; else dd->usage--;
      dd = dd->pdd;
   }
}

/** walk over tree to the selected path.
    @param   vd           volume data
    @param   path         full path (accepts '\' only)
    @param   [out] dd     directory data
    @param   [out] index  0 means directory read, else it return index of
                          dir_entry in parent (even for directory!) and parent
                          directory in dd.
    @return error code or 0 */
static qserr follow_path(volume_data *vd, char *path, dir_data **dd, int *index) {
   qserr     res = 0;
   dir_data *cdd = 0;
   char      *cp = path+2;
   // path MUST be full
   if (path[1]!=':' || path[0]-'A'!=(vd->vdsk&QDSK_DISKMASK)) return E_SYS_SOFTFAULT;

   if (!vd->root) res = read_dir(vd, vd->root_dirblk, &vd->root);
   // is it root dir?
   if (!res) {
      while (*cp=='\\') cp++;
      if (*cp==0) {
         if (index) return E_SYS_INVNAME;
         *dd = vd->root;
         return 0;
      }
      cdd = vd->root;
      // directory read
      do {
         char   *ce = cp;
         u32t   idx = 0, nhash, nmlen, *npos;

         while (*ce && *ce!='\\') ce++;
         nmlen = ce-cp;
         nhash = calc_hash(vd, cp, nmlen);
         // loop over directory
         do {
            u32t *pnpos = cdd->hash + idx;
            npos = memchrd(pnpos, nhash, cdd->entries - idx);
            if (!npos) return E_SYS_NOPATH;
            idx  = npos - cdd->hash;

            if (nmlen==cdd->de[idx].nmlen) {
               char *fnp = cdd->de[idx].name, *cpp = cp;
               while (cpp!=ce)
                  if (hpfs_toupper(vd,*cpp++)!=hpfs_toupper(vd,*fnp++)) break;
               // name match!
               if (cpp==ce) {
                  cp = ce;
                  while (*cp=='\\') cp++;
                  // reach the end?
                  if (!*cp && index) *index = idx; else
                  if ((cdd->de[idx].attrs&IOFA_DIR)==0) res = E_SYS_NOPATH; else
                  if (cdd->de[idx].dd) cdd = cdd->de[idx].dd; else {
                     hpfs_fnode *dn = (hpfs_fnode*)malloc_thread(512);
                     dir_data  *cdn;
                     u32t     fnnum;
                     // read directory fnode
                     res   = hlp_diskread(vd->vdsk, cdd->de[idx].fnode, 1, dn) ?0:E_DSK_ERRREAD;
                     fnnum = dn->fst.aall[0].phys;
                     free(dn);
                     // read directory
                     if (!res) res = read_dir(vd, fnnum, &cdn);
                     // next level
                     if (!res) {
                        cdd->de[idx].dd = cdn;
                        cdn->pdd    = cdd;
                        cdn->pindex = idx;
                        cdd = cdn;
                     }
                  }
                  if (!*cp && !res) *dd = cdd;
                  break;
               }
            }
            if (++idx>=cdd->entries) res = E_SYS_NOPATH;
         } while (!res);
      } while (!res && *cp);
   }
//  log_it(3, "follow_path(%s, %X, %i) = %X\n", path, *dd, index?(int)index:-1, res);
   return res;
}

static qserr _exicc hpfs_open(EXI_DATA, const char *name, u32t mode,
                             io_handle_int *pfh, u32t *action)
{
   file_handle   *fh = 0;
   dir_data      *dd;
   char       *cpath;
   qserr         res;
   int         index;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (mode>SFOM_CREATE_ALWAYS) return E_SYS_INVPARM;
   if (!pfh || !name || !action) return E_SYS_ZEROPTR;
   if (strlen(name)>QS_MAXPATH) return E_SYS_LONGNAME;
   // deny any file overwrite
   if (mode>SFOM_OPEN_ALWAYS) return E_SYS_ACCESS;

   cpath = hpfs_cvtpath(vd, name);
   if (!cpath) return E_SYS_INVPARM;
   *pfh  = 0;
   res   = follow_path(vd, cpath, &dd, &index);

   if (!res) {
      dir_entry *de = &dd->de[index];
      if (de->attrs&ATTR_DIRECTORY) res = E_SYS_ACCESS; else {
         hpfs_fnode *fn = malloc_thread(512);

         if (!hlp_diskread(vd->vdsk,de->fnode,1,fn)) res = E_DSK_ERRREAD; else
            if (fn->header!=HPFS_FNODE_SIG || (fn->flag&HPFS_FNF_DIR))
               res = E_DSK_FSSTRUCT;
         else {
            dq_list   loc = NEW(dq_list);
            u32t    snext = 0;
            // update valid data size from FNODE
            if (fn->fst.vdlen<de->vsize) de->vsize = fn->fst.vdlen;
            // collect file location into the list
            res = read_node(vd, loc, &snext, &fn->fst);
            if (!res) {
               qs_filecc fd = NEW_G(qs_filecc);
               // setup file cache in r/o mode
               res = fd->setdisk(0,0,0,0, vd->vdsk, 0, vd->sup->sectors, SECTSHIFT);

               if (!res) {
                  io_handle_info  fi;
                  fi.attrs = de->attrs;
                  fi.size  = de->size;
                  fi.vsize = de->vsize;
                  loc->add(0);
                  res = fd->init(&fi, (cc_loc*)loc->array(), FCCF_READONLY);

                  if (!res) {
                     fh = (file_handle*)malloc(sizeof(file_handle));
                     fh->sign = HPFSFH_SIGN;
                     fh->de   = de;
                     fh->dd   = dd;
                     fh->fd   = fd;
                     strcpy(fh->fp, name);
                     usage_modify(dd,1);
                     *pfh = (io_handle_int)fh;
                  }
               }
               if (res) DELETE(fd);
            }
            DELETE(loc);
         }
         free(fn);
      }
   }
   free(cpath);
   if (!res) *action = IOFN_EXISTED;

   return res?res:E_SYS_READONLY;
}

static qserr _exicc hpfs_read(EXI_DATA, io_handle_int file, u64t pos,
                             void *buffer, u32t *size)
{
   file_handle   *fh = (file_handle*)file;
   if (!buffer || !size) return E_SYS_ZEROPTR;
   if (fh->sign!=HPFSFH_SIGN || pos>=fh->de->size) return E_SYS_INVPARM;
   if (!*size) return 0;
   return fh->fd->read(pos, buffer, *size, size);
}

static qserr _exicc hpfs_write(EXI_DATA, io_handle_int file, u64t pos,
                              void *buffer, u32t *size)
{
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_ACCESS;
}

static qserr _exicc hpfs_flush(EXI_DATA, io_handle_int file) {
   file_handle   *fh = (file_handle*)file;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=HPFSFH_SIGN) return E_SYS_INVPARM;
   return E_SYS_ACCESS;
}

static qserr _exicc hpfs_close(EXI_DATA, io_handle_int file) {
   file_handle   *fh = (file_handle*)file;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=HPFSFH_SIGN) return E_SYS_INVPARM;
   fh->fd->close();
   DELETE(fh->fd);
   usage_modify(fh->dd,0);
   fh->sign = 0;
   free(fh);
   return 0;
}

static qserr _exicc hpfs_size(EXI_DATA, io_handle_int file, u64t *size) {
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=HPFSFH_SIGN) return E_SYS_INVPARM;
   if (!size) return E_SYS_ZEROPTR;
   *size = fh->de->size;
   return 0;
}

static qserr _exicc hpfs_setsize(EXI_DATA, io_handle_int file, u64t newsize) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc hpfs_finfo(EXI_DATA, io_handle_int file, io_direntry_info *info) {
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=HPFSFH_SIGN) return E_SYS_INVPARM;
   if (!info) return E_SYS_ZEROPTR;
   // a bit faster than function below
   info->attrs   = fh->de->attrs;
   info->fileno  = 0;
   info->size    = fh->de->size;
   info->vsize   = fh->de->vsize;
   io_timetoio(&info->atime, fh->de->wtime);
   io_timetoio(&info->wtime, fh->de->wtime);
   io_timetoio(&info->ctime, fh->de->ctime);
   strcpy(info->name, fh->fp);
   info->dir     = 0;

   return 0;
}

static qserr _exicc hpfs_pathinfo(EXI_DATA, const char *path, io_handle_info *info) {
   qserr         res;
   char       *cpath;
   int         index;
   dir_data      *dd;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;

   cpath = hpfs_cvtpath(vd, path);
   if (!cpath) return E_SYS_INVPARM;
   res   = follow_path(vd, cpath, &dd, &index);
   free(cpath);

   if (!res) {
      dir_entry *de = &dd->de[index];
      info->attrs   = de->attrs;
      info->fileno  = 0;
      info->size    = de->size;
      info->vsize   = de->size;
      io_timetoio(&info->atime, de->wtime);
      io_timetoio(&info->wtime, de->wtime);
      io_timetoio(&info->ctime, de->ctime);
   }
   return res;
}

static qserr _exicc hpfs_setinfo(EXI_DATA, const char *path, io_handle_info *info, u32t flags) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc hpfs_setexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc hpfs_getexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t *size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static str_list* _exicc hpfs_lstexattr(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, 0);
   return 0;
}

// warning! it acts as rmdir too!!
static qserr _exicc hpfs_remove(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc hpfs_renmove(EXI_DATA, const char *oldpath, const char *newpath) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!oldpath || !newpath) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc hpfs_makepath(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc hpfs_diropen(EXI_DATA, const char *path, dir_handle_int *pdh) {
   dir_enum_data  *ed;
   char        *cpath = 0;
   qserr          res;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !pdh) return E_SYS_ZEROPTR;
   *pdh = 0;
   if (strlen(path)>QS_MAXPATH) return E_SYS_LONGNAME;
   // dir handle storage
   ed = (dir_enum_data*)malloc(sizeof(dir_enum_data));
   if (!ed) return E_SYS_NOMEM;
   strcpy(ed->dirname, path);
   cpath = hpfs_cvtpath(vd, path);
   if (!cpath) res = E_SYS_INVPARM; else {
      dir_data    *dd;
      res = follow_path(vd, cpath, &dd, 0);
      free(cpath);

      if (!res) {
         ed->sign = HPFSDIR_SIGN;
         ed->dd   = dd;
         ed->pos  = 0;
         *pdh     = (dir_handle_int)ed;
         usage_modify(dd,1);
      }
   }
   if (res && ed) free(ed);
   return res;
}

static qserr _exicc hpfs_dirnext(EXI_DATA, dir_handle_int dh, io_direntry_info *info) {
   dir_enum_data *ed = (dir_enum_data*)dh;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!ed || !info) return E_SYS_ZEROPTR;
   if (ed->sign!=HPFSDIR_SIGN) return E_SYS_INVPARM;

   if (ed->pos>=ed->dd->entries) return E_SYS_NOFILE; else {
      dir_entry *de = &ed->dd->de[ed->pos++];
      info->attrs   = de->attrs;
      info->fileno  = 0;
      info->size    = de->size;
      info->vsize   = de->size;
      info->dir     = ed->dirname;
      io_timetoio(&info->atime, de->wtime);
      io_timetoio(&info->wtime, de->wtime);
      io_timetoio(&info->ctime, de->ctime);
      strcpy(info->name, de->name);
   }
   return 0;
}

static qserr _exicc hpfs_dirclose(EXI_DATA, dir_handle_int dh) {
   dir_enum_data *ed = (dir_enum_data*)dh;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!ed) return E_SYS_ZEROPTR;
   if (ed->sign!=HPFSDIR_SIGN) return E_SYS_INVPARM;

   ed->sign = 0;
   usage_modify(ed->dd,0);
   free(ed);
   return 0;
}

static qserr _exicc hpfs_volinfo(EXI_DATA, disk_volume_data *info, int fast) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!info) return E_SYS_ZEROPTR;
   memset(info, 0, sizeof(disk_volume_data));

   if (vd->vdsk) {
      vol_data *vdta = _extvol + (vd->vdsk&QDSK_DISKMASK);
      // mounted?
      if (vdta->flags&VDTA_ON) {
         info->StartSector  = vdta->start;
         info->TotalSectors = vdta->length;
         info->Disk         = vdta->disk;
         info->SectorSize   = vdta->sectorsize;
         info->FSizeLim     = x7FFF;
         info->ClBad        = vdta->badclus;
         info->SerialNum    = vdta->serial;
         info->ClSize       = vdta->clsize;
         //info->ClAvail      = 0;
         info->ClTotal      = vdta->cltotal;
         info->FsVer        = FST_OTHER;
         info->InfoFlags    = VIF_READONLY;
         //info->Reserved     = 0;
         info->DataSector   = HPFSSEC_SUPERB + 4;
         //info->OemData      = 0;
         strcpy(info->Label,vdta->label);
         strcpy(info->FsName,vdta->fsname);
         return 0;
      }
   }
   return E_DSK_NOTMOUNTED;
}

static qserr _exicc hpfs_setlabel(EXI_DATA, const char *label) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   /* allow set label even if we`re read-only driver, because it safe and
      does not touch any filesystem structures */
   if (vd->vdsk) {
      u8t          sbuf[512];
      char            cn[12];
      struct Boot_Record *br = (struct Boot_Record*)&sbuf;
      vol_data         *vdta = _extvol + (vd->vdsk&QDSK_DISKMASK);
      u32t               len = strlen(label);
      if (len>11) return E_SYS_LONGNAME;

      if (len<11) memset(&cn, ' ', 11);
      cn[11] = 0;
      while (len--) {
         cn[len] = toupper(label[len]);
         if (strchr("+.,;=[]\"*:<>\?|\x7F",cn[len]) || cn[len]>=128)
            return E_SYS_INVNAME;
      }
      if (!hlp_diskread(vd->vdsk, 0, 1, br)) return E_DSK_ERRREAD;
      memcpy(br->BR_EBPB.EBPB_VolLabel, cn, 11);
      if (!hlp_diskwrite(vd->vdsk, 0, 1, br)) return E_DSK_ERRWRITE;
      // update own saved string
      strcpy(vdta->label, cn);
      trimright(vdta->label, " ");
      return 0;
   }
   return E_DSK_NOTMOUNTED;
}

static u32t _exicc hpfs_avail(EXI_DATA) { return SFAF_FINFO; }

static void _std hpfs_init(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   vd->sign        = HPFS_SIGN;
   vd->vdsk        = 0;
   vd->self        = (qs_sysvolume)instance;
}

static void _std hpfs_done(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   if (vd->sign==HPFS_SIGN)
      if (vd->vdsk) log_printf("Vol %X fini???\n", vd->vdsk);
   hpfs_freedata(vd);
   memset(vd, 0, sizeof(volume_data));
}

static void *m_list[] = { hpfs_initialize, hpfs_finalize, hpfs_avail,
   hpfs_volinfo, hpfs_setlabel, hpfs_drive, hpfs_open, hpfs_read, hpfs_write,
   hpfs_flush, hpfs_close, hpfs_size, hpfs_setsize, hpfs_finfo, hpfs_setexattr,
   hpfs_getexattr, hpfs_lstexattr, hpfs_remove, hpfs_renmove, hpfs_makepath,
   hpfs_remove, hpfs_pathinfo, hpfs_setinfo, hpfs_diropen, hpfs_dirnext,
   hpfs_dirclose };

u32t register_hpfs(void) {
   u32t cid;
   // something forgotten! interface part is not match to the implementation
   if (sizeof(_qs_sysvolume)!=sizeof(m_list)) {
      log_printf("hpfs: function list mismatch\n");
      _throw_(xcpt_align);
   }
   cid = exi_register("qs_sys_hpfsio", m_list, sizeof(m_list)/sizeof(void*),
                       sizeof(volume_data), EXIC_PRIVATE, hpfs_init, hpfs_done, 0);
   if (cid) {
      qserr res = io_registerfs(cid);
      if (res) {
         log_printf("hpfs: regfs err %X!\n", res);
         exi_unregister(cid);
         cid = 0;
      }
   }
   return cid;
}
