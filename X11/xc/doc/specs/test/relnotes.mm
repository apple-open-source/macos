.\" $XConsortium: relnotes.mm,v 1.7 92/07/07 16:59:43 rws Exp $
.ds dD Release Notes for the X Test Suite
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
.H 1 "Acknowledgements"
The \*(xT was produced by UniSoft Group Limited under contract to the
MIT X Consortium.
.P
.nf
UniSoft Group Limited,
Spa House,
Chapel Place,
Rivington Street,
LONDON EC2A 3DQ
.fi
.H 1 "Introduction"
This release of the \*(xT tests sections 2 to 10 
of the 
\fIXlib: C\ Language X\ Interface 
(MIT\ X\ Consortium\ Standard - X\ Version\ 11, Release 4)\fR\*F. 
.FS
The \*(xW is a trademark of the Massachusetts Institute of Technology.
.br
\*(xW Version\ 11 Release\ 4 is abbreviated to X11R4 in this document.
.br
\*(xW Version\ 11 Release\ 5 is abbreviated to X11R5 in this document.
.FE
It also tests parts of the 
\fIX\ Window\ System\ Protocol 
(MIT\ X\ Consortium\ Standard - X\ Version\ 11)\fR
where these cannot be inferred from tests at the Xlib level.
.P
The \*(xT may be used to test later versions of X11. The 
test suite is known to build correctly using the X11R5 Xlib distributed by MIT.
However, only R4 functionality is tested; new interfaces and functionality
introduced in later releases are not tested.
.H 1 "Installation"
The distribution normally comes a single tar file, either on
tape or across a network.  Create a directory to hold the distribution,
\fCcd\fP to it, and untar everything from that directory.  For example:
.DS
mkdir \fIsourcedir\fP
cd \fIsourcedir\fP
tar xfp \fItar-file-or-tape-device\fP
.DE
.P
If you have obtained compressed and split tar file over the network,
then the sequence might be:
.DS
cat xtest.?? | uncompress | (cd \fIsourcedir\fP; tar xfp -)
.DE
.P
The \fIsourcedir\fP directory you choose can be anywhere in any of your
filesystems that is convenient to you.
.P
The \*(xT requires about 10Mb of disk space to 
unpack the sources, and perhaps 50-100Mb of disk space to build
space-saving executable files (dependent on machine architecture).
If you choose to build standard executable files
you will require perhaps 100-250Mb of disk space.
See the User Guide for build instructions.
.H 1 "Documentation"
The following documentation is provided for the \*(xT.
To format the .mm files, you need the utilities \fCsoelim\fP, \fCtbl\fP, 
and \fCnroff/troff\fP with the mm macros. The file xtest/doc/Makefile contains 
rules showing how to use these utilities to format and print 
the documents.
.AL
.LI
The User Guide
gives enough information to enable an experienced test suite user,
(not necessarily familiar with the \*(xW) to configure, build and 
execute the \*(xT, and analyse the results produced.
.P
You can find the source of the User Guide
in the file xtest/doc/userguide.mm, and in PostScript
form in file xtest/doc/userguide.ps.
.LI
The Programmers Guide
gives enough information to enable an experienced programmer 
familiar with the \*(xW to modify or extend the \*(xT.
.P
You can find the source of the Programmers Guide
in the file xtest/doc/progguide.mm, and in PostScript
form in file xtest/doc/progguide.ps.
.LI
A paper distributed in the old T7 X test suite,
"An Approach to Testing X Window System Servers at a Protocol Level",
is included in this release.  This is a technical paper which defines in
outline terms the areas of the \*(xW server which should be tested at the
X Protocol level rather than the Xlib level.
.P
The approach recommended in this paper, and adopted in the design of
the T7 X test suite, has been maintained in this \*(xT.
The paper explains the choice of test cases and division of tests
between the \*(xP and \*(xL.  This paper has been left "as is"; as a result,
some sections of this paper
are out of date in that they refer to development schedules for a
previous software development project.
.P
You can find the source of this paper in the file xtest/doc/paper.mm.
.LE
.H 1 "Portability"
The main portability limitations occur in the
.SM TET
which is described further below. This is because the 
.SM TET 
was originally developed to run on systems which are POSIX.1\*F
.FS
IEEE Std 1003.1-1990, \fIPortable Operating System Interface for
Computer Environments\fR
.FE
compliant.
.P
To enable the \*(xT to build easily on 
BSD4.2 systems, a portability library has been developed which contains 
POSIX.1 functions not present on vanilla BSD4.2 systems. The 
contents and use of this library are described further in the User Guide.
Beyond this, non-POSIX systems may require 
some porting effort dependent on the number of commonly supported functions
which are absent in a particular implementation.
.P
Maintenance and enhancement of the portability library is a low priority
for the MIT X Consortium.
.P
It should be possible to build and run this test suite against any
R4 or later Xlib and X server.  However, to build and execute the complete
set of tests, your X server must support the XTEST protocol extension and
you need the library interface to this extension.  This extension is not
part of R4 or R5; it was developed after R5 was released.  The extension is
not included in this distribution, and had only been released to members
of the X Consortium at the time this distribution was released.
It is expected that the extension will be released to the public
sometime in the future (before R6).
.P
It is also possible to configure the test suite to use an
Xlib internal function to obtain raw connections to the X server.
The interface to this function was revised after R5 was released
in order to provide an adequate interface for this test suite.
The Xlib changes for this had only been released to members
of the X Consortium at the time this distribution was released.
It is expected that these changes will be released to the public
sometime in the future (before R6).
.H 1 "Status of the Test Environment Toolkit (\s-1TET\s0)"
Included in this release is a version of the
"Test Environment Toolkit"
.SM ( TET ).
This is required to build and execute the \*(xT.
The "Test Environment Toolkit" 
is a software tool developed by X/Open,
UNIX International,
and the Open Software Foundation.
.AL
.LI
The \*(xT includes a copy of
.SM TET 
version 1.9 with a small number of changes described below.
.LI
The supplied version of 
.SM TET 
includes fixes to several bugs reported since the 
.SM TET 
1.9 release.
.LI
The Makefiles supplied with
.SM TET 
1.9 have been modified slightly to use the build configuration 
scheme used by the \*(xT.
This reduces the need to edit Makefiles to modify configuration variables
when building the 
.SM TET.
.LI
You should only refer to the instructions in the User Guide for the \*(xT 
for details of installation of the 
.SM TET .
.P
For more complete information on the features of the \s-1TET\s0, you can format 
and print the on-line documentation for the
.SM TET 
(see "\s-1TET\s0 Documentation").
.LI
It is intended that the \*(xT should work in conjunction with 
future versions of the
.SM TET 
later than 1.9.
.P
You can obtain the latest released version by 
sending electronic mail to infoserver@xopen.co.uk. A message body of 
.cS
request: tet
topic: index
request: end
.cE
will obtain the index of files available for the 
.SM TET .
.LE
.H 1 "TET Documentation"
You need only refer to the instructions in the
User Guide for the \*(xT for details of installation and usage of the 
.SM TET .
.P
For more background information on the features and scope of the 
.SM TET ,
you can format and print the following items of documentation which are 
part of the 
.SM TET .
.P
Any conflict between this documentation and the User Guide for the \*(xT is
unintentional. You should assume the User Guide is correct in case of conflict,
because it has been checked against the \*(xT.
.AL
.LI
The release note for 
.SM TET 
1.9 is supplied in the file 
tet/doc/posix_c/rel_note.mm, and in PostScript
form in file tet/doc/posix_c/rel_note.ps.
.P
To format rel_note.mm, you require the utilities \fCtbl\fP,
and \fCnroff/troff\fP with the mm macros.
.LI
A manual page for the \fCtcc\fP utility is provided in file 
tet/doc/posix_c/tcc.1.
.P
To format the man page, you require the utility
\fCnroff/troff\fP with the man macros.
.LE
.H 1 "Filing bug reports"
If you find a reproducible bug in the software or documentation,
please send a bug report to MIT using the form in the file bug-report
and the destination address:
.DS
xbugs@expo.lcs.mit.edu
.DE
.P
Please try to provide all of the information requested on the form if it is
applicable; the little extra time you spend on the report will make it
much easier for us to reproduce, find, and fix the bug.  Receipt of bug
reports is generally acknowledged, but sometimes it can be delayed by a
few weeks.
.P
This test suite will report numerous bugs in the public R4 and R5
distributions from MIT, and in some cases will cause the X server to crash.
In general, it is not necessary to report bugs in the MIT Xlib and X server
software found by running this test suite to MIT.  The test suite is used
extensively at the X Consortium, and at the time of this release nearly all
bugs reported by this test suite when running on monochrome and 8-bit color
systems (as well as some 12-bit and 24-bit systems) have been corrected in
the sources maintained at MIT.  However, if you discover bugs that you think
will not show up on systems tested at MIT, feel free to report them.
.P
Bugs in TET/tcc software and documentation should not be reported to MIT.
Send TET/tcc bug reports to tet_support@xopen.co.uk.
.H 1 "Setting up your X\ server"
Your attention is drawn to section 7.1 of the User Guide entitled 
"Setting up your X\ server". You should follow the guidelines in section 
7.1.1 to obtain reliable, repeatable results against your X\ server,
when running formal verification tests.
.P
It is also important to ensure that your X\ server is running no other clients
before starting formal verification tests. This is because some test programs
(for example, those which enable access control) may interfere with later
tests unless the X\ server resets in between. To ensure the X\ server resets
after each test program, make sure you are not running any other clients 
at the time.
.SK
