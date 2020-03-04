#include "fmtdata.h"
#include "vioext.h"
#include "time.h"
#include "stdlib.h"
#include "memmgr.h"
#include "qsint.h"
#include "qsstor.h"
#include "qsio.h"
#include "qcl/cplib.h"

#define PERCENTS_PER_DISKCHECK   90
#define WRITE_CODEPAGES           1
/* unsuccessfull try to prevent autocheck on HPFS386.
   Still can be used to write single small file to a formatted partition */
#define WRITE_CHKDSKLOG           0

extern char  hpfs_bsect[];  ///< HPFS partition boot sector + micro-FSD code
extern u32t  hpfs_bscount;  ///< number of sectors in array above

#if WRITE_CHKDSKLOG
const u8t   chkdsk_data[] = { 0x10,0,0x2D,1,2,0,0,0,0,0,0,0,0,0,0,0,8,0,0xB5,4,
                              0,0,0,0 };
const char   *chkdsk_name = "chkdsk.log";
#endif

static void free_fmt_info(fmt_info *fd) {
   if (fd->bmp) { DELETE(fd->bmp); fd->bmp = 0; }
   if (fd->bslist) { DELETE(fd->bslist); fd->bslist = 0; }
   if (fd->mwlist) { 
      if (fd->mwlist->count()) fd->mwlist->freeitems(0,fd->mwlist->max());
      DELETE(fd->mwlist);
      fd->mwlist = 0; 
   }
   if (fd->mapidx) { free(fd->mapidx); fd->mapidx = 0; }
   if (fd->bblist) { free(fd->bblist); fd->bblist = 0; }
   if (fd->dbbmp) { DELETE(fd->dbbmp); fd->dbbmp = 0; }
   if (fd->pf) { hlp_memfree(fd->pf); fd->pf = 0; }
   free(fd);
}

static int check_esc(u32t disk, u64t sector) {
   u16t key = key_wait(0);
   if ((key&0xFF)==27)
      if (confirm_dlg("Break format process?^Partition will be unusable!")) {
         // zero boot sector for incomplete partition format
         char   buffer[MAX_SECTOR_SIZE];
         memset(buffer, 0, MAX_SECTOR_SIZE);
         hlp_diskwrite(disk|QDSK_DIRECT, sector, 1, buffer);
         return 1;
      }
   return 0;
}

#define CHECK_ESC()                               \
   if (kbask)                                     \
      if (check_esc(fd->voldisk, fd->volstart)) { \
         free_fmt_info(fd);                       \
         return E_SYS_UBREAK;                     \
      }

static void marklist(bit_map bmp, int value, dd_list lst) {
   u32t ii, count = lst->count();
   if (count)
      for (ii=0; ii<count; ii++) bmp->set(value, lst->value(ii), 1);
}

#define allocate_early(bmp,len,align) allocate_sectors(bmp,len,align,SEC_EARLY)
#define allocate_middle(bmp,len,align) \
   allocate_sectors(bmp,len,align,(bmp)->size()>>1&~7)

static u32t allocate_sectors(bit_map bmp, u32t length, u32t align, u32t hint) {
   u32t pos;
   if (align<=1) pos = bmp->findset(1,length,hint); else {
      pos = bmp->find(1,length+align-1,hint);
      if (pos!=FFFF) {
         // align position
         if (pos%align) pos+=align-pos%align;
         bmp->set(0,pos,length);
      }
   }
   return pos;
}

/// allocate bitmap with specific index
static u32t allocate_bitmap(bit_map bmp, u32t index) {
   u32t  odd = index&1,
      wanted = (index+odd)*SEC_PER_BAND - (odd?SPB:0);

   return allocate_sectors(bmp, SPB, SPB, wanted);
}

/** prepare bad sector lists.
    @return success flag (1/0) */
