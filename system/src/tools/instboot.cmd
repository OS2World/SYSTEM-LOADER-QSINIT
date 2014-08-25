@echo off
rem detect build OS
if "%OS%".=="Windows_NT". goto Win
if "%VIO_SVGA%".=="". goto UnkOS
set suffix=o
goto cont
:UnkOS
echo Uncknown OS!
exit
:Win
set suffix=w
:cont
if "%1".=="". goto about
if not exist bootset%suffix%.exe goto bserr
if not exist QSINIT.LDI goto dotdot
if not exist OS2LDR goto dotdot
set bsrc=
goto runit
:dotdot
set bsrc=..\
if not exist ..\QSINIT.LDI goto berr
if not exist ..\OS2LDR goto berr
:runit
bootset%suffix% -w %2 -q %1
if errorlevel 1 goto serr
if exist %1\QSINIT attrib -r -s -h %1\QSINIT
if exist %1\QSINIT.LDI attrib -r -s -h %1\QSINIT.LDI
copy %bsrc%OS2LDR %1\QSINIT /b
copy %bsrc%QSINIT.LDI %1\QSINIT.LDI /b
attrib +r +s +h %1\QSINIT
attrib +r +s +h %1\QSINIT.LDI
exit
:berr
echo Unable to locate QSINIT files: OS2LDR and QSINIT.LDI.
exit

:serr
echo Error writing boot sector, cancelled.
exit

:bserr
echo Unable to locate QSINIT "BOOTSET" executable.
exit

:about
echo.
echo Install QSINIT to the FAT/FAT32 partition as BOOT FILE.
echo.
echo Usage: instboot.cmd drive: [-y]
echo        use -y to skip confirmation(!)
echo.
