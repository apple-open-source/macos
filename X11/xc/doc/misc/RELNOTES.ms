.\" $Xorg: RELNOTES.ms,v 1.7 2001/04/04 17:37:28 coskrey Exp $
.\"
.\" X11R6.4 Release Notes. Use troff -ms macros
.\"
.\"
.\" $XFree86: xc/doc/misc/RELNOTES.ms,v 1.8 2001/12/21 20:59:12 dawes Exp $
.ds Ts April 4, 2001\" 
.\"
.ie t \{
.nr LL 6.5i
.\}
.el \{
.nr LL 72n
.na
.if n .pl 9999v		\" no page breaks in nroff
.\}
.nr FL \n(LLu
.nr LT \n(LLu
.ll \n(LLu
.nr PS 11
.de nH
.NH \\$1
\\$2
.XS
\\*(SN \\$2
.XE
.LP
..
.de Ip
.IP "\fB\\$1\fP" \\$2
..
.\"
.ds CH \" empty center heading
.sp 8
.TL
\s+2\fBX Window System, Version 11
.sp 1
Release 6.6\fP\s-2

\s+1\fBRelease Notes\fP\s-1
.AU
.sp 6
.\"The Open Group X Project Team
.AI
.sp 6
\*(Ts
.LP
.if t \{\
.bp
\&
.sp 3
.\}
.sp 5
.nf
Copyright \(co 1999,2000,2001 Compaq Computer Corporation
Copyright \(co 1999,2000,2001 Hewlett-Packard Company
Copyright \(co 1999,2000,2001 IBM Corporation
Copyright \(co 1999,2000,2001 Hummingbird Communications Ltd.
Copyright \(co 1999,2000,2001 Silicon Graphics, Inc.
Copyright \(co 1999,2000,2001 Sun Microsystems, Inc.
Copyright \(co 1999,2000,2001 The Open Group
.fi
.LP
All rights reserved.
.nr PS 9
.nr VS 11
.LP
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.
.LP
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
.LP
Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.
.LP
\fIX Window System\fP is a trademark of The Open Group.
.\"
.if t \{
.OH 'X Version 11, Release 6.6''X Window System Release Notes'
.EH 'X Window System Release Notes''X Version 11, Release 6.6'
.bp 1
.ds CF \\n(PN
.\}
.nr PS 11
.nr VS 13


.nH 1 "Introductory Note"
This document is the release notes that were provided with X.Org's
X11R6.6 release.  The XFree86-specific information in this document is
out of date.  The XFree86-specific documentation can be found
in the \fBxc/programs/Xserver/hw/xfree86/doc\fP directory, and on-line
at http://www.xfree86.org/current/.

.nH 1 "What Is X11"

X11, or X, is a vendor-neutral, system-architecture neutral 
network-transparent window system and user interface standard. 
In other words it's windows for UNIX. But X is not just for UNIX 
\(em X runs on a wide range of computing and graphics machines 
including Macintosh, OS/2, Microsoft's MS-Windows family of 
operating systems, and almost all of the so-called Network 
Computers. X can use your network \(em you may run CPU-intensive 
programs on high powered workstations and display the user 
interface (the windows) on inexpensive desktop machines.

.nH 1 "What is Release 6.6"

Release 6.6 (R6.6) is The X.Org Group's update to its Release 6.5.1
patch 1 and all prior releases.
It is compatible with with all releases going back to R1 at both the
source and protocol levels.

.LP
The X Consortium was an independent, not-for-profit membership
corporation formed in 1993 as the successor to the MIT X Consortium.
It was dissolved at the end of 1996 and all assets such as trademarks
and copyrights were transferred to The Open Group.  The Open Group's
X Project Team was formed to continue maintenance and development of X.
The X Project Team was disbanded after Release 6.4 patch 3.
.LP
X.Org has employed the services of a third party contractor to
establish a CVS repository of the source and a web-based interface
for submitting/viewing bug reports. The CVS repository and
web-based interface are available to X.Org members.  The contractor
has been chartered to provide quarterly update releases to the
X source.
.LP
Membership information for X.Org may be found at:
      http://www.x.org/members.htm.
.LP
This X.Org release addresses a portion of the backlog of bug reports
since Release 6.4 patch 3, along with  additional fixes from the Xfree86
community.
.LP
Instructions for building and installing R6.6 can be found in the
file INSTALL.TXT (INSTALL-X.org), available separately and also contained 
in the release.

.nH 1 "Overview of the X.Org Release"

Like all the releases that preceded it, R6.6 is a source code 
release. In order to use the release it is necessary to first 
unpack the distribution, compile it, and then install it. The 
source contains the following items:
.LP
.KS
.ID
Documentation
Sample implementations
Fonts and bitmaps
Utility libraries
Programs
.DE
.KE

.nH 1 "Supported Operating Systems"

This release was built and tested on the following reference platforms:
.LP
.KS
.ID
Compaq Tru64 UNIX V5.0
HPUX 10.20
Solaris 7.0
.DE
.KE
.LP

.nH 1 "Supported Graphics Devices"

This release includes the necessary device-dependent support to build
a native X server for the following platforms:
.LP
.KS
.ID
HP-UX: Xhp
Compaq Tru64 UNIX: Xdec on DECstation 3000/400 (Alpha) with PMAG-B
SunOS/Solaris: Xsun \(em see the Xsun man page for supported cards
XFree86: See the XF_* man pages for supported cards
.DE
.KE
In addition to the above, the Xvfb and Xnest servers can be built on
all platforms.
.LP
Native X servers are not built on AIX, Fujitsu UXP, IRIX, or 
Microsoft Windows NT.

.KS
.nH 1 "The Source Tree"

The source is distributed in UNIX tar files. The source unpacks from
the tar files into a source tree, and the name of the base directory
of the source tree is \fBxc\fP. The name \fBxc\fP as the base of the 
source tree has been retained from the X Consortium releases.

The general layout under \fBxc/\fP is as follows:
.LP
.DS L
.ta 2i
config/ 	imake config files, \fIimake\fP, \fImakedepend\fP, etc.
doc/    	all documentation other than per-program manual pages
fonts/  	BDF, Speedo, Type1 fonts
include/	common include files
lib/    	libraries
nls/    	national language support files
programs/	all programs, including the X server and \fIrgb\fP, 
util/   	\fIpatch\fP, \fIcompress\fP, other utilities
bug-report	bug reporting template
registry	X Registry
.DE
.KE

.nH 1 "X Registry"

A registry of certain X-related items is maintained to
aid in avoiding conflicts and to aid in sharing of such items. The 
registry is in the file \fBxc/registry\fP.

.nH 1 "Extensions Supported"

Release 6.6  includes source for the following extensions:
BIG-REQUESTS,
DOUBLE-BUFFER,
DPMS,
Extended-Visual-Information,
LBX,
MIT-SHM,
MIT-SUNDRY-NONSTANDARD,
Multi-Buffering,
RECORD,
SECURITY,
SHAPE,
SYNC,
TOG-CUP,
X3D-PEX,
XC-APPGROUP,
XC-MISC,
XFree86-VidModeExtension,
XIE (X Image Extension),
XINERAMA.
XInputExtension,
XKEYBOARD,
XpExtension (printing),
XTEST, and
XTestExtension1,
.LP
Not all of these extensions are standard; see the Standards manual 
page. Some of these extensions may not be supported on every platform.

.nH 1 "Implementation Dependent Parameters"

Some of the specifications define some behavior as
implementation-dependent. Implementations must document how those
parameters are implemented.

.LP
The default values in this release of the implementation dependent 
parameters are:

.IP "XFILESEARCHPATH default:"
This default can be set at build time by setting the \fIimake\fP 
variables
.hw XAppLoadDir
XFileSearchPathDefault, XAppLoadDir, XFileSearchPathBase, and
ProjectRoot in \fBxc/config/cf/site.def\fP. See 
\fBxc/config/cf/README\fP for instructions and 
\fBxc/config/cf/X11.tmpl\fP for details of how these configuration 
variables are used.
.IP
By default the imake variable ProjectRoot is \fB/usr/X11R6.6\fP 
and XFILESEARCHPATH has these components:
.ID
\fI$ProjectRoot\fP/lib/X11/%L/%T/%N%C%S
\fI$ProjectRoot\fP/lib/X11/%l/%T/%N%C%S
\fI$ProjectRoot\fP/lib/X11/%T/%N%C%S
\fI$ProjectRoot\fP/lib/X11/%L/%T/%N%S
\fI$ProjectRoot\fP/lib/X11/%l/%T/%N%S
\fI$ProjectRoot\fP/lib/X11/%T/%N%S
.DE

.IP "XUSERFILESEARCHPATH default:"
If the environment variable XAPPLRESDIR is defined, the default value
of XUSERFILESEARCHPATH has the following components:
.ID
$XAPPLRESDIR/%L/%N%C
$XAPPLRESDIR/%l/%N%C
$XAPPLRESDIR/%N%C
$HOME/%N%C
$XAPPLRESDIR/%L/%N
$XAPPLRESDIR/%l/%N
$XAPPLRESDIR/%N
$HOME/%N
.DE
Otherwise it has these components:
.KS
.ID
$HOME/%L/%N%C
$HOME/%l/%N%C
$HOME/%N%C
$HOME/%L/%N
$HOME/%l/%N
$HOME/%N
.DE
.KE

.IP "XKEYSYMDB default:"
Defaults to \fI$ProjectRoot\fP\fB/lib/X11/XKeysymDB\fP.

.IP "XCMSDB default:"
Defaults to \fI$ProjectRoot\fP\fB/lib/X11/Xcms.txt\fP.

.IP "XLOCALEDIR default:"
Defaults to the directory \fI$ProjectRoot\fP\fB/lib/X11/locale\fP.
The XLOCALEDIR variable can contain multiple colon-separated pathnames.

.IP "XErrorDB location"
The Xlib error database file is \fI$ProjectRoot\fP\fB/lib/X11/XErrorDB\fP.

.IP "XtErrorDB location"
The Xt error database file is \fI$ProjectRoot\fP\fB/lib/X11/XtErrorDB\fP.

.IP "Supported Locales"
Locales supported by this implementation are in \fBxc/nls/locale.dir\fP. 
The mapping between various system locale names and X locale names is 
in \fBxc/nls/locale.alias\fP. Both files are installed in the default
XLOCALEDIR directory, i.e. \fI$ProjectRoot\fP\fB/lib/X11/locale/\fP).

.IP "Supported Input Methods"
This distribution does not include source for any input method servers; 
however Xlib supplies a default built-in input method that supports 
compose processing in 8-bit locales. Compose files are provided for 
Latin-1 and Latin-2. The built-in input method can support other locales, 
given suitable compose files. See \fBxc/nls/Compose/iso8859-*\fP 
for the supported compositions.
.LP
The Input Method Server Development Kit (IMdkit) is at 
ftp://ftp.x.org/pub/unsupported/lib/IMdkit/.

.nH 1 "What is Unchanged in Release 6.6"

As this is an update release, there is a great deal of stability in the
standards, libraries, and clients. No existing standards have changed
in a material way.  All previous interfaces are unchanged.

.nH 1 "New OS Support"

The following table shows the versions of the operating systems
that were used to develop this and prior releases:
.LP
.KS
.DS 0
.ta 1.5i 2.25i 3i 3.75i 4.5i 5.25i
System	R6	R6.1	R6.[23]	R6.4	R6.5	R6.6
.sp
AIX	3.2.5	4.1.4	4.2	4.2	-	-
A/UX	3.0.1	-	-	-	-	-
BSD/386	1.0	-	-	-	-	-
Compaq	Tru64	UNIX	1.0/1.3	3.2C	4.0A4.0A	5.0
FreeBSD	-	2.1.0	2.1.6	2.2.2	-	-
Fujitsu	UXP	-	-	-	V20L10	-
HP-UX	9.1	10.01	10.01	10.20	10.20	10.20
IRIX	5.2	5.3	6.2	6.2	-	-
Linux	(kernel)					
Slackware	2.3	-	1.2.11	-	-	-
Slackware	3.1	-	-	2.0	-	-
S.u.S.E.	5.0	-	-	-	2.0.30	-
Mach	2.5	-	-	-	-	-
NEWS-OS	6.0	-	-	-	-	-
Solaris	2.3	2.4	2.5	2.5	7.0	7.0
SunOS	4.1.3	4.1.3	4.1.4	4.1.4	-	-
Ultrix-32	4.3	4.4	-	-	-	-
UNICOS	8.0	-	-	-	-	-
Unixware	SVR4.2	1.0	2.02	2.02	-	-
Windows	NT	3.1	3.5	4.0	3.51	-
.DE
.KE

.nH 1 "Easy Resource Configuration"

Setting and changing resources in X applications can be difficult for
both the application programmer and the end user.  \fBResource
Configuration Management (RCM)\fP addresses this problem by changing 
the \fBX Intrinsics\fP to immediately modify a resource for a 
specified widget and each child widget in the hierarchy.  
In this context, immediate means: no sourcing of a resource 
file is required; the application does not need to be restarted for 
the new resource values to take effect; and the change occurs 
immediately.
.LP
The main difference between \fBRCM\fP and the \fBEditres\fP protocol 
is that the \fBRCM\fP customizing hooks reside in the \fBIntrinsics\fP 
and thus are linked with other toolkits such as Motif and the Athena 
widgets. However, the \fBEditRes\fP protocol requires the application 
to link with the \fBEditRes\fP routines in the Xmu library and Xmu is 
not used by all applications that use Motif.
.LP
Easy Resource Configuration is not a standard part of the X Toolkit
Intrinsics (libXt). It is neither an X Consortium standard nor an
X Project Team specification.

.nH 1 "ANSIfication"

R6.1 was officially the last release that supported traditional K&R C.
Like all releases since R6.3, R6.6 assumes a Standard C compiler and
environment.  We have not intentionally removed any K&R C support from
old code, and most of the release will continue to build on platforms
without an ANSI C compiler.

.nH 1 "VSW5"

We have tested this release with VSW5 version 5.1.1A. This release 
passes all tests in VSW5 with the following exceptions:
.IP \(bu 
tests for which a permanent waiver has been granted.
.IP \(bu 
tests for which a temporary waiver have been granted.
.IP \(bu 
tests where a defect in the test has been identified and reported.
.LP
VSW licensees may obtain a list of waivers granted from
http://www.rdg.opengroup.org/interpretations/database/.

.nH 1 "XtAppPeekEvent() behavior - Environment variable no longer needed"

XtAppPeekEvent() has been modified in R6.5.1 to behave as it is
documented in the Xt spec.  Certain applications, including Netscape,
did not work properly with these modifications, so the routine was
changed so that users could set the XTAPPPEEKEVENT_SKIPTIMER environment
variable in order to make XtAppPeekEvent() behave as it did prior to
R6.5.1.
.LP
In R6.6, the routine has been changed so that there is no need to set
the environment variable.  Timers are handled as specified by the spec,
but Netscape and other applications now work properly without the
environment variable.  In R6.6, the XTAPPPEEKEVENT_SKIPTIMER variable
is ignored, and users may unset it if they were using it in R6.5.1.

.nH 1 "Year 2000 (Y2K) Compliance"

For a statement of compliance see
http://www.camb.opengroup.org/tech/desktop/faq/y2k.htm

.nH 1 "Security Considerations"

On UNIX and similar operating systems there are serious security 
implications associated with running suid-root programs. 
.LP
By default the xterm terminal emulation program is installed suid-root 
in order to be able to update utmp or utmpx entries. All the known 
(as of this writing) exploitable security holes in the X libraries have 
been eliminated \(em making it theoretically safe for xterm to be 
suid-root. For additional security you may install xterm without 
suid-root; however if you do, xterm will not be able to make utmp or 
utmpx entries.
.LP
On many Intel-based machines the X server must have root privileges
in order to access the graphics card and open other devices. The 
easiest way to grant the requisite privileges is to use xdm to run
your X server. Some people, who prefer not to use xdm, often work
around the need for the X server to run with root privileges by 
making their X server a suid-root program. While all the known (as 
of this writing) exploitable security holes in the server have been 
eliminated, it is still recommended that you \fBnot\fP 
make your X server suid-root. There are \fIsafe\fP suid-root wrapper 
programs available (but not in this release) that you can use to start 
your server if you don't want to use xdm.

.nH 1 "Filing Bug Reports"

If you find a reproducible bug in software built from the source in
this distribution or find bugs in its documentation, please complete 
a bug-report using the form in the file \fBxc/bug-report\fP and send it 
to:
.ID
mailto:xbugs@x.org
.DE
.LP
Please try to provide all of the information requested on the form if 
it is applicable; the little extra time you spend on the report will 
make it much easier for someone to reproduce, find, and fix the bug.
.LP
Bugs in the contributed software that is available on the net are not 
handled on any official basis. Consult the documentation for the 
individual software to see where (if anywhere) to report the bug.

.nH 1 "Acknowledgements"
.LP
The following individuals served as X.Org representatives for their
respective companies.  These companies are Executive Members of X.Org.

.LP
Paul Anderson of Hewlett-Packard.

.LP
Heather Lanigan of Compaq.

.LP
Danny Masci of Hummingbird.

.LP
Dan McNichol of IBM.

.LP
Richard Offer of SGI.

.LP
Steve Swales of Sun Microsystems, Inc.


The following individuals contributed significantly to the development
of X11R6.6, in the form of code, testing, and support.

.LP
Stuart Anderson of Metro Link, Inc.

.LP
Robert W. Boone of Metro Link, Inc.

.LP
Alan Coopersmith of Sun Microsystems, Inc.

.LP
Glen Cornell of Metro Link, Inc.

.LP
Ernie Coskrey of Metro Link, Inc.

.LP
Gudrun Enger of Metro Link, Inc.

.LP
Gemma Exton of Adacel Technologies Limited.

.LP
Jay Hobson of Sun Microsystems, Inc.

.LP
Steve McClure of Metro Link, Inc.

.LP
Scott McDonald of Metro Link, Inc.

.LP
Andrew Miller of Marconi Corporation.

.LP
Quentin Neill of Metro Link, Inc.

.LP
Keith Packard of The XFree86 Project.

.LP
Simon Pickup of Adacel Technologies Limited.

.LP
Paul Shearer of IBM.

.LP
Hidetoshi Tajima of Sun Microsystems, Inc.

.ie t \{
.\"
.\" print Table of Contents
.if o .bp \" blank page to make count even
.bp 1
.af PN i
.PX
.\}
.el .pl \n(nlu+1v
.\"
.\" Local Variables:
.\" time-stamp-start: "^\\.ds Ts "
.\" time-stamp-end: "\\\\\""
.\" time-stamp-format: "%b %d, %y"
.\" End:
