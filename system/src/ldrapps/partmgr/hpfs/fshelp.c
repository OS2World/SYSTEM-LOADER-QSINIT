#include "sys/fs/hpfs.h"
#include "qsdm.h"
#include "stdlib.h"
#include "qsconst.h"
#include "fmtdata.h"

int _std hpfs_dirty(u8t vol, int on) {
   u8t   *sector = (u8t*)malloc(MAX_SECTOR_SIZE);
   int     state = -E_DSK_UNKFS;
   char   fsname[10];
   u32t       bs;

   memset(sector, 0, 512);
   bs = dsk_ptqueryfs(QDSK_VOLUME|vol, 0, fsname, sector);
   if (bs==DSKST_ERROR) state = -E_DSK_ERRREAD; else
   if (bs!=DSKST_BOOTBPB || strcmp(fsname,"HPFS")) state = -E_DSK_UNKFS; else
   if (!hlp_diskread(QDSK_VOLUME|vol, HPFSSEC_SPAREB, 1, sector)) state = -E_DSK_ERRREAD; else {
      hpfs_spareblock *sb = (hpfs_spareblock*)sector;
      if (sb->header1!=HPFS_SPR_SIGN1 || 
          sb->header2!=HPFS_SPR_SIGN2) state = -E_DSK_UNKFS; else 
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
                  state = -E_DSK_ERRWRITE;
            }
      }
   }
   free(sector);
   return state;
}
