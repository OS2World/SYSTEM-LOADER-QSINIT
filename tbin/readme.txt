
  =======================================================================
   1. About.
  -----------------------------------------------------------------------

   QSINIT is a some kind of boot loader ;) Basically it load OS/2 kernel ;)
but  suitable  for  many other things - like playing tetris or implementing
boot time disk editor.

   Common  idea of it is creating a small 32 bit protected mode environment
with  runtime  and module execution support. It looks like old good DOS/4GW
apps, but without DOS at all ;)

   QSINIT  uses  OS/4 compatible configuration file (os2ldr.ini), with many
additional  options.  Full list of options is referenced at the end of this
file (section 12).

  =======================================================================
   2. Installation.
  -----------------------------------------------------------------------

 * rename original IBM OS2LDR to OS2LDR.OLD (for example). You can add it into
   loader menu as one of options (with RESTART key).

 * put OS2LDR and QSINIT.LDI to the root of boot drive.

 * create OS2LDR.INI text file, for example:
  
    [config]
    default=1
    timeout=4
    [kernel]
    os2krnl=Default boot
    os2krnl=Show driver names, ALTF2
    OS2LDR.OLD=Original boot, RESTART

 * reboot ;)


   2.1 Single mode installation
  ------------------------------

   QSINIT can be installed to FAT/FAT32/exFAT partition without OS/2 at all.

   Use tool\instboot.cmd in Windows or OS/2 to install QSINIT bootsector and
modules to the selected disk volume.

   You can create QSINIT.INI or OS2LDR.INI in the root of this volume and
fill [config] section parameters with options, required for you.

   And, also, you need to add this volume into your boot manager bootables :)

   Install available in QSINIT shell too, but named "sys". Mount FAT, FAT32
or exFAT partition via "mount" command and then use it.

   Note that QSINIT supports boot from "big floppy" flash drive.

   Loading from CD/DVD in "no emulation" mode is also supported (only for
QSINIT itself), further process is not supported by the OS/2 system.


   2.1.1 Using as simple boot manager
  ------------------------------------

   The same as previous mode, but you are able to create menu with list of
partitions to boot from and force it as "initial menu".

   Fill QSINIT.INI/OS2LDR.INI in the same manner:

    [config]
    default_partition=2
    timeout=4

    [partition]
    hd0/1 = Boot DOS/XP
    hd0/8 = eCS 2.0
    hd0/7 = Suse 12.3

   Then create QSSETUP.CMD in the root of QSINIT boot partition and write to
it line:
    set start_menu = bootpart

   This step is not required if you have only "partition" section and have
no "kernel" one.

   Reboot - menu with 3 systems will be shown :)

  =======================================================================
   3. Details.
  -----------------------------------------------------------------------

   Package  contain  two parts: basic loader (named OS2LDR) and zip archive
with  apps  and data (named QSINIT.LDI). Duaring init this archive unpacked
to  small  virtual disk in memory (for self usage only, this memory will be
available  for  OS/2)  and "system" operates with data on it as with normal
files.

   Installation mode is determining by OS2BOOT file presence in the root of
boot drive (OS/2 is always have it).

   In OS/2 mode QSINIT initiate process of OS/2 boot with a small pause (by
default).  If OS2LDR.INI file is present in the root - menu with additional
options will be shown.
   In single mode - QSINIT apps menu will be shown immediately.

   This  process  can  be  customized  by changing start.cmd file inside of
QSINIT.LDI zip archive. It support a subset of batch file syntax and define
internal init process execution sequence.

   Another (more easy) way is a creating of QSSETUP.CMD file in the root of
boot disk - it will be called by start.cmd just before showing menu.

   OS/2 boot process performed at the same way, as with IBM or OS/4 OS2LDR,
it support both COM port debug and kernel log output.

   Kernel  options  setup  dialog  is available from menu by pressing RIGHT
key. It allow to change most of OS2LDR.INI options for selected kernel, and
kernel file name itself.

   And  how  to  play  tetris?  Press F9 in kernel selection menu to browse
