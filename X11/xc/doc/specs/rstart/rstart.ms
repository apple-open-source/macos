.\" $XFree86: xc/doc/specs/rstart/rstart.ms,v 1.2 2001/05/29 18:14:12 dawes Exp $
.ds iL Brown
.ds iI J.
.ds iO Quarterdeck Office Systems
.ds iG X Consortium
.ds iN DRAFT
.ds iD August 1993
.so tmac.build
.LG
.LG
.ce
A Flexible Remote Execution Protocol Based on \fBrsh\fP
.SM
.SM
.sp 2
.fi
.ne 4
Status of this Memo
.sp
.QP
This document is being distributed to members of the 
Internet community in order to solicit their reactions 
to the proposals contained in it.  This memo does not
specify an Internet Standard.
Distribution of this memo is unlimited.

.dH "(filename here)"
.LP
.sp
.ne 4
Background:
.sp
One of the X Window System's main strengths is the ability to display
and control graphical
applications remotely.  It does not, however, address the issue of how to
start these applications - the application initiates the connection to the
server sitting in front of the user, not the other way 'round.  Some other,
non-X, mechanism must be used to start the application.

.ne 4
Current methods and their problems:
.IP 1)
Walk over to the other machine, log in, and run something with its
display redirected to your workstation.  Not very appealing, but simple.
No security problems associated with whether or not you should be able to
run something on that machine.
.IP 2)
Telnet to the other machine, log in, and run something with its display
redirected to your workstation.  Relatively simple, but doesn't allow for
starting remote apps using a menu item - requires either human intervention
or an autologin mechanism with its associated troubles.  Security implication
is that the password is passed \*Qraw\*U over the telnet connection and is
susceptible to snooping.
.IP 3)
Use one of the non-standard Berkeley
.UX
\*Qremote command\*U facilities, or their
Kerberized equivalents - rsh, rexec, krsh.  These can provide for the
no-human-involved startup desired for menus, but have security implications
which have been adequately documented elsewhere and require tuning to
achieve \*Qdesirable\*U operation, or indeed operation at all.
.IP 4)
Other proprietary schemes.
.LP
Well, (1) is obviously not very pleasant.  (2) isn't much better, because
you can't run things off menu items.  Neither (1) nor (2) is \*Qfriendly\*U
to an automatic session-restart system.  (4), being proprietary, isn't of
much interest in a wide context.  (Note that in any of these cases once
you've started an app, an xterm say, you can ask it to start others.  This
is OK for some people, but not very acceptable if you want things picked off
menus and especially not if you want apps on various machines picked off
the same menu.  It also isn't friendly to automatic session-restart schemes.)

(3) is what this memo discusses.

.ne 4
Problems:
.IP \(bu
Systems and shells vary in what exactly constitutes a \*Qcommand\*U
and what the syntax is.
.IP \(bu
The apps might not be in the \*Qsystem default search path\*U.
.IP \(bu
The apps might require additional setup before they can run -
environment variables, etc.  (LD_LIBRARY_PATH with OpenWindows 2 on Suns)
.IP \(bu
Resources (TCP connections, processes, etc) get held open on both
ends to support the (mostly-idle) rsh connection.
.IP \(bu
No standard way to pass environment variables - DISPLAY, SESSION_MANAGER,
etc.
.LP
These issues, both in their complexity and in their system-specificity,
cause continual trouble for less technically aware users and are a nuisance
even for technical users.  In addition, more sophisticated uses of remote
execution mechanisms (session restart for instance) may require more control
over the environment than is available through the normal rsh mechanism.

The result is that a great deal of system-dependent manual tuning is required
to achieve a pleasant result, where the DISPLAY and SESSION_MANAGER
values (and others
in the future) are passed transparently, where the app actually gets
started(!), where unnecessary resources are not held open, etc.

The obvious answer is to define a \*Qbetter\*U remote execution mechanism,
but that has a host of problems - while rsh and friends have their
security problems, at least they are reasonably well understood and
the \*Qtrusted\*U executables have been extensively tested.  A new mechanism
would have to be analysed and tested before it could be trusted.

Better to make effective use of current services, and take advantage
of such services as may become available in the future.  In the future
some better remote execution protocol may become available.  Such a
protocol may address some of the issues this paper is intended to
address like passing auxiliary information, but will probably not address
issues like setting up the environment required to run an X app.  Fine,
adopt it, and adopt conventions like those discussed here to allow its
convenient use for X applications.

In this spirit this paper does not mandate \*Qthou shalt start X apps
using rsh\*U, but rather \*QIf you support X, you should arrange that the
following rsh request will start an X app properly\*U.

Such a solution would be to layer a more flexible protocol on top
of rsh et al, using rsh's standard in/out mechanism to pass initial
setup information to a \*Qhelper\*U program on the other end.  Since rsh
(or whatever) has already handled the security aspects of the request,
the helper need have no particular special privileges and hence adds
no new security considerations.

\*QWhy rsh?\*U  While this paper refers to \*Qrsh\*U all over the place, few
if any of the concepts are peculiar to rsh.  Any form of remote execution
protocol that handles the security aspects of remote execution and
provides a bidirectional data stream to the resulting program can be used.
Rsh, rexec, and krsh are obvious examples, but there is no reason why
telnet could not be used, when combined with a scripting mechanism to
do autologin.

\*QBut I don't use X, why do I want this?\*U  Again, while this paper
refers to X all over the place and the impetus for creation of this
protocol is starting and restarting X applications, the mechanisms
defined are as independent of X as possible and are applicable to
non-X start/restart problems.

\*QI don't use POSIX.  What do I do?\*U  The initial target systems are
generally POSIX-based or POSIX-like, and so there are some POSIX-specific
features and lots of POSIX-specific examples, but the protocol is
designed to be operating-system independent.  Such OS independence is,
in fact, one of the primary goals - to provide an OS-independent
remote execution mechanism.
.bp
.ce
The Remote Start Protocol \*Qrstart\*U

Goals:

.IP \(bu
A requester should be able to have a program started in any of a
number of predefined \*Qcontexts\*U.  (For instance, on a dual-universe
Berkeley
.UX
/ System V
.UX
system those might be two such contexts.
On a system with multiple versions of the X Window System installed each
would be available as a predefined context.  A VAX might support VMS
and Eunice contexts.)
.IP \(bu
A requester should be able to override (within security bounds usual
to the system) any aspect of the environment.
.IP \(bu
Neither the requesting program nor the \*Qhelper\*U program on the
host end should need to have any special privileges.
.IP \(bu
Any parameter of the environment that can be controlled should be
controllable through this mechanism.  In particular, on POSIX systems
environment variables, open files(?), umask, current directory, etc
should all be controllable.
.IP \(bu
The full richness of the host's command argument mechanism should
be available.  In particular, on POSIX this means that arguments may
contain any character and may be of any length.
.IP \(bu
Notwithstanding all of the control afforded the requester, none
of it should be required - the requester should be able to provide
simple \*Qcommand lines\*U which will be executed in the desired environment
much as if they were typed to a conventional command processor.
.IP \(bu
A \*QGeneric Command\*U mechanism is provided, where standardized
commands result in an appropriate response, independent of the host
system.  (This might be considered to be similar to the FTP model,
where the LIST command will produce a directory listing no matter what
the underlying system.)
.LP

Requesting System Requirements:

The requesting system MUST support an \*Qrsh\*U client or a suitable
replacement.  Support of the rsh standard error and control connection
is desirable but not essential.

Host System Requirements:

The host system MUST support an \*Qrsh\*U server or a suitable replacement.
Support of the rsh standard error and control connection is desirable but
not essential.  Invocation of the \*Qrstartd\*U program in the default rsh
environment MUST \*Qwork\*U.  On POSIX systems this might require that rstartd
be installed in one of the directories in the default $PATH and that rstartd
not need any non-default shared library setup, etc.  On other systems it
might require that the rsh server specially recognize the string \*Qrstartd\*U
and take appropriate action.

The protocol itself:

The requesting system makes a remote execution request using rsh (or
other suitable protocol) to execute the program \*Qrstartd\*U.  The host
system responds with any amount of data (to accomodate systems that
want to natter before starting a program), and then sends a line
consisting of (exactly):
.nf
	rstartd:<space>Ready:<space><welcome/version message>
.fi
Failure to receive this line before the connection closes
indicates that the host system does not support Extended rsh.  A
timeout is probably appropriate in addition.

The requester then sends a series of lines of ASCII specifying the
program to be run and the environment in which it is to be run.  The
request is terminated by a blank line.

Syntax:  rstart data is passed as a series of lines of ASCII, with
spaces delimiting words in each line, terminated by a blank line.
A \*Qline\*U is terminated by an ASCII LF.  CRs and NULs MUST be ignored,
to allow for a system transmitting in Internet NVT ASCII.  (Yes, rsh
et al are POSIX-style tools and won't have CRs, but if there's ever an
Internet standard remote exec mechanism it will almost certainly require
NVT ASCII, so it seems wise to provide for that possibility from the start.)
It is explicitly allowed for a system to discard characters other than
LF outside the range decimal 32-126; characters outside that range MUST
NOT be used.  Words may contain spaces and non-printable characters by
representing them as octal escape sequences consisting of a backslash
followed by three octal digits.  Note that backslashes themselves MUST
be passed using this mechanism.  Thus the initial parsing sequence
consists of:

.IP 1)
Receive data until the first blank line.
.IP 2)
Break data into lines at LFs, discarding CRs and NULs.
.IP 3)
Break lines into words at spaces.
.IP 4)
Translate \\nnn sequences in words into the appropriate characters.
.IP 5)
Process each line as appropriate.
.IP 6)
Pass (or more likely, allow to be passed) the connection and any data
after the blank line to the program.  (??? Hmm.  stdio buffering
considerations.  Byte count instead of blank line?)
.LP
The first word of each line is a keyword specifying the interpretation of
the line.  Keywords SHOULD be transmitted as shown in this document, 
but the receiver MUST accept them in any case, even mIxEd.

Unless otherwise specified, only one instance of any given keyword is
permitted in a given request.  CONTEXT MUST be specified first, and
after that keywords may be given in any order.

After receiving the blank line, the host responds with any number of
lines of output of the following forms, terminated by a blank line.

rstartd: Ready: <message>
.IP
(This isn't one of the \*Qresponse\*U lines, but it's
included here for completeness.)
This is rstartd's \*Qhello\*U line, and confirms that the host
does indeed support rstartd.
.LP

rstartd: Failure: <message>
.IP
An unrecoverable error has occurred which indicates that either
the requester or the host is fatally misconfigured.  This might
occur if, for instance, a request is malformed or a required
configuration file is not present.
.LP

rstartd: Error: <message>
.IP
An unrecoverable error has occurred which indicates that the
request is in error.  The most common such error would be that
the requested program is unavailable.
.LP

rstartd: Warning: <message>
.IP
A recoverable error has occurred.  The program will be executed
but may not behave as desired.
.LP

rstartd: Success: <message>
.IP
No errors were detected.  Unfortunately this does not mean that
no errors will occur, because there are many classes of errors
that cannot be detected until rstartd actually attempts to pass
control to the program.
.LP

rstartd: Debug: <message>
.IP
A debug message.  Programs (and most humans!) should ignore these.
.LP

<anything else>
.IP
Indicates that something else in the system just HAD to say
something; should be treated as a Warning.
.LP
.bp
.LG
Keywords
.SM

.B CONTEXT
.I name

Initializes defaults suitable for the specified context.  See the section
on contexts for more information.

CONTEXT must be present, and must be the first line of the request.

.B CMD
.I "command args args args"

Executes the specified command with the specified arg in the same general
way as it would have been executed if typed to a the user's command
interpreter.  (If the user's primary interface is not a command language,
a system default command language should be used.)  It is expected that
the command will have been provided by a user, who will expect \*Qnormal\*U
command language handling of it.  (For POSIX people, consider this as
roughly equivalent to system().)

Note:  No particular parsing or interpretation is guaranteed.  The
interpretation should be unsurprising to an ordinary user of the host
system.  This mechanism is therefore unsuitable for completely automated
use; EXEC and GENERIC-CMD are provided for that purpose.

Exactly one of CMD, EXEC, or GENERIC-CMD must be present.

.B EXEC
.I "progname namearg arg arg arg ..."

Executes a program using a low-level execution mechanism which provides
minimal interpretation; in particular a command processor should not enter
the picture and no quoting other than that required by this protocol should
be required.  It is expected that this interface will be used by programs
requesting restart; presumably they know exactly what their desired
arguments are and a command processor will only confuse the issue.
(For POSIX people, consider this to be like execlp().)

.I Progname
is the name of the executable.  This will typically be a
single word but is allowed to be a (system-specific) full filename
specification; if the latter then the behavior is system-specific but
normally path searching would be disabled.

.I Namearg
should be the program name as it should be passed to the program.
Generally, it should be exactly equal to progname.  Hosts which do not
pass a program's name to it should ignore it.

The 
.I args
are the arguments to be passed to the program.  Hosts which do
not separate their arguments into words should concatenate the arguments
back together with spaces between them.  (Optionally, they could elect to
not fully parse the line, and leave in the spaces as originally delivered.)

Note:  The interpretation of the command may not be intuitive to
an ordinary user of the host system; this interface is for precision,
not intuition.

Exactly one of
.B CMD ,
.B EXEC ,
or
.B GENERIC-CMD
must be present.

.B DIR
.I initial-directory

Specifies (in a system-specific way) the initial default file system
location for the program.  If no
.B DIR
line is supplied the program is
started in a system-specific initial location, presumably the user's
\*Qhome\*U.

Note:  It is expected that this value would come from a source with
a priori knowlege of the host - either the user or a \*Qrestart\*U request.
It is not expected that an automated mechanism with no advance knowlege
would be able to make use of this request.

.B MISC
.I "registryname name=value"

Passes a value to the program using a system-specific mechanism.
(Under POSIX and similar systems environment variables should generally
be used.)  The
.I registryname
specifies the naming authority, and is intended to prevent conflicts and
allow for intelligent conversion.  The idea is that systems that
understand a given registry will map these straight into environment
variables.  Systems that don't entirely understand a given registry or
use a different but convertable mechanism can be picky and convert as
needed.  An appendix lists the names that all systems are strongly
encouraged to support.

Note:  The names in the \*Qsuggested\*U appendix are expected to be supplied
by requesters with no advance knowlege of the host, only the blind
assumption that the host will support those \*Qwell-known\*U names with
their \*Qwell-known\*U semantics.  Other names may be used by requesters
with advance knowlege of the host - restart requests, for example.

Any number of
.B MISC
lines may be present.

.B DETACH

This line directs rstartd to attempt to \*Qdetach\*U the resulting process
from the rsh connection, leaving as little overhead (processes, connections,
etc) as possible.  In POSIX-land, this will probably consist of redirecting
standard input, output, and error to /dev/null or a log file and then
executing the program using fork() and exec() and not having the parent
wait.

It would be nice to have a mechanism for specifying where the output
should be redirected - log file or perhaps log/display program/server.
For the moment that's left as a matter of local implementation and
configuration.

Only one of
.B DETACH
and
.B NODETACH
may be present.

.B NODETACH

This line directs rstartd to attempt NOT to \*Qdetach\*U.  It is intended to
allow one to override a configuration default to
.B DETACH .

Only one of
.B DETACH and
.B NODETACH
may be present.

.B AUTH
.I "authscheme authdata ..."

Specifies authentication data to be used by the specified authentication
scheme.  This keyword may be given multiple times to give data for
multiple authentication schemes or for a single scheme to use for multiple
purposes.
.bp
System-specific lines begin with
.B SYSTEMNAME- .
No system-independent line
will be defined that includes a \*Q-\*U in its name.
.B X-
is reserved for experimental use.  (\*QReserved\*U is an interesting word,
there; anybody can use
.B X-
for experiments and nobody can rely on there not being a conflict.)
.B INTERNAL-
is reserved for internal use by rstartd.

.B POSIX-UMASK
.I nnn

Sets the POSIX umask to
.I nnn
(octal).

.B MSDOS-DIR
.I d:path

Sets the current directory on drive
.I d:
to
.I path.
This differs from
.B DIR
in that
.B DIR
sets the current working drive and directory and
.B MSDOS-DIR
doesn't set the current drive;
.B MSDOS-DIR
would be used to set the current directory on drives other than the
current one.

.B DESQVIEW-MEM
.I nnn

Requests
.I nnn
kilobytes of conventional memory.

.B DESQVIEW-EXPMEM
.I nnn

Requests
.I nnn
kilobytes of expanded memory.  (To be more verbose, requests
that the \*Qmax expanded memory\*U parameter be
.I nnn .)

.B GENERIC-xxx

The
.B GENERIC
system is a hypothetical system that offers various services
that can be supported on different real systems; it provides a common
interface for heterogenous systems.

.B GENERIC-CMD
.I "generic-command-name args ..."

Runs the local equivalent to
.I generic-command-name .
See the section on generic commands for more information.  Exactly one of
.B CMD ,
.B EXEC ,
or
.B GENERIC-CMD
must be present.
.bp
.LG
Generic Commands
.SM

Unless otherwise noted, generic commands are optional.

.B "GENERIC-CMD Terminal"

Runs a default terminal emulator for the current context.  (It's not clear
what, if anything, this means outside a windowing system context.)  In POSIX
X contexts this probably means xterm.

.B "GENERIC-CMD LoadMonitor"

Runs a default load monitor for the current context.  In POSIX X contexts,
this probably means xload.

.B "GENERIC-CMD ListContexts"

Sends to standard output a list of available contexts, one per line,
with a comma-separated list of context names followed by a space followed by
a brief description of the context.  If multiple context names invoke the
same context (as for instance X, X11, and X11R4 might) ListContexts SHOULD
list the most specific context first.

This command MUST be available in the Default context, and SHOULD be
available in every context.

.B "GENERIC-CMD ListGenericCommands"

Sends to standard output a list of generic commands available in the
current context, one per line, with the command name followed by a space
followed by a brief description of the command.

This command SHOULD be available in every context.
.bp
.LG
Contexts
.SM

A request can specify what
.I context
a program should be run in.  A context
will most likely include a set of default values for all of the controllable
aspects of the program's execution environment.  

Examples of Predefined Context Names

.nf
None		A minimal environment.  Under POSIX, no environment variables.
Default		The default environment.
X		The X Window System, any version
X11		Version 11 of the X Window System, any release
X11R4		Version 11 of the X Window System, Release 4
X11R5		Version 11 of the X Window System, Release 5
OpenWindows	Sun's OpenWindows, any version
OpenWindows2	Version 2 of Sun's OpenWindows
OpenWindows3	Version 3 of Sun's OpenWindows
NeWS		Some version of Sun's Network Window System.
.fi

Contexts are allowed (encouraged even) to support multiple names for
the same environment configuration.  For instance: \*QX\*U, \*QX11\*U,
\*QX11R4\*U might all refer to exactly the same configuration.
\*QOpenWindows3\*U might well refer to a configuration that is a union
of \*QX11R4\*U and \*QNeWS\*U.
.bp
.LG
Suggested \*Qrequired\*U MISC commands
.SM

General

For most of these the \*Qsimple\*U behavior is to simply pass the value
to the app as an environment variable.  This should be appropriate on
any POSIX systems.  Other systems should use whatever mechanism is
appropriate to the given item; note that different mechanisms may be
appropriate to different items.

All systems are encouraged to \*Qunderstand\*U all of these names, translating
them as appropriate to the local environment.

\fBMISC X LANG=\fP\fIlocale identifier\fP

Specifies the locale desired.  If POSIX specifies a list of locale
identifiers then we can use that (and move it to the POSIX registry),
but if not then we will have to define a list (perhaps based on a good
de facto standard).  Note that if there is no POSIX-standardized list
of locale names the host SHOULD translate from our list to the local
list.  Note that this is based on the POSIX LANG environment variable,
but rumor says that POSIX does not specify a list of identifiers;
since we want a standarized list we must specify a registry and must
assume \*Qcontrol\*U of the name.  If in the future POSIX specifies a
different list then during a transition period MISC X LANG and
MISC POSIX LANG would perform similar functions but using different
names for the locales. [[ It may be that ISO country/language codes
may supply a suitable registry. -- jb ]]

\fBMISC X DISPLAY=\fP\fIhost\fP\fB:\fP\fIdisplaynum\fP

Specifies the X server the app is to connect to.

\fBMISC X SESSION_MANAGER=tcp/\fP\fIhost\fP\fB:\fP\fIport\fP

Specifies the session manager the app is to connect to.

\fBMISC POSIX TZ=\fP\fItime zone info\fP

Specifies the time zone the user is in, using POSIX standard notation.
.bp
.LG
Specific Notes on use of Extended rsh with the X Window System
.SM

To start an X program, the requester should specify an X context (\*QX\*U if
nothing else) and specify the display parameter by saying
.DS
\fBMISC X DISPLAY=\fP\fIhost\fP\fB:\fP\fIport.screen\fP
.DE
If the program is to join a session the requester should say:
.DS
\fBMISC X SESSION_MANAGER=tcp/\fP\fIhost\fP\fB:\fP\fIport\fP
.DE

An X host MUST understand at least the X context and MUST, regardless
of whether or not it has environment variables as a system concept,
interpret \fBMISC X DISPLAY=\fP\fIhost\fP\fB:\fP\fIport\fP and pass it
to the program in
whatever way is locally appropriate.  An X host supporting session
management (which all will do by the time this is adopted, right?)
MUST interpret \fBMISC X SESSION_MANAGER=tcp/\fP\fIhost\fP\fB:\fP\fIport\fP
similarly.

An X host MUST understand the
.B X11
authentication scheme for the
.B AUTH
keyword.  The data given MUST be in the form given by \*Qxauth list\*U and
understood by \*Qxauth add\*U.  AUTH X11 may be given several times to
pass multiple kinds of authorization data or data about multiple displays.

Input methods?  Extensibility?  (Would be nice if new features could
be incorporated without new rstartd executables, which argues for passing
most data as environment variables.)
.bp
.LG
Suggestions for Implementation
.SM

Configurability, extensibility.  Nobody is ever happy with the default.
The host implementation should allow for as much configurability and
extensibility as possible.  In particular, provision should be made for
administrator- and user- defined contexts, with user defined contexts
overriding or augmenting the administrator-defined and system-supplied
contexts.  One good implementation scheme would be to have system-wide
and per-user configuration files which use the same syntax as the protocol.
ListContexts SHOULD list both system-wide and per-user contexts.

Provision SHOULD be made for administrator- and user- defined generic
commands, with user-defined commands overriding system-wide ones.
ListGenericCommands SHOULD list both.
.bp
.LG
Notes on Outstanding specification issues
.SM

Note:  This is not part of the proposal.

Syntax
.IP
The syntax is OK so far for machine generation, but is yucky for human
generation, especially spaces in environment variables in
config files.
.LP

Environment variables
.IP
it would be nice if one could say things like
$HOME in values, especially in config files.
.LP

Error handling
.IP
how to reliably mark successfully starting the app.
Can report syntax errors or lack thereof, but reporting startup or
execution errors is problematical.  (Application execution errors
are tough no matter what; startup errors are tough because you want
to report an error if the exec*() fails, but if it succeeds you
don't have the opportunity to give a success report.) [[Clive gives
a clever trick for reliably distinguishing success, but I think
there's a race condition if the app is allowed to use the rsh
channel for output after it starts. -- jb]]
.LP

Error handling
.IP
how to report post-startup errors and warnings to the
user.  (Can say \*Qnot our problem\*U, but it would be nice if there
was a standard way to say \*Qgive output to a program that will
report it back to the user\*U or something like that.  Session Manager
\*Qdeath reason\*U messages might be adequate, but don't cover warnings.)
.LP

I18n
.IP
What character set should messages and requests be in?  Can this
be determined by the locale as specified by
.B "MISC X LANG" ?
(Does this require that
.B "MISC X LANG"
be one of the first lines?)
.LP

Protocol
.IP
One reviewer suggests an FTP- and SMTP- style protocol with requests
and responses.  Personally I don't think individual responses to
the lines are required and that the roundtrips involved would
be wasted.  The entire conversation is a single request and can
be rejected or accepted en toto.  The response mechanism does
not provide for fabulously rich interpretation by an automaton,
but it does allow for a success / serious failure / routine
failure / warning distinction.
.LP

Multiple requests/connection
.IP
Some have suggested that you should be able
to start multiple programs with a single connection, either to
start multiple apps on login or to allow a \*Qmaster\*U to request
apps occasionally during the course of a session.  As doing so
would make it less possible (or at least more difficult) to use
the data channel for communicating with the app after start,
and as the overhead of starting a new connection isn't all that
high, I'm not enchanted with the idea.  Admittedly, if the overhead
involved got higher (how fast is a Kerberos authentication?)
it might become more attractive.
.LP
.bp
Conclusion:

A small protocol could be easily defined which would layer on top of a
relatively primitive remote execution facility like rsh, rexec, or krsh,
that would allow flexible application startup in a fairly machine-independent
way.  By mandating appropriate local configuration, X applications could
be started (or restarted) conveniently, without requiring the requester
to understand the detailed requirements of the remote system.  The
implementation cost, for both the requester and the \*Qserver\*U, is small.
Security issues are \*Qnot our problem\*U, since they are all handled by
existing protocols.
.sp
Security Considerations
.sp
.QP
Security is assumed handled by the underlying remote execution mechanism.
The \*Qhelper\*U program described by this memo runs with the privileges of
the user and so generally introduces no additional security considerations.
Systems where security is controlled by controlling what programs
may be run - where programs are trusted but users are not - may see
a security impact unless their \*Qhelper\*U is careful about what programs
it is willing to run.
.LP
.sp
Copyright
.sp
.QP
Copyright
.if t \(co
.if n (c)
1993 Quarterdeck Office Systems
.sp
Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name Quarterdeck Office Systems, Inc. not
be used in advertising or publicity pertaining to distribution of this
software without specific, written prior permission.

THIS SOFTWARE IS PROVIDED `AS-IS'.  QUARTERDECK OFFICE SYSTEMS, INC.,
DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL QUARTERDECK
OFFICE SYSTEMS, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE,
DATA, OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND
REGARDLESS OF WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING
OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
.LP
.sp
.ne 5
Author's  Address:
.sp
.IP
.nf
Jordan Brown
Quarterdeck Office Systems
Santa Monica, CA
jbrown@qdeck.com
.LP
