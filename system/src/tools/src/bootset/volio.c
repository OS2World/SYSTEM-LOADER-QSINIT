#include "volio.h"

#if defined(SP_WIN32)

#include <windows.h>
#include <winioctl.h>

#ifdef __WATCOMC__
#define LI(x) (*(u64*)&(x))
#else
#define LI(x) (x)
#endif

typedef struct {
   char   path[64];
   HANDLE       dh;
   u32         pos;
   u32       ssize;
   u32     volsize;
} voldata;

Bool volume_open (char letter, volh *handle) {
   char openpath[64];
   HANDLE   rc;
   DWORD    SectorsPerCluster, BytesPerSector, NumberOfFreeClusters,
            TotalNumberOfClusters, Count;
   voldata *vh;
   sprintf(openpath,"%c:\\",letter);
   if (!GetDiskFreeSpace(openpath,&SectorsPerCluster, &BytesPerSector,
      &NumberOfFreeClusters, &TotalNumberOfClusters)) BytesPerSector = 512;

   sprintf(openpath,"\\\\.\\%c:",letter);
   rc = CreateFile(openpath, FILE_WRITE_DATA|FILE_READ_DATA,
      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
         FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH, NULL);
   *handle = 0;
   if (rc==INVALID_HANDLE_VALUE) return False;
   /** dismount is not required for FAT32 driver, but exFAT switches volume
       to r/o after writing below, FSCTL_LOCK_VOLUME can`t help too.
       GetLogicalDrives() after close should mount it back as MSDN says - and
       it works ;) */
   if (!DeviceIoControl(rc, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &Count, 0))
      return False;

   vh = (voldata*)malloc(sizeof(voldata));
   vh->dh      = rc;
   vh->pos     = 0;
   vh->ssize   = BytesPerSector;
   vh->volsize = 0;
   vh->volsize = volume_size(vh,0);
   strcpy(vh->path, openpath);
   *handle = vh;
   return True;
}

u32 volume_size (volh handle, u32 *sectorsize) {
   voldata *vh = (voldata*)handle;
   u64      sz = 0;
   DWORD   tmp;
   BOOL     rc;
   GET_LENGTH_INFORMATION    info;
   PARTITION_INFORMATION_EX  ppex;

   if (sectorsize) *sectorsize = vh->ssize;
   if (vh->volsize) return vh->volsize;

   rc = DeviceIoControl(vh->dh, IOCTL_DISK_GET_PARTITION_INFO_EX,
        NULL, 0, &ppex, sizeof(ppex), &tmp, (LPOVERLAPPED)NULL);
   if (!rc) {
      PARTITION_INFORMATION  ppn;
      rc = DeviceIoControl(vh->dh, IOCTL_DISK_GET_PARTITION_INFO,
           NULL, 0, &ppn, sizeof(ppn), &tmp, (LPOVERLAPPED)NULL);
      if (!rc) {
         GET_LENGTH_INFORMATION info;
         rc = DeviceIoControl(vh->dh, IOCTL_DISK_GET_LENGTH_INFO,
              NULL, 0, &info, sizeof(info), &tmp, (LPOVERLAPPED)NULL);
         if (!rc) {
            DISK_GEOMETRY geo;
            rc = DeviceIoControl(vh->dh, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                 NULL, 0, &geo, sizeof(geo), &tmp, (LPOVERLAPPED)NULL);
            if (rc) {
               sz = (u64)geo.SectorsPerTrack * geo.TracksPerCylinder *
                  LI(geo.Cylinders);
               sz*= geo.BytesPerSector;
            } else {
#if 0
               DWORD err = GetLastError();
               printf("Error %d (%X)\n",err,err);
#endif
               return 0;
            }
         } else
            sz = LI(info.Length);
      } else
         sz = LI(ppn.PartitionLength);
   } else
      sz = LI(ppex.PartitionLength);

   sz >>= 9;
   if (sz>(u64)FFFF) sz=FFFF;
   return sz;
}

static Bool volume_seek(volh handle, u32 sector) {
   voldata *vh = (voldata*)handle;
   if (vh->pos!=sector) {
      u64   dpos = (u64)sector * vh->ssize;
      DWORD  err;
      SetFilePointer(vh->dh, dpos, ((PLONG)&dpos)+1, FILE_BEGIN);
      err = GetLastError();
      if (err!=NO_ERROR) {
         if (!sector) { // position 0 - re-open it ;)
            CloseHandle(vh->dh);
            vh->dh = CreateFile(vh->path, FILE_WRITE_DATA|FILE_READ_DATA,
               FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                  FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH, NULL);
            if (vh->dh==INVALID_HANDLE_VALUE) return False;
         } else
            return False;
      }
      vh->pos = sector;
   }
   return True;
}

