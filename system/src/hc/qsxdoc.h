/* (: doxygen dox here :) */

/** @mainpage QS Boot loader

@section sec_warn (this doc is outdated permanently, but still usable)

@section sec_intro Introduction
QSINIT is a some kind of boot loader ;) Basically it load OS/2 kernel ;), but
suitable for many other things - like playing tetris or writing boot time
disk editor.

Common idea was creating of a small 32 bit protected mode environment with
runtime and module executing support. Initially it looked like old good
DOS/4GW apps, but without DOS at all ;)

Next, it was extended to (optional) multithreading (UNIprocessor only, sorry -
just because BIOS is used as a driver for everything). So, now it is more
in os-like style, because of existing sessions, threads and other nice things.

Common apps include disk sector editor, partition management tools, text and
binary file editors, tetris :) and so on.

Binary package composed from two parts - starting binary (OS2LDR) and zip
archive with applications and data (QSINIT.LDI). During init this archive
unpacked to the small virtual disk in memory and "system" operates with data
in it (it always mounted as drive B:).

So, common boot process is:
@li partition boot code (FAT or micro-FSD on HPFS/JFS) loads OS2LDR (it looks
like common OS/2 kernel loader for OS/2 micro-FSDs).
@li this binary (named QSINIT below) enters protected mode, creates a small
virtual disk and unpack QSINIT.LDI to it.
@li next, it launch second part of own code from this disk (START module).
@li START module able to launch any other modules. One of it named "bootos2.exe" ;)

@section sec_apiinfo Some general information
- @ref pg_apps "Apps and modules"
- @ref pg_apiref "API notes and reference"

@defgroup API1 API functions
@defgroup API4 API functions (CONSOLE module)
@defgroup API5 Disk management functions (PARTMGR module)
@defgroup API6 API functions (multi-thread mode functions)
@defgroup CODE1 QSINIT source code
@defgroup CODE2 START source code
@defgroup CODE3 BootOS2.exe source code
@defgroup CODE4 PARTMGR source code
@defgroup CODE5 CACHE source code
@defgroup CODE6 VDISK source code (PAE ram disk)
@defgroup CODE7 CPLIB source code (code pages support)
@defgroup CODE8 MTLIB source code (threads support)
@defgroup CODE9 CONSOLE source code (mode command and graphic console)
@defgroup CODE10 VHDD source code (own format filedisk).
@defgroup CODE11 EXTCMD source code (optional system/shell functionality).

@file mtlib.c
@ingroup CODE8
MTLIB module main file

@file mutex.c
@ingroup CODE8
Mutexes handling

@file process.c
@ingroup CODE8
Process initialization/finalization code.

@file sched.c
@ingroup CODE8
Context switching code.

@file smcode.c
@ingroup CODE8
Session management

@file thread.c
@ingroup CODE8
Thread-related code.

@file tmproc.c
@ingroup CODE8
Timer interript.

@file wobj.c 
@ingroup CODE8
Wait object implementation.

@file cplib.c
@ingroup CODE7
CPLIB module main file

@file cplib.h
@ingroup CODE7
CPLIB shared class interface

@file qsvdmem.h
@ingroup CODE6
VDISK module shared class interface

@file vdisk.c
@ingroup CODE6
VDISK module main file

@file vdload.c
@ingroup CODE6
Fixed size VHD file loading to VDISK

@file vdproc.c
@ingroup CODE6
Common VDISK functions

@file qscache.h
@ingroup CODE5
CACHE shared class interface

@file cache.c
@ingroup CODE5
CACHE module main file

@file cproc.c
@ingroup CODE5
Common caching code

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
@ingroup API1
API: errno values

@file memmgr.h
@ingroup API1
API: fast memory manager

@file qsshell.h
@ingroup API1
API: shell functions.

@file qstask.h
@ingroup API6
API: thread management functions.

@file qsio.h 
@ingroup API1
API: system file i/o functions.

@file vioext.h
@ingroup API1
API: additional console functions.

@file qsxcpt.h
@ingroup API1
API: exception handling.

@file qslist.h
@ingroup API1
API: pure C list support

See classes.hpp for detailed list desctiption.

@file qspage.h
@ingroup API1
API: paging mode support


@file bitmaps.h
@ingroup API1
API: bitmap shared class

@file qshm.h
@ingroup API1
API: hardware management functions

@file qssys.h
@ingroup API1
API: system api

@file stdarg.h
@ingroup API1
API: C lib va_* support

@file stdlib.h
@ingroup API1
API: C lib, second part.
This code located in START module.

@file direct.h
@ingroup API1
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

@file loadmsg.c 
@ingroup CODE3
OS2LDR.MSG loading code.

@file diskparm.c 
@ingroup CODE3
BPB replacement magic (for ramdisk boot and switch boot partition).

@file vio.h
@ingroup API1
Text mode output and keyboard support routines.

All output functions update BIOS cursor position, but BIOS called only for
mode changing (vio_setmode(), vio_resetmode()) and keyboard.

In addition, no any assumes may be used on BIOS, because this is may be an EFI
host or graphic console modes or, even, COM port terminal session. I.e. direct
access to the screen is prohibited.

@file qsutil.h
@ingroup API1
API: utils

@file qsmod.h
@ingroup API1
API: module/process base functions

@file qsmodext.h
@ingroup API1
API: module/process additional functions

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

Module handling code - loading, launching (in non-MT mode), import/export
resolving.

Some functions located in lxmisc.c file of START module.

@file membase.c
@ingroup CODE1
Common memory allocating functions.

@li Any allocation rounded to 64k internally.
@li Allocated size is zero filled.
@li Incorrect free/damaged block structure will cause immediate panic.

@file print.c
@ingroup CODE1
Base implementation of all printf fuinctions.

@file init32.c
@ingroup CODE1
Main code.

@file disk1.c
@ingroup CODE1
Virtual disk creation.

@file meminit.c
@ingroup CODE1
System memory tables parsing code (the only one 16-bit real mode C file).

@file fileio.c
@ingroup CODE1
Boot system file i/o.

@file fatio.c 
@ingroup CODE1
Lightweight implementation of r/o FAT/FAT32/exFAT file i/o. It simulates
missing FAT micro-FSD for QSINIT binary code and used to load initial
files (QSINIT.LDI and INI file(s)) from the root directory.

After START module initialization all these calls replaced by FatFs code.

@file process.c
@ingroup CODE1
Default (non-MT mode) process initialization.

@file iniparm.c
@ingroup CODE1
Ini file first time read.

@file diskio.c
@ingroup CODE1
I/o requests processor for FAT code.

@file ff.c
@ingroup CODE2
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

@file console.c
@ingroup CODE9
CONSOLE module main file.

@file conint.h
@ingroup CODE9
CONSOLE module internal header.

@file fonts.c
@ingroup CODE9
Binary fonts support.

@file outbase.c
@ingroup CODE9
Base (common) functions for graphic output.

@file outefi.c
@ingroup CODE9
UEFI based console support.

@file outvesa.c
@ingroup CODE9
VESA based console support.

@file vhvioex.c 
@ingroup CODE9
System code for graphic console output (internal shared class).

@file writegif.c 
@ingroup CODE9
Screenshot writing code (GIF file).

@file rwdisc.c
@ingroup CODE10
File disk shared class implementation.

@file vhdd.c
@ingroup CODE10
VHDD module main file

@file qsvdimg.h
@ingroup CODE10
File disk shared class interface.

@file extcmd.c
@ingroup CODE11
EXTCMD module main file

@file extdlg.c
@ingroup CODE11
Additional system dialog(s) shared class implementation.

@file mtrrmem.c
@ingroup CODE11
MTRR shell command

@file pci.c
@ingroup CODE11
PCI shell command

@file vhtty.c 
@ingroup CODE11
System code for serial port console output (internal shared class).

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
Shell is simulate to default DOS / Windows / OS/2 family command processor.
There are many differences in command implementation and, naturally, QSINIT
shell is not equal to "large OS" shells.

There are no full i/o redirection, filters and "con" file, at least.

Shell have a support for installing new commands and replacing default ones.
Only internal commands like GOTO, FOR, IF, EXIT and so on cannot be overloaded.
See cmd_shelladd() for more details.

Shell commands can be called directly, from C code, without invoking command
processor. Actually, cmd.exe is a stub, which implements only searching modules
in path and determining its type. This mean, in addition, that any application
can call batch files internally, in own process context, with affecting own
environment.

But, be careful with side effects of this (ability of shell commands to open
and write to a file, which is locked by the current process, for example). You
can use "cmd /c ..." to avoid this, of course.

Embedded help is supported for all installed commands (and can be implemended
for a new ones by calling cmd_shellhelp()). Messages located in msg.ini file.

@section shdt_env Environment
There are two predefined environment variables:
BOOTTYPE - current boot type. Values are: FAT, FSD, PXE and SINGLE. SINGLE
           mean FAT16/32/exFAT boot without OS/2 installed (detemined by
           absence of OS2BOOT file in the root - FAT16 OS2BOOT file required
           for IBM OS/2 loader only). CD-ROM boot in non-emulation mode has
           FSD type, because boot code is CDFS micro-FSD in fact. EFI host
           also presents itself as FSD (to block QSINIT access to the boot FAT
           partition, such simultaneous access can damage data on it).

HOSTTYPE - current host type. Values are: BIOS and EFI.

Many other can be accessed from shell as part of SET command syntax: "%TIME%",
"%DATE%", "%DBPORT%", etc.
*/

