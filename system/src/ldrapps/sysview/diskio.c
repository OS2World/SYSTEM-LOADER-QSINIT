#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
   There is no reason to implement any i/o except QSINIT self.
   Volume i/o for Windows & OS/2 implemented in src\tools\src\bootset\volio.c,
   but not used here...
*/

// ************************************************************************
// emulated i/o
// ************************************************************************
#ifdef EMU_IO
#include <sp_defs.h>  // memchrn?, fsize
#include <io.h>

#define DISKETTE_SIZE   2880                 // floppy 1.44
#define HDD_SIZE        (160*1000*1000*2)    // hdd    160Gb
#define SLICE_SIZE      (1024*1024)          // must be < floppy
#define SLICE_SECTORS   (1024*1024>>9)

ul dsk_count(ul *floppies,ul *vdisks) {
   if (vdisks) *vdisks = 0;
   if (floppies) *floppies = 1;
   return 1;
}
static FILE  *edsk = 0;
static ul   *index = 0,
            slices = 0;

void ed_close(void) {
   if (!edsk) return;
   fclose(edsk);
   edsk = 0;
}

void ed_open(void) {
   if (edsk) return;
   atexit(ed_close);
   edsk=fopen("emudisk.bin","w+b");
   if (edsk) {
      ul    sz = fsize(edsk),
        flsize = DISKETTE_SIZE*512;
      // initialize disk data
      if (sz<flsize) {
         // writing empty floppy disk
         void *floppy = malloc(flsize);
         slices = 1;
         index = (ul*)malloc(SLICE_SECTORS*4);
         memset(index,0xFF,SLICE_SECTORS*4);
         memset(floppy,0,flsize);
         fwrite(floppy,1,flsize,edsk);
         // empty 1st claster
         fwrite(floppy,1,SLICE_SIZE,edsk);
         fwrite(index,1,SLICE_SECTORS*4,edsk);
         free(floppy);
      } else {
         ul ii, step = SLICE_SIZE+SLICE_SECTORS*4;
         slices = (sz-flsize)/step;
         index = (ul*)malloc(slices*SLICE_SECTORS*4);
         for (ii=0;ii<slices;ii++) {
            fseek(edsk,flsize+ii*step-SLICE_SECTORS*4,SEEK_SET);
            fread(index+SLICE_SECTORS*ii,1,SLICE_SECTORS*4,edsk);
         }
      }
   }
}

ul ed_sectofs(ul sector) {
   if (!edsk||!index||!slices) return 0; else {
      ul *ofs = memchrd(index,sector,slices*SLICE_SECTORS), pos;
      if (!ofs) return 0;
      pos = ofs-index;
      return (SLICE_SECTORS + (SLICE_SECTORS*4>>9)) * (pos/SLICE_SECTORS) +
             pos%SLICE_SECTORS + DISKETTE_SIZE;
   }
}

static void flush_slices(void) {
   u32 rsize, ii;
   if (!edsk) return;
   rsize = (DISKETTE_SIZE<<9) + (SLICE_SIZE+SLICE_SECTORS*4)*slices;
   CHSIZE(_fileno(edsk),rsize);
   rsize = SLICE_SIZE+SLICE_SECTORS*4;
   for (ii=0;ii<slices;ii++) {
      fseek(edsk,(DISKETTE_SIZE<<9)+ii*rsize-SLICE_SECTORS*4,SEEK_SET);
      fwrite(index+SLICE_SECTORS*ii,1,SLICE_SECTORS*4,edsk);
   }
}

// allocate space for new sectors
int extend_disk(ul sector, ul count) {
   if (!edsk||!index||!slices) return 0; else {
      while (count--) {
         if (!ed_sectofs(sector)) {
            ul *fp = memchrd(index,FFFF,slices*SLICE_SECTORS);
            if (!fp) { // alloc new slice
               index = (ul*)realloc(index,SLICE_SECTORS*4*(slices+1));
               fp    = index+SLICE_SECTORS*slices++;
               memset(fp,0xFF,SLICE_SECTORS*4);
            }
            *fp = sector;
         }
         sector++;
      }
      flush_slices();
      return 1;
   }
}

