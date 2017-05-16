
    This is just a tiny tool to modify CONFIG.SYS during PAE RAM disk boot.

    It counts number of USB controllers & add/replace drivers for it and
able to modify PROTSHELL key to the supplied parameter.

    Example configuration of cd-rom for PAE RAM disk boot:

  * OS2PM.ZIP with FAT installation (minimal PM fits into 15mb zip)
  * OS2LDR.INI, QSSETUP.CMD & FIXCFG.CMD which calls this tool to update
CONFIG.SYS.

==============================================================================
    Example OS2LDR.INI:

[config]
default=1
timeout=5
logsize=256
loglevel=3
; @ - autodetect PAE ram disk mounted partition
source=@

[kernel]
os2krnl=Command line with BootOS/2 shell (start & alt-esc available), call=b:\fixcfg.cmd
os2krnl=... same + USB, call=b:\fixcfg.cmd usb
os2krnl=Command line in PMSHELL, call=b:\fixcfg.cmd pm
os2krnl=... same + USB, call=b:\fixcfg.cmd usb pm
==============================================================================
    Example QSSETUP.CMD:

copy /q /boot fixcfg.cmd .
ramdisk q: min=32 max=1900 /q
echo
echo [1;37mWait, please - unzipping...[0;37m
unzip /q /boot OS2PM.ZIP c:\
==============================================================================
    Text of FIXCFG.CMD:

set shellarg=Q:\OS2\PMSHELL.EXE
set usbarg=

if /i NOT ."%1"==."USB" goto nousb
set usbarg=-usb
shift
:nousb

if /i ."%1"==."PM" goto fixit
set shellarg=Q:\OS2\BOS2SHL.EXE

:fixit
fixcfg c:\config.sys %usbarg% -shell=%shellarg%
==============================================================================

    You can view/modify CONFIG.SYS after the CALL statement in OS2LDR.INI
menu. Press Ctrl-Enter on selected menu line and append string: ,viewmem
(with comma) before launch it. Executed command will open memory editor
in SysView after the kernel was loaded and CALL executed. At this time 
c:\config.sys - should be your target modified config. Just open it in
SysView`s text editor.

    OR - you can append "sysview /edit c:\config.sys" to the CALL`s batch
file itself.
