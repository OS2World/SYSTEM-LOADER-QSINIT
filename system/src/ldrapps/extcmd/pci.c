#include "qshm.h"
#include "qsshell.h"
#include "qslog.h"
#include "stdlib.h"
#include "errno.h"
#include "vioext.h"
#include "pci.h"

static const char *classname[PCI_CB_DSP+1] = {"Legacy", "Storage", "Network",
   "Display", "Multimedia", "Memory", "Bridge", "Comm", "Peripheral", "Input",
   "Docking", "Processor", "Sb", "Wireless", "Io", "Sat", "Encrypt", "Dsp"};

static void shl_printpci(int nopause) {
   pci_location dev;
   int           ok;
   cmd_printseq("PCI devices:", nopause?-1:0, 0);

   ok = hlp_pcigetnext(&dev,1,1);
   while (ok) {
      char   sbuf[128], *hmsg;
      u8t  gclass = dev.classcode>>8;
      snprintf(sbuf, 128, "_PCI%06X",(u32t)dev.classcode<<8|dev.progface);
      hmsg = cmd_shellgetmsg(sbuf);

      snprintf(sbuf, 128, "\t%d.%d.%d\t- %04X:%04X class %02X.%02X.%02X - %s",
         dev.bus, dev.slot, dev.func, dev.vendorid, dev.deviceid, gclass,
            dev.classcode&0xFF, dev.progface, hmsg?hmsg:(gclass>PCI_CB_DSP?
               "Unknown":classname[gclass]));
      if (hmsg) free(hmsg);
      // this function expand tabs
      if (cmd_printtext(sbuf,nopause?-1:0,0)) break;

      ok = hlp_pcigetnext(&dev,0,1);
   }
}

