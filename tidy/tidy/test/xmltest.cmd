@echo off

REM xmltest.cmd - execute all XML test cases
REM
REM (c) 1998-2003 (W3C) MIT, ERCIM, Keio University
REM See tidy.c for the copyright notice.
REM
REM <URL:http://tidy.sourceforge.net/>
REM
REM CVS Info:
REM
REM    $Author: iccir $
REM    $Date: 2007/01/31 02:33:39 $
REM    $Revision: 1.1 $

for /F "tokens=1*" %%i in (xmlcases.txt) do call onetest.cmd %%i %%j
