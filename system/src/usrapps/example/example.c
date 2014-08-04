#include "clib.h"
#include "vioext.h"
#include "setjmp.h"
#include "console.h"
#include "qslog.h"
#include "errno.h"
#include "time.h"
#include "stdlib.h"
#include "direct.h"
#include "qsxcpt.h"
#include "qsmodext.h"
#include "qsutil.h"
#include "qstime.h"
#include "qssys.h"
#include "qshm.h"
#include "qsdm.h"
#include "qsshell.h"
#include "qspage.h"

jmp_buf jmp;

void  _std log_mdtdump(void);

u32t music[][2] = {
   {175,300},{233,450},{262,300},{294,300},{330,300},{294,300},{262,450},{220,300},{175,450},
   {196,300},{220,300},{233,450},{196,300},{196,450},
   {175,300},{196,300},{220,450},{175,300},{147,450},
   {175,300},{233,450},{262,300},{294,300},{330,300},{294,300},{262,450},{220,300},{175,450},
   {196,300},{220,300},{233,300},{220,300},{196,300},{185,300},
   {165,300},{185,300},{196,450},{195,600},
   {349,300},{348,450},{330,300},{294,300},{262,450},{220,300},{175,450},
   {196,300},{220,450},{233,450},{196,300},{195,450},
   {175,300},{196,300},{220,450},{175,300},{147,450},
   {349,300},{349,300},{330,300},{294,300},{262,450},{220,300},{175,450},
   {196,300},{220,450},{233,450},{220,300},{196,450},{185,300},
   {165,300},{185,300},{196,450},{195,450},{0,0}};

void div0(void) {
   u32t xx=0;
   _try_ {
      printf("%d",1/xx);
      _throw_(xcpt_bound);
   }
   _catch_(xcpt_bound) {
      printf("exception in div0()!\n");
   }
   _endcatch_
}

void func(void) {
   printf("3 secs before longjmp!\n");
   key_wait(3);
   longjmp(jmp,1);
}

int _std hook_proc(mod_chaininfo *mc) {
   log_it(2,"%s hook: userdata %08X, replace %08X\n", mc->mc_entry?"Entry":"Exit",
      mc->mc_userdata, mc->mc_replace);
   mc->mc_userdata = 0x12345678;
   return 1;
}

void _std clearscr(void) {
   printf("cls must be here :)\n");
   memsetw((void*)hlp_segtoflat(0xB800), 0xEA20, 80*25*2);
}

// sorted disk map
void printDisk(u32t disk) {
   dsk_mapblock *dm = dsk_getmap(disk);
   if (dm) {
      int ii = 0;
      do {
         printf("%016LX %016LX %02X (%d)\n", dm[ii].StartSector, dm[ii].Length,
            dm[ii].Type, dm[ii].Index);
      } while ((dm[ii++].Flags&DMAP_LEND)==0);
      free(dm);
      printf("press key to continue!\n");
      key_wait(10);
   }
   exit(0);
}

