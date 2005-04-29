# Microsoft Developer Studio Generated NMAKE File, Based on cvsnt.dsp
!IF "$(RECURSE)" == ""
RECURSE=1
!ENDIF
!IF "$(CFG)" == ""
CFG=cvsnt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to cvsnt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "cvsnt - Win32 Release" && "$(CFG)" != "cvsnt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cvsnt.mak" CFG="cvsnt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cvsnt - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "cvsnt - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cvsnt - Win32 Release"

OUTDIR=.\WinRel
INTDIR=.\WinRel
# Begin Custom Macros
OutDir=.\WinRel
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\cvs.exe"

!ELSE 

ALL : "libcvs - Win32 Release" "libz - Win32 Release" "libdiff - Win32 Release" "$(OUTDIR)\cvs.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdiff - Win32 ReleaseCLEAN" "libz - Win32 ReleaseCLEAN" "libcvs - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\add.obj"
	-@erase "$(INTDIR)\admin.obj"
	-@erase "$(INTDIR)\annotate.obj"
	-@erase "$(INTDIR)\buffer.obj"
	-@erase "$(INTDIR)\checkin.obj"
	-@erase "$(INTDIR)\checkout.obj"
	-@erase "$(INTDIR)\classify.obj"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\commit.obj"
	-@erase "$(INTDIR)\create_adm.obj"
	-@erase "$(INTDIR)\cvsrc.obj"
	-@erase "$(INTDIR)\diff.obj"
	-@erase "$(INTDIR)\edit.obj"
	-@erase "$(INTDIR)\entries.obj"
	-@erase "$(INTDIR)\error.obj"
	-@erase "$(INTDIR)\expand_path.obj"
	-@erase "$(INTDIR)\fileattr.obj"
	-@erase "$(INTDIR)\filesubr.obj"
	-@erase "$(INTDIR)\find_names.obj"
	-@erase "$(INTDIR)\hash.obj"
	-@erase "$(INTDIR)\history.obj"
	-@erase "$(INTDIR)\ignore.obj"
	-@erase "$(INTDIR)\import.obj"
	-@erase "$(INTDIR)\JmgStat.obj"
	-@erase "$(INTDIR)\lock.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\login.obj"
	-@erase "$(INTDIR)\logmsg.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\mkdir.obj"
	-@erase "$(INTDIR)\mkmodules.obj"
	-@erase "$(INTDIR)\modules.obj"
	-@erase "$(INTDIR)\myndbm.obj"
	-@erase "$(INTDIR)\ndir.obj"
	-@erase "$(INTDIR)\no_diff.obj"
	-@erase "$(INTDIR)\parseinfo.obj"
	-@erase "$(INTDIR)\patch.obj"
	-@erase "$(INTDIR)\pwd.obj"
	-@erase "$(INTDIR)\rcmd.obj"
	-@erase "$(INTDIR)\rcs.obj"
	-@erase "$(INTDIR)\rcscmds.obj"
	-@erase "$(INTDIR)\recurse.obj"
	-@erase "$(INTDIR)\release.obj"
	-@erase "$(INTDIR)\remove.obj"
	-@erase "$(INTDIR)\repos.obj"
	-@erase "$(INTDIR)\root.obj"
	-@erase "$(INTDIR)\run.obj"
	-@erase "$(INTDIR)\scramble.obj"
	-@erase "$(INTDIR)\server.obj"
	-@erase "$(INTDIR)\sockerror.obj"
	-@erase "$(INTDIR)\stack.obj"
	-@erase "$(INTDIR)\startserver.obj"
	-@erase "$(INTDIR)\status.obj"
	-@erase "$(INTDIR)\subr.obj"
	-@erase "$(INTDIR)\tag.obj"
	-@erase "$(INTDIR)\update.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vers_ts.obj"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(INTDIR)\waitpid.obj"
	-@erase "$(INTDIR)\watch.obj"
	-@erase "$(INTDIR)\woe32.obj"
	-@erase "$(INTDIR)\wrapper.obj"
	-@erase "$(INTDIR)\zlib.obj"
	-@erase "$(OUTDIR)\cvs.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /I ".\WinDebug" /D "NDEBUG" /D "WANT_WIN_COMPILER_VERSION" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /Fp"$(INTDIR)\cvsnt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\cvsnt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=diff\WinRel\libdiff.lib lib\WinRel\libcvs.lib zlib\WinRel\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\cvs.pdb" /machine:I386 /out:"$(OUTDIR)\cvs.exe" 
