#include "qshm.h"
#include "pci.h"

// function called both from INI reader & "MODE SYS" handler
int shl_dbcard(const spstr &setup, printf_function pfn) {
   u32t  wcnt = setup.words(",");
   int     rc = -EINVAL;
   if (wcnt>1) {
      pci_location card;
      spstr w1(setup.word(1,","));

      if (w1.words(".")==3 && wcnt==2) {
         card.bus  = w1.word_Dword(1,".");
         card.slot = w1.word_Dword(2,".");
         card.func = w1.word_Dword(3,".");
         rc = hlp_pciexist(&card);
         if (!rc) {
            pfn("There is no device on %d.%d.%d!\n", card.bus, card.slot, card.func);
            rc = -ENODEV;
         }
      } else
      if (w1.words(":")==2 && wcnt<=3) {
         u32t ven = strtoul(w1.word(1,":")(),0,16),
              dev = strtoul(w1.word(2,":")(),0,16),
              idx = wcnt==3?setup.word_Dword(2,","):1;
         if (ven<=0xFFFF && dev<=0xFFFF && idx>=1) {
            rc = hlp_pcifind(ven, dev, idx-1, &card, 1);
            if (!rc) {
               pfn("Unable to find %04X:%04X %d!\n", ven, dev, idx);
               rc = -ENODEV;
            }
         } else {
            pfn("Invalid device ID/index value!\n");
            rc = 0;
         }
      }
      if (rc>0) {
         char locstr[32];
         snprintf(locstr, 32, "%d.%d.%d", card.bus, card.slot, card.func);

         if (card.classcode>>8!=PCI_CB_COMM) {
            pfn("%s is not comm.device (class code %04hX)!\n", locstr, card.classcode);
            rc = -ENOTTY;
         } else {
            u64t base[6], size[6];
            u32t acnt = hlp_pcigetbase(&card, base, size),
                ioidx = setup.word_Dword(wcnt,",");

            if (!ioidx || ioidx>acnt) {
               pfn("I/O address index is out of range for device at %s!\n", locstr);
               rc = -ENXIO;
            } else
            if ((base[ioidx-1]&PCI_ADDR_SPACE)==PCI_ADDR_SPACE_IO) {
               u32t port = base[ioidx-1]&PCI_ADDR_IO_MASK, rate,
                     now = hlp_seroutinfo(&rate);
               if (port) {
                  if (now && port!=now)
                     log_it(0, "warning! changing COM port from %04X to %04X\n", now, port);

                  u32t cmd = hlp_pciread(card.bus,card.slot,card.func,PCI_COMMAND,1),
                     flush = port!=now;
                  // enable i/o range
                  if ((cmd&PCI_CMD_IO)==0) {
                     hlp_pciwrite(card.bus,card.slot,card.func,PCI_COMMAND,1,
                        cmd|PCI_CMD_IO|PCI_CMD_MEMORY);
                     flush = 1;
                  }
                  // change/set new port
                  if (port!=now) hlp_seroutset(port,rate);
                  // flush all current log entries to this port if required
                  if (flush) {
                     TPtrStrings ft;
                     u32t        ii;
                     char       *cp = log_gettext(3);
                     ft.SetText(cp);
                     free(cp);
                     for (ii=0; ii<ft.Count(); ii++) {
                        hlp_seroutstr((char*)ft[ii]());
                        hlp_seroutstr("\n");
                     }
                  }
                  pfn("COM port %04hX at %s, rate %d!\n", port, locstr, rate);
               }
            } else {
               pfn("Address index %d on device %s is not I/O port!\n", ioidx, locstr);
               rc = -ENXIO;
            }

            if (!rc) pfn("Failed to setup device at %s!\n", locstr);
         }
      }
   }
   return rc<0?-rc:0;
}