// free disk data (sectors will be assumed as zeroed)
void shrink_disk(ul sector, ul count) {
   if (!edsk||!index||!slices) return; else {
      while (count--) {
         ul *ofs = memchrd(index,sector++,slices*SLICE_SECTORS);
         if (ofs) *ofs = FFFF;
      }
      // truncate empty slices at the end of file
      while (slices>1) {
         ul *ofs = memchrnd(index+(slices-1)*SLICE_SECTORS,FFFF,SLICE_SECTORS);
         if (ofs) break; else slices--;
      }
      flush_slices();
   }
}

ul dsk_size(ul disk, ul *sectsize, dsk_geo_data *geo) {
   if (sectsize) *sectsize = 512;
   if (disk==DSK_FLOPPY) {
      if (geo) {
         geo->Heads        = 2;
         geo->SectOnTrack  = 18;
         geo->Cylinders    = DISKETTE_SIZE/(geo->SectOnTrack*geo->Heads);
         geo->SectorSize   = 512;
         geo->TotalSectors = DISKETTE_SIZE;
      }
      return DISKETTE_SIZE;
   }
   if (disk==0) {
      if (geo) {
         geo->Heads        = 255;
         geo->SectOnTrack  = 63;
         geo->Cylinders    = HDD_SIZE/(geo->SectOnTrack*geo->Heads);
         geo->SectorSize   = 512;
         geo->TotalSectors = HDD_SIZE;
      }
      return HDD_SIZE;
   }
   return 0;
}

ul dsk_floppy(ul disk, ul sector, ul count, void *data, int wr) {
   if (disk==DSK_FLOPPY) {
      if (sector>=DISKETTE_SIZE) return 0;
      if (sector+count>DISKETTE_SIZE) count=DISKETTE_SIZE-sector;
      fseek(edsk,sector*512,SEEK_SET);
      if (!wr) fread(data,1,512*count,edsk); else
              fwrite(data,1,512*count,edsk);
      return count;
   }
   return 0;
}

ul dsk_read(ul disk, ul sector, ul count, void *data) {
   if (!edsk) ed_open();
   if (!edsk) return 0;
   if (disk==DSK_FLOPPY) return dsk_floppy(disk,sector,count,data,0); else
   if (disk==0) {
      ul ii;
      if (sector>=HDD_SIZE) return 0;
      if (sector+count>HDD_SIZE) count=HDD_SIZE-sector;
      for (ii=0;ii<count;ii++) {
         ul ofs = ed_sectofs(ii+sector);
         // sector available?
         if (!ofs) memset((char*)data+512*ii,0,512); else {
            fseek(edsk,ofs*512,SEEK_SET);
            fread((char*)data+512*ii,1,512,edsk);
         }
      }
      return count;
   }
   return 0;
}

ul dsk_write(ul disk, ul sector, ul count, void *data) {
   if (!edsk) ed_open();
   if (!edsk) return 0;
   if (disk==DSK_FLOPPY) return dsk_floppy(disk,sector,count,data,1); else
   if (disk==0) {
      // is all data zeroed?
      int zerodata = memchrnb(data,0,count*512)?0:1;
      ul ii;
      if (sector>=HDD_SIZE) return 0;
      if (sector+count>HDD_SIZE) count=HDD_SIZE-sector;

      if (zerodata) { shrink_disk(sector,count); return count; }
      // check/allocate space
      if (!extend_disk(sector,count)) return 0;
      for (ii=0;ii<count;ii++) {
         ul ofs = ed_sectofs(ii+sector);
         fseek(edsk,ofs*512,SEEK_SET);
         fwrite((char*)data+512*ii,1,512,edsk);
      }
      return count;
   }
   return 0;
}

// ************************************************************************
// Win32 i/o
// ************************************************************************
#elif defined(__NT__)
#include <windows.h>
#include <winioctl.h>

#ifdef __WATCOMC__
#define LI(x) (*(uq*)&(x))
#else
#define LI(x) (x)
#endif

BOOL open_disk(ul disk, HANDLE *dh, int rwaccess) {
   char openpath[64];
   if (disk&DSK_FLOPPY) {
      disk&=DSK_DISKMASK;
      if (disk>=2) return FALSE;
      sprintf(openpath,"\\\\.\\%c:",disk+'A');
   } else
   if (disk&DSK_VIRTUAL) return FALSE;
     else
   sprintf(openpath,"\\\\.\\PhysicalDrive%d",disk&DSK_DISKMASK);

   *dh = CreateFile(openpath,
           rwaccess?FILE_WRITE_DATA|FILE_READ_DATA:0,
           FILE_SHARE_READ|FILE_SHARE_WRITE,  // share mode
           NULL, OPEN_EXISTING, 0, NULL);
   return *dh==INVALID_HANDLE_VALUE?FALSE:TRUE;
}

