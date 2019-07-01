//
//
//
//
#include "classes.hpp"
#include <stdlib.h>
#include "qshm.h"
#include "qslog.h"

const char *BASEDEV = "BASEDEV",
              *UHCD = "USBUHCD.SYS",
              *OHCD = "USBOHCD.SYS",
              *EHCD = "USBEHCD.SYS",
              *XHCD = "USBXHCD.SYS";

int finddev(TStrings &cfg, const char *key, const char *name, int startfrom = 0) {
   int ii;
   spstr Key(key), Drv(name);
   Key.upper();
   Drv.upper();

   for (ii=startfrom; ii<cfg.Count(); ii++)
      if (cfg.Name(ii).trim().upper()==Key) {
         spstr av = cfg.Value(ii).trim().upper();
         int  pos = av.pos(Drv);
         // should the 1st word
         if (pos>=0 && (av.words()==1 || pos<av.wordpos(2))) return ii;
      }
   return -1;
}

void wipedev(TStrings &cfg, const char *key, const char *name, int all, int startfrom = 0) {
   int pos = startfrom;
   while (pos>=0) {
      pos = finddev(cfg, key, name, pos);
      if (pos>=0) {
         cfg.Delete(pos);
         if (!all) break; 
      }
   } 
}

/** find a string in form "SET name = ...."
    @param  cfg        config.sys text
    @param  name       set variable name
    @param  nodupes    wipe all duplications of this variable
    @param  startfrom  starting line to search from
    @return line number or -1 */
l index_of_set(TStrings &cfg, const char *name, int nodupes = 1, int startfrom = 0) {
   int  rc = -1, ii;
   spstr SetN(name);
   SetN.upper();

   for (ii=startfrom; ii<cfg.Count(); ) {
      spstr key = cfg.Name(ii).trim().upper();
      if (key.word(1)=="SET" && key.word(2)==SetN)
         if (rc>=0) { cfg.Delete(ii); continue; } else {
            rc = ii; 
            if (nodupes==0) break;
         }
      ii++;
   }
   return rc;
}

extern "C"
int process(const char *cmdline) {
   TStrings args, cfg;
   args.ParseCmdLine(cmdline);

   spstr fname = args[0];
   args.Delete(0);

   if (!cfg.LoadFromFile(fname())) {
      printf("unable to load \"%s\"\n", fname());
      return 2;
   }

   l shell = args.IndexOfName("-shell"),
     video = args.IndexOfName("-video"),
      verb = args.IndexOfICase("-verbose")>=0 || args.IndexOfICase("-v")>=0;
   /* just rem, but not delete original PROTSHELL variable */
   if (shell>=0) {
      l ps = cfg.IndexOfName("PROTSHELL");
      if (ps>=0) cfg[ps].insert("rem ", 0);

      spstr psname = args.Value(shell);
      psname.insert("PROTSHELL=",0);
      cfg.Insert(0,psname);
   }

   if (video>=0) {
      spstr dstr = "SET VIO_SVGA=DEVICE(",
            vstr = "SET VIDEO_DEVICES=VIO_SVGA",
            mstr = args.Value(video);
      if (mstr.length()) {
         l    ps = index_of_set(cfg, "VIDEO_DEVICES"),
             vps = index_of_set(cfg, "VIO_SVGA");
         dstr+=mstr;
         dstr+=")";
         if (vps>=0) cfg[vps] = dstr; else cfg.Add(dstr);
         if (ps >=0) cfg[ps]  = vstr; else cfg.Add(vstr);
      }
   }

   if (args.IndexOfICase("-usb")>=0 || args.IndexOfICase("-usb3")>=0) {
      u16t uc=0, oc=0, ec=0, xc=0, index=0;
      pci_location  devinfo;
      // enum class/subclass
      while (hlp_pciclass(0x0C03, index++, &devinfo, 0)) {
         switch (devinfo.progface) {
            case 0x00: uc++; break;
            case 0x10: oc++; break;
            case 0x20: ec++; break;
            case 0x30: xc++; break;
         }
      }
      // xHCI just to display
      log_printf("fixcfg: %u UHCI, %u OHCI, %u EHCI, %u xHCI adapters\n",
         uc, oc, ec, xc);

      int ucpos = finddev(cfg, BASEDEV, UHCD),
          ocpos = finddev(cfg, BASEDEV, OHCD),
          ecpos = finddev(cfg, BASEDEV, EHCD), 
          xcpos = finddev(cfg, BASEDEV, XHCD), ii;

      wipedev(cfg, BASEDEV, UHCD, 1);
      wipedev(cfg, BASEDEV, OHCD, 1);
      wipedev(cfg, BASEDEV, EHCD, 1);
      wipedev(cfg, BASEDEV, XHCD, 1);

      if (ocpos>=0 && ocpos<ucpos) ucpos = ocpos;
      if (ecpos>=0 && ecpos<ucpos) ucpos = ecpos;
      if (xcpos>=0 && xcpos<ucpos) ucpos = xcpos;
      if (ucpos<0) ucpos = 0;

      spstr dn;
      if (uc) {
         dn.sprintf("%s=%s%s", BASEDEV, UHCD, verb?" /V":"");
         for (ii=0; ii<uc; ii++, ucpos++) cfg.Insert(ucpos,dn);
      }
      if (oc) {
         dn.sprintf("%s=%s%s", BASEDEV, OHCD, verb?" /V":"");
         for (ii=0; ii<oc; ii++, ucpos++) cfg.Insert(ucpos,dn);
      }
      if (ec) {
         dn.sprintf("%s=%s%s", BASEDEV, EHCD, verb?" /V":"");
         for (ii=0; ii<ec; ii++, ucpos++) cfg.Insert(ucpos,dn);
      }
      if (xc && args.IndexOfICase("-usb3")>=0) {
         dn.sprintf("%s=%s%s", BASEDEV, XHCD, verb?" /V":"");
         for (ii=0; ii<xc; ii++, ucpos++) cfg.Insert(ucpos,dn);
      }
   }
   
   if (!cfg.SaveToFile(fname())) {
      printf("unable to save \"%s\"\n", fname());
      return 3;
   }
   return 0;
}
