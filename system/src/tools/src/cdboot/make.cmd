@rem **************************************************
@rem * make [1]        - release make                 *
@rem * make 0          - debug make                   *
@rem * make [x] clean  - clean                        *
@rem * make [x] nolink - make without linking         *
@rem **************************************************
@echo off
rem --------------
set prjname=CDBOOT
rem --------------
set bldtype=%1
set bldarg=%2
if "%bldtype%"=="0" goto make
if NOT "%bldtype%"=="1" set bldarg=%1
set bldtype=1
:make
if "%bldarg%".=="". set bldarg=*

call ..\..\..\setup.cmd

rem creating dirs (else spprj will fail to write misc files before build)
spprj -b -w -nb %toolprjkey% %prjname%.prj %bldtype% makedirs
rem writing makefile for reference
spprj -nb %prjname%.prj %bldtype% bld\%prjname%.mak
rem and build it
spprj -b -bl -w -es %toolprjkey% %prjname%.prj %bldtype% %bldarg%
