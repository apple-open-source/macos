.\" Use tbl macros.t ice.ms | troff -ms
.\" $XdotOrg: doc/xorg-docs/specs/ICE/ice.ms,v 1.3 2004/09/03 16:18:19 kem Exp $
.\"
.\" TODO:
.\" Think about connector/listener originator/answerer terminology.
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.\"
.\" Disable hyphenation.  I hate it.
.hy 0
.de hy
..
.\" A couple of macros to standardize things and make them
.\" easy to type.
.de Ss	\" Begin state - .Ss <state name>
.KS
.LP
\fC\\$1\fP\^:
.br
..
.de St	\" Transition - .St "condition" message <new state>
.RS
\\$1
.PN \\$2
\(-> \fC\\$3\fP
.RE
..
.de Se	\" End state - .Se
.LP
.KE
..
.de Ms	\" Start message header - .Ms messagename
.sM
.PN \\$1
.RS
..
.de Mf	\" Field in message - .Mf name; types follow on separate line(s)
.\".br
.IP "\fI\\$1\fP\^:" "\w'\fI\\$1\fP\^:'u+1"
..
.de Mc	\" Field Continuation - .Mc; description follows on separate line(s)
.br
.\" \h'1i'
..
.de Ma	\" Message addendum - .Ma title; contents follow
.IP "\\$1:" "\w'\\$1:'u+1"
..
.de Me	\" End of message header - .Me
.RE
.LP
.eM
..
.de Es	\" Start Encoding - .Es messagename
.KS
.LP
.nf
.PN \\$1
.ta .2i .5i 2.0i
..
.de Ee	\" End Encoding - .Ee
.fi
.LP
.KE
..
.\"
.\" --- cT --- centered title; centers $1, adds TOC entry unless $2 is "no"
.\"
.de cT
\\&		\" filler so that the following .sp really leaves a space
.sp 1
.ce 1
\\s+1\\fB\\$1\\fP\\s-1
.sp 1
.if !'\\$2'no' \{\
.XS \\n(PN
\\$1
.XE
\}
..
.ps 10
.nr PS 10
\&
.TL
\s+2\fBInter-Client Exchange (ICE) Protocol\fP\s-2
.sp
Version 1.1
.sp
X Consortium Standard
.sp
X Version 11, Release 6.8



.AU
Robert Scheifler
.AI
X Consortium, Inc.
.AU
Jordan Brown
.AI
Quarterdeck Office Systems



