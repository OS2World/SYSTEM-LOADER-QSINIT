/* (: doxygen dox here :) */

/** @mainpage QS Boot loader

@section sec_intro Introduction
QSINIT is a some kind of boot loader ;) Basically it load OS/2 kernel ;), but
suitable for many other things - like playing tetris or writing boot time
disk editor.

Common idea of this loader - creating a small 32 bit protected mode environment
with runtime and module executing support. It looks like old good DOS/4GW apps,
but without DOS at all ;)

Package contain of two parts: loader (OS2LDR) and zip archive with apps
and data (QSINIT.LDI). Duaring init this archive unpacked to small virtual
disk in memory and "system" operates with data on it as with normal files.

So, common boot process is:
@li partition boot code (FAT or micro-FSD on HPFS/JFS) loads OS2LDR (it looks 
like common kernel loader).
@li this init part (named QSINIT below) init protected mode, create a small
virtual disk (for self usage only, this memory will be available for OS/2) and
unpack QSINIT.LDI to it.
@li next, it launch second part of code from this disk (START module).
@li START module can launch any other modules. One of it named "bootos2.exe" ;)

@section sec_apiinfo Some general information
- @ref pg_apps "Apps and modules"
- @ref pg_apiref "API notes and reference"

@defgroup API1 API functions (QSINIT module)
@defgroup API2 API functions (START module)
@defgroup API4 API functions (CONSOLE module)
@defgroup API5 Disk management functions (PARTMGR module)
@defgroup CODE1 QSINIT source code
@defgroup CODE2 START source code
@defgroup CODE3 BootOS2.exe source code
@defgroup CODE4 PARTMGR source code

@file kload.h
@ingroup CODE3
Interface for kernel LX module specific functions.

@file kload.c
@ingroup CODE3
LX module specific functions.

@file k_arena.h
@ingroup CODE3
Some internal OS/2 structs 

@file bootos2.c
@ingroup CODE3
Loading OS/2 kernel ;) 

@file internal.h
@ingroup CODE2
Some internal functions (not exported). 

@file clib.c
@ingroup CODE2
Subset of C library functions. 

@file clibio.c
@ingroup CODE2
I/O related C library functions. 

@file cpplib.cpp
@ingroup CODE2
Stub for Watcom C++ library. 

@file envmgr.c
@ingroup CODE2
Environment C library functions. 

@file memmgr.cpp
@ingroup CODE2
Fast memory manager. 

@file trapscrn.c
@ingroup CODE2 
Trap screen implementation. 

@file lxmisc.c
@ingroup CODE2
Additional LE/LX support code 

@file mdtdump.c
@ingroup CODE2
Dump module table 

@file errno.h
@ingroup API2
API: errno values 

@file malloc.h
@ingroup API2
API: malloc macroses 

@file memmgr.h
@ingroup API2
API: fast memory manager 

@file qsshell.h
@ingroup API2
API: shell functions.

@file vioext.h 
@ingroup API2
API: additional console functions.

Some limits applied to command processing. For example, no support for quotes
in program names:
@code
"bootos2.exe" OS2KRNL, 
@endcode
nested quotes in arguments:
@code
test.exe "("quoted text")" 
@endcode
and many other such "featured" things ;) 

@file qsxcpt.h
@ingroup API2
API: exception handling.

@file qslist.h
@ingroup API2
API: pure C list support

See classes.hpp for detailed list desctiption.

@file qspage.h
@ingroup API2
API: paging mode support

@file qssys.h
@ingroup API2
API: system api

@file stdarg.h
@ingroup API2
API: C lib va_* support 

@file stdlib.h
@ingroup API2
API: C lib, second part.
This code located in START module.

@file direct.h
@ingroup API2
API: C lib, directory functions.
This code located in START module.

@file shell.cpp
@ingroup CODE2 
Common shell functions 

@file time.c
@ingroup CODE2
C lib time functions

@file start.c
@ingroup CODE2 
The "main" function 

@file mbox.cpp
@ingroup CODE2 
"MSGBOX" shell command implementation

@file syshlp.c
@ingroup CODE2 
Processor and hardware support (MTRR, MSR and so on).

@file viofunc.c 
@ingroup CODE2 
Advanced VIO functions

@file pcibus.c
@ingroup CODE2 
PCI bus handling

@file log.c 
@ingroup CODE2 
Internal log

@file doshlp.h
@ingroup CODE3
doshlp binary interface (autoconverted from .inc) 

@file ldrparam.h
@ingroup CODE3
OS/4 boot loader interface (autoconverted from .inc) 

@file vesainfo.h
@ingroup CODE3
OS/4 boot loader vesa data (autoconverted from .inc) 

@file vio.h
@ingroup API1
Text mode output and keyboard support routines.

All output functions update BIOS cursor position, but BIOS called only for
mode changing (vio_setmode(), vio_resetmode()) and keyboard. 

@file qsutil.h
@ingroup API1
API: utils 

@file qsmod.h
@ingroup API1
API: module/process base functions 

@file qslog.h
@ingroup API1
API: debug log 

@file qsint.h
@ingroup CODE1
Internal defs 

@file clib.h
@ingroup API1
API: C library 

@file setjmp.h
@ingroup API1
API: C library

@file mdt.c
@ingroup CODE1
LE/LX module handling.

Module handling code - loading, launching, import/export resolving.

Some functions located in lxmisc.c file in "start" module and available
only after it start. 

@file membase.c
@ingroup CODE1
Common memory allocating functions.

@li Any allocation rounded to 64k internally.
@li Allocated size is zero filled.
@li Incorrect free/damaged block structure will cause immediate panic. 

@file print32.c
@ingroup CODE1
Tiny printf subset. 

@file init32.c
@ingroup CODE1
Main code. 

@file disk1.c
@ingroup CODE1
Virtual disk creation. 

@file meminit.c
@ingroup CODE1
System memory tables parse code. 

@file fileio.c
@ingroup CODE1
Boot system file i/o. 

@file diskio.c
@ingroup CODE1
I/o requests processor for FAT code. 

@file ff.c
@ingroup CODE1
FAT code by ChaN. 

@file console.h
@ingroup API4
Common console mode/read/write functions.

@file qsuibase.h
@ingroup API4
Text mode interface api.

@file qsdm.h
@ingroup API5
Disk management functions.

@file format.c 
@ingroup CODE4
FAT formatting code.

@file lvm.c
@ingroup CODE4
OS/2 LVM support.

@file lvm.h
@ingroup CODE4
OS/2 LVM support.

@file mount.c
@ingroup CODE4
Disk management shell commands.

@file partmgr.c
@ingroup CODE4
Partmgr.dll main file

@file pboot.c
@ingroup CODE4
Boot sector, MBR and boot ops.

@file phlp.c
@ingroup CODE4
Disk management helper code.

@file pscan.c
@ingroup CODE4
Partition scan code.

@file pscan.h
@ingroup CODE4
Disk management internal definitions.

@file ptmgr.c 
@ingroup CODE4
Partition management code.
*/

