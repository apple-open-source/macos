@echo off
rem RCS: @(#) mkd.bat,v 1.4 2002/11/26 19:48:06 hunt Exp

if exist %1\. goto end

if "%OS%" == "Windows_NT" goto winnt

md %1
if errorlevel 1 goto end

goto success

:winnt
md %1
if errorlevel 1 goto end

:success
echo created directory %1

:end


