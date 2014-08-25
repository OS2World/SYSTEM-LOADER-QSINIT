//
// QSINIT "boot menu" module
// kernel start options merging
// -----------------------------
// moved here from bootmenu to save 20k of C++ garbage in separate app
//
#include "qsshell.h"
#include "qsutil.h"
#include "qs_rt.h"
#include "classes.hpp"

// known options
static const char *keys[] = { "ALTE", "ALTF1", "ALTF2", "ALTF3", "ALTF4",
   "ALTF5", "CTRLC", "PRELOAD", "MEMLIMIT", "NODBCS", "LOGSIZE", "NOREV",
   "NOLOGO", "CFGEXT", "SYM", "RESTART", "DBPORT" };

enum _keys { kAltE, kAltFX, kAltFXe=5, kCtrlC, kPreload, kLimit, kDbcs,
   kLogSize, kNoRev, kNoLogo, kCfgExt, kSymName, kRestart, kDbPort, kEnd };

// options to remove from result line
static const char *emptylist[] = { "DEFAULT", "TIMEOUT", "DISKSIZE", "UNZALL",
  "SHOWMEM", "TTYMENU", "MENUPALETTE", "RESETMODE", "DISKSIZE", "USEBEEP", 
  "HEAPFLAGS", "DEFAULT_PARTITION", "DBFLAGS", "SHAREIRQ", 0 };

extern "C"
int _stdcall cmd_mergeopts(char *line, char *args, char *ininame) {
   TStrings opts, comkeys, argsl;
   opts.SplitString(line,",");
   argsl.SplitString(args,",");
   opts.TrimEmptyLines();
   opts.TrimAllLines();
   // delete kernel`s menu name
   argsl.Delete(0);
   argsl.TrimAllLines();

   // query list keys in "config" section
   str_list* sl=str_getsec(ininame,"config",1);
   if (sl) {
      //log_printlist("config entries", sl);
      str_getstrs(sl, comkeys);
      free(sl);
   }

   int pass,kk;

   // merge keys
   for (pass=0;pass<2;pass++) {
      TStrings &src=pass?comkeys:argsl;

      for (kk=kAltE; kk<kEnd; kk++) {
         int pos;
         // key present?
         if ((pos=src.IndexOfName(keys[kk]))>=0) {
            if (kk<=kAltFXe) {
               if (opts.IndexOfName("PKEY")<0) {
                  spstr str;
                  str.sprintf("PKEY=0x%04X",kk==kAltE?0x1200:0x6700+(kk<<8));
                  opts.Add(str);
               } else continue;
            } else
            if (opts.IndexOfName(keys[kk])<0) opts.Add(src[pos]);
            // delete all entries of this key
            while (pos>=0) {
               src.Delete(pos);
               pos=src.IndexOfName(keys[kk]);
            }
         }
      }
      // merge all uncknown options to command line string
      opts.AddStrings(opts.Count(),src);
   }
   // delete dbport key if negative value specified
   if (opts.IntValue(keys[kDbPort])<0) opts.Delete(opts.IndexOfName(keys[kDbPort]));

   // remove menu/misc keys from line
   pass = 0;
   while(emptylist[pass]) {
      do {
         kk = opts.IndexOfName(emptylist[pass]);
         if (kk>=0) opts.Delete(kk);
      } while (kk>=0);
      pass++;
   }
   // run line
   spstr rl(opts.GetTextToStr(","));
   //log_printf("[%s]\n", rl());
   if (rl.lastchar()==',') rl.dellast();

   strcpy(line,rl());
   return opts.IndexOfName(keys[kRestart])>=0?-1:1;
}