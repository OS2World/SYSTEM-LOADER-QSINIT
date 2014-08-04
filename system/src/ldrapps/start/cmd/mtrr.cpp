#include "qshm.h"

void shl_printmtrr(int nopause) {
   u32t flags, state, addrbits, idx,
        regs = hlp_mtrrquery(&flags, &state, &addrbits);
   if (!regs) {
      pause_println("\nThere is no MTRR registers available.");
   } else {
      static const char *mtstr[] = { "uncacheable", "write combining", "???",
         "???", "write-through", "write-protected", "writeback", "???"};
      static u8t color[] = { 0x0C, 0x0B, 0x08, 0x08, 0x0D, 0x0E, 0x0A, 0x08};

      spstr ln;
      ln.sprintf("\nMTRR is %s, fixed range %s, default type - %s", 
         state&MTRRS_MTRRON?"on":"off", state&MTRRS_FIXON?"on":"off",
            mtstr[state&MTRRF_TYPEMASK]);
      pause_println(ln(), nopause?-1:0);

      if (flags&MTRRQ_FIXREGS) {
         u32t fstart[MTRR_FIXEDMAX], flength[MTRR_FIXEDMAX], fstate[MTRR_FIXEDMAX],
              fcnt = hlp_mtrrfread(fstart, flength, fstate),
               fon = (state&(MTRRS_MTRRON|MTRRS_FIXON))==(MTRRS_MTRRON|MTRRS_FIXON);

         if (fcnt) {
            pause_println("\nMTRR fixed range registers:", nopause?-1:0);

            for (idx=0; idx<fcnt; idx++) {
               u32t ct = fstate[idx]&MTRRF_TYPEMASK;

               ln.sprintf("%2d. %06X-%06X, size: %8u kb - %s, %s",idx+1,
                  fstart[idx], flength[idx]?fstart[idx]+flength[idx]-1:fstart[idx],
                     flength[idx]>>10, fon?" on":"off", mtstr[ct]);
               if (pause_println(ln(), nopause?-1:0, fon?color[ct]:0)) break;
            }
         }
      }
      pause_println("\nMTRR variable range registers:", nopause?-1:0);
      for (idx=0; idx<regs; idx++) {
         u64t start, length;
         u32t rstate;
         int rc = hlp_mtrrvread(idx, &start, &length, &rstate);

         ln.sprintf("%2d. %010LX-%010LX, size: %8u kb - %s, %s",idx+1,
            start, length?start+length-1:start, (u32t)(length>>10),
               rstate&MTRRF_ENABLED?" on":"off", mtstr[rstate&MTRRF_TYPEMASK]);

         if (pause_println(ln(), nopause?-1:0, (rstate&MTRRF_ENABLED)!=0 && 
            (state&MTRRS_MTRRON)!=0?color[rstate&MTRRF_TYPEMASK]:0)) break;
      }
   }
}

static int getcachetype(const char *ctstr) {
   if (!ctstr||!*ctstr||ctstr[2]!=0) return -1;
   static const char *cts[7] = { "UC", "WC", 0, 0, "WT", "WP", "WB"};
   int ii=-1;
   while (++ii<7)
      if (cts[ii])
         if (*(u16t*)ctstr==*(u16t*)cts[ii]) return ii;
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
   int rc=-1, nopause=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      idx = al.IndexOfName("/np");
      if (idx>=0) { nopause=1; al.Delete(idx); }
      // process
      al.TrimEmptyLines();
      al[0].upper();

      int  state_setup = 0;
      u32t state_value = 0,
                 vregs = hlp_mtrrquery(0,&state_value,0);

      if (!vregs) {
         printf("MTRR is not available\n");
         return ENOSYS;
      } else
      if (al.Count()==1) {
         if (al[0]=="VIEW") {
            pause_println(0,1);
            shl_printmtrr(nopause);
            rc = 0;
         } else
         if (al[0]=="RESET") {
            rc = hlp_mtrrquery(0,0,0)?0:ENOSYS;
            if (!rc) hlp_mtrrbios();
         } else
         if (al[0]=="ON" || al[0]=="OFF") {
            state_setup = 1;
            if (al[0][1]=='N') state_value|=MTRRS_MTRRON; else
               state_value&=~MTRRS_MTRRON;
         }
      } else
      if (al.Count()==2) {
         al[1].upper();
         if (al[0]=="FIXED") {
            if (al[1]=="ON" || al[1]=="OFF") {
               state_setup = 1;
               if (al[0][1]=='N') state_value|=MTRRS_FIXON; else
                  state_value&=~MTRRS_FIXON;
            }
         } else
         if (al[0]=="DEFAULT") {
            int ct = getcachetype(al[1]());
            if (ct<0) rc = EINVAL; else {
               state_value = state_value & ~MTRRS_DEFMASK | ct;
               state_setup = 1;
            }
         }
      } else
      if (al.Count()==3) {
         al[1].upper(); al[2].upper();
         if (al[0]=="SET" && (al[2]=="ON" || al[2]=="OFF")) {
            int idx = al[1].Int();
            if (idx<=0||idx>vregs) {
               printf("Invalid variable register index: %d.\n", idx);
               return EINVAL;
            }
            u64t start,length;
            idx--;

            if (!hlp_mtrrvread(idx,&start,&length,&state_value)) {
               printMTRRerr(FFFF);
               return EFAULT;
            } else {
               int enabled = state_value&MTRRF_ENABLED?1:0,
                   need2be = al[2][1]=='N'?1:0;
               // call only if state must be changed
               if ((enabled^need2be)==0) rc = 0; else {
                  if (need2be) state_value|=MTRRF_ENABLED; else
                     state_value&=~MTRRF_ENABLED;
                  u32t errc = hlp_mtrrvset(idx,start,length,state_value);
                  if (errc) { printMTRRerr(errc); return EFAULT; }
               }
            }
            return 0;
         }
      } else
      if (al.Count()==4) {
         al[1].upper(); al[2].upper(); al[3].upper();
         if (al[0]=="FIXED") {
            int ct = getcachetype(al[3]());
            if (ct<0) rc = EINVAL; else {
               u32t errc = hlp_mtrrfset(al[1].Dword(),al[2].Dword(),ct);
               if (errc) { printMTRRerr(errc); return EFAULT; }
               return 0;
            }
         }
      } else
      if (al.Count()==5) {
         al[1].upper(); al[2].upper(); al[3].upper(); al[4].upper();
         if (al[0]=="SET") {
            int ct = getcachetype(al[4]());
            if (ct<0) rc = EINVAL; else {
               u32t errc = hlp_mtrrvset(al[1].Int()-1, al[2].Dword64(),
                              al[3].Dword64(), MTRRF_ENABLED|ct);
               if (errc) { printMTRRerr(errc); return EFAULT; }
               return 0;
            }
         }
      }
      if (rc<=0 && state_setup) {
         u32t errc = hlp_mtrrstate(state_value);
         if (errc) { printMTRRerr(errc); return EFAULT; }
         return 0;
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}
