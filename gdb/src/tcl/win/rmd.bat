@echo off
rem RCS: @(#) rmd.bat,v 1.4 2002/11/26 19:48:06 hunt Exp

if not exist %1\. goto end

echo Removing directory %1

if "%OS%" == "Windows_NT" goto winnt

cd %1
if errorlevel 1 goto end
del *.*
cd ..
rmdir %1
if errorlevel 1 goto end
goto success

:winnt
rmdir %1 /s /q
if errorlevel 1 goto end

:success
echo deleted directory %1

:end


