@echo off
rem detect build OS
if "%OS%".=="Windows_NT". goto Win
if "%VIO_SVGA%".=="". goto UnkOS
set suffix=o
goto cont
:UnkOS
echo Unknown OS!
exit
:Win
set suffix=w
:cont
if %1.==. goto about
if not exist %suffix%\vpcfcopy.exe goto bserr
if not exist QSINIT.LDI goto dotdot
if not exist OS2LDR goto dotdot
set bsrc=
goto runit
:dotdot
set bsrc=..\
if not exist ..\QSINIT.LDI goto berr
if not exist ..\OS2LDR goto berr
:runit
if not exist %1 goto noimg

%suffix%\vpcfcopy.exe %1 OS2LDR %bsrc%OS2LDR
if errorlevel 1 goto wrerror
%suffix%\vpcfcopy.exe %1 QSINIT.LDI %bsrc%QSINIT.LDI
if errorlevel 1 goto wrerror
%suffix%\vpcfcopy.exe /boot %1 OS2LDR
if errorlevel 1 goto wrerror
exit

:noimg
if "%2".=="". goto noimgsize
%suffix%\vpcfcopy.exe /create %1 %2
if exist %1 goto runit
echo Unable to create image file %1!
exit

:wrerror
echo Write failure...
exit

:noimgsize
echo Image file %1 not found!
exit

:berr
echo Unable to locate QSINIT files: OS2LDR and QSINIT.LDI.
exit

:serr
echo Error writing boot sector, cancelled.
exit

:bserr
echo Unable to locate QSINIT "VPCFCOPY" executable.
exit

:about
echo.
echo Install QSINIT into a FAT disk image.
echo.
echo Usage: instimg.cmd imgfile [1.44/2.88/size]
echo.
echo        if no "imgfile" found - raw image can be created: 1.44Mb, 2.88Mb
echo        floppy or "size" megabytes HDD (up to 2047).
echo.
echo        HDD image created in RAW format by default, but fixed size VHD
echo        may be forced by ".vhd" extension of the image file.
echo.
