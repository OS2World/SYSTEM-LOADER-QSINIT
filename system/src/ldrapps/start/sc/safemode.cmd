rem
rem This script called instead of start.cmd when left SHIFT key pressed
rem duaring boot. Also, there is no debug COM port output is this mode.
rem

rem search path
set libpath=1:\dll;
set path=1:\;
set tracepath=1:\msg;

rem verbose build of QSINIT binary set 80x50 on init
set errorlevel = %lines%
if errorlevel 50 goto dbg

:cmdloop
cls
echo ******************************************
echo . "safe mode" shell                      .
echo ******************************************
echo . type "bootmenu" to load menu selection .
echo . type "help" for shell commands list    .
echo ******************************************
:dbg
cmd
echo Unable to exit from last copy of cmd.exe!
pause Press any key ...
goto cmdloop
