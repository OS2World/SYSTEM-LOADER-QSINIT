@echo off
rem
rem ----------------------------------------------------
rem environment setup, called by ..\setup.cmd
rem ----------------------------------------------------
rem
rem  * make sure "set WATCOM=..." is not empty
rem  * if QS is not in the root, update Doxyfile INPUT for doxygen
rem  * doxygen require ODIN (pec.exe) in the path
rem
set QS_BASE=%QS_ROOT%\system\src
set QS_BIN=%QS_ROOT%\tbin
set INCLUDE=%QS_BASE%\hc
set QS_APPINC=%QS_BASE%\ldrapps\hc
set QS_ARCH=QSINIT.LDI
set QS_NAME=QSINIT

rem detect build OS
if "%OS%".=="Windows_NT". goto Win
if NOT "%BEGINLIBPATH%".=="". goto Os2
if "%VIO_SVGA%".=="". goto UnkOS
:Os2
set BUILD_HERE=OS2
set PATH=%WATCOM%\BINP;
set TOOL_INC=%WATCOM%\H;%WATCOM%\H\OS2;%WATCOM%\H\NT
set BCKEY=
rem protect from BEGINLIBPATH overflow in long builds
if "%QS_BLP_DONE%".=="yes". goto cont
set BEGINLIBPATH=%WATCOM%\binp\dll;%BEGINLIBPATH%;
set QS_BLP_DONE=yes
goto cont
:UnkOS
echo Warning! Unknown build OS!
set BUILD_HERE=ERROR
goto cont
:Win
set BUILD_HERE=NT
set PATH=%windir%;%windir%\system32;%WATCOM%\BINNT;
set TOOL_INC=%WATCOM%\H;%WATCOM%\H\NT;%WATCOM%\H\OS2
set BCKEY=-bc
set LIB=
if "%USER%".=="". set USER=%USERNAME%
:cont

rem no watcom libs, structs packed to 1 byte,
rem inline FP code, unmangled stdcall (runtime functions), favor size
set QS_CFLAGS=-zz -zl -zls -zq -zp1 -fp5 -fpi87 -zri -os -bt%QS_NAME%

rem special header for main function
set QS_MHDR=%QS_BASE%\ldrapps\runtime\initdef.h

rem common include key for wcc
set QS_WCCINC=-i%QS_BASE%\hc -i%QS_APPINC% -i%QS_BASE%\init\hc

rem /dev/null path
set DEVNUL=nul

rem replace default tooo long path to safer value
set PATH=%QS_BASE%\tools\t_%BUILD_HERE%;%QS_BASE%\tools;%PATH%;%WATCOM%\BINW;