/** @page pg_screen Screen and keyboard access.

@section pgscr_text Text modes
Though, direct writing to text mode memory is possible in easy way, it
highly not recommended, because of "graphic console" presence (text mode
simulation in VESA graphic modes) and because of EFI version, where no VGA
text modes at all.

AND because of session support when multitasking is active (i.e. current
session can be at the background).

So, in addition to common vio_strout(), vio_writebuf() and vio_readbuf() was
designed to read/write text mode memory contents.

Common text mode and keyboard functions referenced in @ref vio.h.

Addititional processing also available: read string from console (with callback),
message box and more: @ref vioext.h.

/** @page pg_fileio File and disk i/o.

@section fdio_fileio File I/O

QSINIT file access initially was based on
<a href="http://elm-chan.org/fsw/ff/00index_e.html">ChaN`s FatFs</a> and used
it without additional code changes, so all "disks" in system has additional
namimg by numbers:
@li "1:\"
@li "0:\os2krnl"

Where "0:" is equal to "A:" and so on. Both variants accepted everywhere in
API.

There are 2 predefined disks:

@li 0:/A: - FAT boot drive (not used in micro-FSD boot: HPFS, JFS, PXE, cd-rom).
@li 1:/B: - own virtual disk with data and apps, unpacked from qsinit.ldi

Disk numbers 2:..9:(C:..J:) can be used to mount FAT/FAT32/exFAT partitions
from the physical drives.

File access functions look usual and defined in @ref qsio.h.
64-bit file size is also supported (on exFAT).

Both "current directory" and "relative path" are supported. Default path after
init is B:\.

File i/o is "process" oriented. Every open file is attached to the current
process and will be closed at process exit (even if it was opened in global
DLL). To prevent this - is should be "detached": detached file becomes shared
over system and can be used/closed by anyone.

@attention FAT code read and write LFN names both on FAT and FAT32 - this is
not compatible with OS/2. And file names must not contain characters above
0x80 - if no codepage was selected (default mode).

@section fdio_btfileio Boot disk I/O
Because of very limited micro-FSD API, QSINIT is able to read only files
from the root of FSD boot drive (HPFS/JFS) and only in order open -> read ->
close.\n
And PXE API is even more limited ;) It unable to return size of file on open,
so support only single read operation.

In multitasking mode opening of file via micro-FSD locks system global mutex
until its close by the same thread (or thread exit). I.e. any other process/thread
will be stopped on boot disk I/O functions until their availability.

FAT/FAT32/exFAT can be accessed instantly, as normal partition with full
access. HPFS now has r/o driver which also provides normal read access.

I.e. there are 3 possible variants of boot file i/o:
@li <b>FSD</b> - @ref hlp_fopen, @ref hlp_fread, @ref hlp_fclose, @ref hlp_freadfull functions.
@li <b>PXE</b> - only @ref hlp_freadfull
@li <b>FAT</b> - hlp_f* functions as in FSD (emulated) and normal file i/o as 0:\\ drive.

As example of more comfortable access - QSINIT copies QSINIT.INI/OS2LDR.INI and
QSSETUP.CMD files (if exist one) from the root of boot disk to virtual drive
and then operates with it. This occur on any type of boot (FAT, mini-FSD, PXE).

@section fdio_diskio Disk I/O
Disk I/O can operate with all drives reported by BIOS. It detects drive type
and size and provides API to read and write it (@ref qsutil.h).

Sometimes it able to show boot CD/DVD too (on rare types of BIOS).

Own virtual disk can be accessed at low level at the same way as real disks.

Any part of disk can be mounted as QSINIT volume. If FAT/HPFS file systems is
detected - it becomes available for use, else mounted volume can be formatted
(for example) or accessed via sector level i/o from appilcation.

Advanced functions are available in @ref sec_partmgr.
*/

