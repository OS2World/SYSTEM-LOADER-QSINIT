//
// QSINIT
// partition minor functions
//
#include "pscan.h"
#include "qsdm.h"
#include "qsint.h"
#include "qsmod.h"
#include <stdlib.h>
#include "cache_ord.h"
#include "vioext.h"

char *get_sizestr(u32t sectsize, u64t disksize) {
   static char buffer[64];
   static char *suffix[] = { "kb", "mb", "gb", "tb" };
   u64t size = disksize * (sectsize?sectsize:1);
   int idx = 0;
   size>>=10;
   while (size>=100000) { size>>=10; idx++; }
   sprintf(buffer, "%8d %s", (u32t)size, suffix[idx]);
   return buffer;
}

static void dsk_ptqueryfs(u32t disk, u64t sector, char *filesys) {
   struct Boot_Record *br = (struct Boot_Record*)malloc(MAX_SECTOR_SIZE);
   u32t st = dsk_sectortype(disk, sector, (u8t*)br);
   u32t ii;

   if (st==DSKST_BOOTFAT) {
      strncpy(filesys, br->BR_BPB.BPB_RootEntries==0?
        ((struct Boot_RecordF32*)br)->BR_F32_EBPB.EBPB_FSType:
           br->BR_EBPB.EBPB_FSType,8);
      filesys[8]=0;
   } else
   if ((st==DSKST_BOOTBPB||st==DSKST_BOOT) && strnicmp(br->BR_OEM,"NTFS",4)==0) {
      strcpy(filesys,"NTFS");
   } else
   if ((st==DSKST_BOOTBPB||st==DSKST_BOOT) && strnicmp(br->BR_OEM,"EXFAT",5)==0) {
      strcpy(filesys,"exFAT");
   } else
   if (st==DSKST_BOOTBPB) {
      strncpy(filesys, br->BR_EBPB.EBPB_FSType, 8);
      filesys[8]=0;
   }

   if (isalpha(filesys[0]) && isalpha(filesys[1]) && isalpha(filesys[2]))
      for (ii=7; ii && filesys[ii]==' '; ii--) filesys[ii]=0;
   else filesys[0]=0;

   free(br);
}

u8t dsk_ptquery(u32t disk, u32t index, u32t *start, u32t *size, char *filesys,
                u32t *flags, u32t *ptofs)
{
   hdd_info *hi = get_by_disk(disk);
   if (start)   *start   = FFFF;
   if (size)    *size    = FFFF;
   if (filesys) *filesys = 0;
   if (flags)   *flags   = 0;
   if (ptofs)   *ptofs   = FFFF;
   if (!hi) return 0;
   if (hi->pt_view<=index) return 0; else {
      u32t *pos = memchrd(hi->index, index, hi->pt_size), recidx;
      struct MBR_Record *rec;
      if (!pos) {
         log_it(3, "No partition %d on disk %X!\n", index, disk);
         return 0;
      }
      recidx = pos-hi->index;
      rec    = hi->pts + recidx;
      if (start)   *start = rec->PTE_LBAStart;
      if (size)    *size  = rec->PTE_LBASize;
      if (ptofs) {
         if (recidx<4) *ptofs = rec->PTE_LBAStart; else
         if (rec->PTE_LBAStart > hi->ptspos[recidx>>2])
            *ptofs = rec->PTE_LBAStart - hi->ptspos[recidx>>2];
      }
      if (flags) {
         *flags = 0;
         if (recidx<4) *flags|=DPTF_PRIMARY;
         if (rec->PTE_Active>=0x80) *flags|=DPTF_ACTIVE;
      }
      if (filesys) dsk_ptqueryfs(disk, rec->PTE_LBAStart, filesys);

      return rec->PTE_Type;
   }
}

