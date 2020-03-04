//
// QSINIT "fslib" module
// JFS filesystem r/o support
// ------------------------------------------------------------
// no sparse file support and no compression, of course
//
#include "fslib.h"
#include "sys/fs/jfs.h"
#include "wchar.h"

#define JFS_SIGN        0x4F49464A   ///< JFIO string
#define JFSDIR_SIGN     0x4544464A   ///< JFDE string
#define JFSFH_SIGN      0x4846464A   ///< JFFH string

struct _dir_data;

/// directory entry data (32 bytes)
typedef struct {
   u64t               size;  ///< file size
   u32t              ctime;
   u32t              wtime;
   u32t              inode;
   struct _dir_data    *dd;  ///< directory data (if available & directory is)
   wchar_t           *name;  ///< name[nmlen] \0 uppercased name[nmlen] \0
   u8t               attrs;
   u8t               nmlen;
   u8t              islink;  ///< just a flag to return error on any access
   u8t                 res;
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
   int               fs_ok;  ///< filesystem is ok

   u8t               shift;  ///< disk sector shift
   u8t             pgshift;  ///< page -> sector shift
   u8t             clshift;  ///< cluster -> sector shift
   u8t            reserved;
   
   u32t              nclus;  ///< # of clusters on disk

   jfs_superblock      *sb;
   qs_filecc        finode;  ///< inode file

   jfs_dinode       *iroot;  ///< root dir inode
   dir_data          *root;  ///< root dir directory data
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
   if (inst->sign!=JFS_SIGN) return err;

#define instance_void(type,inst)         \
   type *inst = (type*)data;             \
   if (inst->sign!=JFS_SIGN) return;

typedef struct {
   jfs_xad         *xad;
   u32t           index,
              lastindex;
   qserr            err;
   jfs_xtreepage      p;  // 4k!
} extent_state;

/** get first extent.
    @param   di    inode to read
    @param   st    state buffer for tree recursion, to be sent to next_extent().
    @return allocation data, physically stored in di/st or 0 (error code in st->err) */
static jfs_xad* first_extent(volume_data *vd, jfs_dinode *di, extent_state *st) {
   jfs_xtreepage *xtp = (jfs_xtreepage*)&di->xtroot;

   st->index = XTENTRYSTART;
   st->xad   = &xtp->xad[XTENTRYSTART];
   st->err   = 0;

   if (xtp->header.flag&BT_LEAF) st->lastindex = xtp->header.nextindex; else {
      do {
         if (hlp_diskread(vd->vdsk, xad_addr(st->xad)<<vd->clshift, 1<<vd->pgshift,
            &st->p) != 1<<vd->pgshift)
         {
            st->err = E_DSK_ERRREAD;
            return 0;
         }
         st->xad = &st->p.xad[XTENTRYSTART];
      } while ((st->p.header.flag&BT_LEAF)==0);
      st->lastindex = st->p.header.nextindex;
   }
   return st->xad;
}

static jfs_xad* next_extent(volume_data *vd, extent_state *st) {
   if (++st->index<st->lastindex) return ++st->xad;
   if (st->p.header.next) {
      if (hlp_diskread(vd->vdsk, st->p.header.next<<vd->clshift, 1<<vd->pgshift,
         &st->p) != 1<<vd->pgshift)
      {
         st->err = E_DSK_ERRREAD;
         return 0;
      }
      st->lastindex  = st->p.header.nextindex;
      st->index      = XTENTRYSTART;
      return st->xad = &st->p.xad[XTENTRYSTART];
   } else {
      st->err = E_SYS_EOF;
      return 0;
   }
}

static wchar_t char_to_wu(char cc) {
   return (u8t)cc>=0x80 ? ff_wtoupper(ff_convert((u8t)cc,1)) : toupper(cc);
}

/** calculate name hash for UPPERcased name.
    @param   name         name to calc
    @param   len          # of chars from name to use (or 0)
    @return hash value */
static u32t calc_hash(const char *name, u32t len) {
   u32t res = 0;
   if (!len) len = x7FFF;
   while (*name && len--) {
      res += char_to_wu(*name++);
      res = (res>>2) + (res<<30);
   }
   return res;
}

/// the same as calc_hash() but for *uppercased* unicode string
static u32t calc_hash_wu(const wchar_t *name) {
   u32t res = 0;
   while (*name) {
      res += *name++;
      res = (res>>2) + (res<<30);
   }
   return res;
}

static void fd_getname(dir_entry *de, char *dst) {
   u32t ii;
   for (ii=0; ii<de->nmlen; ii++) {
      u16t ch = ff_convert(de->name[ii],0);
      dst[ii] = ch?ch:'?';
   }
   dst[de->nmlen] = 0;
}

static u32t cvt_attr(u32t jattr) {
   u32t rc = jattr>>ATTRSHIFT & (IOFA_READONLY|IOFA_HIDDEN|IOFA_SYSTEM|IOFA_DIR|IOFA_ARCHIVE);
   if (jattr&IFDIR) rc|=IOFA_DIR;
   return rc;
}

/** opens a file.
    function converts inode into a file cache object.
    used by file open and for inode file itself */
static qserr open_file(volume_data *vd, jfs_dinode *di, qs_filecc *fcc) {
   extent_state *st;
   jfs_xad     *xad;
   dq_list      loc;
   u32t        nblk = di->di_nblocks,
                ofs = 0,
           fsize_cl = di->di_size+(1<<vd->shift+vd->clshift)-1>>(vd->shift+vd->clshift);
   qserr        res;

   //log_it(3, "size %Lu, %u\n", di->di_size, nblk);
   /* allocated size<file size? (this point should deny sparse files?)
      Tried exact check first, but native JFS code may store nblk greater,
      than inode file size ... */
   if (fsize_cl>nblk) {
      log_it(3, "vol %X: %u clusters, should be %u\n", vd->vdsk, nblk, fsize_cl);
      return E_DSK_FILESIZE;
   }
   /* i have several partitions with wrong nblk for INODE FILE itself (+1 more),
      but real number of allocated blocks is equal to the file size */
   if (nblk>fsize_cl) nblk = fsize_cl;

   st  = (extent_state*)malloc_th(sizeof(extent_state));
   loc = NEW(dq_list);
   xad = first_extent(vd,di,st);
   res = st->err;
   while (!res && xad && nblk) {
      u64t pos = xad_addr(xad);
      u32t len = xad_len(xad);
      //log_it(3, "pos %LX ofs %X(%X) len %u\n", pos, ofs, (u32t)xad_ofs(xad), len);
      // check offset match, cluster number & length
      if (xad_ofs(xad)!=ofs || len>nblk || pos+len>vd->nclus) {
         res = E_DSK_FILEALLOC;
         break;
      }
      // start|len pair for qs_filecc
      pos  |= (u64t)len<<32;
      nblk -= len;
      ofs  += len;
      loc->add(pos);
      if (nblk)
         if ((xad=next_extent(vd,st))==0) res = st->err;
   }
   free(st);

   if (!res) {
      qs_filecc fd = NEW_G(qs_filecc);
      // setup file cache in r/o mode
      res = fd->setdisk(0,0,0,0, vd->vdsk, 0, vd->nclus, vd->shift+vd->clshift);

      if (!res) {
         io_handle_info  fi;
         fi.attrs = cvt_attr(di->di_mode);
         fi.size  = di->di_size;
         fi.vsize = di->di_size;
         loc->add(0);
         res = fd->init(&fi, fi.size?(cc_loc*)loc->array():0, FCCF_READONLY);
      }
      if (res) DELETE(fd); else *fcc = fd;
   }
   DELETE(loc);
   return res;
}

static qserr read_inode(volume_data *vd, u32t inum, jfs_dinode *di) {
   jfs_pxd pxd;
   // offset of IAG
   u64t    ofs = ((inum >> L2INOSPERIAG) << L2INOSPERIAG) + PSIZE;
   // extent # in inoext
   u32t     xd = (inum & INOSPERIAG - 1) >> L2INOSPEREXT, act;
   // read extent address
   qserr   err = vd->finode->read(ofs + offsetof(jfs_iag,inoext) + xd*sizeof(jfs_pxd),
                                  &pxd, sizeof(pxd), &act);
   if (!err) {
      u64t pos = pxd_addr(&pxd);
      if (pos>=vd->nclus) err = E_DSK_FILEALLOC; else {
         u32t iofs = (inum & INOSPEREXT - 1) << L2DISIZE;
         // starting sector
         pos <<= vd->clshift;
         // only for sector size >512
         if (vd->shift>L2DISIZE) {
            jfs_dinode *buf = (jfs_dinode*)malloc_th(1<<vd->shift);

            if (hlp_diskread(vd->vdsk, pos+(iofs>>vd->shift), 1, buf)==0)
               err = E_DSK_ERRREAD;
            else
               memcpy(di, buf + (iofs - (iofs>>vd->shift) >> L2DISIZE),
                  sizeof(jfs_dinode));
            free(buf);
         } else
         if (hlp_diskread(vd->vdsk, pos+(iofs>>vd->shift), 1, di)==0)
           err = E_DSK_ERRREAD;
      }
   }
   return err;
}


/** try to free directory and all its children.
    call free only for directories with zero usage count.
    @return 1 on success */
static int jfs_free_dir(dir_data *dd) {
   u32t ii, rc = 1;
   if (!dd) return 1;

   for (ii=0; ii<dd->entries; ii++) {
      dir_entry *de = dd->de + ii;
      if (de->attrs&IOFA_DIR) {
         int rrc = jfs_free_dir(de->dd);
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
   if (vd->sign!=JFS_SIGN) return DCNR_GONE;

   /* React on codepage change too, because directory data can be cached
      with wrong unicode->oem convertion. Anyway, just release all cached
      data is incorrect way too, it will fail on used directory. */
   if (vd->root && (code==DCN_CPCHANGE || code==DCN_MEM)) {
      log_it(3, "vol %X cache clean\n", vd->vdsk);
      if (jfs_free_dir(vd->root)) vd->root = 0;
   }
   return DCNR_SAFE;
}

static qserr _exicc jfs_initialize(EXI_DATA, u8t vol, u32t flags, void *bootsec) {
   vol_data         *vdta = _extvol+vol;
   qserr              err = E_DSK_UNKFS;
   struct Boot_Record *br = (struct Boot_Record*)bootsec;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   vd->mount_ok = 0;
   vd->fs_ok    = 0;
   vd->root     = 0;
   vd->iroot    = 0;
   vd->shift    = bsf32(vdta->sectorsize);
   vd->pgshift  = 12 - vd->shift;

   // cannot be, but just a check of vol_data
   if (vd->shift<9||vd->shift>12) err = E_DSK_SSIZE; else
   if ((u64t)vdta->length<<vd->shift >= MINJFS) {
      vd->vdsk     = vol|QDSK_VOLUME;
      vd->sb       = (jfs_superblock*)malloc(vdta->sectorsize);
      memset(vd->sb, 0, sizeof(jfs_superblock));
      // read super block
      if (!hlp_diskread(vd->vdsk, JFS_SUPER1_OFS>>vd->shift, 1, vd->sb))
         if (!hlp_diskread(vd->vdsk, JFS_SUPER2_OFS>>vd->shift, 1, vd->sb))
            err = E_DSK_ERRREAD;
      if (vd->sb->sign==JFS_SUPER_SIGN && vd->sb->version==JFS_VERSION) {
         u64t volsize = vd->sb->size;
         u32t  clsize = vd->sb->bsize>>vd->shift;

         if (vd->sb->pbsize!=vdta->sectorsize) {
            log_it(3, "JFS: vol %X, sector size mismatch %u!=%u\n", vd->vdsk,
               vd->sb->pbsize, vdta->sectorsize);
            volsize = volsize<<vd->sb->l2pbsize>>vd->shift;
         }
         // should never occurs
         if (vd->sb->size>>vd->sb->l2bfactor >= _4GBLL) err = E_PTE_LARGE; else
         /* file system block size should be >= physical block size, as IBM
            JFS header says, but clsize==0 means that it is smaller, so ignore
            such a case */
         if (vd->sb->bsize%vdta->sectorsize || volsize>vdta->length ||
            bsf32(vd->sb->bsize)!=vd->sb->l2bsize || !clsize) err = E_DSK_MOUNTERR;
         else {
            jfs_dinode *fs = (jfs_dinode*)malloc_th(vdta->sectorsize);
            // this value needed for open_file() below
            vd->nclus   = vd->sb->size>>vd->sb->l2bfactor;
            vd->clshift = bsf32(clsize);
            // read fileset inode (must be aligned to 4k because ino==16)
            if (!hlp_diskread(vd->vdsk, JFS_AITBL_OFF + FILESYSTEM_I*DISIZE >>
               vd->shift, 1, fs)) err = E_DSK_ERRREAD;
            else {
               err = open_file(vd, fs, &vd->finode);
               if (err)
                  log_it(3, "vol %X, fileset open err %X\n", vd->vdsk, err);
               else {
                  vd->iroot = (jfs_dinode*)malloc(sizeof(jfs_dinode));
                  err       = read_inode(vd, ROOT_I, vd->iroot);
               
                  if (err==0) {
                     u32t fl;
                     vd->fs_ok     = 1;
                     vdta->clsize  = clsize;
                     vdta->clfree  = 0;
                     vdta->cltotal = vd->nclus;
                     vdta->serial  = br->BR_EBPB.EBPB_VolSerial;
                     // copy volume label
                     memcpy(vdta->label, vd->sb->fpack, 11);
                     vdta->label[11] = 0;
                     trimright(vdta->label, " ");
                     
                     fl = vd->sb->flag;
                     log_it(3, "vol %X, flags%s%s%s%s, cluster %u\n", vd->vdsk,
                        fl&JFS_AIX?" AIX":"", fl&JFS_OS2?" OS2":"",
                           fl&JFS_LINUX?" Linux":"", fl&JFS_SPARSE?" sparse":"",
                              clsize<<vd->shift);
                  }
               }
            }
            free(fs);
         }
      }
   }
   if (!err) {
      vd->mount_ok = 1;
      strcpy(vdta->fsname, "JFS");
      sys_dcenew(vd, DCF_NOTIMER, 0, data_cache_notify);
   } else
      vd->vdsk = 0;
   return err;
}

static void jfs_freedata(volume_data *vd) {
   if (vd->root) {
      if (!jfs_free_dir(vd->root))
         log_printf("Vol %X free dir failed\n", vd->vdsk);
      vd->root = 0;
   }

   sys_dcedel(vd);

   if (vd->finode) { DELETE(vd->finode); vd->finode = 0; }
   if (vd->iroot) { free(vd->iroot); vd->iroot = 0; }
   if (vd->sb) { free(vd->sb); vd->sb = 0; }
   vd->vdsk     = 0;
   vd->mount_ok = 0;
   vd->fs_ok    = 0;
}

/// unmount volume
static qserr _exicc jfs_finalize(EXI_DATA) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);

   if (!vd->vdsk) return E_DSK_NOTMOUNTED; else {
      jfs_freedata(vd);
      return 0;
   }
}

static int _exicc jfs_drive(EXI_DATA) {
   instance_ret(volume_data, vd, -1);
   return vd->vdsk ? vd->vdsk&QDSK_DISKMASK : -1;
}

static char *jfs_cvtpath(volume_data *vd, const char *fp) {
   if (fp[1]!=':' || fp[0]-'A'!=(vd->vdsk&QDSK_DISKMASK)) return 0; else {
      char *rc = strdup(fp);
      mem_shareblock(rc);
      return rc;
   }
}

/** read entire directory into a linear list.
    @param   vd           volume data
    @param   di           inode
    @param   [out] dlist  directory data (in the single heap block) in module
                          owned heap block
    @return error code or 0 */
static qserr read_dir(volume_data *vd, jfs_dinode *di, dir_data **dlist) {
   dir_data       *dd = 0;
   ptr_list        pl = 0;
   qserr          res = 0;
   jfs_dtpage     *pb = 0;
   jfs_dtroot    *dtr = &di->dtroot;
   jfs_dinode     *fi = 0;
   u32t     lastindex,
                index = 0,
              namelen = 0;
   int            emb = 0;  // embedded data

   typedef struct {
      u32t      inode;
      u16t    namelen;
      u16t       name[1];
   } de_data;

   // additional check
   if ((di->di_mode&IFMT)==IFLNK) return E_SYS_INVNAME;
   if ((di->di_mode&IFMT)!=IFDIR) return E_SYS_NOPATH;

   // initialization
   do {
      if (dtr->header.flag&BT_LEAF) {
         lastindex = dtr->header.nextindex;
         emb = 1;
      } else {
         jfs_pxd      *xd;
         jfs_idtentry *ie;
         int          idx = dtr->header.stbl[0];
         // check it
         if (idx<0) { res = E_DSK_FILEALLOC; break; }

         pb = (jfs_dtpage*)malloc_th(sizeof(jfs_dtpage));
         ie = (jfs_idtentry*)pb->slot;
         xd = &((jfs_idtentry*)dtr->slot)[idx].xd;
         while (1) {
            if (hlp_diskread(vd->vdsk, pxd_addr(xd)<<vd->clshift,
               1<<vd->pgshift, pb) != 1<<vd->pgshift)
                  { res = E_DSK_ERRREAD; break; }

            if (pb->header.flag & BT_LEAF) break;
            // check for a negative value, at least
            idx = pb->header.stblindex;
            if (idx<0) { res = E_DSK_FILEALLOC; break; }
            idx = ((s8t*)&ie[idx])[0];
            if (idx<0) { res = E_DSK_FILEALLOC; break; }
            xd = &ie[idx].xd;
         }
         if (res) break;
         lastindex = pb->header.nextindex;
      }
   } while (0);

   /* HPFS reader calcs size in 1st pass and save DIRBLK in list for the 2nd.

      Here we saves de_data pointers with unicode name for _every_ entry and
      then allocates one block for entire directory.

      Should be much slower, but any way - we need to read EVERY file inode
      to get file size/attrs */
   if (!res) pl = NEW(ptr_list);

   // entry read loop
   while (res==0) {
      jfs_ldtentry *de = 0;
      de_data     *rde;
      if (!emb) {          
         // read next page
         if (index>=lastindex && pb->header.next) {
            if (hlp_diskread(vd->vdsk, pb->header.next<<vd->clshift,
               1<<vd->pgshift, pb) != 1<<vd->pgshift)
                  { res = E_DSK_ERRREAD; break; }
            lastindex = pb->header.nextindex;
            index     = 0;
         }
         if (index<lastindex) {
            jfs_ldtentry *le = (jfs_ldtentry*)pb->slot;
            int          idx = pb->header.stblindex;
            s8t       *table;

            if (idx<0) { res = E_DSK_FILEALLOC; break; }
            table = (s8t*)&le[idx];
            idx   = table[index++];
            if (idx<0) { res = E_DSK_FILEALLOC; break; }
            de    = le + idx;
         }
      } else
      if (index<lastindex) {
         int idx = dtr->header.stbl[index++];
         if (idx<0) { res = E_DSK_FILEALLOC; break; }
         de = (jfs_ldtentry*)&dtr->slot[idx];
      }
      if (res || !de) break;

      rde = (de_data*)malloc_th(sizeof(de_data) + de->namlen*2);
      if (rde) {
         u32t    nlen = de->namlen;

         rde->inode   = de->inumber;
         rde->namelen = nlen;
         rde->name[nlen] = 0;
         // name buffer size calculation
         namelen += nlen+1;

         if (de->next<0) memcpy(&rde->name, &de->name, nlen<<1); else {
            s8t       next;
            wchar_t    *wp = rde->name;
            memcpy(wp, &de->name, DTLHDRDATALEN<<1); 
            wp   += DTLHDRDATALEN;
            nlen -= DTLHDRDATALEN;
            next  = de->next;

            while (1) {
               jfs_dtslot *ds = (emb?dtr->slot:pb->slot) + next;
               next = ds->next;

               memcpy(wp, &ds->name, (next<0?nlen:DTSLOTDATALEN)<<1);
               if (next<0) break;
               wp   += DTSLOTDATALEN;
               nlen -= DTSLOTDATALEN;
            }
         }
         pl->add(rde);
      }
   }
   // convert inode/name pair into the normal directory listing
   if (!res) {
      wchar_t *namedata;
      u32t dotdot = di==vd->iroot?0:1,
              cnt = pl->count()+dotdot, ii, ic,
              len = sizeof(dir_data) + sizeof(dir_entry)*cnt + sizeof(u32t)*cnt +
                    namelen*2*2 + dotdot*3*2;
      dd          = (dir_data*)malloc(len);
      dd->sign    = JFSDIR_SIGN;
      dd->entries = cnt;
      dd->usage   = 0;
      dd->pdd     = 0;
      dd->pindex  = 0;
      dd->hash    = (u32t*)((u8t*)(dd+1) + sizeof(dir_entry)*(cnt-1));
      namedata    = (wchar_t*)(dd->hash + cnt);

      if (cnt) fi = (jfs_dinode*)malloc_th(sizeof(jfs_dinode));
      // add ".." for the subdirs
      if (dotdot) {
         static u16t *parent = (u16t*)L"..";
         dir_entry       *pd = dd->de;
         pd->size   = 0;
         pd->ctime  = di->di_ctime.tv_sec;
         pd->wtime  = di->di_mtime.tv_sec;
         pd->attrs  = IOFA_DIR;
         pd->islink = 0;
         pd->nmlen  = 2;
         pd->inode  = di->di_number;
         pd->dd     = 0;
         pd->name   = namedata;
         namedata  += 6;
         memcpy(pd->name, parent, 6);
         memcpy(pd->name+3, parent, 6);
         dd->hash[0] = calc_hash_wu(pd->name+3);
      }

      for (ii=dotdot; ii<cnt; ii++) {
         de_data   *rde = (de_data*)pl->value(ii-dotdot);
         dir_entry *cde = dd->de + ii;
         wchar_t *uname;

         res = read_inode(vd, rde->inode, fi);
         if (res) {
            cde->size   = 0;
            cde->ctime  = ERROR_TIME_T;
            cde->wtime  = ERROR_TIME_T;
            cde->attrs  = 0;
            cde->islink = 1;
            /* IGNORE (!!!) inode read error in this case, else we never
               finish reading of a directory with broken inodes.
               islink field will force error on any access to this name. */
            res = 0;
         } else 
         if ((di->di_mode&IFMT)==IFLNK) {
            cde->size   = 0;
            cde->ctime  = fi->di_ctime.tv_sec;
            cde->wtime  = fi->di_mtime.tv_sec;
            cde->attrs  = 0;
            cde->islink = 1;
         } else {
            cde->size   = fi->di_size;
            cde->ctime  = fi->di_ctime.tv_sec;
            cde->wtime  = fi->di_mtime.tv_sec;
            cde->attrs  = cvt_attr(fi->di_mode);
            cde->islink = 0;
         }
         cde->nmlen = rde->namelen;
         cde->inode = rde->inode;
         cde->dd    = 0;
         cde->name  = namedata;
         // TWO string lengths - for normal and uppercased name
         namedata  += rde->namelen+1 << 1;
         uname      = cde->name + rde->namelen + 1;
         // original name
         memcpy(cde->name, &rde->name, rde->namelen+1<<1);
         // uppercased name to speed up search (placed just after normal
         for (ic=0; ic<cde->nmlen; ic++) uname[ic] = ff_wtoupper(cde->name[ic]);
         uname[cde->nmlen] = 0;

         dd->hash[ii] = calc_hash_wu(uname);
      }
   }

   if (pl) {
      pl->freeitems(0,pl->count());
      DELETE(pl);
   }
   if (pb) free(pb);
   if (fi) free(fi);

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

   if (!vd->root) res = read_dir(vd, vd->iroot, &vd->root);
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
         nhash = calc_hash(cp, nmlen);
         // loop over directory
         do {
            u32t   *pnpos = cdd->hash + idx;
            dir_entry *de;

            npos = memchrd(pnpos, nhash, cdd->entries - idx);
            if (!npos) return E_SYS_NOPATH;
            idx  = npos - cdd->hash;
            de   = cdd->de + idx;

            if (nmlen==de->nmlen) {
               // ptr to stored uppercased name
               wchar_t *fnp = de->name + de->nmlen + 1;
               char    *cpp = cp;

               while (cpp!=ce)
                  if (char_to_wu(*cpp++)!=*fnp++) break;
               // name match!
               if (cpp==ce) {
                  cp = ce;
                  while (*cp=='\\') cp++;
                  // reach the end?
                  if (!*cp && index) *index = idx; else
                  if ((de->attrs&IOFA_DIR)==0) res = E_SYS_NOPATH; else
                  if (de->islink) res = E_SYS_ACCESS; else
                  if (de->dd) cdd = de->dd; else {
                     jfs_dinode *dn = (jfs_dinode*)malloc_th(sizeof(jfs_dinode));
                     dir_data  *cdn;
                     // read directory fnode
                     res = read_inode(vd, de->inode, dn);
                     // read directory
                     if (!res) res = read_dir(vd, dn, &cdn);
                     // next level
                     if (!res) {
                        de->dd      = cdn;
                        cdn->pdd    = cdd;
                        cdn->pindex = idx;
                        cdd = cdn;
                     }
                     free(dn);
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

static qserr _exicc jfs_open(EXI_DATA, const char *name, u32t mode,
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

   cpath = jfs_cvtpath(vd, name);
   if (!cpath) return E_SYS_INVPARM;
   *pfh  = 0;
   res   = follow_path(vd, cpath, &dd, &index);

   if (!res) {
      dir_entry *de = &dd->de[index];

      if (de->islink || (de->attrs&IOFA_DIR)) res = E_SYS_ACCESS; else {
         jfs_dinode *fn = malloc_th(sizeof(jfs_dinode));
         qs_filecc   fd = 0;
         // read file inode
         res = read_inode(vd, de->inode, fn);
         // and create file cache object
         if (!res) res = open_file(vd, fn, &fd);
         if (!res) {
            fh = (file_handle*)malloc(sizeof(file_handle));
            fh->sign = JFSFH_SIGN;
            fh->de   = de;
            fh->dd   = dd;
            fh->fd   = fd;
            strcpy(fh->fp, name);
            usage_modify(dd,1);
            *pfh = (io_handle_int)fh;
         }
         if (res) DELETE(fd);
         free(fn);
      }
   }
   free(cpath);
   if (!res) *action = IOFN_EXISTED;
   // signal to i/o code about r/o kind of our`s implementation
   return res?res:E_SYS_READONLY;
}

static qserr _exicc jfs_read(EXI_DATA, io_handle_int file, u64t pos,
                             void *buffer, u32t *size)
{
   file_handle   *fh = (file_handle*)file;
   if (!buffer || !size) return E_SYS_ZEROPTR;
   if (fh->sign!=JFSFH_SIGN || pos>=fh->de->size) return E_SYS_INVPARM;
   if (!*size) return 0;
   return fh->fd->read(pos, buffer, *size, size);
}

static qserr _exicc jfs_write(EXI_DATA, io_handle_int file, u64t pos,
                              void *buffer, u32t *size)
{
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_ACCESS;
}

static qserr _exicc jfs_flush(EXI_DATA, io_handle_int file) {
   file_handle   *fh = (file_handle*)file;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=JFSFH_SIGN) return E_SYS_INVPARM;
   return E_SYS_ACCESS;
}

static qserr _exicc jfs_close(EXI_DATA, io_handle_int file) {
   file_handle   *fh = (file_handle*)file;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (fh->sign!=JFSFH_SIGN) return E_SYS_INVPARM;
   fh->fd->close();
   DELETE(fh->fd);
   usage_modify(fh->dd,0);
   fh->sign = 0;
   free(fh);
   return 0;
}

static qserr _exicc jfs_size(EXI_DATA, io_handle_int file, u64t *size) {
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=JFSFH_SIGN) return E_SYS_INVPARM;
   if (!size) return E_SYS_ZEROPTR;
   *size = fh->de->size;
   return 0;
}

static qserr _exicc jfs_setsize(EXI_DATA, io_handle_int file, u64t newsize) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc jfs_finfo(EXI_DATA, io_handle_int file, io_direntry_info *info) {
   file_handle *fh = (file_handle*)file;
   if (fh->sign!=JFSFH_SIGN) return E_SYS_INVPARM;
   if (!info) return E_SYS_ZEROPTR;
   // a bit faster than function below
   info->attrs   = fh->de->attrs;
   info->fileno  = 0;
   info->size    = fh->de->size;
   info->vsize   = fh->de->size;
   io_timetoio(&info->atime, fh->de->wtime);
   io_timetoio(&info->wtime, fh->de->wtime);
   io_timetoio(&info->ctime, fh->de->ctime);
   strcpy(info->name, fh->fp);
   info->dir     = 0;

   return 0;
}

static qserr _exicc jfs_pathinfo(EXI_DATA, const char *path, io_handle_info *info) {
   qserr         res;
   char       *cpath;
   int         index;
   dir_data      *dd;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;

   cpath = jfs_cvtpath(vd, path);
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

static qserr _exicc jfs_setinfo(EXI_DATA, const char *path, io_handle_info *info, u32t flags) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path || !info) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc jfs_setexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc jfs_getexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t *size) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_SYS_UNSUPPORTED;
}

static str_list* _exicc jfs_lstexattr(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, 0);
   return 0;
}

// warning! it acts as rmdir too!!
static qserr _exicc jfs_remove(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!path) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc jfs_renmove(EXI_DATA, const char *oldpath, const char *newpath) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!oldpath || !newpath) return E_SYS_ZEROPTR;
   return E_DSK_WP;
}

