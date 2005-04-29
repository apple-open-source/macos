.\" $Xorg: xdm.man,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $
.\" Copyright 1988, 1994, 1998  The Open Group
.\"
.\" Permission to use, copy, modify, distribute, and sell this software and its
.\" documentation for any purpose is hereby granted without fee, provided that
.\" the above copyright notice appear in all copies and that both that
.\" copyright notice and this permission notice appear in supporting
.\" documentation.
.\"
.\" The above copyright notice and this permission notice shall be included
.\" in all copies or substantial portions of the Software.
.\"
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
.\" OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
.\" MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
.\" IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
.\" OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
.\" ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
.\" OTHER DEALINGS IN THE SOFTWARE.
.\"
.\" Except as contained in this notice, the name of The Open Group shall
.\" not be used in advertising or otherwise to promote the sale, use or
.\" other dealings in this Software without prior written authorization
.\" from The Open Group.
.\"
.\" $XFree86: xc/programs/xdm/xdm.man,v 3.29 2004/01/12 21:43:19 herrb Exp $
.\"
.TH XDM 1 __xorgversion__
.SH NAME
xdm \- X Display Manager with support for XDMCP, host chooser
.SH SYNOPSIS
.B xdm
[
.B \-config
.I configuration_file
] [
.B \-nodaemon
] [
.B \-debug
.I debug_level
] [
.B \-error
.I error_log_file
] [
.B \-resources
.I resource_file
] [
.B \-server
.I server_entry
] [
.B \-session
.I session_program
]
.SH DESCRIPTION
.I Xdm
manages a collection of X displays, which may be on the local host
or remote servers.  The design of
.I xdm
was guided by the needs of X terminals as well as The Open Group standard
XDMCP, the \fIX Display Manager Control Protocol\fP.
.I Xdm
provides services similar to those provided by \fIinit\fP, \fIgetty\fP
and \fIlogin\fP on character terminals: prompting for login name and password,
authenticating the user, and running a ``session.''
.PP
A ``session'' is defined by the lifetime of a particular process; in the
traditional character-based terminal world, it is the user's login shell.
In the
.I xdm
context, it is an arbitrary session manager.  This is because in a windowing
environment, a user's login shell process does not necessarily have any
terminal-like interface with which to connect.
When a real session manager is not available, a window manager or terminal
emulator is typically used as the ``session manager,'' meaning that
termination of this process terminates the user's session.
.PP
When the session is terminated, \fIxdm\fP
resets the X server and (optionally) restarts the whole process.
.PP
When \fIxdm\fP receives an Indirect query via XDMCP, it can run a
\fIchooser\fP process to
perform an XDMCP BroadcastQuery (or an XDMCP Query to specified hosts)
on behalf of the display and
offer a menu of possible hosts that offer XDMCP display management.
This feature is useful with X terminals that do not offer a host
menu themselves.
.PP
.I Xdm
can be configured to ignore BroadcastQuery messages from selected hosts.
This is useful when you don't want the host to appear in menus produced
by
.I chooser
or X terminals themselves.
.PP
Because
.I xdm
provides the first interface that users will see, it is designed to be
simple to use and easy to customize to the needs of a particular site.
.I Xdm
has many options, most of which have reasonable defaults.  Browse through the
various sections of this manual,
picking and choosing the things you want to change.
Pay particular attention to the
.B "Session Program"
section, which will describe how to
set up the style of session desired.
.SH "OVERVIEW"
\fIxdm\fP is highly configurable, and most of its behavior can be
controlled by resource files and shell scripts.  The names of these
files themselves are resources read from the file \fIxdm-config\fP or
the file named by the \fB\-config\fP option.
.PP
\fIxdm\fP offers display management two different ways.  It can manage
X servers running on the local machine and specified in
\fIXservers\fP, and it can manage remote X servers (typically X
terminals) using XDMCP (the XDM Control Protocol)
as specified in the \fIXaccess\fP file.
.PP
The resources of the X clients run by \fIxdm\fP outside the user's
session, including \fIxdm\fP's own login window, can be
affected by setting resources in the \fIXresources\fP file.
.PP
For X terminals that do not offer a menu of hosts to get display
management from, \fIxdm\fP can collect willing hosts and run the
\fIchooser\fP program to offer the user a menu.
For X displays attached to a host, this step is typically not used, as
the local host does the display management.
.PP
After resetting the X server, \fIxdm\fP runs the \fIXsetup\fP script
to assist in setting up the screen the user sees along with the
\fIxlogin\fP widget.
.PP
The \fIxlogin\fP widget, which \fIxdm\fP presents,
offers the familiar login and password prompts.
.PP
After the user logs in, \fIxdm\fP runs the \fIXstartup\fP script as
root.
.PP
Then \fIxdm\fP runs the \fIXsession\fP script as the user.  This
system session file may do some additional startup and typically runs
the \fI.xsession\fP script in the user's home directory.
When the \fIXsession\fP script exits, the session is over.
.PP
At the end of the session, the \fIXreset\fP script is run to clean up,
the X server is reset, and the cycle starts over.
.PP
The file \fI __projectroot__/lib/X11/xdm/xdm-errors\fP will contain error
messages from
.I xdm
and anything output to stderr by \fIXsetup, Xstartup, Xsession\fP
or \fIXreset\fP.
When you have trouble getting
.I xdm
working, check this file to see if
.I xdm
has any clues to the trouble.
.SH OPTIONS
.PP
All of these options, except \fB\-config\fP itself,
specify values that can also be specified in the configuration file
as resources.
.IP "\fB\-config\fP \fIconfiguration_file\fP"
Names the configuration file, which specifies resources to control
the behavior of
.I xdm.
.I __projectroot__/lib/X11/xdm/xdm-config
is the default.
See the section \fBConfiguration File\fP.
.IP "\fB\-nodaemon\fP"
Specifies ``false'' as the value for the \fBDisplayManager.daemonMode\fP
resource.
This suppresses the normal daemon behavior, which is for
.I xdm
to close all file descriptors, disassociate itself from
the controlling terminal, and put
itself in the background when it first starts up.
.IP "\fB\-debug\fP \fIdebug_level\fP"
Specifies the numeric value for the \fBDisplayManager.debugLevel\fP
resource.  A non-zero value causes
.I xdm
to print lots of debugging statements to the terminal; it also disables the
\fBDisplayManager.daemonMode\fP resource, forcing
.I xdm
to run synchronously.  To interpret these debugging messages, a copy
of the source code for
.I xdm
is almost a necessity.  No attempt has been
made to rationalize or standardize the output.
.IP "\fB\-error\fP \fIerror_log_file\fP"
Specifies the value for the \fBDisplayManager.errorLogFile\fP resource.
This file contains errors from
.I xdm
as well as anything written to stderr by the various scripts and programs
run during the progress of the session.
.IP "\fB\-resources\fP \fIresource_file\fP"
Specifies the value for the \fBDisplayManager*resources\fP resource.  This file
is loaded using
.I xrdb
to specify configuration parameters for the
authentication widget.
.IP "\fB\-server\fP \fIserver_entry\fP"
Specifies the value for the \fBDisplayManager.servers\fP resource.
See the section
.B "Local Server Specification"
for a description of this resource.
.IP "\fB\-udpPort\fP \fIport_number\fP"
Specifies the value for the \fBDisplayManager.requestPort\fP resource.  This
sets the port-number which
.I xdm
will monitor for XDMCP requests.  As XDMCP
uses the registered well-known UDP port 177, this resource should
not be changed except for debugging. If set to 0 xdm will not listen
for XDMCP or Chooser requests.
.IP "\fB\-session\fP \fIsession_program\fP"
Specifies the value for the \fBDisplayManager*session\fP resource.  This
indicates the program to run as the session after the user has logged in.
.IP "\fB\-xrm\fP \fIresource_specification\fP"
Allows an arbitrary resource to be specified, as in most
X Toolkit applications.
.SH RESOURCES
At many stages the actions of
.I xdm
can be controlled through the use of its configuration file, which is in the
X resource format.
Some resources modify the behavior of
.I xdm
on all displays,
while others modify its behavior on a single display.  Where actions relate
to a specific display,
the display name is inserted into the resource name between
``DisplayManager'' and the final resource name segment.
.PP
For local displays, the resource name and class are as read from the
\fIXservers\fP file.
.PP
For remote displays, the resource name is what the network address of
the display resolves to.  See the \fBremoveDomain\fP resource.  The
name must match exactly; \fIxdm\fP is not aware of
all the network aliases that might reach a given display.
If the name resolve fails, the address is
used.  The resource class is as sent by the display in the XDMCP
Manage request.
.PP
Because the resource
manager uses colons to separate the name of the resource from its value and
dots to separate resource name parts,
.I xdm
substitutes underscores for both dots and colons when generating the resource
name.
For example, \fBDisplayManager.expo_x_org_0.startup\fP is the name of the
resource which defines the startup shell file for the ``expo.x.org:0'' display.
.\"
.IP "\fBDisplayManager.servers\fP"
This resource either specifies a file name full of server entries, one per
line (if the value starts with a slash), or a single server entry.
See the section \fBLocal Server Specification\fP for the details.
.IP "\fBDisplayManager.requestPort\fP"
This indicates the UDP port number which
.I xdm
uses to listen for incoming XDMCP requests.  Unless you need to debug the
system, leave this with its default value of 177.
.IP "\fBDisplayManager.errorLogFile\fP"
Error output is normally directed at the system console.  To redirect it,
set this resource to a file name.  A method to send these messages to
.I syslog
should be developed for systems which support it; however, the
wide variety of interfaces precludes any system-independent
implementation.  This file also contains any output directed to stderr
by the \fIXsetup, Xstartup, Xsession\fP and \fIXreset\fP files,
so it will contain descriptions
of problems in those scripts as well.
.IP "\fBDisplayManager.debugLevel\fP"
If the integer value of this resource is greater than zero,
reams of
debugging information will be printed.  It also disables daemon mode, which
would redirect the information into the bit-bucket, and
allows non-root users to run
.I xdm,
which would normally not be useful.
.IP "\fBDisplayManager.daemonMode\fP"
Normally,
.I xdm
attempts to make itself into a daemon process unassociated with any terminal.
This is
accomplished by forking and leaving the parent process to exit, then closing
file descriptors and releasing the controlling terminal.  In some
environments this is not desired (in particular, when debugging).  Setting
this resource to ``false'' will disable this feature.
.IP "\fBDisplayManager.pidFile\fP"
The filename specified will be created to contain an ASCII
representation of the process-id of the main
.I xdm
process.
.I Xdm
also uses file locking on this file
to attempt to eliminate multiple daemons running on
the same machine, which would cause quite a bit of havoc.
.IP "\fBDisplayManager.lockPidFile\fP"
This is the resource which controls whether
.I xdm
uses file locking to keep multiple display managers from running amok.
On System V, this
uses the \fIlockf\fP library call, while on BSD it uses \fIflock.\fP
.IP "\fBDisplayManager.authDir\fP"
This names a directory under which
.I xdm
stores authorization files while initializing the session.  The
default value is \fI __projectroot__/lib/X11/xdm.\fP
Can be overridden for specific displays by
DisplayManager.\fIDISPLAY\fP.authFile.
.IP \fBDisplayManager.autoRescan\fP
This boolean controls whether
.I xdm
rescans the configuration, servers, access control and authentication keys
files after a session terminates and the files have changed.  By default it
is ``true.''  You can force
.I xdm
to reread these files by sending a SIGHUP to the main process.
.IP "\fBDisplayManager.removeDomainname\fP"
When computing the display name for XDMCP clients, the name resolver will
typically create a fully qualified host name for the terminal.  As this is
sometimes confusing,
.I xdm
will remove the domain name portion of the host name if it is the same as the
domain name of the local host when this variable is set.  By default the
value is ``true.''
.IP "\fBDisplayManager.keyFile\fP"
XDM-AUTHENTICATION-1 style XDMCP authentication requires that a private key
be shared between
.I xdm
and the terminal.  This resource specifies the file containing those
values.  Each entry in the file consists of a display name and the shared
key.  By default,
.I xdm
does not include support for XDM-AUTHENTICATION-1, as it requires DES which
is not generally distributable because of United States export restrictions.
.IP \fBDisplayManager.accessFile\fP
To prevent unauthorized XDMCP service and to allow forwarding of XDMCP
IndirectQuery requests, this file contains a database of hostnames which are
either allowed direct access to this machine, or have a list of hosts to
which queries should be forwarded to.  The format of this file is described
in the section
.B "XDMCP Access Control."
.IP \fBDisplayManager.exportList\fP
A list of additional environment variables, separated by white space,
to pass on to the \fIXsetup\fP,
\fIXstartup\fP, \fIXsession\fP, and \fIXreset\fP programs.
.IP \fBDisplayManager.randomFile\fP
A file to checksum to generate the seed of authorization keys.
This should be a file that changes frequently.
The default is \fI/dev/mem\fP.
#ifdef DEV_RANDOM
.IP \fBDisplayManager.randomDevice\fP
A file to read 8 bytes from to generate the seed of authorization keys.
The default is \fI DEV_RANDOM \fP. If this file cannot be read, or if a
read blocks for more than 5 seconds, xdm falls back to using a checksum
of \fBDisplayManager.randomFile\fP to generate the seed.
#endif
#if !defined(ARC4_RANDOM)
.IP \fBDisplayManager.prngdSocket\fP
.IP \fBDisplayManager.prngPort\fP
A UNIX domain socket name or a TCP socket port number on local host on
which a Pseudo-Random Number Generator Daemon, like EGD
(http://egd.sourceforge.net) is listening, in order to generate the
autorization keys. Either a non null port or a valid socket name must
be specified. The default is to use the Unix-domain socket
\fI/tmp/entropy\fP.
.PP
On systems that don't have such a daemon, a fall-back entropy
gathering system, based on various log file contents hashed by the MD5
algorithm is used instead.
#endif
.IP \fBDisplayManager.greeterLib\fP
On systems that support a dynamically-loadable greeter library, the
name of the library.  The default is
\fI __projectroot__/lib/X11/xdm/libXdmGreet.so\fP.
.IP \fBDisplayManager.choiceTimeout\fP
Number of seconds to wait for display to respond after user has
selected a host from the chooser.  If the display sends an XDMCP
IndirectQuery within this time, the request is forwarded to the chosen
host.  Otherwise, it is assumed to be from a new session and the
chooser is offered again.
Default is 15.
.IP \fBDisplayManager.sourceAddress\fP
Use the numeric IP address of the incoming connection on multihomed hosts
instead of the host name. This is to avoid trying to connect on the wrong
interface which might be down at this time.
.IP \fBDisplayManager.willing\fP
This specifies a program which is run (as) root when an an XDMCP
BroadcastQuery is received and this host is configured to offer XDMCP
display management. The output of this program may be displayed on a chooser
window.  If no program is specified, the string \fIWilling to manage\fP is
sent.
.PP
.\"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resources\fP"
This resource specifies the name of the file to be loaded by \fIxrdb\fP
as the resource database onto the root window of screen 0 of the display.
The \fIXsetup\fP program, the Login widget, and \fIchooser\fP will use
the resources set in this file.
This resource data base is loaded just before the authentication procedure
is started, so it can control the appearance of the login window.  See the
section
.B "Authentication Widget,"
which describes the various
resources that are appropriate to place in this file.
There is no default value for this resource, but
\fI __projectroot__/lib/X11/xdm/Xresources\fP
is the conventional name.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.chooser\fP"
Specifies the program run to offer a host menu for Indirect queries
redirected to the special host name CHOOSER.
\fI CHOOSERPATH \fP is the default.
See the sections \fBXDMCP Access Control\fP and \fBChooser\fP.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.xrdb\fP"
Specifies the program used to load the resources.  By default,
.I xdm
uses \fI __projectroot__/bin/xrdb\fP.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.cpp\fP"
This specifies the name of the C preprocessor which is used by \fIxrdb\fP.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.setup\fP"
This specifies a program which is run (as root) before offering the
Login window.  This may be used to change the appearance of the screen
around the Login window or to put up other windows (e.g., you may want
to run \fIxconsole\fP here).
By default, no program is run.  The conventional name for a
file used here is \fIXsetup\fP.
See the section \fBSetup Program.\fP
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.startup\fP"
This specifies a program which is run (as root) after the authentication
process succeeds.  By default, no program is run.  The conventional name for a
file used here is \fIXstartup\fP.
See the section \fBStartup Program.\fP
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.session\fP"
This specifies the session to be executed (not running as root).
By default, \fI __projectroot__/bin/xterm\fP is
run.  The conventional name is \fIXsession\fP.
See the section
.B "Session Program."
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.reset\fP"
This specifies a program which is run (as root) after the session terminates.
By default, no program is run.
The conventional name is \fIXreset\fP.
See the section
.B "Reset Program."
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.openDelay\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.openRepeat\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.openTimeout\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.startAttempts\fP"
These numeric resources control the behavior of
.I xdm
when attempting to open intransigent servers.  \fBopenDelay\fP is
the length of the
pause (in seconds) between successive attempts, \fBopenRepeat\fP is the
number of attempts to make, \fBopenTimeout\fP is the amount of time
to wait while actually
attempting the open (i.e., the maximum time spent in the
.IR connect (2)
system call) and \fBstartAttempts\fP is the number of times this entire process
is done before giving up on the server.  After \fBopenRepeat\fP attempts have been made,
or if \fBopenTimeout\fP seconds elapse in any particular attempt,
.I xdm
terminates and restarts the server, attempting to connect again.
This
process is repeated \fBstartAttempts\fP times, at which point the display is
declared dead and disabled.  Although
this behavior may seem arbitrary, it has been empirically developed and
works quite well on most systems.  The default values are
5 for \fBopenDelay\fP, 5 for \fBopenRepeat\fP, 30 for \fBopenTimeout\fP and
4 for \fBstartAttempts\fP.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.pingInterval\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.pingTimeout\fP"
To discover when remote displays disappear,
.I xdm
occasionally pings them, using an X connection and \fIXSync\fP
calls.  \fBpingInterval\fP specifies the time (in minutes) between each
ping attempt, \fBpingTimeout\fP specifies the maximum amount of time (in
minutes) to wait for the terminal to respond to the request.  If the
terminal does not respond, the session is declared dead and terminated.  By
default, both are set to 5 minutes.  If you frequently use X terminals which
can become isolated from the managing host, you may wish to increase this
value.  The only worry is that sessions will continue to exist after the
terminal has been accidentally disabled.
.I xdm
will not ping local displays.  Although it would seem harmless, it is
unpleasant when the workstation session is terminated as a result of the
server hanging for NFS service and not responding to the ping.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.terminateServer\fP"
This boolean resource specifies whether the X server should be terminated
when a session terminates (instead of resetting it).  This option can be
used when the server tends to grow without bound over time, in order to limit
the amount of time the server is run.  The default value is ``false.''
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.userPath\fP"
.I Xdm
sets the PATH environment variable for the session to this value.  It should
be a colon separated list of directories; see
.IR sh (1)
for a full description.
``:/bin:/usr/bin:__projectroot__/bin:/usr/ucb''
is a common setting.
The default value can be specified at build time in the X system
configuration file with DefaultUserPath.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemPath\fP"
.I Xdm
sets the PATH environment variable for the startup and reset scripts to the
value of this resource.  The default for this resource is specified
at build time by the DefaultSystemPath entry in the system configuration file;
``/etc:/bin:/usr/bin:__projectroot__/bin:/usr/ucb'' is a common choice.
Note the absence of ``.'' from this entry.  This is a good practice to
follow for root; it avoids many common Trojan Horse system penetration
schemes.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemShell\fP"
.I Xdm
sets the SHELL environment variable for the startup and reset scripts to the
value of this resource.  It is \fI/bin/sh\fP by default.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.failsafeClient\fP"
If the default session fails to execute,
.I xdm
will fall back to this program.  This program is executed with no
arguments, but executes using the same environment variables as
the session would have had (see the section \fBSession Program\fP).
By default, \fI __projectroot__/bin/xterm\fP is used.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.grabServer\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.grabTimeout\fP"
To improve security,
.I xdm
grabs the server and keyboard while reading the login name and password.
The
\fBgrabServer\fP resource specifies if the server should be held for the
duration of the name/password reading.  When ``false,'' the server is ungrabbed
after the keyboard grab succeeds, otherwise the server is grabbed until just
before the session begins.  The default is ``false.''
The \fBgrabTimeout\fP resource specifies the maximum time
.I xdm
will wait for the grab to succeed.  The grab may fail if some other
client has the server grabbed, or possibly if the network latencies
are very high.  This resource has a default value of 3 seconds; you
should be cautious when raising it, as a user can be spoofed by a
look-alike window on the display.  If the grab fails,
.I xdm
kills and restarts the server (if possible) and the session.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.authorize\fP"
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.authName\fP"
\fBauthorize\fP is a boolean resource which controls whether
.I xdm
generates and uses authorization for the local server connections.  If
authorization is used, \fBauthName\fP is a list
of authorization mechanisms to use, separated by white space.
XDMCP connections dynamically specify which
authorization mechanisms are supported, so
\fBauthName\fP is ignored in this case.  When \fBauthorize\fP is set for a
display and authorization is not available, the user is informed by having a
different message displayed in the login widget.  By default, \fBauthorize\fP
is ``true.''  \fBauthName\fP is ``MIT-MAGIC-COOKIE-1,'' or, if
XDM-AUTHORIZATION-1 is available, ``XDM-AUTHORIZATION-1\0MIT-MAGIC-COOKIE-1.''
.IP \fBDisplayManager.\fP\fIDISPLAY\fP\fB.authFile\fP
This file is used to communicate the authorization data from
.I xdm
to the server, using the \fB\-auth\fP server command line option.
It should be
kept in a directory which is not world-writable as it could easily be
removed, disabling the authorization mechanism in the server.
If not specified, a name is generated from DisplayManager.authDir and
the name of the display.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.authComplain\fP"
If set to ``false,'' disables the use of the \fBunsecureGreeting\fP
in the login window.
See the section \fBAuthentication Widget.\fP
The default is ``true.''
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resetSignal\fP"
The number of the signal \fIxdm\fP sends to reset the server.
See the section \fBControlling the Server.\fP
The default is 1 (SIGHUP).
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.termSignal\fP"
The number of the signal \fIxdm\fP sends to terminate the server.
See the section \fBControlling the Server.\fP
The default is 15 (SIGTERM).
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resetForAuth\fP"
The original implementation of authorization in the sample server reread the
authorization file at server reset time, instead of when checking the
initial connection.  As
.I xdm
generates the authorization information just before connecting to the
display, an old server would not get up-to-date authorization information.
This resource causes
.I xdm
to send SIGHUP to the server after setting up the file, causing an
additional server reset to occur, during which time the new authorization
information will be read.
The default is ``false,'' which will work for all MIT servers.
.IP "\fBDisplayManager.\fP\fIDISPLAY\fP\fB.userAuthDir\fP"
When
.I xdm
is unable to write to the usual user authorization file ($HOME/.Xauthority),
it creates a unique file name in this directory and points the environment
variable XAUTHORITY at the created file.  It uses \fI/tmp\fP by default.
.SH "CONFIGURATION FILE"
First, the
.I xdm
configuration file should be set up.
Make a directory (usually \fI __projectroot__/lib/X11/xdm\fP) to contain all
of the relevant files.
.LP
Here is a reasonable configuration file, which could be
named \fIxdm-config\fP:
.nf

.ta .5i 4i

	DisplayManager.servers:	__projectroot__/lib/X11/xdm/Xservers
	DisplayManager.errorLogFile:	__projectroot__/lib/X11/xdm/xdm-errors
	DisplayManager*resources:	__projectroot__/lib/X11/xdm/Xresources
	DisplayManager*startup:	__projectroot__/lib/X11/xdm/Xstartup
	DisplayManager*session:	__projectroot__/lib/X11/xdm/Xsession
	DisplayManager.pidFile:	__projectroot__/lib/X11/xdm/xdm-pid
	DisplayManager._0.authorize:	true
	DisplayManager*authorize:	false

.fi
.PP
Note that this file mostly contains references to other files.  Note also
that some of the resources are specified with ``*'' separating the
components.  These resources can be made unique for each different display,
by replacing the ``*'' with the display-name, but normally this is not very
useful.  See the \fBResources\fP section for a complete discussion.
.SH "XDMCP ACCESS CONTROL"
.PP
The database file specified by the \fBDisplayManager.accessFile\fP provides
information which
.I xdm
uses to control access from displays requesting XDMCP service.  This file
contains three types of entries:  entries which control the response to
Direct and Broadcast queries, entries which control the response to
Indirect queries, and macro definitions.
.PP
The format of the Direct entries is simple, either a host name or a
pattern, which is distinguished from a host name by the inclusion of
one or more meta characters (`*' matches any sequence of 0 or more
characters, and `?' matches any single character) which are compared against
the host name of the display device.
If the entry is a host name, all comparisons are done using
network addresses, so any name which converts to the correct network address
may be used.
For patterns, only canonical host names are used
in the comparison, so ensure that you do not attempt to match
aliases.
Preceding either a host name or a pattern with a `!' character
causes hosts which
match that entry to be excluded.
.PP
To only respond to Direct queries for a host or pattern,
it can be followed by the optional ``NOBROADCAST'' keyword.
This can be used to prevent an xdm server from appearing on
menus based on Broadcast queries.
.PP
An Indirect entry also contains a host name or pattern,
but follows it with a list of
host names or macros to which indirect queries should be sent.
.PP
A macro definition contains a macro name and a list of host names and
other macros that
the macro expands to.  To distinguish macros from hostnames, macro
names start with a `%' character.  Macros may be nested.
.PP
Indirect entries
may also specify to have \fIxdm\fP run \fIchooser\fP to offer a menu
of hosts to connect to.  See the section \fBChooser\fP.
.PP
When checking access for a particular display host, each entry is scanned in
turn and the first matching entry determines the response.  Direct and
Broadcast
entries are ignored when scanning for an Indirect entry and vice-versa.
.PP
Blank lines are ignored, `#' is treated as a comment
delimiter causing the rest of that line to be ignored,
and `\e\fInewline\fP'
causes the newline to be ignored, allowing indirect host lists to span
multiple lines.
.PP
Here is an example Xaccess file:
.LP
.ta 2i 4i
.nf
XCOMM
XCOMM Xaccess \- XDMCP access control file
XCOMM

XCOMM
XCOMM Direct/Broadcast query entries
XCOMM

!xtra.lcs.mit.edu	# disallow direct/broadcast service for xtra
bambi.ogi.edu	# allow access from this particular display
*.lcs.mit.edu	# allow access from any display in LCS

*.deshaw.com	NOBROADCAST	# allow only direct access
*.gw.com			# allow direct and broadcast

XCOMM
XCOMM Indirect query entries
XCOMM

%HOSTS	expo.lcs.mit.edu xenon.lcs.mit.edu \\
	excess.lcs.mit.edu kanga.lcs.mit.edu

extract.lcs.mit.edu	xenon.lcs.mit.edu	#force extract to contact xenon
!xtra.lcs.mit.edu	dummy	#disallow indirect access
*.lcs.mit.edu	%HOSTS	#all others get to choose
.fi
.PP
If compiled with IPv6 support, multicast address groups may also be included
in the list of addresses indirect queries are set to.  Multicast addresses
may be followed by an optional / character and hop count. If no hop count is
specified, the multicast hop count defaults to 1, keeping the packet on the
local network. For IPv4 multicasting, the hop count is used as the TTL.
.PP
Examples:
.LP
.ta 2.1i 4.5i
.nf
rincewind.sample.net	ff02::1	#IPv6 Multicast to ff02::1
\&		#with a hop count of 1
ponder.sample.net	CHOOSER 239.192.1.1/16  #Offer a menu of hosts
\&		#who respond to IPv4 Multicast
\&		# to 239.192.1.1 with a TTL of 16
.fi
.SH CHOOSER
.PP
For X terminals that do not offer a host menu for use with Broadcast
or Indirect queries, the \fIchooser\fP program can do this for them.
In the \fIXaccess\fP file, specify ``CHOOSER'' as the first entry in
the Indirect host list.  \fIChooser\fP will send a Query request to
each of the remaining host names in the list and offer a menu of all
the hosts that respond.
.PP
The list may consist of the word ``BROADCAST,'' in which case
\fIchooser\fP will send a Broadcast instead, again offering a menu of
all hosts that respond.  Note that on some operating systems, UDP
packets cannot be broadcast, so this feature will not work.
.PP
Example \fIXaccess\fP file using \fIchooser\fP:

.nf
extract.lcs.mit.edu	CHOOSER %HOSTS	#offer a menu of these hosts
xtra.lcs.mit.edu	CHOOSER BROADCAST	#offer a menu of all hosts
.fi
.PP
The program to use for \fIchooser\fP is specified by the
\fBDisplayManager.\fP\fIDISPLAY\fP\fB.chooser\fP resource.  For more
flexibility at this step, the chooser could be a shell script.
\fIChooser\fP is the session manager here; it is run instead of a
child \fIxdm\fP to manage the display.
.PP
Resources for this program
can be put into the file named by
\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resources\fP.
.PP
When the user selects a host, \fIchooser\fP prints the host chosen,
which is read by the parent \fIxdm\fP, and exits.
\fIxdm\fP closes its connection to the X server, and the server resets
and sends another \fBIndirect\fP XDMCP request.
\fIxdm\fP remembers the user's choice (for
\fBDisplayManager.choiceTimeout\fP seconds) and forwards the request
to the chosen host, which starts a session on that display.
.\"
.SH LISTEN
The following configuration directive is also defined for the Xaccess
configuration file:
.IP "\fBLISTEN\fP \fIinterface\fP \fI[list of multicast group addresses]\fP"
\fIinterface\fP may be a hostname or IP addresss representing a
network interface on this machine, or the wildcard * to represent all
available network interfaces.
.PP
If one or more LISTEN lines are specified, xdm only listens for XDMCP
connections on the specified interfaces. If multicast group addresses
are listed on a listen line, xdm joins the multicast groups on the
given interface.
.PP
If no LISTEN lines are given, the original behavior of listening on
all interfaces is preserved for backwards compatibility.
Additionally, if no LISTEN is specified, xdm joins the default XDMCP
IPv6 multicast group, when compiled with IPv6 support.
.PP
To disable listening for XDMCP connections altogther, a line of LISTEN
with no addresses may be specified, or the previously supported method
of setting DisplayManager.requestPort to 0 may be used.
.PP
Examples:
.ta 2i 4i
.nf
LISTEN * ff02::1	# Listen on all interfaces and to the
\&	# ff02::1 IPv6 multicast group.
LISTEN 10.11.12.13	# Listen only on this interface, as long
\&	# as no other listen directives appear in
\&	# file.
.fi
.SH "IPv6 MULTICAST ADDRESS SPECIFICATION"
.PP
The Internet Assigned Numbers Authority has has assigned 
ff0\fIX\fP:0:0:0:0:0:0:12b as the permanently assigned range of 
multicast addresses for XDMCP. The \fIX\fP in the prefix may be replaced
by any valid scope identifier, such as 1 for Node-Local, 2 for Link-Local,
5 for Site-Local, and so on.  (See IETF RFC 2373 or its replacement for 
further details and scope definitions.)  xdm defaults to listening on the
Link-Local scope address ff02:0:0:0:0:0:0:12b to most closely match the 
old IPv4 subnet broadcast behavior.
.SH "LOCAL SERVER SPECIFICATION"
.PP
The resource \fBDisplayManager.servers\fP gives a server specification
or, if the values starts with a slash (/), the name of a file
containing server specifications, one per line.
.PP
Each specification
indicates a display which should constantly be managed and which is
not using XDMCP.
This method is used typically for local servers only.  If the resource
or the file named by the resource is empty, \fIxdm\fP will offer XDMCP
service only.
.PP
Each specification consists of at least three parts:  a display
name, a display class, a display type, and (for local servers) a command
line to start the server.  A typical entry for local display number 0 would
be:
.nf

  :0 Digital-QV local __projectroot__/bin/X :0

.fi
The display types are:
.ta 1i
.nf

local		local display: \fIxdm\fP must run the server
foreign		remote display: \fIxdm\fP opens an X connection to a running server

.fi
.PP
The display name must be something that can be passed in the \fB\-display\fP
option to an X program.  This string is used to generate the display-specific
resource names, so be careful to match the
names (e.g., use ``:0 Sun-CG3 local __projectroot__/bin/X :0'' instead of
``localhost:0 Sun-CG3 local __projectroot__/bin/X :0''
if your other resources are specified as
``DisplayManager._0.session'').  The display class portion is also used in the
display-specific resources, as the class of the resource.  This is
useful if you have a large collection of similar displays (such as a corral of
X terminals) and would like to set resources for groups of them.  When using
XDMCP, the display is required to specify the display class, so the manual
for your particular X terminal should document the display class
string for your device.  If it doesn't, you can run
.I xdm
in debug mode and
look at the resource strings which it generates for that device, which will
include the class string.
.PP
When \fIxdm\fP starts a session, it sets up authorization data for the
server.  For local servers, \fIxdm\fP passes
``\fB\-auth\fP \fIfilename\fP'' on the server's command line to point
it at its authorization data.
For XDMCP servers, \fIxdm\fP passes the
authorization data to the server via the \fBAccept\fP XDMCP request.
.SH RESOURCES FILE
The \fIXresources\fP file is
loaded onto the display as a resource database using
.I xrdb.
As the authentication
widget reads this database before starting up, it usually contains
parameters for that widget:
.nf
.ta .5i 1i

	xlogin*login.translations: #override\\
		Ctrl<Key>R: abort-display()\\n\\
		<Key>F1: set-session-argument(failsafe) finish-field()\\n\\
		<Key>Return: set-session-argument() finish-field()
	xlogin*borderWidth: 3
	xlogin*greeting: CLIENTHOST
\&	#ifdef COLOR
	xlogin*greetColor: CadetBlue
	xlogin*failColor: red
\&	#endif

.fi
.PP
Please note the translations entry; it specifies
a few new translations for the widget which allow users to escape from the
default session (and avoid troubles that may occur in it).  Note that if
\&#override is not specified, the default translations are removed and replaced
by the new value, not a very useful result as some of the default translations
are quite useful (such as ``<Key>: insert-char ()'' which responds to normal
typing).
.PP
This file may also contain resources for the setup program and \fIchooser\fP.
.SH "SETUP PROGRAM"
The \fIXsetup\fP file is run after
the server is reset, but before the Login window is offered.
The file is typically a shell script.
It is run as root, so should be careful about security.
This is the place to change the root background or bring up other
windows that should appear on the screen along with the Login widget.
.PP
In addition to any specified by \fBDisplayManager.exportList\fP,
the following environment variables are passed:
.nf
.ta .5i 2i

	DISPLAY	the associated display name
	PATH	the value of \fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemPath\fP
	SHELL	the value of \fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemShell\fP
	XAUTHORITY	may be set to an authority file
.fi
.PP
Note that since \fIxdm\fP grabs the keyboard, any other windows will not be
able to receive keyboard input.  They will be able to interact with
the mouse, however; beware of potential security holes here.
If \fBDisplayManager.\fP\fIDISPLAY\fP\fB.grabServer\fP is set,
\fIXsetup\fP will not be able to connect
to the display at all.
Resources for this program
can be put into the file named by
\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resources\fP.
.PP
Here is a sample \fIXsetup\fP script:
.nf

\&	#!/bin/sh
\&	# Xsetup_0 \- setup script for one workstation
	xcmsdb < __projectroot__/lib/monitors/alex.0
	xconsole\0\-geometry\0480x130\-0\-0\0\-notify\0\-verbose\0\-exitOnFail &

.fi
.SH "AUTHENTICATION WIDGET"
The authentication widget reads a name/password pair
from the keyboard.  Nearly every imaginable
parameter can be controlled with a resource.  Resources for this widget
should be put into the file named by
\fBDisplayManager.\fP\fIDISPLAY\fP\fB.resources\fP.  All of these have reasonable
default values, so it is not necessary to specify any of them.
.IP "\fBxlogin.Login.width, xlogin.Login.height, xlogin.Login.x, xlogin.Login.y\fP"
The geometry of the Login widget is normally computed automatically.  If you
wish to position it elsewhere, specify each of these resources.
.IP "\fBxlogin.Login.foreground\fP"
The color used to display the typed-in user name.
.IP "\fBxlogin.Login.font\fP"
The font used to display the typed-in user name.
.IP "\fBxlogin.Login.greeting\fP"
A string which identifies this window.
The default is ``X Window System.''
.IP "\fBxlogin.Login.unsecureGreeting\fP"
When X authorization is requested in the configuration file for this
display and none is in use, this greeting replaces the standard
greeting.  The default is ``This is an unsecure session''
.IP "\fBxlogin.Login.greetFont\fP"
The font used to display the greeting.
.IP "\fBxlogin.Login.greetColor\fP"
The color used to display the greeting.
.IP "\fBxlogin.Login.namePrompt\fP"
The string displayed to prompt for a user name.
.I Xrdb
strips trailing white space from resource values, so to add spaces at
the end of the prompt (usually a nice thing), add spaces escaped with
backslashes.  The default is ``Login:  ''
.IP "\fBxlogin.Login.passwdPrompt\fP"
The string displayed to prompt for a password.
The default is ``Password:  ''
.IP "\fBxlogin.Login.promptFont\fP"
The font used to display both prompts.
.IP "\fBxlogin.Login.promptColor\fP"
The color used to display both prompts.
.IP "\fBxlogin.Login.fail\fP"
A message which is displayed when the authentication fails.
The default is ``Login incorrect''
.IP "\fBxlogin.Login.failFont\fP"
The font used to display the failure message.
.IP "\fBxlogin.Login.failColor\fP"
The color used to display the failure message.
.IP "\fBxlogin.Login.failTimeout\fP"
The number of seconds that the failure message is displayed.
The default is 30.
.IP "\fBxlogin.Login.allowRootLogin\fP"
If set to ``false'', don't allow root (and any other user with uid = 0) to
log in directly.
The default is ``true''.
.IP "\fBxlogin.Login.allowNullPasswd\fP"
If set to ``true'', allow an otherwise failing password match to succeed
if the account does not require a password at all.
The default is ``false'', so only users that have passwords assigned can
log in.
.IP "\fBxlogin.Login.translations\fP"
This specifies the translations used for the login widget.  Refer to the X
Toolkit documentation for a complete discussion on translations.  The default
translation table is:
.nf
.ta .5i 2i

	Ctrl<Key>H:	delete-previous-character() \\n\\
	Ctrl<Key>D:	delete-character() \\n\\
	Ctrl<Key>B:	move-backward-character() \\n\\
	Ctrl<Key>F:	move-forward-character() \\n\\
	Ctrl<Key>A:	move-to-begining() \\n\\
	Ctrl<Key>E:	move-to-end() \\n\\
	Ctrl<Key>K:	erase-to-end-of-line() \\n\\
	Ctrl<Key>U:	erase-line() \\n\\
	Ctrl<Key>X:	erase-line() \\n\\
	Ctrl<Key>C:	restart-session() \\n\\
	Ctrl<Key>\\\\:	abort-session() \\n\\
	<Key>BackSpace:	delete-previous-character() \\n\\
	<Key>Delete:	delete-previous-character() \\n\\
	<Key>Return:	finish-field() \\n\\
	<Key>:	insert-char() \\

.fi
.PP
The actions which are supported by the widget are:
.IP "delete-previous-character"
Erases the character before the cursor.
.IP "delete-character"
Erases the character after the cursor.
.IP "move-backward-character"
Moves the cursor backward.
.IP "move-forward-character"
Moves the cursor forward.
.IP "move-to-begining"
(Apologies about the spelling error.)
Moves the cursor to the beginning of the editable text.
.IP "move-to-end"
Moves the cursor to the end of the editable text.
.IP "erase-to-end-of-line"
Erases all text after the cursor.
.IP "erase-line"
Erases the entire text.
.IP "finish-field"
If the cursor is in the name field, proceeds to the password field; if the
cursor is in the password field, checks the current name/password pair.  If
the name/password pair is valid, \fIxdm\fP
starts the session.  Otherwise the failure message is displayed and
the user is prompted again.
.IP "abort-session"
Terminates and restarts the server.
.IP "abort-display"
Terminates the server, disabling it.  This action
is not accessible in the default configuration.
There are various reasons to stop \fIxdm\fP on a system console, such as
when shutting the system down, when using \fIxdmshell\fP,
to start another type of server, or to generally access the console.
Sending \fIxdm\fP a SIGHUP will restart the display.  See the section
\fBControlling XDM\fP.
.IP "restart-session"
Resets the X server and starts a new session.  This can be used when
the resources have been changed and you want to test them or when
the screen has been overwritten with system messages.
.IP "insert-char"
Inserts the character typed.
.IP "set-session-argument"
Specifies a single word argument which is passed to the session at startup.
See the section \fBSession Program\fP.
.IP "allow-all-access"
Disables access control in the server.  This can be used when
the .Xauthority file cannot be created by
.I xdm.
Be very careful using this;
it might be better to disconnect the machine from the network
before doing this.
.PP
On some systems (OpenBSD) the user's shell must be listed in
.I /etc/shells
to allow login through xdm. The normal password and account expiration
dates are enforced too.
.SH "STARTUP PROGRAM"
.PP
The \fIXstartup\fP program is run as
root when the user logs in.
It is typically a shell script.
Since it is run as root, \fIXstartup\fP should be
very careful about security.  This is the place to put commands which add
entries to \fI/etc/utmp\fP
(the \fIsessreg\fP program may be useful here),
mount users' home directories from file servers,
or abort the session if logins are not
allowed.
.PP
In addition to any specified by \fBDisplayManager.exportList\fP,
the following environment variables are passed:
.nf
.ta .5i 2i

	DISPLAY	the associated display name
	HOME	the initial working directory of the user
	LOGNAME	the user name
	USER	the user name
	PATH	the value of \fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemPath\fP
	SHELL	the value of \fBDisplayManager.\fP\fIDISPLAY\fP\fB.systemShell\fP
	XAUTHORITY	may be set to an authority file

.fi
.PP
No arguments are passed to the script.
.I Xdm
waits until this script exits before starting the user session.  If the
exit value of this script is non-zero,
.I xdm
discontinues the session and starts another authentication
cycle.
.PP
The sample \fIXstartup\fP file shown here prevents login while the
file \fI/etc/nologin\fP
exists.
Thus this is not a complete example, but
simply a demonstration of the available functionality.
.PP
Here is a sample \fIXstartup\fP script:
.nf
.ta .5i 1i

\&	#!/bin/sh
\&	#
\&	# Xstartup
\&	#
\&	# This program is run as root after the user is verified
\&	#
	if [ \-f /etc/nologin ]; then
		xmessage\0\-file /etc/nologin\0\-timeout 30\0\-center
		exit 1
	fi
	sessreg\0\-a\0\-l $DISPLAY\0\-x __projectroot__/lib/xdm/Xservers $LOGNAME
	__projectroot__/lib/xdm/GiveConsole
	exit 0
.fi
.SH "SESSION PROGRAM"
.PP
The \fIXsession\fP program is the command which is run as the user's session.
It is run with
the permissions of the authorized user.
.PP
In addition to any specified by \fBDisplayManager.exportList\fP,
the following environment variables are passed:
.nf
.ta .5i 2i

	DISPLAY	the associated display name
	HOME	the initial working directory of the user
	LOGNAME	the user name
	USER	the user name
	PATH	the value of \fBDisplayManager.\fP\fIDISPLAY\fP\fB.userPath\fP
	SHELL	the user's default shell (from \fIgetpwnam\fP)
	XAUTHORITY	may be set to a non-standard authority file
	KRB5CCNAME	may be set to a Kerberos credentials cache name

.fi
.PP
At most installations, \fIXsession\fP should look in $HOME for
a file \fI\.xsession,\fP
which contains commands that each user would like to use as a session.
\fIXsession\fP should also
implement a system default session if no user-specified session exists.
See the section \fBTypical Usage\fP.
.PP
An argument may be passed to this program from the authentication widget
using the `set-session-argument' action.  This can be used to select
different styles of session.  One good use of this feature is to allow
the user to escape from the ordinary session when it fails.  This
allows users to repair their own \fI.xsession\fP if it fails,
without requiring administrative intervention.
The example following
demonstrates this feature.
.PP
This example recognizes
the special
``failsafe'' mode, specified in the translations
in the \fIXresources\fP file, to provide an escape
from the ordinary session.  It also requires that the .xsession file
be executable so we don't have to guess what shell it wants to use.
.nf
.ta .5i 1i 1.5i

\&	#!/bin/sh
\&	#
\&	# Xsession
\&	#
\&	# This is the program that is run as the client
\&	# for the display manager.

	case $# in
	1)
		case $1 in
		failsafe)
			exec xterm \-geometry 80x24\-0\-0
			;;
		esac
	esac

	startup=$HOME/.xsession
	resources=$HOME/.Xresources

	if [ \-f "$startup" ]; then
		exec "$startup"
	else
		if [ \-f "$resources" ]; then
			xrdb \-load "$resources"
		fi
		twm &
		xman \-geometry +10\-10 &
		exec xterm \-geometry 80x24+10+10 \-ls
	fi

.fi
.PP
The user's \fI.xsession\fP file might look something like this
example.  Don't forget that the file must have execute permission.
.nf
\&	#! /bin/csh
\&	# no \-f in the previous line so .cshrc gets run to set $PATH
	twm &
	xrdb \-merge "$HOME/.Xresources"
	emacs \-geometry +0+50 &
	xbiff \-geometry \-430+5 &
	xterm \-geometry \-0+50 -ls
.fi
.SH "RESET PROGRAM"
.PP
Symmetrical with \fIXstartup\fP,
the \fIXreset\fP script is run after the user session has
terminated.  Run as root, it should contain commands that undo
the effects of commands in \fIXstartup,\fP removing entries
from \fI/etc/utmp\fP
or unmounting directories from file servers.  The environment
variables that were passed to \fIXstartup\fP are also
passed to \fIXreset\fP.
.PP
A sample \fIXreset\fP script:
.nf
.ta .5i 1i
\&	#!/bin/sh
\&	#
\&	# Xreset
\&	#
\&	# This program is run as root after the session ends
\&	#
	sessreg\0\-d\0\-l $DISPLAY\0\-x __projectroot__/lib/xdm/Xservers $LOGNAME
	__projectroot__/lib/xdm/TakeConsole
	exit 0
.fi
.SH "CONTROLLING THE SERVER"
.I Xdm
controls local servers using POSIX signals.  SIGHUP is expected to reset the
server, closing all client connections and performing other cleanup
duties.  SIGTERM is expected to terminate the server.
If these signals do not perform the expected actions,
the resources \fBDisplayManager.\fP\fIDISPLAY\fP\fB.resetSignal\fP and
\fBDisplayManager.\fP\fIDISPLAY\fP\fB.termSignal\fP can specify alternate signals.
.PP
To control remote terminals not using XDMCP,
.I xdm
searches the window hierarchy on the display and uses the protocol request
KillClient in an attempt to clean up the terminal for the next session.  This
may not actually kill all of the clients, as only those which have created
windows will be noticed.  XDMCP provides a more sure mechanism; when
.I xdm
closes its initial connection, the session is over and the terminal is
required to close all other connections.
.SH "CONTROLLING XDM"
.PP
.I Xdm
responds to two signals: SIGHUP and SIGTERM.  When sent a SIGHUP,
.I xdm
rereads the configuration file, the access control file, and the servers
file.  For the servers file, it notices if entries have been added or
removed.  If a new entry has been added,
.I xdm
starts a session on the associated display.  Entries which have been removed
are disabled immediately, meaning that any session in progress will be
terminated without notice and no new session will be started.
.PP
When sent a SIGTERM,
.I xdm
terminates all sessions in progress and exits.  This can be used when
shutting down the system.
.PP
.I Xdm
attempts to mark its various sub-processes for
.IR ps (1)
by editing the
command line argument list in place.  Because
.I xdm
can't allocate additional
space for this task, it is useful to start
.I xdm
with a reasonably long
command line (using the full path name should be enough).
Each process which is
servicing a display is marked \fB\-\fP\fIdisplay.\fP
.SH "ADDITIONAL LOCAL DISPLAYS"
.PP
To add an additional local display, add a line for it to the
\fIXservers\fP file.
(See the section \fBLocal Server Specification\fP.)
.PP
Examine the display-specific resources in \fIxdm-config\fP
(e.g., \fBDisplayManager._0.authorize\fP)
and consider which of them should be copied for the new display.
The default \fIxdm-config\fP has all the appropriate lines for
displays \fB:0\fP and \fB:1\fP.
.SH "OTHER POSSIBILITIES"
.PP
You can use \fIxdm\fP
to run a single session at a time, using the 4.3 \fIinit\fP
options or other suitable daemon by specifying the server on the command
line:
.nf
.ta .5i

	xdm \-server \(lq:0 SUN-3/60CG4 local __projectroot__/bin/X :0\(rq

.fi
.PP
Or, you might have a file server and a collection of X terminals.  The
configuration for this is identical to the sample above,
except the \fIXservers\fP file would look like
.nf
.ta .5i

	extol:0 VISUAL-19 foreign
	exalt:0 NCD-19 foreign
	explode:0 NCR-TOWERVIEW3000 foreign

.fi
.PP
This directs
.I xdm
to manage sessions on all three of these terminals.  See the section
\fBControlling Xdm\fP for a description of using signals to enable
and disable these terminals in a manner reminiscent of
.IR init (__adminmansuffix__).
.SH LIMITATIONS
One thing that
.I xdm
isn't very good at doing is coexisting with other window systems.  To use
multiple window systems on the same hardware, you'll probably be more
interested in
.I xinit.
.SH FILES
.TP 20
.I __projectroot__/lib/X11/xdm/xdm-config
the default configuration file
.TP 20
.I $HOME/.Xauthority
user authorization file where \fIxdm\fP stores keys for clients to read
.TP 20
.I CHOOSERPATH
the default chooser
.TP 20
.I __projectroot__/bin/xrdb
the default resource database loader
.TP 20
.I __projectroot__/bin/X
the default server
.TP 20
.I __projectroot__/bin/xterm
the default session program and failsafe client
.TP 20
.I __projectroot__/lib/X11/xdm/A<display>\-<suffix>
the default place for authorization files
.TP 20
.I /tmp/K5C<display>
Kerberos credentials cache
.SH "SEE ALSO"
.IR X (__miscmansuffix__),
.IR xinit (1),
.IR xauth (1),
.IR Xsecurity (__miscmansuffix__),
.IR sessreg (1),
.IR Xserver (1),
.\" .IR chooser (1), \" except that there isn't a manual for it yet
.\" .IR xdmshell (1), \" except that there isn't a manual for it yet
.br
.I "X Display Manager Control Protocol"
.SH AUTHOR
Keith Packard, MIT X Consortium
