#include "stdlib.h"
#include "qsbase.h"
#include "qsint.h"
#include "errno.h"
#include "vioext.h"

void shl_printmtrr(int nopause) {
   u32t flags, state, addrbits, idx,
        regs = hlp_mtrrquery(&flags, &state, &addrbits);
   if (!regs) {
      cmd_printseq("\nThere is no MTRR registers available.", 0, 0);
   } else {
      static const char *mtstr[] = { "uncacheable", "write combining", "???",
         "???", "write-through", "write-protected", "writeback", "???"};
      static u8t color[] = { 0x0C, 0x0B, 0x08, 0x08, 0x0D, 0x0E, 0x0A, 0x08};

      cmd_printseq("\nMTRR is %s, fixed range %s, default type - %s",
         nopause?-1:0, 0, state&MTRRS_MTRRON?"on":"off", state&MTRRS_FIXON?"on":"off",
            mtstr[state&MTRRF_TYPEMASK]);

      if (flags&MTRRQ_FIXREGS) {
         u32t fstart[MTRR_FIXEDMAX], flength[MTRR_FIXEDMAX], fstate[MTRR_FIXEDMAX],
              fcnt = hlp_mtrrfread(fstart, flength, fstate),
               fon = (state&(MTRRS_MTRRON|MTRRS_FIXON))==(MTRRS_MTRRON|MTRRS_FIXON);

         if (fcnt) {
            cmd_printseq("\nMTRR fixed range registers:", nopause?-1:0, 0);

            for (idx=0; idx<fcnt; idx++) {
               u32t ct = fstate[idx]&MTRRF_TYPEMASK;

               if (cmd_printseq("%2d. %06X-%06X, size: %8u kb - %s, %s", 
                  nopause?-1:0, fon?color[ct]:0, idx+1, fstart[idx],
                     flength[idx]?fstart[idx]+flength[idx]-1:fstart[idx],
                        flength[idx]>>10, fon?" on":"off", mtstr[ct])) break;
            }
         }
      }
      cmd_printseq("\nMTRR variable range registers:", nopause?-1:0, 0);
      for (idx=0; idx<regs; idx++) {
         u64t start, length;
         u32t rstate;
         int rc = hlp_mtrrvread(idx, &start, &length, &rstate);

         if (cmd_printseq("%2d. %010LX-%010LX, size: %8u kb - %s, %s", 
            nopause?-1:0, (rstate&MTRRF_ENABLED)!=0 && (state&MTRRS_MTRRON)!=0?
               color[rstate&MTRRF_TYPEMASK]:0, idx+1, start, length?start+length-1:start,
                  (u32t)(length>>10), rstate&MTRRF_ENABLED?" on":"off",
                     mtstr[rstate&MTRRF_TYPEMASK])) break;
      }
   }
}

static int getcachetype(const char *ctstr) {
   if (!ctstr||!*ctstr||ctstr[2]!=0) return -1; else {
      static const char *cts[7] = { "UC", "WC", 0, 0, "WT", "WP", "WB"};
      int ii = -1;
      while (++ii<7)
         if (cts[ii])
            if (*(u16t*)ctstr==*(u16t*)cts[ii]) return ii;
   }
   return -1;
}

static void printMTRRerr(u32t err) {
   switch (err) {
      case MTRRERR_STATE  : printf("Incorrect cache type parameter\n"); break;
      case MTRRERR_HARDW  : printf("MTRR is not available\n"); break;
      case MTRRERR_ACCESS : printf("MSR registers access failed\n"); break;
      case MTRRERR_INDEX  : printf("Invalid variable register index\n"); break;
      case MTRRERR_ADDRESS: printf("Invalid start address\n"); break;
      case MTRRERR_LENGTH : printf("Invalid memory block length\n"); break;
      case MTRRERR_ADDRTRN: printf("Low bits of address truncated by length mask\n"); break;
      default:
         if (err) printf("MTRR setup error %d\n",err);
   }
}

