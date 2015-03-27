
  =======================================================================
   1. About.
  -----------------------------------------------------------------------

   QSINIT is a some kind of boot loader ;) Basically it load OS/2 kernel ;)
but  suitable  for  many other things - like playing tetris or implementing
boot time disk editor.

   Common  idea of it is creating a small 32 bit protected mode environment
with  runtime  and module execution support. It looks like old good DOS/4GW
apps, but without DOS at all ;)

   QSINIT  supports all options, available in well known OS/4 kernel loader
ini  file  (os2ldr.ini).  Docs for it available in OS/4 kernel archive (for
example,  here:  http://ru2.halfos.ru/core/downloads/).  Some additions and
differences referenced at the end of readme. And unlike OS/4 loader, QSINIT
has able to load OS/2 Warp type kernels (all kernels prior to Merlin FP13).

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

   QSINIT can be installed to FAT/FAT32 partition without OS/2 at all.

   Use tool\instboot.cmd in Windows or OS/2 to install QSINIT bootsector and
modules to the selected disk volume.

   You can create QSINIT.INI or OS2LDR.INI in the root of this volume and
fill [config] section parameters with options, required for you.

   And, also, you need to add this volume into your boot manager bootables :)

   Install available in QSINIT shell too, but named sys.cmd. Mount FAT/FAT32
partition via "mount" command and then use it.

   Note that QSINIT supports boot from "big floppy" flash drive.


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

   Another (more easy) way is a creating of file qssetup.cmd in the root of
boot disk - it will be called by start.cmd just before showing menu.

   OS/2 boot process performed at the same way, as with IBM or OS/4 OS2LDR,
it support both COM port debug and kernel log output.

   Kernel  options  setup  dialog  is available from menu by pressing RIGHT
key. It allow to change most of OS2LDR.INI options for selected kernel, and
kernel file name itself.

   And  how  to  play  tetris?  Press F9 in kernel selection menu to browse
QSINIT apps menu, or type "tetris" in shell. 

   Shell invoked by F3 key.

   Shell  is  incomplete now, sorry (no input/output redirections, CON file
and so on; just remember - this is not a BIG OS).

   Some shell features:
   * "sysview" app include file editor, log viewer, disk management, sector-
      based physical disk editor and other options.

   * "mount" command allow to mount any FAT16/FAT32 partition to QSINIT
      and read/write to it:
          dmgr list hd0     <- list partitions on hd0
          mount 2: hd0 4    <- mount 4th partition of hd0 as drive 2:/C:
       
      Kernel from this partition can be selected for boot by typing it 
      full name in kernel options dialog, something like: 
          c:\kernels\os2k_105

    * files can be copied in PXE boot mode from TFTP server to local
      mounted partitions (only with PXEOS4 package):
         copy /boot file_on_tftp local_name 
      or unzipped:
         unzip /boot rev_arch.zip 2:\kernels

    * "mtrr" command allow to edit MTRR registers directly. You must be
      very familiar with Intel architecture to do this ;) Edited values
      will be used for OS/2 (including SMP) - by default or you may drop
      it by "mtrr reset" command or NOMTRR os2ldr.ini key.

      You can add mtrr setup into qssetup.cmd, for example (see above).

      Or you can use vmtrr utilite - it try do detect video memory size
      and setup Write Combine automatically. Call it from settings menu
      or add "vmtrr" string into qssetup.cmd.

      One note: QSINIT must be used to load kernel file after VMTRR or
      MTRR setup - else it leave "Write Combine" for 1st CPU only. It
      must catch other CPUs start and setup MTRR registers in the same
      way - but can do this only if it "main loader" (i.e. not launcher
      of another loader via RESTART option).

      Write Combine can be set for text mode video memory too, but only
      on ATI cards (Intel and NV go crazy on this ;)). Command is:

        mtrr fixed 0xB8000 0x8000 WC

    * pressing LEFT SHIFT key on loading will cause skipping of start.cmd
      and direct start of QSINIT shell (cmd.exe). This is some kind of 
      emergency mode which allow to skip all additional processing (like
      MTRR setup) - if this processing stops QSINIT boot.

      Also, COM port output is off in this mode to prevent locking on
      missing COM port / incorrect COM port address in os2ldr.ini.

      [[Loading]] message must be red in this mode.

    * type "help" to get shell commands list and "help command" or 
      "command /?" to help on single command. Commands have many features,
      which listed only in help ;)

    * "format" command can be used to format partition to FAT16/32 or HPFS.
      FAT type selected automatically (by disk size), but you can vary
      cluster size (with including not supported by OS/2 - 64k FAT cluster).

      Mount partition to QSINIT before format it.

      All FAT structures will be aligned to 4k (for Advanced Format HDDs).
      This can be turned off by /noaf key.

      HPFS format is fully functional too (note, that QSINIT unable to read
      and write HPFS, only able to format it). HPFS requires code page, by
      default 850 used, but it can be selected in "chcp" command from a short
      list of available in QSINIT.

    * possibility of launching boot sector from any of primary or logical
      partitions. Command:

        dmgr mbr hd0 boot 1

      will launch partition 1 of disk hd0 (use "dmgr list all" to get partition
      list or use F7 in menu).
      Command:

        dmgr mbr hd2 boot

      will launch MBR from disk hd2.

      These options also available from Disk Management dialog in SysView app.

      Please note: all of such boot types starts in non-standard environment -
      low 8 hardware interrupts remapped to 50h and A20 gate is open. Modern
      systems have no problem with such mode, but old versions of DOS`s himem
      can hang.

    * "dmgr pm" command allow to create and delete both primary and logical
      partitions. This functionality is in ALPHA stage!
      The same options available from Disk Management dialog in SysView app.

      Partitions will be created in OS/2 LVM geometry if it present on this
      disk.

      GPT partition table is supported by this command too and by additional
      command "gpt" for minor operations.

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

      Suitable if you want to copy a large amount of data between FAT32
      partitions in QSINIT.

    * to copy those "large amount of data" use xcopy.cmd in shell ;)

    * "chcp" command can be used to select codepage for FAT/FAT32 (single
      byte only, no DBCS now, sorry). Without this command any files with
      national language letters will be unaccessible.
      note: this command only makes these names operable, not displayable.

    * "mem hide" command can be used to hide some memory ranges from
      OS/2 (for example, if memory tests show errors in this range)

   QSINIT can be loaded by "WorkSpace On-Demand" PXE loader, but further
loading process is untested.

   And a little note about QSINIT internal disks and it`s naming. QSINIT
