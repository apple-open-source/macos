@echo off
@setlocal
set CLASSPATH=%~dp0..\lib\ant.jar;%CLASSPATH%
java org.apache.tools.ant.Launcher %*
rem java -jar %ANT_HOME%\lib\ant.jar org.apache.tools.ant.Launcher
@endlocal
