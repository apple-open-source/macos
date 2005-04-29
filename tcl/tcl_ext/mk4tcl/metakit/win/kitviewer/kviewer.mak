# ---------------------------------------------------------------------------
VERSION = BCB.04.04
# ---------------------------------------------------------------------------
!ifndef BCB
BCB = $(MAKEDIR)\..
!endif
# ---------------------------------------------------------------------------
PROJECT = kviewer.exe
OBJFILES = kviewer.obj kview.obj ..\..\src\viewx.obj ..\..\src\custom.obj \
  ..\..\src\derived.obj ..\..\src\field.obj \
  ..\..\src\fileio.obj ..\..\src\format.obj \
  ..\..\src\handler.obj ..\..\src\persist.obj \
  ..\..\src\std.obj ..\..\src\store.obj ..\..\src\string.obj \
  ..\..\src\table.obj ..\..\src\univ.obj ..\..\src\view.obj \
  ..\..\src\column.obj
RESFILES = kviewer.res
RESDEPEN = $(RESFILES) kview.dfm
LIBFILES = 
IDLGENFILES =
IDLFILES =
LIBRARIES = vcldbx40.lib vcldb40.lib vclx40.lib vcl40.lib
SPARELIBS = vcl40.lib vclx40.lib vcldb40.lib vcldbx40.lib
PACKAGES = VCL40.bpi VCLX40.bpi bcbsmp40.bpi VCLDB40.bpi ibsmp40.bpi VCLDBX40.bpi \
  QRPT40.bpi TEEUI40.bpi TEEDB40.bpi TEE40.bpi DSS40.bpi VCLMID40.bpi \
  NMFAST40.bpi INETDB40.bpi INET40.bpi dclocx40.bpi
PATHASM = .;
PATHCPP = .;..\..\src
PATHPAS = .;
PATHRC = .;
DEBUGLIBPATH = $(BCB)\lib\debug
RELEASELIBPATH = $(BCB)\lib\release
SYSDEFINES = NO_STRICT
USERDEFINES = q4_EXPORT=0;q4_CHECK=1
DEFFILE =
# ---------------------------------------------------------------------------
CFLAG1 = -I..\..\src;..\..\include;..;$(BCB)\include;$(BCB)\include\vcl -Od -Hc \
  -H=$(BCB)\lib\vcl.csm -w -Vmd -Ve -RT- -r- -a4 -d -k -y -v -vi- \
  -D$(SYSDEFINES);$(USERDEFINES) -c -b- -w-par -w-inl -Vx -tWM
CFLAG2 =
CFLAG3 =
IDLCFLAGS = -I..\..\src -I..\..\include -I.. -I$(BCB)\include -I$(BCB)\include\vcl \
  -src_suffixcpp -Dq4_EXPORT=0 -Dq4_CHECK=1
PFLAGS = -U..\..\src;$(BCB)\Projects\Lib;$(BCB)\lib\obj;$(BCB)\lib;$(RELEASELIBPATH) \
  -I..\..\src;..\..\include;..;$(BCB)\include;$(BCB)\include\vcl \
  -AWinTypes=Windows;WinProcs=Windows;DbiTypes=BDE;DbiProcs=BDE;DbiErrs=BDE \
  -Dq4_EXPORT=0;q4_CHECK=1 -$YD -$W -$O- -JPHNV -M -JPHNE
RFLAGS = -i..\..\src;..\..\include;..;$(BCB)\include;$(BCB)\include\vcl \
  -Dq4_EXPORT=0;q4_CHECK=1
AFLAGS = /i..\..\src /i..\..\include /i.. /i$(BCB)\include /i$(BCB)\include\vcl \
  /dq4_EXPORT=0 /dq4_CHECK=1 /mx /w2 /zi
LFLAGS = -L..\..\src;$(BCB)\Projects\Lib;$(BCB)\lib\obj;$(BCB)\lib;$(RELEASELIBPATH) -aa \
  -Tpe -x -v
IFLAGS =
LINKER = tlink32
# ---------------------------------------------------------------------------
ALLOBJ = c0w32.obj sysinit.obj $(OBJFILES)
ALLRES = $(RESFILES)
ALLLIB = $(LIBFILES) $(LIBRARIES) import32.lib cp32mt.lib
# ---------------------------------------------------------------------------
.autodepend

!ifdef IDEOPTIONS

[Version Info]
IncludeVerInfo=0
AutoIncBuild=0
MajorVer=1
MinorVer=0
Release=0
Build=0
Debug=0
PreRelease=0
Special=0
Private=0
DLL=0
Locale=1033
CodePage=1252

[Version Info Keys]
FileVersion=1.0.0.0

[HistoryLists\hlIncludePath]
Count=4
Item0=..\..\src;..\..\include;..;$(BCB)\include;$(BCB)\include\vcl
Item1=..\..\src;..\..\include;..\..\win;$(BCB)\include;$(BCB)\include\vcl
Item2=..\mk\src;..\mk\include;..\mk\win;$(BCB)\include;$(BCB)\include\vcl
Item3=..\mk\include;..\mk\src;$(BCB)\include;$(BCB)\include\vcl

[HistoryLists\hlLibraryPath]
Count=1
Item0=..\..\src;$(BCB)\Projects\Lib;$(BCB)\lib\obj;$(BCB)\lib

[HistoryLists\hlConditionals]
Count=2
Item0=q4_EXPORT=0;q4_CHECK=1
Item1=q4_EXPORT=0

[HistoryLists\hlUnitAliases]
Count=1
Item0=WinTypes=Windows;WinProcs=Windows;DbiTypes=BDE;DbiProcs=BDE;DbiErrs=BDE

[Debugging]
DebugSourceDirs=

[Parameters]
RunParams=
HostApplication=
RemoteHost=
RemotePath=
RemoteDebug=0

[Compiler]
InMemoryExe=0
ShowInfoMsgs=0

[CORBA]
AddServerUnit=1
AddClientUnit=1
PrecompiledHeaders=1

!endif

$(PROJECT): $(IDLGENFILES) $(OBJFILES) $(RESDEPEN) $(DEFFILE)
    $(BCB)\BIN\$(LINKER) @&&!
    $(LFLAGS) +
    $(ALLOBJ), +
    $(PROJECT),, +
    $(ALLLIB), +
    $(DEFFILE), +
    $(ALLRES) 
!

.pas.hpp:
    $(BCB)\BIN\dcc32 $(PFLAGS) { $** }

.pas.obj:
    $(BCB)\BIN\dcc32 $(PFLAGS) { $** }

.cpp.obj:
    $(BCB)\BIN\bcc32 $(CFLAG1) $(CFLAG2) -o$* $* 

.c.obj:
    $(BCB)\BIN\bcc32 $(CFLAG1) $(CFLAG2) -o$* $**

.rc.res:
    $(BCB)\BIN\brcc32 $(RFLAGS) $<
#-----------------------------------------------------------------------------
