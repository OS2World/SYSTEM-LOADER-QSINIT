@rem **************************************************
@rem * make [1]        - release make                 *
@rem * make 0          - debug make                   *
@rem * make [x] clean  - clean                        *
@rem **************************************************
@echo off
set bldtype=%1
set bldarg=%2
if "%bldtype%"=="0" goto icc
if NOT "%bldtype%"=="1" set bldarg=%1
set bldtype=1

rem ***************************************************
:icc
if "%CXXMAIN%".=="". goto wcc
NMAKE /nologo /f makefile.icc CFG=%bldtype% %bldarg%
exit

rem ***************************************************
:wcc
if "%WATCOM%".=="". goto unck
set INCLUDE=%WATCOM%\H\OS2;%INCLUDE%
WMAKE -h -f makefile.wcc CFG=%bldtype% %bldarg%
exit

rem ***************************************************
:unck
echo At least one of OpenWATCOM or VAC 3.6.5 required!
