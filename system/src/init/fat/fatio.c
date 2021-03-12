//
// QSINIT
// all FAT types micro-FSD emulation
// -----------------------------------------------
// This code is not designed to handle huge files, it just should load
// QSINIT.LDI & INI file. START module will replace this code to common
// FAT handling as fast as it can.
// 
// There are no long names for FAT12/16/32!
//
#include "qslocal.h"
#include "parttab.h"

#define MAX_FAT12      0xFF5       ///< max # of FAT12 clusters

#define FOPEN_NOMOUNT      1       ///< volume is not mounted
#define FOPEN_OPEN         2       ///< file is already open
#define FOPEN_LONGNAME     3       ///< invalid or long name asked
#define FOPEN_NOFILE       4       ///< no such file name
#define FOPEN_TOOLARGE     5       ///< file too large

extern u8t            bootFT;      ///< boot partition FAT type (FST_*)
extern char    mfsd_openname[];    ///< file name to open
extern u32t       mfsd_fsize;      ///< file size

static u64t           pt_pos;      ///< partition start
static u32t          pt_size,      ///< partition size
                     fat_pos,      ///< 1st sector of FAT
                    data_pos,      ///< 1st sector of data
                     cl_size,      ///< size of cluster in sectors
                    sec_size,      ///< sector size on volume
                        cl_n;      ///< # of clusters on disk
static u8t         *root_dir = 0,  ///< root dir data
                  *file_data = 0,  ///< entire file
                 *fcache_ptr = 0,  ///< current cached FAT sector
                    no_chain = 0;  ///< not FAT chain on exFAT
static u32t           root_n,      ///< # of of root dir entries
                  file_clust = 0,  ///< 1st file cluster, 0 means no file
                  real_fsize,
             disk, fat_cache;
static u32t  eof_mark[FST_EXFAT+1] = { 0, 0xFF8, 0xFFF8, 0xFFFFFF8, 0xFFFFFFF7 };

static u32t fat_nextcluster(u32t cluster) {
   u32t ncsec = bootFT==FST_FAT12?2:1,  fpos;
   // allocate on first use
   if (!fcache_ptr) {
      fcache_ptr = (u8t*)hlp_memallocsig(sec_size*ncsec, "fat", 0);
      fat_cache  = FFFF;
   }
   while (1) {
      if (bootFT==FST_FAT12) {
         fpos = (cluster*3>>1)/sec_size;
         if (fpos==fat_cache)
            return *(u16t*)(fcache_ptr + (cluster*3>>1)%sec_size) >> (cluster&1?4:0) & 0xFFF;
      } else {
         u32t cpf = sec_size>>(bootFT==FST_FAT16?1:2);
         fpos = cluster/cpf;
         if (fpos==fat_cache)
            if (bootFT==FST_FAT16) return ((u16t*)fcache_ptr)[cluster%cpf];
               else return ((u32t*)fcache_ptr)[cluster%cpf];
      }
      // read two sectors on FAT12 because of misaligned records
      if (hlp_diskread(disk, pt_pos+fat_pos+fpos, ncsec, fcache_ptr) != ncsec)
         return 0;
      fat_cache = fpos;
   }
   return 0;
}

/* cluster size can be 32Mb on exFAT, it is a bad idea to read it all.
   sectors per cluster must be a power of two here. */
static void* fat_rootdir(u32t cluster, u32t *size) {
   u32t alloc = _64KB;
   u8t   *res = (u8t*)hlp_memallocsig(alloc, "fat", QSMA_RETERR),
          *rp = res;
   *size = 0;
   if (res) {
      u32t rdsize = _4KB/sec_size, rpos = 0, clpos = 0, ii, ecnt;
      if (cl_size<rdsize) rdsize = cl_size;
      ecnt = rdsize*sec_size >> 5;

      while (1) {
         if (hlp_diskread(disk, pt_pos+data_pos+(cluster-2)*cl_size+clpos,
            rdsize, rp) != rdsize) break;
         // walk over sectors and search for the end marker (zero)
         for (ii=0; ii<ecnt; ii++)
            if (*rp) rp+=32; else break;
         if ((rp-res>>5)%ecnt) break;

         if (rp-res==alloc) {
            res = (u8t*)hlp_memrealloc(res, alloc<<1);
            rp  = res + alloc;
            alloc<<=1;
         }
         //log_printf("clpos=%u cluster=%u \n", clpos, cluster);

         clpos += rdsize;
         if (clpos==cl_size) {
            clpos   = 0;
            cluster = fat_nextcluster(cluster);
            if (!cluster || cluster>=eof_mark[bootFT]) break;
         }
      }
   }
   *size = rp - res;
   if (!*size && res) { hlp_memfree(res); res=0; }
   return res;
}

