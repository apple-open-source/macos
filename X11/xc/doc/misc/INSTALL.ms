.\" $Xorg: INSTALL.ms,v 1.5 2001/04/04 17:37:27 coskrey Exp $
.\"
.\" X11R6.6 Installation instructions. Use troff -ms macros
.\"
.\"
.\" $XFree86: xc/doc/misc/INSTALL.ms,v 1.8 2001/12/21 20:59:12 dawes Exp $
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
\s+2\fBBuilding and Installing X11R6.6\fP\s-2
.AU
.sp 6
.\".AI
.\"
.\"The X.Org Group 
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
Copyright \(co 1998,1999,2000,2001 The Open Group
.fi
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
X Window System is a trademark of The Open Group.
.\"
.if t \{
.OH 'X Version 11, Release 6.6''X Window System Installation'
.EH 'X Window System Installation Version 11, Release 6.6'
.bp 1
.ds CF \\n(PN
.\}
.nr PS 11
.nr VS 13

.nH 1 "Introduction"
.\"
This document is the installation notes that were provided with X.Org's
X11R6.6 release.  If you're building XFree86, it can be used as
a rough guide.  Be aware that most of the details are not targeted
specifically at the current XFree86 source tree.  XFree86-specific
documentation can be found in the \fBxc/programs/Xserver/hw/xfree86/doc\fP
directory and on-line at http://www.xfree86.org/current/.  Some of the
documentation there is out of date, so also be aware of that.  There is
currently no up to date document specifically targeted at building
XFree86 from source.

.nH 1 "Easy Build Instructions"
.\"
This quick summary is no substitute for reading the full build
instructions later in this document.
.LP
Edit \fBxc/config/cf/site.def\fP for local preferences.
If you want to install and use the installation from somewhere other than
\fB/usr\fP,
change \fBProjectRoot\fP. (Do \fInot\fP use \fBDESTDIR\fP.)
.LP
If you are cross compiling you will want to use \fBDESTDIR\fP to specify where
the installation should take place. Failure to do so will corrupt your
native installation of X.
.LP
If you want to build with \fIgcc\fP uncomment the \fBHasGcc2\fP line.
If you have \fIgcc\fP, but not \fIcc\fP, please read the full
build instructions.
.LP
If some time has elapsed since the initial release of R6.6, check 
to see if any public patches have been released. The source tar
files may have been updated \(em check the patch-level line in the
bug-report template. If the source in the tar files has not been
updated then get all the patches and apply them, following the 
instructions at the top of each patch. Ignore the rebuild steps 
in the patch application instructions.
.LP
.\" (never say "vendor.cf", because people look for that literally)
Check the appropriate vendor-specific \fB.cf\fP file in
\fBxc/config/cf/\fP to
make sure that \fIOSMajorVersion\fP, \fIOSMinorVersion\fP, and
\fIOSTeenyVersion\fP are set correctly for your system. On most 
systems imake will figure these out automatically; but you may 
override them in your \fBxc/config/cf/site.def\fP if you want.
.LP
See if there is a \fIBootstrapCFlags\fP mentioned in the comments
in the vendor-specific \fB.cf\fP file. (Most systems don't have
or need one. The BootstrapCFlags in \fIsun.cf\fP is for SunOS
4.0.x, so if you're building on SunOS 4.1.x or SunOS 5/Solaris 2
then BootstrapCFlags doesn't apply.) If there isn't one, \fIcd\fP 
to the \fBxc\fP directory and type (in csh):
.ID
% make World >& world.log
.DE
.LP
If there is an applicable \fBBootstrapCFlags\fP, take its value
and type:
.ID
% make World BOOTSTRAPCFLAGS="\fIvalue\fP" >& world.log
.DE
.LP
Do not call the output file \*Qmake.log\*U when doing \*Qmake World\*U.
After a successful build, you can install with:
.ID
% make install >& install.log
.DE
.LP
You can install manual pages with:
.ID
% make install.man >& man.log
.DE
.LP
\fBWhile the system is building (or if things fail), read the rest of
these installation instructions.\fP


.nH 1 "Building and Installing R6.6"

Historically the MIT X Consortium, The X Consortium, Inc., and X.Org
sample implementation releases have always been source-code-only 
releases, and this release is no different. 

.nH 2 "Introduction"

Every release of X has been progressively easier to configure, build,
and install than the preceding releases \(em and we believe this 
release is the easiest release to build yet. That notwithstanding, 
if things do go amiss during the build we assume that you have the 
basic skills necessary, and the willingness, to debug any errors 
that may occur in the build process. When you install, if you're 
going to use \fIxdm\fP or replace your system's old X, we assume 
you have a basic understanding of your system's initialization 
process. For Remote Execution (RX, embedding) we assume that 
you understand the fundamentals of HTTP, CGI, and HTML. If these 
assumptions are not correct then you should consider finding someone 
who has proficiency in these areas to do the build and install for you.
.LP
After the release has been out for a while more up to date information 
about any newly-discovered problems may be found in the \fIFrequently 
Asked Questions\fP posting, which appears monthly on the Usenet newsgroup
comp.windows.x and xpert mailing list. The FAQ is also available via 
anonymous FTP from ftp://ftp.x.org/ in the file 
ftp://ftp.x.org/contrib/faqs/FAQ.Z, or possibly on one of X 
mirror sites.

.nH 2 "Preparing Your Build System"

The source is distributed in four gzip compressed UNIX \fBT\fPape 
\fBAR\fPchive (tar) files. You will need about 230 Mb of disk space 
in order to unpack and build the release. Installing requires an 
additional 30-50 Mb assuming you have shared libraries (80-100 Mb 
without). 
.LP
On non-UNIX systems you'll need a utility that can extract gzip 
compressed tar files to extract the sources. There are several to
chose from, we do not make recommendations about which one you should
use.
.LP
Release 6.6 sources are distributed among the tar files as follows:
.LP
.KS
.DS
.ta 1.5i
xorg-1.tar	contains everything in xc/ that isn't in the other tar files
xorg-2.tar	contains xc/fonts
xorg-3.tar	contains xc/doc/specs, xc/util
xorg-4.tar	contains xc/doc/hardcopy
.DE
.KE
.LP
If you define \fIBuildFonts\fP to NO in your \fBsite.def\fP file, then
you only need to unpack xorg-1.tar to build. If you build fonts, then you 
will also need xorg-2.tar to build. If you already have the fonts from
prior releases you can use those instead of downloading them again. We
presume that you know how to copy or move them from your old source 
tree to the R6.6 source tree.

.nH 2 "Unpacking the Distribution"

Create a directory to hold the sources and \fIcd\fP into it:
.ID
% mkdir \fIsourcedir\fP
% cd \fIsourcedir\fP
.DE
Then for each tar file \fBxorg\-*.tar.gz\fP, execute this:
.ID
% gunzip\0\-c\0\fIftp-dir\fP/xorg\-\fIN\fP.tar.gz | tar xf\0\-
.DE
.LP
or if you have GNU's tar (FreeBSD, NetBSD, OpenBSD, or Linux too)
.ID
% tar xzf\0\fIftp-dir\fP/xorg\-\fIN\fP.tar.gz
.DE

.nH 2 "Applying Patches"

If some time has elapsed since the initial release of R6.6, check 
to see if any public patches have been released. The source tar
files may have been updated \(em check the patch-level line in the
bug-report template. If the source in the tar files has not been
updated then get all the patches and apply them, following the 
instructions at the top of each patch. Ignore the rebuild steps 
in the patch application instructions.
.LP
See the section \*QPublic Patches\*U later in this document.
.LP
Then continue here.

.nH 2 "Configuration Parameters (Imake Variables)"

This release, like all the releases before it, uses \fIimake\fP, 
a utility for creating system-specific Makefiles from system-independent 
Imakefiles. Almost every directory in the release contains an 
\fBImakefile\fP. System-specific configuration information is 
located in \fBxc/config/cf/\fP, which is used by the \fIimake\fP 
program every time a \fBMakefile\fP is generated in the source tree.
.LP
Most of the configuration work prior to building the release is to
set parameters (imake variables) so that \fIimake\fP will generate 
correct Makefiles. If you're building on one of the supported
systems almost no configuration work should be necessary.
.LP
You should define your configuration parameters in 
\fBxc/config/cf/site.def\fP. We provide an empty \fBsite.def\fP 
file and a \fBsite.sample\fP file. The \fBsite.sample\fP file 
is a suggested \fBsite.def\fP file \(em use it at your own risk. 
.LP
Any public patches we release will never patch \fBsite.def\fP, so 
you can be assured that applying a public-patch will not corrupt 
your site.def file. On rare occasion you may need to make the change 
in your vendor-specific \fB.cf\fP file; but you should avoid doing 
that if at all possible because any patch we might release could 
conceivably patch your vendor-specific \fB.cf\fP file and your 
change may be lost or garbled. You can override most of the things 
in your vendor-specific \fB.cf\fP file in your \fBsite.def\fP file. 
(If you can't, it's a bug \(em please file a bug-report.)
.LP
On the systems we use here, imake will automatically determine the
\fIOSMajorVersion\fP, \fIOSMinorVersion\fP, and \fIOSTeenyVersion\fP
for your system. If your system isn't one of the systems we build 
on here, or you want to build for a different version of your operating 
system, then you can override them in the appropriate entry in 
your \fBsite.def\fP file.
.LP
The \fBsite.def\fP file has two parts, one protected with
\*Q#ifdef BeforeVendorCF\*U and one with \*Q#ifdef AfterVendorCF\*U.
The file is actually processed twice, once before the \fB.cf\fP file
and once after. About the only thing you need to set in the \*Qbefore\*U
section is \fBHasGcc2\fP; just about everything else can be set in the
\*Qafter\*U section.
.LP
The \fBsite.sample\fP also has commented out support to include another 
file, \fBhost.def\fP. This scheme may be useful if you want to set most
parameters site-wide, but some parameters vary from machine to machine.
If you use a symbolic link tree, you can share \fBsite.def\fP across
all machines, and give each machine its own copy of \fBhost.def\fP.
.LP
The config parameters are listed in \fBxc/config/cf/README\fP, but
here are some of the new or more common parameters that you may wish 
to set in your \fBxc/config/cf/site.def\fP.
.Ip "ProjectRoot"
The destination where X will be installed. This variable needs to be
set before you build, as some programs that read files at run-time
have the installation directory compiled in to them. 
.Ip "HasVarDirectory"
Set to \fBNO\fP if your system doesn't have /var or you don't want
certain files to be installed in \fIVarDirectory\fP.
.Ip "VarDirectory"
The location of site editable configuration and run-time files. Many
sites prefer to install their X binaries on \fIread-only\fP media
\(em either a disk slice (partition) that's mounted \fIread-only\fP
for added security, an NFS volume mounted \fIread-only\fP for 
security and/or improved VM paging characteristics, or from a 
\fIlive filesystem\fP on a CD-ROM. In order to simplify things 
like installing \fIapp-default\fP files for locally built software, 
and allowing editing of miscellaneous configuration and policy 
files, and to allow xdm to create its master Xauthority file, some 
directories under \fI$ProjectRoot\fP\fB/lib/X11\fP are actually 
installed in \fB/var/X11\fP, and \fI$ProjectRoot\fP\fB/lib/X11\fP 
contains symlinks to the directories in \fB/var/X11\fP.
.Ip "HasGcc2"
Set to \fBYES\fP to build with \fIgcc\fP version 2.x instead of
your system's default compiler.
.Ip BuildXInputExt
Set to \fBYES\fP to build the X Input Extension. This extension
requires device-dependent support in the X server, which exists only
in \fIXhp\fP and \fIXF86_*\fP in the sample implementation.
.Ip "DefaultUsrBin"
This is a directory where programs will be found even if PATH is not 
set in the environment. It is independent of ProjectRoot and defaults 
to \fB/usr/bin\fP. It is used, for example, when connecting from a 
remote system via \fIrsh\fP. The \fIrstart\fP program installs its 
server in this directory.
.Ip "InstallServerSetUID"
Some systems require the X server to run as root to access the devices
it needs. If you are on such a system and will not be using \fIxdm\fP, 
you may set this variable to \fBYES\fP to install the X server setuid 
to root; however the X.Org Group strongly recommends that you not 
install your server suid-root, but that you use xdm instead. Talk to 
your system manager before setting this variable to \fBYES\fP.
.Ip InstallXdmConfig
By default set to NO, which suppresses installing xdm config files
over existing ones. Leave it set to NO if your site has customized
the files in \fI$ProjectRoot\fP\fB/lib/X11/xdm\fP, as many sites do.
If you don't install the new files, merge any changes present in the 
new files.
.Ip "MotifBC"
Causes Xlib and Xt to work around some bugs in older versions of Motif.
Set to \fBYES\fP only if you will be linking with Motif version 1.1.1,
1.1.2, or 1.1.3.
.Ip "GetValuesBC"
Setting this variable to \fBYES\fP allows illegal XtGetValues requests
with NULL ArgVal to usually succeed, as R5 did. Some applications
erroneously rely on this behavior. Support for this will be removed
in a future release.
.LP
The following vendor-specific \fB.cf\fP files are in the release but have
not been tested recently and hence probably need changes to work:
\fBapollo.cf\fP, \fBbsd.cf\fP,
\fBconvex.cf\fP,
\fBDGUX.cf\fP,
\fBluna.cf\fP,
\fBmacII.cf\fP,
\fBMips.cf\fP,
\fBmoto.cf\fP,
\fBOki.cf\fP,
\fBpegasus.cf\fP,
\fBx386.cf\fP.
\fBAmoeba.cf\fP is known to require additional patches.
.LP
The file \fBxc/lib/Xdmcp/Wraphelp.c\fP, for XDM-AUTHORIZATION-1, is not
included in this release. See ftp://ftp.x.org/pub/R6.6/xdm-auth/README.

.nH 2 "System Build Notes"

This section contains hints on building X with specific compilers and
operating systems.
.LP
If the build isn't finding things right, make sure you are using a 
compiler for your operating system. For example, a pre-compiled 
\fIgcc\fP for a different OS (e.g. as a cross-compiler) will not 
have right symbols defined, so \fIimake\fP will not work correctly.

.nH 3 "gcc"
.\"
X will not compile on some systems with \fIgcc\fP version 2.5, 2.5.1, 
or 2.5.2 because of an incorrect declaration of memmove() in a gcc 
fixed include file.
.LP
If you are using a \fIgcc\fP version prior to 2.7 on Solaris x86,
you need to specify
.nh
\fBBOOTSTRAPCFLAGS="\-Dsun"\fP
.hy
in the \*Qmake World\*U command.
.LP
If you're building on a system that has an unbundled compiler, e.g.
Solaris 2.x, and you do not have the \fIcc\fP compiler, you need to 
contrive to have \fIcc\fP in your path in order to bootstrap imake. 
One way to do this is to create a symlink cc that points to \fIgcc\fP.
.ID
% cd /usr/local/bin; ln \-s \fIpath-to-gcc\fP cc
.DE
Once \fIimake\fP has been built all the Makefiles created with it will 
explicitly use \fIgcc\fP and you can remove the symlink. Another way 
around this is to edit \fBxc/config/imake/Makefile.ini\fP to specify 
\fIgcc\fP instead of \fIcc\fP.

.nH 3 "Other GNU tools"
.\"
Use of the GNU BinUtils assembler, \fIas\fP, and linker, \fIld\fP, is 
not supported \(em period! If you have them installed on your system 
you must rename or remove them for the duration of the R6.6 build. 
(You can restore them afterwards.)
.LP
The system-supplied \fImake\fP works just fine for building R6.6 and 
that's what we suggest you use. If you've replaced your system's 
\fImake\fP with GNU \fImake\fP then we recommend that you restore 
the system \fImake\fP for the duration of your R6.6 build. After 
R6.6 is done building you can revert to GNU make. GNU make on most 
systems (except Linux, where it is the default make) is not a supported 
build configuration. GNU make may work for you, and if it does, great; 
but if it doesn't we do not consider it a bug in R6.6. If, after this 
admonition, you still use GNU make and your build fails, reread the 
above, and retry the build with the system's \fImake\fP before you 
file a bug-report.
.\"
.\"We broke clearmake between R6.1 and R6.3 and didn't get to fix it.
.\".nH 3 "clearmake"
.\"
.\"Atria's \fIclearmake\fP make program, part of their ClearCase product,
.\"was supported in R6.1.
.\"You will need patches to ClearCase version
.\"2.0.2 or 2.0.3.
.\"You need one of 2.0.3-61 through 2.0.3-69, as
.\"appropriate for your platform, or any later patch that fixes bug #7250.
.\"Even with these patches there is still a bug in clearmake that
.\"prevents it from correctly building the X server on HP-UX (the problem
.\"is building the HP ddx).
.\".LP
.\"To use clearmake, set the variable \fBHasClearmake\fP to \fBYES\fP.
.\"Once you make Makefiles with HasClearmake, you
.\"cannot go back and use regular make with the same Makefiles.
.\"You can use clearmake without setting HasClearmake,
.\"but you won't be able to take advantage of clearmake's
.\"file-sharing abilities.
.\"
.\".nH 3 "SparcWorks 2.0"
.\"
.\"If you have a non-threaded program and want to debug it with the old 
.\"SparcWorks 2.0 dbx, you will need to use the thread stubs library in
.\"\fBxc/util/misc/thr_stubs.c\fP. Compile it as follows:
.\".ID
.\"cc -c thr_stubs.c
.\"ar cq libthr_stubs.a thr_stubs.o
.\"ranlib libthr_stubs.a
.\".DE
.\"Install libthr_stubs.a in the same directory with your X libraries
.\"(e.g., \fI$ProjectRoot\fP\fB/lib/libthr_stubs.a\fP).
.\"Add the following line to \fBsite.def\fP:
.\".ID
.\"#define ExtraLibraries\0\-lsocket\0\-lnsl $(CDEBUGFLAGS:\-g=\-lthr_stubs)
.\".DE
.\"This example uses a \fImake\fP macro substitution; not all \fImake\fP
.\"implementations support this feature.
.\"
.\".nH 3 "CenterLine C under Solaris 2"
.\"
.\"If you are using the CenterLine C compiler to compile the distribution
.\"under Solaris 2,
.\"place the following line in your \fBsite.def\fP:
.\".ID
.\"#define HasCenterLineC YES
.\".DE
.\"If clcc is not in your default search path, add this line to \fBsite.def\fP:
.\".ID
.\"#define CcCmd \fI/path/to/your/clcc\fP
.\".DE
.\".LP
.\"If you are using CodeCenter 4.0.4 or earlier, the following files 
.\"trigger bugs in the \fIclcc\fP optimizer:
.\".ID
.\"xc/programs/Xserver/cfb16/cfbgetsp.c
.\"xc/programs/Xserver/cfb16/cfbfillsp.c
.\"xc/programs/Xserver/cfb/cfbgetsp.c
.\".DE
.\".LP
.\"Thus to build the server, you will have to compile these files by hand
.\"with the \fB\-g\fP flag:
.\".ID
.\"% cd xc/programs/Xserver/cfb16
.\"% make CDEBUGFLAGS="\-g" cfbgetsp.o cfbfillsp.o
.\"% cd ../cfb 
.\"% make CDEBUGFLAGS="\-g" cfbgetsp.o
.\".DE
.\"This optimizer bug appears to be fixed in CodeCenter 4.0.6.

.nH 3 "IBM AIX 4.x"

On AIX 4.x, the file \fBlib/font/Type1/objects.c\fP must be compiled
without optimization (\fB\-O\fP) or the X server and fontserver will 
exit when Type 1 fonts are used.

.nH 3 "SunOS 4.0.x"

SunOS 4.0 and earlier need BOOTSTRAPCFLAGS=-DNOSTDHDRS because it does
not have unistd.h and stdlib.h. Do \fInot\fP supply a BOOTSTRAPCFLAGS
when building any SunOS 4.1 or 5.x (Solaris 2) version.

.nH 3 "Linux"

On Linux systems imake has preliminary support to automatically 
determine which Linux distribution you're using. At this time it 
only automatically detects S.u.S.E. Linux. On other Linux systems
you should set the LinuxDistribution parameter in your 
\fBxc/config/cf/site.def\fP \(em see the \fBxc/config/cf/linux.cf\fP
file for the list of valid values. On Linux systems imake will 
also automatically determine which version of libc and binutils your 
system has. You may override these in your \fBxc/config/cf/site.def\fP 
file.
.LP
Many distributions of Linux have poor or no support for ANSI/POSIX/ISO
C locale support. If your Linux distribution is one of these you should 
make certain that the imake variable \fILinuxLocaleDefines\fP is set 
to \fB-DX_LOCALE\fP so that compose processing and other 
internationalization features will work correctly. To help decide if 
you should use -DX_LOCALE, look in /usr/share/locale \(em if it's 
empty, you should probably use the -DX_LOCALE define.

.nH 3 "Microsoft Windows NT"

All of the base libraries are supported, including multi-threading 
in Xlib and Xt, but some of the more complicated applications, 
specifically \fIxterm\fP and \fIxdm\fP, are not supported.
.LP
There are also some other rough edges in the implementation, such 
as lack of support for non-socket file descriptors as Xt alternate 
inputs and not using the registry for configurable parameters like 
the system filenames and search paths.
.LP
The \fIXnest\fP server has been made to run on NT; although it still
requires a real X server for output still. A real X server can not 
be built from these sources \(em in order to display X applications
on a MS-Windows host you will have to acquire a real X Server.
.LP
You have several choices for imake's \fIRmTreeCmd\fP. Look at the
possible definitions in the \fBxc/config/cf/Win32.cf\fP file, choose 
one that's right for you, and add it to your \fBxc/config/cf/site.def\fP 
file.

.nH 2 "The Build"

For all the supported UNIX and UNIX-like systems you can simply
type (in csh):
.ID
% make World >& world.log
.DE
You can call the output file something other than \*Qworld.log\*U; but
don't call it \*Qmake.log\*U because files with this name are
automatically deleted during the initial \*Qcleaning\*U stage of the 
build.
.LP
The build can take several hours on older systems, and may take as
little as an hour on the faster systems that are available today. On 
UNIX and UNIX-like systems you may want to run it in the background 
and keep a watch on the output. For example:
.ID
% make World >& world.log &
% tail\0\-f\0world.log
.DE
.LP
If something goes wrong, the easiest thing is to correct the problem
and start over again, i.e. typing \*Qmake World\*U.

.nH 3 "UNIX and UNIX-like systems"

Check your vendor-specific \fB.cf\fP file; if it doesn't have 
BootstrapCFlags that apply to your version of the operating system
then type (in csh):
.ID
% make World >& world.log
.DE
.LP
Otherwise type (in csh):
.ID
% make World BOOTSTRAPCFLAGS="value" >& world.log
.DE
.LP
None of the \fIsupported\fP operating systems need to use BOOTSTRAPCFLAGS.

.nH 3 "Microsoft Windows NT"

On NT, make certain your Path, Include, and Lib environment variables
are set accordingly. For example here we use the command line compiler
in VC++ 4.0 Standard Edition, which is installed in C:\\MSDEVSTD. To
setup the environment type:
.ID
> set Path=\fIold-path\fP;C:\\MSDEVSTD\\bin;C:\\\fIpath-to-RmTreeCmd\fP
> set Include=C:\\MSDEVSTD\\include
> set Lib=C:\\MSDEVSTD\\lib
.DE
Then to build, at the prompt, type:
.ID
C:\\> nmake World.Win32 > world.log
.DE

.nH 2 "Installing X"

After the build has successfully completed you can install the software
by typing the following as root:
.ID
% make install >& install.log
.DE
or on Microsoft Windows NT
.ID
C:\\> nmake install > install.log
.DE
.LP
Again, you might want to run this in the background and use \fItail\fP
to watch the progress.
.LP
You can install the manual pages by typing the following as root:
.ID
% make install.man >& man.log
.DE

.nH 2 "Shared Libraries"

The version number of some of the shared libraries has been changed.
On SunOS 4, which supports minor version numbers for shared libraries,
programs linked with the R6.6 libraries will use the new libraries with
no special action required. 
.LP
On most other modern operating systems the version portion of the 
library name, i.e. "6.1" portion of "libX11.so.6.1" is a string. Even 
if it's only one character long, e.g. "1" (as in libX11.so.1) it's 
still a string. This string uniquely identifies and distinguishes one 
version of the library from another. Even though all the libraries in 
this release are compatible with the libraries from previous releases, 
and there's otherwise no reason to change the version string, we do
it to identify which source release the libraries were built from.
.LP
An old program that was linked with libXext.so.6.3 won't run if you 
delete libXext.so.6.3 and install libXext.so.6.4 in its place. In
general on these systems you have the following choices:
.IP 1.
Keep the old versions of the libraries around.
.IP 2.
Relink all applications with the new libraries.
.IP 3.
Create a symlink using the old name which points to the new name.
.IP
For example, to have programs that were linked against libXext.so.6.3 
use libXext.so.6.4, make this symlink:
.ID
% cd \fI$ProjectRoot\fP/lib
% ln\0\-s libXext.so.6.4 libXext.so.6.3
.DE
.LP
On some distributions of Linux the run-time loader is broken \(em
requiring that the library's internal SONAME match the \fIfilename\fP 
\(em and the symlink solution won't work. We recommend that you get
a new run-time loader which is not broken or recompile your run-time
loader to not require that the SONAME match.

.nH 2 "Setting Up xterm"

If your \fB/etc/termcap\fP and \fB/usr/lib/terminfo\fP databases do
not have correct entries for \fIxterm\fP, use the sample entries
provided in the directory \fBxc/programs/xterm/\fP. System V users
may need to compile and install the \fBterminfo\fP entry with the
\fItic\fP utility.
.LP
Since each \fIxterm\fP will need a separate pseudoterminal, you need 
a reasonable number of them for normal execution. You probably will 
want at least 32 on a small, multiuser system. On most systems, each 
pty has two devices, a master and a slave, which are usually named 
/dev/tty[pqrstu][0-f] and /dev/pty[pqrstu][0-f]. If you don't have 
at least the \*Qp\*U and \*Qq\*U sets configured (try typing \*Qls 
/dev/?ty??\*U), you should have your system administrator add them. 
This is commonly done by running the \fIMAKEDEV\fP script in the 
\fB/dev\fP directory with appropriate arguments.

.nH 2 "Starting Servers Automatically at System Boot"

The \fIxfs\fP and \fIxdm\fP programs are designed to be run
automatically at system startup. Please read the manual pages for
details on setting up configuration files; reasonable sample files 
are in \fBxc/programs/xdm/config/\fP and \fBxc/programs/xfs/\fP.
.LP
Since \fIxfs\fP can serve fonts over the network, you do not need 
to run a font server on every machine with an X display. You should 
start \fIxfs\fP before \fIxdm\fP, since \fIxdm\fP may start an X 
server which is a client of (dependent on) the font server.

.nH 3 "On BSD-based systems using /etc/rc or /etc/rc.local"

If your system uses an \fB/etc/rc\fP or \fB/etc/rc.local\fP file 
at boot time, you can usually enable these programs by placing the 
following at or near the end of the file:
.ID
if [ \-f \fI$ProjectRoot\fP/bin/xfs ]; then
        \fI$ProjectRoot\fP/bin/xfs & echo \-n ' xfs'
fi

if [ \-f \fI$ProjectRoot\fP/bin/xdm ]; then
        \fI$ProjectRoot\fP/bin/xdm; echo \-n ' xdm'
fi
.DE
.LP
On later versions of FreeBSD the preferred way of doing this is to
create the directory \fI$ProjectRoot\fP/etc/rc.d. Add this directory to
the \fIlocal_startup\fP variable defined in /etc/rc.conf, and then
create short scripts in this directory to start xfs and xdm.
.LP
If you are unsure about how system boot works, or if your system 
does not use \fB/etc/rc\fP, consult your system administrator for 
help.

.nH 3 "On Linux systems"

Most Linux distributions have an /etc/inittab entry specifically for
xdm. Depending on your distribution this may be \fIrun-level\fP three,
four, or five. To use xdm, edit \fB/etc/inittab\fP and find the line
which contains \fIinitdefault\fP and change it from 2 to the appropriate
run-level
.LP
You Linux distribution may already have a script to start xdm at a
particular run-level. For example on S.u.S.E. Linux 5.0 there is the 
file /sbin/init.d/xdm, and the symlink /sbin/init.d/rc3.d/S30xdm 
which points to /sbin/init.d/xdm. Change /sbin/init.d/xdm to use 
\fI$ProjectRoot\fP\fI/bin/xdm\fP. You can use the xdm script as a 
model write an xfs script. Depending on your Linux distribution you 
may find these files in /etc/init.d instead of /sbin/init.d.

.nH 3 "On Digital Unix, HPUX 10, and SVR4 systems"

Most systems run xdm by default at some particular run-level of the
system. There is a master \fIinit.d\fP file and a run-level symlink 
\fIrc?.d\fP that points to the master \fIinit.d\fP file:
.DS 1
.ta 2i 3i 4.5i 
Operating System	rc?.d symlink		init.d file
.sp 1
Digital Unix 4.0	/sbin/rc3.d/S95xlogin	/sbin/init.d/xlogin
HPUX 10.20	/sbin/rc3.d/S800xdm	/sbin/init.d/xdm
Solaris 2.[0-4]
Solaris 2.5	/etc/rc3.d/S99xdm	/etc/init.d/xdm.rc
Solaris 2.6	/etc/rc2.d/S99dtlogin	/etc/init.d/dtlogin
IRIX 6.2	/etc/rc2.d/S98xdm	/etc/init.d/xdm
Unixware	/etc/rc2.d/S69xdm	/etc/init.d/xdm
.DE
In general you can edit the \fIinit.d\fP file to use
\fI$ProjectRoot\fP\fB/bin/xdm\fP. You can use the xdm file as a model 
to write an /etc/rc?.d/S??xfs file to start xfs. Some systems may 
already have files to start xfs. Starting in Solaris 2.5 Sun uses 
inetd to start xfs \(em you should remove the xfs entries from 
/etc/inetd.conf and /etc/services before adding xfs to the run-level 
files.

.nH 3 "On Unix System V-based systems"

On systems with a \fB/etc/inittab\fP file, you can edit this file to
add the lines
.ID
xfs:3:once:\fI$ProjectRoot\fP/bin/xfs
xdm:3:once:\fI$ProjectRoot\fP/bin/xdm
.DE
.LP

.nH 2 "Using OPEN LOOK applications"

You can use the X11R6.x Xsun server with OPEN LOOK applications; but 
you must pass the \fB\-swapLkeys\fP flag to the server on startup, 
or the OPEN LOOK Undo, Copy, Paste, Find, and Cut keys may not work 
correctly. For example, to run Sun's OpenWindows 3.3 desktop environment 
with an X11R6 server, use the command:
.ID
% openwin\0\-server\0\fI$ProjectRoot\fI/bin/Xsun\0\-swapLkeys
.DE
.LP
The keysyms reported by keys on the numeric keypad have also changed 
since X11R5; if you find that OpenWindows applications do not respond 
to keypad keys and cursor control keys when using an R6 server, you 
can remap the keypad to generate R5 style keysyms using the following 
\fIxmodmap\fP commands:
.ID
keysym Pause = F21
keysym Print = F22
keysym Break = F23
keysym KP_Equal = F24
keysym KP_Divide = F25
keysym KP_Multiply = F26
keysym KP_Home = F27
keysym KP_Up = Up
keysym KP_Prior = F29
keysym KP_Left = Left
keycode 100 = F31
keysym KP_Right = Right
keysym KP_End = F33
keysym KP_Down = Down
keysym KP_Next = F35
keysym KP_Insert = Insert
keysym KP_Delete = Delete
.DE

.nH 2 "Rebuilding after Patches"

Eventually you are going to make changes to the sources, for example 
by applying any public patches that may be released or to fix any
bugs you may have found.
.\"See the section \*QPublic Patches\*U later in this document.
.LP
If only source files are changed, rebuild by going to the base of
your source tree \fBxc\fP and typing:
.ID
% make >& make.log
.DE
.LP
If there are imake configuration file changes, the best thing to do 
is type:
.ID
% make Everything >& every.log
.DE
.LP
\*QEverything\*U is similar to \*QWorld\*U in that it rebuilds every
\fBMakefile\fP, but unlike \*QWorld\*U it does not delete the
existing objects, libraries, and executables, and only rebuilds
what is out of date.

.nH 2 "Formatting the Documentation"

The PostScript files in \fBxc/doc/hardcopy\fP can be generated from the
sources in \fBxc/doc/specs\fP. Most of the documentation is in troff 
using the \|\-ms macros. The easiest way to format it is to use the 
Imakefiles provided.
.LP
Set the name of your local troff program by setting the variable 
\fBTroffCmd\fP in \fBxc/config/cf/site.def\fP. Then build the Makefiles:
.ID
cd xc/doc
make SUBDIRS=specs Makefiles
.DE
.LP
Finally, go to the directory you are interested in and type \*Qmake\*U
there. This command will generate \fB.PS\fP files. You can also 
generate text files by specifying the document name with a \fB.txt\fP 
extension as a \fImake\fP target, e.g., \*Qmake icccm.txt\*U.

.nH 1 "Public Patches"

The X.Org Group may from time to time issue public patches for this 
release to fix any serious problems that are discovered. Such fixes 
are a subset of fixes available to X.Org members. Public patches are 
available via anonymous FTP from ftp://ftp.x.org/pub/R6.6/fixes, 
or from your local X mirror site.  Check the site closest to you first.
.LP
You can determine which public patches have already been applied 
to your source tree by examining the \*QVERSION\*U line of 
\fBxc/bug-report\fP. The source in the tar files you have may 
already have some patches applied; you only need to apply later 
patches. If you try to apply patches out of order or apply patches 
that are already in your tree, \fIpatch\fP will tell you that you 
have the wrong version and not apply the patch.
.LP
Source for the \fIpatch\fP program is in \fBxc/util/patch/\fP. The 
\fIpatch\fP program included on some systems may not support all the 
options this version has. If you have problems applying patches, or 
if you're otherwise in doubt, use this version.

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