static qserr _exicc jfs_makepath(EXI_DATA, const char *path) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static qserr _exicc jfs_diropen(EXI_DATA, const char *path, dir_handle_int *pdh) {
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
   cpath = jfs_cvtpath(vd, path);
   if (!cpath) res = E_SYS_INVPARM; else {
      dir_data    *dd;
      res = follow_path(vd, cpath, &dd, 0);
      free(cpath);

      if (!res) {
         ed->sign = JFSDIR_SIGN;
         ed->dd   = dd;
         ed->pos  = 0;
         *pdh     = (dir_handle_int)ed;
         usage_modify(dd,1);
      }
   }
   if (res && ed) free(ed);
   return res;
}

static qserr _exicc jfs_dirnext(EXI_DATA, dir_handle_int dh, io_direntry_info *info) {
   dir_enum_data *ed = (dir_enum_data*)dh;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!ed || !info) return E_SYS_ZEROPTR;
   if (ed->sign!=JFSDIR_SIGN) return E_SYS_INVPARM;

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
      // cvt name to current codepage
      fd_getname(de, info->name);
   }
   return 0;
}

static qserr _exicc jfs_dirclose(EXI_DATA, dir_handle_int dh) {
   dir_enum_data *ed = (dir_enum_data*)dh;
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   if (!ed) return E_SYS_ZEROPTR;
   if (ed->sign!=JFSDIR_SIGN) return E_SYS_INVPARM;

   ed->sign = 0;
   usage_modify(ed->dd,0);
   free(ed);
   return 0;
}

