rem --===========--
rem   sys command
rem --===========--

if "%1".=="". goto about
md 1:\boot.tmp
pushd 1:\boot.tmp
copy /boot /q QSINIT.LDI QSINIT.LDI
copy /boot /q QSINIT QSINIT
if not exist QSINIT copy /boot /q OS2LDR QSINIT
if not exist QSINIT goto read_err
if not exist QSINIT.LDI goto read_err
rem replace boot sector
dmgr bs %1 code
if errorlevel 1 goto fini
echo
rem replace boot files
if exist %1\QSINIT attrib -r -s -h %1\QSINIT
if exist %1\QSINIT.LDI attrib -r -s -h %1\QSINIT.LDI
copy /q QSINIT %1\QSINIT
copy /q QSINIT.LDI %1\QSINIT.LDI
if not exist %1\QSINIT goto err
if not exist %1\QSINIT.LDI goto err
attrib +r +s +h %1\QSINIT
attrib +r +s +h %1\QSINIT.LDI
rem delete IBM`s OS2BOOT if exist (it not required)
if not exist %1\OS2BOOT goto ready
attrib -r -s -h %1\OS2BOOT
del /qn %1\OS2BOOT
:ready
echo Boot files ready!
:fini
cd ..
rmdir /s /q boot.tmp
popd
exit

:err
echo Unable to write boot files!
goto fini

:read_err
if "%HOSTTYPE%"=="EFI" goto efi
echo
echo System files missing on boot disk!?
echo
goto fini
:efi
echo
echo SYS command unable to install BIOS hosted version of QSINIT because
echo there is no suitable QSINIT binary in EFI build.
echo
goto fini

:about
echo
echo Install QSINIT to the FAT/FAT32 partition as BOOT FILE.
echo
echo Usage: sys.cmd drive:
echo
