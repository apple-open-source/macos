.\" Use -ms and macros.t
.\" edited for DP edits and code consistency w/ core protocol/xlib 4/2/96
.\" $Xorg: xtest.ms,v 1.3 2000/08/17 19:42:37 cpqbld Exp $
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
\s+2\fBXTEST Extension Protocol\fP\s-2
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
.EH ''XTEST Extension Protocol''
.OH ''XTEST Extension Protocol''
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
provided.
In the C binding, defined in ``XTEST Extension Library'', routines are
provided to access the internals of two opaque data structures
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
Types
.lP
The following types are used in the request and event definitions in
subsequent sections:
.lP
FAKE_EVENT_TYPE
.Pn { KeyPress ,
.PN KeyRelease ,
.PN MotionNotify ,
.PN ButtonPress ,
.PN ButtonRelease }
.lP
.Ds 0
.TA .75i 1.25i
.ta .75i 1.25i
.R
FAKE_EVENT	[type: FAKE_EVENT_TYPE,
		detail: BYTE,
		time: TIME,
		root: WINDOW,
		rootX, rootY: INT16]
.De
.lP
CURSOR
.Pn { CurrentCursor ,
.PN  None }
or a cursor as defined by the X11 Protocol.
.NH 1
Client Operations
.lP
These are abstract definitions of functionality.  They refer to client-side
objects such as \*QGC\*U and \*QVISUAL\*U that are quoted to
denote their abstract nature.  Concrete versions of these functions are
defined only for particular language bindings.  In some circumstances
a particular language binding may not implement the relevant abstract
type or may provide it as a transparent, rather than opaque, type, with
the result that the corresponding function does not make sense or is
not required, respectively.
.LP
.sM
.PN "XTestSetGContextOfGC"
.IP
\fIgc\fP\^: \*QGC\*U
.br
\fIgid\fP\^: GCONTEXT
.LP
.eM
.LP
Sets the GCONTEXT within the \*QGC\*U gc to have 
the value specified by gid.
.LP
.sM
.PN "XTestSetVisualIDOfVisual"
.IP
\fIvisual\fP\^: \*QVISUAL\*U
.br
\fIvisualid\fP\^: VISUALID
.LP
.eM
.LP
Sets the VISUALID within the \*QVISUAL\*U visual to have
the value specified by visualid.
.LP
.sM
.PN "XTestDiscard"
.IP
\fIdpy\fP\^: \*QCONNECTION\*U
.lP
   =>
.IP
status: BOOL
.LP
.eM
.LP
Discards any requests that are present in the request buffer associated with
the \*QCONNECTION\*U dpy.
The status returned is
.PN True
if there were one or more requests
in the buffer and
.PN False
otherwise.
.NH 1
Server Requests
.LP
.sM
.PN "XTestGetVersion"
.IP
\fIclientMajorVersion\fP\^: CARD16
.br
\fIclientMinorVersion\fP\^: CARD16
.lP
   =>
.IP
serverMajorVersion: CARD16
.br
serverMinorVersion: CARD16
.IP
Errors:
.PN Length
.LP
.eM
.LP
This request can be used to ensure that the server version of the XTEST
extension is usable by the client.  This document defines major version two
(2), minor version one (1).
.LP
.sM
.PN "XTestCompareCursor"
.IP
\fIwindow\fP\^: WINDOW
.br
\fIcursor-id\fP\^: CURSOR or 
.PN CurrentCursor
or
.PN None
.lP
   =>