static int sys_floppies = -1,
               sys_hdds = -1;

ul dsk_count(ul *floppies,ul *vdisks) {
   if (vdisks) *vdisks = 0;
   if (floppies) {
      if (sys_floppies<0) {
         while (sys_floppies<2) {
            HANDLE dh;
            if (open_disk(++sys_floppies|DSK_FLOPPY,&dh,0)) CloseHandle(dh);
               else break;
         }
      }
      *floppies = sys_floppies;
   }
   if (sys_hdds<0) {
      while (1) {
         HANDLE dh;
         if (open_disk(++sys_hdds,&dh,0)) CloseHandle(dh); else break;
      }
   }
   return sys_hdds;
}

ul dsk_size(ul disk, ul *sectsize, dsk_geo_data *geo) {
   HANDLE dh;
   DISK_GEOMETRY pdg;

   if (open_disk(disk,&dh,0)) {
      DWORD tmp;
      BOOL   rc = DeviceIoControl(dh, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                  NULL, 0, &pdg, sizeof(pdg), &tmp, (LPOVERLAPPED)NULL);
      CloseHandle(dh);
      if (rc) {
         uq size = LI(pdg.Cylinders) * pdg.TracksPerCylinder * pdg.SectorsPerTrack;
         if (geo) {
            geo->Cylinders    = LI(pdg.Cylinders);
            geo->Heads        = pdg.TracksPerCylinder;
            geo->SectOnTrack  = pdg.SectorsPerTrack;
            geo->SectorSize   = pdg.BytesPerSector;
            geo->TotalSectors = size;
         }
         if (sectsize) *sectsize = pdg.BytesPerSector;
         if (size>(uq)0xFFFFFFFF) size = 0xFFFFFFFF;
         return size;
      }
   }
   return 0;
}

uq dsk_size64(ul disk, ul *sectsize) {
   return dsk_size(disk, sectsize, 0);
}

ul dsk_read(ul disk, uq sector, ul count, void *data) {
   HANDLE dh;
   if (open_disk(disk,&dh,1)) {
      DWORD readed;
      if (!ReadFile(dh,data,count<<9,&readed,0)) readed = 0;
      CloseHandle(dh);
      return readed>>9;
   }
   return 0;
}

ul dsk_write(ul disk, uq sector, ul count, void *data) {
   return 0;
}

// ************************************************************************
// qsinit i/o
// ************************************************************************
#elif defined(__QSINIT__)
#include "qsdm.h"

ul dsk_count(ul *floppies,ul *vdisks) {
   if (vdisks) *vdisks = 2;
   return hlp_diskcount(floppies);
}

ul dsk_size(ul disk, ul *sectsize, dsk_geo_data *geo) {
   int geok = 0;
   // get actual disk geometry, not BIOS
   if (geo) geok = dsk_getptgeo(disk,(disk_geo_data*)geo)==0;
   return hlp_disksize(disk, sectsize, geok?0:(disk_geo_data*)geo);
}

uq dsk_size64(ul disk, ul *sectsize) {
   return hlp_disksize64(disk, sectsize);
}

ul dsk_read(ul disk, uq sector, ul count, void *data) {
   return hlp_diskread(disk|QDSK_DIRECT, sector, count, data);
}

ul dsk_write(ul disk, uq sector, ul count, void *data) {
   return hlp_diskwrite(disk|QDSK_DIRECT, sector, count, data);
}

// ************************************************************************
// OS/2 i/o
// ************************************************************************
#elif defined(__OS2__)
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#define INCL_DOSDEVICES
#include <os2.h>

static int sys_floppies = -1,
               sys_hdds = -1;

ul dsk_count(ul *floppies,ul *vdisks) {
   ul rc = 0;
   if (vdisks) *vdisks = 0;
   if (floppies) {
      if (sys_floppies<0) {
         sys_floppies = 0;
      }
      *floppies = sys_floppies;
   }
   if (sys_hdds>=0) return sys_hdds;
   if (DosPhysicalDisk(INFO_COUNT_PARTITIONABLE_DISKS,&rc,2,NULL,0)) return 0;
   return sys_hdds = rc;
}

