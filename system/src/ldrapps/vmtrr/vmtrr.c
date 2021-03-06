//
// QSINIT "vmtrr" module
//
#include "stdlib.h"
#include "qsbase.h"
#include "stdarg.h"
#include "memmgr.h"
#include "opt.h"
#include "console.h"
#include "pci.h"
#include "qsint.h"
#include "qsmodext.h"

void error(int errorcode, const char *fmt, ...) {
   va_list arglist;
   va_start(arglist,fmt);
   vprintf(fmt,arglist);
   va_end(arglist);
   if (mtrr) { free(mtrr); mtrr=0; }
   exit(errorcode);
}

static void printmtrr(void) {
   static const char *mtstr[] = { "uncacheable", "write combining", "???",
      "???", "write-through", "write-protected", "writeback", "???"};
   char buffer[128];
   int     idx;

   for (idx=0; idx<regs; idx++) {
      u64t  start = mtrr[idx].start,
           length = mtrr[idx].len;
      u32t rstate = mtrr[idx].on;
      
      snprintf(buffer, 128, "%2d. %010LX-%010LX, size: %8u kb - %s, %s", idx,
         start, length?start+length-1:start, (u32t)(length>>10),
            rstate?" on":"off", mtstr[mtrr[idx].cache]);
      log_it(2, "VMTRR: %s\n", buffer);
   }
}

int get_videomem(u64t *wc_addr, u64t *wc_len, pci_location *dev, int method) {
   u32t       modes = 0, bacount, ii, 
              vmlen = 0, vmidx = 0;
   u16t       index = 0;
   u64t      vmaddr = 0,
              bases[6],
              sizes[6];
   if (!wc_addr || !wc_len) return 0;
   *wc_addr = 0;
   *wc_len  = 0;
   vmlen = con_vmemsize(&vmaddr);
   if (!vmaddr || !vmlen) {
      log_it(2,"VMTRR: Unable to get video memory address (in 4th Gb)\n");
      return 0;
   }
   while (1) {
      switch (method) {
         case 1: // searching by class code
            if (!hlp_pciclass(PCI_CB_DISPLAY<<8|PCI_CS_VIDEO_VGA, index, dev, 1))
               return 0;
            index++;
            break;
         case 2: // searching by pci id
            if (!hlp_pcifind(dev->vendorid, dev->deviceid, index, dev, 1))
               return 0;
            index++;
         case 3: // searching by bus/slot
            if (index++) return 0; else
               if (!hlp_pciexist(dev)) return 0;
         default:
            return 0;
      }
      bacount = hlp_pcigetbase(dev, bases, sizes);
      if (bacount) {
         for (ii=0; ii<bacount; ii++)
            if ((bases[ii]&PCI_ADDR_SPACE)==PCI_ADDR_SPACE_MEMORY && bases[ii]<FFFF) {
                u32t va = bases[ii] & PCI_ADDR_MEM_MASK,
                     sz = sizes[ii];
                /** start addr from vesa can be incorrect, so check or addr or 
                    size match */
                if (va>=sys_endofram() && vmaddr>=va && (vmaddr+vmlen<=va+sz || 
                    vmaddr<=va+sz && vmlen==sz)) 
                {
                   int right = bsf64(sz);
                   if (1<<right==sz) {
                      int st_right = bsf64(va);
                      if (st_right>=right) {
                         *wc_addr = va;
                         *wc_len  = sz;
                         return 1;
                      }
                   }
                   log_it(2,"VMTRR: Address found, but it wrong aligned! See %d.%d.%d in PCI\n",
                      dev->bus, dev->slot, dev->func);
                   return 0;
                }
            }
      }
   } 
   return 0;
}

