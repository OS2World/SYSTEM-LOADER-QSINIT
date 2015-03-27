#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* There is no reason to implement any i/o except QSINIT`s.
   Volume i/o for Windows & OS/2 implemented in src\tools\src\bootset\volio.c,
   but not used here... */

// ************************************************************************
// Win32 i/o
// ************************************************************************
#if defined(__NT__)
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
   if (sector+count>0xFFFFFFFFLL) return 0;
   return dsk_action(disk, PDSK_READPHYSTRACK, sector, count, data);
}

// do not implement write for safe reason
ul dsk_write(ul disk, uq sector, ul count, void *data) {
   return 0;
}

#endif
