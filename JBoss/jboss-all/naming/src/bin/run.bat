@echo off

start java -jar jnpserver.jar

echo Press any key when the server is started to continue
pause
java -jar jnptest.jar

pause