int main(int argc,char *argv[]) {
   char       modname[128];
   pci_location vcard;
   u32t   flags, state, ii,
          vregs = hlp_mtrrquery(&flags, &state, 0),
       memlimit = 0;
   u64t wc_addr, 
         wc_len;
   int    optrc,
         method = 0;

   if (argc>1) {
      if (argc==2 && (strchr(argv[1],':') || strchr(argv[1],'.'))) {
         method = hlp_pciatoloc(argv[1], &vcard);
         if (method==PCILOC_ERROR) {
            cmd_shellhelp("VMTRR",CLR_HELP);
            return 0;
         }
         method++;
      } else 
      if (argc==3 && isdigit(argv[1][0]) && isdigit(argv[2][0])) {
         wc_addr = strtoull(argv[1],0,0);
         wc_len  = strtoull(argv[2],0,0);
         if (!wc_addr) error(3, "Start address cannot be 0\n");
         if (!wc_len) error(3, "Length cannot be 0\n");
      } else {
         cmd_shellhelp("VMTRR",CLR_HELP);
         return 0;
      }
   } else
      method = 1;

   if (!vregs) error(1, "Unable to find MTRR on this PC\n");
   if ((flags&MTRRQ_WRCOMB)==0) error(1, "Write combining is not supported\n");
   if ((state&MTRRS_MTRRON)==0) 
      printf("Warning! MTRR is in disabled state!\nType MTRR ON to turn it back\n");
   ii = state&MTRRS_DEFMASK;
   if (ii!=MTRRF_UC && ii!=MTRRF_WB)
      error(6,"Default memory type is not UC nor WB. Unable to operate!\n");

   mtrr = (mtrrentry*)malloc(vregs*sizeof(mtrrentry));
   mem_zero(mtrr);

   for (ii=0; ii<vregs; ii++) {
      u32t rstate;
      if (!hlp_mtrrvread(ii, &mtrr[ii].start, &mtrr[ii].len, &rstate))
         error(2, "Unable to read MTRR register #%d\n", ii+1);
      mtrr[ii].on    = rstate&MTRRF_ENABLED?1:0;
      mtrr[ii].cache = rstate&MTRRF_TYPEMASK;
   }

   if (method)
      if (!get_videomem(&wc_addr, &wc_len, &vcard, method))
         error(5, "Unable to determine video memory address and size\n");
      else
         log_it(2,"VMTRR: memory at %08X..%08X\n", wc_addr, wc_addr+wc_len-1);

   regs  = vregs;

   printmtrr();

   optrc = mtrropt(wc_addr, wc_len, &memlimit, (state&MTRRS_DEFMASK)==MTRRF_WB);
   switch (optrc) {
      case OPTERR_VIDMEM3GB: error(4, "Video memory is not in 2..4Gb location\n");
      case OPTERR_UNKCT    : error(4, "One of WT/WP/WC cache types present\n");
      case OPTERR_INTERSECT: error(4, "Requested block intersects with others\n");
      case OPTERR_SPLIT4GB : error(4, "Error on splitting block on 4Gb border\n");
      case OPTERR_BELOWUC  : error(4, "Address is below selected UC border\n");
      case OPTERR_LOWUC    : error(4, "Too low large UC block\n");
      case OPTERR_NOREG    : error(4, "No free MTRR register\n");
      case OPTERR_OPTERR   : error(4, "Optimization error\n");
   }

   if ((state&MTRRS_MTRRON)!=0) hlp_mtrrstate(state&~MTRRS_MTRRON|MTRRS_NOCALBRT);

   for (ii=0; ii<vregs; ii++) {
      u32t rc = hlp_mtrrvset(ii, mtrr[ii].start, mtrr[ii].len,
                             mtrr[ii].cache|(mtrr[ii].on?MTRRF_ENABLED:0));
      if (rc) {
         hlp_mtrrbios();
         error(7, "Error %d while assigning register %d value, MTRR restored to initial state\n",
            rc, ii+1);
         printmtrr();
      }
   }
   if ((state&MTRRS_MTRRON)!=0) hlp_mtrrstate(state|MTRRS_NOCALBRT);

   // do not print success result if called from start.cmd ("QSINIT" module parent)
   if (mod_appname(modname,1) && stricmp(modname, MODNAME_QSINIT)) {
      printf("WC enabled for range %010LX..%010LX", wc_addr, wc_addr+wc_len-1);
      if (method) {
         printf(" (%04X:%04X on %d.%d.%d)\n", vcard.vendorid, vcard.deviceid,
            vcard.bus, vcard.slot, vcard.func);
      } else
         printf("\n");
   }
   /* save memlimit for OS/2 boot (setup can change WB to UC for a small part 
      of memory in 4th Gb - and we truncate access to this memory in OS/2 */
   if (memlimit) {
      u32t orglimit = sto_dword(STOKEY_MEMLIM);
      if (!orglimit || memlimit<orglimit) sto_savedword(STOKEY_MEMLIM, memlimit);
   }

   if (mtrr) { free(mtrr); mtrr=0; }
   return 0;
}