/** @page pg_xcpt Exception handling.
Exception handling supported in C code, to use this code must looks as defined
below. The order of sections are important, first catch section must be named
_catch_ and any of following - _alsocatch_.

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
Provided by hlp_memalloc(), hlp_memallocsig(), hlp_memrealloc() and hlp_memfree()
functions.
@li allocated memory is zero-filled (additional space after realloc call - too).
@li any allocation is rounded up to 64kb internally. So if you ask 1 byte, it
gets 64k from system.
@li if you do NOT specify QSMA_RETERR flag - QSINIT hang to panic if no free
memory was found.
@li any damage of memory headers will produce panic.
@li you can print memory table to log by hlp_memprint() (in debug build).
@li hlp_memallocsig() allows to set 4 chars signature to block, which is
visible in log dump.
@li you can select a block ownership - global system or current process.

@section memm_heap Heap alloc
Provided by memmgr.h and malloc.h. Fast "heap" memory manager.
All of data are global too.
@attention Allocated memory is NOT zero filled.

Features:
@li 4 types of allocation - process owned, thread owned, DLL owned and
shared over system.
@li full dump list of blocks (mem_dumplog())
@li for shared memory file and line information is available in log dump.
@li optional paranoidal check mode, mem_checkmgr() call
@li print TOP 32 memory users (mem_statmax(), by Owner & Pool).

System supports "garbage collection" for process and thread owned blocks. I.e.,
when process or thread exits - any belonging blocks will be auto-released.

DLL owned blocks acts as "system shared" while DLL is loaded and will be
auto-released on its unload.

Note, that these rules also include C++ "new"! It result must be converted
to shared type block by special macro (__set_shared_block_info()) if you
planning to use it after process exit/DLL unload.

Manager allocates small blocks in multiple heaps (256kb each one), blocks >48k
are allocated directly from system, but supports the same functionality.<br>
Any damage in memory headers will cause immediate panic and dump.

Dumps of both memory managers can be printed to log in cmd shell or while
pause message is active:
@li press Ctrl-Alt-F11 to dump system memory table
@li press Ctrl-Alt-F12 to dump pool block list (very long)

Zero-fill still can be turned on for heap manager, but this functionality is
not hardly tested. Just add HEAPFLAGS = 4 key to common ini file 
(os2ldr.ini / qsinit.ini).

Also, 2 can be added to HEAPFLAGS key to turn on paranoidal checking of heap
consistency (full check of control structs on every (re)alloc/free call). This
option is terribly slow, but it catches any heap damage on next alloc/free call.

There is no debugger, but sys_errbrk() can help on random memory damages, it
allow to set DR0..DR3 debug registers to catch memory read/write on specified
address. Catched violation will produce INT 1 exception.

Also, log_dumpregs() and hlp_dumpregs() functions dumps CPU registers into 
the debug serial port and log without affect to it and CPU flags.
*/

