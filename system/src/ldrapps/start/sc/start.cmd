rem echo is off by default

rem search path
set libpath=1:\dll;
set path=1:\;
set tracepath=1:\msg;

rem set volume label for 1:
label vdisk

rem load HPFS r/o support
lm /q fslib

rem load cache on first mount command if it wasn`t loaded before
set LM_CACHE_ON_MOUNT = on,6%

if NOT %hosttype%=="EFI" goto common0
rem switch EFI version to virtual console, because EFI`s own is a nightmare
mode con cols=80 lines=30
vmtrr
rem now "apps" menu auto-selected for EFI & non-OS/2 boot, but "start_menu"
rem still have priority above default logic.
rem set start_menu = apps
:common0

rem call additional setup.cmd from the root of boot disk
copy /boot /q qssetup.cmd 1:\qssetup.cmd
if exist qssetup.cmd call qssetup.cmd

if NOT %boottype%=="SINGLE" goto common1
rem non-OS/2 boot - load disk i/o cache
cache 6%
:common1

rem run ArcaLoader OS2LDR.CFG configuration check - AFTER qssetup.cmd, it will
rem ignore "ramdisk" key if disk was created by shell commands in qssetup.cmd
if exist aoscfg.exe aoscfg.exe

rem menu (apps / kernel / partition - depends on "start_menu" and defaults)
bootmenu.exe

:cmdloop
cls
echo ************************************************
echo * type "bootmenu" to load menu selection again *
echo *      "help" for shell commands               *
if NOT %hosttype%=="EFI" goto efiskip
echo *      "exit" to return back to UEFI           *
:efiskip
echo ************************************************
cmd
if %hosttype%=="EFI" goto efiexit
echo Unable to exit from the last copy of cmd.exe!
pause Press any key ...
goto cmdloop

:efiexit
msgbox "QSINIT" "Exit back to UEFI?^Do you want to continue?" YESNO
rem filter YES errorlevel value
if errorlevel 3 goto cmdloop
if not errorlevel 2 goto cmdloop