/** @page pg_apiref API notes and reference.
- @subpage pg_common  "Common information"
- @subpage pg_fileio  "File and disk i/o"
- @subpage pg_screen  "Screen and keyboard access"
- @subpage pg_memory  "Memory management"
- @subpage pg_runtime "Runtime functions"
- @subpage pg_xcpt    "Exception handling"
- @subpage pg_sysapp  "System modules"
- @subpage pg_deblog  "Log and COM port output"
*/

/** @page pg_shell Shell

@section shdt_about Common information
Shell is simulate to default DOS / Windows / OS/2 family command processor. There are
many differences in command implementation and, naturally, QSINIT shell is not
equal to "large OS" shells.

There are no i/o redirection, filters and "con" file, at least.

Shell have a support for installing new commands and replacing the default ones.
Only internal commands like GOTO, FOR, IF, EXIT and so on cannot be overloaded. See
cmd_shelladd() for more details.

Shell commands can be called directly, from C code, without invoking command
processor. Actually, cmd.exe is a stub, which implements only searching modules
in path and determining it type.

Embedded help is supported for all installed commands (and can easy be implemended 
for a new ones by calling cmd_shellhelp()). Messages located in msg.ini file.

@section shdt_env Environment
There is one predefined environment variables:
BOOTTYPE - current boot type. Values are: FAT, FSD, PXE and SINGLE. SINGLE
           mean FAT16/32 boot without OS/2 installed (detemined by absence of
           OS2BOOT file in the root - QSINIT boot sector does not require it).

Some other can be accessed from shell as part of SET command syntax: "%TIME%",
"%DATE%", "%DBPORT%", etc.

*/

