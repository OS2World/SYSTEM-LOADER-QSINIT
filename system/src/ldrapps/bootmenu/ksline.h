//
// QSINIT "boot menu" module
// internal header
//
#ifndef QSINIT_MENUOPTS
#define QSINIT_MENUOPTS

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** merge options from menu, config line and common section of os2ldr.ini.
    Function moved to "start" module to save 20k c++ runtime.

    @param line [in,out]  comma separated menu options, merged options on exit
    @param args           comma separated kernel config line options
    @param ininame        os2ldr.ini file name
    @return -1 if RESTART selected, 1 on success  */
int _std cmd_mergeopts(char *line, char *args, char *ininame);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MENUOPTS
