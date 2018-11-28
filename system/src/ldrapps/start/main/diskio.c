//
// QSINIT "start" module
// volume functions
// -------------------------------------
//
// moved here from QSINIT binary

#include "qsutil.h"
#include "syslocal.h"
#include "qcl/sys/qsedinfo.h"
#include "dskinfo.h"
#include "qstask.h"
#include "qssys.h"
#include "sysio.h"

static void _std notify_diskremove(sys_eventinfo *cbinfo) {
   u32t disk = cbinfo->info;
   // is disk present and emulated? then unmount all
   if ((hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)!=0) hlp_unmountall(disk);
}

void setup_cache(void) {
   sys_notifyevent(SECB_DISKREM|SECB_GLOBAL, notify_diskremove);
}

/** query disk text name.
    @param disk     disk number
    @param buffer   buffer for result, can be 0 for static. At least 8 bytes.
    @return 0 on error, buffer or static buffer ptr */
char* _std dsk_disktostr(u32t disk, char *buffer) {
   if (!buffer) {
      u64t *rc;
      mt_tlsaddr(QTLS_DSKSTRBUF, &rc);
      buffer = (char*)rc;
   }
   // drop known service flags
   disk&=~(QDSK_IAMCACHE|QDSK_IGNACCESS);

   if (disk&QDSK_FLOPPY) snprintf(buffer, 8, "fd%d", disk&QDSK_DISKMASK); else
   if (disk&QDSK_VOLUME) snprintf(buffer, 8, "%c:", 'A'+(disk&QDSK_DISKMASK)); else
   if ((disk&QDSK_DISKMASK)==disk)
      snprintf(buffer, 8, "hd%d", disk&QDSK_DISKMASK);
         else { *buffer=0; return 0; }
   return buffer;
}

u32t _std dsk_strtodisk(const char *str) {
   if (str && *str) {
      u32t disk, floppies, 
          disks = hlp_diskcount(&floppies);
      char   c1 = toupper(str[0]),
             c2 = toupper(str[1]);
      
      do {
         if ((c1=='H'||c1=='F') && isalnum(c2)) { // hd0 h0 hdd0 fd0 f0 fdd0
            int idx = isalpha(c2)?1:0;
            if (idx && c2!='D') break; else
            if (toupper(str[2])=='D') idx++;
            // check for 0..9
            if (!isdigit(str[1+idx])) break;
            disk = str2long(str+1+idx);
            if (disk>=(c1=='F'?floppies:disks)) break;
            if (c1=='F') disk|=QDSK_FLOPPY;
         } else
         if (c2==':' && isalnum(c1)) {            // 0: 1: A: B:
            if (isalpha(c1)) c1 = c1-'A'+'0';
            disk = c1-'0';
            if (disk>=DISK_COUNT) break;
            disk|=QDSK_VOLUME;
         } else break;
      
         return disk;
      } while (0);
   }
   return FFFF;
}

char* _std dsk_formatsize(u32t sectsize, u64t disksize, int width, char *buf) {
   static char *suffix[] = { "kb", "mb", "gb", "tb", "pb", "eb", "zb" }; // ;)
   char fmt[16];
   u64t size = disksize * (sectsize?sectsize:1);
   int idx = 0;
   size>>=10;
   while (size>=100000) { size>>=10; idx++; }
   if (width>4) sprintf(fmt, "%%%dd %%s", width-3);
   if (!buf) {
      u64t *rc;
      mt_tlsaddr(QTLS_DSKSZBUF, &rc);
      buf = (char*)rc;
   }
   sprintf(buf, width>4?fmt:"%d %s", (u32t)size, suffix[idx]);
   return buf;
}

u32t _std hlp_unmountall(u32t disk) {
   if (_extvol) {
      u32t umcnt=0, ii;
      for (ii=2; ii<DISK_COUNT; ii++) {
         vol_data *vdta = _extvol+ii;
         // mounted?
         if ((vdta->flags&VDTA_ON)!=0)
            if (vdta->disk==disk) { hlp_unmountvol(ii); umcnt++; }
      }
      return umcnt;
   }
   return 0;
}

qs_extdisk _std hlp_diskclass(u32t disk, const char *name) {
   struct qs_diskinfo *di;
   qs_extdisk       rcptr = 0;

   mt_swlock();
   di = hlp_diskstruct(disk, 0);
   if (di && di->qd_extptr)
      // "name" is for extension, now only "qs_extdisk" supported
      if (!name || strcmp(name,"qs_extdisk")==0) {
         u32t id = exi_classid(di->qd_extptr), mcnt;
         if (id) {
            mcnt = exi_methods(id);
            // can`t check compat., but at least, can compare number of methods!
            if (mcnt == sizeof(_qs_extdisk)/sizeof(void*)) 
               rcptr = (qs_extdisk)di->qd_extptr;
            else // something was supplied with other number of methods?
               log_it(2, "wrong extptr for disk %03X (%d funcs)\n", disk, mcnt);
         }
      }
   mt_swunlock();
   return rcptr;
}

u32t _std hlp_disktype(u32t disk) {
   // validate disk arg
   if ((disk&~(QDSK_FLOPPY|QDSK_VOLUME|QDSK_DISKMASK|QDSK_DIRECT))==0 &&
      (disk&(QDSK_FLOPPY|QDSK_VOLUME))!=(QDSK_FLOPPY|QDSK_VOLUME))
   {
      struct qs_diskinfo *qe = 0;
      u32t                rv = HDT_INVALID;

      if (disk&QDSK_VOLUME) {
         u8t vol = disk&QDSK_DISKMASK;
         disk    = FFFF;

         if (vol==DISK_LDR) rv = HDT_RAMDISK; else
         if (vol<DISK_COUNT && _extvol) {
            mt_swlock();
            if (_extvol[vol].flags&VDTA_ON) disk = _extvol[vol].disk;
         }
      } else
         mt_swlock();
      // skip B: & invalid volume, it also not locked in such cases
      if (disk!=FFFF) {
         disk &= QDSK_FLOPPY|QDSK_DISKMASK;
         qe = hlp_diskstruct(disk, 0);
         
         if (qe) {
            if (qe->qd_flags&HDDF_HOTSWAP) {
               qs_extdisk exc = hlp_diskclass(disk, 0);
               u32t  extstate = exc ? exc->state(EDSTATE_QUERY) : 0;
         
               rv = extstate&EDSTATE_RAM?HDT_RAMDISK:HDT_EMULATED;
            } else
            // filter cd-rom & real 1.44 (not a flash drive, reported as floppy)
            if (qe->qd_sectorsize==2048) rv = HDT_CDROM; else
               if (qe->qd_sectors<=2880*2) rv = HDT_FLOPPY; else
                  rv = HDT_REGULAR;
         }
         mt_swunlock();
      }
      return rv;
   }
   return HDT_INVALID;
}