/** @page pg_screen Screen and keyboard access.

@section pgscr_text Text modes
Though, direct writing to text mode memory is possible in easy way, it 
highly not recommended, because of "graphic console" presence - i.e. text mode
simulation in VESA graphic modes.

So, in addition to common vio_strout(), vio_writebuf() and vio_readbuf() was
designed to read/write text mode memory contents.

Common text mode and keyboard functions referenced in @ref vio.h.

Addititional processing also available: read string from console (with callback),
message box: @ref vioext.h.

@section pgscr_graph Graphic modes
This functionailty still not finished, but some initial code is availabile in 
console.dll (@ref console.h). */

/** @page pg_fileio File and disk i/o.

@section fdio_fileio File I/O
QSINIT file i/o is based on 
<a href="http://elm-chan.org/fsw/ff/00index_e.html">ChaN`s FatFs</a> and use
it without additional code changes, so all "disks" in system are named by 
numbers, not letters:
@li "1:\"
@li "0:\os2krnl"

There are 2 predefined disks:
@li 0:\ - FAT boot drive (not used in micro-FSD boot: HPFS, JFS).
@li 1:\ - own virtual disk with data and apps, unpacked from qsinit.ldi

Disk numbers 2:-9: can be used to mount FAT/FAT32 partitions from physical
drives.

C library functions (@ref stdlib.h, direct.h) support this drive numbering conversion
to "standard names": A:..Z:, so A: will be FAT boot drive, B: - virtual disk
and so on. This is used in Turbo Vision code, at least. Because most of APIs
use C library as well, A: letters can be used in shell, batch files and so on.

Both "current directory" and "relative path" are supported. Default path after
init is 1:\.

Standard C library file i/o is "process" oriented. Every opened FILE* is attached
to current process and will be closed at process exit (even if it was opened in
global DLL). To prevent this - fdetach() can be called - detached file became
shared and can be used/closed by anyone.

@attention FAT code read and write LFN names both on FAT and FAT32 - this is
not compatible with OS/2. And file names must not contain characters above 
0x80. Only pure ASCII is allowed, else file will not be created/accessed.

@section fdio_btfileio Boot disk I/O
Because of very limited micro-FSD API, QSINIT is able to read only files
from the root of FSD boot drive (HPFS/JFS) and only in order open -> read -> 
close.\n
And PXE API is even more limited ;) It unable to return size of file on open,
so support only single read operation.

FAT or FAT32 can be readed instantly, as normal partition with full access.

There are 3 possible variants of boot file i/o:
@li <b>FSD</b> - @ref hlp_fopen, @ref hlp_fread, @ref hlp_fclose, @ref hlp_freadfull functions.
@li <b>PXE</b> - only @ref hlp_freadfull
@li <b>FAT</b> - hlp_f* functions as in FSD (emulated) and normal file i/o as 0:\\ drive.

As example of more comfortable access - QSINIT copies QSINIT.INI/OS2LDR.INI and
QSSETUP.CMD files (if exist one) from the root of boot disk to virtual drive (path 1:\\)
and then operates with it. This occur on any type of boot (FAT, mini-FSD, PXE).

@section fdio_diskio Disk I/O
Disk I/O can operate with all drives reported by BIOS. It detect drive type and
size and provide API to read and write it (@ref qsutil.h). 

Own virtual disk can be accessed at low level at the same way as real disks.

Any part of disk can be mounted as FAT/FAT32 partition.

Advanced functions is available in @ref sec_partmgr, which can be loaded 
by apps or QSINIT initialization code.
*/

/** @page pg_xcpt Exception handling.
A C++ like exception handling supported, code must look as defined below. The
order of sections are important, first catch section must be named _catch_ and
any of following - _alsocatch_.

For returning from inside of _try_ section, _ret_in_try_ or _retvoid_in_try_
macroses <b>must</b> be used.

Nested exceptions are supported.

Example of use:
@code
   #include <qsxcpt.h>

   _try_ {
      //...
      _ret_in_try_(1);
   }
   _catch_(xcpt_divide0) {
      //...
   }
   _alsocatch_(xcpt_align) {
      //...
   }
   _endcatch_
@endcode

In safe mode (left shift was pressed on start) - all exception handlers are
normal trap gates.

In normal mode - exceptions 8, 12 and 13 - handled by task gate. Virtual PC
unable to process it and close VM. To prevent this - NOTASK=1 can be added
to common section of QSINIT.INI/OS2LDR.INI, it disables task gate usage.

See @ref qsxcpt.h for more.
*/

