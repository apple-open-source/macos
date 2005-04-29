.\" $XConsortium: userguide.mm,v 1.12 94/09/08 13:44:28 dpw Exp $
'
.ds dD User Guide for the X Test Suite
.so 00.header
'\"
'\" Start and end of a user-typed display
'\"
.de cS
.DS I
.ft C
.ps -2
..
.de cE
.ps +2
.ft R
.DE
..
'\" # Courier
.de C
\fC\\$1\fP\\$2\fC\\$3\fP\\$4\fC\\$5\fP\\$6
..
'\"	###
.H 1 "Introduction"
This document is a guide to installing and running
\*(xR of the \*(xT.
In order to do this, please work through all of the steps described
in this guide in order. Information on the content, purpose and goals of the 
\*(xT
is found in a series of appendices, which follow the installation 
instructions.
.P
Please read the "Release Notes for the \*(xT",
which describe particular features of this release.
.P
Further information, which would be required by a programmer to modify or extend 
the \*(xT, is contained in a separate document, "Programmers Guide for the 
\*(xT".
.P
Included in this release is a version of the
"Test Environment Toolkit"
.SM ( TET ).
This is required to build and execute the \*(xT.
The "Test Environment Toolkit" 
is a software tool developed by X/Open,
UNIX International,
and the Open Software Foundation.
More details of the \s-1TET\s0 appear in appendix E.
.P
The contents of this document cover the installation and use of the included 
version of the \s-1TET\s0.
.SK
.H 1 "Preparation"
This section of the User Guide describes how to check that the system on which 
you want to build the \*(xT has the required utilities and sufficient 
disc space, how to check the version of the \*(xW\*F
.FS
The \*(xW is a trademark of the Massachusetts Institute of Technology.
.br
\*(xW Version\ 11 Release\ 4 is abbreviated to X11R4 in this document.
.br
\*(xW Version\ 11 Release\ 5 is abbreviated to X11R5 in this document.
.FE
you wish to test,
and how to extract the software from the supplied distribution media.
.H 2 "Utilities"
The \*(xT assumes that the following utilities are available on your system.
.H 3 "Bourne shell"
The configuration and building stages include example instructions which 
have only been tested using the Bourne shell.
.P
The build configuration file sets the SHELL variable so that the Bourne 
shell will be used by \fCmake\fP. No other settings for this variable 
have been tested.
.H 3 "make"
The building stages assume the existence of \fCmake\fP.
.H 3 "awk"
The report writer \fCrpt\fP uses \fCawk\fP.
.H 3 "Compiler"
A C compiler and link editor are required. The \*(xT assumes that when these 
utilities execute successfully, they return a value of zero.
The names of these utilities may be set in build
configuration parameters.
.H 3 "Library archiver"
A library archiver and a means of ordering the libraries are required.
The ordering software may be part of the library archiver, the 
\fCranlib\fP utility, or the utilities \fClorder\fP and \fCtsort\fP.
The names of these utilities may be set in build
configuration parameters.
.H 3 "File utilities"
The \*(xT uses utilities to copy, move, remove and link files during
the build stages. 
The names of these utilities may be set in build
configuration parameters.
.H 2 "Checking your version of the \*(xW"
If your version of the \*(xW supports the 
.SM XTEST
extension, you will
be able to perform tests for 
some assertions which are otherwise untestable. The
.SM XTEST
extension has been produced by MIT since the initial release of X11R5,
based on a specification\*F
.FS
Drake, K.J.,
\(lqSome Proposals for a Minimal X11 Testing Extension.\(rq
.I
UniSoft Ltd. June 1991
.FE
produced by UniSoft.
The extension provides access to the X server to enable
testing of the following areas of the \*(xW:
.DL
.LI
Those which rely on the simulation of device events.
.LI
Those requiring access to opaque client side data structures.
.LI
Those requiring information on the cursor attribute of windows.
.LE
.P
Before you configure the \*(xT, you should determine whether your 
version of the \*(xW includes the 
.SM XTEST
extension, and, if so, whether 
you wish to configure and build the \*(xT to enable these features to 
be tested.
.P
There are two things to check:
.AL
.LI
Check whether your X server supports the 
.SM XTEST
extension.
This can be done by printing the list of extensions available in your 
X server using the X utility
.C xdpyinfo .
Note - the name of the 
extension should be printed exactly as in this User Guide - 
there are other testing extensions for X which are not compatible 
with the 
.SM XTEST
extension.
.LI
Check whether you have the required libraries to link the test suite clients
so as to access the 
.SM XTEST
extension. All test suite clients must be linked with Xlib, which is 
normally named 
.C libX11.a .
If you want to access the 
.SM XTEST
extension, 
you will need two further libraries. These are the 
.SM XTEST 
library (normally named
.C libXtst.a )
and the X extension library (normally named
.C libXext.a ).
.LE
.H 2 "Installing the \*(xT"
Change to the directory in which you wish to install the distribution.
Set an environment variable \s-1TET_ROOT\s0 to the full path name of
that directory.
.P
Load the software from the media supplied into that directory.
The precise commands you should use depend on the format of the media
supplied to you, the utilities available on your system, 
the options
supported by the utilities, and 
the names of 
the tape devices on your system.
See the Release Notes for more information about installation.
.P
.SK
.H 1 "Configuring the \*(xT"
This section contains instructions on all the procedures you should 
go through in order to configure the \*(xT, before attempting to 
build it. 
.P
There is a description of the \s-1TET\s0 build tool in section 3.1, and the 
relationship between the \s-1TET\s0 build scheme and the Imake scheme in 
section 3.2.
.P
Sections 3.3 and 3.4 contain details of build and clean 
configuration parameters, which you
should edit to reflect the configuration of the target platform on which 
the \*(xT is to be built.
.P
Section 3.5 contains details of source files and include files 
which contain system dependent 
data which cannot be specified via the build configuration parameters.
You should check these files before configuration and if necessary edit
them to be suitable for your system.
.H 2 "The \s-1TET\s0 build tool"
The \s-1TET\s0 provides a scheme to execute a build tool, which builds
the tests in the \*(xT.
The execution of the build tool in the 
\s-1TET\s0
is controlled by a small number of 
\s-1TET\s0
configuration parameters, contained in a build configuration file. These
are described in section 3.3.1.
.P
A build tool has been developed and is provided as part of the \*(xT. 
This is a shell script named \fCpmake\fP, which is supplied in the directory
\fC$TET_ROOT/xtest/bin\fP. The shell script \fCpmake\fP is an interface to the 
\fCmake\fP system command, and when invoked from the 
\s-1TET\s0
it builds a test using the rules provided in a
\fCMakefile\fP.
.P
Each \fCMakefile\fP in the \*(xT
is written portably, using symbolic names to describe commands and 
parameters which may vary from system to system. The values of these
symbolic names are all obtained by \fCpmake\fP from additional parameters in
the build configuration file
which are described in sections 3.3.2-3.3.7.
.P
The \fCpmake\fP utility may be invoked directly from the shell,
as well as via the \s-1TET\s0, to build individual parts of the 
\*(xT. This is described further in subsequent sections of this guide.
.P
There is also a clean tool \fCpclean\fP which is
an interface to the system \fCmake clean\fP system command. This 
uses parameters in a clean configuration file. 
.H 2 "Relationship between \s-1TET\s0 build scheme and Imake"
The \s-1TET\s0 is designed to provide a simple and self contained
interface to configure and build tests. The \*(xT can be configured and
built with no specialised knowledge of the \*(xW beyond 
that contained in the \*(xT documentation, and using a limited set of commonly
available system commands. The only information required to configure and 
build the \*(xT is the location of the \*(xW Xlib and include files.
.P
The \*(xW itself includes a
configuration scheme which is known as \fCImake\fP.
This uses a utility \fCimake\fP supplied as part of the \*(xW 
to create \fCMakefiles\fP from portable 
description files called \fCImakefiles\fP.
.P
If you are familiar with the \fCImake\fP scheme, and have used it to configure 
and build
the \*(xW on the platform being used to build the \*(xT, you may be able 
to set a limited number of the \s-1TET\s0 build configuration variables 
described in section 3.3 to the same value you used for 
an \fCImake\fP variable. Where this 
is possible, the name of the corresponding \fCImake\fP variable is cross 
referenced.
.H 2 "Build configuration parameters"
All build configuration parameters are contained in 
a configuration file that forms part of the \s-1TET\s0.
This file should be edited to reflect the configuration of the target machine.
The file
.cS
$TET_ROOT/xtest/tetbuild.cfg
.cE
contains all the parameters that are needed to build the \*(xT.
The parameters are grouped in seven sections within the configuration file.
.H 3 "Configuration Parameters defined by the \s-1TET\s0"
None of these parameters require changing. They are already set to 
defaults which are correct for the \*(xT.
.P
.VL 15 0
.LI \s-1TET_BUILD_TOOL\s0
.br
The name of the program that the
.SM TET
will execute in build mode.
.P
You should use the \fCpmake\fP command that is supplied
in the directory \fC$TET_ROOT/xtest/bin\fP.
.cS
Eg: TET_BUILD_TOOL=pmake
.cE
.LI \s-1TET_BUILD_FILE\s0
.br
Any flags required by the build tool.
This parameter should be empty.
.cS
Eg: TET_BUILD_FILE=
.cE
.LI \s-1TET_CLEAN_TOOL\s0
.br
The name of the program that the
.SM TET
will execute in clean mode.
.P
You should use the \fCpclean\fP command that is supplied
in the directory \fC$TET_ROOT/xtest/bin\fP.
.cS
Eg: TET_CLEAN_TOOL=pclean
.cE
.LI \s-1TET_CLEAN_FILE\s0
.br
Any flags required by the clean tool. 
This parameter should be empty.
.cS
Eg: TET_CLEAN_FILE=
.cE
.LI \s-1TET_OUTPUT_CAPTURE\s0
.br
This flag is used by the \s-1TET\s0 to enable the output from the build tool 
to be saved and copied into the journal file. This line should not be 
altered.
.LE
.H 3 "Configuration for system commands"
In this section the names of system commands are specified.
.VL 15 0
.LI \s-1SHELL\s0
The following line should cause the Bourne shell to be used 
by make.
.cS
Eg: SHELL=/bin/sh
.cE
.LI \s-1CC\s0
A command to invoke the C compiler.
.cS
Eg: CC=cc
.cE
.P
\fIImake variable: CcCmd\fP
.LI \s-1RM\s0
A command to remove a file without interactive help.
.cS
Eg: RM=rm -f
.cE
.P
\fIImake variable: RmCmd\fP
.LI \s-1AR\s0
A command to generate a library archive.
.cS
Eg: AR=ar crv
.cE
.P
\fIImake variable: ArCmd\fP
.LI \s-1LD\s0
A command to link object files.
.cS
Eg: LD=ld
.cE
.P
\fIImake variable: LdCmd\fP
.LI \s-1LN\s0
A command to make hard links.
.cS
Eg: LN=ln
.cE
.P
\fINB: This does not correspond to the Imake variable: LnCmd\fP
.LI \s-1RANLIB\s0
If the system supports a command to order library 
archives into random access libraries, then set the parameter to that command.
Otherwise it should be set to \fCecho\fP (or a command that does nothing).
.cS
Eg: RANLIB=ranlib
.cE
.P
\fIImake variable: RanlibCmd\fP
.LI \s-1TSORT\s0
Set to \fCcat\fP if AR was set to a command which inserts a symbol table
in the library archive,
or if RANLIB was set to a command which creates a random access library,
otherwise set to \fCtsort\fP.
.LI \s-1LORDER\s0
Set to \fCecho\fP if AR was set to a command which inserts a symbol table
in the library archive,
or if RANLIB was set to a command which creates a random access library.
otherwise set to \fClorder\fP.
.LI \s-1CP\s0
A command to copy files.
.cS
Eg: CP=cp
.cE
.P
\fIImake variable: CpCmd\fP
.LI \s-1CODEMAKER\s0
A utility to produce C source files from dot-m files.
The supplied utility \fCmc\fP should always be used.
This line should not be altered.
.cS
Eg: CODEMAKER=mc
.cE
.LE
.H 3 "Configuration for the \s-1TET\s0"
This section contains the locations of various parts of the \s-1TET\s0.
Usually only the first four parameters will need changing,
unless files have been moved from their default locations.
.VL 15 0
.LI \s-1TET_ROOT\s0
The directory that contains all the files in the \*(xT.
This should be set to the path to which \fCTET_ROOT\fP was set (see the
section entitled "Installing the \*(xT").
It must be written out as a full path without using any variable notation.
.LI \s-1TETBASE\s0
The directory that contains all the files in the \s-1TET\s0 system.
This is used for convenience in defining the other directories.
This should be set to \fC${TET_ROOT}/tet\fP.
.LI \s-1PORTINC\s0
An option that can be given to the C compiler
that will cause it to search all directories that are
required to allow portability to systems that do not support
.SM POSIX .
Should be empty for
.SM POSIX
systems.
If compiling on a \s-1BSD\s0 system using the supplied compatibility library,
then the following line should be used.
(See section entitled "The portability library").
.cS
Eg: PORTINC=-I${TET_ROOT}/port/INC
.cE
.LI \s-1PORTLIB\s0
A library containing
.SM POSIX.1
and C library functions that
are not supplied by the system.
This should be empty for a
.SM POSIX
system.
If compiling on a \s-1BSD\s0 system using the supplied compatibility library,
then the following line should be used.
(See section entitled "The portability library").
.cS
Eg: PORTLIB=${TET_ROOT}/port/libport.a
.cE
.LI \s-1TETINCDIR\s0
The directory containing the
.SM TET
headers.
.cS
Eg: TETINCDIR=${TETBASE}/inc/posix_c
.cE
.LI \s-1TETLIB\s0
The directory containing the 
.SM TET
library.
.cS
Eg: TETLIB=${TETBASE}/lib/posix_c
.cE
.LI \s-1TCM\s0
The Test Control Manager.
This is part of the \s-1TET\s0.
It is an object file that is linked with each test.
.cS
Eg: TCM=${TETLIB}/tcm.o
.cE
.LI \s-1TCMCHILD\s0
The Test Control Manager.
This is part of the \s-1TET\s0.
It is an object file that is linked with each program that is 
executed within a test by tet_exec().
.cS
Eg: TCMCHILD=${TETLIB}/tcmchild.o
.cE
.LI \s-1APILIB\s0
The \s-1TET\s0 \s-1API\s0 library.
.cS
Eg: APILIB=${TETLIB}/libapi.a
.cE
.LE
.H 3 "Configuration parameters for the \*(xT"
Only the first two of these parameters require changing unless
directories have been moved from their default locations.
.VL 15 0
.LI \s-1XTESTHOST\s0
The name of the host on which test suite clients are to be executed.
This may be set to the value returned by a command which can be executed
using the PATH you have set on your host, or may be set to a specific name.
This is used to produce a resource file named .Xdefaults-$(\s-1XTESTHOST\s0)
in the test execution directory.
The resource file is created when building the test for XGetDefault.
This parameter is only used in the Makefile of the test for XGetDefault.
.cS
Eg. XTESTHOST=`hostname`
Eg. XTESTHOST=`uname -n`
Eg. XTESTHOST=triton
.cE
.LI \s-1XTESTFONTDIR\s0
The directory in which to install the test fonts.
.cS
Eg: XTESTFONTDIR=/usr/lib/X11/fonts/xtest
.cE
.LI \s-1XTESTROOT\s0
The directory that is the root of the \*(xT.
.cS
Eg: XTESTROOT=${TET_ROOT}/xtest
.cE
.LI \s-1XTESTLIBDIR\s0
The directory containing libraries for the \*(xT.
.cS
Eg: XTESTLIBDIR=${XTESTROOT}/lib
.cE
.LI \s-1XTESTLIB\s0
The xtest library.
This library contains
subroutines that are common to many tests in the \*(xT.
.cS
Eg: XTESTLIB=${XTESTLIBDIR}/libxtest.a
.cE
.LI \s-1XSTLIB\s0
The X Protocol test library.
This library contains
subroutines that are common to many tests in the X Protocol section 
of the \*(xT.
.cS
Eg: XSTLIB=${XTESTLIBDIR}/libXst.a
.cE
.LI \s-1XTESTFONTLIB\s0
The fonts library.
This library contains
font descriptions that are common to many tests in the \*(xT.
.cS
Eg: XTESTFONTLIB=${XTESTLIBDIR}/libfont.a
.cE
.LI \s-1XTESTINCDIR\s0
The xtest header file directory.
This directory contains headers that are local to the \*(xT.
.cS
Eg: XTESTINCDIR=${XTESTROOT}/include
.cE
.LI \s-1XTESTBIN\s0
The xtest binary file directory.
This directory contains utility programs that are used by \*(xT.
.cS
Eg: XTESTBIN=${XTESTROOT}/bin
.cE
.LE
.H 3 "System Parameters"
Location of system libraries and include files.
.VL 15 0
.LI \s-1SYSLIBS\s0
Options to cause the C compiler to search
any system libraries that are required for the \*(xT
that are not searched by default.
This will probably include Xlib.
.cS
Eg: SYSLIBS=-lX11
.cE
If you wish to build the \*(xT to make use of the 
.SM XTEST
extension, you
will need to include the 
.SM XTEST
library and the X\ extension library (in that order).
.cS
Eg: SYSLIBS=-lXtst -lXext -lX11
.cE
.P
\fIImake variables: ExtraLibraries\fP
.LI \s-1XP_SYSLIBS\s0
Any system libraries that are needed, to link the
X Protocol tests. This will include Xlib, since libXst.a
(which is part of the test suite) will include at least one call to
XOpenDisplay.
.cS
Eg: XP_SYSLIBS=-lX11
.cE
.P
\fIImake variables: ExtraLibraries\fP
.LI \s-1SYSINC\s0
Any commands that should be given to the C compiler
to cause all relevant system include files to be included.  This will
probably
include /usr/include/X11.
.cS
Eg: SYSINC=-I/usr/include/X11
.cE
.LE
.H 3 "C Compiler Directives"
Directives to the C compiler.
Usually only the first four parameters will need changing. The
remainder are internally used parameters, which are an amalgam of previously
set parameters.
.VL 15 0
.LI \s-1COPTS\s0
Options to the C compiler.
.cS
Eg: COPTS=-O
.cE
.P
\fIImake variables: DefaultCDebugFlags and DefaultCCOptions\fP
.LI \s-1DEFINES\s0
Options required by the C compiler to set up any required defines.
For example in strict
.SM ANSI
Standard-C systems you will need to define
.SM _POSIX_SOURCE .
Additionally on an
.SM X/Open
conformant system it may be necessary to define
.SM _XOPEN_SOURCE .
.cS
Eg: DEFINES=-D_POSIX_SOURCE
.cE
.P
If there is no symbol
.C NSIG
defined in the system header file 
\fCsignal.h\fP, then this has to be supplied for
use by the \s-1TET\s0 \s-1API\s0.
It should be the number of signal types on the system.
.cS
Eg: DEFINES=-D_POSIX_SOURCE -DNSIG=32
.cE
If you wish to build the \*(xT to make use of the 
.SM XTEST
extension, you
will need to define 
.SM XTESTEXTENSION .
.br
.SM XTESTEXTENSION
is only used when building the \*(xT library.
.cS
Eg: DEFINES=-D_POSIX_SOURCE -DNSIG=32 -DXTESTEXTENSION
.cE
.P
\fIImake variables: StandardDefines\fP
.LI \s-1XP_DEFINES\s0
C compiler defines specific to the X Protocol tests.
.br
This can be set as DEFINES, but
you can build support for additional connection methods beyond TCP/IP,
using the following defines, if XP_OPEN_DIS is
XlibNoXtst.c (R4/R5 XOpenDisplay emulation):
.cS
-DDNETCONN - Connections can also use DECnet\*F.
-DUNIXCONN - Connections can also use UNIX\*F domain sockets.
.cE
.br
Refer to your documentation for building and installing Xlib on
your platform.
.FS
DEC and DECnet are registered trademarks of Digital
Equipment Corporation.
.FE
.FS
UNIX is a registered trademark of UNIX System Laboratories, Inc. in
the U.S. and other countries.
.FE
.br
If XP_OPEN_DIS is one of XlibXtst.c or XlibOpaque.c then none of
the defines listed above will be required.
.cS
Eg: XP_DEFINES=-D_POSIX_SOURCE -DUNIXCONN
.cE
.P
\fIImake variables: StandardDefines\fP
.LI \s-1LINKOBJOPTS\s0
Options to give to the LD program to link object
files together into one object file that can be further linked.
.cS
Eg: LINKOBJOPTS=-r
.cE
.LI \s-1INCLUDES\s0
Options to cause C compiler to search the correct directories
for headers.
This should not need changing as it is just an amalgam of
other parameters.
.cS
.ps -2
INCLUDES=-I. ${PORTINC} -I${TETINCDIR} -I${XTESTINCDIR} ${SYSINC}
.ps +2
.cE
.LI \s-1CFLAGS\s0
Flags for the C compiler.
This should not need changing as it is just an amalgam of other
parameters.
Note that \fC\s-1CFLOCAL\s0\fP is not defined in the configuration file;
it is available for use in makefiles, to define
parameters that only apply to a particular case.
(It intentionally uses parentheses rather than braces)
.cS
.ps -2
CFLAGS=$(CFLOCAL) $(COPTS) $(INCLUDES) $(DEFINES)
.ps +2
.cE
.LI \s-1XP_CFLAGS\s0
Flags for the C compiler.
This parameter is used by the \*(xP in the \*(xT.
This should not need changing as it is just an amalgam of other
parameters.
.cS
.ps -2
XP_CFLAGS=$(CFLOCAL) $(COPTS) $(INCLUDES) $(XP_DEFINES)
.ps +2
.cE
.LI \s-1LDFLAGS\s0
Flags used by the loader.
This is needed on some systems 
to specify options used when object files are linked to produce an executable.
.cS
.ps -2
Eg. LDFLAGS=-ZP
.ps +2
.cE
.LI \s-1LIBS\s0
List of libraries.
This should not need changing as it is just an amalgam of other
parameters.
.cS
.ps -2
LIBS=${XTESTLIB} ${XTESTFONTLIB} ${APILIB} ${PORTLIB}
.ps +2
.cE
.LI \s-1XP_LIBS\s0
List of libraries.
This parameter is used by the \*(xP in the \*(xT.
This should not need changing as it is just an amalgam of other
parameters.
.cS
.ps -2
XP_LIBS=${XSTLIB} ${XTESTLIB} ${XTESTFONTLIB} ${APILIB} ${PORTLIB}
.ps +2
.cE
.LI \s-1XP_OPEN_DIS\s0
A choice of which code to build in the X Protocol library 
to make an X server connection.
This must be set to one of three possible values:
.VL 4 0
.LI XlibXtst.c
.br
Use this option only if your Xlib includes post R5 enhancements 
to _XConnectDisplay 
ensuring maximum portable protocol test coverage. These enhancements include
arguments to _XConnectDisplay to return authorisation details on 
connection. If you use this option when your Xlib does not have these 
enhancements to _XConnectDisplay, the results of running the \*(xP 
will be 
.B undefined .
.LI XlibOpaque.c
.br
You have a normal R4 Xlib or early R5 Xlib which you 
cannot patch to include the enhancements to 
_XConnectDisplay, and you cannot emulate these by 
building XlibNoXtst.c, so only client-native testing 
can be done portably, and no failure testing of 
XOpenDisplay can be done.
This option uses XOpenDisplay to make the connection, 
from which the file descriptor is recovered for our own use. 
XCloseDisplay shuts down the connection.
.LI XlibNoXtst.c
.br
As for XlibOpaque.c but you can use the R4/R5 
connection emulation supplied. (Note: R4/R5 independent)
This will ensure maximum protocol test coverage
but may not be portable to all platforms.
.LE
.P
Reasons for not being able to build XlibNoXtst.c might include:
.br
i)  different interfaces to connection setup and connection read/write;
.br
ii) different access control mechanisms.
.br
Refer to your Xlib documentation for further details.
.cS
Eg. XP_OPEN_DIS=XlibOpaque.c
.cE
.LE
.H 3 "Pixel validation section"
This section defines a number of parameters that are used only when
generating known good image files. These are not intended to be modified 
and need not be used when running the test suite.
They are only used in the development environment
at UniSoft when generating known good image files.
.H 2 "Clean configuration parameters"
The \s-1TET\s0 provides a scheme to execute a clean tool, which removes
previously built tests and object files.
.P
All clean configuration parameters are contained in a configuration file that
forms part of the \s-1TET\s0.
The file
.cS
$TET_ROOT/xtest/tetclean.cfg
.cE
contains all the parameters that are needed to clean the \*(xT.
.P
To save configuration effort, 
we have arranged that the build and clean configuration files
may contain identical parameter settings. Both files are needed,
since the 
.SM TET
requires both a default build and clean configuration file.
.P
Copy the build configuration file into the clean configuration file:
.cS
cd $TET_ROOT/xtest
cp tetbuild.cfg tetclean.cfg
.cE
.H 2 "System dependent source files"
This section describes source files and include files provided 
in the \*(xT which contain data,
which you may need to edit to reflect the system under test.
.H 3 "Host address structures"
The file \fCxthost.c\fP
in the directory \fC$TET_ROOT/xtest/src/lib\fP
contains three items, which you may need to edit to reflect the system 
under test. These are all related to the mechanisms provided by the X
server under test to add, get or remove hosts from the access control list.
These are only used in the tests
for those Xlib functions which use or modify the access control list.
.P
The host access control functions use the XHostAddress structure.  You
should refer to the Xlib documentation for your system, to determine
the allowed formats for host addresses in an XHostAddress structure.
You may also find it helpful to refer to the X Window System Protocol
documentation supplied with the X server under test. The section
describing the ChangeHosts protocol request gives examples of host
address formats supported by many X servers.  The symbols FamilyInternet,
FamilyDECnet, FamilyChaos and FamilyUname are defined on many systems
in the include files X.h and Xstreams.h. The X server under test is not
guaranteed to support these families, and may support families not
listed here. You should find out which families are supported for the X
server under test, by examining the header files supplied with your
system, and consulting the documentation supplied with the X server.
.P
Some default declarations are contained in the file, but there is no 
guarantee that they will work correctly on your system.
.P
The three items are as follows:
.AL
.LI
You should ensure that there is a declaration for an
array xthosts[] of at least 5 XHostAddress structures containing
valid family,length,address triples.
.LI
You should ensure that there is a declaration for an
array xtbadhosts[] of at least 5 XHostAddress structures containing
invalid family,length,address triples.
You should ensure that there is a declaration for an array
xtbadhosts[] of at least 5 XHostAddress structures containing invalid
family,length,address triples. If you cannot use the supplied examples,
the simplest way to do this is to use an invalid family, which is not
supported by the X server under test, in each structure of the array.
.LI
You should ensure that there is a declaration for a
function samehost() that compares two XHostAddress structures
and returns True if they are equivalent. (It is unlikely that the sample 
function will need modification - no systems requiring modification have 
yet been identified).
.LE
.SK
.H 1 "Building the \s-1TET\s0"
The \*(xT
runs under the 
Test Environment Toolkit
(\s-1TET\s0).
.P
This section of the User Guide tells you how to build and install the 
supplied version of the
.SM TET .
.P
The following instructions assume the use of a Bourne shell.
.P
The \s-1PATH\s0 variable should have the directory \fC$TET_ROOT/xtest/bin\fP
prepended to it.
.cS
PATH=$TET_ROOT/xtest/bin:$PATH
export PATH
.cE
.H 2 "The portability library"
The current version of the \s-1TET\s0 used by the \*(xT
is designed to run on a
\s-1POSIX.1\s0
system.
.P
Since many systems running the \*(xW
currently run on \s-1BSD\s0 based systems a portability library
that emulates the required routines using \s-1BSD\s0 facilities has
been provided.
.P
This library is not part of the \s-1TET\s0 itself.
.P
The portability library source is kept in \fC$TET_ROOT/port\fP.
.P
The portability library may be useful in porting the \*(xT to other
environments as described below.
Please refer to the "Release Notes for the \*(xT"
for details of the target execution environments, 
and a list of systems to which it has already been ported.
.H 3 "Porting to a POSIX.1 system"
If your system conforms to
.SM POSIX.1
and has an
.SM ANSI
Standard-C compiler then this library should not be built
and so this section can be skipped.
The exception is that \fCputenv()\fP may not exist on a
\s-1POSIX.1\s0\*F
.FS
IEEE Std 1003.1-1990, \fIPortable Operating System Interface for
Computer Environments\fR
.FE
system, however it is in the
\s-1SVID\s0\*F
.FS
System V Interface Definition, Issue 1, AT&T, Spring 1985.
.FE
and the
\s-1XPG\s0\*F,
.FS
\fIX/Open Portability Guide Issue 3, Volume 2: XSI System Interface and Headers\fR
.FE
so in practice most
.SM non- BSD
machines will have this function.
.H 3 "Porting to a BSD system"
If the system is a standard \s-1BSD\s0 one,
then the portability library can be used as it is; build
it as follows.
.cS
cd $TET_ROOT/port
pmake
.cE
.H 3 "Porting to other systems"
The portability
library may be useful as a base for porting the \s-1TET\s0 to other
.SM non- POSIX
systems,
however the portability library is designed to run on a \s-1BSD\s0 system,
and will not necessarily build without change on other systems.
.P
The following routines are emulated for use under a \s-1BSD\s0 system,
and these may be needed on other systems:
.cS
getcwd() getopt() putenv() sigaction()
sigaddset() sigdelset() sigemptyset() sigfillset()
sigismember() sigpending() sigprocmask() sigsuspend()
strchr() strcspn() strftime() strpbrk() strrchr()
strspn() strtok() toupper() upcase()
vsprintf() waitpid() 
.cE
Only the features that are used by the \*(xT are emulated.
They are not meant to mimic completely the standard behaviour.
.P
There is also an include directory 
.C $TET_ROOT/port/INC
that contains header files
that are required that are not found on a \s-1BSD\s0 system.
These files contain only items that are needed for the \*(xT,
they are not designed to replace completely the standard ones.
.P
To adapt the portability library to other systems, 
the following hints may be found useful:
.BL
.LI
Examine the directory 
.C $TET_ROOT/port/INC .
If the system already provides a standard conforming header file
of the same name
as one in the \s-1INC\s0 directory, then remove the version
from the \s-1INC\s0 directory.
.LI
The header files contain the bare minimum required to compile the
\*(xT, and use \s-1BSD\s0 features.
It may be necessary to alter them to suit the local system.
This applies particularly to
.C signal.h .
.LI
It may be necessary to add other header files.
.LI
In the library \fCMakefile\fP remove any function that is already provided by
the system in a standard conforming form.
.LI
Examine the code of the remaining functions to make
sure that they will work on the target system.
.LE
.P
.H 2 "Building libraries and utilities"
There is a top level Makefile which can be used to automatically perform 
a number of the following steps. You should still check through the User Guide
and perform the steps which need to be done manually. In particular you
need to build
and install the test fonts as described in the section entitled 
"Compiling and installing the test fonts".
.P
The top level Makefile enables the following steps to be performed:
.P
Building the \s-1TET\s0:
.AL
.LI
The Test Case Controller (TCC)
.LI
The API library
.LE
.P
Building the X test suite libraries and utilities:
.AL
.LI
Building the X test suite library
.LI
Building the X Protocol library
.LI
Building the X test fonts library
.LI
Building the mc utility
.LI
Building the blowup utility
.LE
.P
To use the top level Makefile, move to the top level directory:
.cS
cd $TET_ROOT
.cE
.P
Make the utilities and libraries with the command:
.cS
make
.cE
.H 2 "The Test Case Controller (TCC)"
Move to the directory containing the \s-1TCC\s0 source.
.cS
cd $TET_ROOT/tet/src/posix_c/tools
.cE
.P
Make the
.SM TCC
with the command:
.cS
pmake install
.cE
.P
Note: the supplied version of the \s-1TCC\s0 
assumes that the \fIcp\fP utility on your system supports recursive 
copy using the option \fI-r\fP.  There are two occurrences of \fIcp\fP in
the file \fCexec.c\fP which use this option.
.P
In the \*(xT, recursive copying is not required.
.P
If your system does not support this option, you can remove the use of 
this option in the source code 
before building the \s-1TCC\s0. If you do this you may 
not be able to use the supplied \s-1TCC\s0 with other test suites.
.P
Alternatively, you can provide a shell script in the directory 
\fC$TET_ROOT/xtest/bin\fP
which copies files using \fIcp\fP but ignores any option \fI-r\fP.
.H 2 "The \s-1API\s0 library"
Move to the 
.SM API
library source directory.
.cS
cd $TET_ROOT/tet/src/posix_c/api
.cE
.P
Run the command
.cS
pmake install
.cE
which should produce the files \fClibapi.a\fP and the Test
Case Manager files \fCtcm.o\fP and \fCtcmchild.o\fP.
.H 1 "Building the X test suite libraries and utilities"
.H 2 "The X test suite library"
A library of common subroutines for the \*(xT
has source in \fC$TET_ROOT/xtest/src/lib\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/src/lib
pmake install
.cE
.P
The list of source files in this library is described in the 
"Programmers Guide".
.H 2 "The X Protocol library"
A library of common subroutines for the \*(xP in the \*(xT
has source in \fC$TET_ROOT/xtest/src/libproto\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/src/libproto
pmake install
.cE
.P
The list of source files in this library is described in the 
"Programmers Guide".
.H 2 "The X test fonts library"
A library of common subroutines defining the 
characteristics of the test fonts for the \*(xT
has source in \fC$TET_ROOT/xtest/fonts\fP.
This is built automatically when building tests in the \*(xT.
Should it be required to build it separately for any reason
run the command.
.cS
cd $TET_ROOT/xtest/fonts
pmake install
.cE
.P
The list of source files in this library is described in the 
"Programmers Guide".
.P
Note that the directory 
\fC$TET_ROOT/xtest/fonts\fP also contains the test fonts themselves in
bdf format, which must be compiled and installed. Instructions for 
performing these steps are included in the next section entitled
"Compiling and installing the test fonts".
.H 2 "Compiling and installing the test fonts"
The \*(xT contains a series of test fonts which are used 
to test the correctness of the information returned by the graphics 
functions in the \*(xW. This is done by comparing the information
returned by those functions with the expected font characteristics 
which are compiled into the tests via the X test fonts library.
The X test fonts library is described in an earlier section of this document.
.P
There are seven test fonts whose descriptions are contained in the files
.cS
xtfont0.bdf	xtfont1.bdf	xtfont2.bdf
xtfont3.bdf	xtfont4.bdf	xtfont5.bdf
xtfont6.bdf
.cE
.P
These files are located in the directory \fC$TET_ROOT/xtest/fonts\fP.
.P
The manner in which fonts should be compiled and installed for any 
particular X server is system dependent, and you should refer to 
the instructions supplied with your release of the \*(xW for 
details of how to do this.
.P
Some sample instructions are given here which may be useful on many 
systems. These may not be appropriate for your system, or they may 
need adaptation to work properly on your system and so are provided 
only as a guide.
.AL
.LI
Move to the directory \fC$TET_ROOT/xtest/fonts\fP.
.cS
cd $TET_ROOT/xtest/fonts
.cE
.LI
Compile the seven \fCbdf\fP files into \fCsnf\fP, \fCpcf\fP, or \fCfb\fP
format, as appropriate for your system.
.cS
pmake comp_snf
.cE
or
.cS
pmake comp_pcf
.cE
or
.cS
pmake comp_dxpcf
.cE
or
.cS
pmake comp_fb
.cE
.LI
Copy the compiled fonts into the server font directory
(the \fCXTESTFONTDIR\fP configuration parameter).
.cS
pmake install_snf
.cE
or
.cS
pmake install_pcf
.cE
or
.cS
pmake install_dxpcf
.cE
or
.cS
pmake install_fb
.cE
.LE
.H 2 "Building the \fCmc\fP utility"
The 
.C mc 
utility is used to generate test set source files and Makefiles
from a template file, known as a dot-m file. 
The file naming scheme is described further in appendix B.
The file formats are described further in the "Programmers Guide".
.P
The Makefiles and test set source files will be created using 
.C mc 
whenever test sets are built, if the dot-m file is found to be 
newer than the source file or Makefile, or if these files do not 
exist.
.P
Build 
.C mc 
and install in the xtest bin directory as follows.
.cS
cd $TET_ROOT/xtest/src/bin/mc
pmake install
cd $TET_ROOT/xtest/src/bin/mc/tmpl
pmake install
.cE
.H 2 "Building the blowup utility"
The blowup utility is required for examining any incorrect image files 
generated by the X server during a test run. Instructions for running the
blowup program are given in the section entitled "Examining image files".
.P
Build 
.C blowup 
and install in the xtest bin directory as follows.
.cS
cd $TET_ROOT/xtest/src/pixval/blowup
pmake install
.cE
.SK
.H 1 "Building the tests"
.H 2 "Building tests using the \s-1TET\s0"
The entire \*(xT can be built by using the 
build mode of the \s-1TCC\s0. In this mode, the build configuration
parameters in the file 
.C $TET_ROOT/xtest/tetbuild.cfg 
are used to build each test 
set in the \*(xT separately.
.P
.cS
cd $TET_ROOT/xtest
tcc -b [ -s scenario_file ]  [ -j journal_file ] [ -y string ] xtest all
.cE
.VL 5 0
.LI \fC-b\fP
.br
This invokes the \s-1TCC\s0 in build mode.
(If you have just finished building the \s-1TCC\s0 from the \fCcsh\fP,
you will probably have to \fCrehash\fP to get \fCtcc\fP in your path.)
.LI "\fC-s scenario_file\fP"
.br
This option builds the test sets in the named scenario file.
The default is a file named \fCtet_scen\fP in the directory 
\fC$TET_ROOT/xtest\fP.
For more details refer to the section entitled 
"Building modified scenarios using the \s-1TET\s0".
.LI "\fC-j journal_file\fP"
.br
This option sends the output of the build to the named journal file.
The default is a file named \fCjournal\fP in a newly created sub-directory 
of \fC$TET_ROOT/xtest/results\fP. Sub-directories are created with sequential
four digit numbers, with the 
.SM TCC
flags (in this case "b") appended.
The \s-1TCC\s0 will exit if the specified journal file already exists, thus the
journal file should be renamed or removed
before attempting to execute the \s-1TCC\s0.
.LI "\fC-y string\fP"
.br
This option only builds tests which include the specified string in the 
scenario file line. This may be used to build specific sections or individual
test sets.
.LI \fCxtest\fP
.br
This is the name of the test suite.
It determines the directory under $\s-1TET_ROOT\s0
where the test suite is to be found.
.LI \fCall\fP
.br
This is the scenario name in the default scenario file
\fC$TET_ROOT/xtest/tet_scen\fP.
For more details refer to the section entitled 
"Building modified scenarios using the \s-1TET\s0"
.LE
.P
This will execute the \s-1TET\s0 build tool in the \s-1TET\s0 configuration 
variable 
.SM TET_BUILD_TOOL 
(which is normally pmake),
in each test set directory of the \*(xT.
.P
The journal file should be examined to verify
that the build process succeeded.
The report writer \fCrpt\fP cannot interpret the contents 
of a journal file produced during the build process.
.P
Note: If the \s-1TCC\s0
terminates due to receipt of a signal which 
cannot be caught, the \s-1TCC\s0
may leave lock files in the test source directories. Subsequent 
attempts to restart the \s-1TCC\s0 may give error messages 
saying that a lock file was encountered. At this point 
\s-1TCC\s0
may suspend the build. It may be 
necessary to find and remove files or directories named \fCtet_lock\fP 
before continuing.
.H 3 "Signal handling in the \s-1TET\s0"
An interrupt signal (caused for example by typing the 
system interrupt character on the controlling terminal)
will cause the \s-1TCC\s0 to abort the currently executing test case. The 
journal file output records the fact that the test case was interrupted.
.P
Any other signal which can be caught by the \s-1TCC\s0 causes it to terminate.
By default, the system suspend character will also cause the \s-1TCC\s0
to terminate. If you wish to be able to suspend the \s-1TCC\s0, 
you can add the relevant signals to the parameter SIG_LEAVE
in the Makefile for the \s-1TCC\s0. 
Signals in this list will not be caught, but will cause their
default action.
This is explained further in the Test Environment Tookit Release Notes.
.H 2 "Building, executing and cleaning tests using the \s-1TET\s0"
Each test in the \*(xT may be built, executed and cleaned before 
the next test set in the scenario. This mode of use has the advantage that 
the entire \*(xT may be executed, without necessarily building all the 
test sets in advance. This mode of use has the disadvantage that you will
need to rebuild a test set before rerunning, which will take 
considerably longer than when it is built in advance.
.P
To do this, skip to the section entitled "Executing the \*(xT", 
and refer to the instructions in the sub-section entitled 
"Building, executing and cleaning tests using the \s-1TET\s0"
.H 2 "Building modified scenarios using the \s-1TET\s0"
.H 3 "Format of the scenario file"
The \s-1TET\s0 uses a scenario file to determine which test sets to build.
The file 
\fC$TET_ROOT/xtest/tet_scen\fP
is the default scenario file.
The format is basically a scenario name starting in column one,
followed by list of test sets to be built (each starting beyond column one).
Only one scenario named "all" is provided in the default scenario file.
.P
The names of the test sets are given relative to the directory 
$TET_ROOT/xtest, and must commence with a leading slash.
.H 3 "Modifying the scenario file"
The file 
\fC$TET_ROOT/xtest/tet_scen\fP
may be modified by removing lines corresponding to test sets which are not 
wanted. These will then simply not be built by the \s-1TCC\s0.
Alternatively, unwanted
lines may be commented out by placing \fC#\fP in column one of a line.
.P
It is recommended that the supplied scenario file should be saved if it is
modified.
.H 3 "Creating new scenario files"
A new scenario file may be created in the directory
\fC$TET_ROOT/xtest\fP.
The \s-1TCC\s0 will use this scenario file instead of the file 
\fC$TET_ROOT/xtest/tet_scen\fP
if it is passed via the \fC-s\fP option. For example
.cS
cd $TET_ROOT/xtest
tcc -b -s scenario_file [ -j journal_file ] [ -y string ] xtest all
.cE
.H 2 "Building tests without using the \s-1TET\s0"
See section 11, entitled
"Building, executing and reporting tests without using the \s-1TET\s0".
.H 2 "Building tests in space-saving format"
It is possible to build the tests in the \*(xT such that 
all the executable files in one section are links to a single 
executable file. This normally allows a considerable reduction 
in the disc space requirements for the \*(xT when fully built.
.P
Note that the names of the files built in space-saving format 
are different to the names of the separate executable files 
built using the instructions in previous sections. There is 
nothing to prevent both sets of executables being built (although
there is no value in this, and unnecessary disc space will be consumed).
.H 3 "Building tests in space-saving format using the \s-1TET\s0"
Before reading this section, read the section entitled "Building 
the tests using the \s-1TET\s0". This gives an explanation of the build mode 
of the \s-1TET\s0, and the structure of scenario files.
.P
A scenario named \fClinkbuild\fP is provided in a scenario file 
named \fClink_scen\fP in the directory 
$TET_ROOT/xtest. 
This enables the \s-1TCC\s0 to build the space-saving
executable files and create all the required links for each test set in
each section of the \*(xT. 
The -y option allows a particular space-saving 
executable for a single section to be built.
.P
Execute the command:
.cS
cd $TET_ROOT/xtest
tcc -b -s link_scen [ -j journal_file ] [ -y string ] xtest linkbuild
.cE
This command will execute the \s-1TET\s0 build tool in the \s-1TET\s0 configuration 
variable
.SM TET_BUILD_TOOL
(which is normally pmake),
in the top level directory of each section of the test suite.
.H 3 "Building tests in space-saving format without using the \s-1TET\s0"
This section describes how to build the space-saving
executable files for a particular section of the 
\*(xT directly without using the \s-1TET\s0.
.P
This can be simply done by calling pmake in the required directory.
For example, to build all the space-saving executable files for
section 5 of the \*(xT, execute the command:
.cS
cd $TET_ROOT/xtest/tset/CH05
pmake
.cE
.SK
.H 1 "Executing the \*(xT"
Once you have built the \*(xT as described in the previous sections,
work through the following sections to execute the tests.
.H 2 "Setting up your X server"
The first step is to ensure that the X server to be tested is correctly set up.
.H 3 "Formal verification testing"
A number of the tests within the \*(xT can only give reliable results 
if there is no window manager and no other clients making connections to
the X server. Thus, when conducting formal verification tests, there 
should be no window manager and no other clients connected to the X server.
.P
It is recommended that you close down and restart your X server before 
a formal verification test run, in order to ensure that results produced 
are repeatable and are not affected by earlier tests, although this is 
not strictly necessary.
.P
You should switch off the
screen saver if possible before starting formal verification tests.
This is because some X servers implement the screen saver in a way
which interferes with windows created by test suite clients, which may cause
misleading results. If the screen saver cannot be switched off, the 
time interval should be set so large as to prevent interference with the tests.
.P
You should also ensure that access control is disabled for the server 
under test, so that the test suite can make connections to the server.
Also (if the X server allows this) you should ensure that clients on the 
host system (as specified in the build 
configuration parameter 
.SM XTESTHOST ) 
can modify the access control list.
Some X servers support the -ac option which disables host-based access 
control mechanisms. If this option is supported, you should use it.
.H 3 "Informal testing and debugging"
Although no guarantee can be made that the tests within the \*(xT will give
correct results if there are window managers and other clients connected
to the X server, it is still possible to run many tests satisfactorily.
.P
This section gives some guidelines which may be helpful in running tests 
with a window manager present, and still deriving correct results.
The guidelines have been derived from the experience gained
during the development of the tests.
.P
Using these guidelines in connection with the instructions
in section 11,
entitled
"Building, executing and reporting tests without using the \s-1TET\s0",
gives a rapid means to investigate the results of particular tests in detail.
.P
.AL
.LI
Set XT_DEBUG_OVERRIDE_REDIRECT=Yes
in your execution configuration file. This is described in more detail in the 
next section.
.LI
Do not raise any windows on top of those created by
running tests.
.LI
Avoid having any windows at position (0,0). Note that some window managers
such as \fCtvtwm\fP
create their own "root" window at position (0,0).
This mainly affects tests for section 8 of the X11R4 Xlib specifications.
.LI
Be prepared to lose the input focus when tests are running
and don't forcibly restore it.
This mainly affects tests for section 8 of the X11R4 Xlib specifications.
.LE
.H 2 "Execute configuration parameters"
The next step is to set up the execution configuration file.
.P
All execution configuration parameters are contained in 
a configuration file that
forms part of the \s-1TET\s0.
This file should be edited to reflect the configuration of the X server 
to be tested and the underlying operating system on which Xlib
is implemented.
The file
.cS
\fC$TET_ROOT/xtest/tetexec.cfg\fP
.cE
contains all the parameters that are needed to execute the \*(xT.
The parameters are grouped in eight sections within the configuration file.
.P
Numeric execution parameters may be specified in decimal,
octal, or hexadecimal. Octal values must be a sequence of octal digits 
preceded by 0. Hexadecimal values must be a sequence of hexadecimal digits 
preceded by 0x or 0X.
.H 3 "Configuration parameters defined by the \s-1TET\s0"
.VL 5 0
.LI \s-1TET_EXEC_IN_PLACE\s0
.br
Setting this variable to 
.C False
indicates that files will be executed
in a temporary execution directory.
Use of a temporary execution directory for each test enables 
parallel execution of the test suite against multiple servers.
.P
Setting this variable to 
.C True
will give you improved performance if you are not attempting 
parallel execution of the test suite against multiple servers.
.cS
Eg: TET_EXEC_IN_PLACE=False
.cE
.LI \s-1TET_SAVE_FILES\s0
.br
This indicates which files generated during execution of
tests are to be saved for later examination. 
This line should not be altered.
.cS
Eg. TET_SAVE_FILES=Err*.err,*.sav
.cE
.LE
.H 3 "Configuration Parameters for the \*(xT"
The following parameters are used in many places in the \*(xT. 
These should be set to match the 
X server to be tested and the underlying operating system on which Xlib 
is implemented.
.VL 5 0
.LI \s-1XT_DISPLAY\s0
.br
This should be set to a display string that can be passed to XOpenDisplay,
to access the display under test.
It must include a screen;
all testing is done for a particular screen.
.cS
Eg: XT_DISPLAY=:0.0
.cE
.LI \s-1XT_ALT_SCREEN\s0
.br
If the display supports more than one screen,
this parameter should be set to the number of a screen 
that is different from that incorporated in the XT_DISPLAY variable.
.br
Set to the string 
.SM UNSUPPORTED 
if only one screen is available.
.br
Note that this should be a screen number, not a display string that can
be passed to XOpenDisplay.
.cS
Eg: XT_ALT_SCREEN=1
.cE
.LI \s-1XT_FONTPATH\s0
.br
This should be set to a comma separated list that 
is a valid font path for the X server. It should include at least 
the components of the default font path for the X server, enabling the cursor 
font to be accessed.
One of the components must be the directory 
in which the test fonts were installed (see the section entitled "Compiling
and installing the test fonts").
.P
This parameter will be used to set the font path for specific test purposes
which access the test fonts. The font path is restored on completion
of the specific test purposes.
.cS
Eg: \s-1XT_FONTPATH=/usr/lib/X11/fonts/xtest/,/usr/lib/X11/fonts/misc/\s0
.cE
.LI \s-1XT_SPEEDFACTOR\s0
.br
This is a speedfactor which should be set to reflect the relative delay 
in response of the underlying operating system and X server combined.
Co-operating processes which 
must synchronize allow a time delay in proportion to this speedfactor, to 
account for scheduling delays in the underlying operating system and X server.
This should be set to a number greater than or equal to one.
There should be no need to change the default unless the round trip time to
the X server can be very long ( >15 seconds);
in this case set this parameter to a
value larger than the maximum round trip time divided by 3.
.cS
Eg: XT_SPEEDFACTOR=5
.cE
.LI \s-1XT_RESET_DELAY\s0
.br
Specifies a delay time in seconds.
Set this to be a time which is greater than or equal to the maximum time
required by your server to reset when the last client is closed.
The test suite pauses for this time whenever a connection is about to be 
opened and the server may be resetting.
The server may be resetting when the test case is entered (in startup())
as a result of closing the last connection in the previous test case.
The server also resets in a few places in the test for XCloseDisplay().
.cS
Eg. XT_RESET_DELAY=1
.cE
.LI \s-1XT_EXTENSIONS\s0
.br
Specifies whether you wish to test the extended assertions
which require the XTEST extension.
Set this to Yes if the XTEST extension is available on your system,
and you have configured the test suite to use the XTEST extension,
and you want to execute these tests, otherwise set to No.
.br
.cS
Eg. XT_EXTENSIONS=No
.cE
.LE
.H 3 "Configuration parameters for specific tests"
The following parameters are used to control 
one or more specific test purposes in the \*(xT. 
These should be set to appropriate values for the X server to be tested.
.P
These parameters may cause temporary changes in the settings 
of the X server under test (such as the font path).
Settings are restored on completion of the specific test purposes.
.VL 5 0
.LI \s-1XT_VISUAL_CLASSES\s0
.br
A space separated list of the visual classes that
are supported for the screen given by XT_DISPLAY.  Each visual class
is followed by a list of depths at which the class is supported
(enclosed by brackets and separated by commas with no spaces).
Visual classes and depths that are supported only by other screens should
not be included.
.br
Note that this parameter is only used to check the correctness of the values
returned by XMatchVisualInfo and XGetVisualInfo. Other tests which loop
over visuals obtain the values by calling these functions.
.cS
Eg. XT_VISUAL_CLASSES=StaticGray(8) GrayScale(8) StaticColor(8) 
                      PseudoColor(8) TrueColor(8) DirectColor(8)
\fR(This must be typed as one line.)\fP
.cE
.LI \s-1XT_FONTCURSOR_GOOD\s0
.br
This specifies the number of a glyph in the 
default cursor font known to exist. 
.SM XT_FONTCURSOR_GOOD+2
should also be a glyph in the
default cursor font.
Neither of these should be the same as the X server's default cursor.
.cS
Eg: XT_FONTCURSOR_GOOD=2
.cE
.LI \s-1XT_FONTCURSOR_BAD\s0
.br
This specifies the number of a glyph in the 
default cursor font known not to exist. 
If no such value exists, set to
.SM UNSUPPORTED .
.cS
Eg: XT_FONTCURSOR_BAD=9999
.cE
.LI \s-1XT_FONTPATH_GOOD\s0
.br
This should be set to a comma separated list that
is a valid font path for the X server.
It should
be different from XT_FONTPATH. It need not contain the 
test fonts.
.cS
Eg: \s-1XT_FONTPATH_GOOD=/usr/lib/X11/fonts/100dpi/,/usr/lib/X11/fonts/75dpi/\s0
.cE
.LI \s-1XT_FONTPATH_BAD\s0
.br
This should be set to a comma separated list that 
is an invalid font path for the X server.
If you cannot determine a suitable value, set to 
.SM UNSUPPORTED .
There is no default value - by default, tests which use this parameter
will be reported as 
.SM UNSUPPORTED .
.cS
Eg: XT_FONTPATH_BAD=/jfkdsjfksl
.cE
.LI \s-1XT_BAD_FONT_NAME\s0
.br
This should be set to a non-existent font name.
.cS
XT_BAD_FONT_NAME=non-existent-font-name
.cE
.LI \s-1XT_GOOD_COLORNAME\s0
.br
This should be set to the name of a colour 
which exists in the colour database for the X server.
.cS
Eg: XT_GOOD_COLORNAME=red
.cE
.LI \s-1XT_BAD_COLORNAME\s0
.br
This should be set to the name of a colour 
which does not exist in the colour database for the X server.
.cS
Eg: XT_BAD_COLORNAME=nosuchcolour
.cE
.LI \s-1XT_DISPLAYMOTIONBUFFERSIZE\s0
.br
This should be set to a non-zero value (the value
returned by XDisplayMotionBufferSize) if the X server supports a more complete 
history of pointer motion than that provided by event notification, or
zero otherwise. The
more complete history is made available via the Xlib functions 
XDisplayMotionBufferSize and XGetMotionEvents.
.cS
Eg: XT_DISPLAYMOTIONBUFFERSIZE=256
.cE
.LE
.H 3 "Configuration parameters for Display functions"
The following parameters are used to control one or more test purposes for Xlib 
Display functions which are in section 2 of the X11R4 Xlib specifications.
These should be set to match the display specified in the XT_DISPLAY parameter. 
.P
Some of these parameters are specific to the particular screen of 
the display under test. This is also specified in the XT_DISPLAY parameter.
.P
Settings to these parameters will not cause any change in the settings 
of the X server under test.
.P
Suitable values for most of these parameters can be obtained from the 
output of the X11 utility 
.C xdpyinfo .
.VL 5 0
.LI \s-1XT_SCREEN_COUNT\s0
.br
This parameter should be set to the number of screens
available on the display as returned by XScreenCount.
.br
.cS
Eg: XT_SCREEN_COUNT=2
.cE
.LI \s-1XT_PIXMAP_DEPTHS\s0
.br
A space separated list of depths supported by the specified 
screen of the display that can be used for pixmaps.
.cS
Eg: XT_PIXMAP_DEPTHS=1 8
.cE
.LI \s-1XT_BLACK_PIXEL\s0
.br
This parameter should be set to the black pixel value 
of the specified screen of the display.
.cS
Eg: XT_BLACK_PIXEL=1
.cE
.LI \s-1XT_WHITE_PIXEL\s0
.br
This parameter should be set to the white pixel value 
of the specified screen of the display.
.cS
Eg: XT_WHITE_PIXEL=0
.cE
.LI \s-1XT_HEIGHT_MM\s0
.br
This parameter should be set to the height in millimeters
of the specified screen of the display.
.cS
Eg: XT_HEIGHT_MM=254
.cE
.LI \s-1XT_WIDTH_MM\s0
.br
This parameter should be set to the width in millimeters
of the specified screen of the display.
.cS
Eg: XT_WIDTH_MM=325
.cE
.LI \s-1XT_PROTOCOL_VERSION\s0
.br
This should be set to the major version number (11) 
of the X protocol as returned by XProtocolVersion.
.cS
Eg. XT_PROTOCOL_VERSION=11
.cE
.LI \s-1XT_PROTOCOL_REVISION\s0
.br
This should be set to the minor protocol 
revision number as returned by XProtocolRevision.
.cS
Eg. XT_PROTOCOL_REVISION=0
.cE
.LI \s-1XT_SERVER_VENDOR\s0
.br
This should be set to the X server vendor string
as returned by XServerVendor.
.cS
Eg: XT_SERVER_VENDOR=MIT X Consortium
.cE
.LI \s-1XT_VENDOR_RELEASE\s0
.br
This should be set to the X server vendor's release 
number as returned by XVendorRelease.
.cS
Eg. XT_VENDOR_RELEASE=5000
.cE
.LI \s-1XT_DOES_SAVE_UNDERS\s0
.br
Set this to Yes if the specified screen of the display
supports save unders (indicated by XDoesSaveUnders returning True)
otherwise set to No.
.cS
Eg. XT_DOES_SAVE_UNDERS=Yes
.cE
.LI \s-1XT_DOES_BACKING_STORE\s0
.br
Set this to the following value:
.br
0 - the specified screen supports backing store NotUseful
.br
1 - the specified screen supports backing store WhenMapped
.br
2 - the specified screen supports backing store Always
.br
The way the specified screen supports backing store is indicated by the
return value of XDoesBackingStore.
.cS
Eg. XT_DOES_BACKING_STORE=2
.cE
.LE
.H 3 "Configuration parameters for connection tests"
The following parameters are used to control 
one or more test purposes for XOpenDisplay, XCloseDisplay and 
XConnectionNumber.
These should be set to match the display specified in the XT_DISPLAY parameter
and the characteristics of the underlying operating system.
.P
Settings to these parameters will not cause any change in the settings 
of the X server under test.
.P
These parameters are not used when making connections to the 
X server in other tests.
.VL 5 0
.LI \s-1XT_POSIX_SYSTEM\s0
.br
This may be set to Yes to indicate that the 
underlying operating system is a POSIX system. If this parameter is
set to Yes, some extended assertions which describe implementation 
dependent functionality will be tested assuming POSIX concepts.
.cS
Eg. XT_POSIX_SYSTEM=Yes
.cE
.LI \s-1XT_DECNET\s0
.br
Set this to Yes if clients can connect to the X server under 
test using DECnet.
This will be used (on a POSIX system)
in the tests for XOpenDisplay.
.cS
Eg. XT_DECNET=No
.cE
.LI \s-1XT_TCP\s0
.br
Set this to Yes if clients can connect to the X server under 
test using TCP streams.
This will be used (on a POSIX system)
in the tests for XOpenDisplay.
.cS
Eg. XT_TCP=Yes
.cE
.LI \s-1XT_DISPLAYHOST\s0
.br
Set this to the hostname of the machine on which the display
is physically attached. This will be used instead of XT_DISPLAY
(on a POSIX system) 
in the tests for XOpenDisplay which specifically test the hostname 
component of the display name.
.P
Note that this may not be the same as the machine on which the 
test suite clients execute (XTESTHOST).
.cS
Eg. XT_DISPLAYHOST=xdisplay.lcs.mit.edu
.cE
.LI \s-1XT_LOCAL\s0
.br
Set this to Yes if clients can connect to a local X server 
without passing a hostname to XOpenDisplay.
This will be used (on a POSIX system)
in the tests for XOpenDisplay.
This is usually the case when the X server under test is running on the
same platform as the \*(xT.
When a hostname is omitted, the Xlib implementation of XOpenDisplay 
can use the fastest available transport mechanism to make local connections.
.cS
Eg. XT_LOCAL=No
.cE
.LE
.P
.H 3 "Configuration Parameters which do not affect test results"
There are a number of execution configuration parameters which can be used
to reduce the size of the journal file, or dump out more information from the 
test suite.
They will not alter the behaviour of the tests or the test results.
.P
.VL 5 0
.LI \s-1XT_SAVE_SERVER_IMAGE\s0
.br
When set to Yes,
the image produced by the server 
that is compared with the known good image is dumped to a file 
with suffix ".sav" .
.cS
Eg: XT_SAVE_SERVER_IMAGE=Yes
.cE
.LI \s-1XT_OPTION_NO_CHECK\s0
.br
This may be set to Yes to suppress the journal file
records containing 
.SM CHECK 
keywords. Refer to appendix D for 
information on the contents of these messages.
.cS
Eg: XT_OPTION_NO_CHECK=Yes
.cE
.LI \s-1XT_OPTION_NO_TRACE\s0
.br
This may be set to Yes to suppress the journal file
records containing 
.SM TRACE 
keywords. Refer to appendix D for 
information on the contents of these messages.
.cS
Eg: XT_OPTION_NO_TRACE=Yes
.cE
.LE
.P
.H 3 "Configuration Parameters for debugging tests"
There are a number of execution configuration parameters which should
not be set when performing verification test runs. These are intended
for debugging purposes. These parameters may affect the behaviour of some test 
purposes if they are set to assist debugging.
.P
.VL 5 0
.LI \s-1XT_DEBUG\s0
.br
This may be set to a debugging level.
A higher level produces more debugging output. Output is only 
produced by the test suite at levels 1, 2 and 3. Setting
this variable to 0 produces no debug output, and 3 gives everything
possible (setting this variable to 3 can give an enormous volume of output
so you should not do this when running large numbers of test sets).
.cS
Eg: XT_DEBUG=0
.cE
.LI \s-1XT_DEBUG_OVERRIDE_REDIRECT\s0
.br
When set to 
.C Yes , 
windows are created with
override_redirect set. 
This enables tests to be run more easily with a
window manager running on the same screen.
This should not be set to 
.C Yes 
for verification tests.  
.cS
Eg: XT_DEBUG_OVERRIDE_REDIRECT=No
.cE
.LI \s-1XT_DEBUG_PAUSE_AFTER\s0
.br
When set to 
.C Yes , 
the test pauses after each call to the Xlib
function being tested, until Carriage Return is entered.
This is useful to enable the results of
graphics operations to be observed.
This should not be set to 
.C Yes 
for verification tests.
.cS
Eg: XT_DEBUG_PAUSE_AFTER=No
.cE
.LI \s-1XT_DEBUG_PIXMAP_ONLY\s0
.br
When set to 
.C Yes , 
tests which would normally loop over
both windows and pixmaps are restricted to loop over just pixmaps.
This is useful for speeding up the 
execution of the test set.
This should not be set to 
.C Yes 
for verification tests.
.P
If 
.SM XT_DEBUG_WINDOW_ONLY 
is also set to 
.C Yes ,
some tests will report 
.SM UNRESOLVED 
due to the fact that nothing has been tested.
.cS
Eg: XT_DEBUG_PIXMAP_ONLY=No
.cE
.LI \s-1XT_DEBUG_WINDOW_ONLY\s0
.br
When set to 
.C Yes , 
tests which would normally loop over
both windows and pixmaps are restricted to loop over just windows.
This is useful for speeding up the 
execution of the test set.
This should not be set to 
.C Yes 
for verification tests.
.P
If 
.SM XT_DEBUG_PIXMAP_ONLY 
is also set to 
.C Yes ,
some tests will report 
.SM UNRESOLVED 
due to the fact that nothing has been tested.
.cS
Eg: XT_DEBUG_WINDOW_ONLY=No
.cE
.LI \s-1XT_DEBUG_DEFAULT_DEPTHS\s0
.br
When set to 
.C Yes , 
tests which would normally loop over multiple
depths are restricted to test just the first visual returned by
XGetVisualInfo and/or the first pixmap depth returned by XListDepths
(depending on whether 
.SM XT_DEBUG_PIXMAP_ONLY
or 
.SM XT_DEBUG_WINDOW_ONLY
is also set).
This is useful for speeding up the 
execution of the test set.
This should not be set to 
.C Yes 
for verification tests.
.P
Note that the first visual returned by XGetVisualInfo may not be the default 
visual for the screen.
.cS
Eg: XT_DEBUG_DEFAULT_DEPTHS=No
.cE
.LI \s-1XT_DEBUG_VISUAL_IDS\s0
.br
When set to a non-empty string, tests which would 
normally loop over multiple depths are restricted to test just the 
visuals ID's listed. Note that visual ID's for visuals on more than 
one screen may be entered, but those used will depend on whether the test 
being executed uses visuals on the default screen or alternate screen.
The visuals ID's should be entered in decimal, octal or hexadecimal
and separated with commas and with no intervening spaces.
This should not be set to a non-empty string for verification tests.
.cS
Eg. XT_DEBUG_VISUAL_IDS=0x22,0x24,0x27
.cE
.LI \s-1XT_DEBUG_NO_PIXCHECK\s0
.br
When set to 
.C Yes , 
tests
which would normally perform pixmap verification omit this (all other
processing is performed in those tests as normal).
Pixmap verification is a scheme which compares the image produced by the 
X server with a known good image file which is part of the \*(xT (this is 
described further in the section entitled "Examining Image Files").
This should not be set to 
.C Yes 
for verification tests.
.cS
Eg: XT_DEBUG_NO_PIXCHECK=No
.cE
.LI \s-1XT_DEBUG_BYTE_SEX\s0
.br
When set to 
.SM NATIVE ,
.SM REVERSE ,
.SM MSB 
or 
.SM LSB , 
the X Protocol tests will only be executed with the specified byte sex. 
When set to
.SM BOTH ,
the X Protocol tests make connections to the X server using
both the native and reversed byte sex.
.P
Note: The parameter should always be set to 
.SM NATIVE
when the build configuration parameter
.SM XP_OPEN_DIS
was set to XlibOpaque.c
.cS
Eg: XT_DEBUG_BYTE_SEX=NATIVE
.cE
.LI \s-1XT_DEBUG_VISUAL_CHECK\s0
.br
When set to a non-zero value, the X Protocol tests
will pause for the specified time interval (in seconds), to enable a visual
check to be performed on the displayed screen contents.
.cS
Eg: XT_DEBUG_VISUAL_CHECK=5
.cE
.H 3 "Configuration Parameters used only during test development"
This section defines a number of parameters that are used only when
generating known good image files. These are not intended to be modified 
and need not be used when running the test suite.
They are only used in the development environment
at UniSoft when generating known good image files.
.P
.VL 5 0
.LI \s-1XT_FONTDIR\s0
.br
The directory in which the xtest fonts are located 
(before being installed).
This must be set such that appending a string gives a valid file name.
This is normally set to \fC$TET_ROOT/xtest/fonts/\fP.
.cS
Eg: XT_FONTDIR=/usr/mit/testsuite/xtest/fonts/
.cE
.LE
.H 2 "Executing tests using the \s-1TET\s0"
The \*(xT is executed by invoking the execute mode of the Test Case
Controller.
.cS
cd $TET_ROOT/xtest
tcc -e [ -s scenario_file ] [ -j journal_file ] [ -x config_file ] 
                                                [ -y string ] xtest all
.cE
.VL 5 0
.LI \fC-e\fP
.br
This invokes the \s-1TCC\s0 in execute mode.
.LI "\fC-s scenario_file\fP"
.br
This option executes the test sets in the named scenario file.
The default is a file named \fCtet_scen\fP in the directory 
\fC$TET_ROOT/xtest\fP.
For more details refer to the section entitled 
"Executing modified scenarios using the \s-1TET\s0".
.LI "\fC-j journal_file\fP"
.br
This option sends the test results to the named journal file.
The default is a file named \fCjournal\fP in a newly created sub-directory 
of \fC$TET_ROOT/xtest/results\fP. Sub-directories are created with sequential
four digit numbers, with the 
.SM TCC
flags (in this case "e") appended.
The \s-1TCC\s0 will exit if the specified journal file already exists, thus the
journal file should be renamed or removed
before attempting to execute the \s-1TCC\s0.
.LI "\fC-x config_file\fP"
.br
This is an option to run the test suite using the information in a 
modified execution configuration file named 
.C config_file .
The default is 
.C tetexec.cfg .
.LI "\fC-y string\fP"
.br
This option only executes tests which include the specified string in the 
scenario file line. This may be used to execute specific sections or individual
test sets.
.LI \fCxtest\fP
.br
This is the name of the test suite.
It determines the directory under $\s-1TET_ROOT\s0
where the test suite is to be found.
.LI \fCall\fP
.br
This is the scenario name in the default scenario file
\fC$TET_ROOT/xtest/tet_scen\fP.
For more details refer to the section entitled 
"Executing modified scenarios using the \s-1TET\s0".
.LE
.P
A journal file 
will be produced.
More information on the contents of the journal file is given in 
appendix C.
.P
Note: If the \s-1TCC\s0
terminates due to receipt of a signal which 
cannot be caught, the \s-1TCC\s0
may leave lock files in the test source directories. Subsequent 
attempts to restart the \s-1TCC\s0 may give error messages 
saying that a lock file was encountered. At this point 
\s-1TCC\s0
may suspend the build. It may be 
necessary to find and remove files or directories named \fCtet_lock\fP 
before continuing.
.H 2 "Building, executing and cleaning tests using the \s-1TET\s0"
Each test in the \*(xT may be built, executed and cleaned before 
the next test set in the scenario. This mode of use has the advantage that 
the entire \*(xT may be executed without necessarily building all the 
test sets in advance, thus ensuring disc space is conserved throughout.
This mode of use has the disadvantage that you will
need to rebuild a test set before rerunning, which will take 
considerably longer than if it is built in advance.
.P
The \*(xT is built, executed and cleaned 
by simultaneously invoking the build, execute and clean modes of the Test Case
Controller.
.cS
cd $TET_ROOT/xtest
tcc -bec [ -s scenario_file ] [ -j journal_file ] [ -x config_file ]
                                                  [ -y string ] xtest all
.cE
.VL 5 0
.LI \fC-b\fP
This invokes the \s-1TCC\s0 in build mode.
.LI \fC-e\fP
This invokes the \s-1TCC\s0 in execute mode.
.LI \fC-c\fP
This invokes the \s-1TCC\s0 in clean mode.
.LE
.P
The other options are as described in the earlier section entitled
"Executing tests using the \s-1TET\s0".
.P
A journal file 
will be produced. This contains for each test set in order the results of 
the build, followed by the test results, followed by the results of the clean.
More information on the contents of the journal file is given in 
appendix C.
.P
The default journal file is named \fCjournal\fP in a newly created sub-directory 
of \fC$TET_ROOT/xtest/results\fP. Sub-directories are created with sequential
four digit numbers, with the 
.SM TCC
flags (in this case "bec") appended.
.H 2 "Executing modified scenarios using the \s-1TET\s0"
.H 3 "Format of the scenario file"
The \s-1TET\s0 uses a scenario file to determine which test sets to execute.
The file 
\fC$TET_ROOT/xtest/tet_scen\fP
is the default scenario file.
The format is basically a scenario name starting in column one,
followed by list of test sets to be executed (each starting beyond column one).
Only one scenario named "all" is provided in the default scenario file.
.P
The names of the test sets are given relative to the directory 
$TET_ROOT/xtest, and must commence with a leading slash.
.H 3 "Modifying the scenario file"
The file 
\fC$TET_ROOT/xtest/tet_scen\fP
may be modified by removing lines corresponding to test sets which are not 
wanted. These will then simply not be executed by the \s-1TCC\s0.
Alternatively, unwanted
lines may be commented out by placing \fC#\fP at the start of the line.
.P
If you wish to execute just a subset of the test purposes in a test set, 
refer to the section below entitled "Executing individual test purposes
using the \s-1TET\s0".
.P
It is recommended that the supplied scenario file should be saved if it is
modified.
.H 3 "Creating new scenario files"
A new scenario file may be created in the directory
\fC$TET_ROOT/xtest\fP.
The \s-1TCC\s0 will use this scenario file instead of the file 
\fC$TET_ROOT/xtest/tet_scen\fP
if it is passed via the \fC-s\fP option. For example
.cS
cd $TET_ROOT/xtest
tcc -e -s scenario_file [ -j journal_file ] [ -x config_file ]
                                            [ -y string ] xtest all
.cE
.H 2 "Executing individual test purposes using the \s-1TET\s0"
Each assertion in the \*(xT has separate test code which is known as a
test purpose.
We have arranged that each test purpose is also
a separately invocable component,
and that the invocable component number is identical 
to the test purpose number.
.P
The expression within the braces 
at the end of a line within a scenario file is an 
invocable component list (or IC_list). 
The default invocable component list \fCall\fP causes the 
\s-1TCC\s0 to execute all invocable components in a test set.
.P
By altering the invocable component list for a test set,
particular invocable components of interest can be 
executed.
.P
The invocable component list consists of one or more elements separated 
by commas. Each element is either an invocable component number,
or a range of invocable component numbers separated by a dash.
.P
This is useful for quickly executing a particular test purpose of interest
for example:
.cS
/tset/CH05/stclpmsk/Test{3}
.cE
This is also useful for executing all test purposes except one known 
to cause a system error. This may be useful if a particular test purpose
causes your X server to exit (at present the \s-1TET\s0 provides no high level
control facilities to conditionally cancel later test sets).
For example:
.cS
/tset/CH05/stclpmsk/Test{1-2,4-6}
.cE
.P
Note that the placement of windows used by the test suite may differ
when an earlier test purpose is not executed. It is intended that test 
purposes produce the same results regardless of window placement.
.H 2 "Executing tests without using the \s-1TET\s0"
See section 11, entitled
"Building, executing and reporting tests without using the \s-1TET\s0".
.H 2 "Executing tests in space-saving format using the \s-1TET\s0"
Before reading this section, read the section entitled "Building 
tests in space-saving format".
When you have built all the sections of the test suite in space-saving 
format, you can execute all the tests in the test suite using
the instructions in this section.
.P
A scenario named \fClinkexec\fP is provided in a scenario file 
named \fClink_scen\fP in the directory 
$TET_ROOT/xtest. 
This enables the \s-1TCC\s0 to execute the space-saving
executable files which have been built.
.P
Execute the command:
.cS
cd $TET_ROOT/xtest
tcc -e -s link_scen [ -j journal_file ] [ -x config_file ]
                                        [ -y string ] xtest linkexec
.cE
.SK
.H 1 "Report writer"
A basic report writer 
\fCrpt\fP
is included with the \*(xT.
It extracts and formats the main information from a 
TET journal file produced by executing the \s-1TCC\s0 in execute 
mode, or build-execute-clean mode.
It does not format the \s-1TET\s0 journal file produced by the 
\s-1TCC\s0 in build only or clean only mode. 
The main features of the \s-1TET\s0 journal file produced by the \s-1TCC\s0 in
execute or build-execute-clean mode are described in appendix\ C.
.P
Execute the report writer as follows:
.P
.cS
rpt [ -t ] [ -d ] [ -p ] [ -s ] [ -f file ]
.cE
With only the -f argument, \fCrpt\fP lists the results of each test purpose
for all test sets that appear in the journal file \fCfile\fP. 
The default is the file named
.C journal
in the highest numbered subdirectory of the
.C $TET_ROOT/xtest/results
directory that has an 'e' suffix.
.P
The reason for any test result code which is other than PASS is printed out.
This is done by copying the test information messages of type REPORT. 
For further details, see appendix D.
.P
A warning message is printed if a test information message of type REPORT
is given in a test purpose which produced a test result code PASS.
.P
The results for each test set are followed
by a summary of the number of test purposes in the test set
which produced each result code type.
.P
There is no overall summary list of results for all test sets in the journal 
file.
.P
.VL 5 0
.LI -t
.br
Test information messages of type TRACE in the test purposes
specified are printed.
For further details, see appendix D.
.LI -d
.br
Test information messages of type TRACE or DEBUG in the test purposes
specified are printed.
For further details, see appendix D.
.LI -p
.br
Output is restricted to omit reporting on test purposes that
resolve to \s-1PASS\s0, \s-1UNSUPPORTED\s0, \s-1UNTESTED\s0
or \s-1NOTINUSE\s0 \*(EM thereby
reporting only tests showing possible errors.
.LI -s
.br
The result summaries after the end of each test set are omitted.
.LE
.SK
.H 1 "Examining image files"
.H 2 "Generating pixmap error files"
During the test run, discrepancies may be encountered between the image
displayed by the server and the known good image. The known good image 
may have been obtained from a known good image file supplied with the 
release, or it may have been determined analytically. 
.P
Should a discrepancy be encountered, the test purpose 
will give a result code of FAIL. The failure reason message will 
name a pixmap error file in which is contained both the known good image and 
the server image. 
.P
A debug option has been provided, which skips any verification of the 
image produced by the server with known good image files. This 
is done by setting the execution configuration parameter 
.SM XT_DEBUG_NO_PIXCHECK 
to \fCYes\fP.
.H 2 "Pixmap error file naming scheme"
Each invocation of the 
.SM TCC
creates a sub-directory 
in \fC$TET_ROOT/xtest/results\fP. Sub-directories are created with sequential
four digit numbers, with the 
.SM TCC
flags ("e" or "bec") appended. The default
.SM TET
journal is a file named
\fCjournal\fP created in this directory.
.P
Pixmap error files are stored in a directory tree created within the 
newly created results sub-directory. So, for example, when the 
line 
.cS
/tset/CH06/drwln
.cE
is executed in a scenario file, pixmap error files might be produced
in a directory named
\fC$TET_ROOT/xtest/results/0001bec/tset/CH06/drwln\fP.
.P
The creation of a new results directory tree for each execution of the 
.SM TCC
enables results to be obtained in parallel against multiple 
X servers.
.P
Pixmap error files are named Err\fCnnnn\fP.err, 
where \fCnnnn\fP
is a four digit number. This number does not
correspond to the number of the test purpose which caused the error.
.P 
Note - when tests are executed without using the 
.SM TCC
the error files are produced in the current directory.
.H 2 "Known good image file naming scheme"
All the required known good image files for the test programs in the 
\*(xT (as supplied) have been created in advance.
The known good image files for each test program 
are supplied in the \*(xT in the test set directory in which the dot-m 
file is supplied. They are named a\fCnnn\fP.dat, where \fCnnn\fP is the 
number of the test purpose for which the known good image file was generated.
.P
More details of the contents of this release are in appendix A.
.H 2 "Using blowup to view image files"
The contents of the two images in a pixmap error file
may be compared by using the blowup utility.
.P
Also, a known good image file may be viewed directly. 
.P
The file formats of the error file and the known good image file are 
the same. The blowup utility detects which file type is being viewed 
by means of file name. For this reason, do not rename the 
pixmap error or known good image files.
.H 3 "Blowup command"
The blowup utility may be used to view one or more pixmap error files or 
known good image files as follows:
.P
.cS
blowup [-z zoom_factor] [-f font] [-d display] [-colour] file(s)
.cE
.VL 5 0
.LI "\fC-z zoom_factor\fP"
.br
This option sets the magnification factor in all the blowup windows.
.LI "\fC-f font\fP"
.br
This option ensures that 
.C font
is used rather than the default font. The default font is 6x10.
.LI "\fC-d display\fP"
.br
This option uses the display named 
.C display
for the display windows.
.LI "\fC-colour\fP"
.br
On a colour display, this option will display different pixel
values in different colours corresponding to that server's colour table. 
No attempt is made to preserve colours between different servers.
.LE
.H 3 "Blowup windows"
Two windows are created. The first is called Comparison, 
and the second is called Blowup.
The Blowup window 
shows a magnified version of a portion of the Comparison window, 
which is indicated in the Comparison window by a rectangle. A user 
interface menu is shown in the Blowup window.
.P
The title of the Comparison window will change to 
"Server Data", then to "Pixval Data" and then back to "Comparison" 
when the "B/G/Both" option on the menu is used.
.H 3 "Selection of a viewing region"
This may be done in one of three ways:
.AL
.LI
Click in the Comparison window.
.LI
Click in a square in the Blowup window. This becomes the new centre square
in that window.
.LI
Choose the "next error" option on the menu. The next pixel at which there
is a discrepancy will become the new centre square in the Blowup 
window.
.LE
.H 3 "Information displayed"
The value stored in the centre pixel
and its coordinates are shown as the top items 
in the menu. Under some circumstances, the expected pixel value will
be shown to the right of the actual value.
.H 3 "Display of errors"
When the "B/G/Both" option is set to Both, and the title of the Comparison 
window is Comparison,
errors are displayed in two ways: one for each window.
.VL 5 0
.LI
In the Comparison window pixels set to non-zero in the "good image"
but set incorrectly in the "server data" are shown as a cross (X).
.LI
In the blowup window these are shown as a white square with
a cross (X) through it.
.LI
In the Comparison window pixels set to zero in the "good image"
but set incorrectly in the "server data" are shown as shaded squares.
.LI
In the blowup window these are shown as a black square with
a white cross through it.
.LE
.P
The reason that we have proposed the two different methods of displaying errors
is as follows. One normally has a higher magnification in the Blowup
window and the use of a cross (X) through incorrect pixels
is good, and simple to remember
at this level of zoom. In the Comparison window 
this style of display
does not work well at the lower magnification levels; all the crosses merge to
a blur so it is hard to see what type of error is being displayed.
.H 3 "Commands (via menu in the Blowup window)"
All of the commands are invoked by clicking the left mouse button when the
corresponding menu item is highlighted (inverted). The available commands are,
from top to bottom:
.VL 5 0
.LI B/G/Both
.br
Show Bad (Server Data), Good (Pixval Data) or Both
(Comparison). Clicking in this advances around the
cycle
.ft C
.DS
Bad ----> Good -> Both
 \------<------<----/
.DE
.ft
The Comparison window's name changes to reflect the
current state.
.P
If a known good image file is being displayed then only the Good
option is available. A pixmap error file is required for this
command to be useful.
.LI color/mono
.br
Use colour/monochrome in the Blowup window.
.LI "next error"	
.br
Advance centre pixel point to be next pixel at which there is a 
discrepancy.
.LI "sub-zoom +"
.br
Zoom in (make bigger by zoomfactor) on the Blowup window
.LI "sub-zoom -"
.br
Zoom out (make smaller by zoomfactor) on the Blowup window
.LI "quit"
.br
Quit from the blowup utility.
.LI "big-zoom +"
.br
Zoom in (make bigger by zoomfactor) on the Comparison window
.LI "big-zoom -"
.br
Zoom out (make smaller by zoomfactor) on the Comparison window
.LI "next"
.br
View next file in the list. The Blowup window will
be removed and a new one created for each file. The
size, and zoom factor, of the Comparison window will
be preserved across files.
.LE
.SK
.H 1 "Cleaning the tests"
.H 2 "Cleaning tests using the \s-1TET\s0"
The entire \*(xT can be cleaned by using the 
clean mode of the \s-1TCC\s0. In this mode, the clean configuration
parameters in the file 
.C $TET_ROOT/xtest/tetclean.cfg 
are used to clean each test 
set in the \*(xT separately. Previously built test set executables 
and object files are removed.
.P
.cS
cd $TET_ROOT/xtest
tcc -c [ -s scenario_file ] [ -j journal_file ] [ -y string ] xtest all
.cE
.VL 5 0
.LI \fC-c\fP
.br
This invokes the \s-1TCC\s0 in clean mode.
.LI "\fC-s scenario_file\fP"
.br
This option cleans the test sets in the named scenario file.
The default is a file named \fCtet_scen\fP in the directory 
\fC$TET_ROOT/xtest\fP.
For more details refer to the section entitled 
"Cleaning modified scenarios using the \s-1TET\s0".
.LI "\fC-j journal_file\fP"
.br
This option sends the output of the clean to the named journal file.
The default is a file named \fCjournal\fP in a newly created sub-directory 
of \fC$TET_ROOT/xtest/results\fP. Sub-directories are created with sequential
four digit numbers, with the 
.SM TCC
flags (in this case "c") appended.
The \s-1TCC\s0 will exit if the specified journal file already exists, thus the
journal file should be renamed or removed
before attempting to execute the \s-1TCC\s0.
.LI "\fC-y string\fP"
.br
This option only cleans tests which include the specified string in the 
scenario file line. This may be used to clean specific sections or individual
test sets.
.LI \fCxtest\fP
.br
This is the name of the test suite.
It determines the directory under $\s-1TET_ROOT\s0
where the test suite is to be found.
.LI \fCall\fP
.br
This is the scenario name in the default scenario file
\fC$TET_ROOT/xtest/tet_scen\fP.
For more details refer to the section entitled 
"Cleaning modified scenarios using the \s-1TET\s0".
.LE
.P
This will execute the \s-1TET\s0 clean tool in the \s-1TET\s0 configuration 
variable
.SM TET_CLEAN_TOOL
(which is normally pclean),
in each test set directory of the \*(xT.
.P
The journal file should be examined to verify
that the clean process succeeded.
The report writer \fCrpt\fP cannot interpret the contents 
of a journal file produced during the clean process.
.P
Note: If the \s-1TCC\s0
terminates due to receipt of a signal which 
cannot be caught, the \s-1TCC\s0
may leave lock files in the test source directories. Subsequent 
attempts to restart the \s-1TCC\s0 may give error messages 
saying that a lock file was encountered. At this point 
\s-1TCC\s0
may suspend the build. It may be 
necessary to find and remove files or directories named \fCtet_lock\fP 
before continuing.
.H 2 "Cleaning modified scenarios using the \s-1TET\s0"
.H 3 "Format of the scenario file"
Refer to the earlier section "Building modified scenarios using the \s-1TET\s0".
.H 3 "Modifying the scenario file"
Refer to the earlier section "Building modified scenarios using the \s-1TET\s0".
.H 3 "Creating new scenario files"
A new scenario file may be created in the directory
\fC$TET_ROOT/xtest\fP.
The \s-1TCC\s0 will use this scenario file instead of the file 
\fC$TET_ROOT/xtest/tet_scen\fP
if it is passed via the \fC-s\fP option. For example
.cS
cd $TET_ROOT/xtest
tcc -c -s scenario_file [ -j journal_file ] [ -y string ] xtest all
.cE
.H 2 "Cleaning tests without using the \s-1TET\s0"
See section 11, entitled
"Building, executing and reporting tests without using the \s-1TET\s0".
.H 2 "Cleaning tests built in space-saving format"
It is possible to clean the tests in the \*(xT which were
previously built in space-saving format.
.H 3 "Cleaning tests in space-saving format using the \s-1TET\s0"
A scenario named \fClinkbuild\fP is provided in a scenario file 
named \fClink_scen\fP in the directory 
$TET_ROOT/xtest. 
This enables the \s-1TCC\s0 to clean the space-saving
executable files and remove all the required links for each test set in
each section of the \*(xT.
The -y option allows a particular space-saving 
executable and its links for a single section to be removed.
.P
Execute the command:
.cS
cd $TET_ROOT/xtest
tcc -c -s link_scen [ -j journal_file ] [ -y string ] xtest linkbuild
.cE
This command will execute the \s-1TET\s0 clean tool in the \s-1TET\s0 configuration 
variable
.SM TET_CLEAN_TOOL
(which is normally pclean),
in the top level directory of each section of the test suite.
.H 3 "Cleaning tests in space-saving format without using the \s-1TET\s0"
This section describes how to clean the space-saving
executable files for a particular section of the 
\*(xT directly without using the \s-1TET\s0.
.P
This can be simply done by calling pclean in the required directory.
For example, to clean all the space-saving executable files for
section 5 of the \*(xT, execute the command:
.cS
cd $TET_ROOT/xtest/tset/CH05
pclean
.cE
.SK
.H 1 "Building, executing and reporting tests without using the \s-1TET\s0"
.H 2 "Building tests"
An individual test set can be rebuilt
without the need to use the build mode of the \s-1TCC\s0.
This is done by executing \fCpmake\fP directly, rather than as a \s-1TET\s0 
build tool.
.P
This is a useful facility for building a single test set after a 
previous build has failed.
.P
The build configuration parameters used by \fCpmake\fP
are obtained from a file named
$TET_BUILDCONFIG, or, if TET_BUILDCONFIG is not set in your environment,
from the file named $TET_ROOT/xtest/tetbuild.cfg.
.P
The \fCpmake\fP command should be executed in the directory containing the 
source code for the test set which is to be rebuilt. For more details 
of the names of the directories containing the source code 
for the test sets, refer to appendix A.
.P
For example
.cS
cd $TET_ROOT/xtest/tset/CH05/stclpmsk
pmake
.cE
.P
No journal file is created when \fCpmake\fP is executed directly.
.P
The test set can also be rebuilt using the command
.cS
pmake Test
.cE
.P
If there is a macro version of the Xlib function, this may be rebuilt
using the command
.cS
pmake MTest
.cE
.H 2 "Executing tests"
An individual test set can be executed
without the need to use the execute mode of the \s-1TCC\s0.
This is done by executing a shell script \fCpt\fP.
.P
This is a useful facility for executing a single test set 
repeatedly when investigating a particular test result.
.P
The execution configuration parameters used by \fCpt\fP
are obtained from a file named
$TET_CONFIG, or, if TET_CONFIG is not set in your environment,
from the file named $TET_ROOT/xtest/tetexec.cfg.
.P
The \fCpt\fP command is a shell 
script, which attempts to execute the binary file named \fCTest\fP 
in the current directory. If the file \fCTest\fP is not found, 
the \fCpt\fP command attempts to execute the space-saving executable file
built in that directory.
.P
The \fCpt\fP command should be executed in the directory containing the 
test set which has been built. 
Unless you have manually installed the test set elsewhere, this will be
the directory containing the source code for the test set. For more details 
of the names of the directories containing the source code 
for the test sets, refer to appendix A.
.P
For example
.cS
cd $TET_ROOT/xtest/tset/CH05/stclpmsk
pt
.cE
.P
A \s-1TET\s0 results file is created when \fCpt\fP is executed directly.
This is a file named \fCtet_xres\fP located in the directory in which the 
test was executed.
.P
There are a number of options which may be passed to \fCpt\fP which 
alter the manner in which the test set is executed.
.P
Execute \fCpt\fP as follows:
.P
.cS
pt [ -v XT_VARIABLE_NAME ] [ -d display ] [-i IC_list ] [ -p ] [ -w ]
   [ -P ] [ -D ] [ -x debug_level ] [ -g ] [ -m ]
.cE
.P
.VL 5 0
.LI "-v XT_VARIABLE_NAME=Value"
.br
Modifies the value of the execution configuration parameter named
XT_VARIABLE_NAME, assigning it a value of \fCValue\fP.
.LI "-d display"
.br
Sets the display string to be used for the test.
.br
The default value is taken from the environment variable DISPLAY,
or, if this is not set, from the
execution configuration parameter XT_DISPLAY.
.LI "-i IC_list"
.br
The invocable components executed will be those specified 
in IC_list. 
.P
Each assertion in the \*(xT has separate test code, which is known as a
test purpose.
We have arranged that each test purpose is also
a separately invocable component,
and that the invocable component number is identical 
to the test purpose number.
.P
The invocable component list consists of one or more elements separated 
by commas. Each element is either an invocable component number 
or a range of invocable component numbers separated by a dash.
.P
This is useful for quickly executing a particular test purpose of interest
for example:
.cS
pt -i 37
.cE
This is also useful for executing all test purposes except one known 
to cause a system error.
For example:
.cS
pt -i 1-36,38-57
.cE
Note that the placement of windows used by the test suite may differ
when an earlier test purpose is not executed. It is intended that test 
purposes produce the same results regardless of window placement.
.LI -p                              
.br
This option is equivalent to setting the 
execution configuration parameter XT_DEBUG_PIXMAP_ONLY to Yes.
.LI -w                              
.br
This option is equivalent to setting the 
execution configuration parameter XT_DEBUG_WINDOW_ONLY to Yes.
.LI -P                              
.br
This option is equivalent to setting the 
execution configuration parameter XT_DEBUG_PAUSE_AFTER to Yes.
.LI -D                              
.br
This option is equivalent to setting the 
execution configuration parameter XT_DEBUG_DEFAULT_DEPTHS to Yes.
.LI "-x debug_level"
.br
This option is equivalent to setting the 
execution configuration parameter XT_DEBUG to \fCdebug_level\fP.
.LI -g                              
.br
The binary file \fCpvgen\fP will be executed instead of the binary 
file \fCTest\fP. This option should not be used, since binary 
files named \fCpvgen\fP
are only used in the development environment
at UniSoft when generating known good image files.
.LI -m                              
.br
The binary file \fCMTest\fP will be executed instead of the binary 
file \fCTest\fP. Files named \fCMTest\fP contain tests for the 
macro version of an Xlib function.
.LE
.P
Note that \fCpt\fP creates a temporary file 
\fCCONFIG\fP in the current directory
containing the configuration parameters, so write permission is required
to this file (or if no file is there, to the current directory).
.P
Note also that the binary file \fCTest\fP creates a temporary
file \fC.tmpresfd\fP in the current directory
containing the configuration parameters, so write permission is required
to this file.
.H 2 "Reporting tests"
The \s-1TET\s0 results file produced for an individual test set can be formatted
using the basic report writer \fCrpt\fP, which is described in more detail
in the section entitled "Report writer". The argument \fC-f tet_xres\fP
formats the contents of the \fCtet_xres\fP file.
.P
For convenience, a separate report writer
\fCprp\fP
is provided, which is identical to 
\fCrpt\fP,
except that the default file used is \fCtet_xres\fP 
in the current directory.
.P
This is a useful facility for quickly formatting the
results from the execution of a test set, and looking at the summary 
of the result codes for each test purpose executed.
.P
The \fCprp\fP command should be executed in the directory containing 
the \s-1TET\s0 results file named \fCtet_xres\fP.
Unless you have manually installed and executed the 
test set elsewhere, this will be the directory containing the source 
code for the test set. For more details 
of the names of the directories containing the source code 
for the test sets, refer to appendix A.
.P
For example
.cS
cd $TET_ROOT/xtest/tset/CH05/stclpmsk
prp
.cE
.H 2 "Cleaning tests"
An individual test set can be cleaned
without the need to use the clean mode of the \s-1TCC\s0.
This is done by executing \fCpclean\fP directly, rather than as a \s-1TET\s0 
clean tool.
.P
The clean configuration parameters used by \fCpclean\fP
are obtained from a file named
$TET_CLEANCONFIG, or, if TET_CLEANCONFIG is not set in your environment,
from the file named $TET_ROOT/xtest/tetclean.cfg.
.P
The \fCpclean\fP command should be executed in the directory containing the 
test set which was built. For more details 
of the names of the directories containing the source code 
for the test sets, refer to appendix A.
.P
For example
.cS
cd $TET_ROOT/xtest/tset/CH05/stclpmsk
pclean
.cE
.P
No journal file is created when \fCpclean\fP is executed directly.
.SK
.H 1 "Appendix A - Contents of \*(xR"
This section describes the contents of the directories in the \fCTET_ROOT\fP
directory which are supplied in
this release of the \*(xT. The revised \*(xT has been developed from the 
T7 \*(xT.
This section therefore also explains how the arrangement of the 
revised \*(xT compares with the T7 \*(xT.
.H 2 "tet"
.br
This contains the source files and include files needed to build
the Test Environment Toolkit (TET). The contents of the 
subdirectories are as follows:
.H 3 "tet/src"
.br
This contains the source files for
the \s-1TET\s0.
.H 3 "tet/inc"
.br
This contains the include files for
the \s-1TET\s0.
.H 3 "tet/lib"
.br
This contains the libraries and object files when the
TET has been built.
.H 3 "tet/bin"
.br
This should be empty since the 
TET utilities will be copied into 
.C $TET_ROOT/xtest/bin ,
rather than this directory, when using the modified 
Makefiles supplied with the \s-1TET\s0.
.H 3 "tet/doc"
.br
This contains the release notes and man pages for
the \s-1TET\s0.
.H 3 "tet/demo"
.br
This contains a demonstration program for
the \s-1TET\s0.
.H 2 "xtest"
.br
This contains the tests included in the revised X test suite
which are stored as a complete \s-1TET\s0 test suite. This includes all
necessary configuration files and scenario files to enable you to use
the \s-1TET\s0 following the instructions in the documentation.
.H 2 "xtest/bin"
.br
This contains commands you will need to install, configure, build 
and execute the \*(xT.
After installation, this directory 
contains shell script commands. After configuration 
and building the \*(xT, 
this directory also contains executable programs built for your system.
.H 2 "xtest/doc"
.br
This contains the documentation.
It contains this user guide, the programmers guide and the release
notes. These are supplied in \fItroff(1)\fP format requiring the \fCmm\fP 
macro package, and also in PostScript format. It also contains a 
template for error
reports and a description of how to submit them.
.P
It also contains a file 
.C paper.mm ,
which is a copy of the file
.C Xproto_verf/doc/paper.ms 
originally supplied in the T7 \*(xT, converted
to use the \fImm\fP macro package.
This file contains a paper entitled 
"An Approach to Testing X Window System Servers at a Protocol Level".
.P
This is a technical paper, which defines in outline terms the areas 
of the \*(xW server which should be tested at the X Protocol level 
rather than the Xlib level.
.P
The approach recommended in this paper, and adopted in the design of the 
T7 \*(xT, has been maintained in the revised \*(xT. The paper 
explains the choice of test cases and division of tests between 
the \*(xP and \*(xL.
.P
Before the revision of the \*(xT, UniSoft recommended that this paper should be 
left "as is". As a result, some sections of this paper are out of date in 
that they refer to development schedules for a 
previous software development project, which have now been superseded with the 
production of the revised \*(xT.
.H 2 "xtest/fonts"
.br
This contains test fonts which should be installed using the instructions 
in this user guide. It also contains a software library describing the 
fonts which is used by the tests for text drawing.
.H 2 "xtest/include"
.br
This contains include files for the software in the xtest/src and xtest/tset 
directories.
.H 2 "xtest/lib"
.br
This contains libraries and other common software which are used by the 
tests in the xtest/tset directory. The libraries are built using the 
instructions in this user guide.
.H 2 "xtest/results"
.br
This is an empty directory which is used by the \s-1TET\s0 to store journal 
files produced when executing the \*(xT.
.H 2 "xtest/src"
.br
This contains the source for the libraries and utilities. These are 
built using the instructions in this user guide.
.H 2 "xtest/tset"
.br
This contains the source for the tests for sections 2 to 10 of the 
X11R4 Xlib specifications, in directories CH02 to CH10. 
These are
built using the instructions in this user guide.
In the rest of this document, these 
are refered to as the "\*(xL".
.P
Each of the directories CH02 to CH10 contains further subdirectories
which are known as test set directories. 
Each of these contain all the test code for a single Xlib function. 
The name of the directory is derived from the name of the Xlib 
function by a scheme which is described in appendix B.
.P
So, for example for
.C XSetClipMask
we have the following:
.cS
tset/CH05/stclpmsk
tset/CH05/stclpmsk/stclpmsk.m
.cE
.P
The file
.C stclpmsk.m
is the source file, which is also known as a dot-m file.
The format of the dot-m file is described further in the "Programmers Guide".
.P
The \*(xL are designed to accomplish the following:
.P
.AL
.LI
Test the ability of the Xlib function to behave as specified in the X11R4 
Xlib specifications, in situations where no X Protocol error should 
be generated. This is tested by a series of separate tests known,
as "test purposes", each of which is designed to test a single statement 
in the Xlib specifications. The statement which is tested is contained
in an "assertion", which is also contained in the dot-m file, and 
precedes the test code for that test purpose.
.LI
Test the ability of the Xlib function to produce the expected X Protocol 
errors in specified situations. This is tested in further test purposes, 
each preceded in the dot-m file
by an assertion describing the situation which should produce the error.
.LE
.H 2 "xtest/tset/XPROTO"
.br
This contains the source for the touch tests for the X Protocol (version 11).
These are
built using the instructions in this user guide.
In the rest of this document, these 
are refered to as the "\*(xP".
.P
These tests were in a separate test suite in the T7 \*(xT, which was
located in a directory Xproto_verf. This included
separate documentation, drivers, parameter files, include files and libraries.
In the revised \*(xT, the directory XPROTO only contains the source of 
the tests - the other items are integrated within the \*(xT as described 
in this user guide.
.P
The directory XPROTO contains further subdirectories
which are known as test set directories. 
The structure of test set directories is exactly as for the \*(xL, 
described in the previous section.
.P
The \*(xP tests are designed to accomplish the following:
.P
.AL
.LI
Test the ability of the \*(xW server to accept all legal message types and 
respond appropriately.
.LI
Ensure that the server capabilities which Xlib testing depends on 
work in the simplest cases.
.LI
Test that the X server adheres to the canonical byte stream of the X Protocol,
independent of the host byte sex or compiler structure packing.
.LE
.P
For further details of the choice of test cases and division of tests between
the \*(xP and \*(xL, refer to the document entitled 
"An Approach to Testing X Window System Servers at a Protocol Level"
contained in file xtest/doc/paper.mm.
.SK
.H 1 "Appendix B - File naming scheme"
A file naming scheme has been devised which 
is for naming the directories containing dot-m files
and the dot-m files themselves. 
.P
The file naming scheme converts from an 
\*(xW name to an abbreviated name. This is done as follows:
.DL
.LI
Remove leading X.
.LI
Replace:
.DS
    Background -> Bg
    Subwindow -> Sbwn
    String -> Str
    Window -> Wdw
.DE
.LI
Remove all lowercase vowels aeiou.
.LI
Truncate to 10 chars.
We have already checked that
truncation to ten characters still gives uniqueness.
.LI
If the string before truncation ended in "16" then force truncated
    string to end in "16".
.LI
convert to lowercase.
.LI
add ".m" suffix to get name of source file containing C code, assertions
    and strategies.
.LE
.SK
.H 1 "Appendix C - Format of the \s-1TET\s0 journal file"
This appendix describes the manner in which the \*(xT uses 
some of the \s-1TET\s0 journal file facilities. The format of the \s-1TET\s0 
journal file is not fully described here - only the main features used by the 
\*(xT are described.
In a future release of the \*(xT the format is expected to
be described fully in the \s-1TET\s0 documentation.
.P
Journal files are produced by the 
\s-1TCC\s0 
during the build and execute stages.
.P
The journal file produced during the execute stage contains two basic sections:
.AL
.LI
Details of the configuration parameters and environment in which the 
tests were executed. This may also be preceded by build configuration 
parameters and/or followed by clean configuration parameters, if the 
.SM TCC
was invoked in build-execute-clean mode.
.LI
Details of the test results.
This includes the test result
codes and test information messages output by the test suite.
.LE
.P
Each line in the second section of a journal file 
is made up from three components separated by a vertical bar:
.AL
.LI
Message type. There is a unique numeric code for each message 
type which is always the first field on a line.
.LI
Message parameters. These contain serial number and similar 
information.
.LI
Message area. The format of this area is specific to the 
Message Type.
.LE
.P
An example of the second section is as follows:
.cS
10|53 /tset/CH02/vndrrls/vndrrls 13:41:12|TC Start, scenario ref 85-1, ICs {all}
15|53 1.9 1|TCM Start
520|53 0 7457 1 1|TRACE:NAME: XVendorRelease
400|53 1 1 13:41:14|IC Start
200|53 1 13:41:14|TP Start
520|53 1 7457 1 1|REPORT:XVendorRelease() returned 5000 instead of 1.
220|53 1 1 13:41:14|FAIL
410|53 1 1 13:41:14|IC End
80|53 0 13:41:15|TC End
.cE
This consists of a block of information 
for each test set executed which contains the following
lines:
.AL
.LI
Message Type 10 - Test Case Start (TC Start)
.br
A single message indicating that the Test Case Controller (the part of the 
\s-1TET\s0 which executes test sets) is about to execute a test set.
This also indicates the start of the results for this
test set (which in \s-1TET\s0 terminology is known as a test case).
This line indicates the name of the test set
obtained from the scenario file.
.LI
Message Type 15 - Test Case Manager Start (TCM Start)
.br
A single message indicating that the Test Case Manager (the part of the 
\s-1TET\s0 which calls the test purposes) has started executing.
.LI
Each assertion in the \*(xT has separate test code, which is known as a 
test purpose. 
We have arranged that each test purpose is also 
a separately invocable component,
and that the invocable component number is identical 
to the test purpose number.
For each test purpose these
lines follow:
.AL
.LI
Message Type 400 - Invocable Component Start (IC Start)
.br
The second field of the Message Parameters gives the IC number.
.LI
Message Type 200 - Test Purpose Start (TP Start)
.br
The second field of the Message Parameters gives the
number of the test purpose within the test set.
.LI
Message Type 520 - Test Case Information
.br
The second field of the Message Parameters gives the
number of the test purpose within the test set.
.br
The Message Area contains a text message output by the \*(xT -
the possible types of message are described further in 
appendix D 
"Interpreting test results" in the section entitled 
"Test information messages".
.LI
Message Type 220 - Test Purpose Result
.br
The second field of the Message Parameters gives the
number of the test purpose within the test set.
.br
The Message Area contains the test result code - the possible 
test result codes are described further in appendix D
"Interpreting test results" in the section entitled
"Test result codes".
.LI
Message Type 410 - Invocable Component End (IC End)
.br
The second field of the Message Parameters gives the IC number.
.LE
.LI
Message Type 80 - Test Case End (TC End)
.br
A single message indicating the end of the results for this
test set (which in \s-1TET\s0 terminology is known as a test case).
.LE
.SK
.H 1 "Appendix D - Interpreting test results"
This section includes information describing the significance 
of the test result codes and the accompanying 
test information messages that may appear in a \s-1TET\s0 journal file.
.H 2 "Categorisation of assertions"
The test result codes which are output for each test purpose 
are dependent on the category of the assertion.
.P
The model for the categorisation of assertions 
which is used in the \*(xT is described in POSIX.3.
.P
There are four categories of assertions described by POSIX.3 which are 
designated A, B, C and D. 
.P
If the assertion tests a conditional feature, it is categorised as type 
C or D, otherwise it is categorised as type A or B.
.P
If the assertion is classified as an "extended assertion",
it is categorised as type
B or D. Otherwise it is categorised
as type A or C and is known as a "base assertion".
.P
Tests are always required for base assertions. Tests are not required 
for extended assertions, but should be provided if possible. 
There are a number of "extended assertions" for which tests have 
been written in the \*(xT.
Extended assertions are used to describe features that may be difficult 
to test conclusively.
.P
.TS
center box;
c | c | c.
	Base Assertion	Extended Assertion
_
Required Feature	A	B
_
Conditional Feature	C	D
.TE
.H 2 "Test result codes"
The following test result codes may be found within the \s-1TET\s0 journal
file. These will be found in Test Purpose Result lines with Message Type 220
(described in appendix C).
.P
The reason for any 
result codes 
.SM NORESULT , 
.SM UNRESOLVED 
and 
.SM UNINITIATED 
should be determined and resolved.
.VL 15 0
.LI \s-1PASS\s0
The implementation passed the test for the corresponding assertion.
.LI \s-1FAIL\s0
The implementation failed the test for the corresponding assertion.
.LI \s-1UNRESOLVED\s0
The test for the corresponding assertion commenced, but was 
unable to establish the required conditions to complete all 
required stages of the test. 
The reasons for this should be investigated
and if necessary the test rerun.
.P
In some tests, reliance is made on the successful behaviour of another 
area of the \*(xW. Where this reliance is made, and that area of 
the \*(xW does not behave as expected, a result code 
.SM UNRESOLVED 
may occur. The test information messages 
should indicate the area of the underlying problem. It may be 
necessary to look at the test results for that area
first and investigate and resolve the underlying problem
before re-running the 
.SM UNRESOLVED 
tests.
.P
Tests which give a result code of UNRESOLVED and the message "Path check error"
normally contain programming errors. The test reached the point at which a PASS
result would be assigned, but the number of check points passed 
in executing the test code differs from the expected number.
.P
Tests which give a result code of UNRESOLVED and the message 
"No CHECK marks encountered"
may be due to programming errors. 
The test reached the point at which a PASS
result would be assigned, but no check points had been passed.
This can also occur when you execute the tests using debug options.
For example, the message occurs 
when you execute tests which normally loop over windows
and pixmaps and set 
.SM XT_DEBUG_PIXMAP_ONLY=Yes 
or 
.SM XT_DEBUG_WINDOW_ONLY=Yes
or 
.SM XT_DEBUG_DEFAULT_DEPTHS=Yes
or
.SM XT_DEBUG_VISUAL_IDS=Yes .
.LI \s-1NOTINUSE\s0
Although there is an assertion within the test set, there is no
specific test provided for the assertion. This might be because 
the assertion is tested adequately as part of the test for another
assertion, or because the assertion has been automatically included
into a test set in which it is not applicable.
.P
In either case, tests which report the result code NOTINUSE do not 
need to be investigated further.
.LI \s-1UNSUPPORTED\s0
This result code may only be used for assertions in category C or D
(conditional assertions).
.P
The implementation does not support some optional feature of 
the \*(xW, which is needed to test the 
corresponding assertion. In this case, the assertion will normally
make clear what optional feature is required, and there will be 
an accompanying test information message describing the feature
which was found to be unsupported.
.LI \s-1UNTESTED\s0
This result code may only be used for assertions in category B or D
(extended assertions).
.P
The implementation could not be conclusively tested to determine 
the truth of the corresponding assertion.
.P
Note that this does not mean that no testing was performed 
in the \*(xT. There are a number of "extended assertions"
for which we have provided tests where possible (to test some likely
problem areas, for example).
.LI \s-1UNINITIATED\s0
The test for the corresponding assertion could not commence due to
a problem in an earlier test purpose or an initialisation routine.
.P
Tests which produce this result code should be resolved as for
those reporting 
.SM UNRESOLVED .
.LI \s-1NORESULT\s0
Each test purpose should output a test result code before 
completing.
This special result code will be inserted by the \s-1TET\s0 in the journal 
file, if the test purpose
did not output a test result code. This indicates
a major fault during the execution of the test set
which should be investigated.
.LI \s-1WARNING\s0
The implementation passed the test for the corresponding assertion.
Whilst the behaviour of the implementation was found to be 
acceptable,
it behaves in a manner which is not recommended in the 
X11 specification
on which the assertion is based. 
.LI \s-1FIP\s0
The contents of the journal file must be examined by hand 
to determine whether the 
implementation passed the test for the corresponding assertion.
This is used for testing functions which produce 
output whose correctness cannot be easily determined automatically
by the test suite.
.LE
.H 2 "Test information messages"
There are four types of test information messages which are output
by the \*(xT. Each one results in a \s-1TET\s0 journal file line 
with Message Type "Test Case Information" (520), and with
the Message Area beginning with one of the following keywords:
.VL 15 0
.LI \s-1REPORT\s0
This keyword is used to report
the reason for any test result codes whch are other than 
.SM PASS .
A warning message is printed by the report 
writer \fCrpt\fP, if a test information message of type 
.SM REPORT
is given in a test purpose which produced a test result code 
.SM PASS .
.LI \s-1CHECK\s0
This keyword is used to record the passing of a particular 
checkpoint in the test suite code. These messages contain 
the checkpoint number within the test purpose, and the line 
number within the source code.
.P
These messages are not output
to the journal file if the execution configuration parameter
.SM XT_OPTION_NO_CHECK 
is set to Yes. 
This option can reduce the size
of the journal file considerably.
.LI \s-1TRACE\s0
This keyword is used for any messages describing the state of the 
test being executed which are not failure messages.
.P
When running the \*(xP, messages with this keyword are output to 
the journal file, to
describe briefly the interaction between the X server and the test program.
.P
These messages are not output 
to the journal file, if the execution configuration parameter
.SM XT_OPTION_NO_TRACE 
is set to Yes.
This option can reduce the size
of the journal file considerably.
.LI \s-1DEBUG\s0
This keyword is used for debug messages
inserted during the development of the \*(xT.
.P
When running the \*(xP, messages with this keyword
are output to the journal file, when the debug level is greater than zero, to
describe in detail the interaction between the X server and the test program.
This includes the contents of requests, replies and events.
.P
This is output if the value of the
execution configuration parameter 
.SM XT_DEBUG
is greater than or equal to the level of the debug message.
.SM XT_DEBUG
may be set from 0 to 3.
.LE
.SK
.H 1 "Appendix E - Outline of Test Environment Toolkit"
The "Test Environment Toolkit" 
is a software tool developed by X/Open\*F,
.FS
X/Open is a trademark of the X/Open Company, Ltd. in the U.K. and
other countries.
.FE
UNIX International (UI)\*F,
.FS
UI is a trademark of UNIX International.
.FE
and the Open Software Foundation (OSF)\*F.
.FS
Open Software Foundation, OSF and OSF/1
are trademarks of the Open Software Foundation, Inc.
.FE
The project which produced this software and the 
associated interface specifications was also known as Project Phoenix.
.P
The \s-1TET\s0 consists of a user interface program known as the Test Case
Controller (\s-1TCC\s0). This enables test software to be built and executed.
The \s-1TCC\s0 uses configuration files to specify the environment
for both the build and execute operations. The \s-1TCC\s0 also uses a 
scenario file to control which tests to build or execute.
.P
The \s-1TCC\s0 produces a journal file which is intended to
be formated by a test suite specific report writing program.
.P
The \s-1TET\s0 also includes an Application Programming Interface (API)
Part of the API is the Test Case Manager (TCM).
This includes a 
.B main()
function which calls user supplied test functions.
The
API also includes a library of functions to manage the test 
functions and perform output operations to the journal file
in a structured fashion.
.P
Since the developers of the \s-1TET\s0 have indicated a commitment to
develop software test suites that execute within this 
environment, the \s-1TET\s0 can be seen as an emerging de facto standard
environment for test suite execution.
.P 
During stage one of the \*(xT development project we
identified that the \s-1TET\s0 provides features which are
required by the revised test suite.
.P
For this reason we have developed the revised test suite
within the \s-1TET\s0 environment, 
and supplied a copy of the \s-1TET\s0 with the revised test suite. 
.P
Release 1.9 of the \s-1TET\s0 was issued by the
developers during March 1992,
and is included in this release of the \*(xT.
The software is complete
in that the functionality is stable and the implementation
agrees with the \s-1TET\s0 specification.
Documentation including
release notes and 
\fCman\fP pages for the \s-1TET\s0 utilities, are included in
this release of the \*(xT.
However, this release does not contain a Programmers Guide or Users Guide.
These are under development by UNIX International, 
but are not complete at the time of this
release, and so are not part of the current version of the \s-1TET\s0.
.SK
.H 1 "Appendix F - Glossary"
.VL 4 0
.LI "assertion"
.br
A statement of functionality for an 
.I element
of the \*(xW,
phrased such that it will be 
.B true 
for a system conforming to the \*(xW specifications.
An example would be a 
.I "test description"
phrased according to the requirements of POSIX.3.
.LI "assertion test"
.br
Synonymous with 
.IR "test purpose" .
.LI "base assertion"
.br
An 
.I assertion
for which a test suite must provide an 
.IR "assertion test" .
Every assertion that is not an extended assertion is a base assertion.
.LI "element"
.br
A particular \*(xW interface 
such as an Xlib function, header file or X11 Procotol.
.LI "extended assertion"
.br
An 
.I assertion
for which a test suite is not required to provide an
.IR "assertion test" .
An 
.I "assertion test"
should still be provided if possible.
.P
Reasons why a test suite is not required to provide a test are given in
appendix A of the Programmers Guide.
.LI "POSIX.1"
.br
Part one of the IEEE POSIX 1003 standards, the document\(dg
entitled 
\fISystem Application Program Interface (API) [C language]\fR.
Also known as P1003.1.
.LI "POSIX.3"
.br
Part three of the IEEE POSIX 1003 standards, the document\(dg
.FS \(dg
Obtainable from Publication Sales, IEEE\ Service\ Center, P.O.\ Box\ 1331,
445\ Hoes\ Lane, Piscataway, NJ\ 08854-1331, (201)\ 981-0060
.FE
entitled 
\fITest Methods for Measuring Conformance to POSIX\fR.
Also known as P1003.3.
.LI "Project Phoenix"
.br
Synonymous with
.SM TET .
.LI SVID
.br
System V Interface Definition.
.LI TCC
.br
The Test Case Controller. This is part of the 
.SM TET .
This is a user interface program which enables the test software to be 
executed. See appendix E for more details.
.LI TCM
.br
The Test Case Manager. This is part of the 
.SM TET .
This is an object file containing a 
.B main()
function which calls user supplied test functions.
See appendix E for more details.
.LI "TET"
.br
The "Test Environment Toolkit". This is a software tool which provides a 
framework within which tests may be developed and executed.
More information is given in appendix E.
.LI "test case"
.br
Synonymous with
.IR "test set".
.LI "test description"
.br
The description of a particular test 
to be performed on an 
.IR element .
This is presented in functional terms and 
describes precisely what aspect of the \*(xW is to be tested for that 
.I element .
An 
.I assertion 
is an example of a 
.IR "test description" ,
but the reverse is not the case.
.LI "test program"
.br
Synonymous with 
.IR "test set" .
.LI "test purpose"
.br
The software which tests the conformance of an implementation of the \*(xW
to an 
.IR assertion .
.LI "test set"
.br
The software containing all the 
.I "test purposes"
for an 
.IR "element" .
.LI "test strategy"
.br
A description of the design and method used to implement a 
.IR "test purpose" . 
This should say how a
.I "test purpose"
is implemented rather than what feature is being tested.
.LI XPG
.br
The X/Open Portability Guide.
.LI "\*(xL"
.br
These are the tests for sections 2 to 10 of the 
X11R4 Xlib specifications.
They are stored in subdirectories of the directories \fCCH02\fP to \fCCH10\fP
(which are to be found in the directory \fC$TET_ROOT/xtest/tset\fP).
.LI "\*(xP"
.br
These are the touch tests for the X Protocol (version 11).
They are stored in subdirectories of the directory \fCXPROTO\fP 
(which is to be found in the directory \fC$TET_ROOT/xtest/tset\fP).
.LE
.TC
