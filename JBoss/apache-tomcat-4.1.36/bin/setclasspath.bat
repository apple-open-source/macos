rem ---------------------------------------------------------------------------
rem Set CLASSPATH and Java options
rem
rem $Id: setclasspath.bat 289088 2005-02-15 22:17:53Z markt $
rem ---------------------------------------------------------------------------

rem Make sure prerequisite environment variables are set
if not "%JAVA_HOME%" == "" goto gotJavaHome
echo The JAVA_HOME environment variable is not defined
echo This environment variable is needed to run this program
goto end
:gotJavaHome
if not exist "%JAVA_HOME%\bin\java.exe" goto noJava
if not exist "%JAVA_HOME%\bin\javaw.exe" goto noJavaw
if not exist "%JAVA_HOME%\bin\jdb.exe" goto noJdb
if not exist "%JAVA_HOME%\bin\javac.exe" goto noJavac
goto okJavaHome
:noJava
echo JAVA_HOME\bin\java.exe is missing
goto badJavaHome
:noJavaw
echo JAVA_HOME\bin\javaw.exe is missing
goto badJavaHome
:noJdb
echo JAVA_HOME\bin\jdb.exe is missing
goto badJavaHome
:noJavac
echo JAVA_HOME\bin\javac.exe is missing
goto badJavaHome
:badJavaHome
echo The JAVA_HOME environment variable may not be defined correctly
echo This environment variable is needed to run this program
echo NB: JAVA_HOME should point to a JDK not a JRE
goto end
:okJavaHome

if not "%BASEDIR%" == "" goto gotBasedir
echo The BASEDIR environment variable is not defined
echo This environment variable is needed to run this program
goto end
:gotBasedir
if exist "%BASEDIR%\bin\setclasspath.bat" goto okBasedir
echo The BASEDIR environment variable is not defined correctly
echo This environment variable is needed to run this program
goto end
:okBasedir

rem Set the default -Djava.endorsed.dirs argument
set JAVA_ENDORSED_DIRS=%BASEDIR%\common\endorsed

rem Set standard CLASSPATH
rem Note that there are no quotes as we do not want to introduce random
rem quotes into the CLASSPATH
set CLASSPATH=%JAVA_HOME%\lib\tools.jar

rem Set standard command for invoking Java.
rem Note that NT requires a window name argument when using start.
rem Also note the quoting as JAVA_HOME may contain spaces.
set _RUNJAVA="%JAVA_HOME%\bin\java"
set _RUNJAVAW="%JAVA_HOME%\bin\javaw"
set _RUNJDB="%JAVA_HOME%\bin\jdb"
set _RUNJAVAC="%JAVA_HOME%\bin\javac"

:end
