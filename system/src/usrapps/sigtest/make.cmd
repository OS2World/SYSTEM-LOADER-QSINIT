@rem **************************************************
@rem * make            - make                         *
@rem * make clean      - clean                        *
@rem * make nolink     - make without linking         *
@rem **************************************************
@echo off
rem --------------
set prjname=sigtest
rem --------------
set bldarg=%1
if "%bldarg%".=="". set bldarg=*

call ..\..\setup.cmd

set appinit_lib=%QS_BASE%\ldrapps\runtime

rem writing makefile for reference
spprj -nb %prjname%.prj 0 bld\%prjname%.mak
rem and build it
spprj -b -bl -w -es %prjname%.prj 0 %bldarg%

set PATH=%PATH_ORG%
