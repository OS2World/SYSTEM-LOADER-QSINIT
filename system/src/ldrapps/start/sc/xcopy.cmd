rem --=============--
rem   xcopy command
rem --=============--

if "%1".=="". goto about
set mask = *.*
set subdirs =
rem get args
:argloop
if /i "%1"=="/s" goto dirkey
if /i "%1"=="/m" goto getmask
if "%2".=="". goto about
rem check target dir presence
if not exist "%2" mkdir "%2"
if not exist "%2" goto targeterr

rem make subdirectory tree
if "%subdirs%".=="". goto skipdir
echo Copying directory tree ...
for /d /r /s %n in (%1\*.*) do mkdir "%2\%n"
:skipdir
rem copying files
for %subdirs% /r %n in (%1\%mask%) do copy /a "%1\%n" "%2\%n"
exit
rem -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
:dirkey
shift
set subdirs = /s
goto argloop

:getmask
set mask = %2
shift
shift
goto argloop

:targeterr
echo Target dir "%2" is not exists and cannot be created!
exit

:about
echo Usage: xcopy [/s] [/m mask] source_dir dest_dir 
echo .  /s      - process subdirectories
echo .  /m mask - use file mask instead of *.*