static int prepare_bb_list(fmt_info *fd) {
   u32t nbb = fd->bslist->count(), ii;

   if (!nbb) fd->bbidxlen = 1; else {
      fd->bbidxlen = nbb/(512-1);
      if (nbb%(512-1)) fd->bbidxlen++;
   }
   fd->bblist = (hpfs_badlist*)malloc_thread(fd->bbidxlen*sizeof(hpfs_badlist));
   if (!fd->bblist) return 0;
   mem_zero(fd->bblist);

   // sectors for the bad block list
   fd->sup.badlist.main = allocate_early(fd->bmp, SPB, 1);

   if (fd->bbidxlen>1)
      for (ii=0; ii<fd->bbidxlen-1; ii++)
         fd->bblist[ii].fwd = allocate_early(fd->bmp, SPB, 1);

   // convert sector list to bad block list in filesystem format
   if (nbb) {
      u32t idx;
      // sort it (not required, but for fun
      fd->bslist->sort(0,1);

      for (ii=0, idx=0; ii<nbb; ii++) {
         if (ii && ii%511==0) idx++;
         fd->bblist[idx].list[ii%511] = fd->bslist->value(ii);
      }
   }
   fd->sup.badsectors = nbb;

   DELETE(fd->bslist);
   fd->bslist = 0;

   return 1;
}

#if WRITE_CHKDSKLOG
static void add_chkdsk_log(fmt_info *fd, hpfs_dirent *pd) {
   fmt_mwrite *fn_data = (fmt_mwrite*)calloc_thread(1,sizeof(fmt_mwrite)+512),
              *fl_data = (fmt_mwrite*)calloc_thread(1,sizeof(fmt_mwrite)+512);
   hpfs_fnode      *fn = (hpfs_fnode*)&fn_data->data;

   fn_data->nsec   = 1;
   fl_data->nsec   = 1;
   fn_data->sector = allocate_early(fd->bmp, 2, 1);
   fl_data->sector = fn_data->sector + 1;
   memcpy(&fl_data->data, &chkdsk_data, sizeof(chkdsk_data));

   pd->fnode       = fn_data->sector;
   pd->time_mod    = pd->time_access = pd->time_create = time(0);
   pd->fsize       = sizeof(chkdsk_data);
   pd->namelen     = strlen(chkdsk_name);
   pd->recsize     = Round4(sizeof(hpfs_dirent) + pd->namelen);
   memcpy(&pd->name, chkdsk_name, pd->namelen);

   push_writelist(fd, fn_data);
   push_writelist(fd, fl_data);

   fn->header      = HPFS_FNODE_SIG;
   fn->aclofs      = offsetof(hpfs_fnode,rspace[0]);
   fn->pdir        = fd->sup.rootfn;
   fn->fst.vdlen          = pd->fsize;
   fn->fst.alb.free       = HPFS_LEAFPERFNODE - 1;
   fn->fst.alb.used       = 1;
   fn->fst.alb.freeofs    = 0x14;
   fn->fst.aall[0].phys   = fl_data->sector;
   fn->fst.aall[0].runlen = 1;
   fn->fst.aall[1].log    = FFFF;
   fn->name[0]            = pd->namelen;
   memcpy(&fn->name[1], chkdsk_name, pd->namelen>15?15:pd->namelen);
}
#endif

/** allocate directory structures & root dir.
    @return success flag (1/0) */
