.\" Use -ms and macros.t
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
\s+1\fBLibrary Specification\fP\s-1
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
There are four power levels specified by the Video Electronics Standards
Association (VESA) Display Power Management Signaling (DPMS) standard.
These are mapped onto the X DPMS Extension like this:
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

.NH 1
DPMS Functions

.PN DPMSQueryExtension
.LP

Bool DPMSQueryExtension \^(\fIdisplay, event_base, error_base\fP\^)
.IP
    Display \fI*display;\fP
.br
    int \fI*event_base, *error_base;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.
.br
\fIevent_base\fP	Specifies the return location for the assigned base event
.br
\fIerror_base\fP	Specifies the return location for the assigned base error

.LP
The DPMSQueryExtension function queries the X server to determine the availability
of the DPMS Extension.  If the extension is available, the return value is TRUE,
and \fIevent_base\fP and \fIerror_base\fP are set to the base event number and base
error number for the extension, respectively.  Otherwise, the return value is
FALSE, and the values of \fIevent_base\fP and \fIerror_base\fP are undefined.
.LP

.PN DPMSGetVersion
.LP

Status DPMSGetVersion\^(\fIdisplay, major_version, minor_version\fP\^)
.IP
    Display \fI*display;\fP
.br
    int     \fI*major_version, *minor_version;\fP
.LP
\fIdisplay\fP		Specifies the connection to the X server.
.br
\fImajor_version\fP	Specifies the return location for the extension major version.
.br
\fIminor_version\fP	Specifies the return location for the extension minor version.

.LP
The DPMSGetVersion function returns the version of the DPMS extension implemented by
the X server.  The version is returned in \fImajor_version\fP and \fIminor_version\fP.
The major version and minor version for this specification are '1' and '1', respectively.
The major version will be incremented for protocol incompatible changes, and the minor
version will be incremented for small, upwardly compatible changes.
.LP

.PN DPMSCapable
.LP

Bool DPMSCapable\^(\fIdisplay\fP\^)
.IP
    Display \fI*display;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.

.LP
The DPMSCapable function returns the DPMS capability of the X server, either
TRUE \^(capable of DPMS\^) or FALSE \^(incapable of DPMS\^).  The capability 
of an X server is implementation defined.  For example, if a multi-headed  X server
is capable of DPMS on one head, and incapable on another, the truth value of this
function is defined by the X server implementation.
.LP

.PN DPMSSetTimeouts
.LP


Status DPMSSetTimeouts\^(\fIdisplay, standby, suspend, off\fP\^)
.IP
    Display \fI*display;\fP
.br
    CARD16 \fIstandby, suspend, off;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.
.br
\fIstandby\fP	Specifies the new standby timeout in seconds.
.br
\fIsuspend\fP	Specifies the new suspend timeout in seconds.
.br
\fIoff\fP		Specifies the new off timeout in seconds.

.LP
The DPMSSetTimeouts function permits applications to set the timeout values
used by the X server for DPMS timings.  
.LP
The value \fIstandby\fP is the amount of time of inactivity in seconds before
standby mode is invoked. The actual effects of this mode are implementation
defined, but in the case of DPMS compliant hardware, it is implemented by
shutting off the horizontal sync signal, and pulsing the vertical sync signal.
Standby mode provides the quickest monitor recovery time.  Note also that many
monitors implement this mode identically to suspend mode.  A value of zero
disables this mode.
.LP
The value \fIsuspend\fP is the amount of time of inactivity in seconds
before the second level of power savings is invoked. Suspend mode's physical
and electrical characteristics are implementation defined, but in DPMS compliant
hardware, results in the pulsing of the horizontal sync signal, and shutting off
of the vertical sync signal.  Suspend mode recovery is considered to be slower
than standby mode, but faster than off mode, however this is monitor 
dependent.  As noted above, many monitors implement this mode identically to
standby mode.  A value of zero disables this mode.
.LP
The value \fIoff\fP is the amount of time of inactivity in seconds before the third and
final level of power savings is invoked. Off mode's physical and electrical
characteristics are implementation defined, but in DPMS compliant hardware,
is implemented by shutting off both horizontal and vertical sync signals,
resulting in the power-down of the monitor.  Recovery time is implementation
dependant, but frequently is similar to the power-up time of the monitor. 
A value of zero disables this mode.
.LP
Chronologically, standby mode occurs before or simultaneously with suspend mode, and
suspend mode must occur before or simultaneously with off mode.  Therefore,
non-zero mode timeout values must be greater than or equal to the timeout values of
earlier modes.  If inconsistent values are supplied, a BadValue error will result.
.LP