LINK32_OBJS= \
	"$(INTDIR)\add.obj" \
	"$(INTDIR)\admin.obj" \
	"$(INTDIR)\annotate.obj" \
	"$(INTDIR)\buffer.obj" \
	"$(INTDIR)\checkin.obj" \
	"$(INTDIR)\checkout.obj" \
	"$(INTDIR)\classify.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\commit.obj" \
	"$(INTDIR)\create_adm.obj" \
	"$(INTDIR)\cvsrc.obj" \
	"$(INTDIR)\diff.obj" \
	"$(INTDIR)\edit.obj" \
	"$(INTDIR)\entries.obj" \
	"$(INTDIR)\error.obj" \
	"$(INTDIR)\expand_path.obj" \
	"$(INTDIR)\fileattr.obj" \
	"$(INTDIR)\filesubr.obj" \
	"$(INTDIR)\find_names.obj" \
	"$(INTDIR)\hash.obj" \
	"$(INTDIR)\history.obj" \
	"$(INTDIR)\ignore.obj" \
	"$(INTDIR)\import.obj" \
	"$(INTDIR)\JmgStat.obj" \
	"$(INTDIR)\lock.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\login.obj" \
	"$(INTDIR)\logmsg.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mkdir.obj" \
	"$(INTDIR)\mkmodules.obj" \
	"$(INTDIR)\modules.obj" \
	"$(INTDIR)\myndbm.obj" \
	"$(INTDIR)\ndir.obj" \
	"$(INTDIR)\no_diff.obj" \
	"$(INTDIR)\parseinfo.obj" \
	"$(INTDIR)\patch.obj" \
	"$(INTDIR)\pwd.obj" \
	"$(INTDIR)\rcmd.obj" \
	"$(INTDIR)\rcs.obj" \
	"$(INTDIR)\rcscmds.obj" \
	"$(INTDIR)\recurse.obj" \
	"$(INTDIR)\release.obj" \
	"$(INTDIR)\remove.obj" \
	"$(INTDIR)\repos.obj" \
	"$(INTDIR)\root.obj" \
	"$(INTDIR)\run.obj" \
	"$(INTDIR)\scramble.obj" \
	"$(INTDIR)\server.obj" \
	"$(INTDIR)\sockerror.obj" \
	"$(INTDIR)\stack.obj" \
	"$(INTDIR)\startserver.obj" \
	"$(INTDIR)\status.obj" \
	"$(INTDIR)\subr.obj" \
	"$(INTDIR)\tag.obj" \
	"$(INTDIR)\update.obj" \
	"$(INTDIR)\vers_ts.obj" \
	"$(INTDIR)\version.obj" \
	"$(INTDIR)\waitpid.obj" \
	"$(INTDIR)\watch.obj" \
	"$(INTDIR)\woe32.obj" \
	"$(INTDIR)\wrapper.obj" \
	"$(INTDIR)\zlib.obj" \
	".\diff\WinRel\libdiff.lib" \
	".\zlib\WinRel\libz.lib" \
	".\lib\WinRel\libcvs.lib"

"$(OUTDIR)\cvs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

OUTDIR=.\WinDebug
INTDIR=.\WinDebug
# Begin Custom Macros
OutDir=.\WinDebug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\cvs.exe"

!ELSE 

