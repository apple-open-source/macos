@ECHO OFF

REM How to marathon tests
REM
REM 1. Create or adjust a ".ant.properties" file
REM - set "marathon.duration" to the time the test should run in milliseconds !!
REM - set "marathon.timeout" to add least 3 minutes longer than the duration !!
REM - set "marathon.threadcount" to the number of concurrent thread running in this test
REM       NOTE: that each thread represents a customer in this test
REM
REM 2. Start JBoss 3
REM
REM 3. Adjust jndi.properties if you want to access a remote JBoss 3 server (recommended)
REM
REM 4. Start this script and wait
REM
REM 5. Have a look at the generated output file
REM

echo ".ant.properites" file contains:
type .ant.properties

echo build.bat tests-standard-marathon tests-report  %1 %2 %3 %4 %5 %6 %7 %8 %9

build.bat tests-standard-marathon tests-report  %1 %2 %3 %4 %5 %6 %7 %8 %9

