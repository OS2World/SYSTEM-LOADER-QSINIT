@rem **************************************************
@rem * make            - debug make                   *
@rem * make  1         - release make                 *
@rem * make  2         - init debug make              *
@rem * make [x] clean  - clean                        *
@rem * make [x] nolink - make without linking         *
@rem **************************************************
@echo off
set bldtype=%1
set bldarg=%2
if "%bldtype%"=="1" goto make
if "%bldtype%"=="2" goto make
if NOT "%bldtype%"=="0" set bldarg=%1
set bldtype=0
:make
if "%bldarg%".=="". set bldarg=*

rem setup variables
call ..\setup.cmd

rem creating dirs (else spprj will failed to write misc files before build)
spprj -b -w -nb qsinit.prj %bldtype% makedirs

rem writing makefile for reference
spprj -nb qsinit.prj %bldtype% bld\qsinit.mak

rem and build it
spprj -b -w qsinit.prj %bldtype% %bldarg%

rem wdis -l=OPEN_PM.lst -p -s OPEN_PM.obj

if not exist OS2LDR goto quit
copy OS2LDR %QS_BIN% >nul
:quit