ALL : "libcvs - Win32 Debug" "libz - Win32 Debug" "libdiff - Win32 Debug" "$(OUTDIR)\cvs.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libdiff - Win32 DebugCLEAN" "libz - Win32 DebugCLEAN" "libcvs - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\add.obj"
	-@erase "$(INTDIR)\admin.obj"
	-@erase "$(INTDIR)\annotate.obj"
	-@erase "$(INTDIR)\buffer.obj"
	-@erase "$(INTDIR)\checkin.obj"
	-@erase "$(INTDIR)\checkout.obj"
	-@erase "$(INTDIR)\classify.obj"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\commit.obj"
	-@erase "$(INTDIR)\create_adm.obj"
	-@erase "$(INTDIR)\cvsrc.obj"
	-@erase "$(INTDIR)\diff.obj"
	-@erase "$(INTDIR)\edit.obj"
	-@erase "$(INTDIR)\entries.obj"
	-@erase "$(INTDIR)\error.obj"
	-@erase "$(INTDIR)\expand_path.obj"
	-@erase "$(INTDIR)\fileattr.obj"
	-@erase "$(INTDIR)\filesubr.obj"
	-@erase "$(INTDIR)\find_names.obj"
	-@erase "$(INTDIR)\hash.obj"
	-@erase "$(INTDIR)\history.obj"
	-@erase "$(INTDIR)\ignore.obj"
	-@erase "$(INTDIR)\import.obj"
	-@erase "$(INTDIR)\JmgStat.obj"
	-@erase "$(INTDIR)\lock.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\login.obj"
	-@erase "$(INTDIR)\logmsg.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\mkdir.obj"
	-@erase "$(INTDIR)\mkmodules.obj"
	-@erase "$(INTDIR)\modules.obj"
	-@erase "$(INTDIR)\myndbm.obj"
	-@erase "$(INTDIR)\ndir.obj"
	-@erase "$(INTDIR)\no_diff.obj"
	-@erase "$(INTDIR)\parseinfo.obj"
	-@erase "$(INTDIR)\patch.obj"
	-@erase "$(INTDIR)\pwd.obj"
	-@erase "$(INTDIR)\rcmd.obj"
	-@erase "$(INTDIR)\rcs.obj"
	-@erase "$(INTDIR)\rcscmds.obj"
	-@erase "$(INTDIR)\recurse.obj"
	-@erase "$(INTDIR)\release.obj"
	-@erase "$(INTDIR)\remove.obj"
	-@erase "$(INTDIR)\repos.obj"
	-@erase "$(INTDIR)\root.obj"
	-@erase "$(INTDIR)\run.obj"
	-@erase "$(INTDIR)\scramble.obj"
	-@erase "$(INTDIR)\server.obj"
	-@erase "$(INTDIR)\sockerror.obj"
	-@erase "$(INTDIR)\stack.obj"
	-@erase "$(INTDIR)\startserver.obj"
	-@erase "$(INTDIR)\status.obj"
	-@erase "$(INTDIR)\subr.obj"
	-@erase "$(INTDIR)\tag.obj"
	-@erase "$(INTDIR)\update.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vers_ts.obj"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(INTDIR)\waitpid.obj"
	-@erase "$(INTDIR)\watch.obj"
	-@erase "$(INTDIR)\woe32.obj"
	-@erase "$(INTDIR)\wrapper.obj"
	-@erase "$(INTDIR)\zlib.obj"
	-@erase "$(OUTDIR)\cvs.exe"
	-@erase "$(OUTDIR)\cvs.ilk"
	-@erase "$(OUTDIR)\cvs.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I ".\windows-NT" /I ".\lib" /I ".\src" /I ".\zlib" /I ".\diff" /D "_DEBUG" /D "_CONSOLE" /D "HAVE_CONFIG_H" /D "WIN32" /D "WANT_WIN_COMPILER_VERSION" /Fp"$(INTDIR)\cvsnt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\cvsnt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=diff\WinDebug\libdiff.lib lib\WinDebug\libcvs.lib zlib\WinDebug\libz.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\cvs.pdb" /debug /machine:I386 /out:"$(OUTDIR)\cvs.exe" 