u32 volume_read (volh handle, u32 sector, u32 count, void *data) {
   voldata *vh = (voldata*)handle;
   DWORD readed;
   void *buffer;

   if (!count) return 0;
   if (!volume_seek(handle, sector)) return 0;

   buffer = VirtualAlloc(0, count*vh->ssize, MEM_COMMIT, PAGE_READWRITE);
   if (!ReadFile(vh->dh, buffer, count*vh->ssize, &readed,0)) {
      readed  = 0;
      vh->pos = 0xFFFF;
   } else {
      memcpy(data, buffer, readed);
      vh->pos+= readed/vh->ssize;
   }
   VirtualFree(buffer, 0, MEM_RELEASE);

   return readed/vh->ssize;
}

u32 volume_write(volh handle, u32 sector, u32 count, void *data) {
   voldata *vh = (voldata*)handle;
   DWORD written;
   void *buffer;

   if (!count) return 0;
   if (!volume_seek(handle, sector)) return 0;

   buffer = VirtualAlloc(0, count*vh->ssize, MEM_COMMIT, PAGE_READWRITE);
   memcpy(buffer, data, count*vh->ssize);
   if (!WriteFile(vh->dh, buffer, count*vh->ssize, &written,0)) {
      written = 0;
      vh->pos = 0xFFFF;
   } else
      vh->pos+= written/vh->ssize;
   VirtualFree(buffer, 0, MEM_RELEASE);
   return written/vh->ssize;
}

void volume_close(volh handle) {
   HANDLE   rc;
   voldata *vh = (voldata*)handle;
   CloseHandle(vh->dh);
   // force windows to mount volume back
   GetLogicalDrives();
   memset(vh, 0, sizeof(voldata));
   free(vh);
}
#elif defined(SP_OS2)
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_LONGLONG
#define INCL_DOSMISC
#include <os2.h>
#include <string.h>

#define DSK_GETPHYSDRIVE      0x68

typedef struct {
   HFILE               dh;  // disk/physdisk handle (depends on pdsk)
   u32              ssize;
   u32            volsize;
   char             drive;
   u8                pdsk;  // 0 if unknown, else 1..255
   BIOSPARAMETERBLOCK bpb;
} voldata;

/* two bad news here:
   1). primary partition, located AFTER extended looks unreadable ...
       DASD (or who?) assumed start of EXTENDED as a start of this volume!!!
       I.e. "bpb.cHiddenSectors" does not match to the read position in
       DSK_READTRACK/DSK_WRITETRACK.
       So, if bpb.cHiddenSectors > spt - just route i/o to the physical disk
       ioctl!
   2). spt value retuned from BPB in case above, i.e. we get 63 instead of 127
       in DSK_GETDEVICEPARAMS on 1Tb hdd if partition was formatted in Windows.
       So need to read PDSK_GETPHYSDEVICEPARAMS too */

Bool volume_open (char letter, volh *handle) {
   char openpath[8];
   ULONG  resAction,
                 rc;
   HFILE         dh;
   sprintf(openpath,"%c:",letter);

   DosError(FERR_DISABLEHARDERR);

   rc = DosOpen(openpath,&dh,&resAction,0,0,OPEN_ACTION_OPEN_IF_EXISTS,
      OPEN_FLAGS_DASD|OPEN_FLAGS_NO_CACHE|OPEN_FLAGS_NOINHERIT|
         OPEN_SHARE_DENYNONE|OPEN_FLAGS_FAIL_ON_ERROR|OPEN_ACCESS_READWRITE,0);
   // OPEN_SHARE_DENYREADWRITE
   if (rc==0)
   {
      voldata *vh = (voldata*)malloc(sizeof(voldata));
      struct { b Infotype, DriveUnit; } DriveRequest;
      d  psize, dsize;

      memset(vh, 0, sizeof(voldata));
      vh->dh   = dh;
      psize    = 0;
      dsize    = sizeof(vh->pdsk);
      /* query physical disk from a logical drive (unoff DASD IOCTL) */
      if (DosDevIOCtl(dh,IOCTL_DISK,DSK_GETPHYSDRIVE,0,0,&psize,
         (PVOID)&vh->pdsk,sizeof(vh->pdsk),&dsize)) vh->pdsk = 0;

      psize = sizeof(DriveRequest);
      dsize = sizeof(vh->bpb);
      // get recommended BPB ...
      DriveRequest.Infotype  = 0;
      DriveRequest.DriveUnit = 0;

      if (DosDevIOCtl(dh,IOCTL_DISK,DSK_GETDEVICEPARAMS,(PVOID)&DriveRequest,
        sizeof(DriveRequest),&psize,(PVOID)&vh->bpb,sizeof(vh->bpb),&dsize)==0)
      {
         u64 sectors = vh->bpb.cSectors;
         if (!sectors) sectors = vh->bpb.cLargeSectors;
         vh->volsize = sectors;
         vh->ssize   = vh->bpb.usBytesPerSector;
      } else {
         FSALLOCATE    fa;
         if (DosQueryFSInfo(letter-'A'+1,FSIL_ALLOC,&fa,sizeof(fa)))
            memset(&fa,0,sizeof(fa));

         vh->volsize = ((u64)fa.cSectorUnit*fa.cbSector*fa.cUnit)/fa.cbSector;
         vh->ssize   = fa.cbSector;
      }
      if (vh->pdsk) {
         char    dsk[8];
         HFILE   pdh;
         int  noswap = 1;
         sprintf(dsk, "%u:", (u32)vh->pdsk);

         if (DosPhysicalDisk(INFO_GETIOCTLHANDLE,&pdh,2,dsk,strlen(dsk)+1)==0) {
            DEVICEPARAMETERBLOCK dpb;
            u8 ci = 0;
            psize = 1;
            dsize = sizeof(DEVICEPARAMETERBLOCK);

            if (DosDevIOCtl(pdh,IOCTL_PHYSICALDISK,PDSK_GETPHYSDEVICEPARAMS,
               &ci,1,&psize,&dpb,sizeof(DEVICEPARAMETERBLOCK),&dsize)==0)
            {
               // fix spt to physical disk value!
               vh->bpb.usSectorsPerTrack = dpb.cSectorsPerTrack;
               // !!! switch i/o to physical as described above
               if (vh->bpb.cHiddenSectors > dpb.cSectorsPerTrack) {
                  vh->bpb.cHeads     = dpb.cHeads;
                  vh->bpb.cCylinders = dpb.cCylinders;

                  DosClose(vh->dh);
                  vh->dh = pdh;
                  noswap = 0;
               }
            }
            if (noswap) {
               vh->pdsk = 0;
               DosPhysicalDisk(INFO_FREEIOCTLHANDLE,0,0,&pdh,2);
            }
         }
      }
      vh->drive   = letter;
      *handle = vh;
      return True;
   } else {
      printf("Error %d (%X)\n",rc,rc);
   }

   *handle = 0;
   return False;
}

