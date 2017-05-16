rem
rem This script called instead of start.cmd when left SHIFT key pressed
rem during boot. Also, there is no debug COM port output in this mode.
rem

rem search path
set libpath=1:\dll;
set path=1:\;
set tracepath=1:\msg;

rem Verbose build of QSINIT binary sets 80x50 on init and uses console for
rem debug output. Avoid cls/echo on it to keep boot log on screen:
set errorlevel = %lines%
if errorlevel 50 goto dbg

:cmdloop
cls
echo ******************************************
echo . "safe mode" shell                      .
echo ******************************************
echo . type "bootmenu" to load menu selection .
echo . type "help" for shell commands list    .
if NOT %hosttype%=="EFI" goto efiskip
echo . type "exit" to return back to UEFI     .
:efiskip
echo ******************************************
:dbg
cmd /e bootmenu.exe
if %hosttype%=="EFI" goto efiexit
echo Unable to exit from the last copy of cmd.exe!
pause Press any key ...
goto cmdloop

:efiexit
msgbox "QSINIT" "Exit back to UEFI?^Do you want to continue?" YESNO
rem filter YES errorlevel value
if errorlevel 3 goto cmdloop
if not errorlevel 2 goto cmdloop