.AB
.LP
There are numerous possible protocols that can be used for communication
among clients.  They have many similarities and common needs, including 
authentication,
version negotiation,
data typing, and
connection management.  The
.I
Inter-Client Exchange
.R
(ICE) protocol is intended to provide a framework for building such
protocols.  Using ICE reduces the complexity of designing new protocols and
allows the sharing of many aspects of the implementation.
.AE
.LP
.bp
\&
.sp 8
.LP
.DS C
.if n Copyright (c) 1993, 1994 X Consortium
.if t Copyright \(co 1993, 1994 X Consortium
.DE
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
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
.sp 5
X Window System is a trademark of The Open Group.
.EH '\fBInter-Client Exchange Protocol\fP''\fBX11, Release 6.8\fP'
.OH '\fBInter-Client Exchange Protocol\fP''\fBX11, Release 6.8\fP'
.bp 1
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.nH 1 "Purpose and Goals"
.LP
In discussing a variety of protocols \(em existing, under development, and
hypothetical \(em it was noted that they have many elements in common.  Most
protocols need mechanisms for authentication, for
version negotiation,
and for setting up and taking down connections.  There are also
cases where the same two parties need to talk to each other using multiple
protocols.  For example, an embedding relationship between two parties is
likely to require the simultaneous use of session management, data transfer,
focus negotiation, and command notification protocols.  While these are
logically separate protocols, it is desirable for them to share as many
pieces of implementation as possible.
.LP
The
.I
Inter-Client Exchange
.R
(ICE) protocol provides a generic framework for building protocols on top of
reliable, byte-stream transport connections.  It provides basic mechanisms
for setting up and shutting down connections, for performing authentication,
for negotiating
versions,
and for reporting errors.  The
protocols running within an ICE connection are referred to here as
.I subprotocols.
ICE provides facilities for each subprotocol to do its own version
negotiation, authentication, and error reporting.  In addition, if two
parties are communicating using several different subprotocols, ICE will
allow them to share the same transport layer connection.
.nH 1 "Overview of the protocol"
.LP
Through some mechanism outside ICE, two parties make themselves known to
each other and agree that they would like to communicate using an ICE
subprotocol.  ICE assumes that this negotation includes some notion by which
the parties will decide which is the \*Qoriginating\*U party and which is
the \*Qanswering\*U party.  The negotiation will also need to provide the
originating party with a name or address of the answering party.  Examples
of mechanisms by which parties can make themselves known to each other are
the X selection mechanism, environment
variables, and shared files.
.LP
The originating party first determines whether there is an existing ICE
connection between the two parties.  If there is, it can re-use the existing
connection and move directly to the setup of the subprotocol.  If no ICE
connection exists, the originating party will open a transport connection to
the answering party and will start ICE connection setup.
.LP
The ICE connection setup dialog consists of three major parts: byte order
exchange, authentication, and connection information exchange.  The first
message in each direction is a
.PN ByteOrder
message telling which byte order will be used by the sending party in
messages that it sends.  After that, the originating party sends a
.PN ConnectionSetup
message giving information about itself (vendor name and release number) and
giving a list of ICE version numbers it is capable of supporting and a list
of authentication schemes it is willing to accept.  Authentication is
optional.  If no authentication is required, the answering party responds
with a
.PN ConnectionReply
message giving information about itself, and the connection setup is complete.
.LP
If the connection setup is to be authenticated, the answering party will
respond with an
.PN AuthenticationRequired
message instead of a
.PN ConnectionReply
message.  The parties then exchange
.PN AuthenticationReply
and
.PN AuthenticationNextPhase
messages until authentication is complete, at which time the answering party
finally sends its
.PN ConnectionReply
message.
.LP
Once an ICE connection is established (or an existing connection reused),
the originating party starts subprotocol negotiation by sending a
.PN ProtocolSetup
message.  This message gives the name of the subprotocol that the parties
have agreed to use, along with the ICE major opcode that the originating
party has assigned to that subprotocol.  Authentication can also occur for
the subprotocol, independently of authentication for the connection.
Subprotocol authentication is optional.  If there is no subprotocol
authentication, the answering party responds with a
.PN ProtocolReply
message, giving the ICE major opcode that it has assigned
for the subprotocol.
.LP
Subprotocols are authenticated independently of each other, because they may
have differing security requirements.  If there is authentication for this
particular subprotocol, it takes place before the answering party emits the
.PN ProtocolReply
message, and it uses the
.PN AuthenticationRequired ,
.PN AuthenticationReply ,
and
.PN AuthenticationNextPhase
messages, just as for the connection authentication.  Only when subprotocol
authentication is complete does the answering party send its
.PN ProtocolReply
message.
.LP
When a subprotocol has been set up and authenticated, the two parties can
communicate using messages defined by the subprotocol.  Each message has two
opcodes: a major opcode and a minor opcode.  Each party will send messages
using the major opcode it has assigned in its
.PN ProtocolSetup
or
.PN ProtocolReply
message.  These opcodes will, in general, not be the same.  For a particular
subprotocol, each party will need to keep track of two major opcodes: the
major opcode it uses when it sends messages, and the major opcode it expects
to see in messages it receives.  The minor opcode values and semantics are
defined by each individual subprotocol.
.LP
Each subprotocol will have one or more messages whose semantics are that the
subprotocol is to be shut down.  Whether this is done unilaterally or is
performed through negotiation is defined by each subprotocol.  Once a
subprotocol is shut down, its major opcodes are removed from
use; no further messages on this subprotocol should be sent until the
opcode is reestablished with
.PN ProtocolSetup .
.LP
ICE has a facility to negotiate the closing of the connection when there are
no longer any active subprotocols.  When either party decides that no
subprotocols are active, it can send a
.PN WantToClose
message.  If the other party agrees to close the connection, it can simply
do so.  If the other party wants to keep the connection open, it can
indicate its desire by replying with a
.PN NoClose
message.
.\" XXX - Note that it's likely that both parties will WantToClose at once.
.LP
It should be noted that the party that initiates the connection isn't
necessarily the same as the one that initiates setting up a subprotocol.
For example, suppose party A connects to party B.  Party A will issue the
.PN ConnectionSetup
message and party B will respond with a
.PN ConnectionReply
message.  (The authentication steps are omitted here for brevity.)
Typically, party A will also issue the
.PN ProtocolSetup
message and expect a
.PN ProtocolReply
from party B.  Once the connection is established, however, either party may
initiate the negotiation of a subprotocol.  Continuing this example, party B
may decide that it needs to set up a subprotocol for communication with
party A.  Party B would issue the
.PN ProtocolSetup
message and expect a
.PN ProtocolReply
from party A.
.nH 1 "Data Types"
.LP
ICE messages contain several types of data.  Byte order is negotiated in
the initial connection messages; in general data is sent in the sender's
byte order and the receiver is required to swap it appropriately.
In order to support 64-bit machines, ICE messages
are padded to multiples of 8 bytes.  All messages are designed so that
fields are \*Qnaturally\*U aligned on 16-, 32-, and 64-bit boundaries.
The following formula gives the number of bytes necessary 
to pad \fIE\fP bytes to the next multiple of \fIb\fP\^:
.DS
pad(\fIE\fP, \fIb\fP\^) = (\fIb\fP \- (\fIE\fP mod \fIb\fP\^)) mod \fIb\fP
.DE
.nH 2 "Primitive Types"
.LP
.TS H
expand;
lB lB
l lw(3.5i).
_
.sp 6p
Type Name	Description
.sp 6p
_
.sp 6p
.TH
.R
CARD8	8-bit unsigned integer
CARD16	16-bit unsigned integer
CARD32	32-bit unsigned integer
BOOL	T{
.PN False
or
.PN True
T}
LPCE	T{
A character from the X Portable Character Set in Latin Portable Character
Encoding
T}
.sp 6p
_
.TE
.KS
.nH 2 "Complex Types"
.LP
.TS H
expand;
lB lB
l lw(3.5i).
_
.sp 6p
Type Name	Type
.sp 6p
_
.sp 6p
.TH
.R
VERSION	[Major, minor: CARD16]
STRING	LISTofLPCE
.sp 6p
_
.TE
.KE
LISTof<type> denotes a counted collection of <type>.  The exact encoding
varies depending on the context; see the encoding section.
.nH 1 "Message Format"
.LP
All ICE messages include the following information:
.TS H
expand;
cB lB

l lw(3.5i).
_
.sp 6p
Field Type	Description
.sp 6p
_
.sp 6p
.TH
CARD8	protocol major opcode
CARD8	protocol minor opcode
CARD32	length of remaining data in 8-byte units
.sp 6p
_
.TE
.LP
The fields are as follows:
.LP
Protocol major opcode
.RS
This specifies what subprotocol the message is intended for.  Major opcode
0 is reserved for ICE control messages.  The major opcodes of other
subprotocols are dynamically assigned and exchanged at protocol
negotiation time.
.RE
.LP
Protocol minor opcode
.RS
This specifies what protocol-specific operation is to be performed.
Minor opcode 0 is reserved for Errors; other values are protocol-specific.
.RE
.LP
Length of data in 8-byte units
.RS
This specifies the length of the information following the first 8 bytes.
Each message-type has a different format, and will need to be separately
length-checked against this value.  As every data item has either an
explicit length, or an implicit length, this can be easily accomplished.
Messages that have too little or too much data indicate a serious
protocol failure, and should result in a
.PN BadLength
error.
.RE
.nH 1 "Overall Protocol Description"
.LP
Every message sent in a given direction has an implicit sequence number,
starting with 1.  Sequence numbers are global to the connection; independent
sequence numbers are \fInot\fP maintained for each protocol.
.LP
Messages of a given major-opcode (i.e., of a given protocol) must be
responded to (if a response is called for) in order by the receiving party.
Messages from different protocols can be responded to in arbitrary order.
.LP
Minor opcode 0 in every protocol is for reporting errors.  At most one error
is generated per request.  If more than one error condition is encountered
in processing a request, the choice of which error is returned is
implementation-dependent.
.Ms Error
.Mf offending-minor-opcode
CARD8
.Mf severity
.Pn { CanContinue ,
.PN FatalToProtocol ,
.PN FatalToConnection }
.Mf sequence-number
CARD32
.Mf class
CARD16
.Mf value(s)
<dependent on major/minor opcode and class>
.Me
This message is sent to report an error in response to a message
from any protocol.
The
.PN Error
message
exists in all protocol major-opcode spaces; it
is minor-opcode zero in every protocol.  The minor opcode of the
message that caused the error is reported, as well as the sequence
number of that message.
The severity indicates the sender's behavior following
the identification of the error.
.PN CanContinue
indicates the sender is willing to accept additional messages for this
protocol.
.PN FatalToProcotol
indicates the sender is unwilling to accept further messages for this
protocol but that messages for other protocols may be accepted.
.PN FatalToConnection
indicates the sender is unwilling to accept any further
messages for any protocols on the connection.  The sender
is required to conform to specified severity conditions
for generic and ICE (major opcode 0) errors; see Sections 6.1
and 6.2.
The class defines the generic class of
error.  Classes are specified separately for each protocol (numeric
values can mean different things in different protocols).  The error
values, if any, and their types vary with the specific error class
for the protocol.
.LP
.\" XXX
.\" (Asynchronous errors \(em errors not associated with a previous
.\" message???  If so, offending-minor and sequence = 0.)
.nH 1 "ICE Control Subprotocol \(em Major Opcode 0"
.LP
Each of the ICE control opcodes is described below.
Most of the messages have additional information included beyond the
description above.  The additional information is appended to the message
header and
the length field is computed accordingly.
.LP
In the following message descriptions, \*QExpected errors\*U indicates
errors that may occur in the normal course of events.  Other errors
(in particular
.PN BadMajor ,
.PN BadMinor ,
.PN BadState ,
.PN BadLength ,
.PN BadValue ,
.PN ProtocolDuplicate ,
and
.PN MajorOpcodeDuplicate )
might occur, but generally indicate a serious implementation failure on
the part of the
errant
peer.
.Ms ByteOrder
.Mf byte-order
.Pn { MSBfirst ,
.PN LSBfirst }
.Me
Both parties must send this message before sending any other,
including errors.  This message specifies the byte order that
will be used on subsequent messages sent by this party.
.LP
Note:  If the receiver detects an error in this message,
it must be sure to send its own
.PN ByteOrder
message before sending the
.PN Error .
.Ms ConnectionSetup
.Mf versions
LISTofVERSION
.Mf must-authenticate
BOOL
.Mf authentication-protocol-names
LISTofSTRING
.Mf vendor
STRING
.Mf release
STRING
.LP
.Ma "Responses"
.PN ConnectionReply ,
.PN AuthenticationRequired .
(See note)
.Ma "Expected errors"
.PN NoVersion ,
.PN SetupFailed ,
.PN NoAuthentication ,
.PN AuthenticationRejected ,
.Mc
.PN AuthenticationFailed .
.Me
The party that initiates the connection
(the
one that does the \*Qconnect()\*U)
must send this
message
as the second message (after
.PN ByteOrder )
on startup.
.LP
Versions gives a list, in decreasing order of preference, of the
protocol versions this party is capable of speaking.  This document
specifies major version 1, minor version 0.
.LP
If must-authenticate is
.PN True ,
the initiating party demands authentication; the accepting party \fImust\fP
pick an authentication scheme and use it.  In this case, the only valid
response is
.PN AuthenticationRequired .
.LP
If must-authenticate is
.PN False ,
the accepting party may choose an authentication mechanism, use a
host-address-based authentication scheme, or skip authentication.
When must-authenticate is
.PN False ,
.PN ConnectionReply
and
.PN AuthenticationRequired
are both valid responses.  If a host-address-based authentication scheme is
used,
.PN AuthenticationRejected
and
.PN AuthenticationFailed
errors are possible.
.LP
Authentication-protocol-names  specifies a (possibly null, if
must-authenticate is
.PN False )
list of authentication protocols the party is willing to perform.  If
must-authenticate is
.PN True ,
presumably the party will offer only authentication mechanisms
allowing mutual authentication.
.LP
Vendor gives the name of the vendor of this ICE implementation.
.LP
Release gives the release identifier of this ICE implementation.
.LP
.Ms AuthenticationRequired
.Mf authentication-protocol-index
CARD8
.Mf data
<specific to authentication protocol>
.LP
.Ma "Response"
.PN AuthenticationReply .
.Ma "Expected errors"
.PN AuthenticationRejected ,
.PN AuthenticationFailed .
.Me
This message is sent in response to a
.PN ConnectionSetup
or
.PN ProtocolSetup
message to specify that authentication is to be done and what authentication
mechanism is to be used.
.LP
The authentication protocol is specified by a 0-based index into the list
of names given in the
.PN ConnectionSetup
or
.PN ProtocolSetup .
Any protocol-specific data that might be required is also sent.
.Ms AuthenticationReply
.Mf data
<specific to authentication protocol>
.LP
.Ma "Responses"
.PN AuthenticationNextPhase ,
.PN ConnectionReply ,
.PN ProtocolReply .
.Ma "Expected errors"
.PN AuthenticationRejected ,
.PN AuthenticationFailed ,
.PN SetupFailed .
.Me
This message is sent in response to an
.PN AuthenticationRequired
or
.PN AuthenticationNextPhase
message, to
supply authentication data as defined by the authentication protocol
being used.
.LP
Note that this message is sent by the party that initiated the current
negotiation \(em the party that sent the
.PN ConnectionSetup
or
.PN ProtocolSetup
message.
.LP
.PN AuthenticationNextPhase
indicates that more is to be done to complete the authentication.
If the authentication is complete,
.PN ConnectionReply
is appropriate if the current authentication handshake is the result of a
.PN ConnectionSetup ,
and a
.PN ProtocolReply
is appropriate if it is the result of a
.PN ProtocolSetup .
.Ms AuthenticationNextPhase
.Mf data
<specific to authentication protocol>
.LP
.Ma "Response"
.PN AuthenticationReply .
.Ma "Expected errors"
.PN AuthenticationRejected ,
.PN AuthenticationFailed .
.Me
This message is sent in response to an
.PN AuthenticationReply
message, to supply authentication data as defined by the authentication
protocol being used.
.Ms ConnectionReply
.Mf version-index
CARD8
.Mf vendor
STRING
.Mf release
STRING
.Me
This message is sent in response to a
.PN ConnectionSetup
or
.PN AuthenticationReply
message to indicate that the authentication handshake is complete.
.LP
Version-index gives a 0-based index into the list of versions offered in
the
.PN ConnectionSetup
message; it specifies the version of the ICE protocol that both parties
should speak for the duration of the connection.
.LP
Vendor gives the name of the vendor of this ICE implementation.
.LP
Release gives the release identifier of this ICE implementation.
.Ms ProtocolSetup
.Mf protocol-name
STRING
.Mf major-opcode
CARD8
.Mf versions
LISTofVERSION
.Mf vendor
STRING
.Mf release
STRING
.Mf must-authenticate
BOOL
.Mf authentication-protocol-names
LISTofSTRING
.LP
.Ma "Responses"
.PN AuthenticationRequired ,
.PN ProtocolReply .
.Ma "Expected errors"
.PN UnknownProtocol ,
.PN NoVersion ,
.PN SetupFailed ,
.PN NoAuthentication ,
.Mc
.PN AuthenticationRejected ,
.PN AuthenticationFailed .
.Me
This message is used to initiate negotiation of
a protocol and establish any authentication
specific to it.
.LP
Protocol-name gives the name of the protocol the party wishes
to speak.
.LP
Major-opcode gives the opcode that the party will use in messages
it sends.
.LP
Versions gives a list of version numbers, in decreasing order of
preference, that the party is willing to speak.
.LP
Vendor and release are identification strings with semantics defined
by the specific protocol being negotiated.
.LP
If must-authenticate is
.PN True ,
the initiating party demands authentication; the accepting party \fImust\fP
pick an authentication scheme and use it.  In this case, the only valid
response is
.PN AuthenticationRequired .
.LP
If must-authenticate is
.PN False ,
the accepting party may choose an authentication mechanism, use a
host-address-based authentication scheme, or skip authentication.
When must-authenticate is
.PN False ,
.PN ProtocolReply
and
.PN AuthenticationRequired
are both valid responses.  If a host-address-based authentication scheme is
used,
.PN AuthenticationRejected
and
.PN AuthenticationFailed
errors are possible.
.LP
Authentication-protocol-names  specifies a (possibly null, if
must-authenticate is
.PN False )
list of authentication protocols the party is willing to perform.  If
must-authenticate is
.PN True ,
presumably the party will offer only authentication mechanisms
allowing mutual authentication.
.Ms ProtocolReply
.Mf major-opcode
CARD8
.Mf version-index
CARD8
.Mf vendor
STRING
.Mf release
STRING
.Me
This message is sent in response to a
.PN ProtocolSetup
or
.PN AuthenticationReply
message to indicate that the authentication handshake is complete.
.LP
Major-opcode gives the opcode that this party will use in
messages that it sends.
.LP
Version-index gives a 0-based index into the list of versions offered in the
.PN ProtocolSetup
message; it specifies the version of the protocol that both
parties should speak for the duration of the connection.
.LP
Vendor and release are identification strings with semantics defined
by the specific protocol being negotiated.
.LP
.Ms Ping
.Ma "Response"
.PN PingReply .
.Me
This message is used to test if the connection is still functioning.
.Ms PingReply
.Me
This message is sent in response to a
.PN Ping
message, indicating that the connection is still functioning.
.Ms WantToClose
.Ma "Responses"
.PN WantToClose ,
.PN NoClose ,
.PN ProtocolSetup .
.Me
This message is used to initiate a possible close of the connection.
The sending party has noticed that, as a result of mechanisms specific
to each protocol, there are no active
protocols
left.
There are
four possible scenarios arising from this request:
.IP (1) 5
The receiving side noticed too, and has already sent a
.PN WantToClose .
On receiving a
.PN WantToClose
while already attempting to shut down, each party should simply close the
connection.
.IP (2)
The receiving side hasn't noticed, but agrees.  It closes
the connection.
.IP (3)
The receiving side has a
.PN ProtocolSetup
\*Qin flight,\*U in which case it is to ignore
.PN WantToClose
and the party sending
.PN WantToClose
is to abandon the shutdown attempt when it receives the
.PN ProtocolSetup .
.IP (4)
The receiving side wants the connection kept open for some
reason not specified by the ICE protocol, in which case it
sends
.PN NoClose .
.LP
See the state transition diagram for additional information.
.Ms NoClose
.Me
This message is sent in response to a
.PN WantToClose
message to indicate that the responding
party does not want the connection closed at
this time.  The receiving party should not close the
connection.  Either party may again initiate
.PN WantToClose
at some future time.
.nH 2 "Generic Error Classes"
.LP
These errors should be used by all protocols, as applicable.
For ICE (major opcode 0),
.PN FatalToProtocol
should
be interpreted as
.PN FatalToConnection.
.Ms BadMinor
.Mf offending-minor-opcode
<any>
.Mf severity
.PN FatalToProtocol
or
.PN CanContinue
(protocol's discretion)
.Mf values
(none)
.Me
Received a message with an unknown minor opcode.
.br
.ne 9
.Ms BadState
.Mf offending-minor-opcode
<any>
.Mf severity
.PN FatalToProtocol
or
.PN CanContinue
(protocol's discretion)
.Mf values
(none)
.Me
Received a message with a valid minor opcode which is not appropriate
for the current state of the protocol.
.Ms BadLength
.Mf offending-minor-opcode
<any>
.Mf severity
.PN FatalToProtocol
or
.PN CanContinue
(protocol's discretion)
.Mf values
(none)
.Me
Received a message with a bad length.  The length of the message is
longer or shorter than required to contain the data.
.Ms BadValue
.Mf offending-minor-opcode
<any>
.Mf severity
.PN CanContinue
.Mf values
CARD32 Byte offset to offending value in offending message
.Mc
CARD32 Length of offending value
.Mc
<varies> Offending value
.Me
Received a message with a bad value specified.
.nH 2 "ICE Error Classes"
.LP
These errors are all major opcode 0 errors.
.Ms BadMajor
.Mf offending-minor-opcode
<any>
.Mf severity
.PN CanContinue
.Mf values
CARD8 Opcode
.Me
The opcode given is not one that has been registered.
.Ms NoAuthentication
.Mf offending-minor-opcode
.PN ConnectionSetup ,
.PN ProtocolSetup
.Mf severity
.PN ConnectionSetup
\(->
.PN FatalToConnection
.Mc
.PN ProtocolSetup
\(->
.PN FatalToProtocol
.Mf values
(none)
.Me
None of the authentication protocols offered are available.
.Ms NoVersion
.Mf offending-minor-opcode
.PN ConnectionSetup ,
.PN ProtocolSetup
.Mf severity
.PN ConnectionSetup
\(->
.PN FatalToConnection
.Mc
.PN ProtocolSetup
\(->
.PN FatalToProtocol
.Mf values
(none)
.Me
None of the protocol versions offered are available.
.\" .Ms SetupFailed
.sM
.PN SetupFailed
.RS
.Mf offending-minor-opcode
.PN ConnectionSetup ,
.PN ProtocolSetup ,
.PN AuthenticationReply
.Mf severity
.PN ConnectionSetup
\(->
.PN FatalToConnection
.Mc
.PN ProtocolSetup
\(->
.PN FatalToProtocol
.Mc
.PN AuthenticationReply
\(->
.PN FatalToConnection
if authenticating a connection, otherwise
.PN FatalToProtocol
.Mf values
STRING reason
.Me
The sending side is unable to accept the
new connection or new protocol for a reason other than authentication
failure.  Typically this error will be a result of inability to allocate
additional resources on the sending side.  The reason field will give a
human-interpretable message providing further detail on the type of failure.
.br
.Ms AuthenticationRejected
.Mf offending-minor-opcode
.PN AuthenticationReply ,
.PN AuthenticationRequired ,
.br
.PN AuthenticationNextPhase
.Mf severity
.PN FatalToProtocol
.Mf values
STRING reason
.Me
Authentication rejected.  The peer has failed to properly
authenticate itself.
The reason field will give a human-interpretable message
providing further detail.
.Ms AuthenticationFailed
.Mf offending-minor-opcode
.PN AuthenticationReply ,
.PN AuthenticationRequired ,
.br
.PN AuthenticationNextPhase
.Mf severity
.PN FatalToProtocol
.Mf values
STRING reason
.Me
Authentication failed.
.PN AuthenticationFailed
does not imply that the authentication was rejected, as
.PN AuthenticationRejected
does.  Instead it means that the sender was unable to complete
the authentication for some other reason.  (For instance, it
may have been unable to contact an authentication server.)
The reason field will give a human-interpretable message
providing further detail.
.br
.ne 10
.Ms ProtocolDuplicate
.Mf offending-minor-opcode
.PN ProtocolSetup
.Mf severity
.PN FatalToProtocol
(but see note)
.Mf values
STRING protocol name
.Me
The protocol name was already registered.  This is fatal to
the \*Qnew\*U protocol being set up by
.PN ProtocolSetup ,
but it does not affect the existing registration.
.Ms MajorOpcodeDuplicate
.Mf offending-minor-opcode
.PN ProtocolSetup
.Mf severity
.PN FatalToProtocol
(but see note)
.Mf values
CARD8 opcode
.Me
The major opcode specified was already registered.  This is
fatal to the \*Qnew\*U protocol being set up by
.PN ProtocolSetup ,
but it does not affect the existing registration.
.Ms UnknownProtocol
.Mf offending-minor-opcode
.PN ProtocolSetup
.Mf severity
.PN FatalToProtocol
.Mf values
STRING protocol name
.Me
The protocol specified is not supported.
.nH 1 "State Diagrams"
.LP
Here are the state diagrams for the party that initiates the connection:
.Ss start
.\" .St "connect to other end, send" ConnectionSetup conn_wait
.RS
connect to other end, send
.PN ByteOrder ,
.PN ConnectionSetup
\(-> \fCconn_wait\fP
.RE
.Se
.Ss conn_wait
.St "receive" ConnectionReply stasis
.St "receive" AuthenticationRequired conn_auth1
.St "receive" Error quit
.St "receive <other>, send" Error quit
.Se
.Ss conn_auth1
.St "if good auth data, send" AuthenticationReply conn_auth2
.St "if bad auth data, send" Error quit
.Se
.Ss conn_auth2
.St "receive" ConnectionReply stasis
.St "receive" AuthenticationNextPhase conn_auth1
.St "receive" Error quit
.St "receive <other>, send" Error quit
.Se
.br
.ne 22
Here are top-level state transitions for the party that accepts connections.
.Ss listener
.\" .St "accept connection" "" init_wait
.RS
accept connection \(-> \fCinit_wait\fP
.RE
.Se
.Ss init_wait
.\" .St "receive ByteOrder, ConnectionSetup" auth_ask
.RS
receive
.PN ByteOrder ,
.PN ConnectionSetup
\(-> \fCauth_ask\fP
.RE
.St "receive <other>, send" Error quit
.Se
.Ss auth_ask
.\" .St "send ByteOrder, ConnectionReply" stasis
.RS
send
.PN ByteOrder ,
.PN ConnectionReply
\(-> \fCstasis\fP
.RE
.St "send" AuthenticationRequired auth_wait
.St "send" Error quit
.Se
.Ss auth_wait
.St "receive" AuthenticationReply auth_check
.St "receive <other>, send" Error quit
.Se
.Ss auth_check
.St "if no more auth needed, send" ConnectionReply stasis
.St "if good auth data, send" AuthenticationNextPhase auth_wait
.St "if bad auth data, send" Error quit
.Se
.sp 1
Here are the top-level state transitions for all parties after the initial
connection establishment subprotocol.
.LP
Note:  this is not quite the truth for branches out from stasis, in
that multiple conversations can be interleaved on the connection.
.Ss stasis
.St "send" ProtocolSetup proto_wait
.St "receive" ProtocolSetup proto_reply
.St "send" Ping ping_wait
.\" .St "receive Ping, send PingReply" stasis
.RS
receive
.PN Ping ,
send
.PN PingReply
\(-> \fCstasis\fP
.RE
.St "receive" WantToClose shutdown_attempt
.St "receive <other>, send" Error stasis
.St "all protocols shut down, send" WantToClose close_wait
.Se
.Ss proto_wait
.St "receive" ProtocolReply stasis
.St "receive" AuthenticationRequired give_auth1
.\" .St "receive Error, give up on this protocol" stasis
.RS
receive
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.St "receive" WantToClose proto_wait
.Se
.Ss give_auth1
.St "if good auth data, send" AuthenticationReply give_auth2
.\" .St "if bad auth data, send Error, give up on this protocol" stasis
.RS
if bad auth data, send
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.St "receive" WantToClose give_auth1
.Se
.Ss give_auth2
.St "receive" ProtocolReply stasis
.St "receive" AuthenticationNextPhase give_auth1
.\" .St "receive Error, give up on this protocol" stasis
.RS
receive
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.St "receive" WantToClose give_auth2
.Se
.Ss proto_reply
.St "send" ProtocolReply stasis
.St "send" AuthenticationRequired take_auth1
.\" .St "send Error, give up on this protocol" stasis
.RS
send
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.Se
.Ss take_auth1
.St "receive" AuthenticationReply take_auth2
.\" .St "receive Error, give up on this protocol" stasis
.RS
receive
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.Se
.Ss take_auth2
.\" .St "if good auth data" take_auth3
.RS
if good auth data \(-> \fCtake_auth3\fP
.RE
.\" .St "if bad auth data, send Error, give up on this protocol" stasis
.RS
if bad auth data, send
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.Se
.Ss take_auth3
.St "if no more auth needed, send" ProtocolReply stasis
.St "if good auth data, send" AuthenticationNextPhase take_auth1
.\" .St "if bad auth data, send Error, give up on this protocol" stasis
.RS
if bad auth data, send
.PN Error ,
give up on this protocol \(-> \fCstasis\fP
.RE
.Se
.Ss ping_wait
.St "receive" PingReply stasis
.Se
.Ss quit
.RS
\(-> close connection
.RE
.Se
.sp 1
Here are the state transitions for shutting down the connection:
.Ss shutdown_attempt
.St "if want to stay alive anyway, send" NoClose stasis
.\" .St "else" quit
.RS
else \(-> \fCquit\fP
.RE
.Se
.Ss close_wait
.St "receive" ProtocolSetup proto_reply
.St "receive" NoClose stasis
.St "receive" WantToClose quit
.\" .St "connection close" quit
.RS
connection close \(-> \fCquit\fP
.RE
.Se
.nH 1 "Protocol Encoding"
.LP
In the encodings below, the first column is the number of bytes occupied.
The second column is either the type (if the value is variable) or the
actual value.  The third column is the description of the value (e.g.,
the parameter name).  Receivers must ignore bytes that are designated
as unused or pad bytes.
.LP
This document describes major version 1, minor version 0 of the ICE protocol.
.LP
LISTof<type> indicates some number of repetitions of <type>, with no
additional padding.  The number of repetitions must be specified elsewhere
in the message.
.KS
.nH 2 "Primitive Types"
.LP
.TS H
expand;
lB lB lB
l l lw(3.5i).
_
.sp 6p
Type Name	Length (bytes)	Description
.sp 6p
_
.sp 6p
.TH
.R
CARD8	1	8-bit unsigned integer
CARD16	2	16-bit unsigned integer
CARD32	4	32-bit unsigned integer
LPCE	1	T{
A character from the X Portable Character Set in Latin Portable Character
Encoding
T}
.sp 6p
_
.TE
.KE
.KS
.nH 2 "Enumerations"
.LP
.TS H
expand;
lB lB lB
l l lw(3.5i).
_
.sp 6p
Type Name	Value	Description
.sp 6p
_
.sp 6p
.TH
.R
BOOL	0	T{
.PN False
T}
	1	T{
.PN True
T}
.sp 6p
_
.TE
.KE
.KS
.nH 2 "Compound Types"
.LP
.TS H
expand;
lB lB lB lB
l l l lw(3.5i).
_
.sp 6p
Type Name	Length (bytes)	Type	Description
.sp 6p
_
.sp 6p
.TH
.R
VERSION
	2	CARD16	Major version number
	2	CARD16	Minor version number
STRING
	2	CARD16	length of string in bytes
	n	LISTofLPCE	string
	p		unused, p = pad(n+2, 4)
.sp 6p
_
.TE
.KE
.ne 6
.nH 2 "ICE Minor opcodes"
.LP
.RS
.TS
lB cB
l n.
_
.sp 6p
Message Name	Encoding
.sp 6p
_
.sp 6p
Error	0
ByteOrder	1
ConnectionSetup	2
AuthenticationRequired	3
AuthenticationReply	4
AuthenticationNextPhase	5
ConnectionReply	6
ProtocolSetup	7
ProtocolReply	8
Ping	9
PingReply	10
WantToClose	11
NoClose	12
.sp 6p
_
.TE
.RE
.\" XXX - This is hokey, but I don't think you can nest .KS/.KE.
.ne 16
.nH 2 "Message Encoding"
.LP
.Es Error
	1	CARD8	major-opcode
	1	0	Error
	2	CARD16	class
	4	(n+p)/8+1	length
	1	CARD8	offending-minor-opcode
	1		severity:
		0	   CanContinue
		1	   FatalToProtocol
		2	   FatalToConnection
	2		unused
	4	CARD32	sequence number of erroneous message
	n	<varies>	value(s)
	p		pad, p = pad(n,8)
.Ee
.Es ByteOrder
	1	0	ICE
	1	1	ByteOrder
	1		byte-order:
		0	   LSBfirst
		1	   MSBfirst
	1		unused
	4	0	length
.Ee
.Es ConnectionSetup
	1	0	ICE
	1	2	ConnectionSetup
	1	CARD8	Number of versions offered
	1	CARD8	Number of authentication protocol names offered
	4	(i+j+k+m+p)/8+1	length
	1	BOOL	must-authenticate
	7		unused
	i	STRING	vendor
	j	STRING	release
	k	LISTofSTRING	authentication-protocol-names
	m	LISTofVERSION	version-list
	p		unused, p = pad(i+j+k+m,8)
.Ee
.Es AuthenticationRequired
	1	0	ICE
	1	3	AuthenticationRequired
	1	CARD8	authentication-protocol-index
	1		unused
	4	(n+p)/8+1	length    
	2	n	length of authentication data
	6		unused
	n	<varies>	data
	p		unused, p = pad(n,8)
.Ee
.Es AuthenticationReply
	1	0	ICE
	1	4	AuthenticationReply
	2		unused
	4	(n+p)/8+1	length
	2	n	length of authentication data
	6		unused
	n	<varies>	data
	p		unused, p = pad(n,8)
.Ee
.Es AuthenticationNextPhase
	1	0	ICE
	1	5	AuthenticationNextPhase
	2		unused
	4	(n+p)/8+1	length
	2	n	length of authentication data
	6		unused
	n	<varies>	data
	p		unused, p = pad(n,8)
.Ee
.Es ConnectionReply
	1	0	ICE
	1	6	ConnectionReply
	1	CARD8	version-index
	1		unused
	4	(i+j+p)/8	length
	i	STRING	vendor
	j	STRING	release
	p		unused, p = pad(i+j,8)
.Ee
.Es ProtocolSetup
	1	0	ICE
	1	7	ProtocolSetup
	1	CARD8	major-opcode
	1	BOOL	must-authenticate
	4	(i+j+k+m+n+p)/8+1	length
	1	CARD8	Number of versions offered
	1	CARD8	Number of authentication protocol names offered
	6		unused
	i	STRING	protocol-name
	j	STRING	vendor
	k	STRING	release
	m	LISTofSTRING	authentication-protocol-names
	n	LISTofVERSION	version-list
	p		unused, p = pad(i+j+k+m+n,8)
.Ee
.Es ProtocolReply
	1	0	ICE
	1	8	ProtocolReply
	1	CARD8	version-index
	1	CARD8	major-opcode
	4	(i+j+p)/8	length
	i	STRING	vendor
	j	STRING	release
	p		unused, p = pad(i+j, 8)
.Ee
.Es Ping
	1	0	ICE
	1	9	Ping
	2	0	unused
	4	0	length
.Ee
.Es PingReply
	1	0	ICE
	1	10	PingReply
	2	0	unused
	4	0	length
.Ee
.Es WantToClose
	1	0	ICE
	1	11	WantToClose
	2	0	unused
	4	0	length
.Ee
.Es NoClose
	1	0	ICE
	1	12	NoClose
	2	0	unused
	4	0	length
.Ee
.nH 2 "Error Class Encoding"
.LP
Generic errors have classes in the range 0x8000\-0xFFFF, and
subprotocol-specific errors are in the range 0x0000\-0x7FFF.
.nH 3 "Generic Error Class Encoding"
.LP
.TS
lB cB
l n.
_
.sp 6p
Class	Encoding
.sp 6p
_
.sp 6p
BadMinor	0x8000
BadState	0x8001
BadLength	0x8002
BadValue	0x8003
.sp 6p
_
.TE
.nH 3 "ICE-specific Error Class Encoding"
.LP
.TS
lB cB
l n.
_
.sp 6p
Class	Encoding
.sp 6p
_
.sp 6p
BadMajor	0
NoAuthentication	1
NoVersion	2
SetupFailed	3
AuthenticationRejected	4
AuthenticationFailed	5
ProtocolDuplicate	6
MajorOpcodeDuplicate	7
UnknownProtocol	8
.sp 6p
_
.TE
.bp
.\" Set registers to number the appendixes A.1, B.1, C.1, ...
.nr H1 0
.af H1 A
.cT "Appendix A" no
.nH 1 "Modification History"
.nH 2 "Release 6 to Release 6.1"
.LP
Release 6.1 added the ICE X rendezvous protocol (Appendix B) and
updated the document version to 1.1.
.nH 2 "Release 6.1 to Release 6.3"
.LP
Release 6.3 added the listen on well known ports feature.
.bp
.cT "Appendix B" no
.nH 1 "ICE X Rendezvous Protocol"
.nH 2 "Introduction"
.LP
The ICE X rendezvous protocol is designed to answer the need posed
in Section 2 for one mechanism by which two clients interested in
communicating via ICE are able to exchange the necessary information.
This protocol is appropriate for any two ICE clients who also have X
connections to the same X server.
.nH 2 "Overview of ICE X Rendezvous"
.LP
The ICE X Rendezvous Mechanism requires clients willing to act as ICE
originating parties to pre-register the ICE subprotocols they support in an
ICE_PROTOCOLS property on their top-level window.  Clients willing to
act as ICE answering parties then send an ICE_PROTOCOLS X
.PN ClientMessage
event to the ICE originating parties.  This
.PN ClientMessage
event identifies
the ICE network IDs of the ICE answering party as well as the ICE
subprotocol it wishes to speak.  Upon receipt of this message the ICE
originating party uses the information to establish an ICE connection
with the ICE answering party.
.nH 2 "Registering Known Protocols"
.LP
Clients willing to act as ICE originating parties preregister
the ICE subprotocols they support in a list of atoms held by an
ICE_PROTOCOLS property on their top-level window.  The name of each
atom listed in ICE_PROTOCOLS must be of the form
ICE_INITIATE_\fIpname\fP where \fIpname\fP is the name of the ICE
subprotocol the ICE originating party is willing to speak, as would be
specified in an ICE
.PN ProtocolSetup
message.
.LP
Clients with an ICE_INITIATE_\fIpname\fP atom in the ICE_PROTOCOLS property
on their top-level windows must respond to
.PN ClientMessage
events of
type ICE_PROTOCOLS specifying ICE_INITIATE_\fIpname\fP.  If a client does not
want to respond to these client message events, it should
remove the ICE_INITIATE_\fIpname\fP atom from its ICE_PROTOCOLS property
or remove the ICE_PROTOCOLS property entirely.
.nH 2 "Initiating the Rendezvous"
.LP
To initiate the rendezvous a client acting as an ICE answering
party sends an X
.PN ClientMessage
event of type ICE_PROTOCOLS to an ICE
originating party.  This ICE_PROTOCOLS client message contains the
information the ICE originating party needs to identify the ICE
subprotocol the two parties will use as well as the ICE network
identification string of the ICE answering party.
.LP
Before the ICE answering party sends the client message event it must
define a text property on one of its windows.  This text property
contains the ICE answering party's ICE network identification string
and will be used by ICE originating parties to determine the ICE
answering party's list of ICE network IDs.
.LP
The property name will normally be ICE_NETWORK_IDS, but may be any
name of the ICE answering party's choosing.  The format for this text
property is as follows:
.ne 7
.TS
lB lB
lw(1.25i) lw(4i) .
_
.sp 6p
Field	Value
.sp 6p
_
.sp 6p
type	XA_STRING
format	8
value	comma-separated list of ICE network IDs
.sp 6p
_
.TE
.LP
Once the ICE answering party has established this text property on one
of its windows, it initiates the rendezvous by sending an
ICE_PROTOCOLS
.PN ClientMessage
event to an ICE originating party's
top-level window.  This event has the following format
and must only be sent to windows that have pre-registered the ICE
subprotocol in an ICE_PROTOCOLS property on their top-level window.
.ne 13
.TS
lB lB
lw(1.25i) lw(4i) .
_
.sp 6p
Field	Value
.sp 6p
_
.sp 6p
message_type	Atom = "ICE_PROTOCOLS"
format	32
data.l[0]	Atom identifying the ICE subprotocol to speak
data.l[1]	Timestamp
data.l[2]	T{
ICE answering party's window ID with
ICE network IDs text property
T}
data.l[3]	T{
Atom naming text property containing the ICE 
answering party's ICE network IDs
T}
data.l[4]	Reserved.  Must be 0.
.sp 6p
_
.TE
The name of the atom in data.l[0] must be of the form
ICE_INITIATE_\fIpname\fP, where \fIpname\fP is the name of the ICE
subprotocol the ICE answering party wishes to speak.
.LP
When an ICE originating party receives a
.PN ClientMessage
event of type
ICE_PROTOCOLS specifying ICE_INITIATE_\fIpname\fP it can initiate an ICE
connection with the ICE answering party.
To open this connection the client retrieves the ICE answering
party's ICE network IDs from the window specified in data.l[2] using
the text property specified in data.l[3].
.LP
If the connection attempt fails for any reason, the client must
respond to the client message event by sending a return
.PN ClientMessage
event to the window specified in data.l[2].  This return
event has the following format:
.ne 13
.TS
lB lB
lw(1.25i) lw(4i) .
_
.sp 6p
Field	Value
.sp 6p
_
.sp 6p
message_type	Atom = "ICE_INITIATE_FAILED"			
format	32
data.l[0]	Atom identifying the ICE subprotocol requested	
data.l[1]	Timestamp
data.l[2]	T{
Initiating party's window ID 
(holding ICE_PROTOCOLS)
T}
data.l[3]	int: reason for failure				
data.l[4]	Reserved, must be 0
.sp 6p
_
.TE
The values of data.l[0] and data.l[1] are copied directly from the
client message event the client received.
.LP
The value in data.l[2] is
the id of the window to which the ICE_PROTOCOLS.ICE_INITIATE_\fIpname\fP
client message event was sent.
.LP
Data.l[3] has one of the following values:
.LP
.ne 21
.TS
lB cBw(0.6i) lB
l n lw(4i) .
_
.sp 6p
Value	Encoding	Description
.sp 6p
_
.sp 6p
T{
.PN OpenFailed
T}	1	T{
The client was unable to open the connection
(e.g. a call to IceOpenConnection() failed).  If the
client is able to distinguish authentication or
authorization errors from general errors, then
the preferred reply is 
.PN AuthenticationFailed
for authorization errors.
T}
.sp 4p
T{
.PN AuthenticationFailed
T}	2	T{
Authentication or authorization of the
connection or protocol setup was refused.
This reply will be given only if the client is 
able to distinguish it from
.PN OpenFailed ;
otherwise
.PN OpenFailed
will be returned.		
T}
.sp 4p
T{
.PN SetupFailed
T}	3	T{
The client was unable to initiate the specified
protocol on the connection (e.g. a call to
IceProtocolSetup() failed).			
T}
.sp 4p
T{
.PN UnknownProtocol
T}	4	T{
The client does not recognize the requested
protocol.  (This represents a semantic error
on the part of the answering party.)		
T}
.sp 4p
T{
.PN Refused
T}	5	T{
The client was in the process of removing
ICE_INITIATE_\fIpname\fP from its ICE_PROTOCOLS list 
when the client message was sent; the client no
longer is willing to establish the specified ICE 
communication.
T}
.sp 6p
_
.TE
.sp
.NT "Advice to Implementors"
Clients willing to act as ICE originating parties must update the
ICE_PROTOCOLS property on their top-level windows to include the
ICE_INITIATE_\fIpname\fP atom(s) identifying the ICE subprotocols they
speak.  The method a client uses to update the ICE_PROTOCOLS property
to include ICE_INITIATE_\fIpname\fP atoms is implementation dependent, but
the client must ensure the integrity of the list to prevent the
accidental omission of any atoms previously in the list.
.LP
When setting up the ICE network IDs text property on one of its
windows, the ICE answering party can determine its comma-separated
list of ICE network IDs by calling IceComposeNetworkIdList() after
making a call to IceListenForConnections().  The method an ICE
answering party uses to find the top-level windows of clients willing
to act as ICE originating parties is dependent upon the nature of the
answering party.  Some may wish to use the approach of requiring the
user to click on a client's window.  Others wishing to find existing
clients without requiring user interaction might use something similar
to the XQueryTree() method used by several freely-available
applications.  In order for the ICE answering party to become
automatically aware of new clients willing to originate ICE
connections, the ICE answering party might register for
SubstructureNotify events on the root window of the display.  When it
receives a SubstructureNotify event, the ICE answering party can check
to see if it was the result of the creation of a new client top-level
window with an ICE_PROTOCOLS property.
.LP
In any case, before attempting to use this ICE X Rendezvous Mechanism
ICE answering parties wishing to speak ICE subprotocol \fIpname\fP should
check for the ICE_INITIATE_\fIpname\fP atom in the ICE_PROTOCOLS property on
a client's top-level window.  A client that does not include an
ICE_INITIATE_\fIpname\fP atom in a ICE_PROTOCOLS property on some top-level
window should be assumed to ignore
.PN ClientMessage
events of type
ICE_PROTOCOLS specifying ICE_INITIATE_\fIpname\fP for ICE subprotocol
\fIpname\fP.
.NE
.nH 2 "ICE Subprotocol Versioning"
.LP
Although the version of the ICE subprotocol could be passed in the
client message event, ICE provides more a flexible version negotiation
mechanism than will fit within a single
.PN ClientMessage
event.  Because
of this, ICE subprotocol versioning is handled within the ICE protocol
setup phase.
.NT Example
Clients wish to communicate with each other via an ICE subprotocol
known as "RAP V1.0".  In RAP terminology one party, the "agent",
communicates with other RAP-enabled applications on demand.  The
user may direct the agent to establish communication with a specific
application by clicking on the application's window, or the agent may
watch for new application windows to be created and automatically
establish communication.
.LP
During startup the ICE answering party (the agent) first calls
IceRegisterForProtocolReply() with a list of
the versions (i.e., 1.0) of RAP the agent can speak.  The answering
party then calls IceListenForConnections() followed by
IceComposeNetworkIdList() and stores the resulting ICE network IDs
string in a text property on one of its windows.
.LP
When the answering party (agent) finds a client with which it wishes to
speak, it checks to see if the ICE_INITIATE_RAP atom is in the ICE_PROTOCOLS
property on the client's top-level window.  If it is present the agent
sends the client's top-level window an ICE_PROTOCOLS client
message event as described above.  When the client receives the client
message event and is willing to originate an ICE connection using RAP,
it performs an IceRegisterForProtocolSetup() with a list of the
versions of RAP the client can speak.  The client then retrieves
the agent's ICE network ID from the property and window specified by
the agent in the client message event and calls IceOpenConnection().
After this call succeeds the client calls IceProtocolSetup() specifying
the RAP protocol.  During this
process, ICE calls the RAP protocol routines that handle the version
negotiation.
.LP
Note that it is not necessary for purposes of this rendezvous that
the client application call any ICElib functions prior to receipt
of the client message event.
.NE
.YZ 1