static qserr _exicc jfs_volinfo(EXI_DATA, disk_volume_data *info, int fast) {
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
         info->FSizeLim     = JFS_MAXSIZE - 1;
         info->ClBad        = vdta->badclus;
         info->SerialNum    = vdta->serial;
         info->ClSize       = vdta->clsize;
         //info->ClAvail      = 0;
         info->ClTotal      = vdta->cltotal;
         info->FsVer        = FST_OTHER;
         info->InfoFlags    = VIF_READONLY;
         //info->Reserved     = 0;
         //this is wrong!
         info->DataSector   = JFS_BMAP_OFF>>vd->shift;
         //info->OemData      = 0;
         strcpy(info->Label,vdta->label);
         strcpy(info->FsName,vdta->fsname);
         return 0;
      }
   }
   return E_DSK_NOTMOUNTED;
}

static qserr _exicc jfs_setlabel(EXI_DATA, const char *label) {
   instance_ret(volume_data, vd, E_SYS_INVOBJECT);
   return E_DSK_WP;
}

static u32t _exicc jfs_avail(EXI_DATA) { return SFAF_FINFO; }

static void _std jfs_init(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   vd->sign        = JFS_SIGN;
   vd->vdsk        = 0;
   vd->self        = (qs_sysvolume)instance;
}