LINK32_OBJS= \
	"$(INTDIR)\add.obj" \
	"$(INTDIR)\admin.obj" \
	"$(INTDIR)\annotate.obj" \
	"$(INTDIR)\buffer.obj" \
	"$(INTDIR)\checkin.obj" \
	"$(INTDIR)\checkout.obj" \
	"$(INTDIR)\classify.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\commit.obj" \
	"$(INTDIR)\create_adm.obj" \
	"$(INTDIR)\cvsrc.obj" \
	"$(INTDIR)\diff.obj" \
	"$(INTDIR)\edit.obj" \
	"$(INTDIR)\entries.obj" \
	"$(INTDIR)\error.obj" \
	"$(INTDIR)\expand_path.obj" \
	"$(INTDIR)\fileattr.obj" \
	"$(INTDIR)\filesubr.obj" \
	"$(INTDIR)\find_names.obj" \
	"$(INTDIR)\hash.obj" \
	"$(INTDIR)\history.obj" \
	"$(INTDIR)\ignore.obj" \
	"$(INTDIR)\import.obj" \
	"$(INTDIR)\JmgStat.obj" \
	"$(INTDIR)\lock.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\login.obj" \
	"$(INTDIR)\logmsg.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mkdir.obj" \
	"$(INTDIR)\mkmodules.obj" \
	"$(INTDIR)\modules.obj" \
	"$(INTDIR)\myndbm.obj" \
	"$(INTDIR)\ndir.obj" \
	"$(INTDIR)\no_diff.obj" \
	"$(INTDIR)\parseinfo.obj" \
	"$(INTDIR)\patch.obj" \
	"$(INTDIR)\pwd.obj" \
	"$(INTDIR)\rcmd.obj" \
	"$(INTDIR)\rcs.obj" \
	"$(INTDIR)\rcscmds.obj" \
	"$(INTDIR)\recurse.obj" \
	"$(INTDIR)\release.obj" \
	"$(INTDIR)\remove.obj" \
	"$(INTDIR)\repos.obj" \
	"$(INTDIR)\root.obj" \
	"$(INTDIR)\run.obj" \
	"$(INTDIR)\scramble.obj" \
	"$(INTDIR)\server.obj" \
	"$(INTDIR)\sockerror.obj" \
	"$(INTDIR)\stack.obj" \
	"$(INTDIR)\startserver.obj" \
	"$(INTDIR)\status.obj" \
	"$(INTDIR)\subr.obj" \
	"$(INTDIR)\tag.obj" \
	"$(INTDIR)\update.obj" \
	"$(INTDIR)\vers_ts.obj" \
	"$(INTDIR)\version.obj" \
	"$(INTDIR)\waitpid.obj" \
	"$(INTDIR)\watch.obj" \
	"$(INTDIR)\woe32.obj" \
	"$(INTDIR)\wrapper.obj" \
	"$(INTDIR)\zlib.obj" \
	".\diff\WinDebug\libdiff.lib" \
	".\zlib\WinDebug\libz.lib" \
	".\lib\WinDebug\libcvs.lib"

"$(OUTDIR)\cvs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("cvsnt.dep")
!INCLUDE "cvsnt.dep"
!ELSE 
!MESSAGE Warning: cannot find "cvsnt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "cvsnt - Win32 Release" || "$(CFG)" == "cvsnt - Win32 Debug"
SOURCE=.\src\add.c

"$(INTDIR)\add.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\admin.c

"$(INTDIR)\admin.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\annotate.c

"$(INTDIR)\annotate.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\buffer.c

"$(INTDIR)\buffer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\checkin.c

"$(INTDIR)\checkin.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\checkout.c

"$(INTDIR)\checkout.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\classify.c

"$(INTDIR)\classify.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\client.c

"$(INTDIR)\client.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\commit.c

"$(INTDIR)\commit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\create_adm.c

"$(INTDIR)\create_adm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\cvsrc.c

"$(INTDIR)\cvsrc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\diff.c

"$(INTDIR)\diff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\edit.c

"$(INTDIR)\edit.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\entries.c

"$(INTDIR)\entries.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\error.c

"$(INTDIR)\error.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\expand_path.c

"$(INTDIR)\expand_path.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\fileattr.c

"$(INTDIR)\fileattr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\filesubr.c"

