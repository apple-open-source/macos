@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.1 2002/02/23 01:08:31 rolf Exp $

if exist %1\nul goto end

md %1
if errorlevel 1 goto end

echo Created directory %1

:end


