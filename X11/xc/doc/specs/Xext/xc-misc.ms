.\" Use -ms and macros.t
.\" $Xorg: xc-misc.ms,v 1.3 2000/08/17 19:42:37 cpqbld Exp $
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.sp 8
.ce 1
\s+2\fBXC-MISC Extension\fP\s-2
.sp 3
.ce 3
Version 1.1
X Consortium Standard
X Version 11, Release 6.4
.sp 6
.ce 4
Bob Scheifler
David P. Wiggins
.sp 6p
X Consortium, Inc.
.ps 9
.nr PS 9
.sp 8
.LP
Copyright \(co 1994 X Consortium
.LP
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the ``Software''), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
.LP
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
.LP
THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
.ps 10
.nr PS 10
.bp 1
.EH ''XC-MISC Extension''
.OH ''XC-MISC Extension''
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.NH 1
Overview
.LP
When an X client connects to an X server, it receives a fixed range of
resource IDs to use to identify the client's resources inside the X
server.  Xlib hands these out sequentially as needed.  When it
overruns the end of the range, an IDChoice protocol error results.
Long running clients, or clients that use resource IDs at a rapid
rate, may encounter this circumstance.  When it happens, there are
usually many resource IDs available, but Xlib doesn't know about them.
.LP
One approach to solving this problem would be to have Xlib notice when
a resource is freed and recycle its ID for future use.  This strategy
runs into difficulties because sometimes freeing one resource causes
others to be freed (for example, when a window is destroyed, so are
its children).  To do a complete job, Xlib would have to maintain a
large amount of state that currently resides only in the server (the
entire window tree in the above example).  Even if a less
comprehensive strategy was adopted, such as recycling only those IDs
that Xlib can identify without maintaining additional state, the
additional bookkeeping at resource creation and destruction time
would likely introduce unacceptable overhead.
.LP
To avoid the problems listed above, the server's complete knowledge of
all resource IDs in use by a client is leveraged.  This extension
provides two ways for Xlib to query the server for available resource
IDs.
Xlib can use these extension requests behind the scenes when it has
exhausted its current pool of resource IDs.
.NH 1
Requests
.LP
.PN "XCMiscGetVersion"
.IP
\fIclient_major_version\fP\^: CARD16
.br
\fIclient_minor_version\fP\^: CARD16
.LP
   =>
.IP
\fIserver_major_version\fP\^: CARD16
.br
\fIserver_minor_version\fP\^: CARD16
.LP
If supplied, the \fIclient_major_version\fP and
\fIclient_minor_version\fP indicate what version of the protocol the
client wants the server to implement.  The server version numbers
returned indicate the protocol this extension actually supports.  This
might not equal the version sent by the client.  An implementation can
(but need not) support more than one version simultaneously.  The
\fIserver_major_version\fP and the \fIserver_minor_version\fP are a
mechanism to support future revisions of the XC-MISC protocol
which may be necessary.  In general, the major version would increment
for incompatible changes, and the minor version would increment for
small, upward-compatible changes.  Servers that support the protocol
defined in this document will return a \fIserver_major_version\fP of
one (1), and a \fIserver_minor_version\fP of one (1).
.LP
.PN "XCMiscGetXIDRange"
.LP
   =>
.IP
\fIstart_id\fP\^: XID
.br
\fIcount\fP\^: CARD32
.LP
This request returns a range of available resource IDs for the client
issuing the request.  \fIstart_id\fP is the first ID in the range.
\fIcount\fP is the number of IDs in the range.  The returned range may
or may not be the largest possible range.
.LP
.PN "XCMiscGetXIDList"
.IP
\fIcount\fP\^: CARD32
.LP
   =>
.IP
\fIids\fP\^: LISTofXID
.LP
This request returns the a list of individual resource IDs in \fIids\fP.
\fIcount\fP is the number of resource IDs requested.
The number returned may be smaller than the number requested.
.NH 1
Events and Errors
.LP
No new events or errors are defined by this extension.
.NH 1
Encoding
.LP
Please refer to the X11 Protocol Encoding document as this document uses
conventions established there.
.LP
The name of this extension is ``XC-MISC''.
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XCMiscGetVersion
	1	CARD8		opcode
	1	0		XC-MISC opcode
	2	2		request length
	2	CARD16		client_major_version
	2	CARD16		client_minor_version
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
 	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		length
	2	CARD16		server_major_version
	2	CARD16		server_minor_version
	20			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XCMiscGetXIDRange
	1	CARD8		opcode
	1	1		XC-MISC opcode
	2	1		request length
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
 	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		length
	4	XID		start_id
	4	CARD32		count
	16			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN XCMiscGetXIDList
	1	CARD8		opcode
	1	2		XC-MISC opcode
	2	2		request length
	4	CARD32		count
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
 	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	CARD32		length
	4	CARD32		number of XIDs in ids
	20			unused
	4n	LISTofXID		ids
.De