QSINIT apps menu, or type "tetris" in shell. 

   Shell invoked by F3 key.

   Shell is incomplete now, sorry (no complete input/output redirections, CON
file and so on; just remember - this is not a BIG OS).

   Some shell features:
   * "sysview" app include text & binary file editors, log viewer, disk
      management dialog, memory editor, sector-based physical disk editor and
      other options.

      It plays a GUI role, actually :)

   * "mount" command allow to mount any FAT16/FAT32/exFAT partition to QSINIT
      and read/write to it:
          dmgr list hd0     <- list partitions on hd0
          mount 2: hd0 4    <- mount 4th partition of hd0 as drive 2:/C:

      HPFS is available to mount too, but with read-only access.
       
      Kernel from this partition can be selected for boot by typing it 
      full name in kernel options dialog, something like: 
          c:\kernels\os2k_105

    * files can be copied in PXE boot mode from TFTP server to local
      mounted partitions (only with PXEOS4 package):
         copy /boot file_on_tftp local_name 
      or unzipped:
         unzip /boot rev_arch.zip 2:\kernels

    * type "help" to get shell commands list and "help command" or 
      "command /?" to help on single command. Commands have many features,
      which listed only in help ;)

    * "format" command can be used to format partition to FAT16/32/exFAT or
      HPFS. FAT16/32 type selected automatically (by disk size), but you can
      vary cluster size (with including not supported by OS/2 - 64k FAT
      cluster).

      Mount partition to QSINIT before format it.

      All FAT structures will be aligned to 4k (for Advanced Format HDDs).
      This can be turned off by /noaf key.

      HPFS format is fully functional too (note, that QSINIT unable to write
      on HPFS, only read and format it). HPFS requires code page, by default
      850 used, but it can be selected in "chcp" command from a short list of
      available in QSINIT.

    * possibility of launching boot sector from any of primary or logical
      partitions. Command:

        dmgr mbr hd0 boot 1

      will launch partition 1 of disk hd0 (use "dmgr list all" to get partition
      list or use F7 in menu).
      Command:

        dmgr mbr hd2 boot

      will launch MBR from disk hd2.

      These options also available from Disk Management dialog in SysView app.

      Note: all of such boot types starts with A20 gate opened. Modern systems
      have no problem with it, but oldest versions of DOS`s himem can hang.

    * "dmgr pm" command allow to create and delete both primary and logical
      partitions. The same options available from Disk Management dialog in
      SysView app.

      Partitions will be created in OS/2 LVM geometry if it present on this
      disk.

      GPT partition table is supported by this command too and by additional
      command "gpt" for minor operations.

    * "dmgr clone" command can clone partition structure and data to another
      disk.
      It knows nothing about file systems, but simple copying usually can
      be done without changes in file system.

      In case of FAT/exFAT/HPFS/JFS QSINIT updates boot sector of target
      partition after cloning (update information about volume location).

    * "lvm" command can change OS/2 LVM drive letters, write LVM signatures
      to disk and some misc.

      Useful example is "lvm query", which can search partition by LVM text
      name and save its disk name and index into environment variables:

        lvm query "[F32_PT1]" ft_disk ft_index
        mount c: %ft_disk% %ft_index%

    * FAT cache available, QSINIT load it on first "mount" command.
      Cache is non-destructive on power outs (read-ahead only), boot partition
      is not cached at all. Cache size it limited to 1/2 of memory, available
      for QSINIT, but no more 1Gb.

      To manual use, type "cache /?" for help.

      Suitable if you want to copy a large amount of data in QSINIT.

    * to copy those "large amount of data" use xcopy.cmd in shell ;)

    * "chcp" command can be used to select codepage for FAT/FAT32/exFAT
      (single byte only, no DBCS now, sorry). Without this command any files
      with national language letters will be unaccessible.
      note: this command only makes these names operable, not displayable.

    * "mem hide" command can be used to hide some memory ranges from
      OS/2 (for example, if memory tests show errors in this range)

    * more of it in the section 5 below.

   QSINIT can be loaded by "WorkSpace On-Demand" PXE loader, but further
loading process is untested.

   And a little note about QSINIT internal disks and it`s naming. QSINIT
use ChaN`s FatFs code to access FAT/FAT32/exFAT partitions and follow its
naming convention: all internal disks has an additional names 0:, 1:,
2: .. 9:.  A:-J: naming is supported as well.  Shell accept both variants,
apps based on Turbo Vision library - accept A:-J: letters only.

   Virtual disk with QSINIT apps and data is always mapped to drive B:/1:

   On FAT/FAT32/exFAT boot - boot partition is available as drive A:/0: and
you can modify it freely (edit text files, copy files in shell and so on...).

   HPFS file i/o is supported, but read-only.

   JFS on the boot partition is mapped to the drive A:/0: too, but only for
per-sector access, because QSINIT does not know this filesystem. But drive
letter still can be used somewhere, for example to show boot partition BPB:

      dmgr bs 0: bpb

   Other 8 disk numbers/letters ( C:/2: - J:/9: ) are free to mount of any
FAT/FAT32/exFAT/HPFS partition in the system.

  =======================================================================
   4. Additional customization.
  -----------------------------------------------------------------------

   4.1 Boot menu 
  ---------------

   Boot  menu  can be altered by editing meni.ini file inside of QSINIT.LDI
archive.  Location  of  this  file  can  be  overloaded by menu_source env.
variable

      rem on FAT boot
      set menu_source = 0:\OS2\menu.ini

   Or menu can be copied from the root of FSD boot disk - in qssetup.cmd

      copy /boot menu.ini b:\menu.ini

   This allow to make an easy changes in menu without editing of QSINIT.LDI
archive.

   4.2 Switching to simple "boot manager" mode.
  ----------------------------------------------

   By adding this line into qssetup.cmd:
      
      set start_menu = bootpart

   you  can  switch  QSINIT to some kind of boot manager. This setting will
select as initial menu list of partitions, defined in "[partition]" section
of OS2LDR.INI file (see notes below).

   4.3 Graphic console.
  ----------------------

   It is possible to use "graphic console" (console emulation in vesa modes).

      mode con list
     
   will show list of available modes.
   To set mode use

      mode con cols=x lines=y
   or
      mode con id=mode_id

   Maximum monitor resolution can be specified by adding an environment string
into qssetup.cmd, for example (DDC is not used now):

      set vesa = on, maxw=1024, maxh=768

   QSINIT can be instructed to not touch VESA at all by the same option in
qssetup.cmd:

      set vesa = off

   (this will break VMTRR too, because it queries video memory address in it).

   "Graphic console" is much faster if you turn on write combining.

   Common syntax of this setup string is:
     set vesa = on/off [, maxw=..] [, maxh=..] [, nofb][, fbaddr=hex]

   NOFB parameter disable using of video memory`s linear frame pointer. This
