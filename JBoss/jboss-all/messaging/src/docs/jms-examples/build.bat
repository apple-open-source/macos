@echo off

set CLASSPATH=.
set LOCALCLASSPATH=
for %%i in (..\..\client\*.*) do call lcp.bat %%i
set CLASSPATH=%CLASSPATH%;%LOCALCLASSPATH%

javac  *.java
