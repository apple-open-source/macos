.\" Use tbl, -ms
.\" $XConsortium: saver.ms,v 1.5 92/02/11 17:05:26 keith Exp $
.\" $NCDId: @(#)screensaver.ms,v 1.24 1992/02/22 14:00:00 jim Exp jim $
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.\" Change the contents of the following of Pp to PP to have it indent
.de Pp
.LP
..
.de PN
\\fB\\^\\$1\^\\fR\\$2
..
.de Pn
\\$1\\fB\\^\\$2\^\\fR\\$3
..
.de Rq
.sp
.LP
.\" .in +.50i
.PN \\$1
.in +.33i
.sp .5v
.\".LP
..
.de Sm
.\".in -.50i
.in -.33i
.Pp
..
.de Ar
.br
\\fI\\$1\\fP\\^: \\$2
..
.de Rp
.in -.2i
.br
.sp .5v
\(->
.br
.sp .5v
.in +.2i
..
.de Rv
.br
\\$1: \\$2
..
.ps 11
.nr PS 11
.nr PD 1v
\&
.sp 8
.ce 1
\s+2\fBX11 SCREEN SAVER EXTENSION\fP\s-2
.sp 3
.ce 3
Version 1.0
MIT X Consortium Proposed Standard
X Version 11, Release 5
.sp 6
.ce 4
\s-1Jim Fulton
.sp 6p
Network Computing Devices, Inc.\s+1
.sp 3
.ce 4
\s-1Keith Packard
.sp 6p
X Consortium
Laboratory for Computer Science
Massachusetts Institute of Technology\s+1
.ps 9
.nr PS 9
.sp 8
.LP
Copyright \(co 1992 by the Massachusetts Institute of Technology and 
Network Computing Devices, Inc.
.LP
Permission to use, copy, modify, and distribute this documentation for any
purpose and without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.  MIT and
Network Computing Devices, Inc. make no
representations about the suitability for any purpose of the information in
this document.  This documentation is provided ``as is'' without express or
implied warranty.
.ps 11
.nr PS 11
.bp 1
.EH ''\s10X11 Screen Saver Extension\s0''
.OH ''\s10X11 Screen Saver Extension\s0''
.EF ''\s10\fB % \fP\s0''
.OF ''\s10\fB % \fP\s0''
.NH 1
Introduction
.Pp
The X Window System provides support for changing the image on a display screen
after a user-settable period of inactivity to avoid burning the cathode ray
tube phosphors.  However, no interfaces are provided for the user to control
the image that is drawn.  This extension allows an external ``screen saver''
client to detect when the alternate image is to be displayed and to provide the
graphics.
.Pp
Current X server implementations typically provide at least one form of
``screen saver'' image.  Historically, this has been a copy of the X logo
drawn against the root background pattern.  However, many users have asked
for the mechanism to allow them to write screen saver programs that provide
capabilities similar to those provided by other window systems.  In
particular, such users often wish to be able to display corporate logos,
instructions on how to reactivate the screen, and automatic screen-locking
utilities.  This extension provides a means for writing such clients.
.NH 1
Assumptions
.Pp
This extension exports the notion of a special screen saver window that is
mapped above all other windows on a display.  This window has the
\fIoverride-redirect\fP attribute set so that it is not subject to manipulation by
the window manager.  Furthermore, the X identifier for the window is never
returned by \fBQueryTree\fP requests on the root window, so it is typically
not visible to other clients.  
.NH 1
Overview
.Pp
The core
.PN SetScreenSaver
request can be used to set the length of time without
activity on any input devices after which the screen saver should ``activate''
and alter the image on the screen.  This image periodically ``cycles'' to 
reduce
the length of time that any particular pixel is illuminated.  Finally, the
screen saver is ``deactivated'' in response to activity on any of the input 
devices
or particular X requests.
.Pp
Screen saving is typically done by disabling video output to the display tube
or by drawing a changing pattern onto the display.  If the server chooses the
latter approach, a window with a special identifier is created and mapped at
the top of the stacking order where it remains until the screen saver
deactivates.  At this time, the window is unmapped and is not accessible to any
client requests.
.Pp
The server's default mechanism is refered to as the \fIinternal\fP screen 
saver.  An \fIexternal\fP
screen saver client requires a means of determining the window
id for the screen saver window and setting the attributes (e.g. size,
location, visual, colormap) to be used when the window is mapped.  These
requirements form the basis of this extension.
.NH 1
Issues
.Pp
This extension raises several interesting issues.  First is the question of
what should be done if some other client has the server grabbed when the screen
saver is supposed to activate?  This commonly occurs with window managers that
automatically ask the user to position a window when it is first mapped by
grabbing the server and drawing XORed lines on the root window.
.Pp
Second, a screen saver program must control the actual RGB values sent to the
display tube to ensure that the values change periodically to avoid phosphor
burn in.  Thus, the client must have a known colormap installed whenever the
screen saver window is displayed.  To prevent screen flashing, the visual type
of the screen saver window should also be controlable.
.Pp
Third, some implementations may wish to destroy the screen saver window when
it is not mapped so that it need not be avoided during event delivery.  Thus,
screen saver clients may find that the requests that reference the screen 
saver window may fail when the window is not displayed.
.NH 1
Protocol
.Pp
The Screen Saver extension is as follows:
.NH 2
Types
.Pp
In adition to the comon types described in the core protocol, the following
type is used in the request and event definitions in subsequent sections.
.Pp
.TS
lw(2i) lw(3.75i).
_
.sp 6p
.B
Name	Value
.sp 6p
_
.sp 6p
.R
SCREENSAVEREVENT	T{
.Pn { ScreenSaverNotify ,
.PN ScreenSaverCycle }
T}
.TE
.NH 2
Errors
.Pp
The Screen Saver extension adds no errors beyond the core protocol.
.NH 2
Requests
.Pp
The Screen Saver extension adds the following requests:
.Rq ScreenSaverQueryVersion
.Ar client-major-version CARD8
.Ar client-minor-version CARD8
.Rp
.Rv server-major-version CARD8
.Rv server-minor-version CARD8
.Sm
This request allows the client and server to determine which version of
the protocol should be used.  The client sends the version that it
prefers; if the server understands that
version, it returns the same values and interprets subsequent requests
for this extension according to the specified version.  Otherwise,
the server returns the closest version of the protocol that it can
support and interprets subsequent requests according to that version.
This document describes major version 1, minor version 0; the major
and minor revision numbers should only be incremented in response to
incompatible and compatible changes, respectively.
.Rq ScreenSaverQueryInfo
.Ar drawable DRAWABLE
.Rp
.Rv saver-window WINDOW
.Rv state 
.Pn { Disabled ,
.PN Off ,
.PN On }
.Rv kind
.Pn { Blanked ,
.PN Internal ,
.PN External }
.Rv til-or-since CARD32
.Rv idle CARD32
.Rv event-mask SETofSCREENSAVEREVENT
.LP
.Rv Errors
.PN Drawable
.Sm
This request returns information about the state of the screen
saver on the screen associated with \fIdrawable\fP.  The \fIsaver-window\fP 
is the XID that is associated with the screen saver window.  This
window is not guaranteed to exist
except when external screen saver is active.  Although it is a 
child of the root, this window is not returned by
.PN QueryTree
requests on the root.  Whenever this window is mapped, it is always above
any of its siblings in the stacking order.  XXX - TranslateCoords?
.Pp
The \fIstate\fP field specifies whether or not the screen saver is currently
active and how the \fItil-or-since\fP value should be interpretted:
.in +.5i
.TS
lw(1.5i) lw(3.5i).
T{
.PN Off
T}	T{
The screen is not currently being saved; \fItil-or-since\fP
specifies the number of milliseconds until the screen saver is expected to
activate.
T}
.sp
T{
.PN On
T}	T{
The screen is currently being saved; \fItil-or-since\fP specifies
the number of milliseconds since the screen saver activated.
T}
.sp
T{
.PN Disabled
T}	T{
The screen saver is currently disabled; \fItil-or-since\fP is zero.
T}
.TE
.in -.5i
.Pp
The \fIkind\fP field specifies the mechanism that either is currently being
used or would have been were the screen being saved:
.in +.5i
.TS
lw(1.5i) lw(3.5i).
T{
.PN Blanked
T}	T{
The video signal to the display monitor was disabled.
T}
.sp
T{
.PN Internal
T}	T{
A server-dependent, built-in screen saver image was displayed; either no
client had set the screen saver window attributes or a different client 
had the server grabbed when the screen saver activated.
T}
.sp
T{
.PN External
T}	T{
The screen saver window was mapped with attributes set by a
client using the \fBScreenSaverSetAttributes\fP request.
T}
.TE
.in -.5i
.Pp
The \fIidle\fP field specifies the number of milliseconds since the last
input was received from the user on any of the input devices.
.Pp
The \fIevent-mask\fP field specifies which, if any, screen saver
events this client has requested using \fBScreenSaverSelectInput\fP.
.Pp
If \fIdrawable\fP is not a valid drawable identifier, a Drawable
error is returned and the request is ignored.
.Rq ScreenSaverSelectInput
.Ar drawable DRAWABLE
.Ar event-mask SETofSCREENSAVEREVENT
.LP
Errors:
.PN Drawable ,
.PN Match
.Sm
This request specifies which Screen Saver extension events on the screen 
associated with \fIdrawable\fP should be generated for this client.  If
no bits are set in \fIevent-mask\fP, then no events will be generated.
Otherwise, any combination of the following bits may be set:
.in +.5i
.TS
lw(1.5i) lw(3.5i).
T{
.PN ScreenSaverNotify
T}	T{
If this bit is set, \fBScreenSaverNotify\fP events are generated whenever
the screen saver is activated or deactivated.
T}
.sp
T{
.PN ScreenSaverCycle
T}	T{
If this bit is set, \fBScreenSaverNotify\fP events are generated whenever
the screen saver cycle interval passes.
T}
.TE
.in -.5i
.Pp
If \fIdrawable\fP is not a valid drawable identifier, a Drawable
error is returned.  If any undefined bits are set in \fIevent-mask\fP,
a Value error is returned.  If an error is returned,
the request is ignored.
.Rq ScreenSaverSetAttributes
.Ar drawable DRAWABLE
.Ar class 
.Pn { InputOutput , 
.PN InputOnly , 
.PN CopyFromParent }
.Ar depth CARD8
.Ar visual "VISUALID or"
.PN CopyFromParent
.Ar "x, y" INT16
.Ar "width, height, border-width" CARD16
.Ar value-mask BITMASK
.Ar value-list LISTofVALUE
.LP
.Rv Errors
.PN Access ,
.PN Window ,
.PN Pixmap ,
.PN Colormap ,
.PN Cursor ,
.PN Match ,
.PN Value ,
.PN Alloc
.Sm
This request sets the attributes that this client would like to see
used in creating the screen saver window on the screen associated
with \fIdrawable\fP.  If another client currently has the attributes set,
an Access error is generated and the request is ignored.
.Pp
Otherwise, the specified window attributes are checked as if
they were used in a core \fBCreateWindow\fP request whose
parent is the root.  The \fIoverride-redirect\fP field is ignored as
it is implicitly set to True.  If the window attributes result in an
error according to the rules for \fBCreateWindow\fP, the request is
ignored.
.Pp
Otherwise, the attributes are stored and will take effect on the next 
activation that occurs when the server is not grabbed by another client.
Any resources specified for the
\fIbackground-pixmap\fP or \fIcursor\fP attributes may be
freed immediately.  The server is free to copy the \fIbackground-pixmap\fP
or \fIcursor\fP resources or to use them in place; therefore, the effect of
changing the contents of those resources is undefined.  If the 
specified \fIcolormap\fP no longer exists when the screen saver activates,
the parent's colormap is used instead.  If no errors are generated by this
request, any previous
screen saver window attributes set by this client are released.
.Pp
When the screen saver next activates and the server is not grabbed by 
another client, the screen saver window is 
created, if necessary, and set to the specified attributes and events
are generated as usual.  The colormap
associated with the screen saver window is 
installed.  Finally, the screen saver window is mapped.
.Pp
The window remains mapped and at the top of the stacking order
until the screen saver is deactivated in response to activity on
any of the user input devices, a \fBForceScreenSaver\fP request with
a value of Reset, or any request that would cause the window to be
unmapped.
.Pp
If the screen saver activates while the server is grabbed by another
client, the internal saver mechanism is used.  The \fBForceScreenSaver\fP 
request may be used with a value of Active to 
deactivate the internal saver and activate the external saver.
.Pp
If the screen saver client's connection to the server is broken
while the screen saver is activated and the client's close down mode has not
been RetainPermanent or RetainTemporary, the current screen saver
is deactivated and the internal screen saver is immediately activated.
.Pp
When the screen saver deactivates, the screen saver window's colormap
is uninstalled and the window is unmapped (except as described below).
The screen saver XID is disassociated
with the window and the server may, but is not required to,
destroy the window along with any children.
.Pp
When the screen saver is being deactivated and then immediately
reactivated (such as when switching screen savers), the server
may leave the screen saver window mapped (typically to avoid 
generating exposures).
.Rq ScreenSaverUnsetAttributes
.Ar drawble DRAWABLE
.LP
.Rv Errors
.PN Drawable
.Sm
This request notifies the server that this client no longer
wishes to control the screen saver window.  Any screen saver
attributes set by this client and any descendents of the screen
saver window created by this client should be released
immediately if the screen saver is not active, else upon
deactivation.
.Pp
This request is ignored if the client has not previously set the screen saver
window attributes.
.NH 2
Events
.Pp
The Screen Saver extension adds one event:
.Rq ScreenSaverNotify
.Ar root WINDOW
.Ar window WINDOW
.Ar state
.Pn { Off ,
.PN On ,
.PN Cycle }
.Ar kind 
.Pn { Blanked ,
.PN Internal ,
.PN External }
.Ar forced BOOL
.Ar time TIMESTAMP
.Sm
This event is delivered to clients that have requested
ScreenSaverNotify events using the \fBScreenSaverSelectInput\fP request
whenever the screen saver activates or deactivates.
.Pp
The \fIroot\fP field specifies root window of the screen for
which the event was generated.  The \fIwindow\fP field specifies
the value that is returned by \fBScreenSaverQueryInfo\fP as
the identifier for the screen saver window.  This window is not
required to exist if the external screen saver is not active.
.Pp
The \fIstate\fP field specifies the cause of the event:
.in +.5i
.TS
lw(1.5i) lw(3.5i).
T{
.PN Off
T}	T{
The screen saver deactivated; this event is sent if the client has set the
ScreenSaverNotify bit in its event mask.
T}
.sp
T{
.PN On
T}	T{
The screen saver activated.  This event is sent if the client has set the
ScreenSaverNotify bit in its event mask.
T}
.sp
T{
.PN Cycle
T}	T{
The cycle interval passed and the client is expected to change the image on
the screen.  This event is sent if the client has set the
ScreenSaverCycle bit in its event mask.
T}
.TE
.in -.5i
.LP
If \fIstate\fP is set to 
.PN On 
or
.PN Off
then \fIforced\fP indicates whether or not activation or
deactivation was caused by a core
.PN ForceScreenSaver
request; otherwise, \fIforced\fP is set to False.
.Pp
The \fIkind\fP field specifies mechanism that was used to save the screen
when the screen saver was activated, as described in 
\fBScreenSaverQueryInfo\fP.
.Pp
The \fItime\fP field indicates the server time when the 
event was generated.
.NH 1
Encoding
.Pp
Please refer to the X11 Protocol Encoding document as this document uses
conventions established there.
.Pp
The name of this extension is ``SCREEN-SAVER''.
.LP
.NH 2
Common Types
.LP
.TA .75i 1.75i
.ta .75i 1.75i
.nf
.R
SETofSCREENSAVEREVENT
	#x00000001	ScreenSaverNotifyMask
	#x00000002	ScreenSaverCycleMask
.fi
.NH 2
Requests
.de En
.LP
.PN \\$1
.TA .5i 1.5i 2.5i
.ta .5i 1.5i 2.5i
.in +.33i
.nf
..
.de Ee
.in -.33i
.fi
..
.En ScreenSaverQueryVersion
1	CARD8		screen saver opcode
1	0		minor opcode
2   	2		request length
1	CARD8		client major version
1	CARD8		client minor version
2			unused
.Rp
1	1		Reply
1			unused
2	CARD16		sequence number
4	0		reply length
1	CARD8		server major version
1	CARD8		server minor version
22			unused
.Ee
.En ScreenSaverQueryInfo
1	CARD8		screen saver opcode
1	1		minor opcode
2	2		request length
4	DRAWABLE		drawable associated with screen
.Rp
1	1		Reply
1	CARD8		state
	0	Off
	1	On
	3	Disabled
2	CARD16		sequence number
4	0		reply length
4	WINDOW		saver window
4	CARD32		milliseconds until saver or since saver
4	CARD32		milliseconds since last user device input
4	SETofSCREENSAVEREVENT		event mask
1	CARD8		kind
	0	Blanked
	1	Internal
	2	External
10			unused
.Ee
.En ScreenSaverSelectInput
1	CARD8		screen saver opcode
1	2		minor opcode
2	3		request length
4	DRAWABLE		drawable associated with screen
4	SETofSCREENSAVEREVENT		event mask
.Ee
.En ScreenSaverSetAttributes
1	CARD8		screen saver opcode
1	3		minor opcode
2	6+n		request length
4	DRAWABLE		drawable associated with screen
2	INT16		x
2	INT16		y
2	CARD16		width
2	CARD16		height
2	CARD16		border-width
1			class
	0	CopyFromParent
	1	InputOutput
	2	InputOnly
1	CARD8		depth
4	VISUALID		visual
	0	CopyFromParent
4	BITMASK			value-mask (has n bits set to 1)
	encodings are the same as for core CreateWindow
4n	LISTofVALUE		value-list
	encodings are the same as for core CreateWindow
.Ee
.En ScreenSaverUnsetAttributes
1	CARD8		screen saver opcode
1	4		minor opcode
2	3		request length
4	DRAWABLE		drawable associated with screen
.Ee
.NH 2
Events
.En ScreenSaverNotify
1	CARD8		code assigned by core
1	CARD8		state
	0	Off
	1	On
	2	Cycle
2	CARD16		sequence number
4	TIMESTAMP		time
4	WINDOW		root
4	WINDOW		screen saver window
1	CARD8		kind
	0	Blanked
	1	Internal
	2	External
1	BOOL		forced
14			unused
.Ee
.NH 1
Inter-Client Communications Conventions
.Pp
Screen saver clients should create at least one resource value whose
identifier can be stored in a property named
.PN _SCREEN_SAVER_ID
on the root of each screen it is managing.
This property should have one 32-bit value corresponding to the resource
identifier; the type of the property should indicate the type of the
resource and should be one of the following: 
.PN WINDOW ,
.PN PIXMAP ,
.PN CURSOR ,
.PN FONT , or
.PN COLORMAP .
.NH 1
C language binding
.Pp
The C binding for this extension simply provide access to the protocol; they
add no semantics beyond what is described above.
.Pp
The include file for this extension is
.Pn < X11/extensions/scrnsaver.h >.
.LP
Bool
.br
XScreenSaverQueryExtension (display, event_base, error_base)
.RS
Display *display;
.br
int *event_base; /* RETURN */
.br
int *error_base; /* RETURN */
.RE
.IP
This routine returns
.PN True
if the specified \fIdisplay\fP supports the SCREEN-SAVER extension; 
otherwise it returns
.PN False .
If the extension is supported, the event number for 
.PN ScreenSaverNotify
events is returned in the value pointed to by \fIevent_base\fP.  Since
no additional errors are defined by this extension, the results
of \fIerror_base\fP are not defined.
.LP
Status
.br
XScreenSaverQueryVersion (display, major, minor)
.RS
Display *display;
.br
int *major; /* RETURN */
.br
int *minor; /* RETURN */
.RE
.IP
If the specified \fIdisplay\fP supports the extension,
the version numbers of the protocol
expected by the server are returned in \fImajor\fP and \fIminor\fP and
a non-zero value is returned.  Otherwise, the arguments are not 
set and 0 is returned.
.LP
XScreenSaverInfo *
.br
XScreenSaverAllocInfo ()
.IP
This routine allocates and returns an \fBXScreenSaverInfo\fP structure
for use in calls to \fBXScreenSaverQueryInfo\fP.  All fields in the
structure are initialized to zero.  If insufficient memory is available,
NULL is returned.  The results of this routine can be released
using \fIXFree\fP.
.LP
Status
.br
XScreenSaverQueryInfo (display, drawable, saver_info)
.RS
Display *display;
.br
Drawable drawable;
.br
XScreenSaverInfo *saver_info; /* RETURN */
.RE
.IP
If the specified \fIdisplay\fP supports the extension,
information about the current state of the
screen server is returned in \fIsaver_info\fP and a non-zero value is
returned.  The \fBXScreenSaverInfo\fP structure is defined as follows:
.sp
.in +.5i
.TA 4i
.ta 4i
typedef struct {
    Window window;                /* screen saver window */
    int state;                    /* ScreenSaver{Off,On,Disabled} */
    int kind;                     /* ScreenSaver{Blanked,Internal,External} */
    unsigned long til_or_since;   /* milliseconds */
    unsigned long idle;           /* milliseconds */
    unsigned long event_mask;     /* events */
.br
} XScreenSaverInfo;
.in -.5i
.sp
See the \fBScreenSaverQueryInfo\fP request for a description of the fields.
If the extension is not supported, \fIsaver_info\fP is not changed and 0
is returned.
.LP
void
.br
XScreenSaverSelectInput (display, drawable, event_mask)
.RS
Display *display;
.br
Drawable drawable;
.br
unsigned long event_mask;
.RE
.IP
If the specified \fIdisplay\fP supports the extension,
this routine asks that events related to 
the screen saver be generated for this client.
The format of the events generated is:
.sp
.in +.5i
.TA 4i
.ta 4i
typedef struct {
    int type;               /* of event */
    unsigned long serial;   /* # of last request processed by server */
    Bool send_event;        /* true if this came frome a SendEvent request */
    Display *display;       /* Display the event was read from */
    Window window;          /* screen saver window */
    Window root;            /* root window of event screen */
    int state;              /* ScreenSaver{Off,On,Cycle} */
    int kind;               /* ScreenSaver{Blanked,Internal,External} */
    Bool forced;            /* extents of new region */
    Time time;              /* event timestamp */
.br
} XScreenSaverNotifyEvent;
.in -.5i
.sp
See the
definition of the \fBScreenSaverSelectInput\fP request for descriptions
of the allowed event masks.  
.LP
void
.br
XScreenSaverSetAttributes (display, drawable, x, y, width, height, border_width, depth, class, visual, valuemask, attributes)
.RS
Display *dpy;
.br
Drawable drawable;
.br
int x;
.br
int y;
.br
unsigned int width;
.br
unsigned int height;
.br
unsigned int border_width;
.br
int depth;
.br
unsigned int class;
.br
Visual *visual;
.br
unsigned long valuemask;
.br
XSetWindowAttributes *attributes;
.RE
.IP
If the specified \fIdisplay\fP supports the extension,
this routine sets the attributes to be used
the next time the external screen saver is activated.  See the definition
of the \fBScreenSaverSetAttributes\fP request for a description of each of
the arguments.
.LP
void
.br
XScreenSaverUnsetAttributes (display, drawable)
.RS
Display *display;
.br
Drawable drawable;
.RE
.IP
If the specified \fIdisplay\fP supports the extension,
this routine instructs the server to discard
any previous screen saver window attributes set by this client.
.LP
Status
.br
XScreenSaverRegister (display, screen, xid, type)
.RS
Display *display;
.br
int screen;
.br
XID xid;
.br
Atom type;
.RE
.IP
This routine stores the given \fIXID\fP in the \fB_SCREEN_SAVER_ID\fP 
property (of the given \fItype\fP) on the
root window of the specified \fIscreen\fP.  It returns zero if an error
is encountered and the property is not changed, otherwise it returns 
non-zero.
.LP
Status
.br
XScreenSaverUnregister (display, screen)
.RS
Display *display;
.br
int screen;
.RE
.IP
This routine removes any \fB_SCREEN_SAVER_ID\fP from the 
root window of the specified \fIscreen\fP.  It returns zero if an error
is encountered and the property is changed, otherwise it returns 
non-zero.
.LP
Status
.br
XScreenSaverGetRegistered (display, screen, xid, type)
.RS
Display *display;
.br
int screen;
.br
XID *xid; /* RETURN */
.br
Atom *type; /* RETURN */
.RE
.IP
This routine returns the \fIXID\fP and \fItype\fP stored in
the \fB_SCREEN_SAVER_ID\fP property on the
root window of the specified \fIscreen\fP.  It returns zero if an error
is encountered or if the property does not exist or is not of the correct
format; otherwise it returns non-zero.