cause old bank switching method on VESA and EFI graphic functions usage in
EFI build. FBADDR affects EFI version only and forces video memory address
to this value.

   It is possible to use own fonts (binary file with 1 bit per pixel - i.e.
8x16 x 256 chars will be 1 byte (8 bits) x 16 x 256 = 4096 bytes). Every
added font will add a number of graphic console modes (one for every available
graphic mode).

   Note, that supplied with QSINIT 10x20 & 8x14 fonts have 866 codepage.

   4.4 Serial port console.
  --------------------------

   QSINIT supports serial port console (fixed to be 80x25 now).

   Use "SM VADD" command to add 16550-compatible COM port as a new console
device and then "SM ATTACH" to attach it to any active session (single session
in default QSINIT mode and any running session(s) in MT (multithread) mode).

   QSINIT simulates ANSI/VT100 terminal and should work fine with most of
terminal programs.

   Ctrl-Alt-F1 hotkey will switch current debug serial port into the such
console and attach current on-screen session to it (if it works in 80x25),
pressing it again will return log output back.

   Via SM command you can set up a real multi-terminal environment, where
each session will have own console (default display and several serial port
connections).

   4.5 Size of QSINIT.LDI file.
  ------------------------------

   Some modules can be removed from QSINIT.LDI archive without affecting
base functionality (when it must fit into diskette image for CD boot, for
example).
   cache.dll   - used only for caching of FAT volumes (if cruel life forces
                 you to use QSINIT shell as copy utility ;)
   cplib.dll   - CHCP command, this module provides code pages for FAT/exFAT
                 and for HPFS formatting.
   fslib.dll   - HPFS r/o support
   vdisk.dll   - PAE ramdisk
   vhdd.dll    - VHDD shell command
   mtlib.dll   - threads, sessions (see below, not required for OS/2 boot)
   console.dll - graphic console, "mode con" command and graphic memory
                 information (required for VMTRR)
   msg\sysview.hlp - small help file for SYSVIEW, can be deleted too

   FNT files in MSG dir - custom fonts for graphic console (both in 866
codepage) - required for EFI build, basically (where own EFI console`s speed
is a nightmare). They provide 80x30 and 100x42 modes, selectable in QSINIT
menu.

   Also, you can remove games, of course... Finally, the only things you
need to boot is BOOTMENU.EXE, MENU.INI, BOOTOS2.EXE, START.* and OS2BOOT
directory.

   4.6 ArcaOS loader OS2LDR.CFG file.
  ------------------------------------

   QSINIT supports Arca OS loader configuration file. This file is used, at
least for HIDEMEM, MEMLIMIT, LOGSIZE, DBCARD, DBPORT, RAMDISKNAME and RAMDISK
options.
   But own QSINIT settings have a priority above options, listed above.

   This mean, that OS2LDR.INI parameter will override LOGSIZE value.
   
   MEMLIMIT may be used only if it smaller, than limit, found during QSINIT
processing - from OS2LDR.INI or rare VMTRR cut of a small part of 4th Gb.

   RAMDISK and RAMDISKNAME keys will affect only if NO ramdisk present (i.e.
no RAMDISK command was used in qssetup.cmd).

   DBCARD or DBPORT options will be used if no debug COM port in use (i.e.
OS2LDR.INI does not define it).

   To remove OS2DLR.CFG processing - just delete file AOSCFG.EXE from the
QSINIT.LDI archive.

  =======================================================================
   5. Several non-evident features.
  -----------------------------------------------------------------------

   5.1 MTRR registers setup.
  ---------------------------

   "mtrr" command allow to edit MTRR registers directly. You must be very
familiar with Intel architecture to do this ;) Edited values will be used
for OS/2 (including SMP) - by default or you may drop it by "mtrr reset"
command or NOMTRR os2ldr.ini key.

   You can add mtrr setup into qssetup.cmd, for example (see above).

   Or you can use vmtrr utilite - it try do detect video memory size and
setup Write Combine automatically. Call it from settings menu or add "vmtrr"
string into qssetup.cmd.

   Single note: QSINIT must be used to load kernel file after VMTRR or MTRR
setup - else it leave "Write Combine" for 1st CPU only. It must catch other
CPUs start and setup MTRR registers in the same way - but can do this only
if it "main loader" (i.e. not launcher of another loader via RESTART option).

   Write Combine can be set for text mode video memory too, but only on AMD
cards (Intel and NV go crazy on this ;)). Command is:

      mtrr fixed 0xB8000 0x8000 WC

   5.2 Boot partition changing.
  ------------------------------

   One of non-obvious features is on-fly boot partition changing, without
booting from boot sector (this works with FAT & HPFS only).

   Default HPFS boot sector code depends on OS/2 MBR code or VPart/AirBoot.
If both is absent - it can fail. In this case such emergency way can be used.

   At first, mount partition to QSINIT drive letter (ex. D:), then type in
   shell:

      bootos2 os2krnl source=d

   and system will start from this partition, without use of boot sector code.

   This mean that you can load QSINIT from flash drive with FAT32 and then
switch boot to HPFS partition on HDD, for example.

   5.3 "Dirty" flag setup.
  -------------------------

   "dmgr" command has an ability to set/reset "dirty" flag on HPFS and FAT
partitions. For example, command
        
        dmgr bs a: dirty on

    will force CHKDSK on current boot volume during system startup.

    But note, it looks like Windows is ignoring dirty flag on exFAT now and
supports "dirty" for FAT/FAT32 only.

   5.4 "Kernel browser".
  -----------------------

   "IMPORT" button inside of kernel options dialog (invoked by "right" key
in the kernel selection menu) - will launch "kernel browser". This option is
available for intensive testing of a large number of different kernel files.

   To use this feature you should pack kernel files into REV_ARCH.ZIP file
and then put it into the root of boot drive. After that, add DISKSIZE=2048
to OS2LDR.INI "config" section.

   This ZIP will be loaded by pressing "IMPORT" button and any file from it
can be selected as a kernel for current boot. This method works on FAT, HPFS
and PXE, but may fail on old-formatted JFS (just try newer build`s sysinstx
on it first).

   ZIP file name can be overridden by REV_ARCH_PATH environment variable
