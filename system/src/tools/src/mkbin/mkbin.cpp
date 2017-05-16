#include "classes.hpp"
#include <stdio.h>
#include <stdarg.h>

int verbose = 0, warnlvl = 1;

void error(const char *fmt, ...);

void read_lx(const char *fname, const char *outname, int bss16, int pack, int hdr);

static void about(void) {
   printf(" mkbin v%X.%02X: special LX -> loadable binary converter\n\n"
          " mkbin [options] source_lx target_bin [options]\n"
          " options:\n"
          "   -v                be verbose\n"
          "   -bss16            store in file 16-bit object`s BSS\n"
          "   -pack             compress 32-bit part\n"
          "   -w=level          set warning level (0..2), 1 by default\n"
          "   -hdr              search for header in 16-bit object, rmStored field\n"
          "                     (16-bit BSS position) must be a valid value in it\n\n"
          " Module can contain 0-1 16-bit CODE/DATA object with REAL MODE code and\n"
          " 1-2 32-bit objects with PM code.\n\n"
          " Module must be a DLL, 32-bit entry point stored in binary for user tasks.\n"
          " User code responsible for unpacking and fixuping 32-bit part of binary.\n\n"
          " 16-bit object can use only offset fixups both to self and 32-bit objects.\n",
          APPVER>>8, APPVER&0xFF);
   exit(1);
}


int main(int argc, char *argv[]) {
   if (argc<3) about();
   TStrings args;
   int  ii, pack=0, bss16=0, hdr=0;

   for (ii=1; ii<argc; ii++) args.Add(argv[ii]);

   ii=args.IndexOfName("-v");
   if (ii>=0) { 
      // allow "-v=level" key 
      verbose = args.Value(ii).Dword();
      if (!verbose) verbose = 1; 
      args.Delete(ii); 
   }
   ii=args.IndexOfName("-w");
   if (ii>=0) { 
      warnlvl = args.Value(ii).Dword();
      if (!warnlvl) warnlvl = 1; 
      args.Delete(ii); 
   }
   ii=args.IndexOfName("-pack");
   if (ii>=0) { pack = 1; args.Delete(ii); }
   ii=args.IndexOfName("-hdr");
   if (ii>=0) { hdr = 1; args.Delete(ii); }
   ii=args.IndexOfName("-bss16");
   if (ii>=0) { bss16 = 1; args.Delete(ii); }

   if (args.Count()!=2) about();

   read_lx(args[0](), args[1](), bss16, pack, hdr);

   return 0;
}
