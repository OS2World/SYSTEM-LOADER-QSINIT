#include "hpfs.h"
#include "qsdm.h"
#include "stdlib.h"
#include "qsconst.h"
#include "fmtdata.h"

int _std hpfs_dirty(u8t vol, int on) {
   u8t   *sector = (u8t*)malloc(MAX_SECTOR_SIZE);
   int     state = -DFME_UNKFS;
   char   fsname[10];
   u32t       bs;

   memset(sector, 0, 512);
   bs = dsk_ptqueryfs(QDSK_VOLUME|vol, 0, fsname, sector);
   if (bs==DSKST_ERROR) state = -DFME_IOERR; else
   if (bs!=DSKST_BOOTBPB || strcmp(fsname,"HPFS")) state = -DFME_UNKFS; else
   if (!hlp_diskread(QDSK_VOLUME|vol, HPFSSEC_SPAREB, 1, sector)) state = -DFME_IOERR; else {
      hpfs_spareblock *sb = (hpfs_spareblock*)sector;
      if (sb->header1!=HPFS_SPR_SIGN1 || 
          sb->header2!=HPFS_SPR_SIGN2) state = -DFME_UNKFS; else 
      {
         state = sb->flags&HPFS_DIRTY ? 1:0;
         // update on mismatch only
         if (on>=0)
            if (Xor(on,state)) {
               if (on>0) sb->flags|=HPFS_DIRTY; else sb->flags&=~HPFS_DIRTY;
               sb->spareCRC = 0;
               sb->spareCRC = hpfs_sectorcrc(sector);
               // write back spare block
               if (!hlp_diskwrite(QDSK_VOLUME|vol, HPFSSEC_SPAREB, 1, sector))
                  state = -DFME_IOERR;
            }
      }
   }
   free(sector);
   return state;
}

/** file offset to sector run.
    @param       vol        volume
    @param       fn         fnode
    @param       buf        buffer for children fnode read (to save stack space)
    @param [in,out] optbuf  in - file offset, out - start sector
    @return run length or 0 on error */
static u32t ofs2sec(u8t vol, hpfs_fnode *fn, hpfs_fnode *buf, u32t *fofs) {
   while (1) {
      u32t cnt = fn->fst.alb.used, ii;
      if (fn->fst.alb.flag&HPFS_ABF_NODE) {
         for (ii=0; ii<cnt; ii++) {
            hpfs_alnode *an = fn->fst.aaln + ii;
            if (an->log>*fofs) {
               if (!hlp_diskread(QDSK_VOLUME|vol,an->phys,1,(u8t*)(fn = buf)))
                  return 0;
               break;
            }
         }
         if (ii==cnt) return 0;
      } else {
         hpfs_alleaf *al = fn->fst.aall + 0;
         if (cnt>1)
            for (ii=0; ii<cnt-1; ii++)
               if (al[1].log>*fofs>>9) break; else al++;
         *fofs = al->phys;
         return al->runlen;
      }
   }
   return 0;
}

#define next_dirent(de)  de = (hpfs_dirent*)((u8t*)de + de->recsize)
#define first_dirent(db) (hpfs_dirent*)((u8t*)(db) + offsetof(hpfs_dirblk,startb))

/// read file from the root of HPFS volume
void* _std hpfs_freadfull(u8t vol, const char *name, u32t *bufsize) {
   u8t         sb[SPB*512];
   struct Boot_Record *bth = (struct Boot_Record *)sb;
   hpfs_superblock    *sup = (hpfs_superblock*)sb;
   hpfs_fnode          *fn = (hpfs_fnode*)sb;
   disk_volume_data     vi;
   u32t                 vt;

   if (!name || !bufsize) return 0;
   if (strchr(name,'/') || strchr(name,'\\')) return 0;
   *bufsize = 0;
   vt = hlp_volinfo(vol, &vi);
   // is this a FAT or volume not mounted?
   if (vt!=FST_NOTMOUNTED || !vi.TotalSectors || vi.SectorSize!=512) return 0;
   // is it HPFS?
   if (!hlp_diskread(QDSK_VOLUME|vol,0,1,sb)) return 0;
   if (memcmp(bth->BR_EBPB.EBPB_FSType,"HPFS",4)) return 0;
   // read super block
   if (!hlp_diskread(QDSK_VOLUME|vol,HPFSSEC_SUPERB,1,sb)) return 0;
   if (sup->header1!=HPFS_SUP_SIGN1 || sup->header2!=HPFS_SUP_SIGN2) return 0;
   // read root fnode
   if (hlp_diskread(QDSK_VOLUME|vol,sup->rootfn,1,sb)) {
      // search tree for the file name
      hpfs_dirblk *db = (hpfs_dirblk*)sb;
      hpfs_dirent *de = 0;
      u32t       nlen = strlen(name), ldtp;
      u8t        *res = 0;

      if (hlp_diskread(QDSK_VOLUME|vol,fn->fst.aall[0].phys,SPB,sb)!=SPB)
         return 0;

      while (1) {
         if (!de) de = first_dirent(db);
         if (de->namelen==nlen)
            if (strnicmp(name,de->name,nlen)==0) break;
     
         if (de->flags&HPFS_DF_BTP) {
            ldtp = *(u32t*)((u8t*)de + de->recsize - 4);
            if (hlp_diskread(QDSK_VOLUME|vol,ldtp,SPB,sb)!=SPB) return 0;
            de = 0;
            continue;
         } else 
         while (de->flags&HPFS_DF_END)
            // the end of topmost block, no file
            if (db->change&1) return 0; else {
               u32t pldtp = ldtp;
               if (hlp_diskread(QDSK_VOLUME|vol,ldtp=db->parent,SPB,sb)!=SPB)
                  return 0;
               de = first_dirent(db);
               // walk to continue point in parent
               while (*(u32t*)((u8t*)de+de->recsize-4) != pldtp) 
                  next_dirent(de);
            }
         next_dirent(de);
      }
      log_it(3, "loading %s - %d bytes\n", name, de->fsize);

      res  = (u8t*)hlp_memallocsig(Round512(de->fsize),"HPrf",QSMA_RETERR|QSMA_NOCLEAR);
      if (!res) return 0;
      nlen = de->fsize;

      if (!hlp_diskread(QDSK_VOLUME|vol,de->fnode,1,sb)) nlen = 0; else {
         u32t pos = 0, rsec, rlen;
         while (pos<nlen) {
            rsec = pos;
            rlen = ofs2sec(vol, fn, (hpfs_fnode*)(sb+512), &rsec);
            if (!rlen) { nlen = 0; break; }

            if (hlp_diskread(QDSK_VOLUME|vol,rsec,rlen,res+pos) != rlen)
               { nlen=0; break; }
            pos += rlen<<9;
         }
      }
      if (nlen) *bufsize = nlen; else
      if (res) { hlp_memfree(res); res = 0; }

      return res;
   }
   return 0;
}
