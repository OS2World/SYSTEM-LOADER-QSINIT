#include "fmkfs.h"

#define GPT_MINSIZE  (16384/GPT_RECSIZE)

extern u32t *crc_table;
u32t           crc_buf[256];

static u32t    winguid[4] = {0xEBD0A0A2, 0x4433B9E5, 0xB668C087, 0xC79926B7};
static u32t    efiguid[4] = {0xC12A7328, 0x11D2F81F, 0xA0004BBA, 0x3BC93EC9};

void make_crc_table(void *table);
u32t _std crc32(u32t crc, u8t* buf, u32t len);

void disk_check(media *dst, u32t *vol_start, u32t *vol_size, u8t *sec_shift) {
   u32t           ii, size = 0;
   u8t                *buf = (u8t*)malloc(4096);
   struct Disk_MBR    *mbr = (struct Disk_MBR*)buf;
   struct MBR_Record  *rec = &mbr->MBR_Table[0];
   struct Boot_Record *bpb = (struct Boot_Record *)buf;

   *sec_shift = 0;
   dst->type  = MTYPE_MBR;
   dst->spt   = 63;
   dst->heads = 254;

   if (!dst->phys) {
      size = fsize(dst->df);

      fseek(dst->df, -512, SEEK_END);
      if (fread(buf,1,512,dst->df)==512) {
         vhd_footer  *vh = (vhd_footer*)buf;
      
         if (memcmp(&vh->creator,"conectix",8)==0 && vh->type==swapdd(2)
            && vh->nextdata==FFFF64)
         {
            *vol_size  = --size;
            *sec_shift = 9;
            dst->type |= MTYPE_VHD;
            dst->spt   = vh->spt; 
            dst->heads = vh->heads;       
         }
      }
   } else {
#ifdef PHYS_IO
      dsk_geo_data geo;
      u32t ssize = 512;
      size = dsk_size(dst->dsk, &ssize, &geo);
      *sec_shift = bsf32(ssize);
      dst->spt   = geo.SectOnTrack;
      dst->heads = geo.Heads;
#endif
   }

   memset(buf, 0, 4096);
   if (!dst->phys) {
      rewind(dst->df);
      fread(buf,1,512,dst->df);
   } else {
#ifdef PHYS_IO
      dsk_read(dst->dsk, 0, 1, buf);
#endif
   }
   // try to check file as a big floppy
   ii = bsr32(bpb->BR_BPB.BPB_BytePerSect);
   
   if (ii==bsf32(bpb->BR_BPB.BPB_BytePerSect) && ii>=9 && ii<=12
      && bpb->BR_BPB.BPB_FATCopies<=2)
   {
      if (*sec_shift==0) *sec_shift = ii;
      dst->type  = dst->type&~MTYPE_TYPEMASK | MTYPE_FLOPPY;
   }
   if (*sec_shift==0) *sec_shift = 9;

   *vol_start = 0;
   *vol_size  = dst->phys ? size : size>>*sec_shift;

   memset(&dst->mbrpos, 0, sizeof(dst->mbrpos));
   memset(&dst->mbrtype, 0, sizeof(dst->mbrtype));

   /* check for GPT disk, but ignore any errors (FatFs will deny
      wrong volume position for us) */
   if (*sec_shift==9 && (dst->type&MTYPE_TYPEMASK)==MTYPE_MBR) {
      for (ii=0; ii<4; ii++) {
         dst->mbrpos [ii] = rec[ii].PTE_LBAStart;
         dst->mbrtype[ii] = rec[ii].PTE_Type;
      }

      for (ii=0; ii<4; ii++)
         if (rec[ii].PTE_Type==PTE_EE_UEFI) {
            u32t   pass, arraysize, pidx;
            struct GPT_Record *list;
            struct Disk_GPT     *pt = (struct Disk_GPT *)malloc(512);
   
            memset(pt, 0, 512);
   
            if (!dst->phys) {
               fseek(dst->df, rec[ii].PTE_LBAStart<<9, SEEK_SET);
               fread(pt,1,512,dst->df);
            } else {
#ifdef PHYS_IO
            dsk_read(dst->dsk, rec[ii].PTE_LBAStart, 1, pt);
#endif
            }
            if (pt->GPT_Sign!=GPT_SIGNMAIN || pt->GPT_PtEntrySize!=GPT_RECSIZE) {
               printf("wrong GPT header!\n");
               exit(2);
            }
            arraysize = Round512(pt->GPT_PtCout*GPT_RECSIZE);
            list = (struct GPT_Record *)malloc(arraysize);
      
            memset(list, 0, arraysize);
      
            if (!dst->phys) {
               fseek(dst->df, pt->GPT_PtInfo<<9, SEEK_SET);
               fread(list, 1, arraysize, dst->df);
            } else {
#ifdef PHYS_IO
               dsk_read(dst->dsk, pt->GPT_PtInfo, arraysize>>9, list);
#endif
            }
            /* searching for EFI Boot GUID at the 1st pass and first
               available "Windows Data" GUID during second */
            for (pass=0; pass<2 && *vol_start==0; pass++)
               for (pidx=0; pidx<pt->GPT_PtCout; pidx++)
                  if (memcmp(&list[pidx].PTG_TypeGUID, pass?&winguid:&efiguid, 16)==0) {
                     *vol_start = list[pidx].PTG_FirstSec;
                     *vol_size  = list[pidx].PTG_LastSec - list[pidx].PTG_FirstSec + 1;
      
                     dst->type  = dst->type&~MTYPE_TYPEMASK | MTYPE_GPT;
                     break;
                  }
            free(list);
            free(pt);
            if (*vol_start) break;
         } else
         if (rec[ii].PTE_Type==PTE_EF_UEFI) {
            *vol_start = rec[ii].PTE_LBAStart;
            *vol_size  = rec[ii].PTE_LBASize;
            dst->type |= MTYPE_EF;
            break;
         }
   }
   free(buf);
}