u32t _std shl_mtrr(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, tolog=0;
   if (args->count>0) {
      static char *argstr   = "/np|/log";
      static short argval[] = {  1,   2};
      // help overrides any other keys
      if (str_findkey(args, "/?", 0)) {
         cmd_shellhelp(cmd,CLR_HELP);
         return 0;
      }
      args = str_parseargs(args, 0, 1, argstr, argval, &nopause, &tolog);
      // command is non-empty
      while (args->count>0) {
         int  state_setup = 0;
         u32t state_value = 0,
                    vregs = hlp_mtrrquery(0, &state_value, 0);
         char       *arg0 = args->item[0];
         // used below to compare chars
         strupr(arg0);
         
         if (!vregs) {
            printf("MTRR is not available\n");
            rc = ENOSYS;
            break;
         } else
         if (args->count==1) {
            if (strcmp(arg0,"VIEW")==0) {
               cmd_printseq(0, 1+tolog, 0);
               shl_printmtrr(nopause);
               rc = 0;
            } else
            if (strcmp(arg0,"RESET")==0) {
               rc = hlp_mtrrquery(0,0,0)?0:ENOSYS;
               if (!rc) hlp_mtrrbios();
            } else
            if (strcmp(arg0,"ON")==0 || strcmp(arg0,"OFF")==0) {
               state_setup = 1;
               if (arg0[1]=='N') state_value|=MTRRS_MTRRON; else
                  state_value&=~MTRRS_MTRRON;
            }
         } else {
            char *arg1, *arg2, *arg3, *arg4;
            // a shoter aliases
            if (args->count>1) {
               if (args->count>2) {
                  if (args->count>3) {
                     if (args->count>4) strupr(arg4 = args->item[4]);
                     strupr(arg3 = args->item[3]);
                  }
                  strupr(arg2 = args->item[2]);
               }
               strupr(arg1 = args->item[1]);
            }

            if (args->count==2) {
               if (strcmp(arg0,"FIXED")==0) {
                  if (strcmp(arg1,"ON")==0 || strcmp(arg1,"OFF")==0) {
                     state_setup = 1;
                     if (arg1[1]=='N') state_value|=MTRRS_FIXON; else
                        state_value&=~MTRRS_FIXON;
                  }
               } else
               if (strcmp(arg0,"DEFAULT")==0) {
                  int ct = getcachetype(arg1);
                  if (ct<0) rc = -1; else {
                     state_value = state_value & ~MTRRS_DEFMASK | ct;
                     state_setup = 1;
                  }
               }
            } else
            if (args->count==3) {
               if (strcmp(arg0,"SET")==0 && (strcmp(arg2,"ON")==0 ||
                  strcmp(arg2,"OFF")==0)) 
               {
                  u64t start,length;
                  // accept hex & dec only, not octal
                  int  idx = str2long(arg1);
            
                  if (idx<=0||idx>vregs) {
                     printf("Invalid variable register index: %d.\n", idx);
                     rc = EINVAL;
                     break;
                  }
                  idx--;
            
                  if (!hlp_mtrrvread(idx, &start, &length, &state_value)) {
                     printMTRRerr(FFFF); rc = EFAULT;
                  } else {
                     int enabled = state_value&MTRRF_ENABLED?1:0,
                         need2be = arg2[1]=='N'?1:0;
                     // call only if state must be changed
                     if ((enabled^need2be)==0) rc = 0; else {
                        if (need2be) state_value|=MTRRF_ENABLED; else
                           state_value&=~MTRRF_ENABLED;
                        rc = hlp_mtrrvset(idx,start,length,state_value);
                        if (rc) { printMTRRerr(rc); rc = EFAULT; } else rc = 0;
                     }
                  }
               }
            } else
            if (args->count==4) {
               if (strcmp(arg0,"FIXED")==0) {
                  int ct = getcachetype(arg3);
                  if (ct<0) rc = -1; else {
                     rc = hlp_mtrrfset(str2ulong(arg1), str2ulong(arg2), ct);
                     if (rc) { printMTRRerr(rc); rc = EFAULT; } else rc = 0;
                  }
               }
            } else
            if (args->count==5) {
               if (strcmp(arg0,"SET")==0) {
                  int ct = getcachetype(arg4);
                  if (ct<0) rc = -1; else {
                     rc = hlp_mtrrvset(str2long(arg1)-1, str2uint64(arg2),
                                       str2uint64(arg3), MTRRF_ENABLED|ct);
                     if (rc) { printMTRRerr(rc); rc = EFAULT; } else rc = 0;
                  }
               }
            }
         }
         if (rc<=0 && state_setup) {
            rc = hlp_mtrrstate(state_value);
            if (rc) { printMTRRerr(rc); rc = EFAULT; } else rc = 0;
         }
         break;
      }
      // free args clone, returned from str_parseargs() above
      free(args);
   }
   if (rc<0) {
      rc = EINVAL;
      if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   }
   return rc;
}