.IP
same: BOOL
.IP
Errors:
.PN Window ,
.PN Length ,
.PN Cursor
.LP
.eM
.LP
This request looks up the cursor associated with the window and
compares it with either the null cursor if cursor-id is
.PN None ,
or the current cursor (that is, the one being displayed),
or the cursor whose ID is cursor-id, and returns
the result of the comparison in same.
.LP
.sM
.PN "XTestFakeInput"
.IP
\fIevents\fP\^: LISTofFAKE_EVENT
.IP
Errors:
.PN Window ,
.PN Length ,
.PN Alloc ,
.PN Value
.LP
.eM
.LP
This request simulates the limited set of core protocol
events within the set FAKE_EVENT_TYPE.  Only the following event fields,
defined in FAKE_EVENT, are interpreted:
.IP \fItype\fP 1i
This must be one of
.PN KeyPress ,
.PN KeyRelease ,
.PN MotionNotify ,
.PN ButtonPress ,
or
.PN ButtonRelease ,
or else a
.PN Value
error occurs.
.IP \fIdetail\fP 1i
For key events, this field is interpreted as the physical keycode. 
If the keycode is less than min-keycode or greater than max-keycode,
as returned in the connection setup, then a
.PN Value
error occurs.
For button events, this field is interpreted as the physical (or core) button,
meaning it will be mapped to the corresponding logical button according to
the most recent
.PN SetPointerMapping
request.
If the button number is less than one or greater than the number of physical
buttons, then a
.PN Value
error occurs.
For motion events, if this field is
.PN True ,
then rootX and rootY
are relative distances from the current pointer location; if this field is
.PN False,
then they are absolute positions.
.IP \fItime\fP 1i
This is either
.PN CurrentTime
(meaning no delay)
or the delay in milliseconds that the server should wait before
simulating this event.  No other requests from this client will be
processed until this delay, if any, has expired and subsequent processing
of the simulated event has been completed.
.IP \fIroot\fP 1i
In the case of motion events this field is the ID of the root window on
which the new motion is to take place.  If
.PN None
is specified, the root window of the screen the pointer is currently on
is used instead.
If this field is not a valid window, then a
.PN Window
error occurs.
.IP "\fIrootX\fP & \fIrootY\fP" 1i
In the case of motion events these fields indicate relative distance or
absolute pointer coordinates, according to the setting of detail.
If the specified coordinates are off-screen, the closest on-screen
coordinates will be substituted.
.LP
When the simulated event(s) are processed, they cause event propagation,
passive grab activation, and so on, just as if the corresponding input device
action had occurred.  However, motion events might not be recorded in the
motion history buffer.
.LP
For the currently supported event types, the event list must have length one,
otherwise a
.PN BadLength
error occurs.
.LP
.sM
.PN "XTestGrabControl"
.IP
\fIimpervious\fP\^: BOOL
.LP
.eM
.LP
If impervious is
.PN True ,
then the executing client becomes impervious to server grabs;
that is, it can continue executing requests even if another client
grabs the server.
If impervious is
.PN False ,
then the executing client returns to the normal state of being
susceptible to server grabs.
.NH 1
Encoding
.lP
Please refer to the X11 Protocol Encoding document as this document uses
conventions established there.
.lP
The name of this extension is ``XTEST''.
.NH 2
New Types
.lP
.Ds 0
.TA .75i 1.75i
.ta .75i 1.75i
.R
FAKE_EVENT_TYPE
	2	KeyPress
	3	KeyRelease
	4	ButtonPress
	5	ButtonRelease
	6	MotionNotify
.De
.LP
NOTE that the above values are defined to be the same as those for
the corresponding core protocol event types.
.NH 2
Requests
.lP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XTestGetVersion
	1	CARD8		opcode
	1	0		xtest opcode
	2	2		request length
	1	CARD8		client major version
	1			unused
	2	CARD16		client minor version
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
 	1	1		Reply
	1	CARD8		server major version
	2	CARD16		sequence number
	4	0		reply length
	2	CARD16		server minor version
	22			unused
.De
.lP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XTestCompareCursor
	1	CARD8		opcode
	1	1		xtest opcode
	2	3		request length
	4	WINDOW		window
	4	CURSOR		cursor-id
		0	None
		1	CurrentCursor
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		Reply
	1	BOOL		cursors are the same
	2	CARD16		sequence number
	4	0		reply length
	24			unused
.De
.lP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XTestFakeInput
	1	CARD8		opcode
	1	2		xtest opcode
	2	1+(1*8)		request length
	1	FAKE_EVENT_TYPE		fake device event type
	1	BYTE		detail: button or keycode
	2			unused
	4	TIME		delay (milliseconds)
		0	CurrentTime
	4	WINDOW		root window for MotionNotify
		0	None
	8			unused
	2	INT16		x position for MotionNotify
	2	INT16		y position for MotionNotify
	8			unused
.De
.lP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XTestGrabControl
	1	CARD8		opcode
	1	3		xtest opcode
	2	2		request length
	1	BOOL		impervious
	3			unused
.De
.NH 1
References
.XP
Annicchiarico, D., et al., \fIXTrap: The XTrap Architecture\fP\^.
Digital Equipment Corporation, July 1991.
.XP
Drake, K. J., \fISome Proposals for a Minimum X11 Testing Extension\fP\^.
UniSoft Ltd., June 1991.
.LP