u8t dsk_ptquery64(u32t disk, u32t index, u64t *start, u64t *size,
   char *filesys, u32t *flags)
{
   u32t tst, tsz;
   u8t     ptype = PTE_EE_UEFI;
   hdd_info *hi = get_by_disk(disk);
   if (start)   *start   = FFFF64;
   if (size)    *size    = FFFF64;
   if (filesys) *filesys = 0;
   if (flags)   *flags   = 0;
   if (!hi) return 0;

   if (hi->pt_view > index) {
      ptype = dsk_ptquery(disk, index, &tst, &tsz, filesys, flags, 0);
      if (start) *start = tst;
      if (size)  *size  = tsz;
   } else
   if (hi->gpt_view<=index) return 0; else {
      u32t *pos = memchrd(hi->gpt_index, index, hi->gpt_size);
      if (!pos) {
         log_it(3, "No GPT partition %d on disk %X!\n", index, disk);
         return 0;
      } else {
         struct GPT_Record *pte = hi->ptg + (pos - hi->gpt_index);
         if (start) *start = pte->PTG_FirstSec;
         if (size)  *size  = pte->PTG_LastSec - pte->PTG_FirstSec + 1;
         if (filesys) dsk_ptqueryfs(disk, pte->PTG_FirstSec, filesys);
         if (flags) *flags = DPTF_PRIMARY | (pte->PTG_Attrs & (1LL << 
            GPTATTR_BIOSBOOT) ? DPTF_ACTIVE : 0);
      }
   }
   return ptype;
}

static int __stdcall dsk_map_compare(const void *b1, const void *b2) {
   register dsk_mapblock *dm1 = (dsk_mapblock*)b1,
                         *dm2 = (dsk_mapblock*)b2;
   if (dm1->StartSector > dm2->StartSector) return 1;
   if (dm1->StartSector == dm2->StartSector) return 0;
   return -1;
}

/** query sorted disk map.
    @param disk     Disk number.
    @return array of dsk_mapblock (must be free()-ed), sorted by
            StartSector, with DMAP_LEND in Flags field of last entry */
dsk_mapblock* _std dsk_getmap(u32t disk) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return 0; else {
      disk_volume_data  vi[DISK_COUNT];
      dsk_mapblock     *rc;
      u32t       ii,kk,cnt;
      // make sure it was scanned at least once
      dsk_ptrescan(disk,0);
      // number of entries
      cnt = hi->gpt_view + hi->fsp_size;
      if (!cnt) return 0;
      // scan error? return 0 
      if (hi->scan_rc) return 0;
      // alloc buffer
      rc  = (dsk_mapblock*)malloc(sizeof(dsk_mapblock)*cnt);
      memZero(rc);
      // query mounted volumes
      for (ii = 0; ii<DISK_COUNT; ii++) hlp_volinfo(ii, vi+ii);
      // copy partitions
      for (ii=0; ii<hi->gpt_view; ii++) {
          u32t  flags = 0, recidx, dlatidx;
          u64t  start, size;
          rc[ii].Type        = dsk_ptquery64(disk, ii, &start, &size, 0, &flags);
          rc[ii].StartSector = start;
          rc[ii].Length      = size;
          rc[ii].Index       = ii;
          rc[ii].Flags       = (flags&DPTF_PRIMARY?DMAP_PRIMARY:DMAP_LOGICAL)|
                               (flags&DPTF_ACTIVE ?DMAP_ACTIVE :0);
         // get qs drive letter
         for (kk = 0; kk<DISK_COUNT; kk++)
            if (vi[kk].StartSector && vi[kk].Disk==disk)
               if (vi[kk].StartSector == start) {
                  rc[ii].Flags  |= DMAP_MOUNTED;
                  rc[ii].DriveQS = '0'+kk;
                  break;
               }
          // get lvm drive letter
          if (ii < hi->pt_view)
             if (!lvm_dlatpos(disk, ii, &recidx, &dlatidx)) {
                DLA_Entry *de = &hi->dlat[recidx>>2].DLA_Array[dlatidx];
                if (de->Drive_Letter) {
                   rc[ii].Flags   |= DMAP_LVMDRIVE;
                   rc[ii].DriveLVM = de->Drive_Letter;
                }
             }
      }
      // copy free space
      for (ii=0; ii<hi->fsp_size; ii++) {
          dsk_mapblock  *pb = rc + ii + hi->gpt_view;
          dsk_freeblock *fb = hi->fsp + ii;
          pb->StartSector = fb->StartSector;
          pb->Length      = fb->Length;
          pb->Index       = fb->BlockIndex;
          pb->Flags       = (fb->Flags&DFBA_PRIMARY?DMAP_PRIMARY:0)|
                            (fb->Flags&DFBA_LOGICAL?DMAP_LOGICAL:0);
      }
      // sort and return
      qsort(rc, cnt, sizeof(dsk_mapblock), dsk_map_compare);
      rc[cnt-1].Flags |= DMAP_LEND;
      return rc;
   }
}

