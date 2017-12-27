@rem ***************************************************
@rem * make            - build all projects            *
@rem * make clean      - clean all projects            *
@rem * make prj        - build single project          *
@rem * make prj xx     - build single project`s target *
@rem ***************************************************
@echo off
rem setup variables
call ..\setup.cmd
set appinit_lib=%QS_BASE%\ldrapps\runtime

if "%1"=="clean" goto build_all
if NOT "%1".=="". goto single_build

:build_all
set bldparm=%1
rem ------- add project here -------
call make.cmd runtime  %bldparm%
call make.cmd tv       %bldparm%
call make.cmd anacnda2 %bldparm%
call make.cmd start    %bldparm%
call make.cmd tetris   %bldparm%
call make.cmd doshlp   %bldparm%
call make.cmd bootos2  %bldparm%
call make.cmd console  %bldparm%
call make.cmd bootmenu %bldparm%
call make.cmd sysview  %bldparm%
call make.cmd cmd      %bldparm%
call make.cmd partmgr  %bldparm%
call make.cmd vmtrr    %bldparm%
call make.cmd cache    %bldparm%
call make.cmd vdisk    %bldparm%
call make.cmd cplib    %bldparm%
call make.cmd vhdd     %bldparm%
call make.cmd extcmd   %bldparm%
call make.cmd mtlib    %bldparm%
call make.cmd aoscfg   %bldparm%
call make.cmd fslib    %bldparm%

goto quit
:single_build
if exist %1\%1.prj goto buildme
echo Project "%1" is not exist!
goto quit
:buildme
rem ----- single project build -----
cd %1
rem creating dirs (else spprj will fail to write misc files before build)
spprj -b -w -nb %1.prj 0 makedirs
rem writing makefile for reference
rem spprj -nb %1.prj 0 bld\%1.mak
rem and build it
set bldarg=%2
if "%bldarg%".=="". set bldarg=*
spprj -b -bl -w %BCKEY% -es %1.prj 0 %bldarg%
cd ..
:quit
