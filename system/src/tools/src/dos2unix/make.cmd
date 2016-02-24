@echo off
rem save original include & path
set INC_ORG=%INCLUDE%
set PATH_ORG=%PATH%
call ..\..\..\setup.cmd
rem and restore it ;)
set INCLUDE=%INC_ORG%
set PATH=%PATH_ORG%

set os_def=SP_%BUILD_HERE%
if "%BUILD_HERE%".=="OS2". goto skipfix
set os_def=SP_WIN32
:skipfix
set defs=-DSRT -DNO_HUGELIST -DNO_RANDOM -DSORT_BY_OBJECTS_ON -DNO_SP_ASSERTS

wcl386 -zq -D%os_def% %defs% -I..\..\..\ldrapps\hc\rt dos2unix.cpp ..\..\..\ldrapps\runtime\rt\sp_defs.cpp ..\..\..\ldrapps\runtime\rt\sp_str.cpp
set TGT_DIR=..\..\t_%BUILD_HERE%
if EXIST %TGT_DIR%\DOS2UNIX.EXE del %TGT_DIR%\DOS2UNIX.EXE >nul
move DOS2UNIX.EXE %TGT_DIR%
del *.obj>nul