u32t _std dsk_getptgeo(u32t disk, disk_geo_data *geo) {
   hdd_info *hi;
   u32t rrc = dsk_ptrescan(disk,0);
   // is it really need?
   if (rrc==DPTE_ERRREAD) return rrc;

   hi = get_by_disk(disk);
   if (!hi||!geo) return DPTE_INVDISK;

   memset(geo, 0, sizeof(disk_geo_data));
   geo->TotalSectors = hi->info.TotalSectors;
   geo->SectorSize   = hi->info.SectorSize;
   if (hi->lvm_snum) {
      geo->Cylinders   = hi->dlat[0].Cylinders;
      geo->Heads       = hi->dlat[0].Heads_Per_Cylinder;
      geo->SectOnTrack = hi->dlat[0].Sectors_Per_Track;
   }
   if (!geo->Cylinders || !geo->Heads || !geo->SectOnTrack) {
      if (geo->TotalSectors < 1024*255*63) {
         geo->Cylinders   = hi->info.Cylinders;
         geo->Heads       = hi->info.Heads;
         geo->SectOnTrack = hi->info.SectOnTrack;
      } else {
         geo->Cylinders   = 1024;
         geo->Heads       = 255;
         geo->SectOnTrack = 63;
      }
   }
   return 0;
}

/** query mounted volume partition index.
    @param [in]  vol      volume (0..9)
    @param [out] disk     disk number (can be 0)
    @return partition index or -1 */
long dsk_volindex(u8t vol, u32t *disk) {
   disk_volume_data  vi;
   u32t mt = hlp_volinfo(vol, &vi);
   if (vi.TotalSectors) {
      hdd_info *hi = get_by_disk(vi.Disk);
      u32t ii;
      if (disk) *disk=vi.Disk;
      if (!hi) return -1;
      dsk_ptrescan(vi.Disk,0);
      // searching in MBR
      for (ii=0; ii<hi->pt_size; ii++) {
         struct MBR_Record *rec = hi->pts+ii;
         if (rec->PTE_Type && rec->PTE_LBAStart==vi.StartSector &&
            hi->index[ii]!=FFFF) return hi->index[ii];
      }
      // in GPT
      if (hi->gpt_size) {
         for (ii=0; ii<hi->gpt_size; ii++) {
            struct GPT_Record *pte = hi->ptg + ii;
            if (pte->PTG_FirstSec && pte->PTG_FirstSec==vi.StartSector &&
               hi->gpt_index[ii]!=FFFF) return hi->gpt_index[ii];
         }
      }
   }
   return -1;
}

/** query disk text name.
    @param disk     disk number
    @param buffer   buffer for result, can be 0 for static. At least 8 bytes.
    @return 0 on error, buffer or static buffer ptr */
char* dsk_disktostr(u32t disk, char *buffer) {
   static char buf[8];
   if (!buffer) buffer = buf;
   if (disk&QDSK_FLOPPY) snprintf(buffer, 8, "fd%d", disk&QDSK_DISKMASK); else
   if (disk&QDSK_VIRTUAL) snprintf(buffer, 8, "%c:", '0'+(disk&QDSK_DISKMASK)); else
   if ((disk&QDSK_DISKMASK)==disk)
      snprintf(buffer, 8, "hd%d", disk&QDSK_DISKMASK);
         else return 0;
   return buffer;
}

