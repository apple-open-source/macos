.\" Use tbl, -ms, and macros.t
.\" $Xorg: xsmp.ms,v 1.3 2000/08/17 19:42:19 cpqbld Exp $
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.TL
\s+2\fBX Session Management Protocol\fP\s-2
.sp
X Consortium Standard
.sp
X Version 11, Release 6.4
.AU
Mike Wexler
.AI
Kubota Pacific Computer, Inc.
.AB
.LP
This document specifies a protocol that facilitates the management of groups
of client applications by a session manager.  The session manager can cause
clients to save their state, to shut down, and to be restarted into a
previously saved state.  This protocol is layered on top of the X
Consortium's ICE protocol.
.AE
.LP
.bp
\&
.sp 8
.LP
.DS C
X Window System is a trademark of X Consortium, Inc.
.sp
Copyright \(co 1992, 1993, 1994 X Consortium
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
.af PN i
.EF ''\\\\n(PN''
.OF ''\\\\n(PN''
.bp 1
.af PN 1
.EH '\fBX Session Management Protocol\fP''\fBX11, Release 6.4\fP'
.OH '\fBX Session Management Protocol\fP''\fBX11, Release 6.4\fP'
.EF ''\fB\\\\n(PN\fP''
.OF ''\fB\\\\n(PN\fP''
.nH 1 "Acknowledgements"
.LP
First I would like to thank the entire ICCCM and Intrinsics working groups for
the comments and suggestions. I would like to make special thanks to the 
following people (in alphabetical order), Jordan Brown, Ellis Cohen, Donna 
Converse, Vania Joloboff, Stuart Marks, Ralph Mor and Bob Scheifler.
.nH 1 "Definitions and Goals"
.LP
The purpose of the X Session Management Protocol (XSMP) is to provide a
uniform mechanism for users to save and restore their sessions.  A
\fIsession\fP is a group of clients, each of which has a particular state.
The session is controlled by a network service called the \fIsession
manager\fP\^.  The session manager issues commands to its clients on behalf
of the user.  These commands may cause clients to save their state or to
terminate.  It is expected that the client will save its state in such a
way that the client can be restarted at a later time and resume its
operation as if it had never been terminated.  A client's state might
include information about the file currently being edited, the current
position of the insertion point within the file, or the start of an 
uncommitted transaction.
The means by which clients are
restarted is unspecified by this protocol.
.LP
For purposes of this protocol, a \fIclient\fP of the session manager is
defined as a connection to the session manager.  A client is typically,
though not necessarily, a process running an application program connected
to an X Window System display.  However, a client may be connected to more
than one X display or not be connected to any X displays at all.
.LP
This protocol is layered on top of the X Consortium's ICE protocol and relies on
the ICE protocol to handle connection management and authentication.
.LP
.nH 1 "Overview of the Protocol"
.LP
Clients use XSMP to register themselves with the session manager (SM).  When
a client starts up, it should connect to the SM.  The client should remain
connected for as long as it runs.  A client may resign from the session by
issuing the proper protocol messages before disconnecting.  Termination of
the connection without notice will be taken as an indication that the client
died unexpectedly.
.LP
Clients are expected to save their state in such a way as to allow multiple
instantiations of themselves to be managed independently.  A unique value
called a \fIclient-ID\fP is provided by the protocol for the purpose of
disambiguating multiple instantiations of clients.  Clients may use this ID,
for example, as part of a filename in which to store the state for a
particular instantiation.  The client-ID should be saved as part of the
command used to restart this client (the \fIRestartCommand\fP\^) so that the
client will retain the same ID after it is restarted.  Certain small pieces
of state might also be stored in the RestartCommand.   For example, an X11 client
might place a `\-twoWindow' option in its RestartCommand to indicate that it
should start up in two window mode when it is restarted.
.LP
The client finds the network address of the SM in a system-dependent way.
On POSIX systems an environment variable called SESSION_MANAGER will contain
a list of network IDs.  Each id will contain the transport name followed by a 
slash and the (transport-specific)
address.  A TCP/IP address would look like this:
.ID
	\fCtcp/\fP\fIhostname\fP\^\fC:\fP\^\fIportnumber\fP
.DE
where the hostname is a fully qualified domain name.
A Unix Domain address looks like this:
.ID
	\fClocal/\fP\fIhostname\fP\^\fC:\fP\^\fIpath\fP
.DE
A DECnet address would look like this:
.ID
	\fCdecnet/\fP\fInodename\fP\^\fC::\fP\^\fIobjname\fP
.DE
If multiple network IDs are specified, they should be separated by commas.
.NT Rationale
There was much discussion over whether the XSMP protocol should use X as
the transport protocol or whether it should use its own independent
transport.  It was decided that it would use an independent protocol for
several reasons.  First, the Session Manager should be able to 
manage programs that
do not maintain an X connection.  Second, the X protocol is not appropriate to
use as a general-purpose transport protocol.  Third, a session might
span multiple displays.
.LP
The protocol is connection based, because there is no other way for the SM
to determine reliably when clients terminate.
.LP
It should be noted that this protocol introduces another single point of 
failure into the system.  Although it is possible for clients to continue 
running after the SM has exited, this will probably not be the case in 
normal practice.  Normally the program that starts the SM will consider the
session to be terminated when the SM exits (either normally or abnormally).
.LP
To get around this would require some sort of 
rendezvous server that would also introduce a single point of failure.  In the
absence of a generally available rendezvous server, XSMP is kept simple in
the hopes of making simple reliable SMs.
.NE
.LP
Some clients may wish to manage the programs they start.  For example, a
mail program could start a text editor for editing the text of a mail
message.  A client that does this is a session manager itself;
it should supply the clients it starts with the appropriate connection
information (i.e., the SESSION_MANAGER environment variable) that specifies
a connection to itself instead of to the top level session manager.
.LP
Each client has associated with it a list of properties.
A property set by one client is not visible to any other client.
These properties are used for the client to inform the SM of the client's
current state.
When a client initially connects to the SM, there are no properties set.
.nH 1 "Data Types"
.LP
XSMP messages contain several types of data.  Both the SM and the client
always send messages in their native byte order.  Thus, both sides may need
to byte-swap the messages received.  The need to do byte-swapping is
determined at run-time by the ICE protocol. 
.LP
If an invalid value is specified for a field of any of the enumerated types, a
.PN BadValue
error message must be sent by the receiver of the message to the sender of the
message.
.br
.ne 6
.TS H
l lw(4.5i).
_
.sp 6p
.B
Type Name	Description
.R
.sp 6p
_
.sp 6p
.TH
BOOL	T{
.PN False
or
.PN True
T}
INTERACT_STYLE	T{
.PN None ,
.PN Errors ,
or
.PN Any
T}
DIALOG_TYPE	T{
.PN Error
or
.PN Normal
T}
SAVE_TYPE	T{
.PN Global ,
.PN Local ,
or
.PN Both
T}
CARD8	a one-byte unsigned integer
CARD16	a two-byte unsigned integer
CARD32	a four-byte unsigned integer
ARRAY8	a sequence of CARD8s
LISTofARRAY8	a sequence of ARRAY8s
PROPERTY	a property name (an ARRAY8), a type name, and a value of that type
LISTofPROPERTY	T{
a counted collection of \%PROPERTYs.
T}		
.sp 6p
_
.TE
.nH 1 "Protocol Setup and Message Format"
.LP
To start the XSMP protocol, the client sends the server an ICE
.PN ProtocolSetup
message.
All XSMP messages are in the standard ICE message format.  The message's major
opcode is assigned to XSMP by ICE at run-time.  The different parties
(client and SM) may be assigned different major opcodes for XSMP.  Once
assigned, all XSMP messages issued by this party will use the same major
opcode.  The message's minor opcode specifies which protocol message this
message contains. 
.nH 1 "Client Identification String"
.LP
A client ID is a string of XPCS characters encoded in ISO Latin 1 (ISO
8859-1).  No null characters are allowed in this string.  The client ID
string is used in the
.PN Register\%Client
and
.PN Register\%ClientReply
messages.
.LP
Client IDs consist of the pieces described below.  The ID is
formed by concatenating the pieces in sequence, without
separator characters.  All pieces are padded on the left 
with '0' characters
so as to fill the specified length.
Decimal numbers are
encoded using the characters `0' through `9', and
hexadecimal numbers using the characters `0' through `9'
and `A' through `F'.
.IP \(bu 4
Version.  This is currently the character `1'.
.IP \(bu 4
Address type and address.  The address type will be one of
.DS
.ta 0.5i
`1'	a 4-byte IP address encoded as 8 hexadecimal digits
`2'	a 6-byte DECNET address encoded as 12 hexadecimal digits
.DE
.IP
The address is the one of the network addresses of the machine where the
session manager (not the client) is running.  For example, the IP address
198.112.45.11 would be encoded as the string \*QC6702D0B\*U.
.IP \(bu 4
Time stamp.  A 13-digit decimal number specifying the number of
milliseconds since 00:00:00 UTC, January 1, 1970.
.IP \(bu 4
Process-ID type and process-ID.  The process-ID type will be one of
.DS
.ta 0.5i
`1'	a POSIX process-ID encoded as a 10-digit decimal number.
.DE
.IP
The process-ID is the process-ID of the session manager, not of a client.
.IP \(bu 4
Sequence number.  This is a four-digit decimal number.  It is incremented
every time the session manager creates an ID.  After reaching \*Q9999\*U it
wraps to \*Q0000\*U.
.NT "Rationale"
Once a client ID has been assigned to the client, the client keeps
this ID indefinitely.  If the client is terminated and restarted, it
will be reassigned the same ID.  It is desirable to be able to pass
client IDs around from machine to machine, from user to user, and
from session manager to session manager, while retaining the
identity of the client.  This, combined with the indefinite
persistence of client IDs, means that client IDs need to be globally
unique.  The construction specified above will ensure that any
client ID created by any user, session manager, and machine will be
different from any other.
.NE
.nH 1 "Protocol"
.LP
The protocol consists of a sequence of messages as described below.  Each
message type is specified by an ICE minor opcode.  A given message type is
sent either from a client to the session manager or from the session manager
to a client; the appropriate direction is listed with each message's
description.  For each message type, the set of 
valid responses and possible error
messages are listed.  The ICE severity is given in parentheses following
each error class.
.LP
.sM
.PN RegisterClient
[Client \(-> SM]
.RS
.LP
\fIprevious-ID\fP\^: ARRAY8
.LP
Valid Responses: 
.PN RegisterClientReply
.LP
Possible Errors:
.PN BadValue
.Pn ( CanContinue )
.RE
.eM
.LP
The client must send this message to the SM to register the client's existence.
If a client is being restarted from a previous
session, the previous-ID field must contain the client ID from the
previous session.
For new clients, previous-ID should be of zero length.
.LP
If previous-ID is not valid, the SM will send a
.PN BadValue
error message to the client.
At this point the SM reverts to the register state and waits for another
.PN RegisterClient .
The client should then send a
.PN RegisterClient
with a null previous-ID field.
.LP
.sM
.PN RegisterClientReply
[Client \(<- SM]
.RS
.LP
\fIclient-ID\fP\^: ARRAY8
.RE
.eM
.LP
The client-ID specifies a unique identification for this client.
If the client had specified an ID in the previous-ID field of the
.PN RegisterClient
message, client-ID will be identical to the previously specified ID.  If
previous-ID was null, client-ID will be a unique ID freshly generated by the
SM.  The client-ID format is specified in section 6.
.LP
If the client didn't supply a previous-ID field to the
.PN Register\%Client
message, the SM must send a
.PN SaveYourself
message with type = Local, shutdown = False, interact-style = None,
and fast = False immediately after the
.PN RegisterClientReply .
The client should respond to this like any other
.PN Save\%Yourself
message.
.LP
.sM
.PN SaveYourself
[Client \(<- SM]
.RS
.LP
\fItype\fP\^: SAVE_TYPE
.br
\fIshutdown\fP\^: BOOL
.br
\fIinteract-style\fP\^: INTERACT_STYLE
.br
\fIfast\fP\^: BOOL
.LP
Valid Responses:
.PN SetProperties ,
.PN DeleteProperties ,
.PN GetProperties ,
.PN SaveYourselfDone ,
.PN SaveYourselfPhase2Request ,
.PN InteractRequest
.RE
.eM
.LP
The SM sends this message to a client to ask it to save
its state.  The client writes a state file, if necessary,
and, if necessary, uses 
.PN SetProperties
to inform the SM of
how to restart it and how to discard the saved state.  During
this process it can, if allowed by interact-style, request
permission to interact with the user by sending an
.PN InteractRequest
message.
After the state has been saved, or
if it cannot be successfully saved, and the properties
are appropriately set, the client sends a 
.PN SaveYourselfDone
message. 
If the client wants to save additional information after all the
other clients have finished changing their own state, the client
should send
.PN SaveYourselfPhase2Request
instead of 
.PN SaveYourselfDone .
The client must then
freeze interaction with the user and wait until it
receives a 
.PN SaveComplete ,
.PN Die ,
or a 
.PN ShutdownCancelled
message.
.LP
If interact-style is
.PN None ,
the client must not interact with the
user while saving state.  If the interact-style is 
.PN Errors ,
the client
may interact with the user only if an error condition arises.  If
interact-style is 
.PN Any ,
then the client may interact with the user for
any purpose.
This is done by sending an
.PN Interact\%Request
message.  The SM will send an
.PN Interact
message to
each client that sent an
.PN Interact\%Request .  
The client must postpone all
interaction until it gets the
.PN Interact
message.  When the client is done
interacting it should send the SM an
.PN Interact\%Done
message.  The 
.PN Interact\%Request
message can be sent any time after a
.PN Save\%Yourself
and before a 
.PN Save\%Yourself\%Done .
.LP
Unusual circumstances may dictate multiple interactions.
The client may initiate as many
.PN Interact\%Request
\-
.PN Interact
\-
.PN InteractDone
sequences as it needs before it sends
.PN SaveYourselfDone .
.LP
When a client receives
.PN Save\%Yourself
and has not yet responded
.PN Save\%Yourself\%Done
to a previous
.PN Save\%Yourself ,
it must send a
.PN Save\%Yourself\%Done
and may then begin responding as appropriate
to the newly received 
.PN Save\%Yourself .
.LP
The type field specifies the type of information that should be saved:
.PN Global ,
.PN Local ,
or
.PN Both .
The 
.PN Local
type indicates that the application must update the
properties to reflect its current state, send a
.PN Save\%Yourself\%Done
and continue.  Specifically it should save enough information to restore
the state as seen by the user of this client.  It should not affect the
state as seen by other users.
The
.PN Global
type indicates that the user wants the client to 
commit all of its data to permanent, globally-accessible
storage.
.PN Both
indicates that the client should do both of these.  If
.PN Both
is specified, the client should first commit the data to permanent storage
before updating its SM properties.
.NT Examples
If a word processor was sent a 
.PN SaveYourself
with a type of 
.PN Local ,
it could create a temporary file that included the
current contents of the file, the location of the cursor, and
other aspects of the current editing session.
It would then update its
.PN Restart\%Command
property with enough information to find the temporary file, 
and its 
.PN Discard\%Command 
with enough information to remove it.
.LP
If a word processor was sent a 
.PN SaveYourself
with a type of
.PN Global ,
it would simply save the currently edited file.
.LP
If a word processor was sent a 
.PN SaveYourself
with a type of
.PN Both ,
it would first save the currently edited file.  It would then create a
temporary file with information such as the current position of the cursor
and what file is being edited.
It would then update its
.PN Restart\%Command
property with enough information to find the temporary file, 
and its 
.PN Discard\%Command 
with enough information to remove it.
.LP
Once the SM has send 
.PN SaveYourself
to a client, it can't send another 
.PN SaveYourself 
to that client until the client either
responds with a 
.PN SaveYourselfDone
or the SM sends a 
.PN ShutdownCancelled .
.NE
.NT "Advice to Implementors"
If the client stores local any state in a file or similar
\*Qexternal\*U storage, it must create a distinct
copy in response to each 
.PN SaveYourself 
message.
It \fImust not\fP simply refer to a previous copy, because
the SM may discard that previous saved state using a 
.PN DiscardCommand
without knowing that it is needed for the new checkpoint.
.NE
.LP
The shutdown field specifies whether the system is being shut down.
.NT Rationale
The interaction
may be different depending on whether or not shutdown is set.
.NE
The client must save and then must prevent interaction
until it receives a
.PN SaveComplete ,
.PN Die ,
or a
.PN Shutdown\%Cancelled ,
because anything the user does after the save will be lost.
.LP
The fast field specifies whether or not the client should save its state as quickly as
possible.  For example, if the SM knows that power is about to fail, it
should set the fast field to
.PN True .
.LP
.sM
.PN SaveYourselfPhase2
[Client \(<- SM]
.RS
.LP
.LP
Valid Responses:
.PN SetProperties ,
.PN DeleteProperties ,
.PN GetProperties ,
.PN SaveYourselfDone ,
.PN InteractRequest
.RE
.eM
.LP
The SM sends this message to a client that has previously sent a
.PN SaveYourselfPhase2Request
message.
This message informs the client that all other clients are in a fixed
state and this client can save state that is associated with other clients.
.NT "Rationale"
Clients that manager other clients (window managers, workspace managers, etc)
need to know when all clients they are managing are idle, so that the manager
can save state related to each of the clients without being concerned with
that state changing.
.NE
The client writes a state file, if necessary, and, if necessary, uses 
.PN SetProperties
to inform the SM of
how to restart it and how to discard the saved state.  During
this process it can request
permission to interact with the user by sending an
.PN InteractRequest
message.
This should only be done if an error occurs that requires user interaction
to resolve.
After the state has been saved, or
if it cannot be successfully saved, and the properties
are appropriately set, the client sends a 
.PN SaveYourselfDone
message. 
.LP
.LP
.sM
.PN SaveYourselfRequest
[Client \(-> SM]
.RS
.LP
\fItype\fP\^: SAVE_TYPE
.br
\fIshutdown\fP\^: BOOL
.br
\fIinteract-style\fP\^: INTERACT_STYLE
.br
\fIfast\fP\^: BOOL
.br
\fIglobal\fP\^: BOOL
.LP
Valid Responses:
.PN SaveYourself
.RE
.eM
.LP
An application sends this to the SM to request a checkpoint.
When the SM receives this request it may generate a 
.PN SaveYourself
message in response and it may leave the fields intact.
.NT Example
A vendor of a UPS (Uninterruptible Power Supply) might include an
SM client that would monitor the status of the UPS and generate
a fast shutdown if the power is about to be lost.
.NE
.LP
If global is set to 
.PN True ,
then the resulting 
.PN SaveYourself 
should be
sent to all applications.  If global is set to 
.PN False ,
then the resulting
.PN SaveYourself 
should be sent to the application that sent the 
.PN Save\%Yourself\%Request .
.LP
.sM
.PN InteractRequest
[Client \(-> SM]
.RS
.LP
\fIdialog-type\fP\^: DIALOG_TYPE
.LP
Valid Responses:
.PN Interact ,
.PN ShutdownCancelled
.LP
Possible Errors:
.PN BadState
.Pn ( CanContinue )
.RE
.eM
.LP
During a checkpoint or session-save operation,
only one client at a time might be granted the privilege of interacting with
the user.  The
.PN InteractRequest
message causes the SM to emit an
.PN Interact
message at some later time if the shutdown is not cancelled
by another client first.
.LP
The dialog-type field specifies either
.PN Errors ,
indicating that the 
client wants to start an error dialog or
.PN Normal ,
meaning the client 
wishes to start a non-error dialog.
.LP
.sM
.PN Interact
[Client \(<- SM]
.RS
.LP
Valid Responses:
.PN InteractDone
.LP
.RE
.eM
.LP
This message grants the client the privilege of interacting with the
user.  When the client is done interacting with the user it must
send an 
.PN InteractDone
message to the SM unless a shutdown cancel is received.
.NT "Advice to Implementors"
If a client receives a ShutdownCancelled after receiving an
.PN Interact
message, but before sending a 
.PN InteractDone ,
the client should abort the interaction and send a 
.PN SaveYourselfDone .
.NE
.LP
.sM
.PN InteractDone
[Client \(-> SM]
.RS
.LP
\fIcancel-shutdown\fP\^: BOOL
.br
.LP
Valid Responses:
.PN ShutdownCancelled
.LP
.RE
.eM
.LP
This message is used by a client to notify the SM that it is done interacting.
.LP
Setting the cancel-shutdown field to 
.PN True
indicates that
the user has requested that the entire shutdown be cancelled.
Cancel-shutdown may only be
.PN True
if the corresponding
.PN SaveYourself
message specified
.PN True
for the shutdown field and
.PN Any
or
.PN Errors
for the interact-style field.  Otherwise, cancel-shutdown must be
.PN False .
.LP
.sM
.PN SaveYourselfDone
[Client \(-> SM]
.RS
.LP
\fIsuccess\fP\^: BOOL
.LP
Valid Responses: 
.PN SaveComplete ,
.PN Die ,
.PN ShutdownCancelled
.LP
.RE
.eM
.LP
This message is sent by a client to indicate that all of the properties
representing its state have been updated.
After sending 
.PN SaveYourselfDone 
the client must
wait for a 
.PN SaveComplete ,
.PN ShutdownCancelled ,
or 
.PN Die 
message before changing its state.
If the 
.PN SaveYourself
operation was successful, then the client
should set the success field to
.PN True ;
otherwise the client should set
it to
.PN False .
.NT Example
If a client tries to save its state and runs out of disk space,
it should return 
.PN False
in the success
field of the 
.PN SaveYourselfDone
message.
.NE
.LP
.sM
.PN SaveYourselfPhase2Request
[Client \(-> SM]
.RS
.LP
Valid Responses: 
.PN ShutdownCancelled ,
.PN SaveYourselfPhase2
.LP
.RE
.eM
.LP
This message is sent by a client to indicate that it needs to be informed
when all the other clients are quiescent, so it can continue its state.
.LP
.sM
.PN Die
[Client \(<- SM]
.RS
.LP
Valid Responses:
.PN ConnectionClosed
.RE
.eM
.LP
When the SM wants a client to die it sends a
.PN Die
message.  Before the client dies it responds
by sending a 
.PN ConnectionClosed
message and may then close
its connection to the SM at any time.
.LP
.sM
.PN SaveComplete
[Client \(<- SM]
.RS
.LP
Valid Responses:
.RE
.eM
.LP
When the SM is done with a checkpoint, it will send each of the clients a
.PN SaveComplete
message.  
The client is then free to change its state.
.LP
.sM
.PN ShutdownCancelled
[Client \(<- SM]
.RS
.RE
.eM
.LP
The shutdown currently in process has been aborted.  The client can now
continue as if the shutdown had never happened.
If the client has not sent
.PN SaveYourselfDone
yet, the client can either
abort the save and send 
.PN SaveYourselfDone
with the success field
set to
.PN False ,
or it can continue with the save and send a
.PN SaveYourselfDone
with the success field set to reflect the outcome
of the save.
.LP
.sM
.PN ConnectionClosed
[Client \(-> SM]
.RS
.LP
\fIreason\fP\^: LISTofARRAY8
.RE
.eM
.LP
Specifies that the client has decided to terminate.
It should be immediately followed by closing the connection.
.LP
The reason field specifies why the client is resigning from the session.  It
is encoded as an array of Compound Text strings.  If the resignation is
expected by the user, there will typically be zero ARRAY8s here.  But if the
client encountered an unexpected fatal error, the error message (which might
otherwise be printed on stderr on a POSIX system) should be forwarded to the
SM here, one ARRAY8 per line of the message.  It is the responsibility of
the SM to display this reason to the user.
.LP
After sending this message, the client must not send any additional XSMP
messages to the SM.
.NT "Advice to Implementors"
If additional messages are received, they should be discarded.
.NE
.NT Rationale
The reason for sending the
.PN ConnectionClosed
message before
actually closing the connections is that some transport protocols will
not provide immediate notification of connection closure.
.NE
.LP
.sM
.PN SetProperties
[Client \(-> SM]
.RS
.LP
\fIproperties\fP: LISTofPROPERTY
.RE
.eM
.LP
Sets the specified properties to the specified values.
Existing properties not specified in the 
.PN Set\%Properties
message are unaffected.
Some properties have predefined semantics.
See section 11, \*QPredefined Properties.\*U
.LP
The protocol specification recommends that property names used 
for properties not defined by the standard should begin with an underscore.
To prevent conflicts among organizations, 
additional prefixes should be chosen 
(for example,  _KPC_FAST_SAVE_OPTION).
The organizational prefixes should be registered with the X Registry.
The XSMP reserves all property names not beginning with an underscore for 
future use.
.LP
.sM
.PN DeleteProperties
[Client \(-> SM]
.RS
.LP
.br
\fIproperty-names\fP: LISTofARRAY8
.RE
.eM
.LP
Removes the named properties.
.LP
.sM
.PN GetProperties
[Client \(-> SM]
.RS
.LP
Valid Responses:
.PN GetPropertiesReply
.RE
.eM
.LP
Requests that the SM respond with the
values of all the properties for this client.
.LP
.sM
.PN GetPropertiesReply
[Client \(<- SM]
.RS
.LP
\fIvalues\fP\^: LISTofPROPERTY
.RE
.eM
.LP
This message is sent in reply to a
.PN GetProperties
message and includes
the values of all the properties.
.nH 1 "Errors"
.LP
When the receiver of a message detects an error condition, 
the receiver sends
an ICE error message to the sender.
There are only two types of errors that are used by the XSMP:
.PN BadValue 
and
.PN BadState .
These are both defined in the ICE protocol.
.LP
Any message received out-of-sequence
will generate a
.PN BadState
error message.
.nH 1 "State Diagrams"
.LP
These state diagrams are designed to cover all actions of both
the client and the SM. 
.nH 2 "Client State Diagram"
.LP
.nf
.DS L 0
\fIstart:\fP
	ICE protocol setup complete \(-> \fCregister\fP
.DE
.sp
.DS L 0
\fIregister:\fP
	send \fBRegisterClient\fP \(-> \fCcollect-id\fP
.DE
.sp
.DS L 0
\fIcollect-id:\fP
	receive \fBRegisterClientReply\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIshutdown-cancelled:\fP
	send \fBSaveYourselfDone\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIidle:\fP [Undoes any freeze of interaction with user.] 
	receive \fBDie\fP \(-> \fCdie\fP
	receive \fBSaveYourself\fP \(-> \fCfreeze-interaction\fP
	send \fBGetProperties\fP \(-> \fCidle\fP
	receive \fBGetPropertiesReply\fP \(-> \fCidle\fP
	send \fBSetProperties\fP \(-> \fCidle\fP
	send \fBDeleteProperties\fP \(-> \fCidle\fP
	send \fBConnectionClosed\fP \(-> \fCconnection-closed\fP
	send \fBSaveYourselfRequest\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIdie:\fP
	send \fBConnectionClosed\fP \(-> \fCconnection-closed\fP
.DE
.sp
.DS L 0
\fIfreeze-interaction:\fP
	freeze interaction with user \(-> \fCsave-yourself\fP
.DE
.sp
.DS L 0
\fIsave-yourself:\fP
	receive \fBShutdownCancelled\fP \(-> \fCshutdown-cancelled\fP
	send \fBSetProperties\fP \(-> \fCsave-yourself\fP
	send \fBDeleteProperties\fP \(-> \fCsave-yourself\fP
	send \fBGetProperties\fP \(-> \fCsave-yourself\fP
	receive \fBGetPropertiesReply\fP \(-> \fCsave-yourself\fP
	send \fBInteractRequest\fP \(-> \fCinteract-request\fP
	send \fBSaveYourselfPhase2Request\fP -> waiting-for-phase2
	if shutdown mode:
		send \fBSaveYourselfDone\fP \(-> \fCsave-yourself-done\fP
	otherwise:
		send \fBSaveYourselfDone\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIwaiting-for-phase2:\fP
	receive \fBShutdownCancelled\fP \(-> \fCshutdown-cancelled\fP
	receive \fBSaveYourselfPhase2\fP \(-> \fCphase2\fP
.DE
.sp
.DS L 0
\fIphase2:\fP
	receive \fBShutdownCancelled\fP \(-> \fCshutdown-cancelled\fP
	send \fBSetProperties\fP \(-> \fCsave-yourself\fP
	send \fBDeleteProperties\fP \(-> \fCsave-yourself\fP
	send \fBGetProperties\fP \(-> \fCsave-yourself\fP
	receive \fBGetPropertiesReply\fP \(-> \fCsave-yourself\fP
	send \fBInteractRequest\fP \(-> \fCinteract-request\fP (errors only)
	if shutdown mode:
		send \fBSaveYourselfDone\fP \(-> \fCsave-yourself-done\fP
	otherwise:
		send \fBSaveYourselfDone\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIinteract-request:\fP
	receive \fBInteract\fP \(-> \fCinteract\fP
	receive \fBShutdownCancelled\fP \(-> \fCshutdown-cancelled\fP
.DE
.sp
.DS L 0
\fIinteract:\fP
	send \fBInteractDone\fP \(-> \fCsave-yourself\fP
	receive \fBShutdownCancelled\fP \(-> \fCshutdown-cancelled\fP
.DE
.sp
.DS L 0
\fIsave-yourself-done:\fP (changing state is forbidden)
	receive \fBSaveComplete\fP \(-> \fCidle\fP
	receive \fBDie\fP \(-> \fCdie\fP
	receive \fBShutdownCancelled\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIconnection-closed:\fP
	client stops participating in session
.DE
.ne 1i
.nH 2 "Session Manager State Diagram"
.LP
.nf
.DS L 0
\fIstart:\fP
	receive \fBProtocolSetup\fP \(-> \fCprotocol-setup\fP
.DE
.sp
.DS L 0
\fIprotocol-setup:\fP
	send \fBProtocolSetupReply\fP \(-> \fCregister\fP
.DE
.sp
.DS L 0
\fIregister:\fP
	receive \fBRegisterClient\fP \(-> \fCacknowledge-register\fP
.DE
.sp
.DS L 0
\fIacknowledge-register:\fP
	send \fBRegisterClientReply\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIidle:\fP
	receive \fBSetProperties\fP \(-> \fCidle\fP
	receive \fBDeleteProperties\fP \(-> \fCidle\fP
	receive \fBConnectionClosed\fP \(-> \fCstart\fP
	receive \fBGetProperties\fP \(-> \fCget-properties\fP
	receive \fBSaveYourselfRequest\fP \(-> \fCsave-yourself\fP
	send \fBSaveYourself\fP \(-> \fCsaving-yourself\fP
.DE
.sp
.DS L 0
\fIsave-yourself:\fP
	send \fBSaveYourself\fP \(-> \fCsaving-yourself\fP
.DE
.sp
.DS L 0
\fIget-properties:\fP
	send \fBGetPropertiesReply\fP \(-> \fCidle\fP
.DE
.sp
.DS L 0
\fIsaving-get-properties:\fP
	send \fBGetPropertiesReply\fP \(-> \fCsaving-yourself\fP
.DE
.sp
.DS L 0
\fIsaving-yourself:\fP
	receive \fBInteractRequest\fP \(-> \fCsaving-yourself\fP
	send \fBInteract\fP \(-> \fCsaving-yourself\fP
	send \fBShutdownCancelled\fP -> \fCidle\fP
	receive \fBInteractDone\fP \(-> \fCsaving-yourself\fP
	receive \fBSetProperties\fP \(-> \fCsaving-yourself\fP
 	receive \fBDeleteProperties\fP \(-> \fCsaving-yourself\fP
	receive \fBGetProperties\fP \(-> \fCsaving-get-properties\fP
	receive \fBSaveYourselfPhase2Request\fP \(-> \fCstart-phase2\fP
	receive \fBSaveYourselfDone\fP \(-> \fCsave-yourself-done\fP
.DE
.sp
.DS L 0
\fIstart-phase2:\fP	
	If all clients have sent either \fBSaveYourselfPhase2Request\fP or \fBSaveYourselfDone\fP:
		send \fBSaveYourselfPhase2\fP \(-> \fCphase2\fP
	else
		\(-> \fCsaving-yourself\fP
.DE
.sp
.DS L 0
\fIphase2:\fP
	receive \fBInteractRequest\fP \(-> \fCsaving-yourself\fP
	send \fBInteract\fP \(-> \fCsaving-yourself\fP
	send \fBShutdownCancelled\fP -> \fCidle\fP
	receive \fBInteractDone\fP \(-> \fCsaving-yourself\fP
	receive \fBSetProperties\fP \(-> \fCsaving-yourself\fP
 	receive \fBDeleteProperties\fP \(-> \fCsaving-yourself\fP
	receive \fBGetProperties\fP \(-> \fCsaving-get-properties\fP
	receive \fBSaveYourselfDone\fP \(-> \fCsave-yourself-done\fP
.DE
.sp
.DS L 0
\fIsave-yourself-done:\fP
	If all clients are saved:
		If shutting down:
			send \fBDie\fP \(-> \fCdie\fP
		otherwise
			send \fBSaveComplete\fP \(-> \fCidle\fP
.sp
	If some clients are not saved:
	\(-> \fCsaving-yourself\fP
.DE
.sp
.DS L 0
\fIdie:\fP
	SM stops accepting connections
.DE
.nH 1 "Protocol Encoding"
.nH 2 "Types"
.LP
.nf
.ta .2i .5i 2.0i
BOOL
	0	False
	1	True
.sp
INTERACT_STYLE
	0	None
	1	Errors
	2	Any
.sp
DIALOG_TYPE
	0	Error
	1	Normal
.sp
SAVE_TYPE
	0	Global
	1 	Local
	2 	Both
.sp
.ne .75i
ARRAY8
	4	CARD32	length
	n	LISTofCARD8	the array
	p		p = pad (4 + n, 8)
.sp
LISTofARRAY8
	4	CARD32	count
	4		unused
	a	ARRAY8	first array
	b	ARRAY8	second array
	\&.
	\&.
	\&.
	q	ARRAY8	last array
.sp
PROPERTY
	a	ARRAY8	name
	b	ARRAY8	type (XPCS encoded in Latin-1, case sensitive)
	c	LISTofARRAY8	values
.sp
LISTofPROPERTY
	4       CARD32	count
	4       	unused
	a       PROPERTY	first property
	b       PROPERTY	second property
	\&.
	\&.
	\&.
	q	PROPERTY	last property
.nH 2 "Messages"
.LP
XSMP is a sub-protocol of ICE.  The major opcode is assigned at run-time
by ICE and is represented here by `?'.
.LP
To start the XSMP protocol, the client sends the server an ICE
.PN ProtocolSetup
message.  
The protocol-name field should be specified as \*QXSMP\*U, the major
version of the protocol is 1, the minor version is 0.
These values may change if the protocol is revised.  The minor version
number will be incremented if the change is compatible, otherwise the major
version number will be incremented.
.LP
In 
.PN ProtocolReply
message sent by the session manager,
the XSMP protocol defines the vendor parameter as product identification
of the session manager, and defines the release parameter as
the software release identification of the session manager. 
The session manager should supply this information in the
ICE 
.PN ProtocolReply
message.
.LP
.nf
.ta .2i .5i 2.0i 
.ne 3
.PN RegisterClient
	1	?	XSMP
	1	1	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	ARRAY8	previous-ID
.ne 6
.sp
.PN RegisterClientReply
	1	?	XSMP
	1	2	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	ARRAY8	client-ID
.ne 4
.sp
.PN SaveYourself
	1	?	XSMP
	1	3	opcode
	2		unused
	4	1	length of remaining data in 8-byte units
	1	SAVE_TYPE	type
	1	BOOL	shutdown
	1	INTERACT_STYLE	interact-style
	1	BOOL	fast
	4		unused
.ne 4
.sp
.PN SaveYourselfRequest
	1	?	XSMP
	1	4	opcode
	2		unused
	4	1	length of remaining data in 8-byte units
	1	SAVE_TYPE	type
	1	BOOL	shutdown
	1	INTERACT_STYLE	interact-style
	1	BOOL	fast
	1	BOOL	global
	3		unused
.ne 4
.sp
.PN InteractRequest
	1	?	XSMP
	1	5	opcode
	1	DIALOG_TYPE	dialog type
	1		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN Interact
	1	?	XSMP
	1	6	opcode
	2		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN InteractDone
	1	?	XSMP
	1	7	opcode
	1	BOOL	cancel-shutdown
	1		unused
	4	0	length of remaining data in 8-byte units
.ne 6
.sp
.PN SaveYourselfDone
	1	?	XSMP
	1	8	opcode
	1	BOOL	success
	1		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN Die
	1	?	XSMP
	1	9	opcode
	2		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN ShutdownCancelled
	1	?	XSMP
	1	10	opcode
	2		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN ConnectionClosed
	1	?	XSMP
	1	11	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	LISTofARRAY8	reason
.ne 4
.sp
.PN SetProperties
	1	?	XSMP
	1	12	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	LISTofPROPERTY	properties
.ne 4
.sp
.PN DeleteProperties
	1	?	XSMP
	1	13	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	LISTofARRAY8	properties
.ne 4
.sp
.PN GetProperties
	1	?	XSMP
	1	14	opcode
	2		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN GetPropertiesReply
	1	?	XSMP
	1	15	opcode
	2		unused
	4	a/8	length of remaining data in 8-byte units
	a	LISTofPROPERTY	properties
.ne 4
.sp
.PN SaveYourselfPhase2Request
	1	?	XSMP
	1	16	opcode
	2		unused
	4	0	length of remaining data in 8-byte units
.ne 4
.sp
.PN SaveYourselfPhase2
	1	?	XSMP
	1	17	opcode
	2		unused
	4	0	length of remaining data in 8-byte units

.sp
.PN SaveComplete
	1	?	XSMP
	1	18	opcode
	2		unused
	4	0	length of remaining data in 8-byte units

.nH 1 "Predefined Properties"
.LP
All property values are stored in a LISTofARRAY8.  If the type of the
property is CARD8, the value is stored as a LISTofARRAY8 with one ARRAY8
that is one byte long.  That single byte contains the CARD8.  If the type of
the property is ARRAY8, the value is stored in the first element of a single
element LISTofARRAY8.
.LP
The required properties must be set each time a client
connects with the SM.  The properties must be set after
the client sends
.PN RegisterClient
and before the client sends
.PN SaveYourselfDone .
Otherwise, the behavior of
the session manager is not defined.
.LP
Clients may set, get, and delete nonstandard properties.
The lifetime of stored properties does not extend into 
subsequent sessions.
.br
.ne 6
.TS H
l l l c .
_
.sp 6p
.B
Name	Type	Posix Type	Required?
.R
.sp 6p
_
.sp 6p
.TH
CloneCommand	OS-specific	LISTofARRAY8	Yes
CurrentDirectory	OS-specific	ARRAY8	No
DiscardCommand	OS-specific	LISTofARRAY8	No*
Environment	OS-specific	LISTofARRAY8	No
ProcessID	OS-specific	ARRAY8	No
Program	OS-specific	ARRAY8	Yes
RestartCommand	OS-specific	LISTofARRAY8	Yes
ResignCommand	OS-specific	LISTofARRAY8	No
RestartStyleHint	CARD8	CARD8	No
ShutdownCommand	OS-specific	LISTofARRAY8	No
UserID	ARRAY8	ARRAY8	Yes
.sp 6p
_
.TE
.LP
* Required if any state is stored in an external repository (e.g., state file).
.IP CloneCommand 3
This is like the 
.PN RestartCommand 
except it restarts a copy of the
application.  The only difference is that the application doesn't
supply its client id at register time.  On POSIX systems the type
should be a LISTofARRAY8.
.IP CurrentDirectory 3
On POSIX-based systems specifies the value of the current directory that
needs to be set up prior to starting the program and should be of type
ARRAY8.
.IP DiscardCommand 3
The discard command contains a command that when delivered to the host that 
the client is running on (determined from the connection), will
cause it to discard any information about the current state.  If this command
is not specified, the SM will assume that all of the client's state is encoded
in the 
.PN Restart\%Command .
On POSIX systems the type should be LISTofARRAY8.
.IP Environment 3
On POSIX based systems, this will be of type LISTofARRAY8 where
the ARRAY8s alternate between environment variable name and environment
variable value.  
.IP ProcessID 3
This specifies an OS-specific identifier for the process.  On POSIX
systems this should of type ARRAY8 and contain the return value 
of getpid() turned into a Latin-1 (decimal) string.
.IP Program 3
The name of the program that is running.  On POSIX systems this 
should be the
first parameter passed to execve and should be of type ARRAY8.
.IP RestartCommand 3
The restart command contains a command that when delivered to the
host that the client is running on (determined from the connection),
will cause the client to restart in
its current state.  On POSIX-based systems this is of type LISTofARRAY8
and each of the elements in the array represents an element in
the argv array.
This restart command should ensure that the client restarts with the specified
client-ID.
.IP ResignCommand 3
A client that sets the
.PN RestartStyleHint
to
.PN RestartAnyway
uses this property to specify a command 
that undoes the effect of the client and removes
any saved state.
.NT Example
A user runs xmodmap.  xmodmap registers with the SM, sets 
.PN Restart\%Style\%Hint
to 
.PN Restart\%Anyway ,
and then terminates.  In order to allow the SM (at the
user's request) to undo this, xmodmap would register a
.PN Resign\%Command
that undoes the effects of the xmodmap.
.NE
.IP RestartStyleHint 3
.RS
.LP
If the RestartStyleHint property is present, it will contain the 
style of restarting the client prefers.  If this flag isn't specified,
.PN RestartIfRunning
is assumed.
The possible values are as follows:
.br
.ne 6
.TS H
l n.
_
.sp 6p
.B
Name	Value
.R
.sp 6p
_
.sp 6p
.TH
RestartIfRunning	0
RestartAnyway	1
RestartImmediately	2
RestartNever	3
.sp 6p
_
.TE
.LP
The
.PN RestartIfRunning
style is used in the usual case.  The client should
be restarted in the next session if it is connected to the 
session manager at the end of the current session.
.LP
The
.PN RestartAnyway
style is used to tell the SM that the application
should be restarted in the next session even if it exits before the 
current session is terminated.
It should be noted that this is only a hint and the SM
will follow the policies specified by its users in determining what applications
to restart.
.LP
.NT Rationale
This can be specified by a client which supports (as MS-Windows clients
do) a means for the user to indicate while exiting that
restarting is desired.  It can also be used for clients that
spawn other clients and then go away, but which want to be
restarted.
.NE
.LP
A client that uses
.PN RestartAnyway
should also set the
.PN ResignCommand
and
.PN ShutdownCommand
properties to commands that undo the state of the client
after it exits.
.LP
The
.PN RestartImmediately
style is like
.PN RestartAnyway ,
but in addition, the
client is meant to run continuously.  If the client exits, the
SM should try to restart it in the current session.
.NT "Advice to Implementors"
It would be wise to sanity-check the frequency which which
.PN RestartImmediately
clients are restarted, to avoid a sick
client being restarted continuously.
.NE
The
.PN RestartNever
style specifies that the client 
does not wish to be restarted in the next session.
.NT "Advice To Implementors"
This should be used rarely, if at all.  It will cause the client
to be silently left out of sessions when they are restarted and
will probably be confusing to users.
.NE
.RE
.IP ShutdownCommand
This command is executed at shutdown time to clean up after a client that
is no longer running but retained its state by setting
.PN RestartStyleHint
to 
.PN RestartAnyway .
The command must not remove any saved state as the client is still part of
the session.
.NT Example
A client is run at start up time that turns on a camera.  This client then
exits.  At session shutdown, the user wants the camera turned off.  This client
would set the 
.PN Restart\%Style\%Hint
to 
.PN Restart\%Anyway
and would register a 
.PN Shutdown\%Command
that would turn off the camera.
.NE
.IP UserID 3
Specifies the user's ID.  On POSIX-based systems this
will contain the the user's name (the pw_name field of struct passwd).
.\" Finish up.
.YZ 3