(add set REV_ARCH_PATH = ... into qssetup.cmd - see note above about it).

  =======================================================================
   6. Documented bugs (aka features).
  -----------------------------------------------------------------------

 * loading of binary files into SysView text editor can cause panic (because
   of reaching 16k characters per line limit). This is limitation of 
   old-designed Turbo Vision library.

 * famous 512Mb problem on EFI.
   QSINIT have NO problem with it, if it used for kernel boot. It will show
   all 4Gb.

   If QSINIT only loads original IBM`s OS2LDR - problem will stay in place,
   because it loader-based (i.e. loader prepares system memory for kernel).

 * using VMs to test QSINIT.
   VPC & Vbox works, in most cases, with some exceptions:

    - no PAE paging mode in VPC. Hardware virtualization must be off too -
      it kills host OS in seconds, at least in VirtualPC 2007 on AMD.

    - 8, 12, 13 exceptions stops VPC VM (because of task gates used).
      Add "NOTASK=1" to OS2LDR.INI to disable task gates in this case.

    - VPC does not support multitasking (no APIC timer in it).

    - VBox is much better, except PAE paging mode - it looks unstable
      (at least in VBox 4.x).

    - in some VBox 5.x versions QSINIT on start hangs into INT 4 interrupt.
      This is VBox bug in CPU _into_ command processing, just update QSINIT
      to more fresh version.

   EFI version works in QEMU only (with TianoCore), but even here small bugs
   still possible (at least CPU _xlat_ command cause troubles in amd64
   compatible mode emulation).

 * note, that huge USB HDDs can be non-operable via BIOS, especially on
   old motherboards. Only a starting part of disk can be readed, up to 128Gb,
   with garbage returned above this border. Looks like all ASUS motherboards,
   prior to EFI, have this bug.

 * write access to exFAT still looks dangerous on huge files - this is own
   FatFs lib troubles.

 * some alternatively gifted BIOS coders (HP laptops, for example) does not
   return disk size information at all. This should not affect to OS/2 boot,
   but for using this disk in QSINIT, disk size must be detected.

   "Detect size" option available in Disk Management dialog in SysView as well
   as "dmgr mode" command is able to set number of sectors directly.

 * pressing LEFT SHIFT key on loading will cause skipping of QSSETUP.CMD and
   direct start of QSINIT shell (cmd.exe). This is some kind of emergency mode
   which allow to skip any additional processing (like MTRR setup) - if this
   processing stops QSINIT boot.

   Also, COM port output is off in this mode to prevent locking on missing
   port / incorrect port address in os2ldr.ini.

   [[Loading]] message is red in this mode. If it green then you are too
   slow :)

  =======================================================================
   7. QSINIT boot details.
  -----------------------------------------------------------------------

   QSINIT can be loaded:

   * via OS/2 boot FSD mechanism - i.e. QSINIT loader looks like OS2LDR for
     micro-FSD code.
     Files OS2LDR and QSINIT.LDI just placed to the root of boot disk.

   * via OS/2 PXE boot - the same.
     Files OS2LDR and QSINIT.LDI must be accessed from TFTP.

   * from FAT12/FAT16/FAT32/exFAT - by using OWN boot sector. Boot sector can
     be installed by tool\bootset? command.
     Boot sector assume QSINIT file name for base loader (not OS2LDR as on
     FSD mechanism).

     I.e. files QSINIT and QSINIT.LDI must be in the root of boot disk.

     This mechanism must work on HDD/floppy/flash drives, really ;)

   * from CD/DVD in "no-emulation" mode. In this case bootstrap code of
     CD disk is a micro-FSD. Look into CDBOOT directory for details.

   QSINIT provide own MBR code both for MBR and GPT partition tables. It is
optional and can be installed to empty disk, for example, or in emergency
case.

   Common MBR code have no differences with other ones. It loads active
partition and launch it.
   In addition - it have special Boot Manager support (as OS/2 MBR).

   MBR code for GPT disks searches for 1st partition with "BIOS Bootable"
attribute ON (google UEFI Wiki about it) and then launch it. But, if partition
is completely above 2Tb border - only exFAT supports booting from it.

   You can set "BIOS Bootable" attribute via "gpt" command or "sysview" app.

   As example, you can create "dual boot" flash/hdd drive:

   * init it to GPT in QSINIT
   * create two partitions, set type of 1st to "EFI boot"
   * format both to FAT
   * copy to first \EFI\BOOT\BOOTX64.EFI - something like EFI shell or
     EFI version of QSINIT.
   * set up second as QSINIT boot partition (set "BIOS Bootable" attribute
     and install QSINIT to it)
   * then you can choose in EFI BIOS boot devices menu: "YOUR DRIVE" or 
     "UEFI: YOUR DRIVE", 1st will be QSINIT and 2nd - EFI boot.

   QSINIT also provides own code for FAT/FAT32/exFAT/HPFS boot record. This
code not depends on Boot Manager and OS/2 MBR code. You can install it on
specified partition by mounting it to QSINIT and then use "dmgr bs" command
in shell (or just in the "Disk management" menu in sysview).
 
   You can learn MBR/boot sector code details in 

      \QS\system\src\ldrapps\partmgr\se.asm
      \QS\system\src\tools\src\cdboot\cdboot.asm

   in QS_SDK.ZIP.

   This is funny, but FAT bootsector code is able to load Windows XP NTLDR
too. This is the legacy of time, when MS & IBM were friends.

  =======================================================================
   8. Log.
  -----------------------------------------------------------------------

   You can send me QSINIT log in case of repeatable troubles ;)
   Log can be saved in some different ways:

     * via COM port cable (DBPORT/DBCARD options in OS2LDR.INI)

     * by adding LOGLEVEL option into OS2LDR.INI (see below). In this case
       QSINIT log will be merged with OS/4-compatible kernel log and can
       be readed as it:
         copy oemhlp$ logfile.txt  - without ACPI.PSD
         copy ___hlp$ logfile.txt  - with ACPI.PSD
       This case is NOT working if RESTART option was used for kernel loading
       (i.e. another OS2LDR is running).

     * mount any available FAT16/FAT32/exFAT partition to QSINIT and save
       log directly to it. This can be done in SysView app or by using LOG
       shell command.
       
  =======================================================================
   9. PAE ram disk (hd4disk.add basedev driver).
  -----------------------------------------------------------------------

   QSINIT can use memory above 4GB border (unlike OS/2 kernel :)).

   RAMDISK  command  in  shell  create  virtual ram disk in this memory (or
memory below 4GB - if specified so). Type RAMDISK /? for help.

   This  disk  will  be  visible  in  OS/2  like another one HDD - just add
HD4DISK.ADD to CONFIG.SYS and put this file to ?:\OS2\BOOT

   There is one limitation - you MUST use QSINIT as boot loader, i.e., load
KERNEL  file  by  him,  not  another  one  loader (IBM or OS/4 with RESTART
option).
   The reason - ADD driver must ask QSINIT about disk location.

   By default this HDD contain one primary partition, formatted with FAT or
FAT32 (depends on size). You can format it into HPFS too. Variants available
via RAMDISK command keys or manual partition creation by DMGR command.

   You can put swap file to this disk :)

   For example, add to QSSETUP.CMD:

       ramdisk /q nohigh min=500 z:

   This  command  will quietly create 500Mb HDD in memory below 4Gb and set
Z: LVM drive letter to it.

   Read more in ramdisk\paedisk.txt.

   If you already using HD4DISK.ADD prior to rev.309 - please update it to
the latest version.

  =======================================================================
  10. Multithreading.
  -----------------------------------------------------------------------
   Latest QSINIT revisions have thread support inside :)
   
   By default, this mode is OFF and QSINIT still an old good single-thread
DOS-like environment.

   "Mode sys mt" command (or API call) will switch it to more OS-like mode,
where threads and sessions are available. START and DETACH shell commands
will also turn this mode on use.

    There is no pretty "task list" and session kill now, only session
switching by Alt-Esc key (Ctrl-N in serial port console). But it still
useful for some long actions, for example copying of huge zip file from
TFTP server (PXE boot):

     detach copy /boot /beep huge_file_via_pxe.zip c:\

   Another one advancement is using of "hlt" during idle time, i.e. QSINIT
in MT mode will save power/CPU temperature.

   Most of QSINIT code is thread safe now.

   Multithreading is far from ideal. For example, long file write operations
can cause huge delays for other threads, but, hey!! - this is not a premium
desktop OS :)
   At least, you can play tetris while second session searches data on the big
HDD - this is real.

   Additional setup is available, by SET string in qssetup.cmd:

     set mtlib = on/off [, timeres=..]
     set kbres = value

   where OFF parameter will disable MT mode at all, TIMERES sets thread "tick"
time, in milliseconds (default is 16 (4 in EFI build)) and KBRES is keyboard
polling period (default is 18 ms (8 in EFI)).

   And another one forgotten MT mode advancement - all QSINIT Ctrl-Alt-Fx
hotkeys (various dumps) works in it at any time.


  =======================================================================
  11. Supported kernels
  -----------------------------------------------------------------------
   Warp 3 and Merlin (up to FP12) kernels can be loaded as well as Aurora
type kernels. Memory for such kernels is limited to 1Gb (if anybody knows
real limit  -  please, tell me ;) Warp Server SMP kernel is not supported.
This is possible, but who really need it?

   Kernel type is determined automatically.

   OS/4 kernel requires additional processing as well. It is difficult a
bit to support it, because this is closed-source project. But, at least now,
QSINIT should load any revision of it.

  =======================================================================
  12. OS2LDR.INI file format.
  -----------------------------------------------------------------------

   OS2LDR.INI format is simple:

      [config]
      ; default kernel index (1..x) for kernel menu ([kernel] section below)
      default=1
      ; default index (1..x) for partition boot menu
      default_partition = 4
      ; timeout, in sec - to wait before default item will be started
      timeout=4
   
      ; common options
      dbport=0x3F8
      ...
   
      [kernel]
      <kernel file name> = menu string [, option, option ...]
      ...
      OS2KRNL = Common boot
      OS2KRNL = Common boot with Alt-F2, ALTF2
   
      [partition]
      disk/partition [, boot sector file] = menu string [, option, option ...]
      ...
      hd0/0 = Boot 1st partition of disk 0
      hd0/0,bootsect.dos = The same but use bootsect.dos file as a boot sector
      hd1/0 = Boot 1nd partition of disk 1
      hd2   = Boot MBR of disk 2 (with MTRR reset), nomtrr

      ; comments supported only on 1st posistion of the line
      
   A new section "[partition]" is available in OS2LDR.INI. It defines a list
of partitions to boot and available from "Boot partition" menu in QSINIT.
   This menu will also be selected as default if no "[kernel]" available.

   OS2LDR.INI can be shared with OS/4 loader, but the last one ignores unknown
sections, so this is not a problem.

   Partition indexes for "[partition]" can be queried by pressing F7 in menu.
   Boot sector file can be specified by direct QSINIT path or will be searched
in the root of this partition (any FAT type).

   TIMEOUT and USEBEEP parameters affect [partition] menu too (timeout counter
occurs during first launch only).
   Options for boot partition menu can be added to menu line only. Only two
of them is supported now: CPUCLOCK and NOMTRR.


   Alphabetical list of OS2LDR.INI options:

 * ALTE      - (OS/4) invoke embedded config.sys editor during kernel startup.

 * ALTF1..ALTF5 - simulate Alt-F1..Alt-F5 keypress for kernel.

 * BAUDRATE  - debug COM port baud rate (115200 by default). BAUDRATE in 
               "config" section will affect both QSINIT and OS/2 kernel, in
               kernel parameters line - kernel only. 
               Supported values: 150,300,600,1200,2400,4800,9600,19200,38400,
               57600 and 115200.

 * CALL=batch_file - call QSINIT batch file just before starting kernel.
               "batch_file" must be a fully qualified path in QSINIT!
               I.e. A:\BATCH.CMD (boot disk, possible on FAT boot only), or
               C:\dir1\batch.cmd (from mounted volume).
               VIEWMEM (if specified) will be called after this batch file and
               then kernel will be launched.

               At this CALL time kernel has loaded and fixuped in their memory
               location, but QSINIT still fully functional. This is possible
               because QSINIT never touches kernel memory areas.

 * CFGEXT=EXT - use CONFIG.EXT file instead of CONFIG.SYS. File must be placed
               in the root directory. EXT - extension, up to 3 chars.

 * CHSONLY   - ansient HDDs supports (switch BIOS disk i/o to CHS mode during
               kernel boot).
               This mode can also be set by DMGR MODE command in QSINIT shell.

 * CPUCLOCK=1..16 - setup clock modulation on Intel CPUs. 1 = 6.25% of speed,
               16 = 100%. On middle aged processors actual step is 12.5% (it
               will be rounded automatically).

               Clock modulation available on P4 and later CPUs. It can be
               used to slow down DOS, launched from partition menu, for
               example. With 12.5% on 3100 MHz Core Duo it shows ~400-600 Mhz
               in DOS performance tests. This can be usefull for some old
               games, at least.

               Or you can look how badly you optimized your software code,
               when boot OS/2 on 1/6 of default frequency.

               This key works both in OS/2 boot and partitions menu.

 * CTRLC=1   - (OS/4, >=2970) enable Ctrl-C check for kernel debugger. This
               options slows the system reasonably. It fources kernel debugger
               to check for incoming Ctrl-C via serial port periodically.

 * DBCARD = bus.slot.func,port_index  or
   DBCARD = vendor:device,[index,]port_index - query address of PCI COM port
               (specified "i/o_port_index" in it) and use it as debug COM
               port. In addition, this command will enable device i/o if it
               turned off by BIOS or UEFI.
               
               Optional "index" (1..x) can be used if your adaper mapped on
               multiple functions or you have two or more adapters with the
               same vendor:device ID.

               port_index - index of connection port in the list of card
               address ranges. It looks like this (in PCI.EXE output):
                   Address 0 is an I/O Port : 0000EC00h
                   Address 1 is an I/O Port : 0000E880h
                   Address 2 is an I/O Port : 0000E800h
               But value is 1-based, not zero.

               Example:
                  we have NetMos 9710:9835 on 4.2.0:

               DBCARD=9710:9835,3         <- port E800h
               DBCARD=4.2.0,2             <- port E880h

 * DBPORT=addr - debug COM port address. Dec or 0xHEX value.
               This option turns COM port ON earlier, than DBCARD, but DBCARD
               is less dependent on PCI configuration changes.

 * DEFMSG    - use OS2LDR.MSG file instead of embedded messages.

 * DISKSIZE=value - additional space in QSINIT "virtual disk", kb. Required
               for "kernel browser" option above.

 * FULLCABLE=1 - use hardware flow control for COM port cable (requires full
               cable, off by default).

 * LETTER=X  - allow to change boot drive letter for OS/2.
               This option only changes letter for later boot, not partition.
               Wrong letter sometimes can be supplied by Boot Manager.

 * LOADSYM   - (OS/4, >=4058) load SYM file by loader, not kernel.

 * LOGSIZE=value - set log size for resident part of loader, in kb. This value
               will rounded down to nearest 64k.
               Boot log is avalable on OS/2 user level:
                  "copy ___hlp$ boot.log"  - with ACPI.PSD running
                  "copy oemhlp$ boot.log"  - without ACPI.PSD

 * LOGLEVEL=0..3 - maximim level of QSINIT log messages to copy to OS/2 kernel
               log. By default - messages is not copied at all.

 * LOGOMASK=value - (OS/4) bit mask to disable vesa mode usage for new logo.
               Bit 0 - 8 bit color, 1 - 15bit, 2 - 16bit, 3 - 24bit, 4 - 32bit
               I.e. value of 31 (or 0x1F) - all modes enabled

 * MEMLIMIT=value - limit available memory for OS/2 to specified value in Mb.
               Where value >=16 & less than total available memory below 4Gb.

 * MENUPALETTE=value - color values for kernel selection menu: byte 0 - text
               color, 1 - selected row color, 2 - background, 3 - border.
               Default value is 0x0F070F01.

 * MFSDSYM=name - (OS/4, >=4058) use "name" as a SYM file for mini-FSD.

 * MFSPRINT=1  allow usage of BIOS screen and keyboard services by micro-FSD
               code (filesystem bootstrap).

               By default QSINIT blocks them during read from boot device. This
               cause elimination of JFS boot copyright message (a kind of
               garbage on screen) and deactivate PXE BIOS keyboard polling for
               ESC key (i.e., PXE BIOS stucks keyboard until the end of file
               load).

               Turning this option on is actual only for someone, who writes
               micro-FSD :)

 * NOAF      - turn OFF Advanced Format aligning for disk read/write ops. 
               Affect both QSINIT and OS/2 boot (until IDE/SATA driver load).
               By default most of long i/o ops aligned to 4k.

 * NOCLOCK=1 - turn off clock in QSINIT menus

 * NODBCS    - disable DBCS file loading (OS2DBCS, OS2DBCS.FNT). Option saves
               a small amount of memory in 1st Mb for drivers on DBCS systems
               and has no any other special effects (except missing graphic
               console on early boot stage).

 * NOLOGO    - do not show kernel logo.

 * NOMTRR    - do not use MTRR changes, was done in QSINIT for OS/2 boot.

               This key works in partition menu too.
               This key can be actual for NTLDR`s graphic mode (with menu
               on national language).

 * NOREV     - do not show kernel revision.

 * PCISCAN_ALL=1 - scan all 256 PCI buses. By default QSINIT scan only number
               of buses, reported by PCI BIOS. This key can be really slooooow
               on old PCs (2 minutes on my PPro).

 * PRELOAD=1 - (OS/4, >=2075) preload all files for BASEDEV boot stage before
               starting to launch them (as Windows do).
               See detailed reference in OS/4 documentation.

 * REIPL=value - reboot PC after fatal QSINIT errors (traps). value - number
               of seconds before reboot (>=1). By default - off.

 * RESETMODE=1 - This option will reset display mode to 80x25 before menu or
               reset it to selected number of lines (80x25, 80x43, 80x50).
               Number of text lines can typed instead of 1: RESETMODE=25,
               RESETMODE=43 or RESETMODE=50.

 * RESTART   - this option turns off ANY other options and loads file in
               kernel selection menu as another OS2LDR (loader, not a kernel).

               I.e. this is "restart loader" option.
               It does not work for PXE boot (ask Moveton why).

               It able to load Windows XP NTLDR on FAT16 partition :) As said
               above, this is the legacy of time, when MS & IBM were friends.

 * SOURCE=X  - changes boot partition for OS/2 boot, where X - QSINIT drive
               letter of mounted (to QSINIT!) FAT or HPFS partition. I.e. this
               key can change boot partition to any available FAT or HPFS with
               OS/2 - without reboot.
               Basically, it designed for booting from PAE RAM disk, but works
               in common case too.

 * SYM=name  - use "name" as SYM file for DEBUG kernel.
               Filename is up to 11 chars length (7.3, limited by patching
               of original IBM kernels)

 * UNZALL=1  - unpack all modules from QSINIT.LDI to virtual disk duaring init.
               By default, EXE/DLL modules only reserve space on disk - until
               first usage. Option suitable to check QSINIT.LDI consistency on
               every boot (unstable PXE connection or devices with bad sectors).

 * USEBEEP=1 - turn PC speaker sound in kernel selection menu (for monitor-less
               configurations). It will beep with one tone on first line and
               another tone on any other.

 * VALIMIT[=value] (OS/4, >=4199) VIRTUALADDRESSLIMIT value, in Mb.
               Key can be used without memory size value, in this case value
               of VIRTUALADDRESSLIMIT in CONFIG.SYS will be used (actual for
               later OS/4 kernels only, where VIRTUALADDRESSLIMIT is absent).

 * VIEWMEM   - open memory editor just before starting kernel (when it ready to
               launch).


   In  OS2LDR.INI  only  DBPORT,  DBCARD, DISKSIZE, NOAF, BAUDRATE, UNZALL,