u32t dsk_emptypt(u32t disk, u32t sector, struct MBR_Record *table) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (!sector || sector>=hi->info.TotalSectors) return DPTE_INVARGS; else {
      char        mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR   *mbr = (struct Disk_MBR *)&mbr_buffer;
      u32t                rc;
      memset(mbr_buffer, 0, MAX_SECTOR_SIZE);
      mbr->MBR_Signature = 0xAA55;
      if (table) memcpy(&mbr->MBR_Table, table, sizeof(mbr->MBR_Table));

      rc = hlp_diskwrite(disk, sector, 1, mbr)?0:DPTE_ERRWRITE;
      log_it(2,"init pt: %X, sector %08X, rc %d\n", disk, sector, rc);
      return rc;
   }
}

u32t _std dsk_emptysector(u32t disk, u64t sector) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (!sector || sector>=hi->info.TotalSectors) return DPTE_INVARGS; else {
      char   buffer[MAX_SECTOR_SIZE];
      memset(buffer, 0, MAX_SECTOR_SIZE);
      return hlp_diskwrite(disk, sector, 1, buffer)?0:DPTE_ERRWRITE;
   }
}

u32t dsk_isquadempty(hdd_info *info, u32t quadpos, int ign_ext) {
   if (!info) return 0;
   if (info->pt_size>>2 <= quadpos) return 0; else {
      u32t ii;
      struct MBR_Record *rec = info->pts + quadpos*4;
      for (ii=0; ii<4; ii++) {
         u8t pt = rec[ii].PTE_Type;
         if (pt && (!ign_ext || !IS_EXTENDED(pt))) return 0;
      }
      return 1;
   }
}

int  dsk_findextrec(hdd_info *info, u32t quadpos) {
   if (!info) return -1;
   if (info->pt_size>>2 <= quadpos) return -1; else {
      u32t ii;
      struct MBR_Record *rec = info->pts + quadpos*4;
      for (ii=0; ii<4; ii++)
         if (IS_EXTENDED(rec[ii].PTE_Type))
            if (quadpos || info->extpos==rec[ii].PTE_LBAStart) return ii;
      return -1;
   }
}

int  dsk_findlogrec(hdd_info *info, u32t quadpos) {
   if (!info) return -1;
   if (info->pt_size>>2 <= quadpos) return -1; else {
      u32t ii;
      struct MBR_Record *rec = info->pts + quadpos*4;
      for (ii=0; ii<4; ii++) {
         u8t pt = rec[ii].PTE_Type;
         if (pt && !IS_EXTENDED(pt)) return ii;
      }
      return -1;
   }
}

u32t dsk_wipequad(u32t disk, u32t quadidx, int delsig) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (hi->pt_size>>2 <= quadidx) return DPTE_INVARGS; else {
      struct MBR_Record *rec;
      char        mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR   *mbr = (struct Disk_MBR *)&mbr_buffer;
      u32t   rc;
      // read appropriate 4 records
      if (!hlp_diskread(disk, hi->ptspos[quadidx], 1, mbr)) return DPTE_ERRREAD;
      memset(&mbr->MBR_Table, 0, sizeof(struct MBR_Record) * 4);
      if (delsig) mbr->MBR_Signature = 0;
      // and write it back
      rc = hlp_diskwrite(disk, hi->ptspos[quadidx], 1, mbr)?0:DPTE_ERRWRITE;
      log_it(2,"wipe pt: %X, quad %d, sector %08X, rc %d\n", disk, quadidx, 
         hi->ptspos[quadidx], rc);
      return rc;
   }
}

u32t dsk_flushquad(u32t disk, u32t quad) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (hi->pt_size>>2 <= quad) return DPTE_INVARGS; else {
      struct MBR_Record *rec;
      char        mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR   *mbr = (struct Disk_MBR *)&mbr_buffer;
      u32t  ii;

      // write appropriate 4 records
      if (!hlp_diskread(disk, hi->ptspos[quad], 1, mbr)) return DPTE_ERRREAD;
      memcpy(&mbr->MBR_Table, hi->pts+quad*4, sizeof(struct MBR_Record) * 4);
      // fix LBA pos back to disk values
      if (quad)
         for (ii=0; ii<4; ii++) {
            u32t lba = mbr->MBR_Table[ii].PTE_LBAStart;
            if (lba) {
               if (IS_EXTENDED(mbr->MBR_Table[ii].PTE_Type)) {
                  if (lba<=hi->extpos || !hi->extpos) return DPTE_EXTENDED;
                  lba-=hi->extpos;
               } else 
                  lba-=hi->ptspos[quad];
      
               mbr->MBR_Table[ii].PTE_LBAStart = lba;
            }
         }
      ii = hlp_diskwrite(disk, hi->ptspos[quad], 1, mbr)?0:DPTE_ERRWRITE;
      log_it(2,"flush pt: %X, quad %d, sector %08X, rc %d\n", disk, quad, 
         hi->ptspos[quad], ii);
      return ii;
   }
}

