.\" Use -ms and macros.t
.\" edited for DP edits and code consistency w/ core protocol/xlib 4/2/96
.\" $Xorg: xtestlib.ms,v 1.3 2000/08/17 19:42:37 cpqbld Exp $
.de lP
.ne 8
.LP
..
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.sp 8
.ce 1
\s+2\fBXTEST Extension Library\fP\s-2
.sp 3
.ce 3
Version 2.2
X Consortium Standard
.sp 6
.ce 4
\s-1Kieron Drake
.sp 6p
UniSoft Ltd.\s+1
.bp
.sp 10
.ps 9
.nr PS 9
.sp 8
.lP
Copyright \(co 1992 by UniSoft Group Ltd.
.lP
Permission to use, copy, modify, and distribute this documentation for any
purpose and without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.  UniSoft makes no
representations about the suitability for any purpose of the information in
this document.  This documentation is provided ``as is'' without express or
implied warranty.
.lP
.sp 5
Copyright \(co 1992, 1994 X Consortium
.lP
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the ``Software''), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
.lP
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
.lP
THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.lP
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
.ps 10
.nr PS 10
.bp 1
.EH ''XTEST Extension Library''
.OH ''XTEST Extension Library''
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.NH 1
Overview
.lP
This extension is a minimal set of client and server extensions
required to completely test the X11 server with no user intervention.
.lP
This extension is not intended to support general journaling and
playback of user actions.  This is a difficult area [XTrap, 89] as it attempts
to synchronize synthetic user interactions with their effects; it is at the
higher level of dialogue recording/playback rather than at the strictly lexical
level.  We are interested only in the latter, simpler, case.  A more detailed
discussion and justification of the extension functionality is given in
[Drake, 91].
.lP
We are aiming only to provide a minimum set of facilities that
solve immediate testing and validation problems.  The testing extension
itself needs testing, where possible, and so should be as simple as possible.
.lP
We have also tried to:
.IP \(bu 5
Confine the extension to an appropriate high level within the server
to minimize portability problems.  In practice this means that the extension
should be at the DIX level or use the DIX/DDX interface, or both.  This
has effects, in particular, on the level at which \*Qinput synthesis\*U
can occur.
.IP \(bu 5
Minimize the changes required in the rest of the server.
.IP \(bu 5
Minimize performance penalties on normal server operation.
.lP
.NH 1
Description
.lP
The functions provided by this extension fall into two groups:
.IP "\fBClient Operations\fP" .5i
These routines manipulate otherwise hidden client-side behavior.  The
actual implementation will depend on the details of the actual language
binding and what degree of request buffering, GContext caching, and so on, is
provided.  In the C binding, defined in section 7, routines are provided
to access the internals of two opaque data structures 
.Pn \*- GC s
and
.PN Visual s\*-
and to discard any requests pending within the
output buffer of a connection.  The exact details can be expected to differ for
other language bindings.
.IP "\fBServer Requests\fP" .5i
The first of these requests is similar to that provided in most
extensions: it allows a client to specify a major and minor version
number to the server and for the server to respond with major and minor
versions of its own.  The remaining two requests allow the following:
.RS
.IP \(bu 5
Access to an otherwise \*Qwrite-only\*U server resource: the cursor
associated with a given window
.IP \(bu 5
Perhaps most importantly, limited synthesis of input device events,
almost as if a cooperative user had moved the pointing device
or pressed a key or button.
.RE
.NH 1
C Language Binding
.lP
The C functions either
provide direct access to the protocol and add no additional
semantics to those
defined in section 5 or they correspond directly to the abstract descriptions
of client operations in section 4.
.lP
All XTEST extension functions and procedures, and all manifest
constants and macros, will start with the string \*QXTest\*U.
All operations are classified as
server/client (Server) or client-only (Client).
All routines that have return type Status will return nonzero for
\*Qsuccess\*U and zero for \*Qfailure.\*U  Even if the XTEST extension is
supported, the server may withdraw such facilities arbitrarily; in which case
they will subsequently return zero.
.lP
The include file for this extension is
.Pn < X11/extensions/XTest.h >.
.LP
.sM
.FD 0
Bool
XTestQueryExtension(\fIdisplay\fP\^, \fIevent_base\fP\^, \fIerror_base\fP\^, \
\fImajor_version\fP\^, \fIminor_version\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     int *\fIevent_base\fP\^; /* RETURN */
.br
     int *\fIerror_base\fP\^; /* RETURN */
.br
     int *\fImajor_version\fP\^; /* RETURN */
.br
     int *\fIminor_version\fP\^; /* RETURN */
.FN
.LP
.eM
.PN XTestQueryExtension
returns
.PN True
if the specified display supports the XTEST extension, else
.PN False .
If the extension is supported, *event_base would be set to the event number for
the first event for this extension and
*error_base would be set to the error number for the first error for
this extension.  As no errors or events are defined for this version of the extension,
the values returned here are not defined (nor useful).
If the extension is supported, *major_version and *minor_version are set to
the major and minor version numbers of the extension supported by the
display.  Otherwise, none of the arguments are set.
.sp
.LP
.sM
.FD 0
Bool
XTestCompareCursorWithWindow(\fIdisplay\fP\^, \fIwindow\fP\^, \fIcursor\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     Cursor \fIcursor\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XTestCompareCursorWithWindow
performs a comparison of the cursor
whose ID is specified by cursor (which may be
.PN None )
with the cursor of the window specified by window returning
.PN True
if they are the same and
.PN False
otherwise. 
If the extension is not supported, then the request is ignored and
zero is returned.
.sp
.LP
.sM
.FD 0
Bool
XTestCompareCurrentCursorWithWindow(\fIdisplay\fP\^, \fIwindow\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.FN
.LP
.eM
If the extension is supported, 
.PN XTestCompareCurrentCursorWithWindow
performs a comparison of the current cursor
with the cursor of the specified window returning
.PN True
if they are the same and
.PN False
otherwise.
If the extension is not supported, then the request is ignored and
zero is returned.
.sp
.LP
.sM
.FD 0
XTestFakeKeyEvent(\fIdisplay\fP\^, \fIkeycode\fP\^, \fIis_press\fP\^, \fIdelay\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     unsigned int \fIkeycode\fP\^;
.br
     Bool \fIis_press\fP\^;
.br
     unsigned long \fIdelay\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XTestFakeKeyEvent
requests the server to simulate either a
.PN KeyPress
(if is_press is
.PN True )
or a
.PN KeyRelease
(if is_press is
.PN False )
of the key with the specified keycode;
otherwise, the request is ignored.
.LP
If the extension is supported,
the simulated event will not be processed until delay milliseconds
after the request is received (if delay is
.PN CurrentTime ,
then this is interpreted as no delay at all).  No other requests from
this client will be processed until this delay, if any, has expired
and subsequent processing of the simulated event has been completed.
.sp
.LP
.sM
.FD 0
XTestFakeButtonEvent(\fIdisplay\fP\^, \fIbutton\fP\^, \fIis_press\fP\^, \fIdelay\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     unsigned int \fIbutton\fP\^;
.br
     Bool \fIis_press\fP\^;
.br
     unsigned long \fIdelay\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XTestFakeButtonEvent
requests the server to simulate either
a
.PN ButtonPress
(if is_press is
.PN True )
or a
.PN ButtonRelease
(if is_press is
.PN False )
of the logical button numbered by the specified button;
otherwise, the request is ignored.
.LP
If the extension is supported,
the simulated event will not be processed until delay milliseconds
after the request is received (if delay is
.PN CurrentTime ,
then this is interpreted as no delay at all).  No other requests from
this client will be processed until this delay, if any, has expired
and subsequent processing of the simulated event has been completed.
.sp
.LP
.sM
.FD 0
XTestFakeMotionEvent(\fIdisplay\fP\^, \fIscreen_number\fP\^, \fIx\fP\^, \
\fIy\fP\^, \fIdelay\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     int \fIscreen_number\fP\^;
.br
     int \fIx\fP\^ \fIy\fP\^;
.br
     unsigned long \fIdelay\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XTestFakeMotionEvent
requests the server to simulate
a movement of the pointer to the specified position (x, y) on the
root window of screen_number;
otherwise, the request is ignored.  If screen_number is -1, the
current screen (that the pointer is on) is used.
.LP
If the extension is supported,
the simulated event will not be processed until delay milliseconds
after the request is received (if delay is
.PN CurrentTime ,
then this is interpreted as no delay at all).  No other requests from
this client will be processed until this delay, if any, has expired
and subsequent processing of the simulated event has been completed.
.sp
.LP
.sM
.FD 0
XTestFakeRelativeMotionEvent(\fIdisplay\fP\^, \fIscreen_number\fP\^, \
\fIx\fP\^, \fIy\fP\^, \fIdelay\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     int \fIscreen_number\fP\^;
.br
     int \fIx\fP\^ \fIy\fP\^;
.br
     unsigned long \fIdelay\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XTestFakeRelativeMotionEvent
requests the server to simulate
a movement of the pointer by the specified offsets (x, y) relative
to the current pointer position on screen_number;
otherwise, the request is ignored.  If screen_number is -1, the
current screen (that the pointer is on) is used.
.LP
If the extension is supported,
the simulated event will not be processed until delay milliseconds
after the request is received (if delay is
.PN CurrentTime ,
then this is interpreted as no delay at all).  No other requests from
this client will be processed until this delay, if any, has expired
and subsequent processing of the simulated event has been completed.
.sp
.LP
.sM
.FD 0
XTestGrabControl(\fIdisplay\fP\^, \fIimpervious\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Bool \fIimpervious\fP\^;
.FN
.LP
.eM
If impervious is
.PN True ,
then the executing client becomes impervious to server grabs.
If impervious is
.PN False ,
then the executing client returns to the normal state of being
susceptible to server grabs.
.sp
.LP
.sM
.FD 0
Bool
XTestSetGContextOfGC(\fIgc\fP\^, \fIgid\fP\^)
.br
     GC \fIgc\fP\^;
.br
     GContext \fIgid\fP\^;
.FN
.LP
.eM
.PN XTestSetGContextOfGC
sets the GContext within the opaque datatype referenced by gc to
be that specified by gid.
.sp
.LP
.sM
.FD 0
XTestSetVisualIDOfVisual(\fIvisual\fP\^, \fIvisualid\fP\^)
.br
     Visual *\fIvisual\fP\^;
.br
     VisualID \fIvisualid\fP\^;
.FN
.LP
.eM
.PN XTestSetVisualIDOfVisual
sets the VisualID within the opaque datatype referenced by visual to
be that specified by visualid.
.sp
.LP
.sM
.FD 0
Bool
XTestDiscard(\fIdisplay\fP\^)
.br
     Display *\fIdisplay\fP\^;
.FN
.LP
.eM
.PN XTestDiscard
discards any requests within the output buffer for the specified display.
It returns
.PN True
if any requests were discarded; otherwise, it returns
.PN False .
.NH 1
References
.XP
Annicchiarico, D., et al., \fIXTrap: The XTrap Architecture\fP\^.
Digital Equipment Corporation, July 1991.
.XP
Drake, K. J., \fISome Proposals for a Minimum X11 Testing Extension\fP\^.
UniSoft Ltd., June 1991.
.LP
