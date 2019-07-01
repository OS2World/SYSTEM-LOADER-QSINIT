//
// QSINIT
// dump system pages
// ----------------------------------------------------
//
// For every function here 0 can be used to force common log (serial port)
// dump.
// Also note, that most of functions works inside of MT lock or unique
// global mutex, so any printing in pfn should be atomic.
//
// These functions are available in the shell via "ps /d?" command.
//
#ifndef QSINIT_DUMPPAGES
#define QSINIT_DUMPPAGES

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "qsmodint.h"
#include "stddef.h"   // for printf_function type

/** dump single process context
    @param  pq          Process context
    @param  pfn         Use 0 for common log_printf(). */
void  _std  log_dumppctx (process_context* pq, printf_function pfn);

/** MDT table dump (Ctrl-Alt-F10)
    @param  pfn      Dump printing function. Use 0 for log_printf() */
void  _std  log_mdtdump  (printf_function pfn);

/// open files list (clib, Ctrl-Alt-F4)
void  _std  log_ftdump   (printf_function pfn);

/// system file table dump (Ctrl-Alt-F4)
void  _std  io_dumpsft   (printf_function pfn);

/// sessions list (Ctrl-Alt-F2)
void  _std  log_sedump   (printf_function pfn);

/// shared classes list (Ctrl-Alt-F3)
void   _std exi_dumpall  (printf_function pfn);

/// GDT dump (Ctrl-Alt-F6)
void   _std sys_gdtdump  (printf_function pfn);

/// IDT dump (Ctrl-Alt-F7)
void   _std sys_idtdump  (printf_function pfn);

/// process tree dump (Ctrl-Alt-F5)
void   _std mod_dumptree (printf_function pfn);

/** print all active page tables (Ctrl-Alt-F8)
    VERY long dump, especially in UEFI.
    @param  pfn      Dump printing function. Use 0 for log_printf() */
void  _std  pag_printall (printf_function pfn);

/// PCI config space dump (Ctrl-Alt-F9)
void  _std  log_pcidump  (printf_function pfn);

/// global memory table dump (Ctrl-Alt-F11)
void  _std  hlp_memprint (printf_function pfn);

/** complete memory table dump (Ctrl-Alt-F11)
    This function print entire PC memory, including blocks above 4Gb */
void  _std  hlp_memcprint(printf_function pfn);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_DUMPPAGES
