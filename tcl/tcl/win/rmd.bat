@echo off
rem RCS: @(#) $Id: rmd.bat,v 1.1.1.2 2000/04/12 02:02:36 wsanchez Exp $

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