void disk_init(u32t disk_size, u32t *vol_start, u32t *vol_size) {
   static u16t  *str_name = (u16t*)L"EFI Boot";
   struct Disk_GPT    *pt = (struct Disk_GPT *)calloc(512, 1),
                     *pt2 = (struct Disk_GPT *)malloc(512);
   struct GPT_Record *rec = (struct GPT_Record *)calloc(GPT_MINSIZE, GPT_RECSIZE);
   u32t            reccap = GPT_MINSIZE, ii,
              rec_sectors = reccap * GPT_RECSIZE >> 9;

   pt->GPT_Sign      = GPT_SIGNMAIN;
   pt->GPT_Revision  = 0x10000;
   pt->GPT_HdrSize   = sizeof(struct Disk_GPT);
   pt->GPT_Hdr1Pos   = 1;
   pt->GPT_Hdr2Pos   = disk_size - 1;
   pt->GPT_UserFirst = pt->GPT_Hdr1Pos + rec_sectors + 1;
   pt->GPT_UserLast  = pt->GPT_Hdr2Pos - rec_sectors - 1;

   pt->GPT_PtInfo    = pt->GPT_Hdr1Pos + 1;
   pt->GPT_PtCout    = reccap;
   pt->GPT_PtEntrySize = GPT_RECSIZE;

   makeguid(&pt->GPT_GUID);
   // create single partition
   for (ii=0; ii<35; ii++)
      if (!str_name[ii]) break; else rec->PTG_Name[ii] = str_name[ii];

   memcpy(rec->PTG_TypeGUID, &efiguid, 16);
   makeguid(rec->PTG_GUID);
   // set to 0 to align volume on 1Mb
#if 1
   rec->PTG_FirstSec = pt->GPT_UserFirst;
   rec->PTG_LastSec  = pt->GPT_UserLast;
#else
   rec->PTG_FirstSec = Round2k(pt->GPT_UserFirst);
   rec->PTG_LastSec  = pt->GPT_UserLast & ~(2048-1);
#endif
   // cut it to custom asked size
   if (*vol_size && *vol_size<rec->PTG_LastSec-rec->PTG_FirstSec+1)
      rec->PTG_LastSec = rec->PTG_FirstSec + *vol_size - 1;
   // no attrs
   rec->PTG_Attrs    = 0;
   // use crc32 code from zlib (linked into QSINIT binary)
   make_crc_table(crc_table = crc_buf);
   // crc for partition records
   pt->GPT_PtCRC     = crc32(crc32(0,0,0), (u8t*)rec, rec_sectors<<9);
   // build the second header
   memcpy(pt2, pt, 512);
   pt2->GPT_PtInfo   = pt->GPT_Hdr2Pos - rec_sectors;
   pt2->GPT_Hdr1Pos  = disk_size - 1;
   pt2->GPT_Hdr2Pos  = 1;
   // update crc32 in headers
   pt->GPT_HdrCRC    = 0;
   pt->GPT_HdrCRC    = crc32(crc32(0,0,0), (u8t*)pt, sizeof(struct Disk_GPT));
   pt2->GPT_HdrCRC   = 0;
   pt2->GPT_HdrCRC   = crc32(crc32(0,0,0), (u8t*)pt2, sizeof(struct Disk_GPT));

   if (disk_write(0,(BYTE*)pt,1,1) != RES_OK ||
      disk_write(0,(BYTE*)pt2,disk_size-1,1) != RES_OK ||
         disk_write(0,(BYTE*)rec,2,1) != RES_OK ||
            disk_write(0,(BYTE*)rec,pt2->GPT_PtInfo,1) != RES_OK) {
               printf("error while writing disk image!\n");
               exit(2);
            }

   *vol_start = rec->PTG_FirstSec;
   *vol_size  = rec->PTG_LastSec - rec->PTG_FirstSec + 1;

   free(rec); free(pt2); free(pt);
}