int main(int argc,char *argv[]) {
   u32t  ii, modes;
   time_t      now;
   dir_t        di;
   con_modeinfo *mi = con_querymode(&modes);

   if (argc>1) {
      if (stricmp(argv[1],"r")==0 && argc==4) {
         u64t addr = strtoull(argv[2],0,16),
               len = strtoull(argv[3],0,16);
         sys_markmem(addr, len>>PAGESHIFT, PCMEM_HIDE|PCMEM_USERAPP);
      } else {
         u64t addr = strtoull(argv[1],0,16);
         u8t*  mem = pag_physmap(addr, 4096, 0);
         if (mem) {
            // print to screen and log
            cmd_printseq(0, 3, 0);
            cmd_printf("%010LX: %16b\n", addr, mem);
            cmd_printf("%010LX: %16b\n", addr + 16, mem + 16);
            cmd_printf("%010LX: %16b\n", addr + 32, mem + 32);
            
            pag_physunmap(mem);
         } else 
            cmd_printf("unable to read memory at addr %LX\n", addr);
      }
      return 0;
   }
   if (argc>1)
      return vio_msgbox("test", argv[1], MSG_GREEN|MSG_CENTER|MSG_OKCANCEL, 0);

#if 0
   {
      u16t buffer[160], *bp;
      u32t cnt = hlp_mtrrbatch(buffer);
      printf("%d msr entries\n",cnt);
      bp = buffer;
      for (ii=0; ii<cnt; ii++) {
         printf("%2d. %04X %016LX", ii+1, *bp, *(u64t*)(bp+1));
         printf(ii&1?"\n":"    ");
         bp+=5;
      }
      return 0;
   }
#endif

   //printDisk(1);

   con_setmode(80,43,0);
   printf("%d modes available\n",modes);

   vio_setcolor(VIO_COLOR_LRED);
   printf("hello, world!\n");
   time(&now);
   printf(ctime(&now));


   if (argc>1) {
      setenv("TEST_ENV","1",1);
      printf("%s\n",argv[1]);
   }

   if (!_dos_findfirst("B:\\", _A_ARCH,&di)) {
      do {
         struct tm tme;
         dostimetotm((u32t)di.d_date<<16|di.d_time, &tme);
         printf("%15s %8d %s",di.d_name,di.d_size,asctime(&tme));
      } while (!_dos_findnext(&di));
      _dos_findclose(&di);
   } else 
      printf("_dos_findfirst error %i\n",errno);

   if (setjmp(jmp)) {
      printf("longjmp ok\n");
   } else {
      vio_setcolor(VIO_COLOR_RESET); // reset colored output
      func();
   }

   // exceptions test
   _try_ {
      u64t aa = tm_counter();
      printf("__int64 test: %LX %LX\n", aa*123, strtoull("0x140000000",0,0));
      div0();
   }
   _catch_(xcpt_opcode) {
      log_it(2,"invalid opcode exception in main()\n");
   }
   _alsocatch_(xcpt_all) {
      log_it(2,"exception %d in main()\n",_currentexception_());
   }
   _endcatch_

   key_wait(3);
#if 1
   ii = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   if (ii) {
      mod_apichain(ii,102,APICN_ONENTRY,hook_proc);
      mod_apichain(ii,102,APICN_ONEXIT,hook_proc);
      mod_apichain(ii,102,APICN_REPLACE,clearscr);
      
      vio_clearscr();
      ii = mod_apiunchain(ii,102,0,0);
      log_it(2,"%d hooks removed\n",ii);
   }
#else
   vio_clearscr();
#endif

   if (mi[0].infosize!=sizeof(con_modeinfo)) {
      printf("CONSOLE.DLL version mismatch, unable to print mode table!\n");
   } else
   for (ii=0; ii<modes; ii++) {
      char cch = ii<10?'0'+ii:'A'+ii-10;

      if (mi[ii].flags&CON_GRAPHMODE) {
         printf("%c. %4dx%4dx%2d bits", cch, mi[ii].width, mi[ii].height,
            mi[ii].bits);
         if (mi[ii].bits>8)
            printf("(r:%08X g:%08X b:%08X a:%08X)", mi[ii].rmask, mi[ii].gmask,
               mi[ii].bmask, mi[ii].amask);
         printf("\n");
      } else {
         printf("%c. %4dx%4d text\n", cch, mi[ii].width, mi[ii].height);
      }
   }
#if 0
   // rdmsr test
   _try_ {
      u32t aa, bb, reg=512;
      u32t rc = hlp_readmsr(reg, &aa, &bb);
      log_it(2,"msr reg %d = %08X%08X\n", reg, bb, aa);
   }
   _catch_(xcpt_all) {
      log_it(2,"exception %d in hlp_readmsr()\n",_currentexception_());
   }
   _endcatch_
#endif
   printf("play music? (Y/n, pc speaker required)");
   ii = key_wait(3)&0xFF;
   printf("\rtoo late, playing ;)                  \n");
   if (ii==27||tolower(ii)=='n') return 0;

   ii = 0;
   while (music[ii][0]) {
      log_printf("%d Hz, %d ms\n", music[ii][0], music[ii][1]);
      vio_beep(music[ii][0], music[ii][1]);
      if (key_pressed()) break;
      usleep(music[ii][1]*1000);
      ii++;
   }
   return 0;
}