u32t _std shl_pci(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, tolog=0;
   if (args->count>0) {
      static char *argstr   = "/np|/log";
      static short argval[] = {  1,   2};
      char            *arg0 = 0;
      // help overrides any other keys
      if (str_findkey(args, "/?", 0)) {
         cmd_shellhelp(cmd,CLR_HELP);
         return 0;
      }
      args = str_parseargs(args, 0, SPA_RETLIST, argstr, argval, &nopause, &tolog);

      // process command
      if (args->count>0) {
         strupr(arg0 = args->item[0]);
         cmd_printseq(0, 1+tolog, 0);
      }
      if (args->count==1) {
         if (strcmp(arg0,"VIEW")==0) {
            shl_printpci(nopause);
            rc = 0;
         }
      } else
      if (args->count>0) {
         pci_location dev;
         int success = 0, batch = 0;
         char  *arg1 = args->item[1];
         strupr(arg1);

         if (strcmp(arg1,"ALL")==0) batch = 1; else
         if (strccnt(arg1,':')==1) {  // ven:dev
            str_list *dstr = str_split(arg1,":");
            u32t     venid = strtoul(dstr->item[0],0,16),
                     devid = strtoul(dstr->item[1],0,16);
            int        idx = arg0[0]=='W'?5:2;  // index of "index" parameter
            free(dstr);

            if (args->count-1==idx && isdigit(args->item[idx][0])) {
               idx = str2long(args->item[idx]);
               // remove optional to get constant number of parameters
               args->count--;
            } else idx = 0;

            if (idx>=0)
               if (hlp_pcifind(venid, devid, idx, &dev, 1)) success = 1; else
                  rc = ENODEV;
         } else
         if (strccnt(arg1,'.')==2) {  // bus.slot.func
            str_list *dstr = str_split(arg1,".");
            dev.bus  = str2ulong(dstr->item[0]);
            dev.slot = str2ulong(dstr->item[1]);
            dev.func = str2ulong(dstr->item[2]);
            free(dstr);

            if (dev.func>=8 || !hlp_pciexist(&dev)) rc = ENODEV; else
               success = 1;
         }

         if (args->count==2 && (strcmp(arg0,"VIEW")==0 || strcmp(arg0,"DUMP")==0)) {
            // enable ansi for this scope
            u32t ast_save = vio_setansi(1);

            if (batch) success = hlp_pcigetnext(&dev,1,1);

            while (success) {
               static const char *hdrname[3] = {"normal device", "bridge", "cardbus"};
               char    sbuf[128], *hmsg;
               u8t   gclass = dev.classcode>>8,
                       hdrt = dev.header&0x7F;
               u32t  ubreak;
               u16t  devcmd;

               rc = EINTR;
               if (cmd_printseq(" Bus %d Slot %d Function %d", nopause?-1:0,
                  VIO_COLOR_LWHITE, dev.bus, dev.slot, dev.func)) break;

               if (cmd_printseq(" Vendor %04X:%04X, type %d (%s)", nopause?-1:0, 0,
                  dev.vendorid, dev.deviceid, hdrt, hdrt>2?"invalid":hdrname[hdrt]))
                     break;

               snprintf(sbuf, 128, "_PCI%06X",(u32t)dev.classcode<<8|dev.progface);
               hmsg   = cmd_shellgetmsg(sbuf);
               ubreak = cmd_printseq(" Class %02X.%02X.%02X : %s", nopause?-1:0, 0,
                  gclass, dev.classcode&0xFF, dev.progface, hmsg?hmsg:
                     (gclass>PCI_CB_DSP?"Unknown":classname[gclass]));
               if (hmsg) free(hmsg);
               if (ubreak) break;

               if (hdrt==0) {
                  u32t devsub = hlp_pciread(dev.bus, dev.slot, dev.func,
                                            PCI_SUBSYSTEM_VENDOR_ID, 4);
                  if (cmd_printseq(" Subsystem Vendor %04X, Subsystem ID %04X",
                     nopause?-1:0, 0, devsub&0xFFFF, devsub>>16)) break;
               }

               devcmd = hlp_pciread(dev.bus, dev.slot, dev.func, PCI_COMMAND, 2);
               // use dyn buffer for to be safe
               hmsg   = sprintf_dyn(" Command %04hX (", devcmd);
               // no i/o and memory access - warn it!
               if ((devcmd&(PCI_CMD_IO|PCI_CMD_MEMORY))==0)
                  hmsg = strcat_dyn(hmsg, ANSI_LRED "Bus Access Disabled" ANSI_RESET);

               if (devcmd) {
                  int  dcii;
                  for (dcii=0; dcii<16; dcii++) {
                     if ((devcmd & 1<<dcii)) {
                        char keyname[32], *cname;
                        snprintf(keyname, 32, "_PCICMD_%04X", 1<<dcii);
                        cname = cmd_shellgetmsg(keyname);
                        if (cname) {
                           if (hmsg[strlen(hmsg)-1]!='(')
                              hmsg = strcat_dyn(hmsg, ", ");
                           hmsg = strcat_dyn(hmsg, cname);
                           free(cname);
                        }
                     }
                  }
               }
               hmsg   = strcat_dyn(hmsg, ")");
               ubreak = cmd_printseq(hmsg, nopause?-1:0, 0);
               free(hmsg); hmsg=0;
               if (ubreak) break;

               if (arg0[0]=='D') {
                  u32t  buf[4];
                  int    ii;
                  for (ii=0; ii<64; ii++) {
                     buf[ii&3] = hlp_pciread(dev.bus, dev.slot, dev.func, ii<<2, 4);
                     if ((ii&3)==3)
                        if (cmd_printseq("%03X: %16b", nopause?-1:0, 0,
                           (ii&~3)<<2, &buf)) break;
                  }
                  // user break in line above, return EINTR here
                  if (ii<64) break;
               } else {
                  u64t bases[6], sizes[6];
                  u32t bcnt = hlp_pcigetbase(&dev, bases, sizes), ii;
                  if (bcnt>0) {
                     if (cmd_printseq(" Addresses:", 0, 0)) break;

                     for (ii=0; ii<bcnt; ii++) {
                        if ((bases[ii]&PCI_ADDR_SPACE)==PCI_ADDR_SPACE_IO) {
                           u32t start = bases[ii]&PCI_ADDR_IO_MASK;
                           snprintf(sbuf, 128, "  %d. Port: %04X-%04X (%d)", ii+1,
                              start, start+(u32t)sizes[ii]-1,(u32t)sizes[ii]);
                        } else {
                           u64t start = bases[ii]&~((u64t)0xF);
                           snprintf(sbuf, 128, "  %d. Memory: %010LX-%010LX (%u kb)%s%s",
                              ii+1, start, start+sizes[ii]-1, (u32t)(sizes[ii]>>10),
                                 bases[ii]&PCI_ADDR_MEM_PREFETCH?", Prefetch":"",
                                    (bases[ii]&PCI_ADDR_MEMTYPE_MASK)==PCI_ADDR_MEMTYPE_64?
                                       ", 64-bit":"");
                        }
                        if (cmd_printseq(sbuf, nopause?-1:0, 0)) break;
                     }
                     // user break in line above, return EINTR here
                     if (ii<bcnt) break;
                  }
                  // interrupt #
                  ii = hlp_pciread(dev.bus,dev.slot,dev.func,PCI_INTERRUPT_LINE,2);
                  if (ii>>8)
                     if (cmd_printseq(" Interrupt %d, INT #%c", nopause?-1:0, 0,
                        ii&0xFF, '@'+(ii>>8))) break;
               }
               // next device?
               success = batch ? hlp_pcigetnext(&dev,0,1) : 0;
               // set no error
               rc = 0;
            }
            vio_setansi(ast_save);
         } else
         if (args->count==5 && strcmp(arg0,"WRITE")==0) {
            if (success) {
               u32t offs = str2ulong(args->item[2]),
                     val = str2ulong(args->item[3]),
                      sz = str2ulong(args->item[4]);

               if (isdigit(args->item[2][0]) && isdigit(args->item[3][0]) &&
                  isdigit(args->item[4][0]) && (sz==1 || sz==2 || sz==4))
               {
                  hlp_pciwrite(dev.bus, dev.slot, dev.func, offs, sz, val);
                  rc = 0;
               }
            }
         }
      }
      // free args clone, returned by str_parseargs() above
      free(args);
   }
   if (rc<0) {
      rc = EINVAL;
      if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
   }
   return rc;
}

