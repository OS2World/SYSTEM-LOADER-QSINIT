@rem **************************************************
@rem * make [1]        - release make                 *
@rem * make 0          - debug make                   *
@rem * make [x] clean  - clean                        *
@rem * make [x] nolink - make without linking         *
@rem **************************************************
@echo off
set bldtype=%1
set bldarg=%2
if "%bldtype%"=="0" goto make
if NOT "%bldtype%"=="1" set bldarg=%1
set bldtype=1
:make
if "%bldtype%"=="1" set bldtype=2
if "%bldtype%"=="0" set bldtype=1
if "%bldarg%".=="". set bldarg=*

rem save original watcom inc path
set INC_ORG=%include%
rem setup variables
call ..\..\setup.cmd
set INCLUDE=%inc_org%;%include%
rem creating dirs (else spprj will fail to write misc files before build)
spprj -b -w -nb sysview.prj %bldtype% makedirs
rem writing makefile for reference
spprj -nb sysview.prj %bldtype% bld\sysview.mak
rem and build it
spprj -b -bl -w %BCKEY% -es sysview.prj %bldtype% %bldarg%
