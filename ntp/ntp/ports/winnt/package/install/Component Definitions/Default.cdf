[Components]
component0=Help Files
component1=Server Files
component2=Program Files
component3=Example Files

[Help Files]
SELECTED=Yes
FILENEED=STANDARD
HTTPLOCATION=
STATUS=
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=
DISPLAYTEXT=
IMAGE=
DEFSELECTION=Yes
filegroup0=Help Files
COMMENT=
INCLUDEINBUILD=Yes
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[TopComponents]
component0=Program Files
component1=Example Files
component2=Help Files
component3=Server Files

[Server Files]
SELECTED=Yes
FILENEED=STANDARD
HTTPLOCATION=
STATUS=
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=Server Files
DISPLAYTEXT=Server Files
IMAGE=
DEFSELECTION=Yes
filegroup0=Server Executable Files
COMMENT=ntpd.exe - Network Time Protocol Daemon
INCLUDEINBUILD=Yes
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[SetupType]
setuptype0=Server
setuptype1=Administrator

[Program Files]
SELECTED=Yes
FILENEED=STANDARD
HTTPLOCATION=
STATUS=
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=User programs
DISPLAYTEXT=Program Files
IMAGE=
DEFSELECTION=Yes
filegroup0=DLL Files
COMMENT=ntpdc.exe, ntpdate.exe, ntpq.exe, ntptrace.exe
INCLUDEINBUILD=Yes
filegroup1=Program Executable Files
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[SetupTypeItem-Server]
Comment=
item0=Help Files
item1=Server Files
item2=Program Files
item3=Example Files
Descrip=Server Installation includes the Administrator files, and the Service Executable: ntpd.exe
DisplayText=

[Info]
Type=CompDef
Version=1.00.000
Name=

[Example Files]
SELECTED=Yes
FILENEED=STANDARD
HTTPLOCATION=
STATUS=
UNINSTALLABLE=Yes
TARGET=<TARGETDIR>
FTPLOCATION=
VISIBLE=Yes
DESCRIPTION=Sample ntp.conf configuration file and ntp.drift file
DISPLAYTEXT=Sample Setup
IMAGE=
DEFSELECTION=Yes
filegroup0=Example Files
COMMENT=Sample ntp.conf and ntp.drift
INCLUDEINBUILD=Yes
INSTALLATION=ALWAYSOVERWRITE
COMPRESSIFSEPARATE=No
MISC=
ENCRYPT=No
DISK=ANYDISK
TARGETDIRCDROM=
PASSWORD=
TARGETHIDDEN=General Application Destination

[SetupTypeItem-Administrator]
Comment=
item0=Help Files
item1=Program Files
Descrip=Administrator Setup includes all of the commands to get time, or manipulate an NTP Server: ntpq, ntpdc, ntptrace, ntpdate
DisplayText=