static void _std jfs_done(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   if (vd->sign==JFS_SIGN)
      if (vd->vdsk) log_printf("Vol %X fini???\n", vd->vdsk);
   jfs_freedata(vd);
   memset(vd, 0, sizeof(volume_data));
}

static void *m_list[] = { jfs_initialize, jfs_finalize, jfs_avail,
   jfs_volinfo, jfs_setlabel, jfs_drive, jfs_open, jfs_read, jfs_write,
   jfs_flush, jfs_close, jfs_size, jfs_setsize, jfs_finfo, jfs_setexattr,
   jfs_getexattr, jfs_lstexattr, jfs_remove, jfs_renmove, jfs_makepath,
   jfs_remove, jfs_pathinfo, jfs_setinfo, jfs_diropen, jfs_dirnext,
   jfs_dirclose };

u32t register_jfs(void) {
   u32t cid;
   // something forgotten! interface part is not match to the implementation
   if (sizeof(_qs_sysvolume)!=sizeof(m_list)) {
      log_printf("jfs: function list mismatch\n");
      _throw_(xcpt_align);
   }
   // headers align mismatch
   if (sizeof(jfs_iag)!=4096 || sizeof(jfs_dinode)!=512 || sizeof(jfs_dtpage)!=4096
      || sizeof(jfs_superblock)!=512)
   {
      log_printf("jfs: check headers! (%u,%u,%u,%u,%u)\n", sizeof(jfs_iag),
                 sizeof(jfs_dinode), sizeof(jfs_dtpage), sizeof(jfs_superblock),
                 offsetof(jfs_superblock,attach));
      _throw_(xcpt_align);
   }
   // register fs class
   cid = exi_register("qs_sys_jfsio", m_list, sizeof(m_list)/sizeof(void*),
                       sizeof(volume_data), EXIC_PRIVATE, jfs_init, jfs_done, 0);
   // and then class in the i/o subsystem
   if (cid) {
      qserr res = io_registerfs(cid);
      if (res) {
         log_printf("jfs: regfs err %X!\n", res);
         exi_unregister(cid);
         cid = 0;
      }
   }
   return cid;
}
