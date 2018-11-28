#include "qsbase.h"
#include "qsmodext.h"
#include "vioext.h"
#include "qsint.h"
#include "qsdm.h"

#include "clib.h"
#include "time.h"
#include "errno.h"
#include "setjmp.h"
#include "stdlib.h"
#include "direct.h"

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

static u32t _std threadfunc(void *arg) {
   mod_stop(0, 0, 0);
   return 0;
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
   u32t   ii, modes, mh;
   time_t       now;
   dir_t         di;
   vio_mode_info *mi = vio_modeinfo(0, 0, 0), *mp;

   if (argc>1) {
      if (stricmp(argv[1],"fps")==0) {
         // screen clear fps counter (for virtual console)
         u32t start = tm_counter(),
                cnt = 0;
         do {
            vio_clearscr();
            cnt++;
         } while (tm_counter()-start < 182);
         printf("%d.%d frames per second\n", cnt/10,cnt%10);
         return 0;
      } else
      if (stricmp(argv[1],"t")==0) {
         // rdtsc cycles in 55 ms.
         do {
            printf("%LX\n", hlp_tscin55ms());
         } while (key_wait(2)==0);
         return 0;
      } else
      if (stricmp(argv[1],"k")==0) {
         u32t tid;
         // trying to kill self from the second thread
         if (!mt_active()) {
            qserr res = mt_initialize();
            if (res) {
               cmd_shellerr(EMSG_QS, res, "MT mode:");
               return 1;
            }
         }
         // turn on trace output for MTLIB
         cmd_shellcall(shl_trace, "on mtlib", 0);

         tid = mt_createthread(threadfunc, MTCT_SUSPENDED|MTCT_NOFPU, 0, 0);
         mod_dumptree();
         mt_resumethread(0, tid);
         mt_waitthread(tid);
         // this line should never been printed
         printf("Something going wrong!\n");
         return 0;
      } else
      if (stricmp(argv[1],"m")==0 && argc==4) {
         static const char *cstr[MTRRF_TYPEMASK+1] = { "UC", "WC", "??", "??",
            "WT", "WP", "WB", "Mixed" };
         // try to query cache type for memory range
         u64t addr = strtoull(argv[2],0,16),
               len = strtoull(argv[3],0,16);
          
         u32t   ct = hlp_mtrrsum(addr,len);

         printf("cache type for %09LX..%09LX is %s\n", addr, addr+len-1, cstr[ct]);
      } else
      if (stricmp(argv[1],"r")==0 && argc==4) {
         // mem hide test
         u64t addr = strtoull(argv[2],0,16),
               len = strtoull(argv[3],0,16);
         sys_markmem(addr, len>>PAGESHIFT, PCMEM_HIDE|PCMEM_USERAPP);
      } else
      if (stricmp(argv[1],"c")==0 && (argc==2 || argc==3)) {
         // clock modulation setup
         u32t  cmv = argc==3 ? atoi(argv[2]) : CPUCLK_MAXFREQ, ii, cs;

         if (cmv<1 || cmv>CPUCLK_MAXFREQ) printf("value 1..16 required!\n"); else 
         for (ii=0; ii<argc-1; ii++) {
            // setup on 2st pass
            if (ii==1) {
               cs = hlp_cmsetstate(cmv);
               if (cs) printf(ANSI_LRED "setup error %u\n" ANSI_GRAY, cs); else 
                  printf(ANSI_LGREEN "setup ok\n" ANSI_GRAY);
            }
            cs = hlp_cmgetstate();

            if (!cs) printf("current state: not supported\n"); else {
               cs = 10000/CPUCLK_MAXFREQ*cs;
               printf("current state: %u.%2.2u%%\n", cs/100, cs%100);
            }
         }
         return 0;
      } else
      if (stricmp(argv[1],"u")==0 && argc==2) {
         // trap screen
         __asm { int 1 }
      } else
      // test exFAT i/o on 4Gb border
      if (stricmp(argv[1],"e")==0 && argc==2) {
        io_handle fh;
        u32t   stage = 0;
        u8t      buf[512];
        u64t    size;
        qserr     rc = io_open("test.bin", IOFM_CREATE_ALWAYS|IOFM_WRITE|IOFM_READ, &fh, 0);

        do {
           if (rc) break; else stage++;
           rc = io_setsize(fh, _4GBLL - 1024);
           if (rc) break; else stage++;
           if (io_write(fh,buf,512)!=512) rc = io_lasterror(fh);
           if (rc) break; else stage++;
           rc = io_setsize(fh, _4GBLL + 1024);
           if (rc) break; else stage++;
           if (io_seek(fh, _4GBLL + 1024, IO_SEEK_SET)==FFFF64) rc = io_lasterror(fh);
           if (rc) break; else stage++;
           if (io_write(fh,buf,512)!=512) rc = io_lasterror(fh);
           if (rc) break; else stage++;
           if (io_seek(fh, _4GBLL + 0x42000, IO_SEEK_SET)==FFFF64) rc = io_lasterror(fh);
           if (rc) break; else stage++;
           if (io_write(fh,buf,512)!=512) rc = io_lasterror(fh);
           rc = io_size(fh, &size);
           if (rc) break; else stage++;
           printf("Size: %LX\n", size);
           rc = io_close(fh);
        } while (0);
        if (rc) {
           char msg[32];
           snprintf(msg, 32, "Stage %u, error: ", stage);
           cmd_shellerr(EMSG_QS, rc, msg);
        }
        return 0;
      } else
      if (stricmp(argv[1],"a")==0 && argc==3) {
         u32t  size = str2ulong(argv[2]);
         void  *ptr = hlp_memalloc(size<<10, QSMA_LOCAL|QSMA_RETERR);
         printf("hlp_memalloc(%ukb) = %08X\npress any key to exit\n", size, ptr);
         key_read();
      } else
      if (stricmp(argv[1],"ct")==0 && argc==2) {
         u64t cv;
         while (!key_pressed()) {
            printf("\rclock: %Lu ms", clock()/1000LL);
         }
         printf("\n");
      } else
      if (stricmp(argv[1],"l")==0 && argc==3) {
         u32t  size;
         void   *fd = freadfull(argv[2], &size);
         if (!fd) printf("no file \"%s\"\n", argv[2]); else {
            vio_listref *ref = (vio_listref*)malloc(sizeof(vio_listref));
            ref->size  = sizeof(vio_listref);
            ref->text  = str_settext((char*)fd, size);
            ref->items = ref->text->count-1;
            ref->id    = ref->items?(u32t*)calloc(ref->items,4):0;
            ref->subm  = 0;
            hlp_memfree(fd);
            vio_showlist("List test", ref, MSG_LIGHTBLUE, 0);
            if (ref->id) free(ref->id);
            free(ref->text);
            free(ref);
         }
      } else
      if (stricmp(argv[1],"f")==0 && argc==3) {
         // test of module unload
         u32t mod = mod_query(argv[2], MODQ_NOINCR), rc;
         if (!mod) printf("There is no module \"%s\"\n",argv[2]); else {
            //log_mdtdump();
            rc = mod_free(mod);
            if (rc) printf("mod_free() error %d\n",rc); else
               printf("mod_free(\"%s\") ok\n",argv[2]);
            //log_mdtdump();
         }
      } else
      if (stricmp(argv[1],"f")==0 && (argc==2 || argc==4)) {
         u8t *savebuf = malloc(fpu_statesize()+48),
               *alptr = savebuf;
         double    aa = 1.5,
                   bb = 123.456;
         /* heap ptr is always aligned to 16 bytes, but we needs 64-byte
            alignment here */
         if ((ptrdiff_t)alptr&0x3F) alptr = (u8t*)Round64((ptrdiff_t)alptr);
         mem_zero(savebuf);

         printf("saving state... ");
         fpu_statesave(alptr);
         printf("ok\nrestoing... ");
         fpu_staterest(alptr);
         printf("done\nand still alive!\n");

         printf("%f %f\n", aa, bb);
         printf("%g %g\n", aa/2, bb-123);
         if (argc==4) {
            printf("atof(%s) = %f\n", argv[2], atof(argv[2]));
            printf("atof(%s) = %g\n", argv[3], atof(argv[3]));
         }

         free(savebuf);
      } else
      if (stricmp(argv[1],"g")==0 && argc==3) {
         // create 40Gb GPT partition directly on 2TB border
         u32t disk = dsk_strtodisk(argv[2]);
         if (disk==FFFF) {
            printf("Invalid disk name (%s)\n", argv[2]);
         } else {
            // 32 sectors before 2Tb
            u32t rc = dsk_gptcreate(disk, 0xFFFFFFE0, 40*1024*1024*2, DFBA_PRIMARY, 0);
            if (rc) {
               char topic[16], *msg;
               sprintf(topic, "_DPTE%02d", rc);
               msg = cmd_shellgetmsg(topic);
               if (msg) {
                  printf("Error: %s\n", msg);
                  free(msg);
               } else
                  printf("Error code 0x%04X\n", rc);
            }
         }
      } else
      if (stricmp(argv[1],"p")==0 && argc==3) {
         // query page access
         static const char* pastr[4] = {"unknown", "not present", 
                                        "read only", "writeable"};
         u32t addr = strtoul(argv[2],0,16),
              mode = pag_query((void*)addr);
         printf("page at %08X is %s\n", addr&~PAGEMASK, pastr[mode]);
      } else
      if (stricmp(argv[1],"s")==0 && argc==3) {
         // query GDT selector data
         u8t sd[8];
         u16t  sel = strtoul(argv[2],0,16);
         if (sys_selquery(sel,&sd)) printf("%04X: %8b\n", sel, &sd);
            else printf("no sel %04X\n", sel);
      } else
      if (stricmp(argv[1],"scanf")==0 && argc==2) {
         int aa, rc;
         char s1[24], s2[10], s3[10];
         double fv;

         printf("str str float int hex hex_chars: ");
         rc = scanf("%23s %9s %lf %*d %x %9[0123456789ABCDEFabcdef]",
            s1, s2, &fv, &aa, s3);

         if (rc==EOF) {
            printf("\nerror in string!");
         } else {
            printf("\n%i values: \"%s\" \"%s\" %g %X \"%s\"\n", rc, s1, s2,
               fv, aa, s3);
            printf("gcvt: %s\n", _gcvt(fv, 3, s1));
         }
      } else
      if (stricmp(argv[1],"d")==0 && argc==3) {
         // delay x ms
         u32t delay = atoi(argv[2]);
         usleep(delay*1000);
      } else
      if (stricmp(argv[1],"b")==0 && argc==3) {
         return vio_msgbox("test", argv[2], MSG_GREEN|MSG_CENTER|MSG_OKCANCEL|
            MSG_NOSHADOW, 0);
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

   vio_setmodeex(80,43);

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
         printf("%15s %8Lu %s", di.d_name, di.d_size, ctime(&di.d_wtime));
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
   mh = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   if (mh) {
      mod_apichain(mh,102,APICN_ONENTRY,hook_proc);
      mod_apichain(mh,102,APICN_ONEXIT,hook_proc);
      vio_clearscr();
      ii  = mod_apiunchain(mh,102,0,hook_proc);
      log_it(2,"%d hooks removed\n",ii);
   }
#else
   vio_clearscr();
#endif

   for (mp=mi, modes=0; mp->size; mp++) {
      char cch = ++modes<10?'0'+modes:'A'+modes-10;

      if (mp->flags&VMF_GRAPHMODE) {
         printf("%c. %4ux%-4u   in %ux%ux%u ", cch, mp->mx, mp->my,
            mp->gmx, mp->gmy, mp->gmbits);
         if (mp->gmbits>8)
            printf("(r:%08X g:%08X b:%08X a:%08X)", mp->rmask, mp->gmask,
               mp->bmask, mp->amask);
         printf("\n");
      } else
         printf("%c. %4ux%-4u\n", cch, mp->mx, mp->my);
   }
   printf("%d modes available\n", modes);
   free(mi);
   mi = 0;
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
   do {
      ii = key_wait(3);
      ii&= 0xFF;
      if (ii==27||tolower(ii)=='n') return 0; else break;
   } while (1);
   printf("\rtoo late, playing ;)                  \n");

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