use fine ChaN`s FatFs code to access FAT/FAT32 partitions and follow its
naming convention: all internal disks has named as 0:, 1:, 2: .. 9:

   For compatibility A:-J: naming is supported as well.  Shell accept both
variants, apps based on Turbo Vision library - accept A:-J: letters only.

   Virtual disk with QSINIT apps and data is always mapped as drive 1:/B:

   On FAT/FAT32 boot - boot partition is available as drive 0:/A: and you
can modify it freely (edit config.sys, copy files in shell and so on...).

   On FSD boot drive 0:/A: is mapped to boot partition too, but only for
per-sector  access,  because QSINIT does not support full HPFS/JFS reading.
But  drive  letter  still  can  be used somewhere, for example to show boot
partition BPB:

      dmgr bs 0: bpb

   Other 8 disk numbers/letters ( 2:/C: - 9:/J: ) are free to mount of any
FAT/FAT32 partitions in system.

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

      copy /boot menu.ini 1:\menu.ini

   This allow to make an easy changes in menu without editing archive.

   4.2 Switching to simple "boot manager" mode.
  ----------------------------------------------

   By adding this line into qssetup.cmd:
      
      set start_menu = bootpart

   you  can  switch  QSINIT to some kind of boot manager. This setting will
select as initial menu list of partitions, defined in "[partition]" section
of OS2LDR.INI file (see notes below).

   4.3 Graphic console.
  ----------------------

   It is possible to use "graphic console" (colsole emulation in vesa 800x600
mode).

      mode con list
     
   will show list of available modes.
   To set mode use

      mode con cols=x lines=y

Maximum monitor resolution can be specified by adding string into qssetup.cmd,
for example (DDC is not used now):

      set vesa = on, maxw=1024, maxh=768

   QSINIT can be instructed to not touch VESA at all by the same option in
qssetup.cmd:

      set vesa = off

   (this will break VMTRR too, because it query video memory address in VESA).

   "Graphic console" is much faster if you turn on Write Combining.

   It is possible to use own font (binary file with 1 bit per pixel - i.e.
8x16 x 256 chars will be 1 byte (8 bits) x 16 x 256 = 4096 bytes).
   And it is possible to add own console modes on base of existing graphic
modes (read more in "mode /?").

   And - there is some kind of problems with Virtual PC VESA emulation, use
VBox or hardware PC for this mode :)

  =======================================================================
   5. "Kernel browser".
  -----------------------------------------------------------------------

   "IMPORT"  button  inside  of  kernel  options  setup  dialog will launch
"kernel browser". This option is available for intensive testing of a large
number of different kernel files.

   To  use this feature you must pack a number of kernels into REV_ARCH.ZIP
and  then put it into the root of boot drive. After that, add DISKSIZE=2048
to OS2LDR.INI "config" section.

   This ZIP will be loaded by pressing "IMPORT" button and any file from it
can  be selected as kernel for current boot. This method works on FAT, HPFS
and  PXE, but can fail on JFS boot because of bug in micro-FSD (it hangs on
large  files  or  access  over 4Gb - Mensys fix something here a short time
ago).

   ZIP  file  name  can be overloaded by REV_ARCH_PATH environment variable
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
   because it loader-based (i.e. loader prepare system memory for kernel).

 * using VMs to test QSINIT.
   VPC & Vbox works fine, in most cases, with some exceptions:
   - no graphic console and PAE paging mode in VPC
   - PAE paging mode in VBox is unstable
   - 8, 12, 13 exceptions stops VPC VM (because of task gates used).
     "NOTASK=1" in OS2LDR.INI can be used to prevent using task gates.

   EFI version works only in QEMU (with TianoCore).

  =======================================================================
   7. QSINIT boot details.
  -----------------------------------------------------------------------

   QSINIT can be loaded:

   * via OS/2 boot FSD mechanism - i.e. QSINIT loader looks like OS2LDR for
     micro-FSD code.
     Files OS2LDR and QSINIT.LDI just placed to the root of boot disk.

   * via OS/2 PXE boot - the same.
     Files OS2LDR and QSINIT.LDI must be accessed from TFTP.

   * from FAT12/FAT16/FAT32 - by using OWN boot sector. Boot sector can be
     installed by tool\bootset? command.
     Boot sector assume QSINIT file name for base loader (not OS2LDR as on
     FSD mechanism).

     I.e. files QSINIT and QSINIT.LDI must be in the root of boot disk.

     This mechanism must work on HDD/floppy/flash drives, really ;)

   QSINIT provide own MBR code both for MBR and GPT partition tables.
   It is optional and can be installed to empty disk, for example, or in
   emergency case.

   Common MBR code have no differences with other ones. It loads active
   partition and launch it.
   In addition - it have special Boot Manager support (as OS/2 MBR).

   MBR code for GPT disks searches for 1st partition with "BIOS Bootable"
   attribute ON (google UEFI Wiki about it) and start sector below 2TB
   (32 bit sector number) - and then launch it.
   You can set "BIOS Bootable" attribute via "gpt" command or "sysview" app.
   
   I.e. you can create "dual boot" flash drive, for example:

   * init it to GPT in QSINIT
   * create two partitions, set type of 1st to "EFI boot"
   * format both to FAT
   * copy to first \EFI\BOOT\BOOTX64.EFI - something like EFI shell or
     EFI version of QSINIT.
   * set up second as QSINIT boot partition (set "BIOS Bootable" attribute
     and install QSINIT to it)
   * then you can choose in EFI BIOS boot devices menu: "YOUR DRIVE" or 
     "UEFI: YOUR DRIVE", 1st will be QSINIT and 2nd - EFI shell.

   You can learn MBR/boot sector code details in 

      \QS\system\src\ldrapps\partmgr\se.asm

   in QS_SDK.ZIP.

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

     * mount any available FAT16/FAT32 partition to QSINIT and save log
       directly to it. This can be done in SysView app or by using LOG
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
  10. Additions in kernel loading and OS2LDR.INI
      in comparision with OS/4 loader.
  -----------------------------------------------------------------------

   Warp  3  and Merlin (up to FP12) kernels can be loaded as well as Aurora
type  kernels. But this functionality is not hardly tested. Warp Server SMP
kernel is not supported (if anyone still need it,  even for playing in VBox
- just tell me).
   Kernel version is determined automatically.

   In  OS2LDR.INI  only  DBPORT,  DBCARD, DISKSIZE, NOAF, BAUDRATE, UNZALL,
PCISCAN_ALL  and  RESETMODE  keys affect QSINIT, any other is used for OS/2
kernel menu/boot only.

   LETTER,  LOGLEVEL,  NOAF  and  BAUDRATE  can  be  added  both  to kernel
parameters  line  and  to  "config"  section; DISKSIZE, RESETMODE, USEBEEP,
UNZALL, DBCARD and PCISCAN_ALL has effect in "config" section only.

 * DISKSIZE  - additional space for qsinit "virtual disk" in kb. Required by
               "kernel browser" option above.

 * BAUDRATE  - debug COM port baud rate (115200 by default). BAUDRATE in 
               "config" section will affect both QSINIT and OS/2 kernel, in
               kernel parameters line - kernel only. 
               Supported values: 150,300,600,1200,2400,4800,9600,19200,38400,
               57600 and 115200.

 * NOAF      - turn OFF Advanced Format aligning for disk read/write ops. 
               Affect both QSINIT and OS/2 boot (until IDE/SATA driver load).
               By default most of long i/o ops aligned to 4k.

 * RESETMODE - in addition to RESETMODE=1 support number of text lines to set:
               RESETMODE=25, RESETMODE=43 and RESETMODE=50;
               This option will reset display mode to 80x25 before menu or
               reset it to selected number of lines (80x25, 80x43, 80x50).

 * LETTER=X  - allow to change boot drive letter for OS/2.
               This option only changes letter, not partition. Wrong letter
               sometimes can be supplied by Boot Manager, for example.

 * SOURCE=X  - allow to change boot partition for OS/2 boot, where X - QSINIT
               drive letter of mounted (to QSINIT!) FAT partition.
               I.e. this key can change boot partition to any available FAT
               with OS/2 - without reboot.
               Basically, it designed for booting from PAE RAM disk.

 * USEBEEP=1 - turn PC speaker sound in kernel selection menu (for monitor-less
               configurations). It will beep with one tone on first line and
               another tone on any other.

 * LOGLEVEL=0..3 - maximim level of QSINIT log messages to copy to OS/2 kernel
               log. By default - messages is not copied at all.

 * UNZALL    - unpack all modules from QSINIT.LDI to virtual disk duaring init.
               By default, EXE/DLL modules only reserve space on disk - until
               first usage. Option suitable to check QSINIT.LDI consistency on
               every boot (unstable PXE connection or devices with bad sectors).

 * NOMTRR    - do not use MTRR changes, was done in QSINIT for OS/2 boot.

 * VIEWMEM   - open memory editor just before starting kernel (when it ready to
               launch).

 * CALL=batch_file - call QSINIT batch file just before starting kernel.
               "batch_file" must be a fully qualified path in QSINIT!
               I.e. A:\BATCH.CMD (boot disk, possible on FAT boot only), or
               C:\dir1\batch.cmd (from mounted volume).
               VIEWMEM (if specified) will be called after this batch file and
               then kernel will be launched.

 * PCISCAN_ALL=1 - scan all 256 PCI buses. By default QSINIT scan only number
               of buses, reported by PCI BIOS. This key can be really slooooow
               on old PCs (2 minutes on my PPro).

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


   Also,  new section "[partition]" is available in OS2LDR.INI. It define a
list  of partitions to boot and available to use from "Boot partition" menu
in QSINIT.

   OS2LDR.INI  can  be  shared  with  OS/4  loader, but it ignores unknown
sections, so this is not a problem.
   Section format is very simple:

   [partition]
   hd0/0 = Boot 1st partition of disk 0
   hd1/0 = Boot 1nd partition of disk 1
   hd2   = Boot MBR of disk 2

   Partition index can be queried by pressing F7 in QSINIT menu.
   TIMEOUT and USEBEEP parameters will affect this menu too (timeout counter
occurs on first launch only).

   Default string number can be placed into "default_partition" key in
"[config]" section (in the same way as in kernel setup):

   [config]
   default_partition = 3

   will select "Boot MBR of disk 2" as default.


   Other differences:

 * both IBM and OS/4 OS2LDR checks all HDDs in system for real support of 
   Int13x API (long disk read). QSINIT verify only boot HDD (to avoid problems
   with USB devices).

 * there is no DBCS boot support in QSINIT (os2dbcs, os2dbcs.fnt loading, etc)

 * options SHAREIRQ and DBFLAGS ignored.

 * NOLFB option (do not use LFB for VESA logo, was available in older OS/4
   loader versions) - ignored.

 * both IBM and OS/4 OS2LDR use PCI BIOS calls. QSINIT use PCI BIOS only for
   basic info (version and number of buses in system), all scan, read and write
   operations performed internally.

 * OEMHLP$ driver (loader IS this driver) use much less memory in 1st Mb.

   System requirements: 80486, 24Mb of memory (actually 16 of it reserved
   for OS/2 boot).

   Small article on russian is available on RU/2, you can try to google
   translate it (at least half of text will be readable ;)
      http://ru2.halfos.ru/core/articles/qsinit

   Made on Earth,
   Enjoy ;) and
   check ftp://212.12.30.18/public/QS for new versions and SDK ;)

   QSINIT source code available in SDK archive.

   PXEOS4 package: ftp://212.12.30.18/public/pxeos4-1.130806.zip
     newer versions here: http://moveton.ho.ua/files

   Author: dixie, e-dixie@mail.ru