/** @page pg_common Common topics.

@section comm_addrspace Address space
Address space is physical-zero based. In previous versions of QSINIT it was
non-zero based, and zero of FLAT selectors was placed at 9100 or 9000 real
mode segment (may be a much lower on PXE boot).

But now FLAT is classic zero-based, with linear==physical mapping.

BUT! To use any memory above the end of RAM in first 4GBs (call sys_endofram()
to query it) - pag_physmap() MUST be used. This is the only way to preserve
access to this memory when QSINIT will be switched to PAE paging mode
(can occur at any time by single API call).

In safe mode (left shift was pressed on start) - all 4GBs are writable,
including address 0. In normal mode - first page is placed beyond of segment
limit and writing to offset 0...4095 of FLAT address space will cause a GPF.

In PAE paging mode first page is mapped as r/o. Special hlp_memcpy() call
can be used to access it in all modes.

@section comm_paemode PAE paging mode

QSINIT can be switched to PAE paging mode to get access to memory above 4Gb
limit. This can be done by pag_enable() API function or "mode sys pae" shell
command. Switching is permanent, returning back to non-paged mode is not
supported.

In this mode QSINIT itself still operates in "FLAT" mode, without using
of page mapping. I.e. entire PC RAM below 4Gb is directly mapped "one-in-one",
as it was addressed in non-paging mode.

But new option available: possibility of mapping physical memory above the end
RAM in first 4th GBs / above 4GB limit.

@section comm_stack Using stack
Real mode callbacks of QSINIT are designed in TINY 16 bit model and assume
DS==ES==SS way of living. Real mode call helpers like hlp_rmcall() are always
switch to its stack duaring call.

The size of this stack is printed to log before "start" module init:
<b>"stack for rm calls: xx bytes"</b>. It vary on size from build to build
(page aligned).

"start" module and other executed apps use own stack, allocated by module
launcher. 32kb is used in most of prj files. Be careful - dir functions
like _dos_stat(), _searchenv() can eat >=1kb because of multiple LFN buffers
allocated in stack. Many of @ref sec_partmgr functions creates 4kb buffer
in stack for one sector reading. Buffer for main() argv's array data is
allocated <b>from stack</b> too.

DLLs does not have own stack and uses stack of calling module.

Threads allocate stack from the system heap (i.e. it independent from the
common process stack).
*/