u32t _std pt_action(u32t action, u32t disk, u32t index, u32t arg) {
   hdd_info *hi;
   u32t      rc;
   if ((action&ACTION_NORESCAN)==0) dsk_ptrescan(disk,1);
   hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (hi->scan_rc) return hi->scan_rc;

   /* function exits here if GPT index specified, but allow
      mixed partition processing */
   if ((action&ACTION_DIRECT)!=0 && hi->pt_size<=index ||
       (action&ACTION_DIRECT)==0 && hi->pt_view<=index) 
      return DPTE_PINDEX; 
   else {
      struct MBR_Record *rec;
      char        mbr_buffer[MAX_SECTOR_SIZE];
      struct Disk_MBR   *mbr = (struct Disk_MBR *)&mbr_buffer;
      u32t   quadpos, rescan = 0, ii,
                       v_idx = action&ACTION_DIRECT?x7FFF:index;

      if ((action&ACTION_DIRECT)==0) {
         u32t *pos = memchrd(hi->index, index, hi->pt_size);
         if (!pos) return DPTE_PINDEX;
         index = pos - hi->index;
      }
      rec     = hi->pts + index;
      quadpos = index>>2;

      switch (action&ACTION_MASK) {
         case ACTION_SETACTIVE:
            // allow active bit for primary partitons only
            if (index>=4) return DPTE_PRIMARY;
            if (IS_EXTENDED(rec->PTE_Type)) return DPTE_EXTENDED;
            for (ii=0; ii<4; ii++) hi->pts[ii].PTE_Active&=0x7F;
            rec->PTE_Active|=0x80;
            break;
         case ACTION_SETTYPE:
            // we must rescan disk after changing from/to extended pt type
            if (rec->PTE_Type==arg) return 0;
            if (IS_EXTENDED(rec->PTE_Type) || IS_EXTENDED(arg)) rescan=1;
            rec->PTE_Type = arg;
            break;
         case ACTION_DELETE:
            if (rec->PTE_LBAStart!=arg) return DPTE_RESCAN;
            // del partition record from LVM (if it exists) and record itself
            if (v_idx!=x7FFF) lvm_delptrec(disk, v_idx);
            memset(rec, 0, sizeof(struct MBR_Record));
            // we must rescan disk after deleting partition
            rescan=1;
            break;
         default:
            return DPTE_INVDISK;
      }

      // write appropriate 4 records
      if (!hlp_diskread(disk, hi->ptspos[quadpos], 1, mbr)) return DPTE_ERRREAD;
      memcpy(&mbr->MBR_Table, hi->pts+quadpos*4, sizeof(struct MBR_Record) * 4);
      // fix LBA pos back to disk values
      if (quadpos)
         for (ii=0; ii<4; ii++) {
            u32t lba = mbr->MBR_Table[ii].PTE_LBAStart;
            if (lba) {
               if (IS_EXTENDED(mbr->MBR_Table[ii].PTE_Type)) {
                  if (lba<=hi->extpos || !hi->extpos) return DPTE_EXTENDED;
                  lba-=hi->extpos;
               } else
                  lba-=hi->ptspos[quadpos];

               mbr->MBR_Table[ii].PTE_LBAStart = lba;
            }
         }
      rc = hlp_diskwrite(disk, hi->ptspos[quadpos], 1, mbr)?0:DPTE_ERRWRITE;
      // rescan disk after extended type was set or removed
      if (!rc && rescan) dsk_ptrescan(disk,1);
      return rc;
   }
}

