#include "qshm.h"
#include "pci.h"

static const char *classname[PCI_CB_DSP+1] = {"Legacy", "Storage", "Network",
   "Display", "Multimedia", "Memory", "Bridge", "Comm", "Peripheral", "Input",
   "Docking", "Processor", "Sb", "Wireless", "Io", "Sat", "Encrypt", "Dsp"};


void shl_printpci(int nopause) {
   spstr ln;
   ln.sprintf("PCI devices:");
   pause_println(ln(), nopause?-1:0);

   pci_location dev;
   int ok = hlp_pcigetnext(&dev,1,1);
   while (ok) {
      u8t gclass = dev.classcode>>8;
      ln.sprintf("_PCI%06X",(u32t)dev.classcode<<8|dev.progface);
      char *hmsg = cmd_shellgetmsg(ln());
      ln.sprintf("\t%d.%d.%d\t- %04X:%04X class %02X.%02X.%02X - %s",
         dev.bus, dev.slot, dev.func, dev.vendorid, dev.deviceid, gclass,
            dev.classcode&0xFF, dev.progface, hmsg?hmsg:(gclass>PCI_CB_DSP?
               "Uncknown":classname[gclass]));
      if (hmsg) free(hmsg);
      // this function expand tabs
      if (cmd_printtext(ln(),!nopause,0,0)) break;

      ok = hlp_pcigetnext(&dev,0,1);
   }
}

u32t _std shl_pci(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, tolog=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd,CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/np|/log";
      static short argval[] = {  1,   2};
      process_args(al, argstr, argval, &nopause, &tolog);
      // process command
      al.TrimEmptyLines();
      al[0].upper();
      pause_println(0,1+tolog);

      if (al.Count()==1) {
         if (al[0]=="VIEW") {
            shl_printpci(nopause);
            rc = 0;
         }
      } else {
         pci_location dev;
         int success = 0, batch = 0;

         al[1].upper();

         if (al[1]=="ALL") batch = 1; else
         if (al[1].words(":")==2) {
            u32t venid = strtoul(al[1].word(1,":")(),0,16),
                 devid = strtoul(al[1].word(2,":")(),0,16);
            // index of "index" parameter
            int idx = al[0][0]=='W'?5:2;

            if (al.Max()==idx && isdigit(al[idx][0])) {
               idx=al[idx].Int();
               // remove optional to get constant number of parameters
               al.Delete(al.Max());
            } else idx=0;

            if (idx>=0)
               if (hlp_pcifind(venid, devid, idx, &dev, 1)) success = 1; else
                  rc = ENODEV;
         } else
         if (al[1].words(".")==3) {
            dev.bus  = al[1].word_Dword(1,".");
            dev.slot = al[1].word_Dword(2,".");
            dev.func = al[1].word_Dword(3,".");
            if (dev.func>=8 || !hlp_pciexist(&dev)) rc = ENODEV; else
               success = 1;
         }

         if (al.Count()==2 && (al[0]=="VIEW" || al[0]=="DUMP")) {
            if (batch) success = hlp_pcigetnext(&dev,1,1);

            while (success) {
               static const char *hdrname[3] = {"normal device", "bridge", "cardbus"};
               u8t gclass = dev.classcode>>8,
                     hdrt = dev.header&0x7F;
               spstr   ln;
               ln.sprintf(" Bus %d Slot %d Function %d", dev.bus, dev.slot, dev.func);
               if (pause_println(ln(), nopause?-1:0, VIO_COLOR_LWHITE)) return EINTR;

               ln.sprintf(" Vendor %04X:%04X, type %d (%s)",
                  dev.vendorid, dev.deviceid, hdrt, hdrt>2?"invalid":hdrname[hdrt]);
               if (pause_println(ln(), nopause?-1:0)) return EINTR;

               ln.sprintf("_PCI%06X",(u32t)dev.classcode<<8|dev.progface);
               char *hmsg = cmd_shellgetmsg(ln());
               ln.sprintf(" Class %02X.%02X.%02X : %s", gclass, dev.classcode&0xFF,
                  dev.progface, hmsg?hmsg:(gclass>PCI_CB_DSP?"Uncknown":classname[gclass]));
               if (hmsg) free(hmsg);
               if (pause_println(ln(), nopause?-1:0)) return EINTR;

               if (al[0][0]=='D') {
                  u32t  buf[4];
                  for (int ii=0; ii<64; ii++) {
                     buf[ii&3] = hlp_pciread(dev.bus, dev.slot, dev.func, ii<<2, 4);
                     if ((ii&3)==3) {
                        ln.sprintf("%03X: %16b", (ii&~3)<<2, &buf);
                        if (pause_println(ln(), nopause?-1:0)) return EINTR;
                     }
                  }
               } else {
                  u64t bases[6], sizes[6];
                  u32t bcnt = hlp_pcigetbase(&dev, bases, sizes), ii;
                  if (bcnt>0) {
                     if (pause_println(" Addresses:")) return EINTR;

                     for (ii=0; ii<bcnt; ii++) {
                        if ((bases[ii]&PCI_ADDR_SPACE)==PCI_ADDR_SPACE_IO) {
                           u32t start = bases[ii]&PCI_ADDR_IO_MASK;
                           ln.sprintf("  %d. Port: %04X-%04X (%d)",ii+1, start,
                              start+(u32t)sizes[ii]-1,(u32t)sizes[ii]);
                        } else {
                           u64t start = bases[ii]&~((u64t)0xF);
                           ln.sprintf("  %d. Memory: %010LX-%010LX (%u kb)%s%s",ii+1,
                              start, start+sizes[ii]-1, (u32t)(sizes[ii]>>10),
                                 bases[ii]&PCI_ADDR_MEM_PREFETCH?", Prefetch":"",
                                    (bases[ii]&PCI_ADDR_MEMTYPE_MASK)==PCI_ADDR_MEMTYPE_64?
                                       ", 64-bit":"");
                        }
                        if (pause_println(ln(), nopause?-1:0)) return EINTR;
                     }
                  }
                  // interrupt #
                  ii = hlp_pciread(dev.bus,dev.slot,dev.func,PCI_INTERRUPT_LINE,2);
                  if (ii>>8) {
                     ln.sprintf(" Interrupt %d, INT #%c",ii&0xFF,'@'+(ii>>8));
                     if (pause_println(ln(), nopause?-1:0)) return EINTR;
                  }
               }
               // next device?
               success = batch ? hlp_pcigetnext(&dev,0,1) : 0;
               // set no error
               rc = 0;
            }
         } else
         if (al.Count()==5 && al[0]=="WRITE") {
            if (success) {
               u32t offs = al[2].Dword(),
                     val = al[3].Dword(),
                      sz = al[4].Dword();

               if (isdigit(al[2][0]) && isdigit(al[3][0])&& isdigit(al[4][0]) &&
                  (sz==1 || sz==2 || sz==4))
               {
                  hlp_pciwrite(dev.bus,dev.slot,dev.func,offs,sz,val);
                  rc = 0;
               }
            }
         }
      }
   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(rc,0);
   return rc;
}
