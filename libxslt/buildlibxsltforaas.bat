@echo off

setlocal

set VCVARS_BAT=vcvarsall.bat
set VS_VERS=2017

:VCVARS_DIR

:: VS Professional 2017+
set VCVARS_DIR=C:\Program Files (x86)\Microsoft Visual Studio\%VS_VERS%\Professional\VC\Auxiliary\Build\
IF EXIST "%VCVARS_DIR%%VCVARS_BAT%" (
    goto EXECUTE
)

:: VS Build Tools 2017+
set VCVARS_DIR=C:\Program Files (x86)\Microsoft Visual Studio\%VS_VERS%\BuildTools\VC\Auxiliary\Build\
IF EXIST "%VCVARS_DIR%%VCVARS_BAT%" (
    goto EXECUTE
)

:MISSING_MSBUILD
echo Cannot determine location of %VCVARS_BAT%
goto ERR_END

:EXECUTE

set ARCH=32
set PROGRAMFILESAAS=Program Files (x86)\Common Files\Apple\Apple Application Support
call "%VCVARS_DIR%%VCVARS_BAT%" x86
nmake /f NMakefileArch %1
if %errorlevel% NEQ 0 (
    goto ERR_END
)

set ARCH=64
set PROGRAMFILESAAS=Program Files\Common Files\Apple\Apple Application Support
call "%VCVARS_DIR%%VCVARS_BAT%" amd64
nmake /f NMakefileArch %1
if %errorlevel% NEQ 0 (
    goto ERR_END
)

goto END

:ERR_END
exit /b 1

:END

endlocal
