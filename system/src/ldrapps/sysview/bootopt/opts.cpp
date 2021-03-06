#define Uses_TProgram
#define Uses_TApplication
#include <tv.h>
#include <errno.h>

#ifdef __QSINIT__
#include "qsbase.h"
#include "stdlib.h"
#include "qsint.h"
#include "qssys.h"
#include "direct.h"
#include "qsxcpt.h"
#include "diskedit.h"
#include "qcl/qsmt.h"
#include <sys/stat.h>

str_list *opl = 0;
qs_mtlib  mtl = 0;

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
   if (!opl) {
      opl = str_split(opts,",");
      //log_printlist("options:", opl);
   }
}

char *opts_get(const char *name) {
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
   return (char)sys_queryinfo(QSQI_OS2LETTER,0);
}

void  opts_settitle(const char *title) {
   se_settitle(se_sesno(), title);
}

/* this call should dramatically cool down CPU in MT mode.
   By default Turbo Vision loops in TApplication::idle all of the time.
   This is BAD.
   Here we just sleeps 32 ms or until the first available key press */
void  opts_yield() {
   // mt_active() is best, because it never loads MTLIB to check MT mode
   if (mt_active()) {
      /* just leave DELETE for QSINIT ;)
         class was used here to prevent static linking of MTLIB.
         instance may be created outside of this file, in seslist.cpp for ex. */
      if (!mtl) mtl = NEW(qs_mtlib);
      if (mtl) {
         mt_waitentry we[2] = {{QWHT_KEY,1}, {QWHT_CLOCK,0}};
         u32t        sig;
         we[1].tme = sys_clock() + 32*1000;
         mtl->waitobject(we, 2, 0, &sig);
      }
   }
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

u32t opts_loadldr(char *name) {
   TProgram::application->suspend();
   u32t rc = exit_restart(name,0,0);
   TProgram::application->resume();
   TProgram::application->redraw();
   return rc;
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

   AcpiMemInfo *tbl = hlp_int15mem();
   int idx = 0;
   while (tbl[idx].LengthLow||tbl[idx].LengthHigh) idx++;
   if (idx) {
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
   }
   free(tbl);
   rc>>=10;
   if (highaddr) *highaddr = haddr;
   return rc;
}

u32t opts_getcputemp(void) {
   return hlp_getcputemp();
}

u32t opts_acpiroot(void) {
   return sys_acpiroot();
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
               if (SysApp.askDlg(MSGA_TURNPAE)) pag_enable(); else
                  return 0;
            }
         }
      }
      /* we should avoid of this function if askDlg(MSGA_TURNPAE) returns
         false, because it too smart and turning it ON internally ;) */
      return sys_memhicopy(write?pos:(u32t)data, write?(u32t)data:pos, 256);
   } else {
      void *addr = (char*)pos + hlp_segtoflat(0);
      return hlp_memcpy(write?addr:data, write?data:addr, 256, MEMCPY_PG0);
   }
}

u32t opts_memread(u64t pos, void *data) {
   return opts_memio(pos, data, 0);
}

u32t opts_memwrite(u64t pos, void *data) {
   return opts_memio(pos, data, 1);
}

u32t opts_memend(void) {
   return sys_endofram();
}

void* opts_freadfull(const char *name, unsigned long *bufsize, int *reterr) {
   void *rc = freadfull(name, bufsize);
   if (reterr) *reterr = rc?0:errno;
   return rc;
}

int opts_fsetsize(FILE *ff, u64t newsize) {
   return _chsizei64(fileno(ff),newsize)?0:1;
}

int opts_fseek(FILE *ff, long long offset, int where) {
   return _lseeki64(fileno(ff), offset, where)==-1;
}

#else
#include "classes.hpp"
#include <conio.h>
#include <io.h>
#include <sys/stat.h>
#include <dos.h>
#define u64t u64

static TStrings opl;

void  opts_prepare(char *opts) {
   opl.SplitString(opts,",");
}

char *opts_get(const char *name) {
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

char  opts_bootdrive() { return 0; }
d     opts_port() { return 0; }
void  opts_yield() {}
void  opts_settitle(const char *title) {}

void  opts_bootkernel(char *name, char *opts) {
   TProgram::application->suspend();
   printf("%s [%s]\npress any key\n",name, opts);
   getch();
   TProgram::application->resume();
   TProgram::application->redraw();
}

unsigned long opts_loadldr(char *name) {
   TProgram::application->suspend();
   printf("[%s]\npress any key\n",name);
   getch();
   TProgram::application->resume();
   TProgram::application->redraw();
   // E_SYS_UNSUPPORTED
   return 14;
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

unsigned long opts_acpiroot(void) {
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
   memcpy((char*)data+1, &pos, 8);
   return (unsigned long)(pos>>8) & 1;
}

unsigned long opts_memwrite(u64t pos, void *data) {
   return 1;
}

unsigned long opts_memend(void) {
   return 0x80000000;
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


int opts_fsetsize(FILE *ff, u64t newsize) {
   if (newsize>=x7FFF) return 0;
   return chsize(fileno(ff),newsize)?0:1;
}

int opts_fseek(FILE *ff, long long offset, int where) {
   if ((long long)(long)offset!=offset) return 1;
   /* unfortunately _lseeki64() does not share file pointer with FILE* */
   return fseek(ff, offset, where);
}

#define CRC_POLYNOMIAL    0xEDB88320
static unsigned long CRC_Table[256];
static int CRC_Ready = 0;

static void lvm_buildcrc(void) {
   unsigned long ii, jj, crc;
   for (ii=0; ii<=255; ii++) {
      crc = ii;
      for (jj=8; jj>0; jj--)
         if (crc&1) crc = crc>>1^CRC_POLYNOMIAL; else crc>>=1;
      CRC_Table[ii] = crc;
   }
   CRC_Ready = 1;
}

extern "C"
unsigned long _stdcall lvm_crc32(unsigned long crc, void *Buffer, unsigned long BufferSize) {
   unsigned char *Current_Byte = (unsigned char*) Buffer;
   unsigned long Temp1, Temp2, ii;

   if (!CRC_Ready) lvm_buildcrc();

   for (ii=0; ii<BufferSize; ii++) {
     Temp1 = crc >> 8 & 0x00FFFFFF;
     Temp2 = CRC_Table[( crc ^ (unsigned long) *Current_Byte ) & (unsigned long)0xFF];
     Current_Byte++;
     crc = Temp1 ^ Temp2;
   }
   return crc;
}
#endif

u64t opts_fsize(const char *str) {
   struct _stati64 st;
   if (_stati64(str, &st)) return FFFF64;
   if (st.st_mode & S_IFDIR) return FFFF64;
   return st.st_size;
}

int opts_fileexist(const char *str) {
   struct _stati64 st;
   if (_stati64(str, &st)) return 0;
   if (st.st_mode & S_IFDIR) return 2;
   return 1;
}

u64t opts_freespace(unsigned drive) {
   diskfree_t dt;
   if (_dos_getdiskfree(drive+1, &dt)) return 0;
   return (u64t)dt.avail_clusters * (u64t)dt.sectors_per_cluster *
      (u64t)dt.bytes_per_sector;
}
