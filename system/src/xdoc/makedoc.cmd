@rem 
@rem Use ODIN to run doxygen
@rem 
@echo off
set PATH_ORG=%path%
call ..\setup.cmd
set PATH=%PATH%;%PATH_ORG%
if not %BUILD_HERE%==OS2 goto make
set PEC=pec.exe
:make
%PEC% doxygen.exe
