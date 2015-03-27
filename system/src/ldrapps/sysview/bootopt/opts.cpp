#define Uses_TProgram
#define Uses_TApplication
#include <tv.h>
#include <errno.h>

#ifdef __QSINIT__
#include "qsshell.h"
#include "qsutil.h"
#include "qslog.h"
#include "stdlib.h"
#include "ioint13.h"
#include "vio.h"
#include "qsint.h"
#include "qshm.h"
#include "direct.h"
#include "qssys.h"
#include "qsxcpt.h"
#include "qspage.h"
#include "diskedit.h"

extern "C"
extern struct Disk_BPB __cdecl BootBPB;
str_list *opl = 0;

// is key present? (with argument or not)
#define key_present(key) key_present_pos(key,0)

// is key present? for subsequent search of the same parameter
char *key_present_pos(const char *key, u32t *pos) {
   u32t ii=pos?*pos:0, len = strlen(key);
   if (!opl||ii>=opl->count) return 0;
   for (;ii<opl->count;ii++) {
      u32t plen = strlen(opl->item[ii]);
      if (plen>len+1 && opl->item[ii][len]=='=' || plen==len)
         if (strnicmp(opl->item[ii],key,len)==0) {
            char *rc=opl->item[ii]+len;
            if (*rc=='=') rc++;
            if (pos) *pos=ii;
            return rc;
         }
   }
   return 0;
}

void  opts_prepare(char *opts) {
   if (!opl) opl=str_split(opts,",");
}

char *opts_get(char *name) {
   return key_present(name);
}

void  opts_free() {
   if (opl) { free(opl); opl=0; }
}

u32t  opts_baudrate() {
   u32t baud = 0;
   hlp_seroutinfo(&baud);
   return baud?baud:115200;
}

u32t  opts_port() {
   return hlp_seroutinfo(0);
}

char  opts_bootdrive() {
   return BootBPB.BPB_BootLetter&0x80?(BootBPB.BPB_BootLetter&0x7F)+'C':0;
}

void  opts_bootkernel(char *name, char *opts) {
   TProgram::application->suspend();

   char cmdline[1024];
   sprintf(cmdline,"bootos2.exe %s %s",name,opts);
   cmd_state cst = cmd_init(cmdline,0);
   vio_clearscr();
   cmd_run(cst,CMDR_ECHOOFF);
   cmd_close(cst);

   TProgram::application->resume();
   TProgram::application->redraw();
}

void  opts_loadldr(char *name) {
   TProgram::application->suspend();
   exit_restart(name);
   TProgram::application->resume();
   TProgram::application->redraw();
}

void  opts_run(char *cmdline, int echoon, int pause) {
   TProgram::application->suspend();

   cmd_state cst = cmd_init(cmdline,0);
   vio_clearscr();
   cmd_run(cst,echoon?0:CMDR_ECHOOFF);
   cmd_close(cst);

   if (pause) {
      vio_setcolor(VIO_COLOR_GRAY);
      vio_strout("\npress any key");
      vio_setcolor(VIO_COLOR_RESET);
      key_read();
   }

   TProgram::application->resume();
   TProgram::application->redraw();
}

char *opts_getlog(int time, int dostext) {
   return log_gettext((time?LOGTF_TIME|LOGTF_LEVEL:0)|LOG_GARBAGE|
      (dostext?LOGTF_DOSTEXT:0));
}

u32t opts_cpuid(u32t index, u32t *data) {
   return hlp_getcpuid(index, data);
}

u32t opts_getpcmem(u64t *highaddr) {
   static u32t rc    = 0;
   static u64t haddr = 0;

   if (highaddr) *highaddr = haddr;
   if (rc) return rc;

   AcpiMemInfo *rtbl = int15mem();
   // copying data from disk buffer first
   int idx = 0;
   while (rtbl[idx].LengthLow||rtbl[idx].LengthHigh) idx++;
   if (idx) {
      AcpiMemInfo *tbl = new AcpiMemInfo[idx];
      memcpy(tbl, rtbl, sizeof(AcpiMemInfo)*idx);
      for (int ii=0; ii<idx; ii++) {
         int tidx = tbl[ii].AcpiMemType;
         // count only "usable", "reserved (ACPI)" and "reserved (NVS)" blocks
         if (tidx==1||tidx==3||tidx==4)
            rc += (tbl[ii].LengthHigh<<22) + (tbl[ii].LengthLow>>10);
         // calc max memory addr
         u64t ea = ((u64t)tbl[ii].BaseAddrHigh<<32) + tbl[ii].BaseAddrLow +
            ((u64t)tbl[ii].LengthHigh<<32) + tbl[ii].LengthLow;
         if (ea > haddr) haddr = ea;
      }
      delete tbl;
   }
   rc>>=10;
   if (highaddr) *highaddr = haddr;
   return rc;
}

u32t opts_getcputemp(void) {
   return hlp_getcputemp();
}

u32t opts_mtrrquery(u32t *flags, u32t *state, u32t *addrbits) {
   return hlp_mtrrquery(flags, state, addrbits);
}

static u32t opts_memio(u64t pos, void *data, int write) {
   if (pos>=sys_endofram()) {
      // warn about PAE mode
      if (pos>=_4GBLL && !sys_is64mode()) {
         if (!sys_pagemode()) {
            static int askonce = 0;
            if (askonce) return 0; else {
               askonce++;
               if (SysApp.askDlg(MSGA_TURNPAE)) pag_enable();
            }
         }
      }
      return sys_memhicopy(write?pos:(u32t)data, write?(u32t)data:pos, 256);
   } else {
      void *addr = (char*)pos + hlp_segtoflat(0);
      return hlp_memcpy(write?addr:data, write?data:addr, 256, 1);
   }
}