u32t _std dsk_setactive(u32t disk, u32t index) {
   u32t rc = pt_action(ACTION_SETACTIVE, disk, index, 0);
   log_it(2,"set active: %X, index %d, rc %d\n", disk, index, rc);
   return rc;
}

u32t _std dsk_setpttype(u32t disk, u32t index, u8t type) {
   return pt_action(ACTION_SETTYPE, disk, index, type);
}

u32t _std dsk_mountvol(u8t *vol, u32t disk, u32t index) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return DPTE_INVDISK;
   if (!vol || *vol==DISK_LDR || *vol>=DISK_COUNT) return DPTE_INVARGS; else {
      u32t     ii, mdsk;
      u64t  start, size = 0;
      // check for LM_CACHE_ON_MOUNT and load cache if asked
      cache_envstart();
      // scan disk at least once (console "mount" force it, this function - no)
      dsk_ptrescan(disk,0);
      // get partition info
      if (!dsk_ptquery64(disk, index, &start, &size, 0, 0)) return DPTE_PINDEX;
      if (size>=_4GBLL) return DPTE_LARGE;
      // check - is it mounted already?
      for (ii=0; ii<DISK_COUNT; ii++)
         if ((u32t)dsk_volindex(ii, &mdsk)==index)
            if (mdsk==disk) {
               *vol = ii;
               return DPTE_MOUNTED;
            }
      // search for free letter
      if (!*vol)
         for (ii=2; ii<DISK_COUNT; ii++) {
            disk_volume_data  vi;
            hlp_volinfo(ii, &vi);
            if (!vi.TotalSectors) { *vol = ii; break; }
         }
      if (!*vol) return DPTE_NOLETTER;
      // mount
      if (!hlp_mountvol(*vol, disk, start, size))
         return DPTE_INVARGS;
      return 0;
   }
}

long _std dsk_partcnt(u32t disk) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return -1;
   // just for first time scan
   dsk_ptrescan(disk,0);
   if (hi->scan_rc) return -1;
   return hi->gpt_view;
}

u32t _std dsk_unmountall(u32t disk) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return 0; else {
      u32t umcnt=0, ii;
      // unmount volumes
      for (ii=2; ii<DISK_COUNT; ii++) {
         disk_volume_data vi;
         hlp_volinfo(ii,&vi);
         if (vi.Disk==disk) { hlp_unmountvol(ii); umcnt++; }
      }
      return umcnt;
   }
}

/* ------------------------------------------------------------------------ */
typedef struct {
   u32t     p1;
   u16t     p2;
   u16t     p3;
   u64t     p4;
} _guidrec;

/** convert GPT GUID to string.
    @param  guid     16 bytes GUID array
    @param  str      target string (at least 38 bytes)
    @return boolean (success flag) */
int _std dsk_guidtostr(void *guid, char *str) {
   _guidrec *gi = (_guidrec*)guid;
   if (!guid || !str) return 0; else {
      _guidrec *gi = (_guidrec*)guid;
      u64t     sw4 = bswap64(gi->p4);

      sprintf(str, "%08X-%04X-%04X-%04X-%012LX", gi->p1, gi->p2, gi->p3,
         (u16t)(sw4>>48), sw4&0xFFFFFFFFFFFFUL);
   }
   return 1;
}

/** convert string to GPT GUID.
    String format must follow example: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.
    @param  str      source string
    @param  guid     16 bytes GUID array
    @return boolean (success flag) */
int _std dsk_strtoguid(const char *str, void *guid) {
   if (!guid || !str) return 0;
   while (*str==' ' || *str=='\t') str++;

   if (strlen(str)<36) return 0; else
   if (str[8]!='-' || str[13]!='-' || str[18]!='-' || str[23]!='-') return 0; else 
   {
      _guidrec *gi = (_guidrec*)guid;
      char     *ep;
      u32t      p1 = strtoul(str, &ep, 16), px[3], ii;
      u64t      p4;

      if (ep-str!=8) return 0;
      str += 9;
      for (ii=0; ii<3; ii++) {
         px[ii] = strtoul(str, &ep, 16);
         if (ep-str!=4) return 0;
         str  += 5;
      }
      p4 = strtoull(str, 0, 16);
      if (p4>0xFFFFFFFFFFFFUL) return 0;

      gi->p1 = p1;
      gi->p2 = px[0];
      gi->p3 = px[1];
      gi->p4 = bswap64(p4|(u64t)px[2]<<48);
   }
   return 1;
}

