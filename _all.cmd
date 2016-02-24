@rem *********************************
@rem usage: _all         - build
@rem        _all clean   - clean
@rem *********************************
@echo off

if exist system\src\setup.cmd goto installed
rem actually, user should prepare it
copy system\src\setup.cmd.template system\src\setup.cmd > nul
:installed

rem * loader *
cd system\src\init
call make.cmd %1 %2

rem * ldi *
cd ..\ldrapps
call make.cmd %1 %2

cd ..\..\..