.PN DPMSGetTimeouts
.LP


Bool DPMSGetTimeouts\^(\fIdisplay, standby, suspend, off\fP\^)
.IP
    Display \fI*display;\fP
.br
    CARD16 \fI*standby, *suspend, *off;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.
.br
\fIstandby\fP	Specifies the current standby timeout in seconds.
.br
\fIsuspend\fP	Specifies the current suspend timeout in seconds.
.br
\fIoff\fP		Specifies the current off timeout in seconds.

.LP
The DPMSGetTimeouts function retrieves the timeout values used by the X
server for DPMS timings.  
.LP
The value \fIstandby\fP is the amount of time of inactivity in seconds before
standby mode is invoked. A value of zero indicates that this mode has been disabled.
.LP
The value \fIsuspend\fP is the amount of time of inactivity in seconds before the
second level of power savings is invoked.  A value of zero indicates that this mode
has been disabled.
.LP
The value \fIoff\fP is the amount of time of inactivity in seconds before the third
and final level of power savings is invoked. A value of zero indicates that this mode
has been disabled.
.LP

.PN DPMSEnable
.LP


Status DPMSEnable\^(\fIdisplay\fP\^)
.IP
    Display \fI*display;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.

.LP
The DPMSEnable function enables DPMS on the specified display.  When
enabled, DPMS will use the currently saved timeout values, and will
invoke the DPMS power mode appropriate for the amount of time that
the workstation input devices have been idle.  If DPMSEnable is invoked
on a display with DPMS already enabled, no change is made, and no
error is returned.  If DPMSEnable is invoked on a display without
support for DPMS, no change is made and no error is returned.
.LP

.PN DPMSDisable
.LP


Status DPMSDisable\^(\fIdisplay\fP\^)
.IP
    Display \fI*display;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.

.LP
The DPMSDisable function disables DPMS on the specified display.  When
disabled, DPMS returns the display to DPMSModeOn.  If DPMSDisable is
invoked on a display with DPMS already disabled, no change is made,
and no error is returned.  If DPMSDisable is invoked on a display
without support for DPMS, no change is made and no error is returned.
.LP

.PN DPMSForceLevel
.LP


Status DPMSForceLevel\^(\fIdisplay, level\fP\^)
.IP
    Display \fI*display;\fP
.br
    CARD16 \fIlevel;\fP
.LP
\fIdisplay\fP	Specifies the connection to the X server.
.br
\fIlevel\fP	Specifies the level to force power to.

.LP
The DPMSForceLevel function forces a DPMS capable display into the
specified power level.  The \fIlevel\fP must be one of 
DPMSModeOn, DPMSModeStandby, DPMSModeSuspend, or DPMSModeOff.
Values other than these will result in a BadValue error.  If DPMS
is disabled on the display, a BadMatch protocol error will result.
.LP

.PN DPMSInfo
.LP


Status DPMSInfo\^(\fIdisplay, power_level, state\fP\^)
.IP
    Display \fI*display;\fP
.br
    CARD16 \fI*power_level;\fP
.br
    BOOL \fI*state;\fP
.LP
\fIdisplay\fP		Specifies the connection to the X server.
.br
\fIpower_level\fP		Specifies the current power level.
.br
\fIstate\fP			Specifies the current DPMS state

.LP
The DPMSInfo function returns information about the current DPMS state.
The \fIstate\fP return parameter indicates whether or not DPMS is enabled 
\^(TRUE\^) or
disabled \^(FALSE\^).  The \fIpower_level\fP return parameter indicates the current
power level \^(one of DPMSModeOn, DPMSModeStandby, DPMSModeSuspend, or
DPMSModeOff.\^)
.LP