/** generate pseudo-unique GPT GUID.
    @param  [out] guid  16 bytes GUID array */
void _std dsk_makeguid(void *guid) {
   _guidrec *gi = (_guidrec*)guid;

   gi->p1 = random(FFFF);
   srand(tm_counter());
   gi->p2 = random(0xFFFF);
   gi->p3 = 0x4000 | random(0xFFF);
   gi->p4 = (u64t)random(FFFF) << 32 | random(FFFF);
}

/** check disk/partition type (GPT or MBR).
    @param disk     Disk number.
    @param index    Partition index (use -1 to check entire disk type).

    Note: If disk is hybrid, but partition located in MBR only - function will return 0,
    not 2 for it.

    @return -1 on invalid disk number/partition index, 0 for MBR disk/partition,
       1 for GPT, >1 if disk have hybrid partition table (both GPT and MBR) or 
       partition is hybrid (located both un MBR and GPT) */
int   _std dsk_isgpt(u32t disk, long index) {
   hdd_info *hi = get_by_disk(disk);
   if (!hi) return -1; else {
      u32t *pos;
      // scan it on first use
      if (!hi->inited) dsk_ptrescan(disk, 0);

      if (index < 0) return hi->gpt_present?(hi->hybrid?2:1):0;
      if (index >= hi->gpt_view) return -1;
      // GPT
      if (index >= hi->pt_view) return 1;
      // MBR
      if (!hi->hybrid) return 0;
      // searching index in GPT for hybrid disk
      pos = memchrd(hi->gpt_index, index, hi->gpt_size);
      return pos ? 2 : 0;
   }
}

/** query GPT partition info.
    Function deny all types of extended partition.
    @param  [in]  disk    Disk number
    @param  [in]  index   Partition index
    @param  [out] pinfo   Buffer for partition info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if partition is MBR). */
u32t  _std dsk_gptpinfo(u32t disk, u32t index, dsk_gptpartinfo *pinfo) {
   hdd_info *hi = get_by_disk(disk);

   if (!hi) return DPTE_INVDISK; else {
      // this call scan disk if required
      int type = dsk_isgpt(disk, -1);
      if (!type) return DPTE_MBRDISK;
      // check index
      if (index>=hi->gpt_view) return DPTE_PINDEX;
      // and partition type if hybrid
      if (type>1) type = dsk_isgpt(disk, index);
      if (type<=0) return DPTE_MBRDISK;

      if (pinfo) {
         u32t *pos = memchrd(hi->gpt_index, index, hi->gpt_size);

         if (!pos) {
            memset(pinfo, 0, sizeof(dsk_gptpartinfo)); 
            return DPTE_PINDEX;
         } else {
            struct GPT_Record  *pte = hi->ptg + (pos - hi->gpt_index);

            memcpy(pinfo->TypeGUID, pte->PTG_TypeGUID, 16);
            memcpy(pinfo->GUID, pte->PTG_GUID, 16);
            memcpy(pinfo->Name, pte->PTG_Name, sizeof(pinfo->Name));
            pinfo->StartSector = pte->PTG_FirstSec;
            pinfo->Length      = pte->PTG_LastSec - pte->PTG_FirstSec + 1;
            pinfo->Attr        = pte->PTG_Attrs;
         } 
      }
      return 0;
   }
}

/** query GPT disk info.
    @param  [in]  disk    Disk number
    @param  [out] pinfo   Buffer for disk info
    @return 0 on success or DPTE_* constant (DPTE_MBRDISK if disk is MBR). */
