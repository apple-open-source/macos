.\" Use -ms and macros.t
.\" $XFree86: xc/doc/specs/Xext/DPMS.ms,v 1.2 1998/12/20 11:56:39 dawes Exp $
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.sp 8
.ce 1
\s+2\fBX Display Power Management Signaling (DPMS) Extension\fP\s-2
.sp
.ce 1
\s+1\fBProtocol Specification\fP\s-1
.sp 3
.ce 3
Version 1.0
X Project Team Standard
X Version 11, Release 6.4
.sp 6
.ce 4
Rob Lembree
\fIlembree@zk3.dec.com\fP
.sp 6p
Digital Equipment Corporation
.sp 6p
24 April 1996
.ps 9
.nr PS 9
.sp 8
.LP
.bp
.sp 15
Copyright \(co Digital Equipment Corporation, 1996
.LP
Permission to use, copy, modify, distribute, and sell this
documentation for any purpose is hereby granted without fee,
provided that the above copyright notice and this permission
notice appear in all copies.  Digital Equipment Corporation
makes no representations about the suitability for any purpose
of the information in this document.  This documentation is
provided ``as is'' without express or implied warranty.
.LP
.ps 10
.nr PS 10
.bp 1
.EH '\fBDisplay Power Management Signaling (DPMS) Extension\fP'
.OH '\fBDisplay Power Management Signaling (DPMS) Extension\fP'
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.NH 1
Overview
.LP
This extension provides X Protocol control over the VESA Display
Power Management Signaling (DPMS) characteristics of video boards
under control of the X Window System.
.FS
1. \fIX Window System\fP is a trademark of X Consortium, Inc.
.FE
.LP
Traditionally, the X Window System has provided for both blanking and
non-blanking screen savers.  Timeouts associated with these built-in
screen saver mechanisms are limited to idle (dwell) time, and a change
timeout that specifies the change interval for non-blanking screen savers.
.LP
The United States' Environmental Protection Agency (EPA) Energy Star program
requires that monitors power down after some idle time by default.
While it is possible to simply overload the existing screen saver timeouts,
this solution leaves the non-privileged user little to no control over
the DPMS characteristics of his or her system.  For example, disabling 
DPMS would require some unintended side effect in the core screen saver,
such as disabling the changing of a non-blanking screen saver.  Providing
clients with this control requires an extension to the core X Window System
Protocol, and this extension seeks to fill this gap.
.LP
The design goal of the DPMS extension is to be a logical extension to
the traditional screen saver.  The protocol and sample implementation is
designed to use the same date types and time units as the screen saver. 
The sample implementation works independently from the screen saver so that
policy as it pertains to the interaction between screen saver and DPMS can
be deferred to the user or screen saver application. The extension has
been tested with and shown to work correctly with both the internal blanking
and non-blanking screen savers, as well as with screen saver extension
clients.
.LP
The DPMS extension is designed to be simple, yet export sufficient
VESA DPMS information to enable full function clients to be written.  
Included is the ability to sense DPMS capability, set and get DPMS timeouts,
enable and disable individual DPMS modes, enable and disable DPMS (without
destroying timeout values), and sense current DPMS on/off state and
power level. 
.LP
There are four power levels specified by the Video Electronics Standards
Association (VESA) Display Power Management Signaling (DPMS) standard.
These are:
.LP
.Ds 0
.TA .2i 1.5i 2.5i 3.5i
.ta .2i 1.5i 2.5i 3.5i
.R
.PN "DPMS Extension Power Levels"
	0	DPMSModeOn		In use
	1	DPMSModeStandby		Blanked, low power
	2	DPMSModeSuspend		Blanked, lower power
	3	DPMSModeOff		Shut off, awaiting activity
.De
.LP
.LP
It is logical to assume that successive DPMS modes be chronologically
at the same time or later than one another, and the protocol is designed
to enforce this rule.
.LP
Note however that a concious decision is made to decouple the timeouts
associated with screen saver from the DPMS timeouts.  While it might be
considered logical to require that the first non-zero DPMS timeout be
greater than or equal to the screen saver timeout, this is intentionally
omitted, leaving this policy decision to the user or the screen saver
application.  In the case of a laptop where power may be scarce, the
importance of power savings should supersede the screen saver.  If the
laptop user plugs the unit in and power is no longer a scarce commodity,
it may be decided to make DPMS less aggressive, or disable it completely.
.LP
.NH 1
Requests
.LP
.PN "DPMSGetVersion"
.LP
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
mechanism to support future revisions of the Display Power Management 
Signaling protocol which may be necessary.  In general, the major version
would increment for incompatible changes, and the minor version would
increment for small, upward-compatible changes.  Servers that support the
protocol defined in this document will return a \fIserver_major_version\fP
of one (1), and a \fIserver_minor_version\fP of one (1).
.LP
.PN "DPMSCapable"
.IP
   =>
.IP
\fIcapable\fP\^: BOOL
.LP
This request is used to determine whether or not the currently running
server's devices are capable of DPMS operations.  The truth value of this
request is implementation defined, but is generally based on the capabilities
of the graphic card and monitor combination.  Also, the return value in the
case of heterogeneous multi-head servers is implementation defined.  
.LP
.PN "DPMSGetTimeouts"
.LP
   =>
.IP
\fIstandby_timeout\fP\^: CARD16
.br
\fIsuspend_timeout\fP\^: CARD16
.br
\fIoff_timeout\fP\^: CARD16
.LP
This request returns the current values of the DPMS timeout values.  All
values are in units of seconds.
.LP
\fIstandby_timeout\fP is the amount of time of inactivity before standby
mode is invoked. The actual effects of this mode are implementation defined,
but in the case of DPMS compliant hardware, it is implemented by shutting off
the horizontal sync signal, and pulsing the vertical sync signal. Standby
mode provides the quickest monitor recovery time.  Note also that many
monitors implement this mode identically to suspend mode.  A value of
zero indicates that this mode is disabled.
.LP
\fIsuspend_timeout\fP is the amount of time of inactivity before the second
level of power savings is invoked. Suspend mode's physical and electrical
characteristics are implementation defined, but in DPMS compliant hardware,
results in the pulsing of the horizontal sync signal, and shutting off of
the vertical sync signal.  Suspend mode recovery is considered to be slower
than standby mode, but faster than off mode, however this is monitor 
dependent.  As noted above, many monitors implement this mode identically to
standby mode.  A value of zero indicates that this mode is disabled.
.LP
\fIoff_timeout\fP is the amount of time of inactivity before the third and
final level of power savings is invoked. Off mode's physical and electrical
characteristics are implementation defined, but in DPMS compliant hardware,
is implemented by shutting off both horizontal and vertical sync signals,
resulting in the power-down of the monitor.  Recovery time is implementation
dependant, but frequently is similar to the power-up time of the monitor. A
value of zero indicates that this mode is disabled.
.LP
.PN "DPMSSetTimeouts"
.IP
\fIstandby_timeout\fP\^: CARD16
.br
\fIsuspend_timeout\fP\^: CARD16
.br
\fIoff_timeout\fP\^: CARD16
   =>
.IP
.LP
All values are in units of seconds. \fIstandby_timeout\fP is the amount of
time of inactivity before standby mode will be invoked. This is the
lightest level of power savings, and the monitor is generally immediately
ready upon detection of user activity.  This is most often implemented by
shutting off the horizontal sync signal to the monitor.  
A value of zero disables this mode.
.LP
The \fIsuspend_timeout\fP specifies the amount of time of inactivity 
before the screen is placed into suspend mode.  Suspend mode is the
middle level of power savings, resulting in a slightly longer recovery
upon detection of activity.  Suspend mode is most often implemented by
pulsing the horizontal sync signal, and removing the vertical sync
signal. A value of zero disables this mode.
.LP
The \fIoff_timeout\fP specifies the amount of time of inactivity before
the monitor is shut off.  Off mode is the deepest level of power management,
resulting in the greatest power savings and the longest recovery time.
Off mode is most often implemented by removing both the horizontal and
vertical signals. A value of zero disables this mode.
.LP
The values of successive power levels must be greater than or equal
to the value of the previous (non-zero) level.  A BadValue error is generated
if an illegal combination is detected.

.LP
.PN "DPMSEnable"
.IP
   =>
.IP
.LP
This request enables the DPMS characteristics of the server, using the
server's currently stored timeouts.  If DPMS is already enabled, no change is
effected.

.LP
.PN "DPMSDisable"
.IP
   =>
.IP
.LP
This request disables the DPMS characteristics of the server.  It does
not affect the core or extension screen savers.  If DPMS is already
disabled, no change is effected.  This request is provided so that DPMS
may be disabled without damaging the server's stored timeout values.
.LP
.PN "DPMSForceLevel"
.IP
\fIpower_level\fP\^: CARD16
   =>
.IP
.LP
This request forces a specific DPMS level on the server.  If DPMS is
disabled, a BadMatch error is generated.  If an erroneous power level
is specified, a BadValue error is returned, and the error value contains
the bad value.  If the power level specified is already in effect, no
changes occur.  Power Level must be one of DPMSModeOn, DPMSModeStandby,
DPMSModeSuspend or DPMSModeOff.
.LP
.PN "DPMSInfo"
.IP
   =>
.IP
\fIpower_level\fP\^: CARD16
.br
\fIstate\fP\^: BOOL
.LP
This request returns information about the current DPMS state of the 
display.  \fIstate\fP\^ is one of DPMSEnabled or DPMSDisabled.
If \fIstate\fP\^ is DPMSEnabled, \fIpower_level\fP\^ is returned as one
of DPMSModeOn, DPMSModeStandby, DPMSModeSuspend or DPMSModeOff, otherwise
it is undefined.
.LP
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
The name of this extension is "DPMS".
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSGetVersion"
	1	CARD8		opcode
	1	0		DPMS opcode
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
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSCapable"
	1	CARD8		opcode
	1	1		DPMS opcode
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
	1	BOOL		capable
	23			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSGetTimeouts"
	1	CARD8		opcode
	1	2		DPMS opcode
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
	2	CARD16		standby_timeout
	2	CARD16		suspend_timeout
	2	CARD16		off_timeout
	18			unused				
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSSetTimeouts"
	1	CARD8		opcode
	1	3		DPMS opcode
	2	3		request length
	2	CARD16		standby_timeout
	2	CARD16		suspend_timeout
	2	CARD16		off_timeout
	2			unused
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSEnable"
	1	CARD8		opcode
	1	4		DPMS opcode
	2	1		request length
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSDisable"
	1	CARD8		opcode
	1	5		DPMS opcode
	2	1		request length
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSForceLevel"
	1	CARD8		opcode
	1	6		DPMS opcode
	2	2		request length
	2			power_level
		0	DPMSModeOn
		1	DPMSModeStandby
		2	DPMSModeSuspend
		3	DPMSModeOff
	2			unused
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN "DPMSInfo"
	1	CARD8		opcode
	1	7		DPMS opcode
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
	2			power_level
		0	DPMSModeOn
		1	DPMSModeStandby
		2	DPMSModeSuspend
		3	DPMSModeOff
	1	BOOL		state
	21			unused				