static int prepare_dirblk(fmt_info *fd) {
   hpfs_dirent *pd;
   u32t         ii,
           dirband = fd->nfreemaps>>1,
           dsksize = fd->ptsize/(_1MB/512),
            dbsize = dsksize>10 ? 50+(dsksize-10)*5 : 50;
   // follow native HPFS rules in allocation of dir band
   if (dbsize>MAX_DIRBLK) dbsize = MAX_DIRBLK;
   fd->sup.dbsectors     = dbsize*SEC_PER_DIRBLK;
   // pos to the middle
   fd->sup.dirblk_start  = allocate_sectors(fd->bmp, fd->sup.dbsectors, SPB,
                           dirband * 8 * SPB * 512);
   fd->sup.dirblk_end    = fd->sup.dirblk_start+fd->sup.dbsectors-1;
   // dirblk bitmap and root dirblk
   if (!dirband) {
      // in the middle of small disk
      fd->rootdir.self   = allocate_middle(fd->bmp, SEC_PER_DIRBLK, SEC_PER_DIRBLK);
      fd->sup.dirblk_bmp = allocate_middle(fd->bmp, SPB, SPB);
   } else {
      // just in the end of band, prior to used for DIRBLKs
      fd->rootdir.self   = allocate_sectors(fd->bmp, SEC_PER_DIRBLK,
                            SEC_PER_DIRBLK, dirband * 8 * SPB * 512 - 8);
      fd->sup.dirblk_bmp = allocate_sectors(fd->bmp, SPB, SPB, fd->rootdir.self);
   }

   // spare dirblks
   for (ii=0; ii<SPARE_DIRBLK; ii++)
      fd->spr.spare_dirblk[ii] = allocate_sectors(fd->bmp, SEC_PER_DIRBLK,
         SEC_PER_DIRBLK, fd->sup.dirblk_end);
   fd->spr.dnspare_free  = SPARE_DIRBLK;
   fd->spr.dnspare_max   = SPARE_DIRBLK;

   // create dirblk bitmap
   fd->dbbmp = NEW(bit_map);
   if (!fd->dbbmp) return 0;
   if (!fd->dbbmp->alloc(SPB*512*8)) return 0;
   // turn off common trace for this instance
   exi_chainset(fd->dbbmp,0);
   // unused bits was zeroed by alloc, so mark only used part here
   fd->dbbmp->set(1,0,fd->sup.dbsectors/SEC_PER_DIRBLK);

   // fill only non-zero FNODE fields
   fd->rootfn.header   = HPFS_FNODE_SIG;
   fd->rootfn.flag     = HPFS_FNF_DIR;
   fd->rootfn.aclofs   = offsetof(hpfs_fnode,rspace[0]);
   fd->sup.rootfn      = allocate_sectors(fd->bmp, 1, 1, fd->rootdir.self);
   fd->rootfn.pdir     = fd->sup.rootfn;
   fd->rootfn.fst.alb.free     = HPFS_LEAFPERFNODE - 1;
   fd->rootfn.fst.alb.used     = 1;
   fd->rootfn.fst.alb.freeofs  = 0x14;
   fd->rootfn.fst.aall[0].phys = fd->rootdir.self;
   fd->rootfn.fst.aall[1].log  = FFFF;

   // fill root DIRBLK
   fd->rootdir.sig     = HPFS_DNODE_SIG;
   fd->rootdir.change  = 1;
   fd->rootdir.parent  = fd->sup.rootfn;

   // build ".." & closing dir entries
   pd = (hpfs_dirent*)&fd->rootdir.startb;
   pd->recsize  = Round4(sizeof(hpfs_dirent) + 2);
   pd->flags    = HPFS_DF_SPEC;
   pd->attrs    = ATTR_DIRECTORY;
   pd->fnode    = fd->sup.rootfn;
   pd->time_mod = pd->time_access = pd->time_create = time(0);
   pd->fsize    = 5;
   pd->namelen  = 2;
   pd->name[0]  = 1;
   pd->name[1]  = 1;
#if WRITE_CHKDSKLOG
   pd = (hpfs_dirent*)((u8t*)pd + pd->recsize);
   add_chkdsk_log(fd, pd);
#endif
   pd = (hpfs_dirent*)((u8t*)pd + pd->recsize);
   pd->recsize  = sizeof(hpfs_dirent);
   pd->flags    = HPFS_DF_END;
   pd->namelen  = 1;
   pd->name[0]  = -1;

   /// calc first free byte value
   fd->rootdir.firstfree = (u8t*)pd - (u8t*)&fd->rootdir + pd->recsize;

   return fd->sup.dirblk_start!=FFFF && fd->rootdir.self!=FFFF && fd->sup.dirblk_bmp!=FFFF;
}

u32t hpfs_sectorcrc(u8t *sector) {
   u32t rc=0, cnt=512;
   while (cnt--) { rc+=*sector++; rc = (rc>>2) + (rc<<30); }
   return rc;
}

/// what the f$ed morons wrote TWO different checksum calcs?
u32t hpfs_cpdatacrc(u8t *data, u32t cnt) {
   u32t rc=0;
   while (cnt--) { rc+=*data++; rc = (rc<<7) + (rc>>25); }
   return rc;
}