static void* fat_readfile(u32t cluster, u32t alloc, u32t size, int lin) {
   if (alloc<size) alloc = size;
   if (!alloc || !size || !cluster) return 0; else {
      // max sector size is 4k so align to it should be enough
      u8t *res = hlp_memallocsig(Round4k(alloc), "fat", QSMA_RETERR),
           *rp = res;
      u32t clb = cl_size*sec_size;
      int  err = 1;

      while (res) {
         u32t to_read = size - (rp-res);
         if (to_read>clb) to_read = clb;
         to_read = (to_read + sec_size - 1) / sec_size;

         if (hlp_diskread(disk, pt_pos+data_pos+(cluster-2)*cl_size, to_read,
            rp) != to_read) break;
         rp += to_read*sec_size;
         if (rp-res>=size) { err=0; break; }

         if (lin) cluster++; else cluster = fat_nextcluster(cluster);
         //log_printf("next file cluster=%u (%u)\n", cluster, lin);
         if (!cluster || cluster>=eof_mark[bootFT]) break;
      }
      //log_misc(2, "res = %X, err = %i\n", res, err);
      if (err && res) { hlp_memfree(res); return 0; }
      return res;
   }
}

void hlp_fatsetup(void) {
   u32t       idx, ii;
   vol_data  *vd0 = extvol;
   struct Boot_Record    *brw = (struct Boot_Record    *)DiskBufPM;
   struct Boot_RecordF32 *brd = (struct Boot_RecordF32 *)DiskBufPM;
   struct Boot_RecExFAT  *bre = (struct Boot_RecExFAT  *)DiskBufPM;

   pt_pos   = vd0->start;
   sec_size = vd0->sectorsize;
   disk     = vd0->disk|QDSK_IGNACCESS;

   if (!hlp_diskread(disk, pt_pos, 1, (void*)DiskBufPM)) {
      log_printf("FAT: read!\n");
      exit_pm32(QERR_FATERROR);
   }
   /* we can ignore errors here, because at least THIS binary was
      loaded by boot sector code, i.e. file system is valid enough */
   if (memcmp(brw->BR_OEM, "EXFAT   ", 8)==0) {
      if (1<<bre->BR_ExF_SecSize == sec_size) {
         bootFT   = FST_EXFAT;
         pt_size  = bre->BR_ExF_VolSize;
         fat_pos  = bre->BR_ExF_FATPos;
         data_pos = bre->BR_ExF_DataPos;
         cl_size  = 1<<bre->BR_ExF_ClusSize;
         cl_n     = bre->BR_ExF_NumClus;
         root_n   = 0;
         root_dir = fat_rootdir(bre->BR_ExF_RootClus, &root_n);
         root_n >>= 5;
      }
   } else
   if (brw->BR_BPB.BPB_BytePerSect == sec_size) {
      fat_pos = brw->BR_BPB.BPB_ResSectors;
      pt_size = brw->BR_BPB.BPB_TotalSec?brd->BR_BPB.BPB_TotalSec:brd->BR_BPB.BPB_TotalSecBig;
      cl_size = brw->BR_BPB.BPB_SecPerClus;

      if (brw->BR_BPB.BPB_RootEntries==0) {
         bootFT   = FST_FAT32;
         data_pos = fat_pos + brd->BR_F32_BPB.FBPB_SecPerFAT * brd->BR_BPB.BPB_FATCopies;
         cl_n     = (pt_size - data_pos) / cl_size;
         // take it in mind (as windows boot sector do)
         if (brd->BR_F32_BPB.FBPB_ActiveCopy) {
            u32t cfat = brd->BR_F32_BPB.FBPB_ActiveCopy & 0xF;
            if (cfat<brd->BR_BPB.BPB_FATCopies)
               fat_pos += brd->BR_F32_BPB.FBPB_SecPerFAT*cfat;
         }
         root_n   = 0;
         root_dir = fat_rootdir(brd->BR_F32_BPB.FBPB_RootClus, &root_n);
         root_n >>= 5;
      } else {
         root_n = brw->BR_BPB.BPB_RootEntries;
         // dir entries must be sector-aligned!
         if (root_n%(sec_size>>5)==0) {
            u32t rsn = root_n/(sec_size>>5);
            data_pos = fat_pos + brw->BR_BPB.BPB_SecPerFAT*brw->BR_BPB.BPB_FATCopies + rsn;
            // select by # of clusters
            bootFT   = (pt_size-data_pos)/cl_size<=MAX_FAT12 ? FST_FAT12 : FST_FAT16;
            cl_n     = (pt_size - data_pos) / cl_size;
            root_dir = hlp_memallocsig(root_n*32, "fat", 0);
            // just ignore root dir read errors if we got at least one sector
            rsn      = hlp_diskread(disk, pt_pos+data_pos-rsn, rsn, root_dir);
            root_n   = rsn*sec_size>>5;
            if (!root_n) bootFT = FST_NOTMOUNTED;
         }
      }
   }
   // is case of read error
   if (!root_dir) bootFT = FST_NOTMOUNTED;

   if (bootFT==FST_NOTMOUNTED) {
      log_printf("FAT: detect!\n");
      exit_pm32(QERR_FATERROR);
   }
#ifdef INITDEBUG
   log_misc(2,"FAT type %u, root len %u (%08X)\n", bootFT, root_n, root_dir);
#endif
}