/** @page pg_memory Memory management.
There are two methods of memory allocation in QSINIT:
@li "system" alloc
@li "heap" alloc

Both system and heap alloc share common address space.

@section memm_system System alloc
Provided by functions hlp_memalloc(), hlp_memrealloc(), hlp_memfree().
@li allocated memory is zero-filled (additional space after realloc call - too).
@li any allocation is rounded up to 64kb internally. So if you ask 1 byte, it
get 64k from system.
@li if you do NOT specify QSMA_RETERR flag - QSINIT hang to panic if no free
memory was founded.
@li any damage of memory headers will produce panic.
@li you can print memory table to log by hlp_memprint() (in debug build).

@section memm_heap Heap alloc
Provided by memmgr.h. Fast "heap" memory manager.
All of data are global too. 
@attention Allocated memory is NOT zero filled.

Features:
@li support of __FILE__ __LINE__ (in malloc.h). Do not reach <b>8190</b> lines per file!!
@li full dump list of blocks (memDumpLog())
@li optional paranoidal check mode, memCheckMgr() call
@li "batch" blocks freeing (marked by own unique Owner & Pool values).
@li print of TOP 32 memory users (memStatMax(), by Owner & Pool).

Manager allocate small blocks in multiple heaps (256kb each one), blocks >48k
are allocated directly from system, but support all of functionality too.<br>
Any damage of memory headers will cause immediate panic and dump.

Dumps of both memory managers can be printed to log in cmd shell or while
pause message is active:
@li press Ctrl-Alt-F11 to dump system memory table
@li press Ctrl-Alt-F12 to dump pool block list (very long)

Zero-fill still can be turned on for heap manager, but this functionality is
not tested extremly and not recommended. Just add HEAPFLAGS=4 key to common 
ini file (os2ldr.ini / qsinit.ini).

Also, 2 can be added to HEAPFLAGS key to turn on paranoidal checking of heap
consistency (full check of control structs on every (re)alloc/free call). This
option is terribly slow on large number of blocks, but will catch any heap
damage on next alloc/free call.

There is no debugger, but sys_errbrk() can help on random memory damages, it
allow to set DR0..DR3 debug registers to catch memory read/write on specified
address. Catched violation will produce INT 1 exception.
*/

/** @page pg_common Common topics.

@section comm_addrspace Address space
Address space is not physical-zero based. Actually zero of FLAT selectors
placed at 9100 or 9000 real mode segment (may be a much lower on PXE boot).

In safe mode (left shift was pressed on start) - all 4GBs are writable,
including address 0. In normal mode - first page is placed beyond of segment
limit and writing to offset 0...4095 of FLAT address space will cause GPF.

Reading from first page is possible in any mode, but at least first 512 bytes
of FLAT space filled with "int 3" call - which will shows trap screen if code
make a jump to it.

@section comm_paemode PAE paging mode

QSINIT can be switched to PAE paging mode to get access to memory above 4Gb
limit. This can be done by pag_enable() API function or "mode sys pae" shell
command.

In this mode QSINIT itself still operates in "FLAT" mode, without using
of page mapping. I.e. entire PC RAM below 4Gb is directly mapped "one-in-one",
as it was addressed in non-paging mode.

But new option available: possibility of mapping physical memory above the end
RAM in first 4th GBs / above 4GB limit.

@section comm_stack Using stack
First (binary) part of QSINIT loader partially designed in TINY 16 bit model
and assume DS==ES==SS way of living. Real mode call helpers like hlp_rmcall()
are always switch to this stack duaring call.

The size of this stack is printed to log before "start" module init: 
<b>"stack for rm calls: xx bytes"</b>. It vary on size from build to build
(page aligned).

"start" module and other executed apps use own stack, allocated by module
launcher. 32kb is used in most of prj files. Be careful - dir functions
like _dos_stat(), _searchenv() can eat >=1kb because of multiple LFN buffers
allocated in stack. Many of @ref sec_partmgr functions creates 4kb buffer
in stack for one sector reading. Buffer for main() argv's array data is
allocated <b>from stack</b> too.

DLLs does not have own stack and use stack of calling module.
*/