static void push_writelist(fmt_info *fd, fmt_mwrite *mw) {
   if (!mw || !mw->nsec) return;
   if (!fd->mwlist) fd->mwlist = NEW(ptr_list);
   fd->mwlist->add(mw);
}

static int prepare_codepage(fmt_info *fd) {
#if WRITE_CODEPAGES
   qs_cpconvert     cpi;
   u8t             *tab = 0;
   u16t        codepage = 0;
   hpfs_cpinfo      *ci;
   hpfs_cpdataentry *di = (hpfs_cpdataentry*)&fd->cpd_sector[sizeof(hpfs_cpdata)];

   // CPLIB lib will be auto-loaded for us if it available
   cpi = NEW(qs_cpconvert);
   if (cpi) {
      codepage = cpi->getsyscp();
      /* force it to 850 if no selected!
         Only for this page country code 0 is valid by HPFS design */
      if (!codepage) codepage = 850;

      tab = cpi->uprtab(codepage);
      if (tab) memcpy(&di->uprtab, tab, 128);
   }
   if (cpi) DELETE(cpi);
   if (!tab) return 0;

   fd->cpi.header     = HPFS_CPDIR_SIG;
   fd->cpi.codepages  = 1;
   fd->spr.cp_dir     = allocate_early(fd->bmp, 1, 1);
   fd->spr.codepages  = 1;
   ci = fd->cpi.cpinfo + 0;
   /* randomly select country code for some known pages ;)
      this is wrong, but at least Russia, US, Greece, various Vikings,
      Turkey & Israel will be happy with correct Country/CodePage pair */
   switch (codepage) {
      case 437: ci->country =   1; break;
      case 852: ci->country = 421; break;  // Czech used
      case 855: ci->country = 381; break;  // Serbia used
      case 857: ci->country =  90; break;  // Turkey
      case 861: ci->country = 354; break;  // Iceland
      case 862: ci->country = 972; break;  // Israel
      case 863: ci->country =   2; break;  // French Canada
      case 864: ci->country = 966; break;  // Saudi Arabia used
      case 865: ci->country =  47; break;  // Norway used
      case 866: ci->country =   7; break;  // just home :)
      case 869: ci->country =  30; break;  // Greece
      default : ci->country = 0;
   }
   ci->codepage       = codepage;
   ci->cpdata         = allocate_early(fd->bmp, 1, 1);

   di->country        = ci->country;
   di->codepage       = codepage;

   fd->cpd.header     = HPFS_CPDAT_SIG;
   fd->cpd.codepages  = 1;
   fd->cpd.dataofs[0] = sizeof(hpfs_cpdata);
   ci->cpdataCRC      = hpfs_cpdatacrc((u8t*)di, sizeof(hpfs_cpdataentry));
   fd->cpd.dataCRC[0] = ci->cpdataCRC;

#else
   fd->spr.cp_dir     = 0;
   fd->spr.codepages  = 0;
#endif
   return 1;
}

/** write filesystem data to volume.
    @return error code */
