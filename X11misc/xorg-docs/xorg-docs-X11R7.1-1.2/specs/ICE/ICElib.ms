.\" $Xorg: ICElib.ms,v 1.3 2000/08/17 19:42:09 cpqbld Exp $
.\" $XdotOrg: doc/xorg-docs/specs/ICE/ICElib.ms,v 1.3 2004/09/03 16:18:19 kem Exp $
.\" 
.\" Use tbl, -ms, and macros.t
.\" 
.\" macro: start marker
.de sM
.ne 4
.sp 1
\\h'-0.3i'\\L'-1v'\\v'3p'\\l'1v'\\v'1v-3p'
.sp -1
..
.\" macro: end marker
.de eM
.sp -1
\\h'-0.3i'\\L'-1v'\\v'1v+4p'\\l'1v'\\v'-4p'
.sp 1
..
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ad b
.sp 10
.TL
\s+2\fBInter-Client Exchange Library\fP\s-2
.sp
Version 1.0
.sp
X Consortium Standard
.sp
X Version 11, Release 6.8
.AU
Ralph Mor
.AI
X Consortium
.LP
.DS C
Copyright \(co 1993, 1994, 1996 X Consortium
.DE
.LP
.sp 5
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
.bp
.EH '\fBInter-Client Exchange Library\fP''\fBX11, Release 6.8\fP'
.OH '\fBInter-Client Exchange Library\fP''\fBX11, Release 6.8\fP'
.bp 1
.EF ''\- \\\\n(PN \-''
.OF ''\- \\\\n(PN \-''
.NH 1
Overview of ICE
.XS
\*(SN Overview of ICE
.XE
.LP
There are numerous possible inter-client protocols, with many similarities
and common needs - authentication, version negotiation, byte
order negotiation, and so on.
The Inter-Client Exchange (ICE) protocol is intended to provide a framework
for building such protocols, allowing them to make use of common negotiation
mechanisms and to be multiplexed over a single transport connection.
.NH 1
The ICE Library - C Language Interface to ICE
.XS
\*(SN The ICE Library - C Language Interface to ICE
.XE
.LP
A client that wishes to utilize ICE must first register the protocols it
understands with the ICE library.  Each protocol is dynamically assigned
a major opcode ranging from 1-255 (two clients can use different
major opcodes for the same protocol).  The next step for the client is either
to open a connection with another client or to wait for connections made
by other clients.  Authentication may be required.  A client can both
initiate connections with other clients and be
waiting for clients to connect to itself (a nested session manager is an
example).  Once an ICE connection is established between the two clients, one
of the clients needs to initiate a 
.PN ProtocolSetup
in order to
"activate" a given protocol.  Once the other client accepts the
.PN ProtocolSetup
(once again, authentication may be required), the
two clients are ready to start passing messages specific to that protocol to
each other.  Multiple protocols may be active on a single ICE connection.
Clients are responsible for notifying the ICE library when a protocol is no
longer active on an ICE connection, although ICE does not define how each
subprotocol triggers a protocol shutdown.
.LP
The ICE library utilizes callbacks to process incoming messages.  Using
callbacks allows
.PN ProtocolSetup
messages and authentication to happen
behind the scenes.  An additional benefit is that messages never need
to be buffered up by the library when the client blocks waiting for a
particular message.
.NH 1
Intended Audience
.XS
\*(SN Intended Audience
.XE
.LP
This document is intended primarily for implementors of protocol libraries
layered on top of ICE.  Typically, applications that wish to utilize ICE
will make calls into individual protocol libraries rather than directly
make calls into the ICE library.  However, some applications will have to
make some initial calls into the ICE library in order to accept ICE
connections (for example, a session manager accepting connections from
clients).  But in general, protocol libraries should be designed to hide
the inner details of ICE from applications.
.NH 1
Header Files and Library Name
.XS
\*(SN Header Files and Library Name
.XE
.LP
The header file
.Pn < X11/ICE/ICElib.h >
defines all of the ICElib data structures and function prototypes.
.PN ICElib.h
includes the header file
.Pn < X11/ICE/ICE.h >,
which defines all of the ICElib constants.
Protocol libraries that need to read and write messages should include
the header file
.Pn < X11/ICE/ICEmsg.h >.
.LP
Applications should link against ICElib using -lICE.
.NH 1
Note on Prefixes
.XS
\*(SN Note on Prefixes
.XE
.LP
The following name prefixes are used in the library to distinguish between
a client that initiates a
.PN ProtocolSetup
and a client that
responds with a 
.PN ProtocolReply :
.IP \(bu 5
.PN IcePo
\- Ice Protocol Originator
.IP \(bu 5
.PN IcePa
\- Ice Protocol Acceptor
.NH 1
Protocol Registration
.XS
\*(SN Protocol Registration
.XE
.LP
In order for two clients to exchange messages for a given protocol, each
side must register the protocol with the ICE library.  The purpose of
registration is for each side to obtain a major opcode for the protocol
and to provide callbacks for processing messages and handling authentication.
There are two separate registration functions:
.IP \(bu 5
One to handle the side that does a
.PN ProtocolSetup
.IP \(bu 5
One to handle the side that responds with a
.PN ProtocolReply
.LP
It is recommended that protocol registration occur before the two clients
establish an ICE connection.  If protocol registration occurs after an
ICE connection is created, there can be a brief interval of time in which
a
.PN ProtocolSetup
is received, but the protocol is not registered.
If it is not possible to register a protocol before the creation of an
ICE connection, proper precautions should be taken to avoid the above race
condition.
.sp
.LP
The
.PN IceRegisterForProtocolSetup
function should be called for the client that initiates a
.PN ProtocolSetup .
.sM
.FD 0
int IceRegisterForProtocolSetup\^(\^\fIprotocol_name\fP, \fIvendor\fP\^, \
\fIrelease\fP\^, \fIversion_count\fP\^, \fIversion_recs\fP\^,
.br
                 \fIauth_count\fP\^, \fIauth_names\fP\^, \fIauth_procs\fP\^, \
\fIio_error_proc\fP\^)
.br
     char *\fIprotocol_name\fP\^;
.br
     char *\fIvendor\fP\^;
.br
     char *\fIrelease\fP\^;
.br
     int \fIversion_count\fP\^;
.br
     IcePoVersionRec *\fIversion_recs\fP\^;
.br
     int \fIauth_count\fP\^;
.br
     char **\fIauth_names\fP\^;
.br
     IcePoAuthProc *\fIauth_procs\fP\^;
.br
     IceIOErrorProc \fIio_error_proc\fP\^;
.FN
.IP \fIprotocol_name\fP 1i
A string specifying the name of the protocol to register.
.IP \fIvendor\fP 1i
A vendor string with semantics specified by the protocol.
.IP \fIrelease\fP 1i
A release string with semantics specified by the protocol.
.IP \fIversion_count\fP 1i
The number of different versions of the protocol supported.
.IP \fIversion_recs\fP 1i
List of versions and associated callbacks.
.IP \fIauth_count\fP 1i
The number of authentication methods supported.
.IP \fIauth_names\fP 1i
The list of authentication methods supported.
.IP \fIauth_procs\fP 1i
The list of authentication callbacks, one for each authentication method.
.IP \fIio_error_proc\fP 1i
IO error handler, or NULL.
.LP
.eM
.PN IceRegisterForProtocolSetup
returns the major opcode reserved or -1 if an error occurred.  In order
to actually activate the protocol, the
.PN IceProtocolSetup
function needs to be called with this major opcode.  Once the protocol is
activated, all messages for the protocol should be sent using this major
opcode.
.LP
A protocol library may support multiple versions of the same protocol.
The version_recs argument specifies a list of supported versions of the protocol,
which are prioritized in decreasing order of preference.
Each version record consists of a major and minor version of the protocol
as well as a callback to be used for processing incoming messages.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	int major_version;
	int minor_version;
	IcePoProcessMsgProc process_msg_proc;
} IcePoVersionRec;
.De
.LP
.eM
The
.PN IcePoProcessMsgProc
callback is responsible for processing the set of messages that can be
received by the client that initiated the
.PN ProtocolSetup .
For further information,
see section 6.1, ``Callbacks for Processing Messages.''
.LP
Authentication may be required before the protocol can become active.
The protocol library must register the authentication methods that it
supports with the ICE library.
The auth_names and auth_procs arguments are a list of authentication names
and callbacks that are  prioritized in decreasing order of preference.
For information on the
.PN IcePoAuthProc
callback, see section 6.2, ``Authentication Methods.''
.LP
The
.PN IceIOErrorProc
callback is invoked if the ICE connection unexpectedly breaks.
You should pass NULL for io_error_proc if not interested in being notified.
For further information,
see section 13, ``Error Handling.''
.sp
.LP
The
.PN IceRegisterForProtocolReply
function should be called for the client that responds to a
.PN ProtocolSetup
with a
.PN ProtocolReply .
.sM
.FD 0
int IceRegisterForProtocolReply\^(\^\fIprotocol_name\fP, \fIvendor\fP\^, \fIrelease\fP\^, \fIversion_count\fP\^, \fIversion_recs\fP\^,
.br
                    \fIauth_count\fP\^, \fIauth_names\fP\^, \fIauth_procs\fP\^, \fIhost_based_auth_proc\fP\^, \fIprotocol_setup_proc\fP\^,
.br
                     \fIprotocol_activate_proc\fP\^, \fIio_error_proc\fP\^)
.br
     char *\fIprotocol_name\fP\^;
.br
     char *\fIvendor\fP\^;
.br
     char *\fIrelease\fP\^;
.br
     int \fIversion_count\fP\^;
.br
     IcePaVersionRec *\fIversion_recs\fP\^;
.br
     int \fIauth_count\fP\^;
.br
     char **\fIauth_names\fP\^;
.br
     IcePaAuthProc *\fIauth_procs\fP\^;
.br
     IceHostBasedAuthProc \fIhost_based_auth_proc\fP\^;
.br
     IceProtocolSetupProc \fIprotocol_setup_proc\fP\^;
.br
     IceProtocolActivateProc \fIprotocol_activate_proc\fP\^;
.br
     IceIOErrorProc \fIio_error_proc\fP\^;
.FN
.IP \fIprotocol_name\fP 1i
A string specifying the name of the protocol to register.
.IP \fIvendor\fP 1i
A vendor string with semantics specified by the protocol.
.IP \fIrelease\fP 1i
A release string with semantics specified by the protocol.
.IP \fIversion_count\fP 1i
The number of different versions of the protocol supported.
.IP \fIversion_recs\fP 1i
List of versions and associated callbacks.
.IP \fIauth_count\fP 1i
The number of authentication methods supported.
.IP \fIauth_names\fP 1i
The list of authentication methods supported.
.IP \fIauth_procs\fP 1i
The list of authentication callbacks, one for each authentication method.
.IP \fIhost_based_auth_proc\fP 1i
Host based authentication callback.
.IP \fIprotocol_setup_proc\fP 1i
A callback to be invoked when authentication has succeeded for a
.PN ProtocolSetup
but before the
.PN ProtocolReply
is sent.
.IP \fIprotocol_activate_proc\fP 1i
A callback to be invoked after the
.PN ProtocolReply
is sent.
.IP \fIio_error_proc\fP 1i
IO error handler, or NULL.
.LP
.eM
.PN IceRegisterForProtocolReply
returns the major opcode reserved or -1 if an error occurred.  The major
opcode should be used in all subsequent messages sent for this protocol.
.LP
A protocol library may support multiple versions of the same protocol.
The version_recs argument specifies a list of supported versions of the protocol,
which are prioritized in decreasing order of preference.
Each version record consists of a major and minor version of the protocol
as well as a callback to be used for processing incoming messages.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	int major_version;
	int minor_version;
	IcePaProcessMsgProc process_msg_proc;
} IcePaVersionRec;
.De
.LP
.eM
The
.PN IcePaProcessMsgProc
callback is responsible for processing the set of messages that can be
received by the client that accepted the
.PN ProtocolSetup .
For further information,
see section 6.1, ``Callbacks for Processing Messages.''
.LP
Authentication may be required before the protocol can become active.
The protocol library must register the authentication methods that it
supports with the ICE library.
The auth_names and auth_procs arguments are a list of authentication names
and callbacks that are prioritized in decreasing order of preference.
For information on the
.PN IcePaAuthProc
callback, see section 6.2, ``Authentication Methods.''
.LP
If authentication fails and the client attempting to initiate
the
.PN ProtocolSetup
has not required authentication, the
.PN IceHostBasedAuthProc
callback is invoked with the host name of the originating client.
If the callback returns
.PN True ,
the
.PN ProtocolSetup
will succeed, even though the original
authentication failed.
Note that authentication can effectively be disabled by registering an
.PN IceHostBasedAuthProc ,
which always returns
.PN True .
If no host based
authentication is allowed, you should pass NULL for host_based_auth_proc.
.LP
.sM
.FD 0
typedef Bool (*IceHostBasedAuthProc) ();

Bool HostBasedAuthProc\^(\^\fIhost_name\fP\^)
.br
    char *\fIhost_name\fP\^;
.FN
.IP \fIhost_name\fP 1i
The host name of the client that sent the
.PN ProtocolSetup .
.LP
.eM
The host_name argument is a string of the form \fIprotocol\fP\^/\^\fIhostname\fP,
where \fIprotocol\fP\^ is one of {tcp, decnet, local}.
.LP
Because
.PN ProtocolSetup
messages and authentication happen behind the scenes
via callbacks, the protocol library needs some way of being notified when the
.PN ProtocolSetup
has completed.
This occurs in two phases.
In the first phase, the
.PN IceProtocolSetupProc
callback is invoked after authentication has
successfully completed but before the ICE library sends a
.PN ProtocolReply .
Any resources required for this protocol should be allocated at this time.
If the
.PN IceProtocolSetupProc
returns a successful status, the ICE library will
send the
.PN ProtocolReply
and then invoke the
.PN IceProtocolActivateProc
callback.  Otherwise, an error will be sent to the
other client in response to the
.PN ProtocolSetup .
.LP
The
.PN IceProtocolActivateProc
is an optional callback and should be registered only if the protocol
library intends to generate a message immediately following the
.PN ProtocolReply .
You should pass NULL for protocol_activate_proc if not interested
in this callback.
.if t .bp
.sM
.FD 0
typedef Status (*IceProtocolSetupProc) ();

Status ProtocolSetupProc\^(\^\fIice_conn\fP, \fImajor_version\fP\^, \
\fIminor_version\fP\^, \fIvendor\fP\^, \fIrelease\fP\^, 
.br
                    \fIclient_data_ret\fP\^, \fIfailure_reason_ret\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fImajor_version\fP\^;
.br
     int \fIminor_version\fP\^;
.br
     char *\fIvendor\fP\^;
.br
     char *\fIrelease\fP\^;
.br
     IcePointer *\fIclient_data_ret\fP\^;
.br
     char **\fIfailure_reason_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fImajor_version\fP 1i
The major version of the protocol.
.IP \fIminor_version\fP 1i
The minor version of the protocol.
.IP \fIvendor\fP 1i
The vendor string registered by the protocol originator.
.IP \fIrelease\fP 1i
The release string registered by the protocol originator.
.IP \fIclient_data_ret\fP 1i
Client data to be set by callback.
.IP \fIfailure_reason_ret\fP 1i
Failure reason returned.
.LP
.eM
The pointer stored in the client_data_ret argument will be passed
to the
.PN IcePaProcessMsgProc
callback whenever a message has arrived for this protocol on the
ICE connection.
.LP
The vendor and release strings should be freed with
.PN free
when they are no longer needed.
.LP
If a failure occurs, the
.PN IceProtocolSetupProc
should return a zero status as well as allocate and return a failure
reason string in failure_reason_ret.
The ICE library will be responsible for freeing this memory.
.LP
The
.PN IceProtocolActivateProc
callback is defined as follows:
.sM
.FD 0
typedef void (*IceProtocolActivateProc)();

void ProtocolActivateProc\^(\^\fIice_conn\fP, \fIclient_data\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIclient_data\fP 1i
The client data set in the
.PN IceProtocolSetupProc
callback.
.LP
.eM
The
.PN IceIOErrorProc
callback is invoked if the ICE connection unexpectedly breaks.
You should pass NULL for io_error_proc if not interested in being notified.
For further information,
see section 13, ``Error Handling.''
.NH 2
Callbacks for Processing Messages
.XS
\*(SN Callbacks for Processing Messages
.XE
.LP
When an application detects that there is new data to read on an ICE
connection (via 
.PN select ),
it calls the
.PN IceProcessMessages
function (see section 9, ``Processing Messages'').
When
.PN IceProcessMessages
reads an ICE message header with a major opcode other than
zero (reserved for the ICE protocol), it needs to call a function that will
read the rest of the message, unpack it, and process it accordingly.
.LP
If the message arrives at the client that initiated the
.PN ProtocolSetup ,
the
.PN IcePoProcessMsgProc
callback is invoked.
.sM
.FD 0
typedef void (*IcePoProcessMsgProc)();

void PoProcessMsgProc\^(\^\fIice_conn\fP, \fIclient_data\fP\^, \fIopcode\fP\^, \fIlength\fP\^, \fIswap\fP\^, \fIreply_wait\fP\^, \fIreply_ready_ret\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.br
    int \fIopcode\fP\^;
.br
    unsigned long \fIlength\fP\^;
.br
    Bool \fIswap\fP\^;
.br
    IceReplyWaitInfo *\fIreply_wait\fP\^;
.br
    Bool *\fIreply_ready_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIclient_data\fP 1i
Client data associated with this protocol on the ICE connection.
.IP \fIopcode\fP 1i
The minor opcode of the message.
.IP \fIlength\fP 1i
The length (in 8-byte units) of the message beyond the ICE header.
.IP \fIswap\fP 1i
A flag that indicates if byte swapping is necessary.
.IP \fIreply_wait\fP 1i
Indicates if the invoking client is waiting for a reply.
.IP \fIreply_ready_ret\fP 1i
If set to
.PN True ,
a reply is ready.
.LP
.eM
If the message arrives at the client that accepted the
.PN ProtocolSetup ,
the
.PN IcePaProcessMsgProc
callback is invoked.
.sM
.FD 0
typedef void (*IcePaProcessMsgProc)();

void PaProcessMsgProc\^(\^\fIice_conn\fP, \fIclient_data\fP\^, \fIopcode\fP\^, \fIlength\fP\^, \fIswap\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.br
    int \fIopcode\fP\^;
.br
    unsigned long \fIlength\fP\^;
.br
    Bool \fIswap\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIclient_data\fP 1i
Client data associated with this protocol on the ICE connection.
.IP \fIopcode\fP 1i
The minor opcode of the message.
.IP \fIlength\fP 1i
The length (in 8-byte units) of the message beyond the ICE header.
.IP \fIswap\fP 1i
A flag that indicates if byte swapping is necessary.
.LP
.eM
In order to read the message, both of these callbacks should use the
macros defined for this purpose (see section 12.2, ``Reading ICE Messages'').
Note that byte swapping may be necessary.
As a convenience, the length field in the ICE header will be swapped by ICElib
if necessary.
.LP
In both of these callbacks, the client_data argument is a pointer to client
data that was registered at
.PN ProtocolSetup
time.
In the case of
.PN IcePoProcessMsgProc ,
the client data was set in the call to
.PN IceProtocolSetup .
In the case of
.PN IcePaProcessMsgProc ,
the client data was set in the
.PN IceProtocolSetupProc
callback.
.LP
The
.PN IcePoProcessMsgProc
callback needs to check the reply_wait argument.
If reply_wait is NULL ,
the ICE library expects the function to
pass the message to the client via a callback.
For example, if this is a Session Management ``Save Yourself'' message,
this function should notify the client of the ``Save Yourself'' via a callback.
The details of how such a callback would be defined
are implementation-dependent.
.LP
However, if reply_wait is not NULL ,
then the client is waiting for
a reply or an error for a message it previously sent.
The reply_wait is of type
.PN IceReplyWaitInfo .
.sM
.Ds 0
.TA .5i 2.5i
.ta .5i 2.5i
typedef struct {
	unsigned long sequence_of_request;
	int major_opcode_of_request;
	int minor_opcode_of_request;
	IcePointer reply;
} IceReplyWaitInfo;
.De
.LP
.eM
.PN IceReplyWaitInfo
contains the major/minor opcodes and sequence number of
the message for which a reply is being awaited.
It also contains a pointer to the reply message to be filled in
(the protocol library should cast this
.PN IcePointer
to the appropriate reply type).
In most cases, the reply will have some fixed-size part, and the client waiting
for the reply will have provided a pointer to a structure to hold
this fixed-size data.  If there is variable-length data, it would be
expected that the
.PN IcePoProcessMsgProc
callback will have to allocate additional
memory and store pointer(s) to that memory in the fixed-size
structure.  If the entire data is variable length (for example., a single
variable-length string), then the client waiting for the reply would probably
just pass a pointer to fixed-size space to hold a pointer, and the
.PN IcePoProcessMsgProc
callback would allocate the storage and store the pointer.
It is the responsibility of the client receiving the reply to
free any memory allocated on its behalf.
.LP
If reply_wait is not NULL and
.PN IcePoProcessMsgProc
has a reply or error to return in response to this reply_wait
(that is, no callback was generated), then the reply_ready_ret argument
should be set to
.PN True .
Note that an error should only be returned
if it corresponds to the reply being waited for.  Otherwise, the
.PN IcePoProcessMsgProc
should either handle the error internally or invoke an error handler
for its library.
.LP
If reply_wait is NULL,
then care must be taken not to store any value in reply_ready_ret,
because this pointer may also be NULL.
.LP
The
.PN IcePaProcessMsgProc
callback, on the other hand, should always pass
the message to the client via a callback.  For example, if this is a Session
Management ``Interact Request'' message, this function should notify the
client of the ``Interact Request'' via a callback.
.LP
The reason the
.PN IcePaProcessMsgProc
callback does not have a reply_wait, like
.PN IcePoProcessMsgProc
does, is because a process that is acting as
a server should never block for a reply (infinite blocking can
occur if the connecting client does not act properly, denying access
to other clients).
.NH 2
Authentication Methods
.XS
\*(SN Authentication Methods
.XE
.LP
As already stated, a protocol library must register the authentication
methods that it supports with the ICE library.  For each authentication
method, there are two callbacks that may be registered:
.IP \(bu 5
One to handle the side that initiates a
.PN ProtocolSetup
.IP \(bu 5
One to handle the side that accepts or rejects this request
.LP
.PN IcePoAuthProc
is the callback invoked for the client that initiated the
.PN ProtocolSetup .
This callback must be able to respond
to the initial ``Authentication Required'' message or subsequent
``Authentication Next Phase'' messages sent by the other client.
.if t .bp
.sM
.FD 0
typedef IcePoAuthStatus (*IcePoAuthProc)();

IcePoAuthStatus PoAuthProc\^(\^\fIice_conn\fP, \fIauth_state_ptr\fP\^, \fIclean_up\fP\^, \fIswap\fP\^, \fIauth_datalen\fP\^, \fIauth_data\fP\^,
.br
                    \fIreply_datalen_ret\fP\^, \fIreply_data_ret\fP\^, \fIerror_string_ret\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer *\fIauth_state_ptr\fP\^;
.br
    Bool \fIclean_up\fP\^;
.br
    Bool \fIswap\fP\^;
.br
    int \fIauth_datalen\fP\^;
.br
    IcePointer \fIauth_data\fP\^;
.br
    int *\fIreply_datalen_ret\fP\^;
.br
    IcePointer *\fIreply_data_ret\fP\^;
.br
    char **\fIerror_string_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIauth_state_ptr\fP 1i
A pointer to state for use by the authentication callback procedure.
.IP \fIclean_up\fP 1i
If
.PN True ,
authentication is over, and the function
should clean up any state it was maintaining.  The
last 6 arguments should be ignored.
.IP \fIswap\fP 1i
If
.PN True ,
the auth_data may have to be byte swapped
(depending on its contents).
.IP \fIauth_datalen\fP 1i
The length (in bytes) of the authenticator data.
.IP \fIauth_data\fP 1i
The data from the authenticator.
.IP \fIreply_datalen_ret\fP 1i
The length (in bytes) of the reply data returned.
.IP \fIreply_data_ret\fP 1i
The reply data returned.
.IP \fIerror_string_ret\fP 1i
If the authentication procedure encounters an error during
authentication, it should allocate and return
an error string.
.LP
.eM
Authentication may require several phases, depending on the authentication
method.  As a result, the
.PN IcePoAuthProc
may be called more than once when authenticating a client, and
some state will have to be maintained between each invocation.
At the start of each
.PN ProtocolSetup ,
*auth_state_ptr is NULL,
and the function should initialize its state and set
this pointer.  In subsequent invocations of the callback, the pointer
should be used to get at any state previously stored by the callback.
.LP
If needed, the network ID of the client accepting the
.PN ProtocolSetup
can be obtained by calling the
.PN IceConnectionString
function.
.LP
ICElib will be responsible for freeing the reply_data_ret and
error_string_ret pointers with
.PN free .
.LP
The auth_data pointer may point to a volatile block of memory.
If the data must be kept beyond this invocation of the callback, be sure
to make a copy of it.
.LP
The
.PN IcePoAuthProc
should return one of four values:
.IP \(bu 5
.PN IcePoAuthHaveReply
\- a reply is available.
.IP \(bu 5
.PN IcePoAuthRejected
\- authentication rejected.
.IP \(bu 5
.PN IcePoAuthFailed
\- authentication failed.
.IP \(bu 5
.PN IcePoAuthDoneCleanup
\- done cleaning up.
.LP
.PN IcePaAuthProc
is the callback invoked for the client that received the
.PN ProtocolSetup .
.if t .bp
.sM
.FD 0
typedef IcePaAuthStatus (*IcePaAuthProc) ();

IcePaAuthStatus PaAuthProc\^(\^\fIice_conn\fP, \fIauth_state_ptr\fP\^, \fIswap\fP\^, \fIauth_datalen\fP\^, \fIauth_data\fP\^, 
.br
                    \fIreply_datalen_ret\fP\^, \fIreply_data_ret\fP\^, \fIerror_string_ret\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer *\fIauth_state_ptr\fP\^;
.br
    Bool \fIswap\fP\^;
.br
    int \fIauth_datalen\fP\^;
.br
    IcePointer \fIauth_data\fP\^;
.br
    int *\fIreply_datalen_ret\fP\^;
.br
    IcePointer *\fIreply_data_ret\fP\^;
.br
    char **\fIerror_string_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIauth_state_ptr\fP 1i
A pointer to state for use by the authentication callback procedure.
.IP \fIswap\fP 1i
If
.PN True ,
auth_data may have to be byte swapped
(depending on its contents).
.IP \fIauth_datalen\fP 1i
The length (in bytes) of the protocol originator authentication data.
.IP \fIauth_data\fP 1i
The authentication data from the protocol originator.
.IP \fIreply_datalen_ret\fP 1i
The length of the authentication data returned.
.IP \fIreply_data_ret\fP 1i
The authentication data returned.
.IP \fIerror_string_ret\fP 1i
If authentication is rejected or fails, an error
string is returned.
.LP
.eM
.LP
Authentication may require several phases, depending on the authentication
method.  As a result, the
.PN IcePaAuthProc
may be called more than once when authenticating a client, and
some state will have to be maintained between each invocation.
At the start of each
.PN ProtocolSetup ,
auth_datalen is zero,
*auth_state_ptr is NULL,
and the function should initialize its state and set
this pointer.  In subsequent invocations of the callback, the pointer
should be used to get at any state previously stored by the callback.
.LP
If needed, the network ID of the client accepting the
.PN ProtocolSetup
can be obtained by calling the
.PN IceConnectionString
function.
.LP
The auth_data pointer may point to a volatile block of memory.
If the data must be kept beyond this invocation of the callback, be sure
to make a copy of it.
.LP
ICElib will be responsible for transmitting and freeing the reply_data_ret and
error_string_ret pointers with
.PN free .
.LP
The
.PN IcePaAuthProc
should return one of four values:
.IP \(bu 5
.PN IcePaAuthContinue
\- continue (or start) authentication.
.IP \(bu 5
.PN IcePaAuthAccepted
\- authentication accepted.
.IP \(bu 5
.PN IcePaAuthRejected
\- authentication rejected.
.IP \(bu 5
.PN IcePaAuthFailed
\- authentication failed.
.NH 1
ICE Connections
.XS
\*(SN ICE Connections
.XE
.LP
In order for two clients to establish an ICE connection, one client has
to be waiting for connections, and the other client has to initiate the
connection.
Most clients will initiate connections, so we discuss that first.
.NH 2
Opening an ICE Connection
.XS
\*(SN Opening an ICE Connection
.XE
.LP
To open an ICE connection with another client (that is, waiting
for connections), use
.PN IceOpenConnection .
.sM
.FD 0
IceConn IceOpenConnection\^(\^\fInetwork_ids_list\fP, \fIcontext\fP\^, \fImust_authenticate\fP\^, \fImajor_opcode_check\fP\^,
.br
                    \fIerror_length\fP\^, \fIerror_string_ret\fP\^)
.br
     char *\fInetwork_ids_list\fP\^;
.br
     IcePointer \fIcontext\fP\^;
.br
     Bool \fImust_authenticate\fP\^;
.br
     int \fImajor_opcode_check\fP\^;
.br
     int  \fIerror_length\fP\^;
.br
     char *\fIerror_string_ret\fP\^;
.FN
.IP \fInetwork_ids_list\fP 1i
Specifies the network ID(s) of the other client.
.IP \fIcontext\fP 1i
A pointer to an opaque object or NULL.  Used to determine if an
ICE connection can be shared (see below).
.IP \fImust_authenticate\fP 1i
If
.PN True ,
the other client may not bypass authentication.
.IP \fImajor_opcode_check\fP 1i
Used to force a new ICE connection to be created (see below).
.IP \fIerror_length\fP 1i
Length of the error_string_ret argument passed in.
.IP \fIerror_string_ret\fP 1i
Returns a null-terminated error message, if any. 
The error_string_ret argument points to user supplied memory.
No more than error_length bytes
are used.
.LP
.eM
.PN IceOpenConnection
returns an opaque ICE connection object if it succeeds;
otherwise, it returns NULL.
.LP
The network_ids_list argument contains a list of network IDs separated by commas.
An attempt will be made to use the first network ID.  If that fails,
an attempt will be made using the second network ID, and so on.
Each network ID has the following format:
.TS
lw(0.25i) lw(2.5i) lw(1i).
	tcp/<hostname>:<portnumber>	or
	decnet/<hostname>::<objname>	or
	local/<hostname>:<path>	
.TE
.LP
Most protocol libraries will have some sort of open function that should
internally make a call into
.PN IceOpenConnection .
When
.PN IceOpenConnection
is called, it may be possible to use a previously opened ICE connection (if
the target client is the same).  However, there are cases in which shared
ICE connections are not desired.
.LP
The context argument is used to determine if an ICE connection can
be shared.
If context is NULL,
then the caller is always willing to share the connection.
If context is not NULL,
then the caller is not willing to use a previously opened ICE connection
that has a different non-NULL context associated with it.
.LP
In addition, if major_opcode_check contains a nonzero major opcode value,
a previously created ICE connection will be used only if the major opcode
is not active on the connection.  This can be used to force multiple ICE
connections between two clients for the same protocol.
.LP
Any authentication requirements are handled internally by the ICE library.
The method by which the authentication data is obtained 
is implementation-dependent.\(dg
.FS \(dg
The X Consortium's ICElib implementation uses an \&.ICEauthority file (see
Appendix A).
.FE
.LP
After
.PN IceOpenConnection
is called, the client is ready to send a
.PN ProtocolSetup
(provided that
.PN IceRegisterForProtocolSetup
was called) or receive a
.PN ProtocolSetup
(provided that
.PN IceRegisterForProtocolReply
was called).
.NH 2
Listening for ICE Connections
.XS
\*(SN Listening for ICE Connections
.XE
.LP
Clients wishing to accept ICE connections must first call
.PN IceListenForConnections
or
.PN IceListenForWellKnownConnections
so that they can listen for connections.  A list of opaque "listen" objects are
returned, one for each type of transport method that is available
(for example, Unix Domain, TCP, DECnet, and so on).
.LP
Normally clients will let ICElib allocate an available name in each
transport and return listen objects.  Such a client will then use
.PN IceComposeNetworkIdList
to extract the chosen names and make them
available to other clients for opening the connection.  In certain
cases it may be necessary for a client to listen for connections
on pre-arranged transport object names.  Such a client may use
.PN IceListenForWellKnownConnections
to specify the names for the listen objects.
.sM
.FD 0
Status IceListenForConnections\^(\^\fIcount_ret\fP, \fIlisten_objs_ret\fP\^, \fIerror_length\fP\^, \fIerror_string_ret\fP\^)
.br
     int  *\fIcount_ret\fP\^;
.br
     IceListenObj **\fIlisten_objs_ret\fP\^;
.br
     int  \fIerror_length\fP\^;
.br
     char *\fIerror_string_ret\fP\^;
.FN
.IP \fIcount_ret\fP 1i
Returns the number of listen objects created.
.IP \fIlisten_objs_ret\fP 1i
Returns a list of pointers to opaque listen objects.
.IP \fIerror_length\fP 1i
The length of the error_string_ret argument passed in.
.IP \fIerror_string_ret\fP 1i
Returns a null-terminated error message, if any. 
The error_string_ret points to user supplied memory.
No more than error_length bytes are used.
.LP
.eM
The return value of
.PN IceListenForConnections
is zero for failure and a positive value for success.
.sp
.sM
.FD 0
Status IceListenForWellKnownConnections\^(\^\fIport_id\fP, \fIcount_ret\fP, \fIlisten_objs_ret\fP\^, \fIerror_length\fP\^, \fIerror_string_ret\fP\^)
.br
     char *\fIport_id\fP\^;
.br
     int  *\fIcount_ret\fP\^;
.br
     IceListenObj **\fIlisten_objs_ret\fP\^;
.br
     int  \fIerror_length\fP\^;
.br
     char *\fIerror_string_ret\fP\^;
.FN
.IP \fIport_id\fP 1i
Specifies the port identification for the address(es)
to be opened.  The value must not contain the slash
(\^``/''\^) or comma (\^``,''\^) character;
these are reserved for future use.
.IP \fIcount_ret\fP 1i
Returns the number of listen objects created.
.IP \fIlisten_objs_ret\fP 1i
Returns a list of pointers to opaque listen objects.
.IP \fIerror_length\fP 1i
The length of the error_string_ret argument passed in.
.IP \fIerror_string_ret\fP 1i
Returns a null-terminated error message, if any. 
The error_string_ret points to user supplied memory.
No more than error_length bytes are used.
.LP
.eM
.PN IceListenForWellKnownConnections
constructs a list of network IDs
by prepending each known transport to port_id and then attempts to
create listen objects for the result.  Port_id is the portnumber,
objname, or path portion of the ICE network ID. If a listen object for
a particular network ID cannot be created the network ID is ignored.
If no listen objects are created
.PN IceListenForWellKnownConnections
returns failure.
.LP
The return value of
.PN IceListenForWellKnownConnections
is zero for failure and a positive value for success.
.sp
.LP
To close and free the listen objects, use
.PN IceFreeListenObjs .
.LP
.sM
.FD 0
void IceFreeListenObjs\^(\^\fIcount\fP, \fIlisten_objs\fP\^)
.br
    int \fIcount\fP\^;
.br
    IceListenObj *\fIlisten_objs\fP\^;
.FN
.IP \fIcount\fP 1i
The number of listen objects.
.IP \fIlisten_objs\fP 1i
The listen objects.
.LP
.eM
.LP
To detect a new connection on a listen object, use
.PN select
on the descriptor associated with the listen object.
.sp
.LP
To obtain the descriptor, use
.PN IceGetListenConnectionNumber .
.LP
.sM
.FD 0
int IceGetListenConnectionNumber\^(\^\fIlisten_obj\fP\^)
.br
    IceListenObj \fIlisten_obj\fP\^;
.FN
.IP \fIlisten_obj\fP 1i
The listen object.
.LP
.eM
.LP
To obtain the network ID string associated with a listen object, use
.PN IceGetListenConnectionString .
.sM
.FD 0
char *IceGetListenConnectionString\^(\^\fIlisten_obj\fP\^)
.br
    IceListenObj \fIlisten_obj\fP\^;
.FN
.IP \fIlisten_obj\fP 1i
The listen object.
.LP
.eM
.LP
A network ID has the following format:
.TS
lw(0.25i) lw(2.5i) lw(1i).
	tcp/<hostname>:<portnumber>	or
	decnet/<hostname>::<objname>	or
	local/<hostname>:<path>	
.TE
.LP
To compose a string containing a list of network IDs separated by commas
(the format recognized by
.PN IceOpenConnection ),
use
.PN IceComposeNetworkIdList .
.LP
.sM
.FD 0
char *IceComposeNetworkIdList\^(\^\fIcount\fP, \fIlisten_objs\fP\^)
.br
    int \fIcount\fP\^;
.br
    IceListenObj *\fIlisten_objs\fP\^;
.FN
.IP \fIcount\fP 1i
The number of listen objects.
.IP \fIlisten_objs\fP 1i
The listen objects.
.LP
.eM
.NH 2
Host Based Authentication for ICE Connections
.XS
\*(SN Host Based Authentication for ICE Connections
.XE
.LP
If authentication fails when a client attempts to open an
ICE connection and the initiating client has not required authentication,
a host based authentication procedure may be invoked to provide
a last chance for the client to connect.  Each listen object has such a
callback associated with it, and this callback is set using the
.PN IceSetHostBasedAuthProc
function.
.sM
.FD 0
void IceSetHostBasedAuthProc\^(\^\fIlisten_obj\fP, \fIhost_based_auth_proc\fP\^)
.br
    IceListenObj \fIlisten_obj\fP\^;
.br
    IceHostBasedAuthProc \fIhost_based_auth_proc\fP\^;
.FN
.IP \fIlisten_obj\fP 1i
The listen object.
.IP \fIhost_based_auth_proc\fP 1i
The host based authentication procedure.
.LP
.eM
By default, each listen object has no host based authentication procedure
associated with it.
Passing NULL for host_based_auth_proc turns off host based authentication
if it was previously set.
.LP
.sM
.FD 0
typedef Bool (*IceHostBasedAuthProc) ();

Bool HostBasedAuthProc\^(\^\fIhost_name\fP\^)
.br
    char *\fIhost_name\fP\^;
.FN
.IP \fIhost_name\fP 1i
The host name of the client that tried to open an ICE connection.
.LP
.eM
The host_name argument is a string in the form \fIprotocol\fP\^/\^\fIhostname\fP,
where \fIprotocol\fP\^ is one of {tcp, decnet, local}.
.LP
If
.PN IceHostBasedAuthProc
returns
.PN True ,
access will be granted, even though the original
authentication failed.  Note that authentication can effectively be
disabled by registering an
.PN IceHostBasedAuthProc ,
which always returns
.PN True .
.LP
Host based authentication is also allowed at
.PN ProtocolSetup
time.
The callback is specified in the
.PN IceRegisterForProtocolReply
function (see section 6, ``Protocol Registration'').
.NH 2
Accepting ICE Connections
.XS
\*(SN Accepting ICE Connections
.XE
.LP
After a connection attempt is detected on a listen object returned by
.PN IceListenForConnections ,
you should call
.PN IceAcceptConnection .
This returns a new opaque ICE connection object.
.sM
.FD 0
IceConn IceAcceptConnection\^(\^\fIlisten_obj\fP, \fI\^status_ret\fP\^)
.br
    IceListenObj \fIlisten_obj\fP\^;
.br
    IceAcceptStatus *\fIstatus_ret\fP\^;
.FN
.IP \fIlisten_obj\fP 1i
The listen object on which a new connection was detected.
.IP \fIstatus_ret\fP 1i
Return status information.
.LP
.eM
The status_ret argument is set to one of the following values:
.IP \(bu 5
.PN IceAcceptSuccess
\- the accept operation succeeded,
and the function returns a new connection object.
.IP \(bu 5
.PN IceAcceptFailure
\- the accept operation failed, and the function returns NULL.
.IP \(bu 5
.PN IceAcceptBadMalloc
\- a memory allocation failed, and the function returns NULL.
.LP
In general, to detect new connections, you should call
.PN select
on the file descriptors associated with the listen objects.
When a new connection is detected, the
.PN IceAcceptConnection
function should be called.  
.PN IceAcceptConnection
may return a new ICE connection that is in a pending state.  This is because
before the connection can become valid, authentication may be necessary.
Because the ICE library cannot block and wait for the connection to
become valid (infinite blocking can occur if the connecting client
does not act properly), the application must wait for the connection status
to become valid.
.LP
The following pseudo-code demonstrates how connections are accepted:
.if t .bp
.LP
.Ds 0
.TA .5i 1i 1.5i 2i
.ta .5i 1i 1.5i 2i
new_ice_conn = IceAcceptConnection (listen_obj, &accept_status);
if (accept_status != IceAcceptSuccess)
{
	close the file descriptor and return
}

status = IceConnectionStatus (new_ice_conn);
time_start = time_now;

while (status == IceConnectPending)
{
	select() on {new_ice_conn, all open connections}

	for (each ice_conn in the list of open connections)
		if (data ready on ice_conn)
		{
			status = IceProcessMessages (ice_conn, NULL, NULL);
			if (status == IceProcessMessagesIOError)
				IceCloseConnection (ice_conn);
		}

	if (data ready on new_ice_conn)
	{
		/*
		 * IceProcessMessages is called until the connection
		 * is non-pending.  Doing so handles the connection
		 * setup request and any authentication requirements.
		 */

		IceProcessMessages (new_ice_conn, NULL, NULL);
		status = IceConnectionStatus (new_ice_conn);
	}
	else
	{
		if (time_now - time_start > MAX_WAIT_TIME)
			status = IceConnectRejected;
	}
}

if (status == IceConnectAccepted)
{
	Add new_ice_conn to the list of open connections
}
else
{
	IceCloseConnection (new_ice_conn);
}
.De
.LP
After
.PN IceAcceptConnection
is called and the connection has been
validated, the client is ready to receive a
.PN ProtocolSetup
(provided
that
.PN IceRegisterForProtocolReply
was called) or send a
.PN ProtocolSetup
(provided that
.PN IceRegisterForProtocolSetup
was called).
.NH 2
Closing ICE Connections
.XS
\*(SN Closing ICE Connections
.XE
.LP
To close an ICE connection created with
.PN IceOpenConnection
or
.PN IceAcceptConnection ,
use
.PN IceCloseConnection .
.sM
.FD 0
IceCloseStatus IceCloseConnection\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection to close.
.LP
.eM
To actually close an ICE connection, the following conditions
must be met:
.IP \(bu 5
The \fIopen reference count\fP must have reached zero on this ICE connection.
When
.PN IceOpenConnection
is called, it tries to use a previously opened
ICE connection.  If it is able to use an existing connection, it increments
the open reference count on the connection by one.
So, to close an ICE connection, each call to
.PN IceOpenConnection
must be matched with a call to
.PN IceCloseConnection .
The connection can be closed only
on the last call to
.PN IceCloseConnection .
.IP \(bu 5
The \fIactive protocol count\fP\^ must have reached zero.  Each time a
.PN ProtocolSetup
succeeds on the connection, the active protocol count
is incremented by one.  When the client no longer expects to use the
protocol on the connection, the
.PN IceProtocolShutdown
function should be called, which decrements the active protocol count
by one (see section 8, ``Protocol Setup and Shutdown'').
.IP \(bu 5
If shutdown negotiation is enabled on the connection, the client on the other
side of the ICE connection must agree to have the connection closed.
.LP
.PN IceCloseConnection
returns one of the following values:
.IP \(bu 5
.PN IceClosedNow
\- the ICE connection was closed at this time.  The watch procedures were
invoked and the connection was freed.
.IP \(bu 5
.PN IceClosedASAP
\- an IO error had occurred on the connection, but
.PN IceCloseConnection
is being called within a nested
.PN IceProcessMessages .
The watch procedures have been invoked at this time, but the connection
will be freed as soon as possible (when the nesting level reaches zero and
.PN IceProcessMessages
returns a status of
.PN IceProcessMessagesConnectionClosed ).
.IP \(bu 5
.PN IceConnectionInUse
\- the connection was not closed at this time, because it is being used by
other active protocols.
.IP \(bu 5
.PN IceStartedShutdownNegotiation
\- the connection was not closed at this time and shutdown negotiation started
with the client on the other side of the ICE connection.  When the connection
is actually closed,
.PN IceProcessMessages
will return a status of
.PN IceProcessMessagesConnectionClosed .
.sp
.LP
When it is known that the client on the other side of the ICE connection
has terminated the connection without initiating shutdown negotiation, the
.PN IceSetShutdownNegotiation
function should be called to turn off shutdown negotiation.  This will prevent
.PN IceCloseConnection
from writing to a broken connection.
.sM
.FD 0
void IceSetShutdownNegotiation\^(\^\fIice_conn\fP, \fInegotiate\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    Bool \fInegotiate\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fInegotiate\fP 1i
If
.PN False ,
shutdown negotiating will be turned off.
.LP
.eM
.LP
To check the shutdown negotiation status of an ICE connection, use
.PN IceCheckShutdownNegotiation .
.sM
.FD 0
Bool IceCheckShutdownNegotiation\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.LP
.eM
.PN IceCheckShutdownNegotiation
returns
.PN True
if shutdown negotiation will take place on the connection;
otherwise, it returns
.PN False .
Negotiation is on by default for a connection.  It
can only be changed with the
.PN IceSetShutdownNegotiation
function.
.NH 2
Connection Watch Procedures
.XS
\*(SN Connection Watch Procedures
.XE
.LP
To add a watch procedure that will be called
each time ICElib opens a new connection via
.PN IceOpenConnection
or
.PN IceAcceptConnection
or closes a connection via
.PN IceCloseConnection ,
use
.PN IceAddConnectionWatch .
.sM
.FD 0
Status IceAddConnectionWatch\^(\^\fIwatch_proc\fP, \fIclient_data\fP\^)
.br
    IceWatchProc \fIwatch_proc\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.FN
.IP \fIwatch_proc\fP 1i
The watch procedure to invoke when ICElib opens or
closes a connection.
.IP \fIclient_data\fP 1i
This pointer will be passed to the watch procedure.
.LP
.eM
The return value of
.PN IceAddConnectionWatch
is zero for failure, and a positive value for success.
.LP
Note that several calls to
.PN IceOpenConnection
might share the same ICE connection.  In such a case, the watch procedure
is only invoked when the connection is first created (after authentication
succeeds).  Similarly, because
connections might be shared, the watch procedure is called only if
.PN IceCloseConnection
actually closes the connection (right before the IceConn is freed).
.LP
The watch procedures are very useful for applications that
need to add a file descriptor to a select mask when a new connection
is created and remove the file descriptor when the connection is destroyed.
Because connections are shared, knowing when to add and remove the file
descriptor from the select mask would be difficult without the watch
procedures.
.LP
Multiple watch procedures may be registered with the ICE library.
No assumptions should be made about their order of invocation.
.LP
If one or more ICE connections were already created by the ICE library at the
time the watch procedure is registered, the watch procedure will instantly
be invoked for each of these ICE connections (with the opening argument
set to
.PN True ).
.LP
The watch procedure is of type
.PN IceWatchProc .
.sM
.FD 0
typedef void (*IceWatchProc)();

void WatchProc\^(\^\fIice_conn\fP, \fIclient_data\fP\^, \fIopening\fP\^, \fIwatch_data\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.br
    Bool \fIopening\fP\^;
.br
    IcePointer *\fIwatch_data\fP\^;
.FN
.IP \fIice_conn\fP\^ 1i
The opened or closed ICE connection.  Call
.PN IceConnectionNumber
to get the file descriptor associated with this connection.
.IP \fIclient_data\fP\^ 1i
Client data specified in the call to
.PN IceAddConnectionWatch .
.IP \fIopening\fP\^ 1i
If
.PN True ,
the connection is being opened.  If
.PN False ,
the connection is being closed.
.IP \fIwatch_data\fP\^ 1i
Can be used to save a pointer to client data.
.LP
.eM
If opening is
.PN True ,
the client should set the *watch_data
pointer to any data it may need to save until the connection is closed
and the watch procedure is
invoked again with opening set to
.PN False .
.sp
.LP
To remove a watch procedure, use
.PN IceRemoveConnectionWatch .
.sM
.FD 0
void IceRemoveConnectionWatch\^(\^\fIwatch_proc\fP, \fIclient_data\fP\^)
.br
    IceWatchProc \fIwatch_proc\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.LP
.FN
.IP \fIwatch_proc\fP 1i
The watch procedure that was passed to
.PN IceAddConnectionWatch .
.IP \fIclient_data\fP 1i
The client_data pointer that was passed to
.PN IceAddConnectionWatch .
.LP
.eM
.NH 1
Protocol Setup and Shutdown
.XS
\*(SN Protocol Setup and Shutdown
.XE
.LP
To activate a protocol on a given ICE connection, use
.PN IceProtocolSetup .
.LP
.sM
.FD 0
IceProtocolSetupStatus IceProtocolSetup\^(\^\fIice_conn\fP, \fImy_opcode\fP\^, \fIclient_data\fP\^, \fImust_authenticate\fP\^,
.br
                    \fImajor_version_ret\fP\^, \fIminor_version_ret\fP\^, \fIvendor_ret\fP\^, \fIrelease_ret\fP\^, \fIerror_length\fP\^, \fIerror_string_ret\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    int \fImy_opcode\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.br
    Bool \fImust_authenticate\fP\^;
.br
    int *\fImajor_version_ret\fP\^;
.br
    int *\fIminor_version_ret\fP\^;
.br
    char **\fIvendor_ret\fP\^;
.br
    char **\fIrelease_ret\fP\^;
.br
    int \fIerror_length\fP\^;
.br
    char *\fIerror_string_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fImy_opcode\fP 1i
The major opcode of the protocol to be set up, as returned by
.PN IceRegisterForProtocolSetup .
.IP \fIclient_data\fP 1i
The client data stored in this pointer will be passed to the
.PN IcePoProcessMsgProc
callback.
.IP \fImust_authenticate\fP 1i
If
.PN True ,
the other client may not bypass authentication.
.IP \fImajor_version_ret\fP 1i
The major version of the protocol to be used is returned.
.IP \fIminor_version_ret\fP 1i
The minor version of the protocol to be used is returned.
.IP \fIvendor_ret\fP 1i
The vendor string specified by the protocol acceptor.
.IP \fIrelease_ret\fP 1i  
The release string specified by the protocol acceptor.
.IP \fIerror_length\fP 1i
Specifies the length of the error_string_ret argument passed in.
.IP \fIerror_string_ret\fP 1i
Returns a null-terminated error message, if any. 
The error_string_ret argument points to user supplied memory.
No more than error_length bytes are used.
.LP
.eM
The vendor_ret and release_ret strings should be freed with
.PN free
when no longer needed.
.LP
.PN IceProtocolSetup
returns one of the following values:
.IP \(bu 5
.PN IceProtocolSetupSuccess
\- the major_version_ret, minor_version_ret, vendor_ret, release_ret are set.
.IP \(bu 5
.PN IceProtocolSetupFailure
or
.PN IceProtocolSetupIOError
\- check error_string_ret for failure reason.
The major_version_ret, minor_version_ret, vendor_ret, release_ret are not set.
.IP \(bu 5
.PN IceProtocolAlreadyActive
\- this protocol is already active on this connection.
The major_version_ret, minor_version_ret, vendor_ret, release_ret are not set.
.sp
.LP
To notify the ICE library when a given protocol
will no longer be used on an ICE connection, use
.PN IceProtocolShutdown .
.LP
.sM
.FD 0
Status IceProtocolShutdown\^(\^\fIice_conn\fP, \fImajor_opcode\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    int \fImajor_opcode\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fImajor_opcode\fP 1i
The major opcode of the protocol to shut down.
.LP
.eM
The return value of
.PN IceProtocolShutdown
is zero for failure and a positive value for success.
.LP
Failure will occur if the major opcode was never registered OR the protocol
of the major opcode was never activated on the connection.  By activated,
we mean that a
.PN ProtocolSetup
succeeded on the connection.
Note that ICE does not define how each sub-protocol triggers a
protocol shutdown.
.NH 1
Processing Messages
.XS
\*(SN Processing Messages
.XE
.LP
To process incoming messages on an ICE connection, use
.PN IceProcessMessages .
.sM
.FD 0
IceProcessMessagesStatus IceProcessMessages\^(\^\fIice_conn\fP, \fIreply_wait\fP\^, \fIreply_ready_ret\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IceReplyWaitInfo *\fIreply_wait\fP\^;
.br
    Bool *\fIreply_ready_ret\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIreply_wait\fP 1i
Indicates if a reply is being waited for.
.IP \fIreply_ready_ret\fP 1i
If set to
.PN True
on return, a reply is ready.
.LP
.eM
.PN IceProcessMessages
is used in two ways:
.IP \(bu 5
In the first, a client may
generate a message and block by calling
.PN IceProcessMessages
repeatedly until it gets its reply.
.IP \(bu 5
In the second, a
client calls
.PN IceProcessMessages
with reply_wait set to NULL in response to
.PN select
showing that there is data to read on the ICE connection.
The ICE library may process zero or more complete messages.
Note that messages that are not blocked for are always processed by
invoking callbacks.
.LP
.PN IceReplyWaitInfo
contains the major/minor opcodes and sequence number
of the message for which a reply is being awaited.  It also contains
a pointer to the reply message to be filled in (the protocol library
should cast this
.PN IcePointer
to the appropriate reply type).  In most
cases, the reply will have some fixed-size part, and the client waiting
for the reply will have provided a pointer to a structure to hold
this fixed-size data.  If there is variable-length data, it would be
expected that the
.PN IcePoProcessMsgProc
callback will have to allocate additional
memory and store pointer(s) to that memory in the fixed-size
structure.  If the entire data is variable length (for example, a single
variable-length string), then the client waiting for the reply would probably
just pass a pointer to fixed-size space to hold a pointer, and the
.PN IcePoProcessMsgProc
callback would allocate the storage and store the pointer.
It is the responsibility of the client receiving the reply to
free up any memory allocated on its behalf.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned long sequence_of_request;
	int major_opcode_of_request;
	int minor_opcode_of_request;
	IcePointer reply;
} IceReplyWaitInfo;
.De
.LP
.eM
If reply_wait is not NULL and
.PN IceProcessMessages
has a reply or error to return in response to this reply_wait
(that is, no callback was generated), then the reply_ready_ret argument
will be set to
.PN True .
.LP
If reply_wait is NULL,
then the caller may also pass NULL
for reply_ready_ret and be guaranteed that no value will be stored
in this pointer.
.LP
.PN IceProcessMessages
returns one of the following values:
.IP \(bu 5
.PN IceProcessMessagesSuccess
\- no error occurred.
.IP \(bu 5
.PN IceProcessMessagesIOError
\- an IO error occurred, 
and the caller must explicitly close the connection by calling
.PN IceCloseConnection .
.IP \(bu 5
.PN IceProcessMessagesConnectionClosed
\- the ICE connection has been closed (closing of the connection was deferred
because of shutdown negotiation, or because the
.PN IceProcessMessages
nesting level was not zero).  Do not attempt
to access the ICE connection at this point, since it has been freed.
.NH 1
Ping
.XS
\*(SN Ping
.XE
.LP
To send a ``Ping'' message to the client on the other side of the
ICE connection, use
.PN IcePing .
.sM
.FD 0
Status IcePing\^(\^\fIice_conn\fP, \fIping_reply_proc\fP\^, \fIclient_data\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePingReplyProc \fIping_reply_proc\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIping_reply_proc\fP 1i
The callback to invoke when the Ping reply arrives.
.IP \fIclient_data\fP 1i
This pointer will be passed to the
.PN IcePingReplyProc
callback.
.LP
.eM
.PN IcePing
returns zero for failure and a positive value for success.
.LP
When
.PN IceProcessMessages
processes the Ping reply, it will invoke the
.PN IcePingReplyProc
callback.
.sM
.FD 0
typedef void (*IcePingReplyProc)();

void PingReplyProc\^(\^\fIice_conn\fP, \fIclient_data\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    IcePointer \fIclient_data\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIclient_data\fP 1i
The client data specified in the call to
.PN IcePing .
.LP
.eM
.NH 1
Using ICElib Informational Functions
.XS
\*(SN Using ICElib Informational Functions
.XE
.LP
.sM
.FD 0
IceConnectStatus IceConnectionStatus\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceConnectionStatus
returns the status of an ICE connection.  The possible return values are:
.IP \(bu 5
.PN IceConnectPending
\- the connection is not valid yet (that is, authentication is taking place).
This is only relevant to connections created by
.PN IceAcceptConnection .
.IP \(bu 5
.PN IceConnectAccepted
\- the connection has been accepted.
This is only relevant to connections created by
.PN IceAcceptConnection .
.IP \(bu 5
.PN IceConnectRejected
\- the connection had been rejected  (that is, authentication failed).
This is only relevant to connections created by
.PN IceAcceptConnection .
.IP \(bu 5
.PN IceConnectIOError
\- an IO error has occurred on the connection.
.LP
.sM
.FD 0
char *IceVendor\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceVendor
returns the ICE library vendor identification
for the other side of the connection.
The string should be freed with a call to
.PN free
when no longer needed.
.LP
.sM
.FD 0
char *IceRelease\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceRelease
returns the release identification of the ICE library
on the other side of the connection.
The string should be freed with a call to
.PN free
when no longer needed.
.LP
.sM
.FD 0
int IceProtocolVersion\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceProtocolVersion
returns the major version of the ICE protocol on this connection.
.LP
.sM
.FD 0
int IceProtocolRevision\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceProtocolRevision
returns the minor version of the ICE protocol on this connection.
.LP
.sM
.FD 0
int IceConnectionNumber\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceConnectionNumber
returns the file descriptor of this ICE connection.
.LP
.sM
.FD 0
char *IceConnectionString\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceConnectionString
returns the network ID of the client that
accepted this connection.  The string should be freed with a call to
.PN free
when no longer needed.
.LP
.sM
.FD 0
unsigned long IceLastSentSequenceNumber\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceLastSentSequenceNumber
returns the sequence number of the last message sent on this ICE connection.
.LP
.sM
.FD 0
unsigned long IceLastReceivedSequenceNumber\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceLastReceivedSequenceNumber
returns the sequence number of the last message received on this
ICE connection.
.LP
.sM
.FD 0
Bool IceSwapping\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceSwapping
returns
.PN True
if byte swapping is necessary when reading messages on the ICE connection.
.LP
.sM
.FD 0
IcePointer IceGetContext\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.eM
.PN IceGetContext
returns the context associated with a connection created by
.PN IceOpenConnection .
.NH 1
ICE Messages
.XS
\*(SN ICE Messages
.XE
.LP
All ICE messages have a standard 8-byte header.  The ICElib macros that
read and write messages rely on the following naming convention for message
headers:
.LP
.Ds 0
.TA .5i 1i
.ta .5i 1i
	CARD8	major_opcode;
	CARD8	minor_opcode;
	CARD8	data[2];
	CARD32	length B32;
.De
.LP
The 3rd and 4th bytes of the message header can be used as needed.
The length field is specified in units of 8 bytes.
.NH 2
Sending ICE Messages
.XS
\*(SN Sending ICE Messages
.XE
.LP
The ICE library maintains an output buffer used for generating messages.
Protocol libraries layered on top of ICE may choose to batch messages
together and flush the output buffer at appropriate times.
.LP
If an IO error has occurred on an ICE connection, all write operations
will be ignored.  
For further information, see section 13, ``Error Handling.''
.sp
.LP
To get the size of the ICE output buffer, use
.PN IceGetOutBufSize .
.sM
.FD 0
int IceGetOutBufSize\^(\^\fIice_conn\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.LP
.eM
.LP
To flush the ICE output buffer, use
.PN IceFlush .
.sM
.FD 0
IceFlush\^(\^\fIice_conn\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.LP
.eM
Note that the output buffer may be implicitly flushed if there is insufficient
space to generate a message.
.LP
The following macros can be used to generate ICE messages:
.LP
.sM
.FD 0
IceGetHeader\^(\^\fIice_conn\fP, \fImajor_opcode\fP\^, \fIminor_opcode\fP\^, \fIheader_size\fP\^, \fI<C_data_type>\fP\^, \fIpmsg\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fImajor_opcode\fP\^;
.br
     int \fIminor_opcode\fP\^;
.br
     int \fIheader_size\fP\^;
.br
     <C_data_type> *\fIpmsg\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fImajor_opcode\fP 1i
The major opcode of the message.
.IP \fIminor_opcode\fP 1i
The minor opcode of the message.
.IP \fIheader_size\fP 1i
The size of the message header (in bytes).
.IP \fI<C_data_type>\fP 1i
The actual C data type of the message header.
.IP \fIpmsg\fP 1i
The message header pointer.  After this macro is called, the
library can store data in the message header.
.LP
.eM
.PN IceGetHeader
is used to set up a message header on an ICE connection.
It sets the major and minor opcodes of the message, and initializes
the message's length to the length of the header.  If additional
variable length data follows, the message's length field should be
updated.
.sp
.LP
.sM
.FD 0
IceGetHeaderExtra\^(\^\fIice_conn\fP, \fImajor_opcode\fP\^, \fIminor_opcode\fP\^, \fIheader_size\fP\^, \fIextra\fP\^, \fI<C_data_type>\fP\^, \fIpmsg\fP\^, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fImajor_opcode\fP\^;
.br
     int \fIminor_opcode\fP\^;
.br
     int \fIheader_size\fP\^;
.br
     int \fIextra\fP\^;
.br
     <C_data_type> *\fIpmsg\fP\^;
.br
     char *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fImajor_opcode\fP 1i
The major opcode of the message.
.IP \fIminor_opcode\fP 1i
The minor opcode of the message.
.IP \fIheader_size\fP 1i
The size of the message header (in bytes).
.IP \fIextra\fP 1i
The size of the extra data beyond the header (in 8-byte units).
.IP \fI<C_data_type>\fP 1i
The actual C data type of the message header.
.IP \fIpmsg\fP 1i
The message header pointer.  After this macro is called, the
library can store data in the message header.
.IP \fIpdata\fP 1i
Returns a pointer to the ICE output buffer that points
immediately after the message header.  The variable length
data should be stored here.  If there was not enough room
in the ICE output buffer, pdata is set to NULL.
.LP
.eM
.PN IceGetHeaderExtra
is used to generate a message with a fixed (and relatively small) amount
of variable length data.  The complete message must fit in the ICE output
buffer.
.sp 
.LP
.sM
.FD 0
IceSimpleMessage\^(\^\fIice_conn\fP, \fImajor_opcode\fP\^, \fIminor_opcode\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fImajor_opcode\fP\^;
.br
     int \fIminor_opcode\fP\^;
.FN
.br
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fImajor_opcode\fP 1i
The major opcode of the message.
.IP \fIminor_opcode\fP 1i
The minor opcode of the message.
.LP
.eM
.PN IceSimpleMessage
is used to generate a message that is identical
in size to the ICE header message, and has no additional data.
.sp
.LP
.sM
.FD 0
IceErrorHeader\^(\^\fIice_conn\fP, \fIoffending_major_opcode\fP\^, \fIoffending_minor_opcode\fP\^, \fIoffending_sequence_num\fP\^,
.br
                    \fIseverity\fP\^, \fIerror_class\fP\^, \fIdata_length\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIoffending_major_opcode\fP\^;
.br
     int \fIoffending_minor_opcode\fP\^;
.br
     int \fIoffending_sequence_num\fP\^;
.br
     int \fIseverity\fP\^;
.br
     int \fIerror_class\fP\^;
.br
     int \fIdata_length\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIoffending_major_opcode\fP 1i
The major opcode of the protocol in which an error was detected.
.IP \fIoffending_minor_opcode\fP 1i
The minor opcode of the protocol in which an error was detected.
.IP \fIoffending_sequence_num\fP 1i
The sequence number of the message that caused the error.
.IP \fIseverity\fP 1i
.PN IceCanContinue ,
.PN IceFatalToProtocol ,
or
.PN IceFatalToConnection .
.IP \fIerror_class\fP 1i
The error class. 
.IP \fIdata_length\fP 1i
Length of data (in 8-byte units) to be written after the header.
.LP
.eM
.PN IceErrorHeader
sets up an error message header.
.LP
Note that the two clients connected by ICE may be using different
major opcodes for a given protocol.  The offending_major_opcode passed
to this macro is the major opcode of the protocol for the client
sending the error message.
.LP
Generic errors, which are common to all protocols, have classes
in the range 0x8000..0xFFFF.
See the \fIInter-Client Exchange Protocol\fP\^ standard for more details.
.TS
lw(1i) lw(1i).
T{
.PN IceBadMinor
T}	T{
0x8000
T}
.sp 4p
T{
.PN IceBadState
T}	T{
0x8001
T}
.sp 4p
T{
.PN IceBadLength
T}	T{
0x8002
T}
.sp 4p
T{
.PN IceBadValue
T}	T{
0x8003
T}
.TE
.LP
Per-protocol errors have classes in the range 0x0000-0x7fff.
.sp
.LP
To write data to an ICE connection, use the
.PN IceWriteData
macro.  If the data fits into the ICE output buffer, it is copied there.
Otherwise, the ICE output buffer is flushed and the data is directly sent.
.LP
This macro is used in conjunction with
.PN IceGetHeader
and
.PN IceErrorHeader .
.sp
.LP
.sM
.FD 0
IceWriteData\^(\^\fIice_conn\fP, \fIbytes\fP\^, \fIdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.br
     char *\fIdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of bytes to write.
.IP \fIdata\fP 1i
The data to write.
.LP
.eM
.sp
To write data as 16-bit quantities, use
.PN IceWriteData16 .
.sM
.FD 0
IceWriteData16\^(\^\fIice_conn\fP, \fIbytes\fP\^, \fIdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.br
     short *\fIdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of bytes to write.
.IP \fIdata\fP 1i
The data to write.
.LP
.eM
.sp
To write data as 32-bit quantities, use 
.PN IceWriteData32 .
.sM
.FD 0
IceWriteData32\^(\^\fIice_conn\fP, \fIbytes\fP\^, \fIdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.br
     long *\fIdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of bytes to write.
.IP \fIdata\fP 1i
The data to write.
.LP
.eM
.sp
To bypass copying data to the ICE output buffer, use
.PN IceSendData
to directly send data over the network connection.  If necessary, the
ICE output buffer is first flushed.
.sM
.FD 0
IceSendData\^(\^\fIice_conn\fP, \fIbytes\fP\^, \fI(char *) data\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.br
     char *\fIdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of bytes to send.
.IP \fIdata\fP 1i
The data to send.
.LP
.eM
.sp
To force 32-bit or 64-bit alignment, use
.PN IceWritePad .
A maximum of 7 pad bytes can be specified.
.sM
.FD 0
IceWritePad\^(\^\fIice_conn\fP, \fIbytes\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of pad bytes.
.LP
.eM
.NH 2
Reading ICE Messages
.XS
\*(SN Reading ICE Messages
.XE
.LP
The ICE library maintains an input buffer used for reading messages.
If the ICE library chooses to perform nonblocking reads (this is
implementation-dependent), then for every read operation that it makes,
zero or more complete messages may be read into the input buffer.  As
a result, for all of the macros described in this section that read
messages, an actual read operation will occur on the connection only if
the data is not already present in the input buffer.
.sp
.LP
To get the size of the ICE input buffer, use
.PN IceGetInBufSize .
.sM
.FD 0
int IceGetInBufSize\^(\^\fIice_conn\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.LP
.eM
.LP
When reading messages, care must be taken to check for IO errors.  If
any IO error occurs in reading any part of a message, the message should
be thrown out.  After using any of the macros described below for reading
messages, the
.PN IceValidIO
macro can be used to check if an IO error occurred on the
connection.  After an IO error has occurred on an ICE connection, all
read operations will be ignored. 
For further information, see section 13, ``Error Handling.''
.sp
.LP
.sM
.FD 0
Bool IceValidIO\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.LP
.eM
.LP
The following macros can be used to read ICE messages.
.sM
.FD 0
IceReadSimpleMessage\^(\^\fIice_conn\fP, \fI<C_data_type>\fP\^, \fIpmsg\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     <C_data_type> *\fIpmsg\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fI<C_data_type>\fP 1i
The actual C data type of the message header.
.IP \fIpmsg\fP 1i
This pointer is set to the message header.
.LP
.eM
.PN IceReadSimpleMessage
is used for messages that are identical in size to the 8-byte ICE header, but
use the spare 2 bytes in the header to encode additional data.  Note that the
ICE library always reads in these first 8 bytes, so it can obtain the major
opcode of the message.
.PN IceReadSimpleMessage
simply returns a pointer to these 8 bytes; it does not actually read any data
into the input buffer.
.LP
For a message with variable length data, there are two ways of reading
the message.  One method involves reading the complete message in one
pass using
.PN IceReadCompleteMessage .
The second method involves reading the message header (note that this may
be larger than the 8-byte ICE header), then reading
the variable length data in chunks (see
.PN IceReadMessageHeader
and
.PN IceReadData ).
.sp
.LP
.sM
.FD 0
IceReadCompleteMessage\^(\^\fIice_conn\fP, \fIheader_size\fP\^, \fI<C_data_type>\fP\^, \fIpmsg\fP\^, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIheader_size\fP\^;
.br
     <C_data_type> *\fIpmsg\fP\^;
.br
     char *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIheader_size\fP 1i
The size of the message header (in bytes).
.IP \fI<C_data_type>\fP 1i
The actual C data type of the message header.
.IP \fIpmsg\fP 1i
This pointer is set to the message header.
.IP \fIpdata\fP 1i
This pointer is set to the variable length data of the message.
.LP
.eM
If the ICE input buffer has sufficient space,
.PN IceReadCompleteMessage
will read the complete message into the
ICE input buffer.  Otherwise, a buffer will be allocated to hold the
variable length data.  After the call, the pdata argument should
be checked against NULL to make sure that there was sufficient memory
to allocate the buffer.
.sp
.LP
After calling
.PN IceReadCompleteMessage
and processing the message,
.PN IceDisposeCompleteMessage
should be called.
.LP
.sM
.FD 0
IceDisposeCompleteMessage\^(\^\fIice_conn\fP, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     char *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIpdata\fP 1i
The pointer to the variable length data returned in
.PN IceReadCompleteMessage .
.LP
.eM
If a buffer had to be allocated to hold the variable length data (because
it did not fit in the ICE input buffer), it is freed here by ICElib.
.sp
.LP
.sM
.FD 0
IceReadMessageHeader\^(\^\fIice_conn\fP, \fIheader_size\fP\^, \fI<C_data_type>\fP\^, \fIpmsg\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIheader_size\fP\^;
.br
     <C_data_type> *\fIpmsg\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIheader_size\fP 1i
The size of the message header (in bytes).
.IP \fI<C_data_type>\fP 1i
The actual C data type of the message header.
.IP \fIpmsg\fP 1i
This pointer is set to the message header.
.LP
.eM
.PN IceReadMessageHeader
reads just the message header.  The rest
of the data should be read with the
.PN IceReadData
family of macros.  This method of reading a message should be used when the
variable length data must be read in chunks.
.sp
.LP
To read data directly into a user supplied buffer, use
.PN IceReadData .
.sM
.FD 0
IceReadData\^(\^\fIice_conn\fP, \fIbytes\fP\^, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.br
     char *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of bytes to read.
.IP \fIpdata\fP 1i
The data is read into this user supplied buffer.
.LP
.eM
.sp
To read data as 16-bit quantities, use
.PN IceReadData16 .
.sM
.FD 0
IceReadData16\^(\^\fIice_conn\fP, \fIswap\fP\^, \fIbytes\fP\^, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     Bool \fIswap\fP\^;
.br
     int \fIbytes\fP\^;
.br
     short *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIswap\fP 1i
If
.PN True,
the values will be byte swapped.
.IP \fIbytes\fP 1i
The number of bytes to read.
.IP \fIpdata\fP 1i
The data is read into this user supplied buffer.
.LP
.eM
.sp
To read data as 32-bit quantities, use
.PN IceReadData32 .
.sM
.FD 0
IceReadData32\^(\^\fIice_conn\fP, \fIswap\fP\^, \fIbytes\fP\^, \fIpdata\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     Bool \fIswap\fP\^;
.br
     int \fIbytes\fP\^;
.br
     long *\fIpdata\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIswap\fP 1i
If
.PN True,
the values will be byte swapped.
.IP \fIbytes\fP 1i
The number of bytes to read.
.IP \fIpdata\fP 1i
The data is read into this user supplied buffer.
.LP
.eM
.sp
To force 32-bit or 64-bit alignment, use 
.PN IceReadPad .
A maximum of 7 pad bytes can be specified.
.sM
.FD 0
IceReadPad\^(\^\fIice_conn\fP, \fIbytes\fP\^)
.br
     IceConn \fIice_conn\fP\^;
.br
     int \fIbytes\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIbytes\fP 1i
The number of pad bytes.
.LP
.eM
.NH 1
Error Handling
.XS
\*(SN Error Handling
.XE
.LP
There are two default error handlers in ICElib: 
.IP \(bu 5
One to handle typically fatal conditions (for example, 
a connection dying because a machine crashed) 
.IP \(bu 5
One to handle ICE-specific protocol errors
.LP
These error handlers can be changed to user-supplied routines if you
prefer your own error handling and can be changed as often as you like.
.sp
.LP
To set the ICE error handler, use
.PN IceSetErrorHandler .
.sM
.FD 0
IceErrorHandler IceSetErrorHandler\^(\^\fIhandler\fP\^)
.br
    IceErrorHandler \fIhandler\fP\^;
.FN
.IP \fIhandler\fP 1i
The ICE error handler.
You should pass NULL to restore the default handler.
.LP
.eM
.PN IceSetErrorHandler
returns the previous error handler.
.LP
The ICE error handler is invoked when an unexpected ICE protocol
error (major opcode 0) is encountered.  The action of the default
handler is to print an explanatory message to 
.PN stderr
and if the severity is fatal, call 
.PN exit
with a nonzero value.  If exiting
is undesirable, the application should register its own error handler.
.LP
Note that errors in other protocol
domains should be handled by their respective libraries (these libraries
should have their own error handlers).
.LP
An ICE error handler has the type of
.PN IceErrorHandler .
.sM
.FD 0
typedef void (*IceErrorHandler)();

void ErrorHandler\^(\^\fIice_conn\fP, \fIswap\fP\^, \fIoffending_minor_opcode\fP\^, \fIoffending_sequence_num\fP\^, \fIerror_class\fP\^, 
.br
                     \fIseverity\fP\^, \fIvalues\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    Bool \fIswap\fP\^;
.br
    int \fIoffending_minor_opcode\fP\^;
.br
    unsigned long \fIoffending_sequence_num\fP\^;
.br
    int \fIerror_class\fP\^;
.br
    int \fIseverity\fP\^;
.br
    IcePointer \fIvalues\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.IP \fIswap\fP 1i
A flag that indicates if the values need byte swapping.
.IP \fIoffending_minor_opcode\fP 1i
The ICE minor opcode of the offending message.
.IP \fIoffending_sequence_num\fP 1i
The sequence number of the offending message.
.IP \fIerror_class\fP 1i
The error class of the offending message.
.IP \fIseverity\fP 1i
.PN IceCanContinue ,
.PN IceFatalToProtocol ,
or
.PN IceFatalToConnection .
.IP \fIvalues\fP 1i
Any additional error values specific to the minor opcode and class.
.LP
.eM
The following error classes are defined at the ICE level:
.LP
.Ds 0
.PN IceBadMinor
.PN IceBadState
.PN IceBadLength
.PN IceBadValue
.PN IceBadMajor
.PN IceNoAuth
.PN IceNoVersion
.PN IceSetupFailed
.PN IceAuthRejected
.PN IceAuthFailed
.PN IceProtocolDuplicate
.PN IceMajorOpcodeDuplicate
.PN IceUnknownProtocol
.De
.LP
For further information, see
the \fIInter-Client Exchange Protocol\fP\^ standard.
.sp
.LP
To handle fatal I/O errors, use
.PN IceSetIOErrorHandler .
.LP
.sM
.FD 0
IceIOErrorHandler IceSetIOErrorHandler\^(\^\fIhandler\fP\^)
.br
    IceIOErrorHandler \fIhandler\fP\^;
.FN
.IP \fIhandler\fP 1i
The I/O error handler.
You should pass NULL to restore the default handler.
.LP
.eM
.PN IceSetIOErrorHandler
returns the previous IO error handler.
.LP
An ICE I/O error handler has the type of
.PN IceIOErrorHandler .
.LP
.sM
.FD 0
typedef void (*IceIOErrorHandler)();

void IOErrorHandler\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.LP
.eM
There are two ways of handling IO errors in ICElib:
.IP \(bu 5
In the first, the IO error handler does whatever is necessary
to respond to the IO error and then returns, but it does not call
.PN IceCloseConnection .
The ICE connection is given a ``bad IO'' status, and all future reads
and writes to the connection are ignored.  The next time
.PN IceProcessMessages
is called it will return a status of
.PN IceProcessMessagesIOError .
At that time, the application should call
.PN IceCloseConnection .
.IP \(bu 5
In the second, the IO error handler does call
.PN IceCloseConnection ,
and then uses the
.PN longjmp
call to get back to the application's main event loop.
The 
.PN setjmp
and 
.PN longjmp
calls may not work properly on all platforms,
and special care must be taken to avoid memory leaks.
Therefore, this second model is less desirable.
.LP
Before the application I/O error handler is invoked, protocol libraries
that were interested in being notified of I/O errors will have their
.PN IceIOErrorProc
handlers invoked.  This handler is set up in the protocol registration
functions (see
.PN IceRegisterForProtocolSetup
and 
.PN IceRegisterForProtocolReply )
and could be used to clean up
state specific to the protocol.
.sp
.LP
.sM
typedef void (*IceIOErrorProc)();
.LP
.FD 0
void IOErrorProc\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection object.
.LP
.eM
Note that every
.PN IceIOErrorProc
callback must return.  This is required
because each active protocol must be notified of the broken connection,
and the application IO error handler must be invoked afterwards.
.NH 1
Multi-Threading Support
.XS
\*(SN Multi-Threading Support
.XE
.LP
To declare that multiple threads in an application will be using the ICE
library, use
.PN IceInitThreads .
.LP
.sM
.FD 0
Status IceInitThreads\^()
.FN
.LP
.eM
The
.PN IceInitThreads
function must be the first ICElib function a
multi-threaded program calls.  It must complete before any other ICElib
call is made. 
.PN IceInitThreads
returns a nonzero status if and only if it was able 
to initialize the threads package successfully.
It is safe to call 
.PN IceInitThreads
more than once, although the threads package will only be initialized once.
.LP
Protocol libraries layered on top of ICElib will have to lock critical
sections of code that access an ICE connection (for example, when
generating messages).  Two calls, which are generally implemented as
macros, are provided:
.sM
.FD 0
IceLockConn\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
.sp
IceUnlockConn\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection.
.LP
.eM
.sp
To keep an ICE connection locked across several ICElib calls, applications use
.PN IceAppLockConn
and
.PN IceAppUnlockConn .
.sM
.FD 0
void IceAppLockConn\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection to lock.
.LP
.eM
The
.PN IceAppLockConn
function completely locks out other threads using the connection
until
.PN IceAppUnlockConn
is called.  Other threads attempting to use ICElib
calls on the connection will block.
If the program has not previously called
.PN IceInitThreads ,
.PN IceAppLockConn
has no effect.
.LP
.sM
.FD 0
void IceAppUnlockConn\^(\^\fIice_conn\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.FN
.IP \fIice_conn\fP 1i
The ICE connection to unlock.
.LP
.eM
The
.PN IceAppUnlockConn
function allows other threads to complete ICElib
calls on the connection that were blocked by a previous call to
.PN IceAppLockConn
from this thread.  If the program has not previously called
.PN IceInitThreads ,
.PN IceAppUnlockConn
has no effect.
.NH 1
Miscellaneous Functions
.XS
\*(SN Miscellaneous Functions
.XE
.LP
To allocate scratch space (for example, when generating
messages with variable data), use
.PN IceAllocScratch .
Each ICE connection has one scratch space associated with it.
The scratch space starts off as empty and grows as needed.
The contents of the scratch space is not guaranteed to be preserved
after any ICElib function is called.
.LP
.sM
.FD 0
char *IceAllocScratch\^(\^\fIice_conn\fP, \fIsize\fP\^)
.br
    IceConn \fIice_conn\fP\^;
.br
    unsigned long \fIsize\fP\^;
.FN
.IP \fIice_conn\fP 1i
A valid ICE connection object.
.IP \fIsize\fP 1i
The number of bytes required.
.LP
.eM
Note that the memory returned by
.PN IceAllocScratch
should not be freed by the caller.
The ICE library will free the memory when the ICE connection is closed.
.NH 1
Acknowledgements
.XS
\*(SN Acknowledgements
.XE
.LP
Thanks to Bob Scheifler for his thoughtful input on the design
of the ICE library.  Thanks also to Jordan Brown, Larry Cable, Donna Converse,
Clive Feather, Stephen Gildea, Vania Joloboff, Kaleb Keithley,
Stuart Marks, Hiro Miyamoto, Ralph Swick, Jim VanGilder, and Mike Wexler.
.bp
.XS
Appendix A \- Authentication Utility Functions
.XE
.ce 10
.sp 5
\s+2\fBAppendix A\fP\s-2
.sp 
\s+1\fBAuthentication Utility Functions\fP\s-1
.ce 0
.sp
.LP
As discussed in this document, the means by which authentication data
is obtained by the ICE library (for
.PN ConnectionSetup
messages or
.PN ProtocolSetup
messages) is implementation-dependent.\(dg
.FS \(dg
The X Consortium's ICElib implementation assumes the presence of an
ICE authority file.
.FE
.LP
This appendix describes some utility functions that manipulate an
ICE authority file.  The authority file can be used to pass authentication
data between clients.
.LP
The basic operations on the \&.ICEauthority file are:
.IP \(bu 5
Get file name
.IP \(bu 5
Lock
.IP \(bu 5
Unlock
.IP \(bu 5
Read entry
.IP \(bu 5
Write entry
.IP \(bu 5
Search for entry
.LP
These are fairly low-level operations, and it is expected that a program,
like "iceauth", would
exist to add, remove, and display entries in the file.
.LP
In order to use these utility functions, the
.Pn < X11/ICE/ICEutil.h >
header file must be included.
.LP
An entry in the \&.ICEauthority file is defined by the following data structure:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	char *protocol_name;
	unsigned short protocol_data_length;
	char *protocol_data;
	char *network_id;
	char *auth_name;
	unsigned short auth_data_length;
	char *auth_data;
} IceAuthFileEntry;
.De
.LP
.eM
The protocol_name member is either ``ICE'' for connection setup authentication
or the subprotocol name, such as ``XSMP''.  For each entry, protocol specific
data can be specified in the protocol_data member.  This can be used
to search for old entries that need to be removed from the file.
.LP
The network_id member is the network ID of the client accepting authentication
(for example, the network ID of a session manager).
A network ID has the following form:
.TS
lw(0.25i) lw(2.5i) lw(1i).
	tcp/<hostname>:<portnumber>	or
	decnet/<hostname>::<objname>	or
	local/<hostname>:<path>	
.TE
.LP
The auth_name member is the name of the authentication method.
The auth_data member is the actual authentication data, 
and the auth_data_length member is the number of bytes in the data.
.sp
.LP
To obtain the default authorization file name, use
.PN IceAuthFileName .
.sM
.FD 0
char *IceAuthFileName\^()
.FN
.LP
.eM
If the ICEAUTHORITY environment variable if set, this value is returned.
Otherwise, the default authorization file name is $HOME/\&.ICEauthority.
This name is statically allocated and should not be freed.
.LP
To synchronously update the authorization file, the file must
be locked with a call to
.PN IceLockAuthFile .
This function takes advantage of the fact that the 
.PN link
system call will fail if the name of the new link already exists.
.sM
.FD 0
int IceLockAuthFile\^(\^\fIfile_name\fP, \fIretries\fP\^, \fItimeout\fP\^, \fIdead\fP\^)
.br
    char *\fIfile_name\fP\^;
.br
    int \fIretries\fP\^;
.br
    int \fItimeout\fP\^;
.br
    long \fIdead\fP\^;
.FN
.IP \fIfile_name\fP 1i
The authorization file to lock.
.IP \fIretries\fP 1i
The number of retries.
.IP \fItimeout\fP 1i
The number of seconds before each retry.
.IP \fIdead\fP 1i
If a lock already exists that is the specified dead seconds old,
it is broken.
A value of zero is used to unconditionally break an old lock.
.LP
.eM
One of three values is returned:
.IP \(bu 5
.PN IceAuthLockSuccess
\- the lock succeeded.
.IP \(bu 5
.PN IceAuthLockError
\- a system error occurred, and
.PN errno
may prove useful.
.IP \(bu 5
.PN IceAuthLockTimeout
\- the specified number of retries failed.
.LP
.sp
To unlock an authorization file, use
.PN IceUnlockAuthFile .
.sM
.FD 0
void IceUnlockAuthFile\^(\^\fIfile_name\fP\^)
.br
    char *\fIfile_name\fP\^;
.FN
.IP \fIfile_name\fP 1i
The authorization file to unlock.
.LP
.eM
.LP
To read the next entry in an authorization file, use
.PN IceReadAuthFileEntry .
.sM
.FD 0
IceAuthFileEntry *IceReadAuthFileEntry\^(\^\fIauth_file\fP\^)
.br
    FILE *\fIauth_file\fP\^;
.FN
.IP \fIauth_file\fP 1i
The authorization file.
.LP
.eM
Note that it is the responsibility of the application to open the file
for reading before calling this function.  If an error is encountered,
or there are no more entries to read, NULL is returned.
.LP
Entries should be free with a call to
.PN IceFreeAuthFileEntry .
.LP
.sp
To write an entry in an authorization file, use
.PN IceWriteAuthFileEntry .
.sM
.FD 0
Status IceWriteAuthFileEntry\^(\^\fIauth_file\fP, \fIentry\fP\^)
.br
    FILE *\fIauth_file\fP\^;
.br
    IceAuthFileEntry *\fIentry\fP\^;
.FN
.IP \fIauth_file\fP 1i
The authorization file.
.IP \fIentry\fP 1i
The entry to write.
.LP
.eM
Note that it is the responsibility of the application to open the file
for writing before calling this function.  The function returns a nonzero
status if the operation was successful.
.LP
.sp
To search the default authorization file for an entry that matches a given
protocol_name/network_id/auth_name tuple, use
.PN IceGetAuthFileEntry .
.sM
.FD 0
IceAuthFileEntry *IceGetAuthFileEntry\^(\^\fIprotocol_name\fP, \fInetwork_id\fP\^, \fIauth_name\fP\^)
.br
    char *\fIprotocol_name\fP\^;
.br
    char *\fInetwork_id\fP\^;
.br
    char *\fIauth_name\fP\^;
.FN
.IP \fIprotocol_name\fP 1i
The name of the protocol to search on.
.IP \fInetwork_id\fP 1i
The network ID to search on.
.IP \fIauth_name\fP 1i
The authentication method to search on.
.LP
.eM
If
.PN IceGetAuthFileEntry
fails to find such an entry, NULL is returned.
.LP
.sp
To free an entry returned by
.PN IceReadAuthFileEntry
or
.PN IceGetAuthFileEntry ,
use
.PN IceFreeAuthFileEntry .
.sM
.FD 0
void IceFreeAuthFileEntry\^(\^\fIentry\fP\^)
.br
    IceAuthFileEntry *\fIentry\fP\^;
.FN
.IP \fIentry\fP 1i
The entry to free.
.LP
.eM
.bp
.XS
Appendix B \- MIT-MAGIC-COOKIE-1 Authentication
.XE
.ce 10
.sp 5
\s+2\fBAppendix B\fP\s-2
.sp 
\s+1\fBMIT-MAGIC-COOKIE-1 Authentication\fP\s-1
.ce 0
.sp
.LP
The X Consortium's ICElib implementation supports a simple
MIT-MAGIC-COOKIE-1 authentication scheme using the authority file utilities
described in Appendix A.
.LP
In this model, an application, such as a session manager, obtains a
magic cookie by calling
.PN IceGenerateMagicCookie ,
and then stores it in the user's local \&.ICEauthority file
so that local clients can connect.  In order to allow remote clients to
connect, some remote execution mechanism should be used to store the
magic cookie in the user's \&.ICEauthority file on a remote machine.
.LP
In addition to storing the magic cookie in the \&.ICEauthority file, the
application needs to call the
.PN IceSetPaAuthData
function in order to store the magic cookie in memory.  When it comes time
for the MIT-MAGIC-COOKIE-1 authentication procedure to accept or reject the
connection, it will compare the magic cookie presented by the requestor to
the magic cookie in memory.
.LP
.sM
.FD 0
char *IceGenerateMagicCookie\^(\^\fIlength\fP\^)
.br
    int \fIlength\fP\^;
.FN
.IP \fIlength\fP 1i
The desired length of the magic cookie.
.LP
.eM
.LP
The magic cookie returned will be null-terminated.  If memory can not be
allocated for the magic cookie, the function will return NULL.
Otherwise, the magic cookie should be freed with a call to
.PN free .
.LP
.sp
To store the authentication data in memory, use
.PN IceSetPaAuthData .
Currently, this function is only used for MIT-MAGIC-COOKIE-1
authentication, but it may be used for additional authentication
methods in the future.
.sM
.FD 0
void IceSetPaAuthData\^(\^\fInum_entries\fP, \fIentries\fP\^)
.br
    int \fInum_entries\fP\^;
.br
    IceAuthDataEntry *\fIentries\fP\^;
.FN
.IP \fInum_entries\fP 1i
The number of authentication data entries.
.IP \fIentries\fP 1i
The list of authentication data entries.
.LP
.eM
Each entry has associated with it a protocol name 
(for example, ``ICE'' for ICE connection setup authentication,
``XSMP'' for session management authentication), a network ID for the 
``accepting'' client, an authentication name (for example, MIT-MAGIC-COOKIE-1),
and authentication data.  The ICE library
will merge these entries with previously set entries, based on the
(protocol_name, network_id, auth_name) tuple.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	char *protocol_name;
	char *network_id;
	char *auth_name;
	unsigned short auth_data_length;
	char *auth_data;
} IceAuthDataEntry;
.De
.LP
.eM
.EH ''''
.OH ''''
.YZ 3