/** @page pg_apps Apps and modules.

QSINIT can load both LE and LX modules.

Loader features and disadvantages:
@li does not load LE with iterated pages (who can link it?)
@li imports/exports/forwards by name is not supported at all
@li module <b>must</b> contain internal fixups! This is <b>required</b> because
of lack of 4k paging.
@li 32bit module ordinals is not supported (and again - who can link it?)
@li up to 32 imported modules per module (this can be changed in code).
@li 16 bit objects can be used, but cannot be start or stack object.
@li "chaining fixups" can be used to optimize fixup table size.

For linking LX modules by Watcom linker two options must be added:
@code
op internalrelocs
op togglerelocs
@endcode
Watcom linker incorrectly inverse "internalrelocs" flag and add 
an option to inverse it back ;)

LxLite.exe (available in QSINIT SDK) do not compress LX with default parameters,
but optimize LX fixups to "chaining" mode. This can save up to 10-15% of module
size without any compression.

More details:
@li FLAT 32 bit DOS. No threads, no paging, shared address space (PAE still
available, but only for mapping memory above 4Gb limit).
@li launching "process" (module) get parameters and environment in the same
way as in OS/2. The diffrences are: FS == 0 (no TIB), GS contain pointer to
current process reference. Compiler must not alter GS register (this not kill
you, but a lot if runtime functions will return error).
@li DLL modules (both code and data) are <b>global</b>, DLL init called
on first load, fini - on real unload (zero usage couner).
@li system does not mark memory as belonging to specified process, so, no
any "automatic free" on exit. Module <b>must collect all garbage</b> before 
exit!
@li unlike memory, all files opened by module via standard fopen() function
will be closed after process termination.
@li exporting from EXE is supported. EXE and DLL can be cross-linked by
imports from each other, but it is better to not use this feature (DLLs are
global and DLL will be cross-linked with <b>first</b> EXE module, which load
it. Second copy of the same EXE can hang).
@li module can import functions from self (this can be used to trace internal
module calls).
@li forward entries resolved at the moment of first import. So, you can load
DLL with forward to missing index in presented module. But this forward can't
be used.

Environment is not global, child process inherit it in current state. Any
changes was made by setenv() affect current process and it's childs only.

Dump of currently loaded modules can be printed to log by Ctrl-Alt-F10 in cmd 
shell or duaring pause "press any key..." waiting.

@ref pg_runtime "Runtime functions".

API: qsmod.h

*/

/** @page pg_runtime Runtime functions

Linked modules must not use any OS/2 specific functions and any DOS/4GW calls
(if you link for platform pmode/w in Open Watcom). So you must link it without
runtime libraries and, even, startup code.

Because of incompatibility with all presented watcom platforms, own runtime is
used. The only thing used from default watcom libs is C++ constructors/destructors
support (plib3s.lib).

Default C library has a short list of functions, but it's extending from time
to time.

DPMI available, but "unofficially" ;) and without memory allocation functions.
And there is no any support for allocating memory in 1st meg now.

All available runtime and C library functions linking to app as imports
from modules QSINIT ans START (LE exe support DLL import and wlink can link with
it successfully). As result - EXE sizes on pure C are very small ;)

By some hacks Open Watcom base C++ runtime (low level classes support) still
can be linked without C runtime ;)
So C++ (if Open Watcom C++ can be named so ;)) is available to use.

Static variables and classes initialization is supported. 

@ref pg_xcpt "Exception handling" supported.

On FAT boot you can access entire boot partition and, even, write to it by
standard fopen() functions.

On HPFS/JFS/RIPL boot only root dir files can be accessed via hlp_fopen() API.
This is limitation of micro-FSD, available at boot time.

Any call to int 21h will return CF=1 and do nothing. QSINIT is not DOS! ;)

DLL LibMain can be replaced by custom one.

*/

