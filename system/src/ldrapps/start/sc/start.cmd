rem echo is off by default

rem search path
set libpath=1:\dll;
set path=1:\;
set tracepath=1:\msg;

rem set volume label for 1:
label vdisk

rem load cache on first mount command if it wasn`t loaded before
set LM_CACHE_ON_MOUNT = on,6%

rem non-OS/2 boot - change initial menu to "apps"
if NOT %boottype%=="SINGLE" goto common1
set start_menu = apps
:common1
rem call additional setup.cmd from the root of boot disk
copy /boot /q qssetup.cmd 1:\qssetup.cmd
if exist qssetup.cmd call qssetup.cmd

if NOT %boottype%=="SINGLE" goto common2
rem non-OS/2 boot - load disk i/o cache
cache 6%
:common2
rem launch kernel menu first
bootmenu.exe

:cmdloop
cls
echo ************************************************
echo * type "bootmenu" to load menu selection again *
echo *      "help" for shell commands               *
echo ************************************************
cmd
echo Unable to exit from the last copy of cmd.exe!
pause Press any key ...
goto cmdloop
