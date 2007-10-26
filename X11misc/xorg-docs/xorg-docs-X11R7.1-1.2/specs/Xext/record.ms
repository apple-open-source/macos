.\" Record Extension Protocol, v1.13
.\" Use tbl, -ms and macros.t
.\" $Xorg: record.ms,v 1.3 2000/08/17 19:42:36 cpqbld Exp $
.\" $XdotOrg: doc/xorg-docs/specs/Xext/record.ms,v 1.3 2004/09/03 16:18:21 kem Exp $
.\" -----------------------------------------------
.de Ip
.IP \(bu 3
..
.de sC			\" start change (gildea).  arg is issue number
.mc \s+5\(br\s0\"	\" make tall enough to span paragraph skip
.if !^\\$1^^ \{\
'sp -1
.lt +\w'000'u+\w'\s-2\&\\$1\s0'u
.tl !!!\v'\n(.vu'\s-2\&\\$1\s0!
.lt -\w'000'u+\w'\s-2\&\\$1\s0'u
.\}
..
.de eC			\" end change
.if \\n(.u .mc \s+5\(br\s0\" ensure it appears on the last line
.mc
..
.hw RECORD-RANGE
.hw XTest-Fake-Input
.hw Button-Release Motion-Notify Record-Disable-Context
.\"
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.fi
.ps 11
.nr PS 11
\&
.sp 8
.ce 50
\s+3\fBRecord Extension Protocol Specification\fP\s-3
.sp
\fBVersion 1.13\fP
.sp
\fBX Consortium Standard\fP
.sp
\fBX Version 11, Release 6.8\fP
.sp 6
Martha Zimet
Network Computing Devices, Inc.
.sp 6
edited by
Stephen Gildea
X Consortium
.ce 0
.bp
.br
\&
.sp 13
.ps 9
.nr PS 9
.fi
.LP
Copyright \(co 1994 Network Computing Devices, Inc.
.LP
Permission to use, copy, modify, distribute, and sell this
documentation for any purpose is hereby granted without fee,
provided that the above copyright notice and this permission
notice appear in all copies.  Network Computing Devices, Inc.
makes no representations about the suitability for any purpose
of the information in this document.  This documentation is
provided \*Qas is\*U without express or implied warranty.
.LP
Copyright \(co 1994, 1995  X Consortium
.LP
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
\*QSoftware\*U), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
.LP
The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.
.LP
THE SOFTWARE IS PROVIDED \*QAS IS\*U, WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium and
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
from the X Consortium.
.ps 11
.nr PS 11
.P1
.nr LL 6.5i
.nr LT 6.5i
.nr FL 6.5i
.ll 6.5i
.EH '\fBRecord Extension Protocol, Version 1.13\fP''\fBX11, Release 6.8\fP'
.OH '\fBX11, Release 6.8\fP''\fBRecord Extension Protocol, Version 1.13\fP'
.bp 1
.EF ''\fB\\\\n(PN\fP''
.OF ''\fB\\\\n(PN\fP''
.hy 14

.NH 1
Introduction
.XS
\*(SN Introduction
.XE
.LP
Several proposals have been written over the past few years that address some
of the issues surrounding the recording and playback of user actions
in the X Window System\**:
.FS
\fIX Window System\fP is a trademark of The Open Group.
.FE
.Ip
\fISome Proposals for a Minimal X11 Testing Extension\fP,
Kieron Drake, UniSoft Ltd., April 1991
.Ip
\fIX11 Input Synthesis Extension Proposal\fP, Larry Woestman,
Hewlett Packard, November 1991
.Ip
\fIXTrap Architecture\fP, Dick Annicchiario, et al, Digital Equipment Corporation,
July 1991
.Ip
\fIXTest Extension Recording Specification\fP, Yochanan Slonim,
Mercury Interactive, December 1992
.LP
This document both unifies and extends the previous diverse approaches
to generate a proposal for an X extension that provides support for
the recording of all core X protocol and arbitrary extension protocol.
Input synthesis, or playback, has already been implemented in the
XTest extension, an X Consortium standard.  Therefore, this extension
is limited to recording.
.LP
In order to provide both record and playback functionality, a
hypothetical record application could use this extension to capture
both user actions and their consequences.  For example, a button press
(a user action) may cause a window to be mapped and a corresponding
.PN MapNotify
event to be sent (a consequence).  This information could be
stored for later use by a playback application.
.LP
The playback application could use the recorded actions as input for
the XTest extension's
.PN XTestFakeInput
operation to synthesize the
appropriate input events.  The \*Qconsequence\*U or synchronization
information is then used as a synchronization point during playback.
That is, the playback application does not generate specific
synthesized events until their matching synchronization condition
occurs.  When the condition occurs the processing of synthesized
events continues.  Determination that the condition has occurred may be
made by capturing the consequences of the synthesized events and
comparing them to the previously recorded synchronization information.
For example, if a button press was followed by a
.PN MapNotify
event on a
particular window in the recorded data, the playback application might
synthesize the button press then wait for the
.PN MapNotify
event on the
appropriate window before proceeding with subsequent synthesized
input.
.LP
Because
it is impossible to predict what synchronization information will be
required by a particular application, the extension provides
facilities to record any subset of core X protocol and arbitrary
extension protocol.
As such, this extension does not enforce a specific
synchronization methodology; any method based on information in the X
protocol stream (e.g., watching for window mapping/unmapping, cursor
changes, drawing of certain text strings, etc.) can capture the
information it needs using RECORD facilities.
.NH 2
Acknowledgements
.XS
\*(SN Acknowledgements
.XE
.LP
The document represents the culmination of two years of debate and
experiments done under the auspices of the X Consortium xtest working
group.  Although this was a group effort, the author remains
responsible for any errors or omissions.
Two years ago, Robert Chesler of Absol-puter, Kieron Drake of UniSoft
Ltd., Marc Evans of Synergytics and Ken Miller of Digitial shared the
vision of a standard extension for recording and were all instrumental
in the early protocol development.  During the last two years, Bob
Scheifler of the X Consortium and Jim Fulton of NCD continuously
provided input to the protocol design, as well as encouragement to the
author.  In the last few months, Stephen Gildea and Dave Wiggins,
both X Consortium staff, have spent considerable time fine tuning the
protocol design and reviewing the protocol specifications.  Most
recently, Amnon Cohen of Mercury Interactive has assisted in
clarification of the recorded event policy, and Kent Siefkes of
Performance Awareness has assisted in clarification of the timestamp
policy.
.ne 1.5i
.NH 2
Goals
.XS
\*(SN Goals
.XE
.LP
.RS
.Ip
To provide a standard for recording,
whereby both device events and synchronization information in the
form of device event consequences are recorded.
.Ip
To record contextual information used in synchronized playback
without prior knowledge of the application
that
is being recorded.
.Ip
To provide the ability to record arbitrary X protocol extensions.
.RE
.NH 2
Requirements
.XS
\*(SN Requirements
.XE
.LP
The extension should function as follows:
.RS
.Ip
It should
not be dependent on other clients or extensions for its operation.
.Ip
It should
not significantly impact performance.
.Ip
It should
support the recording of all device input (core devices and XInput devices).
.Ip
It should
be extendible.
.Ip
It should
support the recording of synchronization information for user events.
.RE

.NH 1
Design
.XS
\*(SN Design
.XE
.LP
This section gives an overview of the RECORD extension and discusses
its overall operation and data types.

.NH 2
Overview
.XS
\*(SN Overview
.XE
.LP
The mechanism used by this extension for recording is to intercept
core X protocol and arbitrary X extension protocol entirely within the X server
itself.  When the extension has been requested to intercept specific
protocol by one or more clients, the protocol data are formatted and
returned to the recording clients.
.LP
The extension provides a mechanism for capturing all events, including
input device events that go to no clients, that is analogous to a client
expressing \*Qinterest\*U in all events in all windows, including the root
window.  Event filtering in the extension provides a mechanism for feeding
device events to recording clients; it does not provide a mechanism for in-place,
synchronous event substitution, modification, or withholding.
In addition, the
extension does not provide data compression before intercepted protocol
is returned to the recording clients.
.NH 3
Data Delivery
.XS
\*(SN Data Delivery
.XE
.LP
Because
events are limited in size to
32 bytes, using events to return intercepted protocol data to recording
clients is prohibitive in terms of performance.  Therefore, intercepted
protocol data are returned to recording clients through multiple replies
to the extension request to begin protocol interception and reporting.
This utilization is consistent with
.PN ListFontsWithInfo ,
for example, where a
single request has multiple replies.
.LP
Individual requests, replies, events or errors intercepted by the extension
on behalf of recording clients cannot be split across reply packets.  In order
to reduce overhead, multiple intercepted requests, replies, events and errors
might be collected
into a single reply.
Nevertheless, all data are returned to the client in a timely manner.
.NH 3
Record Context
.XS
\*(SN Record Context
.XE
.LP
The extension adds a record context resource (RC)
to the set of resources managed by the server.  All the
extension operations take an RC as an argument.  Although the protocol
permits sharing of RCs between clients, it is expected that clients will
use their own RCs.  The attributes used in extension operations are stored
in the RCs, and these attributes include the protocol and clients to
intercept.
.LP
The terms \*Qregister\*U and \*Qunregister\*U are used to describe the
relationship between clients to intercept and the RC.  To register
a client with an RC means the client is added to the list
of clients to intercept; to unregister a client means the client
is deleted from the list of clients to intercept.  When the
server is requested to register or unregister clients from an RC,
it is required to do so immediately.  That is, it is not permissible for
the server to wait until recording is enabled to register clients
or recording is disabled to unregister clients.
.NH 3
Record Client Connections
.XS
\*(SN Record Client Connections
.XE
.LP
The typical communication model for a recording client is to open
two connections to the server and use one for RC control and
the other for reading protocol data.
.LP
The \*Qcontrol\*U connection can execute requests to obtain information about
the supported protocol version, create and destroy RCs, specify protocol
types to intercept and clients to be recorded, query the current state
of an RC, and to stop interception and reporting of protocol data.  The
\*Qdata\*U connection can execute a request to
enable interception
and reporting of specified protocol for a particular RC.  When the
\*Qenable\*U request is issued, intercepted protocol is sent back on the
same connection, generally in more than one reply packet.  Until the last
reply to the \*Qenable\*U request is sent by the server, signifying that
the request execution is complete, no other requests will be executed by
the server on that connection.  That is, the connection that data are being
reported on cannot issue the \*Qdisable\*U request until the last reply
to the \*Qenable\*U request is sent by the server.  Therefore, unless a
recording client never has the need to disable the interception and reporting
of protocol data, two client connections are necessary.
.NH 3
Events
.XS
\*(SN Events
.XE
.LP
The terms \*Qdelivered events\*U and \*Qdevice events\*U are used
to describe the two event classes recording clients may
select for interception.  These event classes are handled differently
by the extension.  Delivered events are core X events or X extension events
the server actually delivers to one or more clients.  Device events are
events generated by core X devices or extension input devices that the
server may or may not deliver to any clients.  When device events
are selected for interception by a recording client, the extension
guarantees each device event is recorded and will be forwarded
to the recording client in the same order it is generated by the
device.
.LP
The recording of selected device events is not affected
by server grabs.  Delivered events, on the other hand, can be affected
by server grabs.
If a recording client selects both
a device event and delivered events that result from that device
event, the delivered events are recorded after the device event.
In the absence of grabs, the delivered events for a
device event precede later device events.
.LP
Requests that have side effects on
devices, such as
.PN WarpPointer
and
.PN GrabPointer
with a confine-to window,
will cause RECORD to record an associated device event.
The XTEST extension request
.PN XTestFakeInput
causes a device event to be recorded; the
device events are recorded in the same order that the
.PN XTestFakeInput
requests are received by the server.
.LP
If a key autorepeats, multiple
.PN KeyPress
and
.PN KeyRelease
device events are reported.
.NH 3
Timing
.XS
\*(SN Timing
.XE
.LP
Requests are recorded just before
they are executed; the time associated with a request is the server
time when it is recorded.

.ne 1.5i
.NH 2
Types
.XS
\*(SN Types
.XE
.sp
.LP
The following new types are used in the request definitions that appear
in section 3.
.LP
.TS
tab(@);
l l.
RC:@CARD32
.TE
.LP
The
.PN "RC"
type is a resource identifier for a server record context.
.LP
.TS
tab(@);
l l l.
RANGE8:@\s+2[\s0\fIfirst\fP, \fIlast\fP\^:@CARD8\s+2]\s0
RANGE16:@\s+2[\s0\fIfirst\fP, \fIlast\fP\^:@CARD16\s+2]\s0
EXTRANGE:@\s+2[\s0\fImajor\fP\^:@RANGE8
@\fIminor\fP\^:@RANGE16\s+2]\s0
.TE
.LP
.TS
tab(@);
l l l.
RECORDRANGE:@\s+2[\s0\fIcore-requests\fP\^:@RANGE8
@\fIcore-replies\fP\^:@RANGE8
@\fIext-requests\fP\^:@EXTRANGE
@\fIext-replies\fP\^:@EXTRANGE
@\fIdelivered-events\fP\^:@RANGE8
@\fIdevice-events\fP\^:@RANGE8
@\fIerrors\fP\^:@RANGE8
@\fIclient-started\fP\^:@BOOL
@\fIclient-died\fP\^:@BOOL\s+2]\s0
.TE
.LP
The
.PN "RECORDRANGE"
structure contains the protocol values to intercept.  Typically,
this structure is sent by recording clients over the control connection
when creating or modifying an RC.
.IP \fIcore-requests\fP
.IN "core-requests"
.br
Specifies core X protocol requests with an opcode field between \fIfirst\fP
and \fIlast\fP inclusive.  If \fIfirst\fP is equal to 0 and \fIlast\fP is equal to 0, no
core requests are specified by this RECORDRANGE.  If \fIfirst\fP is greater
than \fIlast\fP, a
.PN "Value"
error results.
.IP \fIcore-replies\fP
.IN "core-replies"
.br
Specifies replies resulting from core X protocol requests with an opcode
field between \fIfirst\fP and \fIlast\fP inclusive.  If \fIfirst\fP is equal to 0 and \fIlast\fP
is equal to 0, no core replies are specified by this RECORDRANGE.  If
\fIfirst\fP is greater than \fIlast\fP, a
.PN "Value"
error results.
.IP \fIext-requests\fP
.IN "ext-requests"
.br
Specifies extension protocol requests with a major opcode field between
\fImajor.first\fP and \fImajor.last\fP and a minor opcode field between \fIminor.first\fP
and \fIminor.last\fP inclusive.
If \fImajor.first\fP and \fImajor.last\fP are equal to 0, no
extension protocol requests are specified by this RECORDRANGE.  If
\fImajor.first\fP or \fImajor.last\fP is less than 128 and greater than 0,
if \fImajor.first\fP is greater than \fImajor.last\fP,
or if \fIminor.first\fP
is greater than \fIminor.last\fP, a
.PN "Value"
error results.
.IP \fIext-replies\fP
.IN "ext-replies"
.br
Specifies replies resulting from extension protocol requests with a
major opcode field between \fImajor.first\fP and \fImajor.last\fP and
a minor opcode field between \fIminor.first\fP and \fIminor.last\fP
inclusive.  If \fImajor.first\fP and \fImajor.last\fP are equal to 0,
no extension protocol replies are specified by this RECORDRANGE.  If
\fImajor.first\fP or \fImajor.last\fP is less than 128 and greater
than 0,
if \fImajor.first\fP is greater than \fImajor.last\fP,
or if \fIminor.first\fP is greater than \fIminor.last\fP, a
.PN "Value"
error results.
.IP \fIdelivered-events\fP
.IN "delivered-events"
.br
This is used for both core X protocol events and arbitrary extension
events.  Specifies events that are delivered to at least one client
that have a code field between \fIfirst\fP and \fIlast\fP
inclusive.  If \fIfirst\fP is equal to 0 and \fIlast\fP is equal to 0,
no events are specified by this RECORDRANGE.
Otherwise, if \fIfirst\fP is less than 2
or \fIlast\fP is less than 2, or if
\fIfirst\fP is greater than \fIlast\fP, a
.PN "Value"
error results.
.IP \fIdevice-events\fP
.IN "device-events"
.br
This is used for both core X device events and X extension device
events that may or may not be delivered to a client.
Specifies device events that have a code field between \fIfirst\fP and
\fIlast\fP inclusive.  If \fIfirst\fP is equal to 0 and \fIlast\fP
is equal to 0, no device events are specified by this RECORDRANGE.
Otherwise,
if \fIfirst\fP is less than 2 or \fIlast\fP is less
than 2, or if \fIfirst\fP is greater than \fIlast\fP, a
.PN "Value"
error results.
.IP
Because
the generated device event may or may not be associated with a
client, unlike other RECORDRANGE components, which select protocol for a
specific client, selecting for device events in any RECORDRANGE in an RC
causes the recording client to receive one instance for each device event
generated that is in the range specified.
.IP \fIerrors\fP
.IN "errors"
.br
This is used for both core X protocol errors and arbitrary extension
errors.  Specifies errors that have a code field between \fIfirst\fP and
\fIlast\fP inclusive.  If \fIfirst\fP is equal to 0 and \fIlast\fP is equal to 0, no
errors are specified by this RECORDRANGE.  If \fIfirst\fP is greater
than \fIlast\fP, a
.PN "Value"
error results.
.IP \fIclient-started\fP
.IN "client-started"
.br
Specifies the connection setup reply.
If
.PN False ,
the connection setup reply is not specified by
this RECORDRANGE.
.IP \fIclient-died\fP
.IN "client-died"
.br
Specifies notification when a client disconnects.
If
.PN False ,
notification when a client disconnects is not specified by
this RECORDRANGE.
.LP
.TS
tab(@);
l l l.
ELEMENT_HEADER:@\s+2[\s0\fIfrom-server-time\fP\^:@BOOL
@\fIfrom-client-time\fP\^:@BOOL
@\fIfrom-client-sequence\fP\^:@BOOL\s+2]\s0
.TE
.LP
The
.PN ELEMENT_HEADER
structure specifies additional data that precedes each protocol
element in the \fIdata\fP field of a
.PN RecordEnableContext
reply.
.Ip
If \fIfrom-server-time\fP is
.PN True ,
each intercepted protocol element
with category
.PN FromServer
is preceded by the server time when the protocol was recorded.
.Ip
If \fIfrom-client-time\fP is
.PN True ,
each intercepted protocol element
with category
.PN FromClient
is preceded by the server time when the protocol was recorded.
.Ip
If \fIfrom-client-sequence\fP is
.PN True ,
each intercepted protocol
element with category
.PN FromClient
or
.PN ClientDied
is preceded by the
32-bit sequence number of the recorded client's most recent request
processed by the server at that time.
For
.PN FromClient ,
this will be one less than the sequence number of the
following request.
For
.PN ClientDied ,
the sequence number will be the only data, because no
protocol is recorded.
.LP
Note that a reply containing device events is treated the same as
other replies with category
.PN FromServer
for purposes of these flags.
Protocol with category
.PN FromServer
is never preceded by a sequence
number because almost all such protocol has a sequence number in it anyway.
.LP
If both a server time and a sequence number have been requested for a
reply, each protocol request is
preceded first by the time and second by the sequence number.
.LP
.TS
tab(@);
l l.
XIDBASE:@CARD32
.TE
.LP
The XIDBASE type is used to identify a particular client.  Valid
values are any existing resource identifier
of any connected client,
in which case the client
that created the resource is specified, or the resource identifier
base sent to the target client from the server in the connection setup
reply.  A value of 0 (zero) is valid when the XIDBASE is associated
with device events that may not have been delivered to a client.
.LP
.TS
tab (@) ;
l l l.
CLIENTSPEC:@XIDBASE or \s+2{\s0\fBCurrentClients\fP, \fBFutureClients\fP, \fBAllClients\fP\s+2}\s0
.TE
.LP
The CLIENTSPEC type defines the set of clients the RC attributes are
associated with.  This type is used by recording clients when creating
an RC or when changing RC attributes.  XIDBASE specifies that the RC
attributes apply to a single client only.
.PN CurrentClients
specifies
that the RC attributes apply to current client connections;
.PN FutureClients
specifies future client connections;
.PN AllClients
specifies all client connections, which includes current and future.
.LP
The numeric values for
.PN CurrentClients ,
.PN FutureClients
and
.PN AllClients
are
defined such that there will be no intersection with valid XIDBASEs.
.LP
When the context is enabled, the data connection is unregistered if it
was registered.
If the context is enabled,
.PN CurrentClients
and
.PN AllClients
silently exclude the recording data connection.
It is an error to explicitly register the data connection.
.LP
.KS
.TS
tab (@) ;
l l l.
CLIENT_INFO\^:@\s+2[\s0\fIclient-resource\fP\^:@CLIENTSPEC
@\fIintercepted-protocol\fP\^:@LISTofRECORDRANGE\s+2]\s0
.TE
.KE
.LP
This structure specifies an intercepted client and the protocol to be
intercepted for the client.  The \fIclient-resource\fP field is a
resource base that identifies the intercepted client.  The
\fIintercepted-protocol\fP field specifies the protocol to intercept
for the \fIclient-resource\fP.

.NH 2
Errors
.LP
.IP \fBRecordContext\fP
.IN RecordContext
.br
This error is returned if the value for an RC argument
in a request does not name a defined record context.

.NH 1
Protocol Requests
.XS
\*(SN Protocol Requests
.XE
.sp
.LP
.PN "RecordQueryVersion"
.TA .75i
.ta .75i
.IP
\fImajor-version\fP, \fIminor-version\fP\^: CARD16
.LP
\(->
.IP
\fImajor-version\fP, \fIminor-version\fP\^: CARD16
.LP
This request specifies the RECORD extension protocol version the client
would like to use.  When the specified protocol version is supported
by the extension, the protocol version the server expects from the
client is returned.  Clients must use this request before other RECORD
extension requests.
.LP
This request also determines whether or not the RECORD extension protocol
version specified by the client is supported by the extension.  If the
extension supports the version specified by the client, this version number
should be returned.  If the client has requested a higher version than is
supported by the server, the server's highest version should be returned.
Otherwise, if the client has requested a lower version than is supported
by the server, the server's lowest version should be returned.  This document
defines major version one (1),
minor version thirteen (13).
.LP
.PN "RecordCreateContext"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.IP
\fIelement-header\fP\^: ELEMENT_HEADER
.IP
\fIclient-specifiers\fP\^: LISTofCLIENTSPEC
.IP
\fIranges\fP\^: LISTofRECORDRANGE
.br
.IP
Errors:
.PN Match ,
.PN Value ,
.PN IDChoice ,
.PN Alloc
.LP
This request creates a new
record context
within the server and assigns the identifier \fIcontext\fP to
it.  After the \fIcontext\fP is created, this request registers the
set of clients in \fIclient-specifiers\fP with the \fIcontext\fP and
specifies the protocol to intercept for those clients.
The recorded protocol elements will be preceded by data as specified
by \fIelement-header\fP.
Typically,
this request is used by a recording client over the control
connection.  Multiple RC
objects can exist simultaneously, containing overlapping sets of
protocol and clients to intercept.
.LP
If any of the values in
\fIelement-header\fP or
\fIranges\fP is invalid, a
.PN "Value"
error results.  Duplicate items in the list of \fIclient-specifiers\fP are
ignored.  If any item in the \fIclient-specifiers\fP list is not a valid
CLIENTSPEC, a
.PN "Match"
error results.  Otherwise, each item in the \fIclient-specifiers\fP list is
processed as follows:
.Ip
If the item is an XIDBASE identifying a particular client, the
specified client is registered with the \fIcontext\fP and the protocol
to intercept for the client is then set to \fIranges\fP.
.Ip
If the item is
.PN CurrentClients ,
all existing clients are registered with the
\fIcontext\fP at this time.
The protocol to intercept for all clients registered
with the \fIcontext\fP is then set to \fIranges\fP.
.Ip
If the item is
.PN FutureClients ,
all clients that connect to the server
after this request executes will be automatically registered with the
\fIcontext\fP.  The protocol to intercept for such clients will be set to
\fIranges\fP in the \fIcontext\fP.
.Ip
If the item is
.PN AllClients ,
the effect is as if the actions described
for
.PN FutureClients
are performed, followed by the actions for
.PN CurrentClients .
.LP
The
.PN "Alloc"
error results when the server is unable to allocate the necessary
resources.

.LP
.PN "RecordRegisterClients"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.IP
\fIelement-header\fP\^: ELEMENT_HEADER
.IP
\fIclient-specifiers\fP\^: LISTofCLIENTSPEC
.IP
\fIranges\fP\^: LISTofRECORDRANGE
.br
.IP
Errors:
.PN Match ,
.PN Value ,
.PN RecordContext ,
.PN Alloc
.LP
This request registers the set of clients in \fIclient-specifiers\fP with
the given \fIcontext\fP and specifies the protocol to intercept for those
clients.
The header preceding each recorded protocol element is set as specified
by \fIelement-header\fP.
These flags affect the entire
context; their effect is not limited to the clients registered by
this request.
Typically, this request is used by a recording client over
the control connection.
.LP
If \fIcontext\fP does not name a valid RC, a
.PN "RecordContext"
error results.  If any of the values in
\fIelement-header\fP or \fIranges\fP is invalid, a
.PN "Value"
error results.  Duplicate items in the list of \fIclient-specifiers\fP are
ignored.  If any item in the list of \fIclient-specifiers\fP is not a
valid CLIENTSPEC, a
.PN "Match"
error results.
If the \fIcontext\fP is enabled and the XID of the enabling connection
is specified, a
.PN "Match"
error results.
Otherwise, each item in the \fIclient-specifiers\fP list is
processed as follows:
.Ip
If the item is an XIDBASE identifying a particular client, the
specified client is registered with the \fIcontext\fP if it is not already
registered.  The protocol to intercept for the client is then set to
\fIranges\fP.
.Ip
If the item is
.PN CurrentClients ,
all existing clients that are not
already registered with the specified \fIcontext\fP,
except the enabling connection if the \fIcontext\fP is enabled,
are registered at this
time.  The protocol to intercept for all clients registered with the
\fIcontext\fP is then set to \fIranges\fP.
.Ip
If the item is
.PN FutureClients ,
all clients that connect to the server
after this request executes will be automatically registered with the
\fIcontext\fP.  The protocol to intercept for such clients will be set to
\fIranges\fP in the \fIcontext\fP.
The set of clients that are registered with the
\fIcontext\fP and their corresponding sets
of protocol to intercept are left intact.
.Ip
If the item is
.PN AllClients ,
the effect is as if the actions described
for
.PN FutureClients
are performed, followed by the actions for
.PN CurrentClients .
.LP
The
.PN "Alloc"
error results when the server is unable to allocate the necessary
resources.

.LP
.PN "RecordUnregisterClients"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.IP
\fIclient-specifiers\fP\^: LISTofCLIENTSPEC
.br
.IP
Errors:
.PN Match ,
.PN RecordContext
.LP
This request removes the set of clients in \fIclient-specifiers\fP from the
given \fIcontext\fP's set of registered clients.  Typically, this request is
used by a recording client over the control connection.
.LP
If \fIcontext\fP does not name a valid RC, a
.PN "RecordContext"
error results.  Duplicate items in the list of \fIclient-specifiers\fP are
ignored.  If any item in the list is not a valid CLIENTSPEC, a
.PN "Match"
error results.  Otherwise, each item in the \fIclient-specifiers\fP list is
processed as follows:
.Ip
If the item is an XIDBASE identifying a particular client, and the
specified client is currently registered with the \fIcontext\fP, it is
unregistered, and the set of protocol to intercept for the client is
deleted from the \fIcontext\fP.  If the specified client is not registered
with the \fIcontext\fP, the item has no effect.
.Ip
If the item is
.PN CurrentClients ,
all clients currently registered with
the \fIcontext\fP are unregistered from it, and their corresponding sets of
protocol to intercept are deleted from the \fIcontext\fP.
.Ip
If the item is
.PN FutureClients ,
clients that connect to the server after
this request executes will not automatically be registered with the
\fIcontext\fP.  The set of clients that are registered with this context
and their corresponding sets of protocol that will be
intercepted are left intact.
.Ip
If the item is
.PN AllClients ,
the effect is as if the actions described
for
.PN FutureClients
are performed, followed by the actions for
.PN CurrentClients .
.LP
A client is unregistered automatically when it disconnects.

.LP
.PN "RecordGetContext"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.LP
\(->
.IP
\fIenabled\fP\^: BOOL
.IP
\fIelement-header\fP\^: ELEMENT_HEADER
.IP
\fIintercepted-clients\fP\^: LISTofCLIENT_INFO
.IP
Errors:
.PN RecordContext
.LP
This request queries the current state of the specified \fIcontext\fP
and is typically used by a recording client over the control connection.
The \fIenabled\fP field
specifies the state of data transfer between the extension and the
recording client, and is either enabled
.Pn ( True )
or disabled
.Pn ( False ).
The initial state is disabled.
When enabled, all core X protocol and
extension protocol received from (requests) or sent to (replies,
errors, events) a particular client, and requested to be intercepted
by the recording client, is reported to the recording client over the
data connection.
The \fIelement-header\fP specifies the header that precedes each
recorded protocol element.
The
\fIintercepted-clients\fP field specifies the list of clients currently
being recorded and the protocol associated with each client.
If future clients will be automatically registered with the context,
one of the returned CLIENT_INFO structures has a \fIclient-resource\fP value
of FutureClients and an \fIintercepted-protocol\fP giving the protocol to
intercept for future clients.
Protocol ranges may be decomposed, coalesced, or otherwise modified
by the server from how they were specified by the client.
All CLIENTSPECs registered with the server are returned, even if the
RECORDRANGE(s) associated with them specify no protocol to record.
.LP
When the \fIcontext\fP argument is not valid, a
.PN RecordContext
error results.

.LP
.PN "RecordEnableContext"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.LP
\(->\(pl
.br
.IP
\fIcategory\fP\^: {\fBFromServer\fP, \fBFromClient\fP, \fBClientStarted\fP, \fBClientDied\fP,
\fBStartOfData\fP,
\fBEndOfData\fP}
.IP
\fIelement-header\fP\^: ELEMENT_HEADER
.IP
\fIclient-swapped\fP\^: BOOL
.IP
\fIid-base\fP\^: XIDBASE
.IP
\fIserver-time\fP\^: TIMESTAMP
.IP
\fIrecorded-sequence-number\fP\^: CARD32
.IP
\fIdata\fP\^: LISTofBYTE
.br
.IP
Errors:
.PN Match ,
.PN RecordContext
.LP
This request enables data transfer between the recording client
and the extension and returns the protocol data the recording client
has previously expressed interest in.  Typically, this request is
executed by the recording client over the data connection.
.LP
If the client is registered on the \fIcontext\fP, it is unregistered
before any recording begins.
.LP
Once the server receives this request, it begins intercepting
and reporting to the recording client all core and extension protocol
received from or sent to clients registered with the RC that the
recording client has expressed interest in.  All intercepted protocol data
is returned in the byte-order of the recorded client.  Therefore,
recording clients are responsible for all byte swapping, if required.
More than one recording client cannot enable data transfer on the
same RC at the same time.  Multiple intercepted requests, replies,
events and errors might be packaged into a single reply before
being returned to the recording clients.
.LP
The
\fIcategory\fP field determines the possible
types of the data.
When a context is enabled, the server will immediately send a reply of
category
.PN StartOfData
to notify the client that recording is enabled.
A category of
.PN FromClient
means the data are from the client
(requests);
.PN FromServer
means data are from the server (replies,
errors, events, or device events).
For a new client, the category is
.PN ClientStarted
and the data are the connection setup reply.
When
the recorded client connection is closed, \fIcategory\fP is
set to the value
.PN ClientDied
and no protocol is included in this reply.
When the disable request is made over the control connection,
a final reply is sent over the data connection with category
.PN EndOfData
and no protocol.
.LP
The \fIelement-header\fP field returns the value currently set for the
context, which tells what header information precedes each recorded
protocol element in this reply.
.LP
The \fIclient-swapped\fP field is
.PN True
if the byte order of
the protocol being recorded
is swapped
relative to the recording client;
otherwise, \fIclient-swapped\fP is
.PN False .
The recorded protocol
is in the byte order of the client being
recorded; device events are in the byte order of the
recording client.
For replies of category
.PN StartOfData
and
.PN EndOfData
the
\fIclient-swapped\fP bit is set
according
to the byte order of the server relative to the recording client.
The \fIid-base\fP field is the resource identifier base
sent to the client from the server in the
connection setup reply, and hence, identifies the client being
recorded.  The \fIid-base\fP field is 0 (zero) when the protocol
data being
returned are device events.
The \fIserver-time\fP field is set to the time of the
server when the first protocol element in this reply was intercepted.
The \fIserver-time\fP
of reply N+1 is greater than or equal to the \fIserver-time\fP of reply N,
and is greater than or equal to the time of the last protocol
element in reply N.
.LP
The \fIrecorded-sequence-number\fP field is set to the sequence number
of the recorded client's most recent request processed by the server.
.LP
The \fIdata\fP field
contains the raw protocol data being returned to the recording client.
If requested by the \fIelement-header\fP of this record context, each
protocol element may be preceded by a 32-bit timestamp and/or
a 32-bit sequence number.
If present, both the timestamp and sequence number are always in the
byte order of the recording client.
.LP
For the core X events
.PN KeyPress ,
.PN KeyRelease ,
.PN ButtonPress ,
and
.PN ButtonRelease ,
the fields of a device event that contain
valid information are \fItime\fP and \fIdetail\fP.
For the core X event
.PN MotionNotify ,
the fields of a device event that contain
valid information are \fItime\fP, \fIroot\fP,
\fIroot-x\fP and \fIroot-y\fP.
The \fItime\fP field refers to the time the event was generated by the
device.
.LP
For the extension input device events
.PN DeviceKeyPress ,
.PN DeviceKeyRelease ,
.PN DeviceButtonPress ,
and
.PN DeviceButtonRelease ,
the fields of a device event that contain valid information are
\fIdevice\fP, \fItime\fP and \fIdetail\fP.
For
.PN DeviceMotionNotify ,
the valid device event fields are
\fIdevice\fP and \fItime\fP.
For the extension input device events
.PN ProximityIn
and
.PN ProximityOut ,
the fields of a device event that contain valid
information are \fIdevice\fP and \fItime\fP.
For the extension input device event
.PN DeviceValuator ,
the fields of a device event that contain valid information are
\fIdevice\fP,
\fInum_valuators\fP, \fIfirst_valuator\fP, and \fIvaluators\fP.
The \fItime\fP field refers to the time the event was generated by the
device.
.LP
The error
.PN "Match"
is returned when data transfer is already enabled.
When the \fIcontext\fP argument is not valid, a
.PN RecordContext
error results.

.LP
.PN "RecordDisableContext"
.TA .75i
.ta .75i
.IP
\fIcontext\fP\^: RC
.br
.IP
Errors:
.PN RecordContext
.LP
This request is typically executed by the recording client over the
control connection.  This request directs the extension to immediately
send any complete protocol elements currently buffered,
to send a final reply with category
.PN EndOfData ,
and to discontinue
data transfer between the extension and the recording client.
Protocol reporting is disabled
on the data connection that is currently enabled for the given
\fIcontext\fP.  Once the extension completes
processing this request, no additional recorded protocol will
be reported to the recording client.  If a data connection is not
currently enabled when this request is executed, then this request has
no affect on the state of data transfer.
An RC is disabled automatically when the connection to the enabling
client is closed down.
.LP
When the \fIcontext\fP argument is not valid, a
.PN RecordContext
error results.

.LP
.PN "RecordFreeContext"
.TA .75i
.ta .75i
.IP
\fIcontext \fP\^: RC
.br
.IP
Errors:
.PN RecordContext
.LP
This request deletes the association between the resource ID and the
RC and destroys the RC.
If a client has enabled data transfer on this \fIcontext\fP, the actions
described in
.PN RecordDisableContext
are performed before the \fIcontext\fP
is freed.
.LP
An RC is destroyed automatically when the connection to the creating client
is closed down and the close-down mode is \fBDestroyAll\fP.  When the
\fIcontext\fP argument is not valid, a
.PN RecordContext
error results.

.NH 1
Encoding
.XS
\*(SN Encoding
.XE
.LP
Please refer to the X11 Protocol Encoding document as this document uses
conventions established there.
.LP
The name of this extension is \*QRECORD\*U.
.LP
.NH 2
Types
.LP
RC: CARD32
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
RANGE8
	1	CARD8		first
	1	CARD8		last
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
RANGE16
	2	CARD16		first
	2	CARD16		last
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
EXTRANGE
	2	RANGE8		major
	4	RANGE16		minor
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
RECORDRANGE
	2	RANGE8		core-requests
	2	RANGE8		core-replies
	6	EXTRANGE		ext-requests
	6	EXTRANGE		ext-replies
	2	RANGE8		delivered-events
	2	RANGE8		device-events
	2	RANGE8		errors
	1	BOOL		client-started
	1	BOOL		client-died
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
ELEMENT_HEADER
	1	CARD8
		0x01	from-server-time
		0x02	from-client-time
		0x04	from-client-sequence
.DE
.LP
XIDBASE: CARD32
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
CLIENTSPEC
	4	XIDBASE		client-id-base
		1	CurrentClients
		2	FutureClients
		3	AllClients
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
CLIENT_INFO
	4	CLIENTSPEC		client-resource
	4	CARD32		n, number of record ranges in intercepted-protocol
	24n	LISTofRECORDRANGE		intercepted-protocol
.DE
.NH 2
Errors
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordContext
	1	0		Error
	1	CARD8		extension's base error code + 0
	2	CARD16		sequence number
	4	CARD32		invalid record context
	24			unused
.DE
.NH 2
Requests
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordQueryVersion
	1	CARD8		major opcode
	1	0		minor opcode
	2	2		request length
	2	CARD16		major version
	2	CARD16		minor version
 =>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	2	CARD16		major version
	2	CARD16		minor version
	20			unused
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordCreateContext
	1	CARD8		major opcode
	1	1		minor opcode
	2	5+m+6n		request length
	4	RC		context
	1	ELEMENT_HEADER	element-header
	3			unused
	4	CARD32		m, number of client-specifiers
	4	CARD32		n, number of ranges
	4m	LISTofCLIENTSPEC		client-specifiers
	24n	LISTofRECORDRANGE	ranges
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordRegisterClients
	1	CARD8		major opcode
	1	2		minor opcode
	2	5+m+6n		request length
	4	RC		context
	1	ELEMENT_HEADER	element-header
	3			unused
	4	CARD32		m, number of client-specifiers
	4	CARD32		n, number of ranges
	4m	LISTofCLIENTSPEC		client-specifiers
	24n	LISTofRECORDRANGE	ranges
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordUnregisterClients
	1	CARD8		major opcode
	1	3		minor opcode
	2	3+m		request length
	4	RC		context
	4	CARD32		m, number of client-specifiers
	4m	LISTofCLIENTSPEC		client-specifiers
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordGetContext
	1	CARD8		major opcode
	1	4		minor opcode
	2	2		request length
	4	RC		context
 =>
	1	1		Reply
	1	BOOL		enabled
	2	CARD16		sequence number
	4	j		reply length
	1	ELEMENT_HEADER	element-header
	3			unused
	4	CARD32		n, number of intercepted-clients
	16			unused
	4j	LISTofCLIENT_INFO		intercepted-clients
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordEnableContext
	1	CARD8		major opcode
	1	5		minor opcode
	2	2		request length
	4	RC		context
 =>+
	1	1		Reply
	1			category
		0	FromServer
		1	FromClient
		2	ClientStarted
		3	ClientDied
		4	StartOfData
		5	EndOfData
	2	CARD16		sequence number
	4	n		reply length
	1	ELEMENT_HEADER	element-header
	1	BOOL		client-swapped
	2			unused
	4	XIDBASE		id-base
	4	TIMESTAMP		server-time
	4	CARD32		recorded-sequence-number
	8			unused
	4n	BYTE		data
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordDisableContext
	1	CARD8		major opcode
	1	6		minor opcode
	2	2		request length
	4	RC		context
.DE
.LP
.DS 0
.TA .2i 1.0i 2.0i 3.0i
.ta .2i 1.0i 2.0i 3.0i
.R
.PN RecordFreeContext
	1	CARD8		major opcode
	1	7		minor opcode
	2	2		request length
	4	RC		context
.DE
