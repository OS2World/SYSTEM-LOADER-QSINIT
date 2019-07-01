//
// QSINIT "bootos2" module
// advanced disk tasks
//
#include "qsbase.h"
#include "qsint.h"
#include "qsdm.h"
#include "stdlib.h"
#include "doshlp.h"
#include "qsconst.h"
#include "dparm.h"
#include "ldrparam.h"
#include "parttab.h"
#include "partmgr_ord.h"
#include "filetab.h"

typedef struct {
   u16t    DPCylcount;
   u16t         DPSPT;
   u16t       DPHeads;
   u16t     DPPrecomp;
   u16t       DPFlags;
} DHDriveParams;

typedef int   (_std *fplvm_partinfo)(u32t disk, u32t index, lvm_partition_data *info);
typedef long  (_std *fpvol_index)(u8t vol, u32t *disk);

static u32t   partmgr = 0;
static fplvm_partinfo   plvm_partinfo   = 0;
static fpvol_index      pvol_index      = 0;
// variables for switch code setup
u32t         paeppd, swcode;
u8t              *swcodelin;
extern u8t    switch_code[];
extern u16t  switch_codelen;

extern void __far _cdecl pae_setup(u32t hdrpage, u32t hdrcopy, u32t dwbuf,
                                   u16t pdsel, u16t bhseg);

void error_exit(int code, const char *message);

static void dmgr_free(void) {
   if (partmgr) {
      mod_free(partmgr);
      plvm_partinfo = 0;
      pvol_index = 0;
      partmgr = 0;
   }
}

static int dmgr_load(void) {
   if (!partmgr) {
      partmgr = mod_query(MODNAME_DMGR,0);
      if (!partmgr) partmgr = mod_searchload(MODNAME_DMGR, 0, 0);
      // query functions & install unload proc
      if (partmgr) {
         plvm_partinfo   = (fplvm_partinfo)   mod_getfuncptr(partmgr,ORD_PARTMGR_lvm_partinfo);
         pvol_index      = (fpvol_index)      mod_getfuncptr(partmgr,ORD_PARTMGR_vol_index);
         atexit(dmgr_free);
      }
   }
   return partmgr && plvm_partinfo && pvol_index?1:0;
}

void print_bpb(struct Disk_BPB *bpb) {
   log_printf("Boot disk BPB data:\n");
   log_printf(" * BytesPerSec    %5d\n", bpb->BPB_BytesPerSec);
   log_printf(" * SecPerClus     %5d\n", bpb->BPB_SecPerClus );
   log_printf(" * Res Sectors    %5d\n", bpb->BPB_ResSectors );
   log_printf(" * FAT Copies     %5d\n", bpb->BPB_FATCopies  );
   log_printf(" * Root Entries   %5d\n", bpb->BPB_RootEntries);
   log_printf(" * Total Sec      %5d\n", bpb->BPB_TotalSec   );
   log_printf(" * Media Byte        %02X\n",bpb->BPB_MediaByte);
   log_printf(" * SecPerFAT      %5d\n", bpb->BPB_SecPerFAT  );
   log_printf(" * SecPerTrack    %5d\n", bpb->BPB_SecPerTrack);
   log_printf(" * Heads          %5d\n", bpb->BPB_Heads      );
   log_printf(" * HiddenSec   %08X\n"  , bpb->BPB_HiddenSec  );
   log_printf(" * TotalSecBig %08X\n"  , bpb->BPB_TotalSecBig);
   log_printf(" * Boot Disk         %02X\n",bpb->BPB_BootDisk);
   log_printf(" * Boot Drive Number %02X\n",bpb->BPB_BootLetter);
}

int replace_bpb(u8t vol, struct Disk_BPB *pbpb, u8t *pbootflags, 
                void **pmfsptr, u32t *pmfssize)
{
   u8t           sbuf[512];
   disk_volume_data     vi;
   struct Boot_Record  *br = (struct Boot_Record*)sbuf;
   u32t             vol_fs;
   if (!pbpb || vol<=DISK_LDR) return 0;
   // check FS type on volume
   vol_fs = hlp_volinfo(vol, &vi);
   // OS/2 boot will fail on 1024/2048/4096
   if (vi.SectorSize!=512) return 0;
   // read boot sector to get BPB data
   if (!hlp_diskread(vi.Disk, vi.StartSector, 1, br)) return 0;

   *pmfsptr    = 0;
   *pmfssize   = 0;
   if (vol_fs!=FST_NOTMOUNTED && vol_fs<FST_OTHER) { // FAT/FAT32/exFAT
      *pbootflags = 0;
      if (vol_fs==FST_EXFAT) return 0;
   } else {                      
      if (vol_fs==FST_OTHER) {   // this can be only HPFS now
         char  *path = sprintf_dyn("%c:\\OS2BOOT", 'A'+vol);
         *pmfsptr    = freadfull(path, pmfssize);
         *pbootflags = BF_MINIFSD|BF_MICROFSD;
         free(path);
      }
      if (!*pmfsptr || !*pmfssize) 
         error_exit(10,"Unable to find OS2BOOT file on the SOURCE volume!\n");
   }
   pbpb->BPB_BytesPerSec = vi.SectorSize;
   pbpb->BPB_SecPerClus  = br->BR_BPB.BPB_SecPerClus;
   pbpb->BPB_ResSectors  = br->BR_BPB.BPB_ResSectors;
   pbpb->BPB_FATCopies   = br->BR_BPB.BPB_FATCopies;
   pbpb->BPB_RootEntries = br->BR_BPB.BPB_RootEntries;
   pbpb->BPB_TotalSec    = vi.TotalSectors<65535?vi.TotalSectors:0;
   pbpb->BPB_MediaByte   = br->BR_BPB.BPB_MediaByte;
   pbpb->BPB_SecPerFAT   = br->BR_BPB.BPB_SecPerFAT;
   pbpb->BPB_SecPerTrack = br->BR_BPB.BPB_SecPerTrack;
   pbpb->BPB_Heads       = br->BR_BPB.BPB_Heads;
   pbpb->BPB_HiddenSec   = vi.StartSector;
   pbpb->BPB_TotalSecBig = vi.TotalSectors>65535?vi.TotalSectors:0;
   pbpb->BPB_BootDisk    = hlp_diskbios(vi.Disk,1);
   pbpb->BPB_BootLetter  = vi.Disk&QDSK_FLOPPY ? 0 : 0x80;
   // query LVM drive letter
   if (dmgr_load()) {
      long vidx = pvol_index(vol, 0);
      if (vidx>=0) {
         lvm_partition_data li;
         if (plvm_partinfo(vi.Disk, vidx, &li))
            if (li.Letter && li.Letter>'C')
               pbpb->BPB_BootLetter = 0x80+(li.Letter-'C');
      }
   }
   return 1;
}