static int write_volume(fmt_info *fd, read_callback cbprint) {
   u32t dsk = fd->voldisk|QDSK_DIRECT, ii;
   u64t  v0 = fd->volstart, step;

   // boot sector, super & spare blocks
   if (!hlp_diskwrite(dsk, v0, 1, &fd->br) ||
      !hlp_diskwrite(dsk, v0+HPFSSEC_SUPERB, 1, &fd->sup) ||
         !hlp_diskwrite(dsk, v0+HPFSSEC_SPAREB, 1, &fd->spr))
            return E_DSK_ERRWRITE;
   // write remaining part of micro-FSD code
   if (hpfs_bscount>1)
      if (hlp_diskwrite(dsk, v0+1, hpfs_bscount-1, hpfs_bsect+512)!=hpfs_bscount-1)
         return E_DSK_ERRWRITE;
   // zero sectors between micro-FSD & superblock (but ignore result)
   if (HPFSSEC_SUPERB-hpfs_bscount>0)
      dsk_emptysector(dsk, v0+hpfs_bscount, HPFSSEC_SUPERB-hpfs_bscount);
   // boot block ready
   if (cbprint) cbprint(91,0);
   // code page dir
   if (fd->spr.cp_dir) {
      if (!hlp_diskwrite(dsk, v0+fd->spr.cp_dir, 1, &fd->cpi))
         return E_DSK_ERRWRITE;
      if (!hlp_diskwrite(dsk, v0+fd->cpi.cpinfo[0].cpdata, 1, &fd->cpd_sector))
         return E_DSK_ERRWRITE;
   }
   // hot fix table
   if (hlp_diskwrite(dsk, v0+fd->spr.hotfix, SPB, &fd->hotfixes)!=SPB)
      return E_DSK_ERRWRITE;
   if (cbprint) cbprint(92,0);
   // dirblk bitmap
   if (hlp_diskwrite(dsk, v0+fd->sup.dirblk_bmp, SPB, fd->dbbmp->mem())!=SPB)
      return E_DSK_ERRWRITE;
   // 8 empty sectors for userid table
   ii = dsk_emptysector(dsk, v0+fd->sup.sid_table, 8);
   if (ii) return ii;
   if (cbprint) cbprint(93,0);
   /* fill spare dirblk`s by 0xF6. This is strange, but other values will force
      aurora`s chkdsk to "minor error" */
   for (ii=0; ii<SPARE_DIRBLK; ii++)
      dsk_fillsector(dsk, v0+fd->spr.spare_dirblk[ii], SEC_PER_DIRBLK, 0xF6);
   // root fnode
   if (!hlp_diskwrite(dsk, v0+fd->sup.rootfn, 1, &fd->rootfn))
      return E_DSK_ERRWRITE;
   // root dikblk
   if (hlp_diskwrite(dsk, v0+fd->rootdir.self, SPB, &fd->rootdir)!=SPB)
      return E_DSK_ERRWRITE;
   if (cbprint) cbprint(94,0);
   // bitmap indirect block
   if (hlp_diskwrite(dsk, v0+fd->sup.bitmaps.main, fd->mapidxlen * SPB,
      fd->mapidx) != fd->mapidxlen * SPB)
         return E_DSK_ERRWRITE;
   // write minor data (it exist one)
   if (fd->mwlist)
      if (fd->mwlist->count())
         for (ii=0; ii<fd->mwlist->count(); ii++) {
            fmt_mwrite *we = (fmt_mwrite*)fd->mwlist->value(ii);
            if (hlp_diskwrite(dsk, v0+we->sector, we->nsec, &we->data)!=we->nsec)
               return E_DSK_ERRWRITE;
         }
   if (cbprint) cbprint(95,0);
   // bitmaps
   step = fd->nfreemaps/4;
   if (!step) step = 1;
   for (ii=0; ii<fd->nfreemaps; ii++) {
      if (cbprint && ii%step==0) cbprint(95+ii/step,0);
      if (hlp_diskwrite(dsk, v0+fd->mapidx[ii], SPB, fd->bmp->mem()+ii*SPB*512)!=SPB)
         return E_DSK_ERRWRITE;
   }
   if (cbprint) cbprint(99,0);
   // first bad sector list
   if (hlp_diskwrite(dsk, v0+fd->sup.badlist.main, SPB, fd->bblist)!=SPB)
      return E_DSK_ERRWRITE;
   // remaining bad sector lists (if available)
   for (ii = 1; ii<fd->bbidxlen; ii++)
      if (hlp_diskwrite(dsk, v0+fd->bblist[ii-1].fwd, SPB, fd->bblist+ii)!=SPB)
         return E_DSK_ERRWRITE;
   if (cbprint) cbprint(100,0);
   return 0;
}