u32t opts_memread(u64t pos, void *data) {
   return opts_memio(pos, data, 0);
}

u32t opts_memwrite(u64t pos, void *data) {
   return opts_memio(pos, data, 1);
}

void* opts_freadfull(const char *name, unsigned long *bufsize, int *reterr) {
   void *rc = freadfull(name, bufsize);
   if (reterr) *reterr = rc?0:errno;
   return rc;
}

void* opts_sysalloc(unsigned long size) {
   return hlp_memallocsig(size, "SYSV", QSMA_RETERR|QSMA_NOCLEAR);
}

void opts_sysfree(void *ptr) {
   hlp_memfree(ptr);
}

#else
#include "classes.hpp"
#include <conio.h>
#include <sys/stat.h>
#include <dos.h>
#define u64t u64

static TStrings opl;

void  opts_prepare(char *opts) {
   opl.SplitString(opts,",");
}

char *opts_get(char *name) {
   static char buffer[384];
   l idx=opl.IndexOfName(name);
   if (idx>=0) strcpy(buffer,opl.Value(idx)());
   return idx>=0?buffer:0;
}

d opts_baudrate() {
   d rate = opl.DwordValue("BAUDRATE");
   return !rate?115200:rate;
}

void  opts_free() {
   opl.Clear();
}

char  opts_bootdrive() {
   return 0;
}

d     opts_port() {
   return 0;
}

void  opts_bootkernel(char *name, char *opts) {
   TProgram::application->suspend();
   printf("%s [%s]\npress any key\n",name, opts);
   getch();
   TProgram::application->resume();
   TProgram::application->redraw();
}

void  opts_loadldr(char *name) {
   TProgram::application->suspend();
   printf("[%s]\npress any key\n",name);
   getch();
   TProgram::application->resume();
   TProgram::application->redraw();
}

void  opts_run(char *cmdline, int echoon, int pause) {
   TProgram::application->suspend();
   printf("\r[%s]\npress any key\n",cmdline);
   getch();
   TProgram::application->resume();
   TProgram::application->redraw();
}

unsigned long __bcmp(const void *s1, const void *s2, unsigned long length) {
   const char *cc1 = (const char*) s1,
              *cc2 = (const char*) s2;
   while (length--)
      if (*cc1++!=*cc2++) return cc1-(char*)s1;
   return 0;
}

#define test_log_len 250000

char *opts_getlog(int time, int dostext) {
   char *log = (char*)malloc(test_log_len);
   memset(log,'.',test_log_len-1);
   log[test_log_len-1]=0;
   for (int ii=1; ii<test_log_len/65; ii++) log[ii*65]='\n';
   return log;
}

unsigned long opts_cpuid(unsigned long index, unsigned long *data) {
   unsigned long rc = 0;
   __asm {
      push    edi
      pushfd
      mov     eax, [esp]
      xor     dword ptr [esp], 00200000h
      popfd
      pushfd
      xor     eax, [esp]
      pop     ecx
      jnz     gcid_read
      mov     edi, data
      stosd
      stosd
      stosd
      stosd
      pop     edi
      jmp     gcid_done
gcid_read:
      mov     eax, index
      mov     edi, data
      push    ebx
      cpuid
      stosd
      mov     [edi], ebx
      pop     ebx
      mov     [edi+4], ecx
      mov     [edi+8], edx
      pop     edi
      mov     rc,1
gcid_done:
   }
   return rc;
}

unsigned long opts_getpcmem(u64t *highaddr) {
   if (highaddr) *highaddr = 0x100000000;
   return 4096;
}

unsigned long opts_getcputemp(void) {
   return 0;
}

unsigned long opts_mtrrquery(unsigned long *flags, unsigned long *state, unsigned long *addrbits) {
   if (flags) flags = 0;
   if (state) state = 0;
   if (addrbits) addrbits = 0;
   return 0;
}

unsigned long opts_memread(u64t pos, void *data) {
   memset(data, 0x11, 256);
   memcpy((char*)data+1, &pos, 4);
   return pos>>8 & 1;
}

unsigned long opts_memwrite(u64t pos, void *data) {
   return 1;
}

void* opts_freadfull(const char *name, unsigned long *bufsize, int *reterr) {
   if (!bufsize || !name) return 0;
   FILE *ff = fopen(name, "rb");
   *bufsize = 0;
   if (!ff) {
      if (reterr) *reterr = errno;
      return 0;
   } else {
     unsigned long sz = fsize(ff);
     void *rc = 0;
     if (reterr) *reterr = 0;
     if (sz) {
        rc = malloc(sz);
        if (!rc) {
           if (reterr) *reterr = ENOMEM;
        } else
        if (fread(rc,1,sz,ff)!=sz) {
           free(rc); rc=0;
           if (reterr) *reterr = EIO;
        } else
           *bufsize = sz;
     }
     fclose(ff);
     return rc;
   }
}

void* opts_sysalloc(unsigned long size) {
   return malloc(size);
}

void opts_sysfree(void *ptr) {
   free(ptr);
}

#endif

unsigned long opts_fsize(const char *str) {
#ifdef __QSINIT__
   dir_t st;
   if (_dos_stat(str, &st)) return FFFF;
   if (st.d_attr & _A_SUBDIR) return FFFF;
   return st.d_size;
#else
   struct stat st;
   if (stat(str, &st)) return FFFF;
   if (st.st_mode & S_IFDIR) return FFFF;
   return st.st_size;
#endif
}

u64t opts_freespace(unsigned drive) {
   diskfree_t dt;
   if (_dos_getdiskfree(drive+1, &dt)) return 0;
   return (u64t)dt.avail_clusters * (u64t)dt.sectors_per_cluster *
      (u64t)dt.bytes_per_sector;
}