static BOOL open_disk(ul disk, HFILE *dh, int rwaccess) {
   char dsk[8];
   ul   rc;
   *dh = 0;
   if ((disk&(DSK_VIRTUAL|DSK_FLOPPY))!=0) return 0;
   sprintf(dsk,"%d:",1+disk);
   rc = DosPhysicalDisk(INFO_GETIOCTLHANDLE,dh,2,dsk,strlen(dsk)+1);
#if 0
   if (!rc) {
      char   cmdinfo = 0, out[31];
      ul       psize = 1,
               dsize = sizeof(out);
      memset(out, 0, sizeof(out));
      rc = DosDevIOCtl(*dh,IOCTL_PHYSICALDISK,PDSK_LOCKPHYSDRIVE,
         &cmdinfo,1,&psize,out,sizeof(out),&dsize);
   }
#endif
   return rc?0:1;
}

static void close_disk(HFILE dh) {
   DosPhysicalDisk(INFO_FREEIOCTLHANDLE,0,0,&dh,2);
}

static ul get_size(HFILE dh, ul *sectsize, dsk_geo_data *geo) {
   DEVICEPARAMETERBLOCK *dpb = (DEVICEPARAMETERBLOCK*)malloc(sizeof(DEVICEPARAMETERBLOCK));
   char              cmdinfo = 0;
   ul                  psize = 1,
                       dsize = sizeof(DEVICEPARAMETERBLOCK),
                     sectors = 0;

   if (sectsize) *sectsize = 0;
   if (geo) memset(geo, 0, sizeof(dsk_geo_data));

   if (DosDevIOCtl(dh,IOCTL_PHYSICALDISK,PDSK_GETPHYSDEVICEPARAMS,
      &cmdinfo,1,&psize,dpb,sizeof(DEVICEPARAMETERBLOCK),&dsize)==0)
   {
      sectors = dpb->cCylinders * dpb->cHeads * dpb->cSectorsPerTrack;
      if (geo) {
         geo->Cylinders    = dpb->cCylinders;
         geo->Heads        = dpb->cHeads;
         geo->SectOnTrack  = dpb->cSectorsPerTrack;
         geo->SectorSize   = 512;
         geo->TotalSectors = sectors;
      }
      if (sectsize) *sectsize = 512;
   }
   free(dpb);
   return sectors;
}

ul dsk_size(ul disk, ul *sectsize, dsk_geo_data *geo) {
   HFILE     dh = 0;
   ul   sectors;

   if (!open_disk(disk, &dh, 0)) return 0;
   sectors = get_size(dh, sectsize, geo);
   close_disk(dh);
   return sectors;
}

uq dsk_size64(ul disk, ul *sectsize) {
   return dsk_size(disk, sectsize, 0);
}

static ul dsk_action(ul disk, ul action, ul sector, ul count, void *data) {
   HFILE          dh = 0;
   ul        initcnt = count;
   char        *bptr = (char*)data;
   dsk_geo_data  geo;

   if (!open_disk(disk, &dh, 0)) return 0;
   get_size(dh, 0, &geo);

   if (geo.SectOnTrack && geo.Heads) {
      while (count) {
         TRACKLAYOUT td;
         ul   sptrc = geo.SectOnTrack,
              spcyl = sptrc * geo.Heads,
             psize  = sizeof(TRACKLAYOUT),
             dsize  = geo.SectorSize, rc;
         td.bCommand      = 0;
         td.usFirstSector = sector%spcyl%sptrc;
         td.usHead        = sector%spcyl/sptrc;
         td.usCylinder    = sector/spcyl;
         td.cSectors      = 1;
         td.TrackTable[0].usSectorNumber = td.usFirstSector + 1;
         td.TrackTable[0].usSectorSize   = geo.SectorSize;

         if (td.usCylinder>geo.Cylinders) return 0;

         rc = DosDevIOCtl(dh,IOCTL_PHYSICALDISK,action,&td,psize,&psize,bptr,
            dsize,&dsize);
         if (rc==0) {
            bptr += geo.SectorSize;
            sector++;
            count--;
         } else {
            close_disk(dh);
            return initcnt - count;
         }
      }
   }
   close_disk(dh);
   return initcnt;
}

ul dsk_read(ul disk, uq sector, ul count, void *data) {
   if (sector+count>FFFF) return 0;
   return dsk_action(disk, PDSK_READPHYSTRACK, sector, count, data);
}

// do not implement write for safe reason
ul dsk_write(ul disk, uq sector, ul count, void *data) {
   return 0;
}

#endif
