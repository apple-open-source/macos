                           GL widgets for Xt/Motif

                           XFree86 implementation

                         by Carlos A. M. dos Santos
                        casantos@cpmet.ufpel.tche.br

  ------------------------------------------------------------------------

1. Introduction

This directory contains the source code for SGI's OpenGL Xt/Motif(TM)
widgets, slightly modified to generate both Motif 1.x and 2.x versions of
the widget even if Motif is not available, as in the XFree86 distribution of
the X Window System. This code is based on that distributed by Silicon
Graphics as part of it's OpenGL Sample Implementation, not on the modified
version distributed with Mesa.

2. Installation

This code is intended to be compiled as part of the normal XFree86 building
process, under the xc/lib/GLw directory. To compile the library out of the X
source tree, follow instructions below.

2.1. Requirements

Gzip and tar are needed to extract the files from the distribution archive.
Tar is a standard UNIX utility but if you don't have it use GNU tar,
available in source form at ftp://ftp.gnu.org/pub/gnu/. Gzip is not a
standard UNIX utility, though available in many systems. You may obtain it
(also source form) at the same FTP server as GNU tar.

You need xmkmf and imake too. Depending on your operating system you may
need to install some kind of X Window System development kit, so check the
vendor documentation first. I was told that some UNIX systems don't have
imake: report this as a bug (to your vendor, not me).

Of course you will need a C compiler.

2.2. Steps

  1. Extract the source code from the distribution archive.

     gzip -dc GLw.tar.gz | tar xf -

  2. Go to the surce code directory and generate the makefile.

     cd GLw
     xmkmf

     For LessTif, use "mxmkmf" instead of "xmkmf". Imake support is much
     better in recent versions of LessTif (since late july 2000). If your
     Motif or OpenGL libraries and/or include files are installed in
     non-standard locations (some UNIX vendors seem to be very creative :-)
     then edit the file named Imakefile, remove the leading XCOMM of the
     lines containing MOTIF_INCLUDES and MOTIF_LDFLAGS and set the
     appropriate values.

  3. Compile the code with

     make includes
     make standalone

  4. Install the library and manual pages with

     make install
     make install.man

  5. Optionally, you may compile two test programs: xmdemo and xtdemo. You
     need OpenGL (or Mesa) for both and Motif (or LessTif) for xmdemo. There
     are four extra make targers for these programs: demos, stand-demos,
     stand-xmdemo and stand-xtdemo. You may use them with:

     make stand-demos
     ./xmdemo ./xtdemo

2.3. Creating a shared library

By default only a static library is created because most of the UNIX
loaders, if not all, complain about unresolved symbols even if the
application doesn't use the modules in which such symbols are referenced.
However, if your system supports libraries with weak symbols (e.g. Solaris,
FreeBSD and Linux) it is possible to fool the loader. All you have to do is
edit the Imakefile, changing "#define SharedLibGLw" and "#define
GLwUseXmStubs" to YES, then repeat the compilation process starting from
step 2 in the previous section.

2.4. Problems

If you have trouble, ask for help in the XFree86 "xperts" mailing list. Look
at http://www.xfree86.org for instructions on how to subscribe. In
desperation, send an email to casantos@cpmet.ufpel.tche.br.

PLEASE DO NOT SEND ME EMAIL ASKING HOW TO INSTALL OR CONFIGURE YOUR
OPERATING SYSTEM, MODEM, NETWORK CARD, BREAD TOASTER, COFEE MAKER OR
WHATEVER ELSE!

3. Copyrights

