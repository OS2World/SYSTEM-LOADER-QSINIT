rem --=============--
rem   APM power off
rem --=============--

msgbox "Power" "The system will now be powered off.^Do you want to continue?" YESNO
rem filter YES errorlevel value
if errorlevel 3 exit
if not errorlevel 2 exit
power off /q
rem ENODEV error code
if errorlevel 36 goto noAPM
msgbox "Power" "APM BIOS error!" OK RED
exit
:noAPM
msgbox "Power" "There is no APM on this PC!" OK RED
