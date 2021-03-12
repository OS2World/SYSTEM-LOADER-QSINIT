rem --============--
rem     dump all
rem --============--

if "%1".=="". goto about
if "%1".=="con". goto print
cmd "/o:%1" /c "%0" con
exit
:print
ver /q
echo Version %qsver% build %qsbuild%.
mode sys status
chcp
echo
mode sys cpu
echo
mem /a /o
echo
date
time
echo
echo - <<Processes>> ----------------
ps /a
echo
lm /list
if not loaded MTLIB goto skipse
echo
echo - <<Sessions>> -----------------
sm vlist
sm list
:skipse
echo
echo - <<PCI>> ----------------------
pci view
echo --
pci view all
echo
echo - <<MTRR>> ---------------------
mtrr view
echo
echo - <<Console>> ------------------
mode con list
echo
mode con gmlist /v
echo
mode con edid
echo
echo - <<Mounted volumes>> ----------
mount list /v
echo
echo - <<FS handlers>> --------------
mode sys list fs
echo
echo - <<Disks>> --------------------
dmgr list /v
echo --
dmgr mode all
echo --
dmgr list all
if not loaded CACHE goto skipcache
echo
echo - <<Cache>> --------------------
cache info
:skipcache
if "%hosttype%"=="EFI" goto skipramdisk
echo
echo - <<RAM disk>> -----------------
ramdisk info
:skipramdisk
if not loaded VHDD goto skipvhdd
echo
echo - <<VHDD>> ---------------------
vhdd list
:skipvhdd
echo
echo - <<Environment>> --------------
set
echo
echo - <<Dump pages>> ---------------
ps /d3 /d4 /d5 /d6
rem EFI page dump will be TOO HUGE
if "%hosttype%"=="BIOS" ps /d8
ps /d9 /d10 /d11
echo
echo - <<Log>> ----------------------
log /l /f type
exit
:about
echo Saves all QSINIT information into a text file
echo
echo Usage: dumpall.cmd filename
echo