u32  volume_size (volh handle, u32 *sectorsize) {
   voldata *vh = (voldata*)handle;
   return vh->volsize;
}

static u32 volume_action(volh handle, u32 action, u32 sector, u32 count, void *data) {
   voldata    *vh = (voldata*)handle;
   u32         vs, initcnt = count;
   char     *bptr = (char*)data;
   if (!vh->bpb.cCylinders || !vh->bpb.cHeads || !vh->bpb.usSectorsPerTrack)
      return 0;
   sector += vh->bpb.cHiddenSectors;

   if (vh->pdsk) action = action ? PDSK_WRITEPHYSTRACK : PDSK_READPHYSTRACK;
      else action = action ? DSK_WRITETRACK : DSK_READTRACK;

   while (count) {
      TRACKLAYOUT td;
      u32  sptrc = vh->bpb.usSectorsPerTrack,
           spcyl = sptrc * vh->bpb.cHeads,
          psize  = sizeof(TRACKLAYOUT),
          dsize  = vh->bpb.usBytesPerSector;
      td.bCommand      = 0;
      td.usFirstSector = 0;
      td.usHead        = sector%spcyl/sptrc;
      td.usCylinder    = sector/spcyl;
      td.cSectors      = 1;
      td.TrackTable[0].usSectorNumber = sector%spcyl%sptrc + 1;
      td.TrackTable[0].usSectorSize   = vh->bpb.usBytesPerSector;
#if 0
      printf("heads:%u  spt:%u  cyls:%u  hidden:%u(%X)\n", vh->bpb.cHeads,
         vh->bpb.usSectorsPerTrack, vh->bpb.cCylinders, vh->bpb.cHiddenSectors,
            vh->bpb.cHiddenSectors);
      printf("sector %X c:%u h:%u s:%u\n", sector, (u32)td.usCylinder, (u32)td.usHead,
         (u32)td.usFirstSector);
#endif
      if (td.usCylinder>vh->bpb.cCylinders) return 0;
      // check volume size here, because we can use physical disk i/o!
      if (sector-vh->bpb.cHiddenSectors < vh->volsize &&
         DosDevIOCtl(vh->dh, vh->pdsk?IOCTL_PHYSICALDISK:IOCTL_DISK, action,
            (PVOID)&td, psize, &psize, bptr, dsize, &dsize)==0)
      {
         bptr += vh->bpb.usBytesPerSector;
         sector++;
         count--;
      } else {
         return initcnt - count;
      }
   }
   return initcnt;
}

u32  volume_read (volh handle, u32 sector, u32 count, void *data) {
   return volume_action(handle, 0, sector, count, data);
}

u32  volume_write(volh handle, u32 sector, u32 count, void *data) {
   return volume_action(handle, 1, sector, count, data);
}

void volume_close(volh handle) {
   voldata *vh = (voldata*)handle;

   if (vh->pdsk) DosPhysicalDisk(INFO_FREEIOCTLHANDLE, 0, 0, &vh->dh, 2);
      else DosClose(vh->dh);

   memset(vh, 0, sizeof(voldata));
   free(vh);
}

#elif defined(SP_DOS32)
Bool volume_open (char letter, volh *handle) {
   return False;
}

u32  volume_size (volh handle, u32 *sectorsize) {
   return 0;
}

u32  volume_read (volh handle, u32 sector, u32 count, void *data) {
   return 0;
}

u32  volume_write(volh handle, u32 sector, u32 count, void *data) {
   return 0;
}

void volume_close(volh handle) {
}

#else
#error Unknown platform!
#endif