u32t _std shl_log(const char *cmd, str_list *args) {
   int rc=-1, nopause=0, usedate=0, usetime=1, usecolor=1, level=-1,
       useflags=0, ncol=1, openend=0;

   if (args->count>0) {
      static char *argstr   = "f|d|np|nt|nc|l|l:0|l:1|l:2|l:3|2|e";
      static short argval[] = {1,1, 1, 0, 0,3,  0,  1,  2,  3,2,1};
      // help overrides any other keys
      if (str_findkey(args, "/?", 0)) {
         cmd_shellhelp(cmd,CLR_HELP);
         return 0;
      }
      args = str_parseargs(args, 0, SPA_RETLIST|SPA_NOSLASH, argstr, argval,
                   &useflags, &usedate, &nopause, &usetime, &usecolor,
                   &level, &level, &level, &level, &level, &ncol, &openend);
      // process command
      if (args->count>0 && level>=-1 && level<=3) {
         char *arg0 = args->item[0];
         strupr(arg0);
         // init "paused" output
         cmd_printseq(0, 1, 0);

         if (strcmp(arg0,"SAVE")==0 || strcmp(arg0,"TYPE")==0 || strcmp(arg0,"LIST")==0) {
            int is_save = arg0[0]=='S';

            if (!is_save || args->count==2) {
               u32t flags = LOGTF_DOSTEXT;
               char   *cp;

               if (usedate) flags|=LOGTF_DATE;
               if (usetime) flags|=LOGTF_TIME;
               if (useflags) flags|=LOGTF_FLAGS;
               if (level>=0) flags|=LOGTF_LEVEL|level; else
               if (usecolor) flags|=LOGTF_LEVEL|LOG_GARBAGE;

               cp = log_gettext(flags);

               if (is_save) {
                  FILE *lf = fopen(args->item[1], "wb");
                  if (!lf) rc = errno; else {
                     u32t len = strlen(cp);
                     if (fwrite(cp,1,len,lf)!=len) rc = EIO; else rc = 0;
                     if (fclose(lf) && !rc) rc = errno;
                  }
                  free(cp);
               } else {
                  u32t      ii;
                  str_list *ft = str_settext(cp, 0, 0);
                  free(cp);

                  for (ii=0; ii<ft->count; ii++) {
                     char    *cs = ft->item[ii];
                     u8t   color = 0;

                     if (usecolor) {
                        char *cp = strchr(cs,'[');
                        if (cp && cp-cs<19) {
                           char *lp = cp;
                           while (!isdigit(*lp) && *lp) lp++;
                           switch (*lp) {
                              case '0': color = VIO_COLOR_YELLOW; break;
                              case '1': color = VIO_COLOR_LWHITE; break;
                              case '2': color = VIO_COLOR_WHITE; break;
                              case '3': color = VIO_COLOR_CYAN; break;
                           }
                           if (level<0) {
                              char *ep = strchr(cp,']');
                              if (ep && ep-cp<=4) memmove(cp,ep+1,strlen(ep));
                           }
                        }
                     }
                     if (cmd_printseq(cs, nopause?-1:0, color)) { rc = EINTR; break; }
                  }
                  free(ft);
                  if (rc<0) rc = 0;
               }
            }
         } else
         if (strcmp(arg0,"CLEAR")==0) {
            log_clear();
            rc = 0;
         }
      }
      // free args clone, returned by str_parseargs() above
      free(args);
   }
   if (rc<0) rc = EINVAL;
   if (rc && rc!=EINTR) cmd_shellerr(EMSG_CLIB,rc,0);
   return 0;
}