/** @page pg_sysapp System modules.

@section sec_qsinit QSINIT module

This is not a module, but binary file with size <64k. It looks like IBM OS2LDR
for partition boot code.

Internally it contain:
@li PMODE (from famous free protected mode extender sources)
@li zlib (inflate part only) with tiny unzip support
@li FAT code by ChaN (best FAT code I ever seen ;))
@li LE/LX module loading/launching
@li a bit of C runtime functions
@li system functions (keyboard, text video modes, file i/o, memory management)

@section sec_start START module

This module is a special case. It containing some LX/LE loading functionality, so
it cannot use DLLs (except QSINIT), cannot be compressed and cannot be unloaded.

Contain:
@li heap memory manager (memmgr.cpp)
@li LE/LX code (lxmisc.c, mdtdump.c)
@li exception handler (trapscrn.c)
@li C lib (clib.c, clibio.c, time.c)
@li working with environment (envmgr.c)
@li ini files support (ldrini.cpp)
@li shell functions (shell.cpp)
@li exportable to C "classes" support (exi.cpp) and lists based on it.

@section sec_boot Boot process

@subsection sec_bootstep1 Step 1: QSINIT module
@li QSINIT (named as OS2LDR) loaded by micro-FSD code or FAT boot
@li save register values on init (for exit_restart() code), zero own bss, etc
@li if micro-FSD present - it header checked for various errors
@li memory allocated for protected mode data (GDT, etc)
@li copying self to the top of low memory (9xxx:0000) and saving
mini-FSD (binary file) in 1000:0 (temporary)
@li make some real mode task: i/o delay calculation, query PC memory,
remap low 8 ints to 50h
@li going to 16 bit PM
@li init minor things and going to 32 bit PM (init32.c)
@li init own memory management and get for self all memory above 16Mb until 
the end of whole block
@li save mini-fsd/fat root in available high memory (clearing 1st Mb)
@li load qsinit.ldi zip file, calculate unpacked size, create virtual
disk and format it with FAT. Unpack zip file here - this will be "1:\" drive
(disk1.c).
@li load module "start" from this disk and start it.

@subsection sec_bootstep2 Step 2: START module
@li initing exception handlers
@li initing heap manager (memmgr.cpp)
@li initing internal log
@li exporting functions for QSINIT
@li deleting self from RAM disk (to save space)
@li reading minor options from os2ldr.ini
@li launching start.cmd - shell is ready

@section sec_partmgr PARTMGR module

This module is an optional resident DLL, which add a support for advanced disk 
management and appropriate shell commands: mount, dmgr, format, etc.

By default, it loaded automaticaly by start.cmd or bootmenu.exe, but also can
be loaded by any process, linked with it. 

When DLL became resident (loaded by LM command) - it adds shell commands to the
list of available commands.

Most of available functions defined in qsdm.h. Some of them allow more, than shell
commands and contain much less checks, so be carefull in direct use.

@section sec_cachemod CACHE module

This module is an optional resident DLL, which add a support of read-ahead disk
cache on a sector i/o level. FAT system areas have a priority in caching, if this
FAT/FAT32 partition is mounted to QSINIT.

By default module loaded by PARTMGR on first mount command, or loaded by start.cmd
in non-OS/2 boot mode, there no reasons to speed up boot time.

Cache can use up to 50% of memory, available to QSINIT. Cache is non-destructive,
i.e. all writings will direct to HDD immediately.

When module is loaded - "cache" command is available in shell, it allow to setup
shell and include/exclude disks for caching.

@section sec_consolemod CONSOLE module

This module provide support for "graphic console" and VESA graphic modes.
"Mode con" command loads it permanently on first use. */

/** @page pg_deblog Log and COM port output

@section sec_intlog System log
QSINIT supply internal 64k buffer for logging. Log output is routed also
to COM port, if address available in INI file. 

Every log message have time label and log level value (0..3) attached.

This log can be viewed in sysview.exe app and can be copied to OS/4 kernel
log by adding key "LOGLEVEL" to kernel options.

Some dumps are available in cmd shell or while pause message is active:
@li Ctrl-Alt-F6 - gdt dump
@li Ctrl-Alt-F7 - idt dump
@li Ctrl-Alt-F8 - page tables dump (very long, in paging mode only)
@li Ctrl-Alt-F9 - entire pci config space dump (very long)
@li Ctrl-Alt-F10 - module table dump
@li Ctrl-Alt-F11 - system memory table dump
@li Ctrl-Alt-F12 - pool block list (very long)

@section sec_comport COM port output.
COM port address and baud rate can be changed "on fly", by "mode sys"
shell command.*/