Most of the code is covered by the following license terms:

     License Applicability. Except to the extent portions of this file
     are made subject to an alternative license as permitted in the SGI
     Free Software License B, Version 1.1 (the "License"), the contents
     of this file are subject only to the provisions of the License.
     You may not use this file except in compliance with the License.
     You may obtain a copy of the License at Silicon Graphics, Inc.,
     attn: Legal Services, 1600 Amphitheatre Parkway, Mountain View, CA
     94043-1351, or at:

     http://oss.sgi.com/projects/FreeB

     Note that, as provided in the License, the Software is distributed
     on an "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND
     CONDITIONS DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED
     WARRANTIES AND CONDITIONS OF MERCHANTABILITY, SATISFACTORY
     QUALITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.

     Original Code. The Original Code is: OpenGL Sample Implementation,
     Version 1.2.1, released January 26, 2000, developed by Silicon
     Graphics, Inc. The Original Code is Copyright © 1991-2000 Silicon
     Graphics, Inc. Copyright in any portions created by third parties
     is as indicated elsewhere herein. All Rights Reserved.

     Additional Notice Provisions: The application programming
     interfaces established by SGI in conjunction with the Original
     Code are The OpenGL® Graphics System: A Specification (Version
     1.2.1), released April 1, 1999; The OpenGL® Graphics System
     Utility Library (Version 1.3), released November 4, 1998; and
     OpenGL® Graphics with the X Window System® (Version 1.3), released
     October 19, 1998. This software was created using the OpenGL®
     version 1.2.1 Sample Implementation published by SGI, but has not
     been independently verified as being compliant with the OpenGL®
     version 1.2.1 Specification.

The demonstration programs are covered by the following license terms:

     Copyright © Mark J. Kilgard, 1995, 1996.

     NOTICE: This source code distribution contains source code
     contained in the book "Programming OpenGL for the X Window System"
     (ISBN: 0-201-48359-9) published by Addison-Wesley. The programs
     and associated files contained in the distribution were developed
     by Mark J. Kilgard and are Copyright 1994, 1995, 1996 by Mark J.
     Kilgard (unless otherwise noted). The programs are not in the
     public domain, but they are freely distributable without licensing
     fees. These programs are provided without guarantee or warrantee
     expressed or implied.

The files contained in directory GLwXm are fake Motif headers, covered by
the XFree86 license, they permit us to generate both Motif 1.x and 2.x
versions of the widget without having Motif. Notice that they are NOT part
of a standard Motif distribution and shall not be used to compile any other
code.

4. Thanks

Many thanks to Silicon Graphics and Mark J. Kilgard for making their
software free.

5. No thanks

     THIS SECTION CONTAINS MY PERSONAL OPINIONS AND DOESN'T REPRESENT
     AN OFFICIAL POSITION OF THE XFree86 PROJECT.

The first incarnation of this version of libGLw used eight header files from
LessTif, four for each Motif version. LessTif is covered by the GNU Library
General Public License (LGPL) whose terms are not compatible with the
XFree86 licensing policy. Since the copyright holder of LessTif is the Free
Software Foundation (FSF), I asked Richard Stallman, president of FSF and so
called "leader of the Free Software movement", permission to redistribute a
copy of those eight headers under XFree86 terms, still maintaining the FSF
copyright.

Observe that I was not asking him to change the license of LessTif as a
whole, but only to allow me to distribute copies of some header files
containing function prototypes, variable declarations and data type
definitions. Even so, Stallman said no because the files contained "more
than 6000 lines of code". Which code? The LessTif headers are mostly copies
of the Motif ones and don't contain any original GNU "code"! I can't still
imagine a reason for Stallman's negative answer except for paranoia. He
seems to ignore what Motif is and that LessTif's API is simply a copy of
Motif's one.

After spending some time, I made my own headers, that became much smaller
than the previous ones because I included only a subset of the Motif API and
merged everything into four files: 417 lines instead 6000. Humm, perhaps I
should be grateful to Sallman too :-).

6. Trademarks

   * OpenGL is a trademark of Silicon Graphics, Inc.
   * Motif is a trademark of the Open Group.

  ------------------------------------------------------------------------



$XFree86: xc/lib/GLw/README.txt,v 1.2 2000/11/06 21:57:10 dawes Exp $