u32t _std shl_mem(const char *cmd, str_list *args) {
   static char *argstr   = "/a|/o|/m|/np|/log|hide|save";
   static short argval[] = { 1, 1, 1,  1,   2,   1,   1 };
   u32t  maxblock, total, avail;
   int  acpitable = 0, os2table = 0, nopause = 0, idx, tolog = 0, mtrr = 0,
             hide = 0, save = 0;
   qserr      res = E_SYS_INVPARM;
   int      nomsg = 0;
   // help is in the priority
   if (str_findkey(args, "/?", 0)) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   args = str_parseargs(args, 0, 1, argstr, argval, &acpitable, &os2table, 
                        &mtrr, &nopause, &tolog, &hide, &save);

   if (save && !hide && (args->count==3 || args->count==2 &&
      stricmp(args->item[1],"ALL")==0) && !acpitable && !os2table && !mtrr)
   {
      u64t   start = args->count==3 ? strtoull(args->item[1], 0, 16) : 0,
               len = args->count==3 ? strtoull(args->item[2], 0, 16) : sys_endofram();
      if (len)
         if (start + len < start) {
            printf("Address range cannot cross 0.\n");
            nomsg++;
         } else
         if (start<_4GBLL && start + len>_4GBLL) {
            printf("Address range cannot cross 4Gb border.\n");
            nomsg++;
         } else {
            io_handle fh = 0;
            res = io_open(args->item[0], IOFM_CREATE_NEW|IOFM_WRITE|IOFM_SHARE, &fh, 0);
            if (!res) res = io_setsize(fh, len);
            if (!res) {
               void *mem = hlp_memalloc(_64KB, QSMA_RETERR|QSMA_NOCLEAR|QSMA_LOCAL);
               if (!mem) res = E_SYS_NOMEM; else {
                  while (len) {
                     u32t towr = len>_64KB?_64KB:len, succ;
                     u16t  key = key_wait(0);
                     void *src = mem;
                     if ((key&0xFF)==27) {
                        char msg[64];
                        snprintf(msg, 64, "Break process?^Current position is %LX", start);
                        if (vio_msgbox("mem save", msg, MSG_LIGHTRED|MSG_DEF2|
                           MSG_YESNO, 0)==MRES_YES) { res = E_SYS_UBREAK; break; }
                     }
                     /* both disk i/o buffer & page 0 located in 1st Mb, so
                        copy it via buffer, as well as memory above 4Gb.
                        For memory above 1st Mb use hlp_memcpy() just as
                        test for exception */
                     if (start>=_4GBLL)
                        succ = sys_memhicopy((u32t)mem, start, towr);
                     else {
                        succ = hlp_memcpy(mem, (u8t*)(u32t)start, towr, MEMCPY_PG0);
                        if (succ && start>=_1MB) src = (void*)(u32t)start;
                     }

                     if (!succ) {
                        printf("Memory access error at pos %LX.\n", start);
                        nomsg++; res = E_SYS_SOFTFAULT;
                        break; 
                     }
                     if (io_write(fh, src, towr)<towr) {
                        printf("Disk write error at pos %LX.\n", start);
                        nomsg++; res = E_DSK_ERRWRITE;
                        break; 
                     }
                     start += towr;
                     len   -= towr;
                  }
                  hlp_memfree(mem);
               }
            }
            if (fh)
               if (!res) res = io_close(fh); else {
                  io_close(fh);
                  io_remove(args->item[0]);
               }
         }   
   } else
   if (hide && !save && !mtrr) {
      int        ii;
      // merge it back to get comma separated list
      char    *line = str_gettostr(args, " ");
      str_list *cml = str_split(line, ",");
      free(line);

      for (ii=0, res=0; ii<cml->count && res==0; ii++) {
         str_list *crange = str_split(cml->item[ii], " ");
         if (crange->count!=2) {
            printf("Invalid range argument at pos %i (%s).\n", ii+1, cml->item[ii]);
            nomsg++; res = E_SYS_INVPARM;
         } else {
            u64t addr = strtoull(crange->item[0],0,16),
                  len = strtoull(crange->item[1],0,16);
            if (!addr || !len) {
               printf("Start address and length cannot be zero.\n");
               nomsg++; res = E_SYS_INVPARM;
            } else
            if ((addr&PAGEMASK)!=0 || (len&PAGEMASK)!=0) {
               printf("Both start address and length must be aligned to page size (4kb).\n");
               nomsg++; res = E_SYS_INVPARM;
            } else
            if (addr>=(u64t)sys_endofram()) {
               printf("Start address beyond the end of physical memory.\n");
               nomsg++; res = E_SYS_INVPARM;
            } else {
               u32t      pages = len>>PAGESHIFT, errcnt = 0,
                        staddr = addr;
               pcmem_entry *ml = sys_getpcmem(0), *mp = ml;
               while (mp->pages) {
                  u64t next = mp->start + ((u64t)mp->pages<<PAGESHIFT);
                  if (mp->start<=addr && next>addr) {
                     u32t  lp = next - addr >> PAGESHIFT,
                        btype = mp->flags&PCMEM_TYPEMASK;
                     if (lp>pages) lp = pages;
               
                     if (btype!=PCMEM_RESERVED) {
                        if (sys_markmem(addr, lp, PCMEM_HIDE|(btype==PCMEM_FREE?
                           PCMEM_USERAPP:btype))) errcnt++;
                        // trying to preserve in QSINIT`s own memory manager too
                        if (btype==PCMEM_QSINIT)
                           if (!hlp_memreserve(addr,lp<<PAGESHIFT,0)) errcnt++;
                     }
                     pages -= lp;
                     addr  += lp<<PAGESHIFT;
                     if (!pages) break;
                  }
                  mp++;
               }
               free(ml);
               // update array in "physmem" storage key
               hlp_setupmem(0, 0, 0, SETM_SPLIT16M);
               if (errcnt)
                  printf("%d error(s) occured while hiding range %08X..%08X.\n",
                     errcnt, staddr, staddr+len-1);
               res = errcnt?E_SYS_ACCESS:0;
            }
         }
         free(crange);
      }
      free(cml);
   } else 
   if (args->count>0 || hide || save) {
      // unknown / invalid combination of arguments in source string
      res = E_SYS_INVPARM;
   } else
      res = 0;

   free(args);
   args = 0;
   if (res) {
      if (!nomsg) cmd_shellerr(EMSG_QS, res, 0);
      return qserr2errno(res);
   }
   if (save) return 0;
   // continue with /o or /a even after hide/save command
   if (hide && !os2table && !acpitable) return 0;

   cmd_printseq(0, 1+tolog, 0);
   avail = hlp_memavail(&maxblock, &total);

   cmd_printseq("Loader memory: %dkb, available %dkb\n"
      "Maximum available block size: %d kb", 0, 0, total>>10, avail>>10,
         maxblock>>10);

   if (os2table) {
      physmem_block *phm = (physmem_block*)sto_data(STOKEY_PHYSMEM);
      u32t     phm_count = sto_size(STOKEY_PHYSMEM)/sizeof(physmem_block);

      cmd_printseq("\nPhysical memory, available for OS/2:", 0, 0);
      for (idx=0;idx<phm_count;idx++)
         if (cmd_printseq("%2d. Address:%08X Size:%7u kb", nopause?-1:0, 0,
            idx+1, phm[idx].startaddr, phm[idx].blocklen>>10)) return 0;
   }
   if (acpitable) {
      AcpiMemInfo *tbl = hlp_int15mem();
      u32t       memsz = 0;
      // copying data from disk buffer first
      idx = 0;
      while (tbl[idx].LengthLow||tbl[idx].LengthHigh) {
         if (tbl[idx].AcpiMemType==1)
            memsz += (tbl[idx].LengthHigh<<22) + (tbl[idx].LengthLow>>10);
         idx++;
      }
      cmd_printseq("\nPC physical memory table (%ukb available):", 0, 0, memsz);
      if (idx) {
         int  ii;
         for (ii=0; ii<idx; ii++) {
            // avoid 64bit arithmetic
            u32t  szk = tbl[ii].LengthHigh<<22;
            int btidx = tbl[ii].AcpiMemType;
            static char *btype[] = { "usable", "reserved", "reserved (ACPI)",
               "reserved (NVS)", "unusable", "used by UEFI", "unknown" };
            static u8t color[] = { 0x0A, 0x0C, 0x0E, 0x0E, 0x08, 0x0E, 0x06 };
            /* types 5 & 6 are custom, added by our`s EFI host, common BIOS
               should never return it */
            btidx = btidx>6||!btidx ? 6 : btidx-1;

            if (cmd_printseq("%2d. Address:%02X%08X  Size:%10d kb - %s",
               nopause?-1:0, color[btidx], ii+1, tbl[ii].BaseAddrHigh,
                  tbl[ii].BaseAddrLow, szk + (tbl[ii].LengthLow>>10),
                     btype[btidx])) break;
         }
      }
      free(tbl);
   }
   if (mtrr) shl_printmtrr(nopause);
   return 0;
}
