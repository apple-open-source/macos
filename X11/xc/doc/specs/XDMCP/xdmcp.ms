.\" Use eqn, tbl, and -ms
.\" $Xorg: xdmcp.ms,v 1.3 2000/08/17 19:42:20 cpqbld Exp $
.\" $XFree86: xc/doc/specs/XDMCP/xdmcp.ms,v 1.3 2003/11/22 04:50:59 dawes Exp $
.EQ
delim @@
define oc % "\\fR{\\fP" %
define cc % "\\fR}\\fP" %
.EN
.de PT
..
.de BT
..
.ps 11
.nr PS 11
\&
.sp 8
.ce 7
\s+2\fBX Display Manager Control Protocol\fP\s-2

\s+1\fBVersion 1.1 DRAFT

X.Org Standard

X Version 11, Release 6.7\fP\s-1
.sp 4
.ce 5
\s-1Keith Packard

X Consortium
Laboratory for Computer Science
Massachusetts Institute of Technology\s+1
.bp
.br
\&
.sp 15
Copyright \(co 1989, 2002 The Open Group
.sp 3
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
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
.LP
.sp 3
\fIX Window System\fP is a trademark of The Open Group.
.de PT
.ie o .tl 'XDMCP''X Display Manager Control Protocol '
.el .tl 'X Display Manager Control Protocol ''XDMCP'
..
.bp 1
.de BT
.tl ''\fB % \fP''
..
.NH 1
Purpose and Goals
.XS
\*(SN Purpose and Goals
.XE
.LP
The purpose of the X Display Manager Control Protocol (XDMCP)
is to provide a uniform mechanism for an autonomous
display to request login service from a remote host.
By autonomous, we mean
the display consists of hardware and processes that are independent of any
particular host where login service is desired.  (For example, the server
cannot simply be started by a 
.PN fork/exec
sequence on the host.)
An X terminal (screen, keyboard, mouse, processor, network interface)
is a prime example of an autonomous display.
.LP
From the point of view of the end user, it is very important to make
autonomous displays as easy to use as traditional hardwired character
terminals.  Specifically, you can typically just power on a hardwired
terminal and be greeted with a login prompt.  The same should be possible
with autonomous displays.  However, in a network environment with multiple
hosts, the end user may want to choose which host(s) to connect to.  In an
environment with many displays and many hosts, a site administrator may want
to associate particular collections of hosts with particular displays.  We
would like to support the following options:
.IP \(bu 5
The display has a single, fixed host to which it should connect.  It should be
possible to power on the display and receive a login prompt, without user
intervention.
.IP \(bu 5
Any one of several hosts on a network or subnetwork may be acceptable
for accepting login from the display.
(For example, the user's file systems can be mounted onto
any such host, providing comparable environments.)  It should be possible
for the display to broadcast to find such hosts and to have the display
either automatically choose a host or present the possible hosts to the
user for selection.
.IP \(bu 5
The display has a fixed set of hosts that it can connect to.  It should be
possible for the display to have that set stored in RAM, but it should also be
possible for a site administrator to be able to maintain host sets for a
large number of displays using a centralized facility, without having to
interact (physically or electronically) with each individual display.
Particular hosts should be allowed to refuse login service, based on
whatever local criteria are desired.
.LP
The control protocol should be designed in such a way that it can be used over
a reasonable variety of communication transport layers.  In fact, it is quite
desirable if every major network protocol family that supports the standard X
protocol is also capable of supporting XDMCP, because the end result of XDMCP
negotiation will be standard X protocol connections to the display.
However, because the number of displays per host may be large,
a connection-based protocol appears less desirable 
than a connection-less protocol.  For this reason the protocol is designed
to use datagram services with the display responsible for sequencing and
retransmission.
.LP
To keep the burden on displays at a minimum (because display cost is not
a factor that can be ignored), it is desirable that displays not be required
to maintain permanent state (across power cycles) for the purposes 
of the control protocol,
and it is desirable to keep required state at a minimum while the
display is powered on.
.LP
Security is an important consideration and must be an integral part of the
design.  The important security goals in the context of XDMCP are:
.IP \(bu 5
It should be possible for the display to verify that it is communicating
with a legitimate host login service.  Because the user will present
credentials (for example, password) to this service, 
it is important to avoid spoof attacks.
.IP \(bu 5
It should be possible for the display and the login service to negotiate the
authorization mechanism to be used for the standard X protocol.
.IP \(bu 5
It should be possible to provide the same level of security in verifying the
login service as is provided by the negotiated authorization mechanism.
.IP \(bu 5
Because there are no firm standards yet in the area of security,
XDMCP must be flexible enough to accomodate a variety of security mechanisms.
.NH 1
Overview of the Protocol
.XS
\*(SN Overview of the Protocol
.XE
.LP
XDMCP is designed to provide authenticated access to display management
services for remote displays.  A new network server, called a \fIDisplay
Manager\fP, will use XDMCP to communicate with displays to negotiate the
startup of X sessions.  The protocol allows the display to authenticate the
manager.  It also allows most of the configuration information to be
centralized with the manager and to ease the burden of system administration
in a large network of displays.
The essential goal is to provide plug-and-play
services similar to those provided in the familiar mainframe/terminal world.
.LP
Displays may be turned off by the user at any time.  Any existing session
running on a display that has been turned off must be identifiable.  This
is made possible by requiring a three-way handshake to start a session.  If
the handshake succeeds, any existing session is terminated immediately and a
new session started.  There is the problem (at least with TCP) that
connections may not be closed when the display is turned off.  In most
environments, the manager should reduce this problem by periodically XSync'ing
on its own connection, perhaps every five to ten minutes, and terminating the
session if its own connection ever closes.
.LP
Displays should not be required to retain permanent state for purposes of
the control protocol.  One solution to packets received out of sequence
would be to use monotonically increasing message identifiers in each message
to allow both sides to ignore messages that arrive out-of-sequence.  For
this to work, displays would at a minimum have to increment a stable crash
count each time they are powered on and use that number as part of a
larger sequence number.  But if displays cannot retain permanent state this
cannot work.  Instead, the manager assumes the responsibility for permanent
state by generating unique numbers that identify a particular session and
the protocol simply ignores packets that correspond to an invalid session.
.LP
The Manager must not be responsible for packet reception.  To prevent the
Manager from becoming stuck because of a hostile display, no portion of the
protocol requires the Manager to retransmit a packet.  Part of this means
that any valid packet that the Manager does receive must be
acknowledged in some way to prevent the display from continuously resending
packets.  The display can keep the protocol running as it will always know
when the Manager has received (at least one copy of) a packet.  On the
Manager side, this means that any packet may be received more than once (if
the response was lost) and duplicates must be ignored.
.NH 1
Data Types
.XS
\*(SN Data Types
.XE
.LP
XDMCP packets contain several types of data.  Integer values are always
stored most significant byte first in the packet (``Big Endian'' order).
As XDMCP will not be used to transport large quantities of data, this
restriction will not substantially hamper the efficiency of any
implementation.  Also, no padding of any sort will occur within the packets.
.TS H
lw(1.25i) lw(.75i) lw(3.5i).
_
.sp 6p
.B
Type Name	Length (Bytes)	Description
.sp 6p
_
.sp 6p
.R
.TH
CARD8	1	A single byte unsigned integer
CARD16	2	Two byte unsigned integer
CARD32	4	Four byte unsigned integer
ARRAY8	n+2	This is actually a CARD16 followed by
		a collection of CARD8.  The value of the CARD16
		field (n) specifies the number of CARD8 values to
		follow
ARRAY16	2*m+1	This is a CARD8 (m) which specifies the
		number of CARD16 values to follow
ARRAY32	4*l+1	This is a CARD8 (l) which specifies the
		number of CARD32 values to follow
ARRAYofARRAY8	?	This is a CARD8 which specifies the
		number of ARRAY8 values to follow.
.sp 6p
_
.TE
.NH 1
Packet Format
.XS
\*(SN Packet Format
.XE
.LP
All XDMCP packets have the following information:
.TS
lw(1.25i) lw(.75i) lw(3.5i).
_
.sp 6p
.B
Length (Bytes)	Field Type	Description
.sp 6p
_
.R
2	CARD16	version number
2	CARD16	opcode packet header
2	CARD16	n = length of remaining data in bytes

n	???	packet-specific data
.sp 6p
_
.TE
.LP
The fields are as follows:
.IP \(bu 5
Version number
.IP
This specifies the version of XDMCP that generated this packet in
case changes in this protocol are required.  Displays and
managers may choose to support older versions for compatibility.
This field will initially be one (1).
.IP \(bu 5
Opcode
.IP
This specifies what step of the protocol this packet represents and should
contain one of the following values (encoding provided in section below):
.PN BroadcastQuery ,
.PN Query ,
.PN IndirectQuery ,
.PN ForwardQuery ,
.PN Willing ,
.PN Unwilling ,
.PN Request ,
.PN Accept ,
.PN Decline ,
.PN Manage ,
.PN Refuse ,
.PN Failed ,
.PN KeepAlive ,
or
.PN Alive .
.IP \(bu 5
Length of data in bytes
.IP
This specifies the length of the information following the first 6 bytes.
Each packet-type has a different format and will need to be separately
length-checked against this value.  Because every data item has either an
explicit or implicit length, this can be easily accomplished.
Packets that have too little or too much data should be ignored.
.LP	
Packets should be checked to make sure that they satisfy the following
conditions:
.IP 1. 5
They must contain valid opcodes.
.IP 2. 5
The length of the remaining data should correspond to the sum of the 
lengths of the individual remaining data items.
.IP 3. 5
The opcode should be expected (a finite state diagram is given
in a later section).
.IP 4. 5
If the packet is of type
.PN Manage
or
.PN Refuse ,
the Session ID should match the value sent in the preceding
.PN Accept
packet.
.NH 1
Protocol
.XS
\*(SN Protocol
.XE
.LP
Each of the opcodes is described below.  Because a given packet type is only
ever sent one way, each packet description below indicates the direction.
Most of the packets have additional information included beyond the
description above.  The additional information is appended to the packet
header in the order described without padding, and the length field is
computed accordingly.
.LP
.PN Query
.br
.PN BroadcastQuery
.br
.PN IndirectQuery
.RS
Display \(-> Manager
.br
Additional Fields:
.RS
\fIAuthentication Names\fP: ARRAYofARRAY8
.RS
Specifies a list of authentication names that the display supports.
The manager will choose one of these and return it in the
.PN Willing
packet.
.RE
.RE
Semantics:
.RS
A
.PN Query
packet is sent from the display to a specific host to ask if
that host is willing to provide management services to this display.  The
host should respond with
.PN Willing
if it is willing to service the display or
.PN Unwilling
if it is not.
.LP
A 
.PN BroadcastQuery
packet is similar to the
.PN Query
packet except that it is intended to be received by all hosts on the network
(or subnetwork).  However, unlike 
.PN Query
requests, hosts that are not willing to service the display 
should simply ignore
.PN BroadcastQuery
requests.
.LP
An
.PN IndirectQuery
packet is sent to a well known manager that forwards
the request to a larger collection of secondary managers using
.PN ForwardQuery
packets.
In this way, the collection of managers that respond can be grouped
on other than network boundaries; the use of a central manager reduces
system administrative overhead.
The primary manager may also send a
.PN Willing
packet in response to this packet.
.LP
Each packet type has slightly different semantics:
.IP \(bu 5
The 
.PN Query
packet is destined only for a single host.
If the display is instructed to
.PN Query
multiple managers, it will send multiple
.PN Query
packets.  The
.PN Query
packet also demands a response from the manager, either
.PN Willing
or
.PN Unwilling .
.IP \(bu 5
The
.PN BroadcastQuery
packet is sent to many hosts.
Each manager that receives this packet will not respond with an
.PN Unwilling
packet.
.IP \(bu 5
The
.PN IndirectQuery
packet is sent to only one manager with the request
that the request be forwarded to a larger list of managers using
.PN ForwardQuery
packets.  This list is expected to be maintained at one
central site to reduce administrative overhead.
The function of this packet type is similar to
.PN BroadcastQuery except that
.PN BroadcastQuery
is not forwarded.
.RE
Valid Responses:
.RS
.PN Willing ,
.PN Unwilling
.RE
Problems/Solutions:
.RS
Problem:
.RS
Not all managers receive the query packet.
.RE
.RS
Indication:
.RS
None if
.PN BroadcastQuery
or 
.PN IndirectQuery
was sent, else failure to receive
.PN Willing .
.RE
Solution:
.RS
Repeatedly send the packet while waiting for user to choose a manager.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
An exponential backoff algorithm should be used here to reduce network load
for long-standing idle displays.  Start at 2 seconds, back off by factors of
2 to 32 seconds, and discontinue retransmit after 126 seconds.  The display
should reset the timeout when user-input is detected.  In this way, the
display will wakeup when touched by the user.
.RE
.RE
.LP
.PN ForwardQuery
.RS
Primary Manager \(-> Secondary Manager
.br
Additional Fields:
.RS
\fIClient Address\fP: ARRAY8
.RS
Specifies the network address of the client display.
.RE
\fIClient Port\fP: ARRAY8
.RS
Specifies an identification of the client task on the client display.
.RE
\fIAuthentication Names\fP: ARRAYofARRAY8
.RS
Is a duplicate of Authentication Names array that was received
in the
.PN IndirectQuery
packet.
.RE
.RE
Semantics:
.RS
When primary manager receives a
.PN IndirectQuery
packet, it is responsible for sending
.PN ForwardQuery
packets to an appropriate list of
managers that can provide service to the display using the same network
type as the one the original
.PN IndirectQuery
packet was received from.
The Client Address and Client Port fields must contain an
address that the secondary manager can use to reach the display also using
this same network.  Each secondary manager sends a
.PN Willing
packet to the display if it is willing to provide service.
.LP
.PN ForwardQuery
packets are similar to
.PN BroadcastQuery
packets in that managers that are not willing to service 
particular displays should not send a
.PN Unwilling
packet.
.RE
Valid Responses:
.RS
.PN Willing
.RE
Problems/Solutions:
.RS
Identical to
.PN BroadcastQuery
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from a manager, this packet should never be
retransmitted.
.RE
.RE
.LP
.PN Willing
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fIAuthentication Name\fP: ARRAY8
.RS
Specifies the authentication method, selected from the list offered in the
.PN Query ,
.PN BroadcastQuery ,
or
.PN IndirectQuery
packet that the manger expects the display to use in the subsequent
.PN Request
packet.
This choice should remain as constant as feasible so that displays that
send multiple
.PN Query
packets can use the Authentication Name from any
.PN Willing
packet that arrives.
.LP
The display is free to ignore managers that request an insufficient level
of authentication.
.RE
\fIHostname\fP: ARRAY8
.RS
Is a human readable string describing the host from which the packet was sent.
The protocol specifies no interpretation of the data in this field.
.RE
\fIStatus\fP: ARRAY8
.RS
Is a human readable string describing the status of the host.  This could
include load average/number of users connected or other information.  The
protocol specifies no interpretation of the data in this field.
.RE
.RE
Semantics:
.RS
A
.PN Willing
packet is sent by managers that may service connections from
this display.  It is sent in response to either a
.PN Query ,
.PN BroadcastQuery ,
or
.PN ForwardQuery
but does not imply a commitment to provide service 
(for example, it may later decide that it has accepted enough
connections already).
.RE
Problems/Solutions:
.RS
Problem:
.RS
.PN Willing
not received by the display.
.br
Indication:
.RS
None if
.PN BroadcastQuery
or
.PN IndirectQuery
was sent, else failure to receive
.PN Willing .
.RE
Solution:
.RS
The display should continue to send the query until a response is received.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from the manager to the display, this packet should
never be retransmitted.
.RE
.RE
.LP
.PN Unwilling
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
The Hostname and Status fields as in the
.PN Willing
packet.
The Status field should indicate to the user a reason 
for the refusal of service.
.RE
Semantics:
.RS
An
.PN Unwilling
packet is sent by managers in response to direct
.PN Query
requests (as opposed to
.PN BroadcastQuery
or
.PN IndirectQuery
requests) if the manager will not accept requests for management.
This is typically sent by managers that wish to only service
particular displays or that handle a limited number of displays at once.
.RE
Problems/Solutions:
.RS
Problem:
.RS
.PN Unwilling
not received by the display.
.br
Indication:
.RS
Display fails to receive
.PN Unwilling .
.RE
Solution:
.RS
The display should continue to send
.PN Query
messages until a response is received.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from the manager to the display, this packet should
never be retransmitted.
.RE
.RE
.LP
.PN Request
.br
.RS
Display \(-> Manager
.br
Additional Fields:
.RS
\fIDisplay Number\fP: CARD16
.RS
Specifies the index of this particular server for the host
on which the display is resident.
This value will be zero for most autonomous displays.
.RE
\fIConnection Types\fP: ARRAY16
.RS
Specifies an array indicating the stream services accepted by the display.
If the high-order byte in a particular entry is zero, the low-order byte
corresponds to an X-protocol host family type.
.RE
\fIConnection Addresses\fP: ARRAYofARRAY8
.RS
For each connection type in the previous array, the corresponding entry in
this array indicates the network address of the display device.
.RE
\fIAuthentication Name\fP: ARRAY8
.br
\fIAuthentication Data\fP: ARRAY8
.RS
Specifies the authentication protocol that the display expects
the manager to validate itself with.  The Authentication Data is
expected to contain data that the manager will interpret, modify
and use to authenticate itself.
.RE
\fIAuthorization Names\fP: ARRAYofARRAY8
.RS
Specifies which types of authorization the display supports.  The
manager may decide to reject displays with which it cannot perform
authorization.
.RE
\fIManufacturer Display ID\fP: ARRAY8
.RS
Can be used by the manager to determine how to decrypt the
Authentication Data field in this packet.  See the section below on
Manufacturer Display ID Format.
.RE
.RE
Semantics:
.RS
A
.PN Request
packet is sent by a display to a specific host to request a
session ID in preparation for a establishing a connection.  If the manager
is willing to service a connection to this display, it should return an
.PN Accept
packet with a valid session ID and should be ready for a subsequent
.PN Manage
request.  Otherwise, it should return a
.PN Decline
packet.
.RE
Valid Responses:
.RS
.PN Accept ,
.PN Decline
.RE
Problems/Solutions:
.RS
Problem:
.RS
Request not received by manager.
.br
Indication:
.RS
Display timeout waiting for response.
.RE
Solution:
.RS
Display resends
.PN Request
message.
.RE
.RE
Problem:
.RS
Message received out of order by manager.
.br
Indication:
.RS
None.
.RE
Solution:
.RS
Each time a
.PN Request
is sent, the manager sends the Session ID
associated with the next session in the
.PN Accept .
If that next session is not yet started,
the manager will simply resend with the same Session ID.
If the session is in progress, the manager will reply 
with a new Session ID; in which case, the
.PN Accept
will be discarded by the display.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Timeout after 2 seconds, exponential backoff to 32 seconds.
After no more than 126 seconds, give up and report an error to the user.
.RE
.RE
.LP
.PN Accept
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fISession ID\fP: CARD32
.RS
Identifies the session that can be started by the manager.
.RE
\fIAuthentication Name\fP: ARRAY8
.br
\fIAuthentication Data\fP: ARRAY8
.RS
Is the data sent back to the display to authenticate the manager.
If the Authentication Data is not the value expected by the display, it
should terminate the protocol at this point and display an error to the user.
.RE
\fIAuthorization Name\fP: ARRAY8
.br
\fIAuthorization Data\fP: ARRAY8
.RS
Is the data sent to the display to indicate the type of authorization the
manager will be using in the first call to
.PN XOpenDisplay
after the
.PN Manage
packet is received.
.RE
.RE
Semantics:
.RS
An 
.PN Accept
packet is sent by a manager in response to a
.PN Request
packet if the manager is willing to establish a connection for the display.
The Session ID is used to identify this connection from any preceding
ones and will be used by the display in its subsequent
.PN Manage
packet.
The Session ID is a 32-bit number that is incremented each time an
.PN Accept
packet is sent as it must be unique over a reasonably long period of time.
.LP
If the authentication information is invalid, a
.PN Decline
packet will be returned with an appropriate 
.PN Status
message.
.RE
Problems/Solutions:
.RS
Problem:
.RS
.PN Accept
or
.PN Decline
not received by display.
.br
Indication:
.RS
Display timeout waiting for response to
.PN Request .
.RE
Solution:
.RS
Display resends
.PN Request
message.
.RE
.RE
Problem:
.RS
Message received out of order by display.
.br
Indication:
.RS
Display receives
.PN Accept
after
.PN Manage
has been sent.
.RE
Solution:
.RS
Display discards
.PN Accept
messages after it has sent a
.PN Manage
message.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from the manager to the display, this packet should
never be retransmitted.
.RE
.RE
.LP
.PN Decline
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fIStatus\fP: ARRAY8
.RS
Is a human readable string indicating the reason for refusal of
service.
.RE
\fIAuthentication Name\fP:
ARRAY8
.br
\fIAuthentication Data\fP:
ARRAY8
.RS
Is the data sent back to the display to authenticate the manager.  If the
Authentication Data is not the value expected by the display, it
should terminate the protocol at this point and display an error to the user.
.RE
.RE
Semantics:
.RS
A
.PN Decline
packet is sent by a manager in response to a
.PN Request
packet if the manager is unwilling to establish a connection for the
display.
This is allowed even if the manager had responded
.PN Willing
to a previous query.
.RE
Problems/Solutions:
.RS
Same as for
.PN Accept .
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from a manager to a display, this packet should never
be retransmitted.
.RE
.RE
.LP
.PN Manage
.RS
Display \(-> Manager
.br
Additional Fields:
.RS
\fISession ID\fP: CARD32
.RS
Should contain the nonzero session ID returned in the
.PN Accept
packet.
.RE
\fIDisplay Number\fP: CARD16
.RS
Must match the value sent in the previous
.PN Request
packet.
.RE
\fIDisplay Class\fP: ARRAY8
.RS
Specifies the class of the display.  
See the Display Class Format section, 
which discusses the format of this field.
.RE
.RE
Semantics:
.RS
A
.PN Manage
packet is sent by a display to ask the manager to begin a
session on the display.  If the Session ID is correct the manager
should open a connection; otherwise, it should respond with a
.PN Refuse
or
.PN Failed
packet, unless the Session ID matches a currently
running session or a session that has not yet successfully opened the
display but has not given up the attempt.  In this latter case, the
.PN Manage
packet should be ignored.
This will work as stream connections give positive success indication
to both halves of the stream, and positive failure indication 
to the connection initiator (which will eventually generate a
.PN Failed
packet).
.RE
Valid Responses:
.RS
X connection with correct auth info,
.PN Refuse ,
.PN Failed .
.RE
Problems/Solutions:
.RS
Problem:
.RS
.PN Manage
not received by manager.
.br
Indication:
.RS
Display timeout waiting for response.
.RE
Solution:
.RS
Display resends
.PN Manage
message.
.RE
.RE
Problem:
.RS
.PN Manage
received out of order by manager.
.br
Indication:
.RS
Session already in progress with matching Session ID.
.RE
Solution:
.RS
.PN Manage
packet ignored.
.RE
Indication:
.RS
Session ID does not match next Session ID.
.RE
Solution:
.RS
.PN Refuse
message is sent.
.RE
.RE
Problem:
.RS
Display cannot be opened on selected stream.
.br
Indication:
.RS
Display connection setup fails.
.RE
Solution:
.RS
.PN Failed
message is sent including a human readable reason.
.RE
.RE
Problem:
.RS
Display open does not succeed before a second manage packet is received
because of a timeout occuring in the display.
.br
Indication:
.RS
.PN Manage
packet received with Session ID matching the session
attempting to connect to the display.
.RE
Solution:
.RS
.PN Manage
packet is ignored.  As the stream connection will either
succeed, which will result in an active session, or the stream will
eventually give up hope of connecting and send a 
.PN Failed
packet; no response to this
.PN Manage
packet is necessary.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Timeout after 2 seconds, exponential backoff to 32 seconds.  After no more
than 126 seconds, give up and report an error to the user.
.RE
.RE
.LP
.PN Refuse
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fISession ID\fP: CARD32
.RS
Should be set to the Session ID received in the
.PN Manage
packet.
.RE
.RE
Semantics:
.RS
A
.PN Refuse
packet is sent by a manager when the Session ID received in the
.PN Manage
packet does not match the current Session ID.
The display should assume that it received an old
.PN Accept
packet and should resend its
.PN Request
packet.
.RE
Problems/Solutions:
.RS
Problem:
.RS
Error message is lost.
.br
Indication:
.RS
Display times out waiting for
new connection,
.PN Refuse
or
.PN Failed .
.RE
Solution:
.RS
Display resends
.PN Manage
message.
.RE
.RE
.RE
Timeout/Retransmission policy:
.RS
Like all packets sent from a manager to a display, this packet should never be
retransmitted.
.RE
.RE
.LP
.PN Failed
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fISession ID\fP: CARD32
.RS
Should be set to the Session ID received in the
.PN Manage
packet.
.RE
\fIStatus\fP: ARRAY8
.RS
Is a human readable string indicating the reason for failure.
.RE
.RE
Semantics:
.RS
A 
.PN Failed
packet is sent by a manager when it has problems establishing
the initial X connection in response to the
.PN Manage
packet.
.RE
Problems/Solutions
.RS
Same as for
.PN Refuse .
.RE
.RE
.LP
.PN KeepAlive
.RS
Display \(-> Manager
.br
Additional Fields:
.RS
\fIDisplay Number\fP: CARD16
.RS
Set to the display index for the display host.
.RE
.RE
.RS
\fISession ID\fP: CARD32
.RS
Should be set to the Session ID received in the
.PN Manage
packet during the negotiation for the current session.
.RE
.RE
Sematics:
.RS
A
.PN KeepAlive
packet can be sent at any time during the session by a
display to discover if the manager is running.
The manager should respond with
.PN Alive
whenever it receives this type of packet.
.LP
This allows the display to discover when the manager host
is no longer running.
A display is not required to send
.PN KeepAlive
packets and, upon lack of receipt of
.PN Alive
packets, is not required to perform any specific action.
.LP
The expected use of this packet is to terminate an active session when the
manager host or network link fails.  The display should keep track of the
time since any packet has been received from the manager host and use
.PN KeepAlive
packets when a substantial time has elapsed since the
most recent packet.
.RE
Valid Responses:
.RS
.PN Alive
.RE
Problems/Solutions:
.RS
Problem:
.RS
Manager does not receive the packet or display does not receive the response.
.RE
.RS
Indication:
.RS
No
.PN Alive
packet is returned.
.RE
Solution:
.RS
Retransmit the packet with an exponential backoff; start at 2 seconds and
assume the host is not up after no less than 30 seconds.
.RE
.RE
.RE
.RE
.LP
.PN Alive
.RS
Manager \(-> Display
.br
Additional Fields:
.RS
\fISession Running\fP: CARD8
.RS
Indicates that the session identified by Session ID is
currently active.  The value is zero if no session is active 
or one if a session
is active.
.RE
\fISession ID\fP: CARD32
.RS
Specifies the ID of the currently running session; if any.
When no session is active this field should be zero.
.RE
.RE
Semantics:
.RS
An
.PN Alive
packet is sent in response to a
.PN KeepAlive
request. 
If a session is currently active on the display, the manager includes the
Session ID in the packet.  The display can use this information to
determine the status of the manager.
.RE
.RE
.NH 1
Session Termination
.XS
\*(SN Session Termination
.XE
.LP
When the session is over, the initial connection with the display (the one
that acknowledges the
.PN Manage
packet) will be closed by the manager.
If only a single session was active on the display,
all other connections should be closed by the display
and the display should be reset.  If multiple sessions
are active simultaneously and the display can identify which connections
belong to the terminated sesssion, those connections should be closed.
Otherwise, all connections should be closed and the display reset only when
all sessions have been terminated (that is, all initial connections closed).
.LP
The session may also be terminated at any time by the display if the
managing host no longer responds to
.PN KeepAlive
packets. 
The exact time-outs for sending
.PN KeepAlive
packets is not specified in this protocol as the trade off 
should not be fixed between loading an otherwise idle system with spurious
.PN KeepAlive
packets and not noticing that the manager host is down for a long time.
.NH 1
State Diagrams
.XS
\*(SN State Diagrams
.XE
.LP
The following state diagrams are designed to cover all actions of both
the display and the manager.  Any packet that is received out-of-sequence
will be ignored.
.LP
Display:
.RS
.sp 
.LP
\fIstart\fP:
.RS
User-requested connect to one host \(-> \fIquery\fP
.br
User-requested connect to some host \(-> \fIbroadcast\fP
.br
User-requested connect to site host-list \(-> \fIindirect\fP
.RE
.sp 
.LP
\fIquery\fP:
.RS
Send
.PN Query
packet
.br
\(-> \fIcollect-query\fP
.RE
.sp 
.LP
\fIcollect-query\fP:
.RS
Receive 
.PN Willing
\(-> \fIstart-connection\fP
.br
Receive
.PN Unwilling
\(-> \fIstop-connection\fP
.br
Timeout \(-> \fIquery\fP
.RE	
.sp 
.LP
\fIbroadcast\fP:
.RS
Send
.PN BroadcastQuery
packet
.br
\(-> \fIcollect-broadcast-query\fP
.RE
.sp 
.LP
\fIcollect-broadcast-query\fP:
.RS
Receive
.PN Willing
\(-> \fIupdate-broadcast-willing\fP
.br
User-requested connect to one host \(-> \fIstart-connection\fP
.br
Timeout \(-> \fIbroadcast\fP
.RE
.sp 
.LP
\fIupdate-broadcast-willing\fP:
.RS
Add new host to the host list presented to the user
.br
\(-> \fIcollect-broadcast-query\fP
.RE
.sp 
.LP
\fIindirect\fP:
.RS
Send
.PN IndirectQuery
packet
.br
\(-> \fIcollect-indirect-query\fP
.RE
.sp 
.LP
\fIcollect-indirect-query\fP:
.RS
Receive
.PN Willing
\(-> \fIupdate-indirect-willing\fP
.br
User-requested connect to one host \(-> \fIstart-connection\fP
.br
Timeout \(-> \fIindirect\fP
.RE
.sp 
.LP
\fIupdate-indirect-willing\fP:
.RS
Add new host to the host list presented to the user
.br
\(-> \fIcollect-indirect-query\fP
.RE
.sp 
.LP
\fIstart-connection\fP:
.RS
Send
.PN Request
packet
.br
\(-> \fIawait-request-response\fP
.RE
.sp 
.LP
\fIawait-request-response\fP:
.RS
Receive
.PN Accept
\(-> \fImanage\fP
.br
Receive
.PN Decline
\(-> \fIstop-connection\fP
.br
Timeout \(-> \fIstart-connection\fP
.RE
.sp 
.LP
\fImanage\fP:
.RS
Save Session ID
.br
Send
.PN Manage
packet with Session ID
.br
\(-> \fIawait-manage-response\fP
.RE	
.sp 
.LP
\fIawait-manage-response\fP:
.RS
Receive
.PN XOpenDisplay :
\(-> \fIrun-session\fP
.br
Receive
.PN Refuse
with matching Session ID \(-> \fIstart-connection\fP
.br
Receive
.PN Failed
with matching Session ID \(-> \fIstop-connection\fP
.br
Timeout \(-> \fImanage\fP
.RE
.sp 
.LP
\fIstop-connection\fP:
.RS
Display cause of termination to user
.br
\(-> \fIstart\fP
.RE	
.sp 
.LP
\fIrun-session\fP:
.RS
Decide to send
.PN KeepAlive
packet \(-> \fIkeep-alive\fP
.br
Await close of first display connection
.br
\(-> \fIreset-display\fP
.RE	
.sp 
.LP
\fIkeep-alive\fP:
.RS
Send
.PN KeepAlive
packet with current Session ID
.br
\(-> \fIawait-alive\fP
.RE
.sp
.LP
\fIawait-alive\fP:
.RS
Receive
.PN Alive
with matching Session ID \(-> \fIrun-session\fP
.br
Receive
.PN Alive
with nonmatching Session ID 
or FALSE Session Running \(-> \fIreset-display\fP
.br
Final timeout without receiving
.PN Alive
packet \(-> \fIreset-display\fP
.br
Timeout \(-> \fIkeep-alive\fP
.RE
.sp
.LP
\fIreset-display\fP:
.RS
(if possible) \(-> close all display connections associated with this session
.br
Last session \(-> close all display connections
.br
\(-> \fIstart\fP
.RE
.RE
.LP
Manager:
.RS
.sp
.LP
\fIidle\fP:
.RS
Receive
.PN Query
\(-> \fIquery-respond\fP
.br
Receive
.PN BroadcastQuery
\(-> \fIbroadcast-respond\fP
.br
Receive
.PN IndirectQuery
\(-> \fIindirect-respond\fP
.br
Receive
.PN ForwardQuery
\(-> \fIforward-respond\fP
.br
Receive
.PN Request
\(-> \fIrequest-respond\fP
.br
Receive
.PN Manage
\(-> \fImanage\fP
.br
An active session terminates \(-> \fIfinish-session\fP
.br
Receive 
.PN KeepAlive
\(-> \fIsend-alive\fP
.br
\(-> \fIidle\fP
.RE	
.sp
.LP
\fIquery-respond\fP:
.RS
If willing to manage \(-> \fIsend-willing\fP
.br
\(-> \fIsend-unwilling\fP
.RE
.sp
.LP
\fIbroadcast-respond\fP:
.RS
If willing to manage \(-> \fIsend-willing\fP
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIindirect-respond\fP:
.RS
Send
.PN ForwardQuery
packets to all managers on redirect list
.br
If willing to manage \(-> \fIsend-willing\fP
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIforward-respond\fP:
.RS
Decode destination address, if willing to manage \(-> \fIsend-willing\fP
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIsend-willing\fP:
.RS
Send
.PN Willing
packet
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIsend-unwilling\fP:
.RS
Send
.PN Unwilling
packet
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIrequest-respond\fP:
.RS
If manager is willing to allow a session on display \(-> \fIaccept-session\fP
.br
\(-> \fIdecline-session\fP
.RE
.sp
.LP
\fIaccept-session\fP:
.RS
Generate Session ID and save Session ID, display address, and
display number somewhere
.br
Send
.PN Accept
packet
.br
\(-> \fIidle\fP
.RE	
.sp
.LP
\fIdecline-session\fP:
.RS
Send
.PN Decline
packet
.br
\(-> \fIidle\fP
.RE	
.sp
.LP
\fImanage\fP:
.RS
If Session ID matches saved Session ID \(-> \fIrun-session\fP
.br
If Session ID matches Session ID of session in process of
starting up, or currently active session \(-> \fIidle\fP
.br
\(-> \fIrefuse\fP
.RE	
.sp
.LP
\fIrefuse\fP:
.RS
Send
.PN Refuse
packet
.br
\(-> 
\fIidle\fP
.RE	
.sp
.LP
\fIrun-session\fP:
.RS
Terminate any session in progress
.br
.PN XOpenDisplay
.br
Open display succeeds \(-> 
\fIstart-session\fP
.br
\(-> 
\fIfailed\fP
.RE
.sp
.LP
\fIfailed\fP:
.RS
Send
.PN Failed
packet
.br
\(-> \fIidle\fP
.RE	
.sp
.LP
\fIstart-session\fP:
.RS
Start a new session
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIfinish-session\fP:
.RS
.PN XCloseDisplay
.br
\(-> \fIidle\fP
.RE
.sp
.LP
\fIsend-alive\fP:
.RS
Send
.PN Alive
packet containing current status
.br
\(-> \fIidle\fP
.RE
.RE
.NH 1
Protocol Encoding
.XS
\*(SN Protocol Encoding
.XE
.LP
When XDMCP is implemented on top of the Internet User Datagram Protocol (UDP),
port number 177 is to be used. When using UDP over IPv4, Broadcast Query 
packets are sent via UDP broadcast.  When using UDP over IPv6, Broadcast Query
packets are sent via multicast, either to an address in the IANA registered 
XDMCP multicast address range of FF0\fIX\fP:0:0:0:0:0:0:12B 
(where the \fIX\fP is replaced by a valid scope id) or to a locally assigned 
multicast address. The version number in all packets will be 1.
Packet opcodes are 16-bit integers.
.RS
.TS
l l.
_
.sp 6p
.B
Packet Name	Encoding
.sp 6p
_
.sp 6p
.R
T{
.PN BroadcastQuery
T}	T{
1
T}
T{
.PN Query
T}	T{
2
T}
T{
.PN IndirectQuery
T}	T{
3
T}
T{
.PN ForwardQuery
T}	T{
4
T}
T{
.PN Willing
T}	T{
5
T}
T{
.PN Unwilling
T}	T{
6
T}
T{
.PN Request
T}	T{
7
T}
T{
.PN Accept
T}	T{
8
T}
T{
.PN Decline
T}	T{
9
T}
T{
.PN Manage
T}	T{
10
T}
T{
.PN Refuse
T}	T{
11
T}
T{
.PN Failed
T}	T{
12
T}
T{
.PN KeepAlive
T}	T{
13\(dg
T}
T{
.PN Alive
T}	T{
14\(dg
T}
.sp 6p
_
.TE
.RE
.FS \(dg
A previous version of this document incorrectly reversed the opcodes of
.PN Alive
and
.PN KeepAlive .
.FE
.LP
Per packet information follows:
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Query
.PN BroadcastQuery
.PN IndirectQuery
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Query, BroadcastQuery or IndirectQuery)
	2	CARD16	length
	1	CARD8	number of Authentication Names sent (m)
	2	CARD16	length of first Authentication Name (m\d\s-21\s+2\u)
	m\d\s-21\s+2\u	CARD8	first Authentication Name
	\&...		Other Authentication Names
.De
Note that these three packets are identical except for the opcode field.
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN ForwardQuery
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always ForwardQuery)
	2	CARD16	length
	2	CARD16	length of Client Address (m)
	m	CARD8	Client Address
	2	CARD16	length of Client Port (n)
	n	CARD8	Client Port
	1	CARD8	number of Authentication Names sent (o)
	2	CARD16	length of first Authentication Name (o\d\s-21\s+2\u)
	o\d\s-21\s+2\u	CARD8	first Authentication Name
	\&...		Other Authentication Names
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Willing
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Willing)
	2	CARD16	length (6 + m + n + o)
	2	CARD16	Length of Authentication Name (m)
	m	CARD8	Authentication Name
	2	CARD16	Hostname length (n)
	n	CARD8	Hostname
	2	CARD16	Status length (o)
	o	CARD8	Status
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Unwilling
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Unwilling)
	2	CARD16	length (4 + m + n)
	2	CARD16	Hostname length (m)
	m	CARD8	Hostname
	2	CARD16	Status length (n)
	n	CARD8	Status
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Request
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Request)
	2	CARD16	length
	2	CARD16	Display Number
	1	CARD8	Count of Connection Types (m)
	2 \(mu m	CARD16	Connection Types
	1	CARD8	Count of Connection Addresses (n)
	2	CARD16	Length of first Connection Address (n\s-2\d1\u\s+2)
	n\s-2\d1\u\s+2	CARD8	First Connection Address
	\&...		Other connection addresses
	2	CARD16	Length of Authentication Name (o)
	o	CARD8	Authentication Name
	2	CARD16	Length of Authentication Data (p)
	p	CARD8	Authentication Data
	1	CARD8	Count of Authorization Names (q)
	2	CARD16	Length of first Authorization Name (q\s-2\d1\u\s+2)
	q\s-2\d1\u\s+2	CARD8	First Authorization Name
	\&...		Other authorization names
	2	CARD16	Length of Manufacturer Display ID (r)
	r	CARD8	Manufacturer Display ID
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Accept
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Accept)
	2	CARD16	length (12 + n + m + o + p)
	4	CARD32	Session ID
	2	CARD16	Length of Authentication Name (n)
	n	CARD8	Authentication Name
	2	CARD16	Length of Authentication Data (m)
	m	CARD8	Authentication Data
	2	CARD16	Length of Authorization Name (o)
	o	CARD8	Authorization Name
	2	CARD16	Length of Authorization Data (p)
	p	CARD8	Authorization Data
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Decline
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Decline)
	2	CARD16	length (6 + m + n + o)
	2	CARD16	Length of Status (m)
	m	CARD8	Status
	2	CARD16	Length of Authentication Name (n)
	n	CARD8	Authentication Name
	2	CARD16	Length of Authentication Data (o)
	o	CARD8	Authentication Data
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Manage
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Manage)
	2	CARD16	length (8 + m)
	4	CARD32	Session ID
	2	CARD16	Display Number
	2	CARD16	Length of Display Class (m)
	m	CARD8	Display Class
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Refuse
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Refuse)
	2	CARD16	length (4)
	4	CARD32	Session ID
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Failed
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Failed)
	2	CARD16	length (6 + m)
	4	CARD32	Session ID
	2	CARD16	Length of Status (m)
	m	CARD8	Status
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN KeepAlive
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always KeepAlive)
	2	CARD16	length (6)
	2	CARD16	Display Number
	4	CARD32	Session ID
.De
.LP
.Ds 0
.TA .2i .75i 1.75i 2.5i
.ta .2i .75i 1.75i 2.5i
.PN Alive
.sp 3p
	2	CARD16	version number (always 1)
	2	CARD16	opcode (always Alive)
	2	CARD16	length (5)
	1	CARD8	Session Running (0: not running 1: running)
	4	CARD32	Session ID (0: not running)
.De
.NH 1
Display Class Format
.XS
\*(SN Display Class Format
.XE
.LP
The Display Class field of the
.PN Manage
packet is used by the display manager to collect common sorts of
displays into manageable groups.  This field is a string encoded of
ISO-LATIN-1 characters in the following format:
.LP
.Ds
\fIManufacturerID\fP\^-\^\fIModelNumber\fP
.De
.LP
Both elements of this string must exclude characters of the set { \fB-\fP,
\&\fB.\fP, \fB:\fP, \fB*\fP, \fB?\fP, \fI<space>\fP }.
The ManufacturerID is a string that should be registered 
with the X Consortium. 
The ModelNumber is designed to identify characteristics of the display
within the manufacturer's product line.
This string should be documented in the users manual for the
particular device and  should probably not be specifiable by the
display user to avoid unexpected configuration errors.
.NH 1
Manufacturer Display ID Format
.XS
\*(SN Manufacturer Display ID Format
.XE
.LP
To authenticate the manager, the display and manager will share a private
key.
The manager, then, must be able to discover which key to use for a
particular device.
The Manufacturer Display ID field of the
.PN Request
packet is intended for this purpose.  Typically, the manager host will
contain a map between this number and the key.  This field is intended to be
unique per display, possibly the ethernet address of the display in the form:
.LP
.Ds
-Ethernet-8:0:2b:a:f:d2
.De
.LP
It can also be a string of the form:
.LP
.Ds
\fIManufacturerID\fP\^-\^\fIModelNumber\fP\^-\^\fISerialNumber\fP
.De
.LP
The ManufacturerID, ModelNumber and SerialNumber are encoded using
ISO-LATIN-1 characters, excluding  { \fB-\fP,
\&\fB.\fP, \fB*\fP, \fB?\fP, \fI<space>\fP }
.LP
When the display is shipped to a customer, it should include both the
Manufacturer Display ID and the private key in the documentation set.
This information should not be modifiable by the display user.
.NH 1
Authentication
.XS
\*(SN Authentication
.XE
.LP
In an environment where authentication is not needed, XDMCP can disable
authentication by having the display send empty Authentication Name
and Authentication Data fields in the
.PN Request
packet. 
In this case, the manager will not attempt to authenticate itself.
Other authentication protocols may be developed, depending on local needs.
.LP
In an unsecure environment, the display must be able to verify that the
source of the various packets is a trusted manager.  These packets will
contain authentication information.  As an example of such a system, the
following discussion describes the "XDM-AUTHENTICATION-1" and
"XDM-AUTHENTICATION-2" authentication systems.  The "XDM-AUTHENTICATION-1"
system uses a 56-bit shared private key, and 64 bits of
authentication data.  "XDM-AUTHENTICATION-2" uses a 256 bit shared private key,
and 256 bits of authentication data.  Associated example X authorization
protocol "XDM-AUTHORIZATION-1" and "XDM-AUTHORIZATION-2" will also be 
discussed. The 56-bit key is represented
as a 64-bit number in network order (big endian).  This means that the first
octet in the representation will be zero.  When incrementing a 64-bit value,
the 8 octets of data will be interpreted in network order (big endian). 
That is, the last octet will be incremented, subsequent carries propogate
towards the first octet.
.IP \(bu 5
Assumptions
.RS
.IP 1. 5
The display and manager share a private key.  This key could be programmed
into the display by the manufacturer and shipped with the unit.  It must not
be available from the display itself, but should allow the value to be
modified in some way.  The system administrator would be responsible for
managing a database of terminal keys.
.IP 2. 5
The display can generate random authentication numbers.
.RE
.LP
Some definitions first:
.EQ
oc D cc sup kappa mark = "encryption of plain text " D " by key " kappa
.EN C
.EQ
oc DELTA cc * sup kappa lineup = "decryption of crypto text " DELTA " with key " kappa
.EN C
.EQ
{ tau } lineup = "private key shared by display and manager"
.EN C
.EQ
rho lineup = "64 bit random number generated by display"
.EN C
.EQ
alpha lineup = "authentication data in XDMCP packets"
.EN C
.EQ
sigma lineup = "per-session private key, generated by manager"
.EN C
.EQ
beta lineup = "authorization data"
.EN
.LP
"XDM-AUTHENTICATION-1" encryption will use the Data Encryption Standard (DES, 
FIPS 46-3); blocks shorter than 64 bits will be zero-filled on the right to 
64 bits.  Blocks longer than 64 bits will use block chaining:
.EQ
oc { D } cc sup kappa lineup = oc { D sub 1 } cc sup kappa " "
oc { D sub 2 } " " xor " " oc { D sub 1 } cc sup kappa cc sup kappa
.EN
.LP
"XDM-AUTHENTICATION-2" encryption will use the Advanced Encryption Standard 
(AES, FIPS-197);  blocks shorter than 128 bits will be zero-filled on the right
to 128 bits.  Blocks longer than 128 bits will use block chaining as shown
above.
.LP
The display generates the first authentication data in the
.PN Request
packet:
.EQ
alpha sub roman Request mark = oc rho cc sup tau
.EN
.LP
For the
.PN Accept
packet, the manager decrypts the initial message and returns
@alpha sub roman Accept@:
.EQ
rho lineup = oc alpha sub roman Request cc * sup tau
.EN C
.EQ
alpha sub roman Accept lineup = oc rho + 1 cc sup tau
.EN
.LP
The
.PN Accept
packet also contains the authorization intended for use by
the X server.  A description of authorization type ``XDM-AUTHORIZATION-1''
follows.
.LP
The
.PN Accept
packet contains the authorization name
``XDM-AUTHORIZATION-1''.  The authorization data is the string:
.EQ
beta sub Accept mark = oc sigma cc sup tau
.EN
.LP
To create authorization information for connection setup with the X server
using the XDM-AUTHORIZATION-1 authorization protocol, the client computes the
following:
.EQ
N mark = "X client identifier"
.EN C
.EQ
T lineup = "Current time in seconds on client host (32 bits)"
.EN
.EQ C
beta lineup = oc rho N T cc sup sigma
.EN
.LP
For TCP connections @N@ is 48 bits long and contains the 32-bit IPv4 address of
the client host followed by the 16-bit port number of the client socket.
Formats for other connections must be registered.
The resulting value, @beta@, is 192 bits of authorization data that is sent
in the connection setup to the server.  The server receives the packet,
decrypts the contents.  To accept the connection, the following must hold:
.IP \(bu 5
@rho@ must match the value generated for the most recent XDMCP negotiation.
.IP \(bu 5
@T@ must be within 1200 seconds of the internally stored time.  If no time
been received before, the current time is set to @T@.
.IP \(bu 5
No packet containing the same pair (@N@, @T@) can have been received
in the last 1200 seconds (20 minutes).
.LP
``XDM-AUTHORIZATION-2'' is identical to ``XDM-AUTHORIZATION-1'', except that
for TCP connections  @N@ is 256 bits long and contains the 128 bit 
IPv6 address of the client host followed by the 16 bit port number of the 
client socket, with the remainder filled with zeros, and @T@ is extended to
64-bits.  IPv4 addresses are represented as IPv4-mapped IPv6 addresses, with
an 80-bit prefix of zero bits, followed by a 16-byte value of 0xFFFF, 
followed by the IPv4 address value, as defined in IETF RFC 2373. Formats for 
other connections must be  registered.
.bp
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.TC