USEBEEP, PCISCAN_ALL, MFSPRINT, REIPL and RESETMODE keys affect QSINIT, any
other is used for OS/2 kernel menu/boot only.

   LETTER,  LOGLEVEL, NOAF, CPUCLOCK, NOMTRR and BAUDRATE can be added both
to  kernel  parameters  line  and to "config" section; DISKSIZE, RESETMODE,
USEBEEP,  NOCLOCK,  UNZALL,  DBCARD and PCISCAN_ALL have effect in "config"
section only.

   File also can be named as QSINIT.INI - and this name has a priority. This
allows to solve possible problems with OS/4 loader.


   Other differences:

 * IBM OS2LDR checks all HDDs in system for real support of Int13x API 
   (long disk read). Such check is not implemented in QSINIT, to avoid
   problems with huge USB HDDs.

 * OS/4 loader options SHAREIRQ and DBFLAGS ignored.

 * both IBM and OS/4 OS2LDR make PCI BIOS calls. QSINIT uses PCI BIOS for
   basic info only (version and # of buses in system), all scan, read and
   write operations performed internally.

 * OEMHLP$ driver (loader IS this driver) use much less memory in 1st Mb.

   Min. system requirements: 80486DX, 24Mb of memory (first 16 is reserved for
   OS/2 boot).

   Small article on russian is available on RU/2, you can try to google
   translate it (at least half of text will be readable ;)
      http://ru2.halfos.ru/core/articles/qsinit

   Made on Earth,
   Enjoy ;) and
   check ftp://212.12.30.18/public/QS for new versions and SDK ;)

   QSINIT source code available in SDK archive.

   PXEOS4 package: ftp://212.12.30.18/public/pxeos4-1.130817.zip
     newer versions here: http://moveton.tk/files

   Author: dixie, e-dixie@mail.ru