"$(INTDIR)\filesubr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\find_names.c

"$(INTDIR)\find_names.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\hash.c

"$(INTDIR)\hash.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\history.c

"$(INTDIR)\history.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ignore.c

"$(INTDIR)\ignore.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\import.c

"$(INTDIR)\import.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\JmgStat.c"

"$(INTDIR)\JmgStat.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\lock.c

"$(INTDIR)\lock.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\log.c

"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\login.c

"$(INTDIR)\login.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\logmsg.c

"$(INTDIR)\logmsg.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\main.c

"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\mkdir.c"

"$(INTDIR)\mkdir.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\mkmodules.c

"$(INTDIR)\mkmodules.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\modules.c

"$(INTDIR)\modules.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\myndbm.c

"$(INTDIR)\myndbm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\ndir.c"

"$(INTDIR)\ndir.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\no_diff.c

"$(INTDIR)\no_diff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\parseinfo.c

"$(INTDIR)\parseinfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\patch.c

"$(INTDIR)\patch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\pwd.c"

"$(INTDIR)\pwd.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\rcmd.c"

"$(INTDIR)\rcmd.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\rcs.c

"$(INTDIR)\rcs.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\rcscmds.c

"$(INTDIR)\rcscmds.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\recurse.c

"$(INTDIR)\recurse.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\release.c

"$(INTDIR)\release.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\remove.c

"$(INTDIR)\remove.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\repos.c

"$(INTDIR)\repos.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\root.c

"$(INTDIR)\root.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\run.c"

"$(INTDIR)\run.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\scramble.c

"$(INTDIR)\scramble.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\server.c

"$(INTDIR)\server.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\sockerror.c"

"$(INTDIR)\sockerror.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\stack.c

"$(INTDIR)\stack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\startserver.c"

"$(INTDIR)\startserver.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\status.c

"$(INTDIR)\status.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\subr.c

"$(INTDIR)\subr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\tag.c

"$(INTDIR)\tag.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\update.c

"$(INTDIR)\update.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\vers_ts.c

"$(INTDIR)\vers_ts.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\version.c

"$(INTDIR)\version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\waitpid.c"

"$(INTDIR)\waitpid.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\watch.c

"$(INTDIR)\watch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=".\windows-NT\woe32.c"

"$(INTDIR)\woe32.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\wrapper.c

"$(INTDIR)\wrapper.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\zlib.c

"$(INTDIR)\zlib.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "cvsnt - Win32 Release"

"libdiff - Win32 Release" : 
   cd ".\diff"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdiff.mak" CFG="libdiff - Win32 Release" 
   cd ".."

"libdiff - Win32 ReleaseCLEAN" : 
   cd ".\diff"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdiff.mak" CFG="libdiff - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

"libdiff - Win32 Debug" : 
   cd ".\diff"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdiff.mak" CFG="libdiff - Win32 Debug" 
   cd ".."

"libdiff - Win32 DebugCLEAN" : 
   cd ".\diff"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdiff.mak" CFG="libdiff - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "cvsnt - Win32 Release"

"libz - Win32 Release" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libz.mak" CFG="libz - Win32 Release" 
   cd ".."

"libz - Win32 ReleaseCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libz.mak" CFG="libz - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

"libz - Win32 Debug" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libz.mak" CFG="libz - Win32 Debug" 
   cd ".."

"libz - Win32 DebugCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libz.mak" CFG="libz - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "cvsnt - Win32 Release"

"libcvs - Win32 Release" : 
   cd ".\lib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libcvs.mak" CFG="libcvs - Win32 Release" 
   cd ".."

"libcvs - Win32 ReleaseCLEAN" : 
   cd ".\lib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libcvs.mak" CFG="libcvs - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

"libcvs - Win32 Debug" : 
   cd ".\lib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libcvs.mak" CFG="libcvs - Win32 Debug" 
   cd ".."

"libcvs - Win32 DebugCLEAN" : 
   cd ".\lib"
   $(MAKE) /$(MAKEFLAGS) /F ".\libcvs.mak" CFG="libcvs - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 


!ENDIF 