u16t _std fat_open(void) {
   if (bootFT==FST_NOTMOUNTED || !root_dir) return FOPEN_NOMOUNT;
   if (file_clust) return FOPEN_OPEN;

   if (bootFT==FST_EXFAT) {
      struct Dir_ExF *de = (struct Dir_ExF*)root_dir;
      u32t       namelen = strlen(mfsd_openname), ii;
      // dir record should have least 3 entries, i.e. exclude last two
      for (ii=0; ii<root_n-2; ii++, de++)
         if (!de->Dir_ExF_Type) break; else
         // our`s name forced to be <=15 chars
         if (de->Dir_ExF_Type==0x85 && de->Dir_ExF_SecCnt==2 &&
            de[1].Dir_ExF_Type==0xC0 && de[2].Dir_ExF_Type==0xC1)
         {
            struct Dir_ExF_Stream *dse = (struct Dir_ExF_Stream*)(de+1);
            struct Dir_ExF_Name   *dne = (struct Dir_ExF_Name*)(de+2);

            if (dse->Dir_ExFsNameLen==namelen && (de->Dir_ExF_Attr&FAT_D_ATTR_DIR)==0) {
               u32t idx;
               for (idx=0; idx<namelen; idx++) {
                  u16t ch = dne->Dir_ExFnName[idx];
                  if (ch>=0x80) break;
                  if (toupper(ch)!=mfsd_openname[idx]) break;
                  if (idx==namelen-1) {
                     // file to long!
                     if (dse->Dir_ExFsSize>x7FFF) return FOPEN_TOOLARGE;

                     file_clust = dse->Dir_ExFsClust;
                     mfsd_fsize = dse->Dir_ExFsSize;
                     real_fsize = dse->Dir_ExFsDataLen;
                     no_chain   = dse->Dir_ExFsFlags & 2;
                     return 0;
                  }
               }
            }
         }
   } else {
      char     fnbuf[12];
      struct Dir_FAT *de = (struct Dir_FAT*)root_dir;
      char           *cp = strchr(mfsd_openname, '.');
      u32t ii;
      memset(fnbuf, ' ', 11);
      // cvt name to dir entry format
      if (cp) {
         if (cp-mfsd_openname>8) return FOPEN_LONGNAME;
         if (strlen(++cp)>3) return FOPEN_LONGNAME;
         for (ii=0; *cp; ii++) fnbuf[8+ii] = *cp++;
         for (ii=0, cp=mfsd_openname; *cp!='.'; ii++) fnbuf[ii] = *cp++;
      } else {
         if (strlen(mfsd_openname)>8) return FOPEN_LONGNAME;
         for (ii=0, cp=mfsd_openname; *cp; ii++) fnbuf[ii] = *cp++;
      }
      for (ii=0; ii<root_n; ii++, de++)
         if (!de->Dir_F_Name[0]) break; else
         if (memcmp(de, &fnbuf, 11)==0)
            if ((de->Dir_F_Attr&(FAT_D_ATTR_VOL|FAT_D_ATTR_DIR))==0) {
               file_clust = de->Dir_F_ClustLo;
               mfsd_fsize = de->Dir_F_Size;
               real_fsize = de->Dir_F_Size;
               no_chain   = 0;
               if (bootFT==FST_FAT32) file_clust|=(u32t)de->Dir_F_ClustHi<<16;
               return 0;
            }
   }
   return FOPEN_NOFILE;
}

u32t __cdecl fat_read(u32t offset, void *buf, u32t readsize) {
   if (!file_clust || offset>=mfsd_fsize || !readsize) return 0;
   if (!file_data) file_data = fat_readfile(file_clust, mfsd_fsize, real_fsize, no_chain);
   if (!file_data) return 0;
   if (offset+readsize > mfsd_fsize) readsize = mfsd_fsize-offset;
   memcpy(buf, file_data+offset, readsize);
   return readsize;
}

void __cdecl fat_close(void) {
   if (file_data) { hlp_memfree(file_data); file_data = 0; }
   file_clust = 0;
}

void __cdecl fat_term(void) {
   if (file_clust) fat_close();
   if (fcache_ptr) { hlp_memfree(fcache_ptr); fcache_ptr = 0; }
   if (root_dir) { hlp_memfree(root_dir); root_dir = 0; }
   // bootFT = FST_NOTMOUNTED;
}
