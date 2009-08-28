.\" Use -ms and macros.t
.\" $Xorg: shapelib.ms,v 1.3 2000/08/17 19:42:36 cpqbld Exp $
.\" $XdotOrg: xc/doc/specs/Xext/shapelib.ms,v 1.2 2004/04/23 18:42:18 eich Exp $
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

Shape Extension Library\fP\s-2
.sp 3
.ce 3
Version 1.0
X Consortium Standard
X Version 11, Release 6.8
.sp 6
.ce 4
\s-1Keith Packard
.sp 6p
MIT X Consortium
.ps 9
.nr PS 9
.sp 8
.LP
Copyright \(co 1989 X Consortium
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
.ps 10
.nr PS 10
.bp 1
.EH ''X Nonrectangular Window Shape Extension Library''
.OH ''X Nonrectangular Window Shape Extension Library''
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
This extension provides mechanisms for changing the visible shape of a
window to an arbitrary, possibly disjoint, nonrectangular form.  The intent
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
Each window (even with no shapes specified) is defined by two regions:  the
\fIbounding region\fP and the \fIclip region\fP.  The bounding region is the area of the
parent window that the window will occupy (including border). The clip region
is the subset of the bounding region that is available for subwindows and
graphics.  The area between the bounding region and the clip region is defined
to be the border of the window.
.LP
A nonshaped window will have a bounding region that is a rectangle
spanning the window, including its border; the clip region will be a rectangle
filling the inside dimensions (not including the border).  In this document,
these areas are referred to as the \fIdefault bounding region\fP and the
\fIdefault clip region\fP.  For a window with inside size of \fIwidth\fP by
\fIheight\fP and border width \fIbwidth\fP, the default bounding and clip
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
.De
.LP
.eM
This extension allows a client to modify either or both of the bounding or
clip regions by specifying new regions that combine with the default
regions.  These new regions are called the \fIclient bounding region\fP and
the \fIclient clip region\fP.  They are specified relative to the origin of
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
window can have its bounding region set, but it is a
.PN Match
error to attempt to set a clip region on an
.PN InputOnly
window or to specify its clip region as a source to a request 
in this extension.
.LP
The server must accept changes to the clip region of a root window, but
the server is permitted to ignore requested changes to the bounding region
of a root window.  If the server accepts bounding region changes, the contents
of the screen outside the bounding region are implementation dependent.
.NH 1
C Language Binding
.LP
The C functions provide direct access to the protocol and add no additional
semantics.
.LP
The include file for this extension is
.Pn < X11/extensions/shape.h >.
The defined shape kinds are
.PN ShapeBounding
and
.PN ShapeClip .
The defined region operations are
.PN ShapeSet ,
.PN ShapeUnion ,
.PN ShapeIntersect ,
.PN ShapeSubtract ,
and
.PN ShapeInvert .
.sM
.FD 0
Bool
XShapeQueryExtension(\fIdisplay\fP\^, \fIevent_base\fP\^, \fIerror_base\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     int *\fIevent_base\fP\^; /* RETURN */
.br
     int *\fIerror_base\fP\^; /* RETURN */
.FN
.LP
.eM
.PN XShapeQueryExtension
returns
.PN True
if the specified display supports the SHAPE extension else
.PN False .
If the extension is supported, *event_base is set to the event number for
.PN ShapeNotify
events and *error_base would be set to the error number for the first error for
this extension. 
Because no errors are defined for this version of the extension,
the value returned here is not defined (nor useful).
.LP
.sp
.sM
.FD 0
Status
XShapeQueryVersion(\fIdisplay\fP\^, \fImajor_version\fP\^, \fIminor_version\fP\^)
.br
     Display *\fIdisplay\fP;
.br
     int *\fImajor_version\fP, *\fIminor_version\fP\^;  /* RETURN */
.FN
.LP
.eM
If the extension is supported, 
.PN XShapeQueryVersion
sets the major and minor version numbers of the
extension supported by the display and returns a nonzero value.
Otherwise, the arguments are not set and zero is returned.
.LP
.sp
.sM
.FD 0
XShapeCombineRegion(\fIdisplay\fP\^, \fIdest\fP\^, \fIdest_kind\fP\^, \
\fIx_off\fP\^, \fIy_off\fP\^, \fIregion\fP\^, \fIop\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIdest\fP\^;
.br
      int \fIdest_kind\fP\^, \fIop\fP\^, \fIx_off\fP\^, \fIy_off\fP\^;
.br
      REGION *\fIregion\fP\^;
.FN
.LP
.eM
.PN XShapeCombineRegion
converts the specified region into a list of rectangles and calls
.PN XShapeCombineRectangles .
.LP
.sp
.sM
.FD 0
XShapeCombineRectangles(\fIdisplay\fP\^, \fIdest\fP\^, \fIdest_kind\fP\^, \
\fIx_off\fP\^, \fIy_off\fP\^, \fIrectangles\fP\^, \fIn_rects\fP\^, \fIop\fP\^, \
\fIordering\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIdest\fP\^;
.br
      int \fIdest_kind\fP\^, \fIn_rects\fP\^, \fIop\fP\^, \fIx_off\fP\^, \
\fIy_off\fP\^, \fIordering\fP\^;
.br
      XRectangle *\fIrectangles\fP\^;
.FN
.LP
.eM
If the extension is supported, 
.PN XShapeCombineRectangles
performs a 
.PN ShapeRectangles
operation; otherwise, the request is ignored.
.LP
.sp
.sM
.FD 0
XShapeCombineMask(\fIdisplay\fP\^, \fIdest\fP\^, \fIdest_kind\fP\^, \
\fIx_off\fP\^, \fIy_off\fP\^, \fIsrc\fP\^, \fIop\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIdest\fP\^;
.br
      int \fIdest_kind\fP\^, \fIop\fP\^, \fIx_off\fP\^, \fIy_off\fP\^;
.br
      Pixmap \fIsrc\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XShapeCombineMask
performs a
.PN ShapeMask
operation; otherwise, the request is ignored.
.LP
.sp
.sM
.FD 0
XShapeCombineShape(\fIdisplay\fP\^, \fIdest\fP\^, \fIdest_kind\fP\^, \
\fIx_off\fP\^, \fIy_off\fP\^, \fIsrc\fP\^, \fIsrc_kind\fP\^, \fIop\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIdest\fP\^, \fIsrc\fP\^;
.br
      int \fIdest_kind\fP\^, \fIsrc_kind\fP\^, \fIop\fP\^, \fIx_off\fP\^, \fIy_off\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XShapeCombineShape
performs a
.PN ShapeCombine
operation; otherwise, the request is ignored.
.LP
.sp
.sM
.FD 0
XShapeOffsetShape(\fIdisplay\fP\^, \fIdest\fP\^, \fIdest_kind\fP\^, \
\fIx_off\fP\^, \fIy_off\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIdest\fP\^;
.br
      int \fIdest_kind\fP\^, fIx_off\fP\^, \fIy_off\fP\^;
.FN
.LP
.eM
If the extension is supported,
.PN XShapeOffsetShape
performs a
.PN ShapeOffset
operation; otherwise, the request is ignored.
.LP
.sp
.sM
.FD 0
Status XShapeQueryExtents(\fIdisplay\fP\^, \fIwindow\fP\^, \
\fIbounding_shaped\fP\^, \fIx_bounding\fP\^, \fIy_bounding\fP\^, 
.br
	 \fIw_bounding\fP\^, \ \fIh_bounding\fP\^, \fIclip_shaped\fP\^, \
\fIx_clip\fP\^, \fIy_clip\fP\^, \fIw_clip\fP\^, \fIh_clip\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIwindow\fP\^;
.br
      Bool *\fIbounding_shaped\fP\^, *\fIclip_shaped\fP\^; /* RETURN */
.br
      int *\fIx_bounding\fP\^, *\fIy_bounding\fP\^, *\fIx_clip\fP\^, \
*\fIy_clip\fP\^; /* RETURN */
.br
      unsigned int *\fIw_bounding\fP\^, *\fIh_bounding\fP\^, *\fIw_clip\fP\^, \
* \fIh_clip\fP\^; /* RETURN */
.FN
.LP
.eM
If the extension is supported,
.PN XShapeQueryExtents
sets x_bounding, y_bounding, w_bounding, h_bounding to the extents of the
bounding shape and sets x_clip, y_clip, w_clip, h_clip to the extents of
the clip shape.  For unspecified client regions, the extents of the
corresponding default region are used.
.LP
If the extension is supported,
a nonzero value is returned; otherwise, zero is returned.
.LP
.sp
.sM
.FD 0
XShapeSelectInput(\fIdisplay\fP\^, \fIwindow\fP\^, \fImask\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     unsigned long \fImask\fP\^;
.FN
.LP
.eM
To make this extension more compatible with other interfaces, although
only one event type can be selected via the extension,
.PN XShapeSelectInput
provides a general mechanism similar to the standard Xlib binding for
window events.  A mask value has been defined,
.PN ShapeNotifyMask 
that is the only valid bit in mask that may be specified.
The structure for this event is defined as follows:
.LP
.sM
.Ds 0
.TA 1.5i
.ta 1.5i
typedef struct {
    int type;	/* of event */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came frome a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;	/* window of event */
    int kind;	/* ShapeBounding or ShapeClip */
    int x, y;	/* extents of new region */
    unsigned width, height;
    Time time;	/* server timestamp when region changed */
    Bool shaped;	/* true if the region exists */
} XShapeEvent;
.De
.LP
.eM
.LP
.sM
.FD 0
unsigned long
XShapeInputSelected(\fIdisplay\fP\^, \fIwindow\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.FN
.LP
.eM
.PN XShapeInputSelected
returns the current input mask for extension events on the specified
window; the value returned if
.PN ShapeNotify
is selected for is
.PN ShapeNotifyMask ;
otherwise, it returns zero.
If the extension is not supported, it returns zero.
.LP
.sp
.sM
.FD 0
XRectangle *
XShapeGetRectangles(\fIdisplay\fP\^, \fIwindow\fP\^, \fIkind\fP\^, \
\fIcount\fP\^, \fIordering\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIwindow\fP\^;
.br
      int \fIkind\fP\^;
.br
      int *\fIcount\fP\^; /* RETURN */
.br
      int *\fIordering\fP\^; /* RETURN */
.FN
.LP
.eM
If the extension is not supported, 
.PN XShapeGetRectangles
returns NULL.
Otherwise, it returns a list of rectangles that describe the region
specified by kind.
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
