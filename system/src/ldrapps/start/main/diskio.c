//
// QSINIT "start" module
// volume functions
// -------------------------------------
//
// moved here from QSINIT binary

#include "qsutil.h"
#include "qsint.h"
#include "qsstor.h"
#include "fat/ff.h"
#include "stdlib.h"
#include "qsinit_ord.h"
#include "seldesc.h"
#include "qsmodext.h"

static int _std catch_diskremove(mod_chaininfo *info) {
   u32t disk = *(u32t*)(info->mc_regs->pa_esp+4);
   log_it(2, "hlp_diskremove(%02X)\n", disk);
   // is disk present and emulated? then unmount all
   if ((hlp_diskmode(disk,HDM_QUERY)&HDM_EMULATED)!=0)
      hlp_unmountall(disk);
   return 1;
}

/* catch hlp_diskremove() to call hlp_unmountall() before it */
void setup_cache(void) {
   u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   if (qsinit)
      if (mod_apichain(qsinit, ORD_QSINIT_hlp_diskremove, APICN_ONENTRY, catch_diskremove))
         return;
   log_it(2, "failed to catch!\n");
}

u32t _std hlp_vollabel(u8t drive, const char *label) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   FATFS   **extdrv = (FATFS  **)sto_data(STOKEY_FATDATA);

   if (extvol && drive<_VOLUMES) {
      vol_data  *vdta = extvol+drive;
      FATFS     *fdta = extdrv[drive];
      // mounted & FAT?
      if (drive<=DISK_LDR || (vdta->flags&VDTA_ON)!=0) {
         char lb[14];
         lb[0] = drive+'0';
         lb[1] = ':';
         if (label) {
            strncpy(lb+2,label,12);
            lb[13] = 0;
         } else
            lb[2] = 0;
         if (f_setlabel(lb)==FR_OK) {
            // drop file label cache to make read it again
            vdta->serial = 0;
            return 1;
         }
      }
   }
   return 0;
}

/** query disk text name.
    @param disk     disk number
    @param buffer   buffer for result, can be 0 for static. At least 8 bytes.
    @return 0 on error, buffer or static buffer ptr */
char* _std dsk_disktostr(u32t disk, char *buffer) {
   static char buf[8];
   if (!buffer) buffer = buf;
   if (disk&QDSK_FLOPPY) snprintf(buffer, 8, "fd%d", disk&QDSK_DISKMASK); else
   if (disk&QDSK_VOLUME) snprintf(buffer, 8, "%c:", '0'+(disk&QDSK_DISKMASK)); else
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
   static char buffer[64];
   static char *suffix[] = { "kb", "mb", "gb", "tb", "pb", "eb", "zb" }; // ;)
   char fmt[16];
   u64t size = disksize * (sectsize?sectsize:1);
   int idx = 0;
   size>>=10;
   while (size>=100000) { size>>=10; idx++; }
   if (width>4) sprintf(fmt, "%%%dd %%s", width-3);
   if (!buf) buf = buffer;
   sprintf(buf, width>4?fmt:"%d %s", (u32t)size, suffix[idx]);
   return buf;
}

u32t _std hlp_unmountall(u32t disk) {
   vol_data *extvol = (vol_data*)sto_data(STOKEY_VOLDATA);
   if (extvol) {
      u32t umcnt=0, ii;
      for (ii=2; ii<DISK_COUNT; ii++) {
         vol_data *vdta = extvol+ii;
         // mounted?
         if ((vdta->flags&VDTA_ON)!=0)
            if (vdta->disk==disk) { hlp_unmountvol(ii); umcnt++; }
      }
      return umcnt;
   }
   return 0;
}
