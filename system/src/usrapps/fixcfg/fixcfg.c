/* unfortunately - main function can`t be in C++ file.
   wpp is too capricious to overload it name into _main */

#include <stdlib.h>

int process(const char *cmdline);

// command line arguments variable (in runtime code)
extern char * __stdcall _CmdArgs;

int main(int argc, char *argv[]) {
   if (argc<3) {
      printf("usage: fixcfg <config.sys> options ...\n"
             "where options:\n"
             "   -usb           - adjust # of UHCI/OHCI/EHCI drivers\n"
             "   -shell=os2path - set PROTSHELL variable\n");
      return 1;
   }
   return process(_CmdArgs);
}