#define GET_ADDR(x) (((x)<efd->DisPartOfs?flat1000:bhpart) + (x))

int setup_ramdisk(u8t disk, struct ExportFixupData *efd, u8t *bhpart) {
   u8t      *flat1000 = (u8t*)hlp_segtoflat(DOSHLP_SEG);
   void          *dpt = GET_ADDR(efd->DPOfs);
   u8t           *dft = GET_ADDR(efd->DFTabOfs);
   struct EDParmTable   *edt = (struct EDParmTable *)  GET_ADDR(efd->EDTabOfs);
   struct EDDParmTable *eddt = (struct EDDParmTable *) GET_ADDR(efd->EDDTabOfs);
   HD4_Header     *dh = 0,
                 *dhc = 0;
   u16t         pdsel = 0;
   u32t           res = 0;

   dh        = pag_physmap((u64t)efd->HD4Page<<PAGESHIFT, PAGESIZE, 0);
   dhc       = pag_physmap(paeppd, PAEIO_MEMREQ, 0);
   swcodelin = pag_physmap(swcode, PAGESIZE, 0);
   pdsel     = hlp_selalloc(1);

   if (dh && dhc && pdsel && swcodelin) {
      hlp_selsetup(pdsel, (u32t)dhc, _64KB-1, QSEL_BYTES|QSEL_LINEAR|QSEL_16BIT);
      // copy switch code to 1Mb buffer
      memset(swcodelin+switch_codelen, 0, PAGESIZE-switch_codelen);
      memcpy(swcodelin, &switch_code, switch_codelen);
      // store variables 
      *(u32t*)GET_ADDR(efd->Pswcode) = swcode;
      *(u32t*)GET_ADDR(efd->Ppaeppd) = paeppd;
      // create page dir, GDT and so on
      //log_printf("%08X=%08X, %08X=%08X, \n", dhc, paeppd, dh, efd->HD4Page<<PAGESHIFT);
      memcpy(dhc, dh, PAGESIZE);
      pae_setup(efd->HD4Page, paeppd, paeppd+_64KB, pdsel, efd->DisPartSeg);

      /* vdisk module allocates fake BIOS disk number for own virtual HDD.
         We should get it here and it must fits into the empty slot in
         EDParmTable table */
      log_printf("disk %02X, hcopy %08X\n", disk, paeppd);
      // creating fake disk info
      if (disk<INT13E_TABLESIZE) {
         dft[disk] = 8; // HDDF_HOTSWAP;
         edt[disk].ED_BufferSize  = sizeof(struct EDParmTable);
         edt[disk].ED_Flags       = 2; // CHS info valid 
         edt[disk].ED_Cylinders   = dh->h4_cyls;
         edt[disk].ED_Heads       = dh->h4_heads;
         edt[disk].ED_SectOnTrack = dh->h4_spt;
         edt[disk].ED_Sectors     = dh->h4_pages<<12-9;
         edt[disk].ED_SectorSize  = 512;
         edt[disk].ED_EDDTable    = FFFF;
      
         memset(eddt + disk, 0, sizeof(struct EDDParmTable));
      
         if (disk<2) {
            DHDriveParams *dp = (DHDriveParams*)dpt + disk + 2;
            dp->DPPrecomp  = 0;
            dp->DPFlags    = 1; // NONREMOVABLE
            dp->DPCylcount = dh->h4_cyls;
            dp->DPSPT      = dh->h4_spt;
            dp->DPHeads    = dh->h4_heads;
         }
      }
      res = 1;
   } else
      log_printf("alloc err: %04X %08X %08X\n", pdsel, dh, dhc);

   if (swcodelin) pag_physunmap(swcodelin);
   if (pdsel) hlp_selfree(pdsel);
   if (dhc)   pag_physunmap(dhc);
   if (dh)    pag_physunmap(dh);

   return res;
}