qserr _std hpfs_format(u8t vol, u32t flags, read_callback cbprint) {
   u32t  hidden = 0, percent = 0, pfcnt, ii, badcnt;
   int    align = flags&DFMT_NOALIGN?0:1,  // AF align flag
          kbask = flags&DFMT_BREAK?1:0;    // allow keyboard ESC break
   long  volidx, ptbyte;
   disk_volume_data di;
   disk_geo_data   geo;
   fmt_info        *fd;
   u64t          wsect;
   char         drivel = 0;

   hlp_volinfo(vol, &di);
   //log_it(2, "vol=%X, sz=%d\n", vol, di.TotalSectors);
   if (!di.TotalSectors) return E_DSK_NOTMOUNTED;
   if (di.InfoFlags&VIF_VFS) return E_DSK_NOTPHYS;
   if (di.InfoFlags&VIF_NOWRITE) return E_DSK_WP;

   volidx = vol_index(vol,0);
   // allow floppies and big floppies
   if (volidx<0 && di.StartSector) return E_PTE_PINDEX;
   if (volidx>=0) {
      if (dsk_isgpt(di.Disk,volidx)==1) {
         // GPT partition
         flags |=DFMT_NOPTYPE;
         hidden = di.StartSector>=_4GBLL ? FFFF : di.StartSector;
      } else {
         lvm_partition_data lvd;
         u32t             flags;
         // MBR or hybrid partition
         dsk_ptquery(di.Disk,volidx,0,0,0,&flags,&hidden);
         if (hidden==FFFF) return E_PTE_EXTERR;
         // query drive letter for the primary partition
         if (flags&DPTF_PRIMARY)
            if (lvm_partinfo(di.Disk, volidx, &lvd)) {
               drivel = toupper(lvd.Letter);
               if (drivel<'C' || drivel>'Z') drivel = 0;
            }
      }
   }
   // turn off align for floppies
   if ((di.Disk&QDSK_FLOPPY)!=0) align = 0;
   if (di.SectorSize!=512) return E_DSK_SSIZE;
   // limit to 64Gb
   if (di.TotalSectors>=FORMAT_LIMIT) return E_DSK_VLARGE;
   /* unmount will clear all cache and below all of r/w ops use QDSK_DIRECT
      flag, so no any volume caching will occur until the end of format */
   if (io_unmount(vol, flags&DFMT_FORCE?IOUM_FORCE:0)) return E_DSK_UMOUNTERR;
   // try to query actual values: LVM/PT first, BIOS - next
   if (!dsk_getptgeo(di.Disk,&geo)) {
      // ignore LVM surprise
      if (geo.SectOnTrack>63) geo.SectOnTrack = 63;
   } else
   if (!hlp_disksize(di.Disk,0,&geo)) {
      geo.Heads       = 255;
      geo.SectOnTrack =  63;
   }

   fd = (fmt_info*)malloc_thread(sizeof(fmt_info));
   mem_zero(fd);
   /* create BPB.
      Native format uses "recommended BPB" as base, so some FAT-specific
      stuff non-zero in it, but we just leave it all with 0 */
   fd->br.BR_BPB.BPB_BytePerSect = 512;
   fd->br.BR_BPB.BPB_ResSectors  = 1;
   if (di.TotalSectors < 0x10000) fd->br.BR_BPB.BPB_TotalSec = di.TotalSectors;
      else fd->br.BR_BPB.BPB_TotalSecBig = di.TotalSectors;
   fd->br.BR_BPB.BPB_MediaByte   = di.StartSector?0xF8:0xF0;
   fd->br.BR_BPB.BPB_SecPerTrack = geo.SectOnTrack;
   fd->br.BR_BPB.BPB_Heads       = geo.Heads;
   fd->br.BR_BPB.BPB_HiddenSec   = hidden;
   // use current time as VSN
   fd->br.BR_EBPB.EBPB_VolSerial = tm_getdate();
   if ((di.Disk&QDSK_FLOPPY)==0) fd->br.BR_EBPB.EBPB_PhysDisk = 0x80;
   // extended boot signature (28 on HPFS)
   fd->br.BR_EBPB.EBPB_Sign      = 0x28;
   fd->br.BR_EBPB.EBPB_Dirty     = drivel?0x80+(drivel-'C'):0;
#if 0
   memcpy(fd->br.BR_EBPB.EBPB_VolLabel, "NO NAME    ", 11);
#endif
   memcpy(fd->br.BR_EBPB.EBPB_FSType, "HPFS    ", 8); // HPFS signature
   memcpy(&fd->br,hpfs_bsect,11);
   memcpy(&fd->br+1,(struct Boot_Record *)&hpfs_bsect+1,512-sizeof(struct Boot_Record));

   // 32k buffer
   fd->pf        = (u8t*)hlp_memalloc(_32KB, QSMA_RETERR|QSMA_NOCLEAR|QSMA_LOCAL);
   // sectors in 32k
   pfcnt         = _32KB/di.SectorSize;
   // bad sector list & bitmap
   fd->bslist    = NEW(dd_list);
   fd->bmp       = NEW(bit_map);
   fd->ptsize    = di.TotalSectors & ~3;
   // use TotalSectors here, not aligned down value - to simulate native code
   fd->nfreemaps = ((u32t)di.TotalSectors-1)/(8*SPB*512)+1;
   // number of 2k blocks in bitmap index
   fd->mapidxlen = (fd->nfreemaps*sizeof(u32t)-1)/(512*SPB)+1;
   fd->voldisk   = di.Disk|QDSK_DIRECT;
   fd->volstart  = di.StartSector;

   // allocate bitmap
   if (fd->bmp) {
      u32t alloc = fd->nfreemaps*8*SPB*512;
      if (fd->bmp->alloc(alloc)) {
         // mark all as free
         fd->bmp->setall(1);
         // mark remaining part as used
         fd->bmp->set(0,fd->ptsize,alloc-fd->ptsize);
         // turn off common trace for this instance
         exi_chainset(fd->bmp,0);
         // allocate space for bitmap index
         fd->mapidx = (u32t*)malloc_thread(fd->mapidxlen*512*SPB);
         if (fd->mapidx) mem_zero(fd->mapidx);
      }
   }

   // check all of above
   if (!fd->pf || !fd->bslist || !fd->bmp || !fd->bmp->size() || !fd->mapidx) {
      free_fmt_info(fd);
      return E_SYS_NOMEM;
   }


   // print callback
   if (cbprint) { cbprint(0,0); percent = 0; }

   /* unlike OS/2 HPFS format - we supports checking of disk space for bad sectors and
      take all of it in mind when allocates disk structs. So this check placed here -
      BEFORE any bitmap allocs to minimize chance of i/o error in later code */
   if ((flags&DFMT_WIPE)!=0 || (flags&DFMT_QUICK)==0) {
      int  readonly = flags&DFMT_WIPE?0:1;
      u32t    total = fd->ptsize;
      ii    = total;
      wsect = fd->volstart;                            // first sector
      // data to write
      memset(fd->pf,FMT_FILL,_32KB);

      while (ii) {
         u32t  cpy = ii>pfcnt?pfcnt:ii,
           success = 0;
         // write 32k, read 32k and compare ;)
         if (readonly || hlp_diskwrite(fd->voldisk, wsect, cpy, fd->pf)==cpy)
            if (hlp_diskread(fd->voldisk, wsect, cpy, fd->pf)==cpy)
               if (readonly || !memchrnb(fd->pf,FMT_FILL,_32KB)) success = 1;

         if (success) { // good 32k
            wsect+= cpy;
            ii   -= cpy;
         } else {       // at least one sector is bad, check it all
            // clear array after error
            memset(fd->pf,FMT_FILL,_32KB);
            while (cpy) {
               success = 0;
               if (readonly || hlp_diskwrite(fd->voldisk, wsect, 1, fd->pf))
                  if (hlp_diskread(fd->voldisk, wsect, 1, fd->pf))
                     if (readonly || !memchrnb(fd->pf,FMT_FILL,512))
                        success = 1;
               // add sector to bad list
               if (!success) {
                  memset(fd->pf,FMT_FILL,512);
                  fd->bslist->add(wsect-fd->volstart);
               }
               ii--; wsect++; cpy--;
            }
         }
         if (cbprint) {
            u32t tmpp = (total - ii) * PERCENTS_PER_DISKCHECK / total;
            if (tmpp != percent) cbprint(percent=tmpp,0);
         }
         // check for esc
         CHECK_ESC();
      }
   } else
   if (cbprint) cbprint(percent=PERCENTS_PER_DISKCHECK,0);
   // test bad sectors
   //for (ii=1; ii<2048; ii++) fd->bslist->add(fd->ptsize/2+ii*2);

   // mark bad sectors as used
   if (fd->bslist->count()) marklist(fd->bmp, 0, fd->bslist);
   badcnt = fd->bslist->count();

   // check availability of system sectors
   if (!fd->bmp->check(1, 0, HPFSSEC_SUPERB+SPB)) {
      free_fmt_info(fd);
      return E_DSK_ERRWRITE;
   }
   /* reserve first 20 sectors.
      sectors 18,19 never used because native format made such align to 4 */
   fd->bmp->set(0, 0, HPFSSEC_SUPERB+SPB);

   fd->sup.header1       = HPFS_SUP_SIGN1;
   fd->sup.header2       = HPFS_SUP_SIGN2;
   fd->sup.sectors       = fd->ptsize;
   fd->sup.version       = 2;
   fd->sup.funcversion   = fd->sup.sectors<0x80000000/512 ? 2 : 3;
   fd->sup.chkdsk_date   = time(0);

   /* warp/merlin allocates it in the middle, but aurora - in the first sectors,
      do it in the same way */
   fd->sup.bitmaps.main  = allocate_early(fd->bmp, fd->mapidxlen * SPB, SPB);

   for (ii = 0; ii<fd->nfreemaps; ii++) {
      fd->mapidx[ii] = allocate_bitmap(fd->bmp, ii);

      if (fd->mapidx[ii]==FFFF) {
         free_fmt_info(fd);
         return E_DSK_ERRWRITE;
      }
   }
   /* sectors for the bad block list.
      !! bslist zeroed after this point! */
   if (!prepare_bb_list(fd)) { free_fmt_info(fd); return E_SYS_NOMEM; }

   fd->spr.header1    = HPFS_SPR_SIGN1;
   fd->spr.header2    = HPFS_SPR_SIGN2;
   fd->spr.flags      = (flags&DFMT_WIPE)==0 && (flags&DFMT_QUICK)!=0?HPFS_FASTFMT:0;
   /* allocate hotfixes.
      because traditional size is 100 - it fits to static buffer in fmt_info */
   fd->spr.hotfix     = allocate_early(fd->bmp, SPB, SPB);
   fd->spr.hotfix_max = fd->ptsize>40000 ? MAX_HOTFIXES : fd->ptsize/400;

   // fill codepage data
   if (!prepare_codepage(fd)) { free_fmt_info(fd); return E_SYS_CPLIB; }

   for (ii = 0; ii<fd->spr.hotfix_max; ii++)
      fd->hotfixes[fd->spr.hotfix_max+ii] = allocate_early(fd->bmp, 1, 1);
   // allocate directory structures & root dir
   if (!prepare_dirblk(fd)) { free_fmt_info(fd); return E_DSK_VSMALL; }
   // userid mapping table
   fd->sup.sid_table  = allocate_middle(fd->bmp, 8, SPB);
   // calc checksums (unused data was zeroed above)
   fd->spr.superCRC   = hpfs_sectorcrc((u8t*)&fd->sup);
   fd->spr.spareCRC   = hpfs_sectorcrc((u8t*)&fd->spr);
   // write it!
   ii = write_volume(fd, cbprint);
   // on success - update partition type & mount volume back in format_done()
   if (!ii) {
      ptbyte = volidx>=0 && (flags&DFMT_NOPTYPE)==0 ? 7 : -1;
      ii     = format_done(vol, di, ptbyte, volidx, badcnt);
      // partition was mounted without errors?
      if (!ii) {
         vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
         if (extvol) {
            vol_data *vdta = extvol+vol;
            if ((vdta->flags&VDTA_ON)!=0) {
               /* fill some volume data, used by Format result printing,
                  all of this will be lost after unmount */
               strcpy(vdta->fsname, "HPFS");
               vdta->clsize  = 1;
               vdta->clfree  = fd->bmp->total(1);
               vdta->cltotal = fd->ptsize;
            }
         }
      }

   }
   // free format data
   free_fmt_info(fd);
   return ii;
}
