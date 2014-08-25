rem --===========--
rem   sys command
rem --===========--

if "%1".=="". goto about
md 1:\boot.tmp
pushd 1:\boot.tmp
copy /boot /q QSINIT.LDI QSINIT.LDI
copy /boot /q QSINIT QSINIT
if not exist QSINIT copy /boot /q OS2LDR QSINIT
dmgr bs %1 code
if errorlevel 1 goto fini
echo
if exist %1\QSINIT attrib -r -s -h %1\QSINIT
if exist %1\QSINIT.LDI attrib -r -s -h %1\QSINIT.LDI
copy /q QSINIT %1\QSINIT
copy /q QSINIT.LDI %1\QSINIT.LDI
if not exist %1\QSINIT goto err
if not exist %1\QSINIT.LDI goto err
attrib +r +s +h %1\QSINIT
attrib +r +s +h %1\QSINIT.LDI
if exist %1\OS2BOOT del %1\OS2BOOT
echo Boot files ready!
:fini
cd ..
rmdir /s /q boot.tmp
popd
exit

:err
echo Unable to write boot files!
goto fini

:about
echo
echo Install QSINIT to the FAT/FAT32 partition as BOOT FILE.
echo
echo Usage: sys.cmd drive:
echo