@echo off

set CLASSPATH=.;..\..\client
set LOCALCLASSPATH=
for %%i in (..\..\client\*.*) do call lcp.bat %%i
for %%i in (..\..\lib\*.*) do call lcp.bat %%i
for %%i in (..\..\lib\ext\*.*) do call lcp.bat %%i
set CLASSPATH=%CLASSPATH%;%LOCALCLASSPATH%

java %1 %2 %3 %4 %5 %6 %7 %8 %9
