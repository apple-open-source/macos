.\" Use -ms and macros.t
.\" $Xorg: shape.ms,v 1.3 2000/08/17 19:42:36 cpqbld Exp $
.\" $XdotOrg: doc/xorg-docs/specs/Xext/shape.ms,v 1.4 2006/02/17 15:12:55 keithp Exp $
.\" edited for DP edits and code consistency w/ core protocol/xlib 4/1/96
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.sp 8
.ce 3
\s+2\fBX Nonrectangular Window

Shape Extension Protocol\fP\s-2
.sp 3
.ce 3
Version 1.1
X.Org Standard
X Version 11, Release 7.0
.sp 6
.ce 4
\s-1Keith Packard
.sp 6p
MIT X Consortium
.br
Intel Corporation
.ps 9
.nr PS 9
.sp 8
.LP
Copyright \(co 1989, 2004 The Open Group
.br
Copyright \(co 2006 Keith Packard
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
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the copyright holders shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the
copyright holders.
.ps 10
.nr PS 10
.bp 1
.EH ''X11 Nonrectangular Window Shape Extension Protocol''
.OH ''X11 Nonrectangular Window Shape Extension Protocol''
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.NH 1
Overview
.LP
This extension provides arbitrary window and border shapes within the X11
protocol.
.LP
The restriction of rectangular windows within the X protocol is a significant
limitation in the implementation of many styles of user interface.  For
example, many transient windows would like to display a ``drop shadow'' to
give the illusion of 3 dimensions.  As another example, some user interface
style guides call for buttons with rounded corners; the full simulation of a
nonrectangular shape, particularly with respect to event distribution and
cursor shape, is not possible within the core X protocol.  As a final
example, round clocks and nonrectangular icons are desirable visual addition
to the desktop.
.LP
This extension provides mechanisms for changing both the visible and interactive shape of a
window to arbitrary, possibly disjoint, nonrectangular forms.  The intent
of the extension is to supplement the existing semantics, not replace them.
In particular, it is desirable for clients that are unaware of the
extension to still be able to cope reasonably with shaped windows.  For
example, window managers should still be able to negotiate screen
real estate in rectangular pieces.  Toward this end, any shape specified for
a window is clipped by the bounding rectangle for the window as specified by
the window's geometry in the core protocol.  An expected convention would be
that client programs expand their shape to fill the area offered by the
window manager.
.NH 1
Description
.LP
Each window (even with no shapes specified) is defined by three regions:  the
\fIbounding region\fP, the \fIclip region\fP and the \fIinput region\fP.  The bounding region is the area of the
parent window that the window will occupy (including border). The clip region
is the subset of the bounding region that is available for subwindows and
graphics.  The area between the bounding region and the clip region is defined
to be the border of the window. The input region is the subset of the
bounding region that can ``contain'' the pointer.
.LP
A nonshaped window will have a bounding region that is a rectangle
spanning the window, including its border; the clip region will be a rectangle
filling the inside dimensions (not including the border); the input
region will match the bounding region.  In this document,
these areas are referred to as the \fIdefault bounding region\fP, the
\fIdefault clip region\fP and the \fIdefault input region\fP.  For a window with inside size of \fIwidth\fP by
\fIheight\fP and border width \fIbwidth\fP, the default bounding, clip
and input
regions are the rectangles (relative to the window origin):
.LP
.sM
.Ds 0
bounding.x = -\fIbwidth\fP
bounding.y = -\fIbwidth\fP
bounding.width = \fIwidth\fP + 2 * \fIbwidth\fP
bounding.height = \fIheight\fP + 2 * \fIbwidth\fP

clip.x = 0
clip.y = 0
clip.width = \fIwidth\fP
clip.height = \fIheight\fP

input.x = -\fIbwidth\fP
input.y = -\fIbwidth\fP
input.width = \fIwidth\fP + 2 * \fIbwidth\fP
input.height = \fIheight\fP + 2 * \fIbwidth\fP

.De
.LP
.eM
This extension allows a client to modify any combination of the bounding,
clip or input regions by specifying new regions that combine with the default
regions.  These new regions are called the \fIclient bounding region\fP,
the \fIclient clip region\fP and the \fIclient input region\fP.  They are specified relative to the origin of
the window and are always defined by offsets relative to the window origin
(that is, region adjustments are not required when the window is moved).
Three mechanisms for specifying regions are provided:  a list of rectangles,
a bitmap, and an existing bounding or clip region from a window.  This is
modeled on the specification of regions in graphics contexts in the core
protocol and allows a variety of different uses of the extension.
.LP
When using an existing window shape as an operand in specifying a new shape,
the client region is used, unless none has been set, in which case the
default region is used instead.
.LP
The \fIeffective bounding region\fP of a window is defined to be the intersection of
the client bounding region with the default bounding region.  Any portion of
the client bounding region that is not included in the default bounding
region will not be included in the effective bounding region on the screen.
This means that window managers (or other geometry managers) used to dealing
with rectangular client windows will be able to constrain the client to a
rectangular area of the screen.
.LP
Construction of the effective bounding region is dynamic; the client bounding
region is not mutated to obtain the effective bounding region.  If a client
bounding region is specified that extends beyond the current default bounding
region, and the window is later enlarged, the effective bounding region will
be enlarged to include more of the client bounding region.
.LP
The \fIeffective clip region\fP of a window is defined to be the intersection of the
client clip region with both the default clip region and the client bounding
region.  Any portion of the client clip region that is not included in both
the default clip region and the client bounding region will not be included in
the effective clip region on the screen.
.LP
Construction of the effective clip region is dynamic; the client clip region is
not mutated to obtain the effective clip region.  If a client clip region is
specified that extends beyond the current default clip region and the
window or its bounding region is later enlarged, the effective clip region will
be enlarged to include more of the client clip region if it is included in
the effective bounding region.
.LP
The border of a window is defined to be the difference between the effective
bounding region and the effective clip region.  If this region is empty, no
border is displayed.  If this region is nonempty, the border is filled
using the border-tile or border-pixel of the window as specified in the core
protocol.  Note that a window with a nonzero border width will never be able
to draw beyond the default clip region of the window.  Also note that a zero
border width does not prevent a window from having a border, since the clip
shape can still be made smaller than the bounding shape.
.LP
All output to the window and visible regions of any subwindows will be
clipped to the effective clip region.  The server must not retain window
contents beyond the effective bounding region with backing store.  The window's
origin (for graphics operations, background tiling, and subwindow placement)
is not affected by the existence of a bounding region or clip region.
.LP
The \fIeffective input region\fP of a window is defined to be the intersection of the
client input region with both the default input region and the client bounding
region.  Any portion of the client input region that is not included in both
the default input region and the client bounding region will not be included in
the effective input region on the screen.
.LP
Construction of the effective input region is dynamic; the client input region is
not mutated to obtain the effective input region.  If a client input region is
specified that extends beyond the current default input region and the
window or its bounding region is later enlarged, the effective input region will
be enlarged to include more of the client input region if it is included in
the effective bounding region.
.LP 
Areas that are inside the default bounding region but outside the effective
bounding region are not part of the window; these areas of the screen will
be occupied by other windows.  Input events that occur within the default
bounding region but outside the effective bounding region will be delivered as
if the window was not occluding the event position.  Events that occur in
a nonrectangular border of a window will be delivered to that window, just
as for events that occur in a normal rectangular border.
.LP
An 
.PN InputOnly
window can have its bounding or input region set, but it is a
.PN Match
error to attempt to set a clip region on an
.PN InputOnly
window or to specify its clip region as a source to a request 
in this extension.
.LP
The server must accept changes to the clip and input regions of a root window, but
the server is permitted to ignore requested changes to the bounding region
of a root window.  If the server accepts bounding region changes, the contents
of the screen outside the bounding region are implementation dependent.
.NH 1
Types
.LP
The following types are used in the request and event definitions in
subsequent sections.
.LP
SHAPE_KIND:
.Pn { Bounding ,
.PN Clip ,
.PN Input }
.LP
SHAPE_OP:
.Pn { Set ,
.PN Union ,
.PN Intersect ,
.PN Subtract ,
.PN Invert }
.LP
.PN Set
indicates that the region specified as an explicit source in the request is stored
unaltered as the new destination client region.
.PN Union
indicates that the source and destination regions are unioned together to produce
the new destination client region.
.PN Intersect
indicates that the source and destination regions are intersected together to
produce the new destination client region.
.PN Subtract
indicates that the source region is subtracted from the destination region to
produce the new destination region.
.PN Invert
indicates that the destination region is subtracted from the source region to
produce the new destination region.
.NH 1
Requests
.LP
.sM
.PN "ShapeQueryVersion"
.LP
   =>
.IP
majorVersion: CARD16
.br
minorVersion: CARD16
.LP
.eM
This request can be used to ensure that the server version of the SHAPE
extension is usable by the client.  This document defines major version one
(1), minor version one (1).
.LP
.sM
.PN "ShapeRectangles"
.IP
\fIdest\fP\^: WINDOW
.br
\fIdestKind\fP\^: SHAPE_KIND
.br
\fIop\fP\^: SHAPE_OP
.br
\fIxOff, yOff\fP\^: INT16
.br
\fIrectangles\fP\^: LISTofRECTANGLES
.br
\fIordering\fP\^: 
.Pn { UnSorted ,
.PN YSorted ,
.PN YXSorted ,
.PN YXBanded }
.IP
Errors:
.PN Window ,
.PN Length ,
.PN Match ,
.PN Value
.LP
.eM
This request specifies an array of rectangles, relative to the origin of the
window plus the specified offset (xOff and yOff) that together
define a region.  This region is combined (as specified by the operator
op) with the existing client region (specified by destKind) of the
destination window, and the result is stored as the specified client region of
the destination window.  Note that the list of rectangles can be empty,
specifying an empty region; this is not the same as passing
.PN None
to
.PN ShapeMask .
.LP
If known by the client, 
ordering relations on the rectangles can be specified with the ordering 
argument.
This may provide faster operation by the server.
The meanings of the ordering values are the same as in the core protocol
.PN SetClipRectangles
request.
If an incorrect ordering is specified, 
the server may generate a 
.PN Match 
error, but it is not required to do so.
If no error is generated,
the graphics results are undefined.
Except for
.PN UnSorted ,
the rectangles should be nonintersecting, or the resulting region will
be undefined.
.PN UnSorted 
means that the rectangles are in arbitrary order.
.PN YSorted 
means that the rectangles are nondecreasing in their Y origin.
.PN YXSorted 
additionally constrains 
.PN YSorted 
order in that all rectangles with an equal Y origin are
nondecreasing in their X origin.
.PN YXBanded 
additionally constrains 
.PN YXSorted 
by requiring that, for every possible Y scanline,
all rectangles that include that scanline have identical Y origins and Y
extents.
.LP
.sM
.PN "ShapeMask"
.IP
\fIdest\fP\^: WINDOW
.br
\fIdestKind\fP\^: SHAPE_KIND
.br
\fIop\fP\^: SHAPE_OP
.br
\fIxOff, yOff\fP\^: INT16
.br
\fIsource\fP\^: PIXMAP or
.PN None
.IP
Errors:
.PN Window ,
.PN Pixmap ,
.PN Match ,
.PN Value
.LP
.eM
The source in this request is a 1-bit deep pixmap, or
.PN None .
If source is 
.PN None ,
the specified client region is removed from the window, causing the effective
region to revert to the default region.  The
.PN ShapeNotify
event generated by this request and subsequent
.PN ShapeQueryExtents
will report that a client shape has not been specified.
If a valid pixmap is specified, it is converted
to a region, with bits set to one included in the region and bits set to
zero excluded, and an offset from the window origin as specified by
xOff and yOff.  The resulting region is then combined (as
specified by the operator op) with the existing client region
(indicated by destKind) of the destination window, and the result is
stored as the specified client region of the destination window.  The source
pixmap and destination window must have been created on the same screen,
or else a
.PN Match
error results.
.LP
.sM
.PN "ShapeCombine"
.IP
\fIdest\fP\^: WINDOW
.br
\fIdestKind\fP\^: SHAPE_KIND
.br
\fIop\fP\^: SHAPE_OP
.br
\fIxOff, yOff\fP\^: INT16
.br
\fIsource\fP\^: WINDOW
.br
\fIsourceKind\fP\^: SHAPE_KIND
.IP
Errors:
.PN Window ,
.PN Match ,
.PN Value
.LP
.eM
The client region, indicated by sourceKind, of the source window is
offset from the window origin by xOff and yOff and combined with
the client region, indicated by destKind, of the destination window.
The result is stored as the specified client region of the destination
window.
The source and destination windows must be on the same screen, or else a
.PN Match
error results.
.LP
.sM
.PN "ShapeOffset"
.IP
\fIdest\fP\^: WINDOW
.br
\fIdestKind\fP\^: SHAPE_KIND
.br
\fIxOff, yOff\fP\^: INT16
.IP
Errors:
.PN Window ,
.PN Match ,
.PN Value
.LP
.eM
The client region, indicated by destKind, is moved relative to its
current position by the amounts xOff and yOff.
.LP
.sM
.PN "ShapeQueryExtents"
.IP
\fIdest\fP\^: WINDOW
.LP
   =>
.IP
boundingShaped: BOOL
.br
clipShaped: BOOL
.br
xBoundingShape: INT16
.br
yBoundingShape: INT16
.br
widthBoundingShape: CARD16
.br
heightBoundingShape: CARD16
.br
xClipShape: INT16
.br
yClipShape: INT16
.br
widthClipShape: CARD16
.br
heightClipShape: CARD16
.IP
Errors:
.PN Window
.LP
.eM
The boundingShaped and clipShaped results are 
.PN True
if the corresponding client regions have been specified, else they are
.PN False .
The x, y, width, and height values define the extents of the client regions,
when a client region has not been specified, the extents of the
corresponding default region are reported.
.LP
.sM
.PN "ShapeSelectInput"
.IP
\fIwindow\fP\^: WINDOW
.br
\fIenable\fP\^: BOOL
.IP
Errors:
.PN Window ,
.PN Value
.LP
.eM
Specifying enable as
.PN True
causes the server to send the requesting client a
.PN ShapeNotify
event whenever the bounding, clip or input region of the specified window is
altered by any client.
Specifying enable as
.PN False
causes the server to stop sending such events.
.LP
.sM
.PN "ShapeInputSelected"
.IP
\fIwindow\fP\^: WINDOW
.LP
   =>
.IP
enable: BOOL
.IP
Errors:
.PN Window
.LP
.eM
If enable is
.PN True ,
then
.PN ShapeNotify
events for the window are generated for this client.
.LP
.sM
.PN "ShapeGetRectangles"
.IP
\fIwindow\fP\^: WINDOW
.br
\fIkind\fP\^: SHAPE_KIND
.LP
   =>
.IP
rectangles: LISTofRECTANGLE
.br
ordering: 
.Pn { UnSorted ,
.PN YSorted ,
.PN YXSorted ,
.PN YXBanded }
.IP
Errors:
.PN Window,
.PN Match
.LP
.eM
A list of rectangles describing the region indicated by kind, and the
ordering of those rectangles, is returned.  The meaning of the ordering
values is the same as in the
.PN ShapeRectangles
request.
.NH 1
Events
.LP
.sM
.PN "ShapeNotify"
.IP
\fIwindow\fP\^: WINDOW
.br
\fIkind\fP\^: SHAPE_KIND
.br
\fIshaped\fP\^: BOOL
.br
\fIx\fP, \fIy\fP\^: INT16
.br
\fIwidth\fP, \fIheight\fP\^: CARD16
.br
\fItime\fP\^: TIMESTAMP
.LP
.eM
Whenever the client bounding, clip or input shape of a window is modified, a
.PN ShapeNotify
event is sent to each client that has used
.PN ShapeSelectInput
to request it.
.LP
Kind indicates which client region (bounding or clip) has been modified;
shaped is
.PN True
when the window has a client shape of type kind, and is
.PN False
when the window no longer has a client shape of this type.
The x, y, width, and height indicate the extents of the
current shape.  When shaped is
.PN False
these will indicate the extents of the default region.  The timestamp
indicates the server time when the shape was changed.
.NH 1
Encoding
.LP
Please refer to the X11 Protocol Encoding document as this document uses
conventions established there.
.LP
The name of this extension is ``SHAPE''.
.LP
.NH 2
New Types
.LP
.Ds 0
.TA .75i 1.75i
.ta .75i 1.75i
.R
SHAPE_KIND
	0	Bounding
	1	Clip
	2	Input
.De
.LP
.Ds 0
.TA .75i 1.75i
.ta .75i 1.75i
.R
SHAPE_OP
	0	Set
	1	Union
	2	Intersect
	3	Subtract
	4	Invert
.De
.NH 2
Requests
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeQueryVersion
.sp 6p
	1	CARD8		opcode
	1	0		shape opcode
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
	2	CARD16		major version
	2	CARD16		minor version
	20			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeRectangles
.sp 6p
	1	CARD8		opcode
	1	1		shape opcode
	2	4+2n		request length
	1	SHAPE_OP		operation
	1	SHAPE_KIND		destination kind
	1			ordering
		0	UnSorted
		1	YSorted
		2	YXSorted
		3	YXBanded
	1			unused
	4	WINDOW		destination window
	2	INT16		x offset
	2	INT16		y offset
	8n	LISTofRECTANGLE		rectangles
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeMask
.sp 6p
	1	CARD8		opcode
	1	2		shape opcode
	2	5		request length
	1	SHAPE_OP		operation
	1	SHAPE_KIND		destination kind
	2			unused
	4	WINDOW		destination window
	2	INT16		x offset
	2	INT16		y offset
	4	PIXMAP		source bitmap
		0	None
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeCombine
.sp 6p
	1	CARD8		opcode
	1	3		shape opcode
	2	5		request length
	1	SHAPE_OP		operation
	1	SHAPE_KIND		destination kind
	1	SHAPE_KIND		source kind
	1			unused
	4	WINDOW		destination window
	2	INT16		x offset
	2	INT16		y offset
	4	WINDOW		source window
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeOffset
.sp 6p
	1	CARD8		opcode
	1	4		shape opcode
	2	4		request length
	1	SHAPE_KIND		destination kind
	3			unused
	4	WINDOW		destination window
	2	INT16		x offset
	2	INT16		y offset
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeQueryExtents
.sp 6p
	1	CARD8		opcode
	1	5		shape opcode
	2	2		request length
	4	WINDOW		destination window
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	1	BOOL		bounding shaped
	1	BOOL		clip shaped
	2			unused
	2	INT16		bounding shape extents x
	2	INT16		bounding shape extents y
	2	CARD16		bounding shape extents width
	2	CARD16		bounding shape extents height
	2	INT16		clip shape extents x
	2	INT16		clip shape extents y
	2	CARD16		clip shape extents width
	2	CARD16		clip shape extents height
	4			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeSelectInput
.sp 6p
	1	CARD8		opcode
	1	6		shape opcode
	2	3		request length
	4	WINDOW		destination window
	1	BOOL		enable
	3			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeInputSelected
.sp 6p
	1	CARD8		opcode
	1	7		shape opcode
	2	2		request length
	4	WINDOW		destination window
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		Reply
	1	BOOL		enabled
	2	CARD16		sequence number
	4	0		reply length
	24			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeGetRectangles
.sp 6p
	1	CARD8		opcode
	1	8		shape opcode
	2	3		request length
	4	WINDOW		window
	1	SHAPE_KIND		source kind
	3			unused
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		Reply
	1			ordering
		0	UnSorted
		1	YSorted
		2	YXSorted
		3	YXBanded
	2	CARD16		sequence number
	4	2n		reply length
	4	CARD32		nrects
	20			unused
	8n	LISTofRECTANGLE		rectangles
.De
.NH 2
Events
.LP
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN ShapeNotify
.sp 6p
	1	CARD8		type (0 + extension event base)
	1	SHAPE_KIND		shape kind
	2	CARD16		sequence number
	4	WINDOW		affected window
	2	INT16		x value of extents
	2	INT16		y value of extents
	2	CARD16		width of extents
	2	CARD16		height of extents
	4	TIMESTAMP		server time
	1	BOOL		shaped
	11			unused
.De
.NH 1
Glossary
.LP
.KS
\fBbounding region\fP
.IP
The area of the parent window that this window will occupy.  This area is
divided into two parts:  the border and the interior.
.KE
.LP
.KS
\fBclip region\fP
.IP
The interior of the window, as a subset of the bounding region.  This
region describes the area that will be painted with the window background
when the window is cleared, will contain all graphics output to the window,
and will clip any subwindows.
.KE
.LP
.KS
\fBinput region\fP
.IP
The subset of the bounding region which can ``contain'' the
pointer.
.KE
.LP
.KS
\fBdefault bounding region\fP
.IP
The rectangular area, as described by the core protocol window size, that
covers the interior of the window and its border.
.KE
.LP
.KS
\fBdefault clip region\fP
.IP
The rectangular area, as described by the core protocol window size, that
covers the interior of the window and excludes the border.
.KE
.LP
.KS
\fBdefault input region\fP
.IP
The rectangular area, as described by the core protocol window size, that
covers the interior of the window and its border.
.KE
.LP
.KS
\fBclient bounding region\fP
.IP
The region associated with a window that is directly modified via this
extension when specified by 
.PN ShapeBounding .
This region is used in conjunction with the default bounding region
to produce the effective bounding region.
.KE
.LP
.KS
\fBclient clip region\fP
.IP
The region associated with a window that is directly modified via this
extension when specified by 
.PN ShapeClip . 
This region is used in conjunction with the default clip region 
and the client bounding region to produce the effective clip region.
.KE
.LP
.KS
\fBclient input region\fP
.IP
The region associated with a window that is directly modified via this
extension when specified by 
.PN ShapeInput . 
This region is used in conjunction with the default input region 
and the client bounding region to produce the effective input region.
.KE
.LP
.KS
\fBeffective bounding region\fP
.IP
The actual shape of the window on the screen, including border and interior
(but excluding the effects of overlapping windows).  When a window has a client
bounding region, the effective bounding region is the intersection of the
default bounding region and the client bounding region.  Otherwise, the
effective bounding region is the same as the default bounding region.
.KE
.LP
.KS
\fBeffective clip region\fP
.IP
The actual shape of the interior of the window on the screen (excluding the
effects of overlapping windows).  When a window has a client clip region or
a client bounding region, the effective clip region is the intersection of
the default clip region, the client clip region (if any) and the client
bounding region (if any).  Otherwise, the effective clip region is the
same as the default clip region.
.KE
.LP
.KS
\fBeffective input region\fP
.IP
The actual shape of the window on the screen (excluding the
effects of overlapping windows) which can ``contain'' the pointer.
When a window has a client input region or
a client bounding region, the effective input region is the intersection of
the default input region, the client input region (if any) and the client
bounding region (if any).  Otherwise, the effective input region is the
same as the default input region.
.KE
.NH 1
Revision History
.LP
1.0 - 1989 - Original Revision
.LP
1.0.1 - March 2004 - Corrected misnumbering of \fIShapeInputSelected\fP 
and \fIShapeGetRectangles\fP requests in encoding section.
.LP
1.1 - February 2006 - Added Input regions.