u32t  _std dsk_gptdinfo(u32t disk, dsk_gptdiskinfo *dinfo) {
   hdd_info *hi = get_by_disk(disk);

   if (!hi) return DPTE_INVDISK; else 
   if (!dinfo) return DPTE_INVARGS; else {
      // this call scan disk if required
      int type = dsk_isgpt(disk, -1);
      if (!type) return DPTE_MBRDISK;

      memcpy(dinfo->GUID, hi->ghead->GPT_GUID, 16);
      dinfo->UserFirst = hi->ghead->GPT_UserFirst;
      dinfo->UserLast  = hi->ghead->GPT_UserLast;
      dinfo->Hdr1Pos   = hi->ghead->GPT_Hdr1Pos;
      dinfo->Hdr2Pos   = hi->ghead->GPT_Hdr2Pos;
   }
   return 0;
}

/* ------------------------------------------------------------------------ */
int confirm_dlg(const char *text) {
   u32t wrc = vio_msgbox("PARTMGR WARNING!", text, MSG_LIGHTRED|MSG_DEF2|MSG_YESNO, 0);
   return wrc==MRES_YES?1:0;
}

/* ------------------------------------------------------------------------ */
typedef int  _std (*t_hlp_cacheon)(u32t disk, int enable);
typedef void _std (*t_hlp_cacheprio)(u32t disk, u64t start, u32t size, int on);
typedef void _std (*t_hlp_cachesize)(u32t size_mb);

static u32t             cachedll_mod     = 0; // cache.dll handle
static t_hlp_cacheon    fp_hlp_cacheon   = 0;
static t_hlp_cacheprio  fp_hlp_cacheprio = 0;
static t_hlp_cachesize  fp_hlp_cachesize = 0;
static cmd_eproc        fp_shl_cache     = 0;

static void cache_load(void) {
   if (cachedll_mod) return;
   // increment CACHE.DLL usage to prevent unloading while we are active
   cachedll_mod = mod_query(MODNAME_CACHE, MODQ_LOADED);
   if (!cachedll_mod) return;
   fp_hlp_cacheon   = (t_hlp_cacheon)  mod_getfuncptr(cachedll_mod, ORD_CACHE_hlp_cacheon);
   fp_hlp_cacheprio = (t_hlp_cacheprio)mod_getfuncptr(cachedll_mod, ORD_CACHE_hlp_cachepriority);
   fp_hlp_cachesize = (t_hlp_cachesize)mod_getfuncptr(cachedll_mod, ORD_CACHE_hlp_cachesize);
   fp_shl_cache     = (cmd_eproc)      mod_getfuncptr(cachedll_mod, ORD_CACHE_shl_cache);
}

void cache_unload(void) {
   if (cachedll_mod) mod_free(cachedll_mod);
   fp_hlp_cacheprio = 0;
   fp_hlp_cachesize = 0;
   fp_hlp_cacheon   = 0;
   fp_shl_cache     = 0;
   cachedll_mod     = 0;
}

int dsk_cacheset(u32t disk, int enable) {
   if (!cachedll_mod) cache_load();
   if (!cachedll_mod || !fp_hlp_cacheon) return -1;
   // call cache indirectly
   return (*fp_hlp_cacheon)(disk,enable);
}

#define KEY_NAME  "LM_CACHE_ON_MOUNT"
/* parse env key:
   "LM_CACHE_ON_MOUNT = ON [, 5%]" */
void cache_envstart(void) {
   static int ccloadcall = 1;
   // check it only once!
   if (ccloadcall) {
      char *cckey = getenv(KEY_NAME);
      ccloadcall  = 0;
      if (cckey) {
         str_list *lst = str_split(cckey,",");
         int    ccload = env_istrue(KEY_NAME);
         if (ccload<0) ccload=0;
         if (ccload) {
            // loaded?
            cache_load();
            // no, load it!
            if (!cachedll_mod) {
               cmd_shellcall(shl_loadmod,"/q " MODNAME_CACHE,0);
               cache_load();
               // call cache command only if WE ARE load it
               if (fp_shl_cache && lst->count>1 && isdigit(lst->item[1][0]))
                  cmd_shellcall(fp_shl_cache,lst->item[1],0);
            }
         }
         free(lst);
      }
   }
}