/** @page pg_apps Apps and modules.

QSINIT can load both LE and LX modules.

Loader features and disadvantages:
@li does not load LE with iterated pages (who can link it?)
@li imports/exports/forwards by name is not supported at all
@li module <b>must</b> contain internal fixups! This is <b>required</b> because
of lack of real page mapping.
@li 32bit ordinal values is not supported (and again - who able link with it?)
@li up to 32 imported modules per module (this can be changed in code).
@li 16 bit objects can be used, but cannot be start or stack objects.
@li "chaining fixups" can be used to optimize fixup table size.
@li data cannot be exported from 32-bit CODE objects! The source of problem
is creation of small thunk for every CODE export entry, this thunk required
for function chaining (qsmodext.h).

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
@li FLAT 32 bit DOS. No threads, no paging, shared address space - by default.
@li threads and sessions still available, but optional ;)
@li switching to PAE paging mode is available too, but only for mapping memory
above 4Gb limit.
@li launching "process" (module) gets parameters and environment in the same
way as in OS/2. The one diffrence is: FS == 0 (no TIB).
@li DLL modules (both code and data) are <b>global</b>, DLL init called
on first load, fini - on real unload (zero usage counter).
@li by default system marks a heap memory as belonging to current process.
This memory will be released on exit. Any way, @ref memm_system blocks are
shared over system and still requires user handling on exit!
@li all files, opened by module via standard i/o functions will be closed
after process termination.
@li exporting from EXE is supported. EXE and DLL can be cross-linked by
imports from each other, but it is better to not use this feature (DLLs are
global and DLL will be cross-linked with <b>first</b> EXE module, which load
it. Second copy of the same EXE can hang).
@li module can import functions from self (usable to trace internal module
calls, at least).
@li forward entries resolved at the moment of first import. So, you can load
DLL with forward to missing index in existing module. But this forward can't
be used.

Environment is not global, child process inherits it in current state. Any
changes was made by setenv() affect current process only.

Dump of all loaded modules can be printed to log by Ctrl-Alt-F10 hotkey and
process tree - Ctrl-Alt-F5.

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

DPMI available, but "unofficially" ;) - in BIOS hosted version only and without
memory allocation functions. And there is no any support for allocation of
memory in 1st meg now.

All available runtime and C library functions linked to app as imports
from modules QSINIT ans START. As result - EXE sizes on pure C are very small ;)

By some hacks Open Watcom base C++ runtime (low level classes support) still
can be linked without C runtime ;)
So C++ (if Open Watcom C++ can be named so ;)) is available to use.

Static variables and classes initialization is supported.

@ref pg_xcpt "Exception handling" supported.

On FAT boot you can access entire boot partition and, even, write to it by
standard fopen() functions. On HPFS partition access is read-only.

On JFS/RIPL boot only root dir files can be accessed via hlp_fopen() API.
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
@li LE/LX module loading/launching
@li a bit of C runtime functions
@li system functions (keyboard, text video modes, file i/o, global memory handler)

EFI version (QSINIT.EFI) contains the same things, but EFI specific. I.e. this
is 64-bit EFI executable, which has linked in 32-bit part. 64-bit part
provides system services for QSINIT as well as real mode 16-bit code in BIOS
version provides the same.

@section sec_start START module

This module is a special case. It containing some LX/LE loading functionality,
so it cannot use DLLs (except QSINIT) and cannot be compressed.

Actually this is main part of QSINIT "kernel".

It contains:
@li heap memory manager (memmgr.cpp)
@li LE/LX code (lxmisc.c, mdtdump.c)
@li exception handling (trapscrn.c)
@li C lib (clib.c, clibio.c, time.c, signals.c)
@li environment functions (envmgr.c)
@li ini files support (pubini.cpp)
@li FAT code by ChaN (ff.c)
@li shell functions (shell.cpp, extcmd.cpp)
@li "shared classes for C" implementation (exi.cpp).
@li trace support (trace.cpp)
@li PCI bus functions (pcibus.c) and CPU specific functions (syshlp.c)
@li system file i/o (sysio.c)
@li queue, tls and other process/thread related (sysmt.c, sysque.cpp)
@li internal log (log.c)
@li final PC memory parsing code (meminit.cpp). Initial code located in QSINIT binary.
@li PAE paging management (pages.c)
@li system VIO handler - sessions/devices support and so on (viobase.cpp, vhvio.cpp)
@li additional VIO functions (mbox.cpp, viofunc.c)
and many other ...

@section sec_boot Boot process (outdated!)

@subsection sec_bootstep1 Step 1: QSINIT module
@li QSINIT (named as OS2LDR) loaded by micro-FSD code or FAT boot
@li save register values on init (for exit_restart() code), zero own bss, etc
@li if micro-FSD present - it header checked for various errors
@li memory allocated for protected mode data (GDT, etc)
@li copy self to the top of low memory (9xxx:0000) and saves mini-FSD (binary file)
@li makes some real mode task: i/o delay calculation, query PC memory,
remap low 8 ints to 50h
@li going to 16 bit PM, unpack 32-bit part and launch it (init32.c).
@li init own memory management and get for self all memory above 16Mb until
the end of whole block
@li loads QSINIT.LDI zip file, calculate unpacked size, creates virtual
disk and format it with FAT. This will be B:\ (1:\) drive (disk1.c).
@li load module START from this zip and start it.

@subsection sec_bootstep2 Step 2: START module continue initialization in this order:
@li exception handlers
@li heap manager (memmgr.cpp)
@li internal log and key storage (micro-registry).
@li session management
@li file i/o
@li unpack QSINIT.LDI to the ramdisk
@li exporting functions for QSINIT
@li clib file i/o initialization
@li reading minor options from OS2LDR.INI
@li more advanced memory setup
@li CPU detection
@li shell commands initialization
@li version match check between OS2LDR binary and QSINIT.LDI (yes, too late,
but usually it helps ;))
@li launching start.cmd - shell is ready

@section sec_partmgr PARTMGR module

This module is an optional resident DLL, which adds a support of advanced disk
management and appropriate shell commands: mount, dmgr, format, etc.

By default, it loaded automaticaly by start.cmd or bootmenu.exe, but also can
be loaded by any process, linked with it.

When DLL stays resident - it adds shell commands to the list of available
commands.

Most of available functions defined in qsdm.h. Some of them allow more, than
shell commands and contain much less checks, so be carefull in direct use.

@ref CODE4

@section sec_modcache CACHE module

This module is an optional resident DLL, which addS a support of read-ahead
disk cache on a sector i/o level. FAT system areas have a priority in caching,
if this FAT/FAT32/exFAT partition is mounted to QSINIT.

By default module loaded by PARTMGR on first mount command, or by start.cmd
in non-OS/2 boot mode, there no reasons to speed up boot time.

Cache can use up to 50% of memory, available to QSINIT. Cache is non-destructive,
i.e. all writings will direct to HDD immediately.

When module is loaded - "cache" command is available in shell, it allows to
setup shell and include/exclude disks for caching.

@ref CODE5

@section sec_modconsole CONSOLE module

This module provides support for "graphic console" and VESA/EFI graphic modes.
"Mode con" command loads it permanently on first use (@ref CODE9).

@ref CODE9

@section sec_modcplib CPLIB module

This module adds code pages and CHCP command into the system. It supports a
short number of SBCS code pages. No DBCS now, sorry.

Without this code any FAT partitions unable to manage names with national
charactes and HPFS format is unavailable.

@ref CODE7

@section sec_modvdisk VDISK module

This is "PAE ramdisk" supporting module (disk creation, RAMDISK shell command
and disk i/o functions for QSINIT itself).

@ref CODE6

@section sec_modvhdd VHDD module

A primitive file-disk implementation. Too slow to be a real filedisk, but
usable for disk structure backup and PARTMGR module testing.

@ref CODE10

@section sec_modmtlib MTLIB module

System module, which provides multithreading. Scheduler, context saving,
thread/process creation in MT mode, wait objects implementation - this is it.

@ref CODE8

@section sec_modextcmd EXTCMD module

The collection of optional code, which is just removed from the START module,
because it optional. MTTR and PCI commands here, as well as serial port
console support.

@ref CODE11
*/

/** @page pg_deblog Log and COM port output

@section sec_intlog System log
QSINIT creates internal 256k buffer (64k on a small amount of memory) for
logging. Log output is routed also to COM port, if address available in INI
file.

Every log message have a time label and log level value (0..3) attached.

This log can be viewed in sysview.exe app and can be copied to boot time
log by adding key "LOGLEVEL" to kernel options.

Some dumps are available during keyboard wait (in MT mode - at any time):
@li Ctrl-Alt-F2 - session list
@li Ctrl-Alt-F3 - installed shared class list
@li Ctrl-Alt-F4 - file/object table dump
@li Ctrl-Alt-F5 - process list dump (+ threads in MT mode)
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
