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

u32t _std shl_sm(const char *cmd, str_list *args) {
   int  rc=-1, quiet=0, nopause=0;
   if (args->count>0) {
      TPtrStrings al;
      str_getstrs(args,al);
      // is help?
      int idx = al.IndexOf("/?");
      if (idx>=0) { cmd_shellhelp(cmd, CLR_HELP); return 0; }
      // process args
      static char *argstr   = "/q|/np";
      static short argval[] = { 1,  1};
      process_args(al, argstr, argval, &quiet, &nopause);
      al.TrimEmptyLines();
      if (al.Count()) {
         u32t devlist = 0;
         // search for /d:list & /q and remove it
         for (idx=0; idx<al.Count(); idx++)
            if (al[idx](0,3).upper()=="/D:") {
               int  len = al[idx].length()-3;
               u64t val;
               if (!len || len>12) {
                  rc = len?EOVERFLOW:EINVAL;
                  break;
               }
               val = strtoull(al[idx]()+3, 0, 32);
               while (len--) {
                  devlist |= 1<<(val&0x1F);
                  val    >>= 5;
               }
               al.Delete(idx);
            }

         if (rc<0 && al.Count()) {
            qserr res = 0;
            u32t argc = al.Count();
            rc  = 0;
            pause_println(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT);

            if (al[0].upper()=="VADD" && (argc==3 || argc==4)) {
               u32t dev_id = 0;
               res = se_deviceadd(al[1](), &dev_id, al[2]());
               if (!res && !quiet) printf("Device added with index %u\n", dev_id);
               if (argc==4) {
                  char value[16];
                  if (!res) ultoa(dev_id, value, 10);
                  setenv(al[3](), res?0:value, 1);
               }
            } else
            if (al[0]=="VDEL" && argc==2) {
               u32t dev_id = al[1].Dword();

               if (!dev_id) printf("Device 0 cannot be deleted\n"); else {
                  res = se_devicedel(dev_id);
                  if (!res && !quiet) printf("Device %u deleted\n", dev_id);
               }
            } else
            if (al[0]=="VLIST" && argc==1) {
               se_stats* dl = se_deviceenum();

               cmd_printseq("Device list:", 0, VIO_COLOR_LWHITE);

               for (idx=0; idx<dl->handlers; idx++) {
                  char    sesno[64];
                  u32t          cnt;
                  vio_mode_info *mi = vio_modeinfo(0, 0, dl->mhi[idx].dev_id), *mp = mi;
                  for (cnt=0; mp->size; mp++) cnt++;
                  free(mi);

                  if (dl->mhi[idx].reserved)
                     snprintf(sesno, 64, "active session %u", dl->mhi[idx].reserved);
                  else
                     strcpy(sesno, "no active session");

                  cmd_printf("%2u. %-10s -%3u modes, %s\n", dl->mhi[idx].dev_id,
                     dl->mhi[idx].name, cnt, sesno);
               }
               free(dl);
            } else
            if ((al[0]=="ATTACH" || al[0]=="DETACH") && argc>2) {
               u32t sno = al[1].Dword();

               for (idx=2; idx<al.Count(); idx++) {
                  u32t dev = al[idx].Dword();
                  if ((res = al[0][0]=='A'?se_attach(sno,dev):se_detach(sno,dev)))
                     break;
               }
            } else
            if (al[0]=="FORE" && argc==2) {
               u32t sno = al[1].Dword();
               res = se_switch(sno, devlist?devlist:FFFF);
            } else
            if (al[0]=="LIST" && argc==1) {
               ansi_push_set ansion(1);
               u32t          seslim = se_curalloc(),
                              sesno = se_sesno();
               int          ioredir = !isatty(fileno(get_stdout()));
               cmd_printseq("Session list:", 0, VIO_COLOR_LWHITE);
               // dumb enumeration (but it works for hidden sessions too!)
               for (idx=1; idx<seslim; idx++) {
                  se_stats *sd = se_stat(idx);
                  // show hidden ONLY when it in the foreground
                  if (sd && ((sd->flags&(VSF_HIDDEN|VSF_FOREGROUND))!=VSF_HIDDEN)) {
                     spstr dstr, astr;
                     for (u32t di=0; di<sd->handlers; di++) {
                        char *cb = sprintf_dyn("%u,", sd->mhi[di].dev_id);
                        astr+=cb;
                        free(cb);
                     }
                     for (u32t di=0; di<32; di++)
                        if (sd->devmask & 1<<di) {
                           char *cb = sprintf_dyn("%u,", di);
                           dstr+=cb;
                           free(cb);
                        }
                     if (!astr) astr="no"; else
                        { astr[astr.length()-1] = ']'; astr.insert("[",0); }
                     if (!dstr) dstr="no"; else
                        { dstr[dstr.length()-1] = ']'; dstr.insert("[",0); }

                     cmd_printf("%s%3u. %s\n     %ux%u  devices: %s  active on: %s%s\n",
                        sesno==idx && !ioredir?ANSI_WHITE:"", idx, sd->title?
                           sd->title:"no title", sd->mx, sd->my, dstr(), astr(),
                              ioredir?"":ANSI_RESET);
                     free(sd);
                  }
               }
            } else
            if (al[0]=="WHO" && argc==1) {
               u32t sno = se_sesno();
               if (!quiet) printf("Current session number is %u\n", sno);
               set_errorlevel(sno);
               return sno;
            } else
            if (al[0]=="TITLE" && argc==3) {
               u32t sno = al[1].Dword();
               res = se_settitle(sno, al[2]());
            } else
               rc = EINVAL;

            if (res) cmd_shellerr(EMSG_QS, res, 0);
         }
      }

   }
   if (rc<0) rc = EINVAL;
   if (rc) cmd_shellerr(EMSG_CLIB, rc, 0);
   return 0;
}
