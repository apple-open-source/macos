.\" Use -ms and macros.t
.\" $Xorg: evi.ms,v 1.3 2000/08/17 19:42:31 cpqbld Exp $
.\"
.\"
.\"
.\"
.\" $XFree86: xc/doc/specs/Xext/evi.ms,v 1.4 2001/01/17 16:57:44 dawes Exp $
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.sp 8
.ce 3


\s+2\fBExtended Visual Information Extension\fP\s-2
.sp 3
.ce 3
Version 1.0
X Project Team Standard
X Version 11, Release 6.4
.sp 6
.ce 4
Peter Daifuku
.sp 6p
Silicon Graphics, Inc.
.ps 9
.nr PS 9
.sp 8
.LP
Copyright \(co 1986-1997   The Open Group    All Rights Reserved
.LP
Permission is hereby granted, free of charge, to any person obtaining a copy of this 
software and associated documentation files (the Software), to use the Software 
without restriction, including, without limitation, the rights to copy, modify, merge, 
publish, distribute and sublicense the Software, to make, have made, license and 
distribute derivative works thereof, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:
.LP
The above copyright notice and the following permission notice shall be included in all 
copies of the Software:
.LP
THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES 
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-
INFRINGEMENT.  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY 
CLAIM, DAMAGES OR OTHER USEABILITIY, WHETHER IN AN ACTION OF 
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN 
CONNNECTION WITH THE SOFTWARE OR THE USE OF OTHER DEALINGS IN 
THE SOFTWARE.
.LP
Except as contained in this notice, the name of The Open Group shall not be used in 
advertising or otherwise to promote the use or other dealings in this Software without 
prior written authorization from The Open Group.
.LP
X Window System is a trademark of The Open Group.
.ps 10
.nr PS 10
.bp 1
.EH ''X11 Extended Visual Information extension''
.OH ''X11 Extended Visual Information extension''
.EF ''\fB % \fP''
.OF ''\fB % \fP''
.NH 1
Introduction
.LP
EVI (Extended Visual Information extension) allows a client to determine
information about core X visuals beyond what the core protocol provides.
.NH 1
Goals
.LP
As the X Window System has evolved, it has become clear that the information
returned by the core X protocol regarding Visuals is often insufficient for a
client to determine which is the most appropriate visual for its needs. This
extension allows clients to query the X server for additional visual
information, specifically as regards colormaps and framebuffer levels.
.LP
This extension is meant to address the needs of pure X clients only. It is
specifically and purposefully not designed to address the needs of X
extensions. Extensions that have an impact on visual information should provide
their own mechanisms for delivering that information. For example, the Double
Buffering Extension (DBE) provides its own mechanism for determining which
visuals support double-buffering.
.NH 1
Requests
.LP
.sM
.PN GetVersion
.IP
\fIclient_major_version\fP\^: CARD8
.br
\fIclient_minor_version\fP\^: CARD8
.LP
  =>
.IP
\fIserver_major_version\fP\^: CARD8
.br
\fIserver_minor_version\fP\^: CARD8
.LP
.eM
If supplied, the client_major_version and client_minor_version indicate
what version of the protocol the client wants the server to implement.
The server version numbers returned indicate the protocol this extension
actually supports. This might not equal the version sent by the client.
An implementation can (but need not) support more than one version
simultaneously. The server_major_version and the server_minor_version
are a mechanism to support future revisions of the EVI protocol that
may be necessary. In general, the major version would increment for
incompatible changes, and the minor version would increment for small
upward-compatible changes. Servers that support the protocol defined in
this document will return a server_major_version of one (1), and a
server_minor_version of zero (0).
.LP
.sM
.PN   GetVisualInfo
.IP
\fIvisual_list\fP\^: LISTofVISUALID
.LP
  =>
.IP
\fIper_visual_info\fP\^: LISTofVISUALINFO
.LP
where:
.TS
l l.
VISUALINFO:	[core_visual_id: VISUALID
.br
	\ screen: CARD8
.br
	\ level: INT8
.br
	\ transparency_type: CARD8
.br
	\ unused: CARD8
.br
	\ transparency_value: CARD32
.br
	\ min_hw_colormaps: CARD8
.br
	\ max_hw_colormaps: CARD8
.br
	\ num_colormap_conflicts: CARD16
.br
	\ colormap_conflicts: LISTofVISUALID]
.TE
.eM
.LP
.IP
    - level is 0 for normal planes, > 0 for overlays, < 0 for underlays.
.br
    - transparency_type is 0 for none, 1 for transparent pixel, 2 for
      transparent mask.
.br
    - transparency_value: value to get transparent pixel if transparency
    supported.
.br
    - min_hw_colormaps: minimum number of hardware colormaps backing up the
visual.
.br
    - max_hw_colormaps: maximum number of hardware colormaps backing up the
visual.
.br
    (architectures with static colormap allocation/reallocation would have min
= max)
.br
    - num_colormap_conflicts: number of elements in colormap_conflicts.
.br
    - colormap_conflicts: list of visuals that may conflict with this one. For
    example, if a 12-bit colormap is overloaded to support 8-bit visuals, the
    8-bit visuals would conflict with the 12-bit visuals.
.LP
.NH 1
Events and Errors
.LP
No new events or errors are defined by this extension.
.LP
.NH 1
Changes to existing protocol.
.LP
None.
.LP
.NH 1
Encoding
.LP
The name of this extension is "Extended-Visual-Information".
.LP
The conventions used here are the same as those for the core X11
Protocol Encoding.
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN GetVersion
.sp 6p
	1	CARD8		opcode
	1	0		EVI opcode
	2	2		request length
	2	CARD16		client_major_version
	2	CARD16		client_minor_version
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		reply
	1			unused
	2	CARD16		sequence number
	4	0		length
	2	CARD16		server_major_version
	2	CARD16		server_minor_version
	20			unused
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
.PN GetVisualInfo
.sp 6p
	1	CARD8		opcode
	1	1		EVI opcode
	2	2+n		request length
	4	CARD32		n_visual
	4n	CARD32		visual_ids
.De
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
 =>
	1	1		reply
	1			unused
	2	CARD16		sequence number
	4	n		length
	4	CARD32		n_info
	4	CARD32		n_conflicts
	16			unused
	16n	LISTofVISUALINFO	items
.De
.LP
.Ds 0
.TA .2i .5i 1.5i 2.5i
.ta .2i .5i 1.5i 2.5i
.R
  VISUALINFO
	4	VisualID		core_visual_id
	1	INT8		screen
	1	INT8		level
	1	CARD8		transparency_type
	1	CARD8		unused
	4	CARD32		transparency_value
	1	CARD8		min_hw_colormaps
	1	CARD8		max_hw_colormaps
	2	CARD16		num_colormap_conflicts
.De
.LP
.NH 1
C Language Binding
.LP
The C functions provide direct access to the protocol and add no additional
semantics.  For complete details on the effects of these functions, refer
to the appropriate protocol request, which can be derived by deleting Xevi
at the start of the function. All functions that have return type Status
will return nonzero for success and zero for failure.
.LP
The include file for this extension is:
.Pn < X11/extensions/XEVI.h >.
.LP
.sM
.FD 0
.PN Bool XeviQueryVersion(
.br
    Display *\fIdisplay\fP\^,
.br
    int *\fImajor_version_return\fP\^,
.br
    int *\fIminor_version_return\fP\^)
.FN
.LP
.eM
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fImajor_version_return\fP 1i
Returns the major version supported by the server.
.IP \fIminor_version_return\fP 1i
Returns the minor version supported by the server.
.LP
XeviQueryVersion sets major_version_return and minor_version_return to
the major and minor EVI protocol version supported by the server.  If
the EVI library is compatible with the version returned by the server,
it returns nonzero.  If dpy does not support the EVI extension, or if
there was an error during communication with the server, or if the server
and library protocol versions are incompatible, it returns zero.  No other
Xevi functions may be called before this function. If a client violates
this rule, the effects of all subsequent Xevi calls that it makes are
undefined.
.LP
To get the extended information for any subset of visuals use 
XeviGetVisualInfo. 
.LP
.sM
.FD 0
.PN int XeviGetVisualInfo(
.br
    Display *\fIdisplay\fP\^,
.br
    VisualID *\fIvisual\fP\^,
.br
    int \fIn_visual\fP\^,
.br
    ExtendedVisualInfo **\fIevi_return\fP\^,
.br
    int *\fIn_info_return\fP\^)
.FN
.LP
.eM
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIvisual\fP 1i
If NULL, then information for all visuals of all 
screens is returned. Otherwise, a pointer to a list of visuals for which
extended visual information is desired.
.IP \fIn_visual\fP 1i
If 0, then information for all visuals of all screens is returned. Otherwise,
the number of elements in the array \fIvisual\fP.
.IP \fIevi_return\fP 1i
Returns a pointer to a list of \fIExtendedVisualInfo\fP. When done, the client
should free the list using \fIXFree\fP.
.IP \fIn_info_return\fP 1i
Returns the number of elements in the list of \fIExtendedVisualInfo\fP.
.LP
XeviGetVisualInfo returns a list of ExtendedVisualInfo structures that describe
visual information beyond that supported by the core protocol. This includes 
layer information relevant for systems supporting overlays and/or underlay
planes, and information that allows applications better to determine the level
of hardware support for multiple colormaps. XeviGetVisualInfo returns Success if
successful, or an X error otherwise.

