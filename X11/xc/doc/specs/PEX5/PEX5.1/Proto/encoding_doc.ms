.\" $Xorg: encoding_doc.ms,v 1.3 2000/08/17 19:42:11 cpqbld Exp $
.\"
.\"
.\"  This macro puts the section numbers, labels, and page numbers out
.\"   to the standard output.   aps, crw, rjr.
.\"   WARNING: This macro assumes certain knowledge about the the ms/mu
.\"   packages work (what number and string registers are used, to be exact).
.\"
.de AC
.NH \\$2
\\$1
.\"
.\"  This indents section heading of level two or greater.
.\"
.tm .Bg \\n(NS
.tm \\*(SN \\$1
.tm .Ed \\n%
..

.de AP
.\"
.\"  This indents section heading of level two or greater.
.\"
.tm .Bg 1
.tm \\$1
.tm .Ed \\n%
..


.de Ao
.PN \\$1
.\"
.\"  This indents section heading of level two or greater.
.\"
.tm .Bg 3
.tm \\$1
.tm .Ed \\n%
..

.de RU
.br
\l'6.5i'
.sp
..

.de AR
.IP \\$1 1.0i
..

.de Sh		\" start a section (chapter type)
.bp
.NH 1
\fB\\$1\fP
.tm .Nh "\\$1" "\\n%" "0" "\\n(H1" "\\n(H2" "\\n(H3"	 \" zero is for chapter,group
..

.de Nh		\" Start a new section
.ds RH \\$1
.nr In 0 1
.ds Ic \\$1
.nr Ac 0 1
.LP
.NH 2
\\$1
.tm .Nh "\\$1" "\\n%" "1" "\\n(H1" "\\n(H2" "\\n(H3"   	\" one is for sub-chapter
..

.de Fs		\" Function Start
..
.de Nn		\" Start name of function
.NH 3
\\$1
..

.de Na		\" Start name of function
.sp
.LP
.in 0.5i
.ti -0.5i
\fBName:\fP 
.ti 0.5i
\fB\\$1\fP
..

.de Or		\" name of output request
.sp -1
.LP
\fB\\$1\fP
.tm .Bg 3
.tm \\$1
.tm .Ed \\n%
..

.de Op		\" add an output request parameter
.br
.RS
\fI\\$1 : \fP  \\$2
.RE
..

.de Pa		\" add a parameter
.ti 0.5i
\fI\\$1 : \fP  \\$2
..

.de Rq		\" Start Request list
.LP
.ti -0.5i
\fBRequest:\fP
..

.de Re		\" Start Reply list
.LP
.ti -0.5i
\fBReply:\fP
..

.de Se		\" Start ERRORS list
.LP
.ti -0.5i
\fBErrors:\fP
.ti 0.5i
..

.de Fe		\" End of Function
.in 0i
..

.de Bl		\" Start of bullet item
.sp -1
.IP "\fI\\$1\fP"
..
                                      
.de 2d		\" Description of 2d primtives
When processed, this command will cause \\$1 primitives to be drawn.
This primitive
functions exactly as the 3D \\$1 primitive
except that modeling
coordinate positions are specified using only \fIx-\fP and \fIy-\fP
coordinates, and the \fIz\fP-coordinate is always assumed to be zero.
..

.de Es		\" Start of enumerated type description table
.LD
.ta 0.2i 1.7i
..

.de Ee		\" End of enumerated type description table
.ta
.DE
..

.de Ds
.nf
.\\$1D \\$2 \\$1
.ft 1
.ps \\n(PS
.if \\n(VS>=40 .vs \\n(VSu
.if \\n(VS<=39 .vs \\n(VSp
..

.de De
.ce 0
.if \\n(BD .DF
.nr BD 0
.in \\n(OIu
.if \\n(TM .ls 2
.sp \\n(DDu
.fi
..

.de Pn
.ie t \\$1\fB\^\\$2\^\fR\\$3
.el \\$1\fI\^\\$2\^\fP\\$3
..

.de PN
.ie t \fB\^\\$1\^\fR\\$2
.el \fI\^\\$1\^\fP\\$2
..
.nr LL 6.5i
.nr PD 0.1i
.nr HM 1.2i
.nr FM 1.0i
.nr PO 1.0i
.nh
.ps 10
.nr PS 10
.vs 12
.nr VS 12
.ad l
.EH '\s+2\fBPEX\ Protocol\ Encoding\fP''\fBPEX\ V5.1\ 31-August-1992\fP\s-2'
.OH '\s+2\fBPEX\ V5.1\ 31-August-1992\fP''\fBPEX\ Protocol\ Encoding\fP\s-2'
.EF ''\s+2\fB % \fP\s-2''
.OF ''\s+2\fB % \fP\s-2''
.nr % 1

.AC Acknowledgements 1
.nh
.LP
To the author and first editor of this document, Todd Newman of Digital
Equipment Corp., go our
sincere thanks.  Countless iterations of protocol changes and nit-fixing
went into early versions of it and we gratefully acknowledge his care and
dedication to this thankless task.
Randi Rost and Jeff Friedberg of Digital Equipment Corp. helped in
the production of the first draft of this document by explaining
the original intent of the architecture team.
Sally C. Barry of Digital Equipment Corp., was a patient and thorough reviewer 
of early versions of this document throughout its many
revisions. 
.LP
Randi Rost of Digital Equipment Corp. took over the job of editing this
document in January of 1989, and performed this task until April of
1990.  During this time, Randi was responsible for five separate
revisions of the document.
.LP
We owe a major debt to the PEX architecture team which met frequently
between June, 1987 and August, 1991:
Jeffrey Friedberg, John McConnell, Pete Nishimoto, Randi J. Rost,
and Jeffrey S. Saltz of Digital Equipment Corporation;
Dave Gorgen, Tom Gross, and Jeff Stevenson of Hewlett-Packard Company;
David Plunkett of Solbourne Computer;
Jan Hardenbergh of Stardent Computer, Inc.;
and Marty Hess, and Jim Van Loo of Sun Microsystems.
.bp

.AC "PEX Protocol Specification" 1
.LP
.FS
.br
.sp
X Window System is a trademark of the
Massachusetts Institute of Technology

X3D-PEX and PEX are trademarks of the 
Massachusetts Institute of Technology
.FE

.AC "Request Processing" 2
.LP
Workstation regeneration and requests which process output commands
are not guaranteed to be atomic with respect to concurrent rendering
to the same destination drawable.  However atomicity of execution is
guaranteed for each individual output command primitive, excluding
Execute Structure, GSE, GDP 3D, and GDP 2D.  Also, requests and output
commands will be executed in sequential order for a given connection.

.AC "Protocol Format" 2
.LP

.AC "Request Format" 3
.LP
Requests are listed in section 2.8 and in section 4 through the end of the
document.  Output Commands are described in section 3.

.AC "Reply Format" 3
.LP
Replies are listed after the request.

.AC "Error Format" 3
.LP
See section 2.5.7 for the encoding of particular errors.

.AC "Event Format" 3
.LP
See section 2.6 for the encoding of events.

.AC "Syntax" 2
.LP
As far as possible, this document uses the layout and syntactic conventions used
in the X encoding document.  In particular, note that all numbers are decimal,
unless prefixed by #x, in which case they are hexadecimal.  Also note that, for
entities of a variable size, E, the notation pad(E) indicates the number of
bytes required to pad the entity to a multiple of 4 bytes.
.LP
There are some unavoidable differences from the X encoding document
conventions, however, because of the many variable sized entities and entities
with optional data in PEX.

.AC "Naming Conventions" 2
.LP
The names of the fields in requests, replies and lists of values match the
names used in the \fIPEX Protocol Specification\fP.

.AC "Common Types" 2
.LP
The types listed in this section are some of the types mentioned in
the PEX protocol.  Generally, derivative types (e.g. ENUM_TYPE_INDEX)
are replaced by the primitive or primitives of which they are composed
(e.g. INT16).  Only when this is impractical is the derivative type
used; generally this is when the type is of variable length or a
list of compound types is used.

.AC "LISTofFOO" 3
.LP
In this document, a LISTofFOO is a counted list of elements of type FOO,
the count field preceding it in the request or reply.
.LP
In cases where the number of items in the list is easily computed from the
length of the list, the number of items may not be supplied. (See, for
example, the \fBPolyline3D\fP output command.)  The goal is to reduce the
burden on the client. Such cases are indicated in the encoding document.
.LP
There is also no count provided for a list that has an associated
item_mask.
.LP
Unless otherwise marked, a LISTofLISTofFOO is implemented as follows:
somewhere in the request or reply there is a number of lists field (perhaps
several if there are multiple lists of lists). Each LIST is preceded by
a CARD32 count field.
.LP
So the overall format is:
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
	4	CARD32		number of lists
	. . .			(potentially other things here)
	4	CARD32		number of items in first list
	n	FOO		first item of first list
	n	FOO		second item of first list
	. . .
	n	FOO		last item of first list
	4	CARD32		number of items in second list
	n	FOO		first item of second list
	. . .
.De
.LP
In only one place in the PEX protocol is a LISTofLISTofLISTofFOO
required, in the \fBSetOfFillAreaSets\fP output command.  In this case, 
the number of lists is a CARD16.  Other counts within the list are
CARD16.
.LP
The format of this is:
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
	2	CARD16		number of lists of lists
	. . .			(potentially other things here)
	2	CARD16		number of lists in first list of list
	2	CARD16		number of items in first list
	n	FOO		first item of first list
	n	FOO		second item of first list
	. . .
	n	FOO		last item of first list
	2	CARD16		number of items in second list
	n	FOO		first item of second list
	. . .
	2	CARD16		number of lists in second list of list
	2	CARD16		number of items in first list
	n	FOO		first item of first list
	n	FOO		second item of first list
	. . .
	n	FOO		last item of first list
	2	CARD16		number of items in second list
	n	FOO		first item of second list
	. . .
	2	CARD16		number of lists in last list of list
	2	CARD16		number of items in first list
	n	FOO		first item of first list
	n	FOO		second item of first list
	. . .
	n	FOO		last item of first list
	2	CARD16		number of items in second list
	n	FOO		first item of second list
	. . .
.De

.AC "BITMASK and LISTofVALUE" 3
.LP
Because the values in a value list are of variable size, a list of values
can only be interpreted serially; there is no way to find the Nth element
without examining the N-1 previous elements.
.LP
Elements have all been padded to a multiple of 4 bytes.  If values appear
which could be encoded in less space, it is assumed that they will be
placed in the least significant bits of the word.  (Byte-swapping may occur
between the client and the server.)

.AC "Floating Point Format - FLOAT" 3
.LP
A FLOAT is a floating point quantity in one of the server supported formats.  If
the client wishes to communicate with the server in other than the server's
native floating point format, the server must perform the necessary
conversions.  The format word of a request tells the server what format the
client wishes to use.
.LP
Depending on the floating point format, the size of a FLOAT may be 4 or
8 bytes.  Floating point values passed to the server may be in any one
of the registered floating point formats:
.TA .2i .6i 1.6i 3.1i
.ta .2i .6i 1.6i 3.1i
.nf
		1	IEEE_754_32	4 bytes
		2	DEC_F_Floating	4 bytes
		3	IEEE_754_64	8 bytes
		4	DEC_D_Floating	8 bytes
.fi
Equations are provided for computing the length of the requests that
contain floating point values, however these equations assume that one of the
four registered floating point formats is being used.
In all of these equations, the name \fIfp\fP refers to the size in bytes
of a FLOAT in the floating point format indicated in the format word of
the request.

.AC "Colors" 3
.LP
Colors passed to the server may be of any one of the registered
color types:
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.nf
		0	Indexed
		1	RGBFloat
		2	CIEFloat
		3	HSVFloat
		4	HLSFloat
		5	RGBInt8
		6	RGBInt16
.fi
Equations are provided for computing the length of the requests that
contain colors, however these equations assume that one of the seven
registered color types and one of the four registered floating point
formats is being used.

.AC "Element Types" 3
.LP

.AC "Types" 3
.LP
Some PEX requests and replies contain matrices.  These are denoted in this
document as FLOAT[4][4] or FLOAT[3][3].  The elements of
the matrices are sent over the wire in a row-wise manner.  That is, the element
of a 4\(mu4 transformation matrix that indicates the
translation in the X direction is the fourth element sent.
.LP
The following compound types appear in the rest of the encoding document.
For a complete list of types refer to the protocol document.
If a variable is given which computes the length of a compound type (e.g.,
the equation for d in OPT_DATA), that variable may be used later to express
the length of a request or reply (e.g. the \fBTriangleStrip\fP output
command).
.sp 1
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
CHARACTER
	1	CARD8		byte character
			or
	2	CARD16		short character
			or
	4	CARD32		long character
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
COLOR
	n = ((color type == Indexed)||(color type == RGBInt8) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4)
	(color type is obtained from elsewhere in the request or output command)
	4n			color value
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
COLOR_SPECIFIER
	n = ((color type == Indexed)||(color type == RGBInt8) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4)
	2			color type
	2			unused
	4n			color value
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
COORD_2D
	\fIfp\fP	FLOAT		x
	\fIfp\fP	FLOAT		y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
COORD_3D
	\fIfp\fP	FLOAT		x
	\fIfp\fP	FLOAT		y
	\fIfp\fP	FLOAT		z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
COORD_4D
	\fIfp\fP	FLOAT		x
	\fIfp\fP	FLOAT		y
	\fIfp\fP	FLOAT		z
	\fIfp\fP	FLOAT		w
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
DC_HIT_BOX_DATA
	2	INT16		x
	2	INT16		y
	\fIfp\fP	FLOAT		pick distance	
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
DEVICE_COORD
	2	INT16		x
	2	INT16		y
	\fIfp\fP	FLOAT		z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
DEVICE_RECT
	2	INT16		xmin
	2	INT16		ymin
	2	INT16		xmax
	2	INT16		ymax
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
DIRECT_COLOR
	n = ((color_type == RGBInt8) ? 1 : ((color_type == RGBInt16) ? 2 : 3*\fIfp\fP/4)
	4n			direct color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
ELEMENT_REF
	4	CARD32		structure ID
	4	CARD32		offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
ENUM_DESC
	2	INT16		index
	2	n		string length 
	n	LISTofCARD8		string	
	p			unused, p=pad(n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
EXTENT_INFO
	\fIfp\fP	FLOAT		lower left x
	\fIfp\fP	FLOAT		lower left y
	\fIfp\fP	FLOAT		upper right x
	\fIfp\fP	FLOAT		upper right y
	\fIfp\fP	FLOAT		concatpoint x
	\fIfp\fP	FLOAT		concatpoint y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
FACET
	(maximum size, some fields may be missing, see below)
	df	OPT_DATA		facet data (c and n may be set, but s is never set)
	4	nv		number of vertices in list
	nv(3*\fIfp\fP+dv) LISTofVERTEX		vertices
.sp 1
	(let f be the size of a FACET in bytes.
	let df be the size of the per facet OPT_DATA.
	let dv be the size of the per vertex OPT_DATA.
	fc is true if facet attributes specifies a color is present.
	fn is true if facet attributes specifies a normal is present.
	let sc be the size of a direct color in bytes.
	sc = (color type == RGBInt8) ? 4 : ((color type == (RGBInt16) ? 8 : 3*\fIfp\fP)
	fi is true if facet color is indexed, false if color is direct. Then
	df = (fc ? (fi ? 4 : sc) : 0 ) + (fn ? 3*\fIfp\fP : 0)
	vc is true if vertex attributes specifies a color is present.
	vn is true if vertex attributes specifies a normal is present.
	ve is true if vertex attributes specifies an edge flag is present.
	vi is true if vertex color is indexed, false if color is direct. Then
	dv = (vc ? (vi ? 4 : sc) : 0 ) + (vn ? 3*\fIfp\fP : 0) + (ve ? 4 : 0)
	so f = 4 + df + nv * (3*\fIfp\fP + dv)
	)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
FONTINFO
	4	CARD32		first glyph
	4	CARD32		last glyph
	4	CARD32		default glyph
	1			all glyphs exist
		0	False
		1	True
	1			stroke font
		0	False
		1	True
	2			unused
	4	n		number of properties
	8n	LISTofFONTPROP		properties
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
FONTPROP
	4	CARD32		name
	4	CARD32		value
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
HALF_SPACE
	\fIfp\fP	FLOAT		point x
	\fIfp\fP	FLOAT		point y
	\fIfp\fP	FLOAT		point z
	\fIfp\fP	FLOAT		vector x
	\fIfp\fP	FLOAT		vector y
	\fIfp\fP	FLOAT		vector z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
HALF_SPACE_2D
	\fIfp\fP	FLOAT		point x
	\fIfp\fP	FLOAT		point y
	\fIfp\fP	FLOAT		vector x
	\fIfp\fP	FLOAT		vector y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
MONO_ENCODING
	2	CARD16		character set
	1			character set width (call this value w)
		0	csByte
		1	csShort
		2	csLong
	1	CARD8		encoding state
	2			unused
	2	c		number of characters
	s	LISTofCHARACTER		string
	p			unused, p=pad(s)
.sp 1
	(let m be the size of the mono encoding.
	let s be the size of the string in bytes.
	s = (w == 0) ? c : ((w == 1) ? 2c : 4c)
	m = 8 + s + p
	)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
NAME_SET_PAIR
	4	CARD32		inclusion nameset ID
	4	CARD32		exclusion nameset ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
NPC_SUBVOLUME
	\fIfp\fP	FLOAT		min x
	\fIfp\fP	FLOAT		min y
	\fIfp\fP	FLOAT		min z
	\fIfp\fP	FLOAT		max x
	\fIfp\fP	FLOAT		max y
	\fIfp\fP	FLOAT		max z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
OPT_DATA
	(maximum size, some fields may be missing, see below)
	sc = ((color type == Indexed)||(color type == RGBInt8)) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4)
	4sc			color
	\fIfp\fP	FLOAT		normal x
	\fIfp\fP	FLOAT		normal y
	\fIfp\fP	FLOAT		normal z
	4	CARD32		edges
.sp 1
	(let d be the size of an OPT_DATA in bytes.
	c is true if control word specifies a color is present.
	n is true if control word specifies a normal is present.
	s is true if control word specifies an edge switch is present.
	i is true if color is indexed, false if color is direct.
	f is 1 if color type is RGBInt8
		2 if color type is RGBInt16
		3 otherwise.
	Then d = (c ? ((i || (f == 1)) ? 4 : ((f == 2) ? 8 : 3*\fIfp\fP)) : 0) + (n ? 3*\fIfp\fP : 0) + (s ? 4 : 0)
	)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
PICK_ELEMENT_REF
	4	CARD32		structure ID
	4	CARD32		offset
	4	CARD32		pick identifier
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
PICK_RECORD
	n = ((pick type == NPC_HitVolume) ? (6*\fIfp\fP/4) : (1+\fIfp\fP/4) )
	2	INT16		pick type
	2			unused
	4n			hit box
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
RENDERER_TARGET
	1	CARD8		depth
	1			unused
	2			type
		0	don't care
		1	window
		2	pixmap
		3	buffer
	4	CARD32		visual ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
STRING
	2	n		length
	n	LISTofCARD8		string
	p			unused, p=pad(n+2)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
STRUCTURE_INFO
	4	CARD32		structure ID
	\fIfp\fP	FLOAT		priority
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
TRIM_CURVE
	1			visibility
		0	Off
		1	On
	1			unused
	2	CARD16		order
	2			type
		0	Rational
		1	NonRational	(t = (type == Rational) ? 3 : 2)
	2	INT16		approximation method
	\fIfp\fP	FLOAT		approximation tolerance
	\fIfp\fP	FLOAT		tmin
	\fIfp\fP	FLOAT		tmax
	4	k		number of knots
	4	p		number of points
	\fIfp\fP*k	LISTofFLOAT		knots
	\fIfp\fP*p*t	LISTofCOORD		points (COORD_3D or COORD_2D depending on t)

	(let c be the size of a TRIM_CURVE in bytes.)
	(c = 16 + \fIfp\fP * (3 + k + p*t))
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
VERTEX
	\fIfp\fP	FLOAT		point x
	\fIfp\fP	FLOAT		point y
	\fIfp\fP	FLOAT		point z
	d	OPT_DATA		data (see OPT_DATA for d)

	(let v be the size of a VERTEX in bytes.)
	(v = 3*\fIfp\fP + d)
.De

.AC "Errors" 3
.LP
\fIBEC\fR is the base error code for the extension, as returned by
\fBQueryExtension\fP.
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN ColorType
	1	0	Error
	1	\fIBEC\fR+0	code
	2	CARD16	sequence number
	4	CARD32	bad color type
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN RendererState
	1	0	Error
	1	\fIBEC\fR+1	code
	2	CARD16	sequence number
	4	CARD32	renderer ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN FloatingPointFormat
	1	0	Error
	1	\fIBEC\fR+2	code
	2	CARD16	sequence number
	4	CARD32	bad format
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN Label
	1	0	Error
	1	\fIBEC\fR+3	code
	2	CARD16	sequence number
	4	CARD32	bad label
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN LookupTable
	1	0	Error
	1	\fIBEC\fR+4	code
	2	CARD16	sequence number
	4	CARD32	bad table ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN Nameset
	1	0	Error
	1	\fIBEC\fR+5	code
	2	CARD16	sequence number
	4	CARD32	bad nameset ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN Path
	1	0	Error
	1	\fIBEC\fR+6	code
	2	CARD16	sequence number
	4	CARD32	index of bad path element
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN PEXFont
	1	0	Error
	1	\fIBEC\fR+7	code
	2	CARD16	sequence number
	4	CARD32	bad font ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN PhigsWKS
	1	0	Error
	1	\fIBEC\fR+8	code
	2	CARD16	sequence number
	4	CARD32	bad PHIGS workstation ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN PickMeasure
	1	0	Error
	1	\fIBEC\fR+9	code
	2	CARD16	sequence number
	4	CARD32	bad device ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN PipelineContext
	1	0	Error
	1	\fIBEC\fR+10	code
	2	CARD16	sequence number
	4	CARD32	bad pipeline context ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN Renderer
	1	0	Error
	1	\fIBEC\fR+11	code
	2	CARD16	sequence number
	4	CARD32	bad renderer ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN SearchContext
	1	0	Error
	1	\fIBEC\fR+12	code
	2	CARD16	sequence number
	4	CARD32	bad search context ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN Structure
	1	0	Error
	1	\fIBEC\fR+13	code
	2	CARD16	sequence number
	4	CARD32	bad structure ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	21		unused
.De
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN OutputCommand
	1	0	Error
	1	\fIBEC\fR+14	code
	2	CARD16	sequence number
	4	CARD32	renderer ID or structure ID
	2	CARD16	minor opcode
	1	CARD8	major opcode
	1	CARD8	unused
	2	CARD16	opcode of output command
	2	CARD16	number of output commands in the request that were processed successfully
	16		unused
.De

.AC "Resource IDs" 3
.LP
Some resources have attributes that contain IDs of other resources.  It
is possible for the referenced resource to be freed while the referencing
resource remains in use.  Attempts to query attributes containing the resource
ID of a resource that has already been freed will return the value 
\fIAlreadyFreed\fP (whose value equals one).

.AC "Events" 2
.LP
\fIBEC\fR is the base event code for the extension, as returned by
\fBQueryExtension\fP.
.LP
.Ds 0
.TA .2i .5i 1.6i
.ta .2i .5i 1.6i
.R
.PN MaxHitsReached
	1	\fIBEC\fR+0	code
	1		unused
	2	CARD16	sequence number
	4	CARD32	renderer id
	24		unused
.De
.LP

.AC "Padding" 2
.LP
The contents of pad bytes is explicitly left undefined.
In some cases, the length of a field is followed by its type
which is shorter than the stated length of the field (e.g., "distinguish 4
CARD8").  In these cases, the value is placed in the least significant
bits of the word and the entire word is byte swapped.

.AC "Extension Information" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetExtensionInfo
	1	CARD8		opcode (X11 assigned)
	1	1		PEX opcode
	2	2		request length
	2	CARD16		client protocol major version
	2	CARD16		client protocol minor version
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1 			unused
	2	CARD16		sequence number
	4	(n+p)/4		reply length
	2	CARD16		protocol major version
	2	CARD16		protocol minor version
	4	CARD32		release number
	4	n		length of vendor name
	4	CARD32		subset info
	8			unused
	n	LISTofCARD8		vendor
	p			unused, p=pad(n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetEnumeratedTypeInfo
	1	CARD8		opcode (X11 assigned)
	1	2		PEX opcode
	2	4+(2n+p)/4		request length
	4	CARD32		drawable ID
	4			item mask
		#x00000001	index
		#x00000002	mnemonic
	4	n		number of enumerated types
	2n	LISTofCARD16		enum types
		1	MarkerType
		2	ATextStyle
		3	InteriorStyle
		4	HatchStyle
		5	LineType
		6	SurfaceEdgeType
		7	PickDeviceType
		8	PolylineInterpMethod
		9	CurveApproxMethod
		10	ReflectionModel
		11	SurfaceInterpMethod
		12	SurfaceApproxMethod
		13	ModelClipOperator
		14	LightType
		15	ColorType
		16	FloatFormat
		17	HLHSRMode
		18	PromptEchoType
		19	DisplayUpdateMode
		20	ColorApproxType
		21	ColorApproxModel
		22	GDP
		23	GDP3
		24	GSE
		25	TrimCurveApproxMethod
		26	RenderingColorModel
		27	ParametricSurfaceCharacteristics
		28	Escape
		29	PickOneMethod
		30	PickAllMethod
	p		unused, p=pad(2n)
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	m/4		reply length
	4	n		number of lists
	20			unused
	m	LISTofENUM_DATA		types
.De
.LP
The contents of ENUM_DATA depends on \fIitem_mask\fP.
If \fIitem_mask\fP = 0 then only the counts are returned
and ENUM_DATA is a CARD32.
If \fIitem_mask\fP = 1 then only the indices are returned
and ENUM_DATA is a LISTofINT16.
If \fIitem_mask\fP = 2 then only the mnemonics are returned
and ENUM_DATA is a LISTofSTRING.
If \fIitem_mask\fP = 3 then the indices and mnemonics are
returned and ENUM_DATA is a LISTofENUM_DESC.
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetImpDepConstants
	1	CARD8		opcode (X11 assigned)
	1	3		PEX opcode
	2	4+(2n+p)/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		example drawable ID
	4	n		number of names
	2n	LISTofCONSTANT_NAME		names
	p			unused, p=pad(2n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n		reply length
	24			unused
	4n	LISTofVALUE		constants (length implicit)
.De
Names for \fBPEXGetImpDepConstants\fP:
All names are 16 bit constants, returns CARD32, except for the
names \fIChromaticityRedU, ChromaticityRedV, LuminanceRed,
ChromaticityGreenU, ChromaticityGreenV, Luminance-Green,
ChromaticityBlueU, ChromaticityBlueV, LuminanceBlue,
ChromaticityWhiteU, ChromaticityWhiteV,\fP and
\fILuminanceWhite\fP, each of which returns a FLOAT.
.sp 1
.Ds 0
.TA .2i .6i 1.6i 2.1i
.ta .2i .6i 1.6i 2.1i
.R
	Name			Value
.sp 1
	DitheringSupported			1
		0	False
		1	True
	MaxEdgeWidth			2
	MaxLineWidth			3
	MaxMarkerSize			4
	MaxModelClipPlanes			5
	MaxNameSetNames			6
	MaxNonAmbientLights			7
	MaxNURBOrder			8
	MaxTrimCurveOrder			9
	MinEdgeWidth			10
	MinLineWidth			11
	MinMarkerSize			12
	NominalEdgeWidth			13
	NominalLineWidth			14
	NominalMarkerSize			15
	NumSupportedEdgeWidths			16
	NumSupportedLineWidths			17
	NumSupportedMarkerSizes			18
	BestColorApproxValues			19
		0	ColorApproxAnyValues
		1	ColorApproxPowersOf2
	TransparencySupported			20
		0	False
		1	True
	DoubleBufferingSupported		21
		0	False
		1	True
	ChromaticityRedU			22
	ChromaticityRedV			23
	LuminanceRed			24
	ChromaticityGreenU			25
	ChromaticityGreenV			26
	LuminanceGreen			27
	ChromaticityBlueU			28
	ChromaticityBlueV			29
	LuminanceBlue			30
	ChromaticityWhiteU			31
	ChromaticityWhiteV			32
	LuminanceWhite			33
	MaxHitsEventSupported			34
		0	False
		1	True
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXMatchRendererTargets
	1	CARD8		opcode (X11 assigned)
	1	94		PEX opcode
	2	5		request length
	4	CARD32		drawable ID
	1	CARD8		depth
	1			unused
	2			type
			0 don't care
			1 window
			2 pixmap
			3 buffer
	4	CARD32		visual ID
	4	CARD32		max targets
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1 			unused
	2	CARD16		sequence number
	4	2n		reply length
	24			unused
	8n	LISTofRENDERER_TARGET	targets	(length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEscape
	1	CARD8		opcode (X11 assigned)
	1	95		PEX opcode
	2	2+n		request length
	4	CARD32		escape ID
	4n			escape data
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEscapeWithReply
	1	CARD8		opcode (X11 assigned)
	1	96		PEX opcode
	2	2+n		request length
	4	CARD32		escape ID
	4n			escape data
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1 			unused
	2	CARD16		sequence number
	4	n		reply length (n can be zero)
	4	CARD32		escape ID
	20			escape reply data
	n 			escape reply data
.De
.LP
See the appendix for the encoding of registered escapes.
.LP
.bp

.AC "Output Commands" 1
.LP
Output command numbers start at 1.  Output command number 0 is reserved
to mean \fIAll\fP for element searching.  Negative output command numbers
are implemenation dependent.

.AC "Data Formats" 2
.LP

.AC "Output Command Descriptions" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao MarkerType 3
	2	1		output command number
	2	2		output command length
	2	INT16		marker type
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao MarkerScale 3
	2	2		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		scale
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao MarkerColorIndex 3
	2	3		output command number
	2	2		output command length
	2	CARD16		indexed color
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao MarkerColor 3
	2	4		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao MarkerBundleIndex 3
	2	5		output command number
	2	2		output command length
	2	CARD16		marker bundle index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextFontIndex 3
	2	6		output command number
	2	2		output command length
	2	CARD16		font index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextPrecision 3
	2	7		output command number
	2	2		output command length
	2			precision
		0	StringPrecision
		1	CharPrecision
		2	StrokePrecision
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CharacterExpansion 3
	2	8		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		expansion
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CharacterSpacing 3
	2	9		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		spacing
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextColorIndex 3
	2	10		output command number
	2	2		output command length
	2	CARD16		color index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextColor 3
	2	11		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CharacterHeight 3
	2	12		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		height
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CharacterUpVector 3
	2	13		output command number
	2	1+\fIfp\fP/2		output command length
	\fIfp\fP	FLOAT		up x
	\fIfp\fP	FLOAT		up y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextPath 3
	2	14		output command number
	2	2		output command length
	2			path
		0	PathRight
		1	PathLeft
		2	PathUp
		3	PathDown
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextAlignment 3
	2	15		output command number
	2	2		output command length
	2			alignment vertical
		0	ValignNormal
		1	ValignTop
		2	ValignCap
		3	ValignHalf
		4	ValignBase
		5	ValignBottom
	2			alignment horizontal
		0	HalignNormal
		1	HalignLeft
		2	HalignCenter
		3	HalignRight
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationTextHeight 3
	2	16		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		height
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationTextUpVector 3
	2	17		output command number
	2	1+\fIfp\fP/2		output command length
	\fIfp\fP	FLOAT		up x
	\fIfp\fP	FLOAT		up y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationTextPath 3
	2	18		output command number
	2	2		output command length
	2			path
		0	PathRight
		1	PathLeft
		2	PathUp
		3	PathDown
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationTextAlignment 3
	2	19		output command number
	2	2		output command length
	2			alignment vertical
		0	ValignNormal
		1	ValignTop
		2	ValignCap
		3	ValignHalf
		4	ValignBase
		5	ValignBottom
	2			alignment horizontal
		0	HalignNormal
		1	HalignLeft
		2	HalignCenter
		3	HalignRight
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationTextStyle 3
	2	20		output command number
	2	2		output command length
	2	INT16		annotation text style
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TextBundleIndex 3
	2	21		output command number
	2	2		output command length
	2	CARD16		text bundle index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LineType 3
	2	22		output command number
	2	2		output command length
	2	INT16		line type
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LineWidth 3
	2	23		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		width
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LineColorIndex 3
	2	24		output command number
	2	2		output command length
	2	CARD16		color index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LineColor 3
	2	25		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CurveApproximation 3
	2	26		output command number
	2	2+\fIfp\fP/4		output command length
	2	INT16		approximation method
	2			unused
	\fIfp\fP	FLOAT		approximation tolerance
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PolylineInterpolationMethod 3
	2	27		output command number
	2	2		output command length
	2	INT16		polyline interpolation method
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LineBundleIndex 3
	2	28		output command number
	2	2		output command length
	2	CARD16		line bundle index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceInteriorStyle 3
	2	29		output command number
	2	2		output command length
	2	INT16		interior style
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceInteriorStyleIndex 3
	2	30		output command number
	2	2		output command length
	2	CARD16 | INT16		table index or interior style
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceColorIndex 3
	2	31		output command number
	2	2		output command length
	2	CARD16		color index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceColor 3
	2	32		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceReflectionAttributes 3
	2	33		output command number
	2	2+5*\fIfp\fP/4+n		output command length
	\fIfp\fP	FLOAT		ambient  coefficient
	\fIfp\fP	FLOAT		diffuse coefficient
	\fIfp\fP	FLOAT		specular coefficient
	\fIfp\fP	FLOAT		specular concentration
	\fIfp\fP	FLOAT		transmission coefficient
	4+4*n	COLOR_SPECIFIER	specular color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceReflectionModel 3
	2	34		output command number
	2	2		output command length
	2	INT16		reflection model
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceInterpolationMethod 3
	2	35		output command number
	2	2		output command length
	2	INT16		surface interpolation method
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceInteriorStyle 3
	2	36		output command number
	2	2		output command length
	2	INT16		interior style
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceInteriorStyleIndex 3
	2	37		output command number
	2	2		output command length
	2	CARD16 | INT16		table index or interior style
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceColorIndex 3
	2	38		output command number
	2	2		output command length
	2	CARD16		color index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceColor 3
	2	39		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceReflectionAttributes 3
	2	40		output command number
	2	2+5*\fIfp\fP/4+n		output command length
	\fIfp\fP	FLOAT		ambient coefficient
	\fIfp\fP	FLOAT		diffuse coefficient
	\fIfp\fP	FLOAT		specular coefficient
	\fIfp\fP	FLOAT		specular concentration
	\fIfp\fP	FLOAT		transmission coefficient
	4+4*n	COLOR_SPECIFIER		specular color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceReflectionModel 3
	2	41		output command number
	2	2		output command length
	2	INT16		reflection model
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao BackfaceSurfaceInterpolationMethod 3
	2	42		output command number
	2	2		output command length
	2	INT16		surface interpolation method
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceApproximation 3
	2	43		output command number
	2	2+\fIfp\fP/2		output command length
	2	INT16		surface approximation method
	2			unused
	\fIfp\fP	FLOAT		approximation u tolerance
	\fIfp\fP	FLOAT		approximation v tolerance
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FacetCullingMode 3
	2	44		output command number
	2	2		output command length
	2			culling mode
		0	None
		1	BackFaces
		2	FrontFaces
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FacetDistinguishFlag 3
	2	45		output command number
	2	2		output command length
	1			distinguish
		0	False
		1	True
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PatternSize 3
	2	46		output command number
	2	1+\fIfp\fP/2		output command length
	\fIfp\fP	FLOAT		size x
	\fIfp\fP	FLOAT		size y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PatternReferencePoint 3
	2	47		output command number
	2	1+\fIfp\fP/2		output command length
	\fIfp\fP	FLOAT		point x
	\fIfp\fP	FLOAT		point y
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PatternReferencePointAndVectors 3
	2	48		output command number
	2	1+9*\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		reference point x
	\fIfp\fP	FLOAT		reference point y
	\fIfp\fP	FLOAT		reference point z
	\fIfp\fP	FLOAT		vector1 x
	\fIfp\fP	FLOAT		vector1 y
	\fIfp\fP	FLOAT		vector1 z
	\fIfp\fP	FLOAT		vector2 x
	\fIfp\fP	FLOAT		vector2 y
	\fIfp\fP	FLOAT		vector2 z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao InteriorBundleIndex 3
	2	49		output command number
	2	2		output command length
	2	CARD16		interior bundle index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceEdgeFlag 3
	2	50		output command number
	2	2		output command length
	1			onoff
		0	Off
		1	On
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceEdgeType 3
	2	51		output command number
	2	2		output command length
	2	INT16		edge type
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceEdgeWidth 3
	2	52		output command number
	2	1+\fIfp\fP/4		output command length
	\fIfp\fP	FLOAT		width
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceEdgeColorIndex 3
	2	53		output command number
	2	2		output command length
	2	CARD16		color index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SurfaceEdgeColor 3
	2	54		output command number
	2	2+n		output command length
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao EdgeBundleIndex 3
	2	55		output command number
	2	2		output command length
	2	CARD16		edge bundle index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SetIndividualASF 3
	2	56		output command number
	2	3		output command length
	4			ASF attribute to be modified
		#x00000001	MarkerTypeASF
		#x00000002	MarkerScaleASF
		#x00000004	MarkerColorASF
		#x00000008	TextFontIndexASF
		#x00000010	TextPrecASF
		#x00000020	CharExpansionASF
		#x00000040	CharSpacingASF
		#x00000080	TextColorASF
		#x00000100	LineTypeASF
		#x00000200	LineWidthASF
		#x00000400	LineColorASF
		#x00000800	CurveApproxASF
		#x00001000	PolylineInterpASF
		#x00002000	InteriorStyleASF
		#x00004000	InteriorStyleIndexASF
		#x00008000	SurfaceColorASF
		#x00010000	SurfaceInterpASF
		#x00020000	ReflectionModelASF
		#x00040000	ReflectionAttrASF
		#x00080000	BfInteriorStyleASF
		#x00100000	BfInteriorStyleIndexASF
		#x00200000	BfSurfaceColorASF
		#x00400000	BfSurfaceInterpASF
		#x00800000	BfReflectionModelASF
		#x01000000	BfReflectionAttrASF
		#x02000000	SurfaceApproxASF
		#x04000000	SurfaceEdgesASF
		#x08000000	SurfaceEdgeTypeASF
		#x10000000	SurfaceEdgeWidthASF
		#x20000000	SurfaceEdgeColorASF
	1			new ASF value
		0	Bundled
		1	Individual
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LocalTransform3D 3
	2	57		output command number
	2	2+4*\fIfp\fP		output command length
	2			composition type
		0	PreConcatenate
		1	PostConcatenate
		2	Replace
	2			unused
	16*\fIfp\fP	FLOAT[4][4]		matrix
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LocalTransform2D 3
	2	58		output command number
	2	2+9*\fIfp\fP/4		output command length
	2			composition type
		0	PreConcatenate
		1	PostConcatenate
		2	Replace
	2			unused
	9*\fIfp\fP	FLOAT[3][3]		matrix
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao GlobalTransform3D 3
	2	59		output command number
	2	1+4*\fIfp\fP		output command length
	16*\fIfp\fP	FLOAT[4][4]		matrix
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao GlobalTransform2D 3
	2	60		output command number
	2	1+9*\fIfp\fP/4		output command length
	9*\fIfp\fP	FLOAT[3][3]		matrix
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ModelClip 3
	2	61		output command number
	2	2		output command length
	1			clip switch
		0	Clip
		1	NoClip
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SetModelClipVolume3D 3
	2	62		output command number
	2	2+(3*\fIfp\fP*n)/2		output command length
	2	INT16		operator
	2	n		number of halfspaces
	6*\fIfp\fP*n	LISTofHALF_SPACE		halfspaces
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SetModelClipVolume2D 3
	2	63		output command number
	2	2+\fIfp\fP*n		output command length
	2	INT16		operator
	2	n		number of halfspaces
	4*\fIfp\fP*n	LISTofHALF_SPACE_2D		halfspaces
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao RestoreModelClipVolume 3
	2	64		output command number
	2	1		output command length
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ViewIndex 3
	2	65		output command number
	2	2		output command length
	2	CARD16		view index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao LightSourceState 3
	2	66		output command number
	2	2+(2e+2d+p1+p2)/4	output command length
	2	CARD16		number of lights to enable
	2	CARD16		number of lights to disable
	2e	LISTofCARD16		lights to be enabled
	p1			unused, p1=pad(2e)
	2d	LISTofCARD16		lights to be disabled
	p2			unused, p2=pad(2d)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao DepthCueIndex 3
	2	67		output command number
	2	2		output command length
	2	CARD16		depth cue index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PickID 3
	2	68		output command number
	2	2		output command length
	4	CARD32		pick identifier
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao HLHSRIdentifier 3
	2	69		output command number
	2	2		output command length
	4	CARD32		HLHSR identifier
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ColorApproximationIndex 3
	2	70		output command number
	2	2		output command length
	2	CARD16		color approximation index
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao RenderingColorModel 3
	2	71		output command number
	2	2		output command length
	2	INT16		rendering color model
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ParametricSurfaceCharacteristics 3
	2	72		output command number
	2	2+(n+p)/4		output command length
	2	INT16		characteristics
		1	None
		2	implementation dependent
		3	IsoparametricCurves
		4	MC_LevelCurves
		5	WC_LevelCurves
	2	n		length of data, bytes
	n	LISTofCARD8		data
	p			unused, p=pad(n)
.De
.sp 1
The data records for the registered parametric surface characteristics
are:
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Type 1, None:
	No data.
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Type 2, implementation dependent:
	No data.
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Type 3, IsoparametricCurves:
	2	CARD16		placement type
		0	Uniform
		1	NonUniform
	2			unused
	2	CARD16		number of curves in u direction
	2	CARD16		number of curves in v direction
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Type 4, MC_LevelCurves:
	\fIfp\fP	FLOAT		MC origin x
	\fIfp\fP	FLOAT		MC origin y
	\fIfp\fP	FLOAT		MC origin z
	\fIfp\fP	FLOAT		MC direction vector x
	\fIfp\fP	FLOAT		MC direction vector y
	\fIfp\fP	FLOAT		MC direction vector z
	2	t		number of intersections
	2			unused
	t*\fIfp\fP	FLOAT		intersection parameters
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Type 5, WC_LevelCurves:
	\fIfp\fP	FLOAT		WC origin x
	\fIfp\fP	FLOAT		WC origin y
	\fIfp\fP	FLOAT		WC origin z
	\fIfp\fP	FLOAT		WC direction vector x
	\fIfp\fP	FLOAT		WC direction vector y
	\fIfp\fP	FLOAT		WC direction vector z
	2	t		number of intersections
	2			unused
	t*\fIfp\fP	FLOAT		intersection parameters
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AddNamesToNameSet 3
	2	73		output command number
	2	1+n		output command length
	4n	LISTofCARD32		list of names (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao RemoveNamesFromNameSet 3
	2	74		output command number
	2	1+n		output command length
	4n	LISTofCARD32		list of names (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ExecuteStructure 3
	2	75		output command number
	2	2		output command length
	4	CARD32		structure ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Label 3
	2	76		output command number
	2	2		output command length
	4	INT32		label
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ApplicationData 3
	2	77		output command number
	2	2+(n+p)/4		output command length
	2	n		length of data
	2			unused
	n	LISTofCARD8		data
	p			unused, p=pad(n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao GSE 3
	2	78		output command number
	2	3+(n+p)/4		output command length
	4	INT32		GSE identifier
	2	n		length of data
	2			unused
	n	LISTofCARD8		data
	p			unused, p=pad(n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Marker3D 3
	2	79		output command number
	2	1+3*\fIfp\fP*n/4		output command length
	3*\fIfp\fP*n	LISTofCOORD_3D		points (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Marker2D 3
	2	80		output command number
	2	1+\fIfp\fP*n/2		output command length
	2*\fIfp\fP*n	LISTofCOORD_2D		points (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Text3D 3
	2	81		output command number
	2	2+s+(9*\fIfp\fP+n)/4		output command length
	\fIfp\fP	FLOAT		origin x
	\fIfp\fP	FLOAT		origin y
	\fIfp\fP	FLOAT		origin z
	\fIfp\fP	FLOAT		vector1 x
	\fIfp\fP	FLOAT		vector1 y
	\fIfp\fP	FLOAT		vector1 z
	\fIfp\fP	FLOAT		vector2 x
	\fIfp\fP	FLOAT		vector2 y
	\fIfp\fP	FLOAT		vector2 z
	2	s		number of mono encodings
	2			unused
				(let n be the sum of the sizes, m, of the MONO_ENCODINGs)
	4s+n	LISTofMONO_ENCODING	string (see MONO_ENCODING for m)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Text2D 3
	2	82		output command number
	2	2+s+(2*\fIfp\fP+n)/4		output command length
	\fIfp\fP	FLOAT		origin x
	\fIfp\fP	FLOAT		origin y
	2	s		number of mono encodings
	2			unused
				(let n be the sum of the sizes, m, of the MONO_ENCODINGs)
	4s+n	LISTofMONO_ENCODING	string (see MONO_ENCODING for m)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationText3D 3
	2	83		output command number
	2	2+s+(6*\fIfp\fP+n)/4		output command length
	\fIfp\fP	FLOAT		origin x
	\fIfp\fP	FLOAT		origin y
	\fIfp\fP	FLOAT		origin z
	\fIfp\fP	FLOAT		offset x
	\fIfp\fP	FLOAT		offset y
	\fIfp\fP	FLOAT		offset z
	2	s		number of mono encodings
	2			unused
				(let n be the sum of the sizes, m, of the MONO_ENCODINGs)
	4s+n	LISTofMONO_ENCODING	string (see MONO_ENCODING for m)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao AnnotationText2D 3
	2	84		output command number
	2	2+s+(4*\fIfp\fP+n)/4		output command length
	\fIfp\fP	FLOAT		origin x
	\fIfp\fP	FLOAT		origin y
	\fIfp\fP	FLOAT		offset x
	\fIfp\fP	FLOAT		offset y
	2	s		number of mono encodings
	2			unused
				(let n be the sum of the sizes, m, of the MONO_ENCODINGs)
	4s+n	LISTofMONO_ENCODING	string (see MONO_ENCODING for m)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Polyline3D 3
	2	85		output command number
	2	1+3*\fIfp\fP*n/4		output command length
	3*\fIfp\fP*n	LISTofCOORD_3D		vertices (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Polyline2D 3
	2	86		output command number
	2	1+\fIfp\fP*n/2		output command length
	2*\fIfp\fP*n	LISTofCOORD_2D		vertices (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao PolylineSet3DWithData 3
	2	87		output command number
	2	3+n+m*v/4		output command length
	2	INT16		color type
	2			vertex attributes
		#x0001	color
	4	n		number of lists
				(let m be the number of vertices in the list of lists)
	4n+m*v	LISTofLISTofVERTEX		vertices (see VERTEX for v)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao NonUniformBSplineCurve 3
	2	88		output command number
	2	4+\fIfp\fP*(2+k+t*p)/4		output command length
	2	CARD16		order
	2			type
		0	Rational
		1	NonRational	(t = (type == Rational) ? 4 : 3)
	\fIfp\fP	FLOAT		tmin
	\fIfp\fP	FLOAT		tmax
	4	k		number of knots
	4	p		number of points
	\fIfp\fP*k	LISTofFLOAT		knots
	\fIfp\fP*t*p	LISTofCOORD		points (COORD_3D or COORD_4D depending on t)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillArea3D 3
	2	89		output command number
	2	2+3*\fIfp\fP*p/4		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			unused
	3*\fIfp\fP*p	LISTofCOORD_3D		vertices (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillArea2D 3
	2	90		output command number
	2	2+\fIfp\fP*p/2		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			unused
	2*\fIfp\fP*p	LISTofCOORD_2D		vertices (length implicit)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillArea3DWithData 3
	2	91		output command number
	2	4+f/4		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			unused
	2	INT16		color type
	2			facet attributes
		#x0001	color
		#x0002	normal
	2			vertex attributes
		#x0001	color
		#x0002	normal
	2			unused
	f	FACET		facet (see FACET for f)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillAreaSet3D 3
	(let m be the number of vertices in the list of lists)
	2	92		output command number
	2	3+n+3*\fIfp\fP*m/4		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			contour hint
		0	Disjoint
		1	Nested
		2	Intersecting
		3	Unknown
	4	n		number of lists
	4n+3*\fIfp\fP*m	LISTofLISTofCOORD_3D		vertices
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillAreaSet2D 3
	(let m be the number of vertices in the list of lists)
	2	93		output command number
	2	3+n+\fIfp\fP*m/2		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			contour hint
		0	Disjoint
		1	Nested
		2	Intersecting
		3	Unknown
	4	n		number of lists
	4n+2*\fIfp\fP*m	LISTofLISTofCOORD_2D		vertices
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao FillAreaSet3DWithData 3
	(let m be the number of vertices in the list of lists)
	2	94		output command number
	2	5+n+(d+m*v)/4		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	1			ignore edges
		0	False
		1	True
	1			contour hint
		0	Disjoint
		1	Nested
		2	Intersecting
		3	Unknown
	2	INT16		color type
	2			facet attributes
		#x0001	color
		#x0002	normal
	2			vertex attributes
		#x0001	color
		#x0002	normal
		#x0004	edges
	2			unused
	4	n		number of lists
	d	OPT_DATA		facet data (see OPT_DATA for d)
	4n+m*v	LISTofLISTofVERTEX		vertices (see VERTEX for v)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao TriangleStrip 3
	2	95		output command number
	2	4+((p-2)d+p*v)/4		output command length
	2	INT16		color type
	2			facet attributes
		#x0001	color
		#x0002	normal
	2			vertex attributes
		#x0001	color
		#x0002	normal
	2			unused
	4	p		number of points
	(p-2)d	LISTofOPT_DATA		facet data (see OPT_DATA for d)
	p*v	LISTofVERTEX		vertices (see VERTEX for v)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao QuadrilateralMesh 3
	2	96		output command number
	2	4+(d(m*n-n-m+1)+m*n*v)/4		output command length
	2	INT16		color type
	2	m		number of points in m direction
	2	n		number of points in n direction
	2			facet attributes
		#x0001	color
		#x0002	normal
	2			vertex attributes
		#x0001	color
		#x0002	normal
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	(m-1)(n-1)d	LISTofOPT_DATA		facet data (see OPT_DATA for d)
	m*n*v	LISTofVERTEX		vertices (see VERTEX for v)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao SetOfFillAreaSets 3
	(let s be the size of the data)
	(s = fc*d + vc*v + ec*e + p1 + 2fc + 2cc + 2ec + p2)
	2	97		output command number
	2	6+s/4		output command length
	2			shape
		0	Complex
		1	Nonconvex
		2	Convex
		3	Unknown
	2	INT16		color type
	2			fill area set attributes
		#x0001	color
		#x0002	normal
	2			vertex attributes
		#x0001	color
		#x0002	normal
	2			edge attributes (call this value e)
		#x0001	On
	1			contour hint
		0	Disjoint
		1	Nested
		2	Intersecting
		3	Unknown
	1			all contour counts = 1
		0	False
		1	True
	2	fc		number of fill area sets
	2	vc		number of vertices
	2	ec		number of edges (sum of all contour counts)
	2	cc		number of contours
	fc*d	LISTofOPT_DATA		fill area set data (see OPT_DATA for d)
	vc*v	LISTofVERTEX		vertex data (see VERTEX for v)
	ec*e	LISTofCARD8		edge data
	p1			unused, p1=pad(ec*e)
				(note that the counts in the connectivity data are CARD16s)
	2fc+2cc+2ec	LISTofLISTofLISTofCARD16	connectivity data
	p2			unused, p2=pad(2fc+2cc+2ec)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao NonUniformBSplineSurface 3
	2	98		output command number
	2	7+\fIfp\fP*(uk+vk+m*n*s)/4+t+sc/4	output command length
	2			type
		0	Rational
		1	NonRational	(s = (type == Rational) ? 4 : 3)
	2	CARD16		u order
	2	CARD16		v order
	2			unused
	4	uk		number of u knots
	4	vk		number of v knots
	2	m		number of points in u direction
	2	n		number of points in v direction
	4	t		number of trim curve lists
	\fIfp\fP*uk	LISTofFLOAT		u knots
	\fIfp\fP*vk	LISTofFLOAT		v knots
	m*n*\fIfp\fP*s	LISTofCOORD		points (COORD_3D or COORD_4D depending on s)
				(let sc be sum of sizes of TRIM_CURVEs in bytes)
	4t+sc	LISTofLISTofTRIM_CURVE		trim curves
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CellArray3D 3
	2	99		output command number
	2	3+(9*\fIfp\fP+2q+p)/4		output command length
	\fIfp\fP	FLOAT		point1 x
	\fIfp\fP	FLOAT		point1 y
	\fIfp\fP	FLOAT		point1 z
	\fIfp\fP	FLOAT		point2 x
	\fIfp\fP	FLOAT		point2 y
	\fIfp\fP	FLOAT		point2 z
	\fIfp\fP	FLOAT		point3 x
	\fIfp\fP	FLOAT		point3 y
	\fIfp\fP	FLOAT		point3 z
	4	CARD32		dx
	4	CARD32		dy (let q = dx * dy)
	2q	LISTofCARD16		color indices
	p			unused, p=pad(2q)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao CellArray2D 3
	2	100		output command number
	2	3+\fIfp\fP+(2s+p)/4		output command length
	\fIfp\fP	FLOAT		point1 x
	\fIfp\fP	FLOAT		point1 y
	\fIfp\fP	FLOAT		point2 x
	\fIfp\fP	FLOAT		point2 y
	4	CARD32		dx
	4	CARD32		dy (let s = dx * dy)
	2s	LISTofCARD16		color indices
	p			unused, p=pad(2s)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao ExtendedCellArray3D 3
	(n = ((color type == Indexed) || (color type == RGBInt8)) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4))
	2	101		output command number
	2	4+9*\fIfp\fP/4+p*n		output command length
	2	INT16		color type
	2			unused
	\fIfp\fP	FLOAT		point1 x
	\fIfp\fP	FLOAT		point1 y
	\fIfp\fP	FLOAT		point1 z
	\fIfp\fP	FLOAT		point2 x
	\fIfp\fP	FLOAT		point2 y
	\fIfp\fP	FLOAT		point2 z
	\fIfp\fP	FLOAT		point3 x
	\fIfp\fP	FLOAT		point3 y
	\fIfp\fP	FLOAT		point3 z
	4	CARD32		dx
	4	CARD32		dy (let p = dx * dy)
	p*4*n	LISTofCOLOR		colors
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao GDP3D 3
	2	102		output command number
	2	4+(3*\fIfp\fP*p+d+q)/4	output command length
	4	INT32		GDP identifier
	4	p		number of points
	4	d		number of bytes of data
	3*\fIfp\fP*p	LISTofCOORD_3D		points
	d	LISTofCARD8		data
	q			unused, q=pad(d)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao GDP2D 3
	2	103		output command number
	2	4+(2*\fIfp\fP*p+d+q)/4	output command length
	4	INT32		GDP identifier
	4	p		number of points
	4	d		number of bytes of data
	2*\fIfp\fP*p	LISTofCOORD_2D		points
	d	LISTofCARD8		data
	q			unused, q=pad(d)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.Ao Noop
	2	104		output command number
	2	1		output command length
.De
.bp

.AC "Lookup Tables" 1
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
LineBundle  (12+2*\fIfp\fP+4*n)
	2	INT16		line type
	2	INT16		polyline interpolation method
	2	INT16		curve approximation method
	2			unused
	\fIfp\fP	FLOAT		curve approximation tolerance
	\fIfp\fP	FLOAT		line width
	4+4*n	COLOR_SPECIFIER	line color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
MarkerBundle (8+\fIfp\fP+4*n)
	2	INT16		marker type
	2			unused
	\fIfp\fP	FLOAT		marker scale
	4+4*n	COLOR_SPECIFIER	marker color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
TextBundle (8+2*\fIfp\fP+4*n)
	2	CARD16		font table index
	2			text precision
		0	StringPrecision
		1	CharPrecision
		2	StrokePrecision
	\fIfp\fP	FLOAT		character expansion factor
	\fIfp\fP	FLOAT		character spacing
	4+4*n	COLOR_SPECIFIER	text color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
InteriorBundle (36+12*\fIfp\fP+4*(n+n1+n2+n3))
	2	INT16		interior style
	2	CARD16 | INT16		interior style index
	2	INT16		reflection model
	2	INT16		surface interpolation method
	2	INT16		bf interior style
	2	CARD16 | INT16		bf interior style index
	2	INT16		bf reflection model
	2	INT16		bf surface interpolation method
	2	INT16		surface approximation method
	2			unused
	\fIfp\fP	FLOAT		surface approximation u tolerance
	\fIfp\fP	FLOAT		surface approximation v tolerance
	4+4*n	COLOR_SPECIFIER	surface color
	\fIfp\fP	FLOAT		reflection attribute ambient coefficient
	\fIfp\fP	FLOAT		reflection attribute diffuse coefficient
	\fIfp\fP	FLOAT		reflection attribute specular coefficient
	\fIfp\fP	FLOAT		reflection attribute specular concentration
	\fIfp\fP	FLOAT		reflection attribute transmission coefficient
	4+4*n1	COLOR_SPECIFIER	reflection attribute specular color
	4+4*n2	COLOR_SPECIFIER	bf surface color
	\fIfp\fP	FLOAT		bf reflection attribute ambient coefficient
	\fIfp\fP	FLOAT		bf reflection attribute diffuse coefficient
	\fIfp\fP	FLOAT		bf reflection attribute specular coefficient
	\fIfp\fP	FLOAT		bf reflection attribute specular concentration
	\fIfp\fP	FLOAT		bf reflection attribute transmission coefficient
	4+4*n3	COLOR_SPECIFIER	bf reflection attribute specular color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
EdgeBundle (8+\fIfp\fP+4*n)
	1			surface edges
		0	Off
		1	On
	1			unused
	2	INT16		surface edge type
	\fIfp\fP	FLOAT		surface edge width
	4+4*n	COLOR_SPECIFIER	surface edge color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Pattern (8+c*4*n)
	(n = ((color type == Indexed) || (color type == RGBInt8)) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4))
	2	INT16		color type
	2	CARD16		numx
	2	CARD16		numy (let c = numx * numy)
	2			unused
	c*4n	LISTofCOLOR		colors
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Color (4+4*n)
	n = ((color type == RGBInt8) ? 1 : ((color type == RGBInt16) ? 2 : 3*\fIfp\fP/4)
	2	INT16		color type
	2			unused
	4n	DIRECT_COLOR		color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
TextFont (4+4n)
	4	n		number of font IDs
	4n	LISTofCARD32		font IDs
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
View (4+38*\fIfp\fP)
	2 			clip flags
		#x0001	clip_xy
		#x0002	clip_back
		#x0004	clip_front
	2			unused
	\fIfp\fP	FLOAT		clip limits min x
	\fIfp\fP	FLOAT		clip limits min y
	\fIfp\fP	FLOAT		clip limits min z
	\fIfp\fP	FLOAT		clip limits max x
	\fIfp\fP	FLOAT		clip limits max y
	\fIfp\fP	FLOAT		clip limits max z
	16*\fIfp\fP	FLOAT[4][4]		orientation
	16*\fIfp\fP	FLOAT[4][4]		mapping
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Light (8+10*\fIfp\fP+4*n)
	2	INT16		light type
	2			unused
	\fIfp\fP	FLOAT		direction x
	\fIfp\fP	FLOAT		direction y
	\fIfp\fP	FLOAT		direction z
	\fIfp\fP	FLOAT		point x
	\fIfp\fP	FLOAT		point y
	\fIfp\fP	FLOAT		point z
	\fIfp\fP	FLOAT		concentration
	\fIfp\fP	FLOAT		spread angle
	\fIfp\fP	FLOAT		attenuation factor1
	\fIfp\fP	FLOAT		attenuation factor2
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
DepthCue (8+4*\fIfp\fP+4*n)
	1			mode
		0	Off
		1	On
	3			unused
	\fIfp\fP	FLOAT		front plane
	\fIfp\fP	FLOAT		back plane
	\fIfp\fP	FLOAT		front scaling
	\fIfp\fP	FLOAT		back scaling
	4+4*n	COLOR_SPECIFIER	color
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
ColorApprox (28+3*\fIfp\fP)
	2	INT16		color approximation type
	2	INT16		color approximation model
	2	CARD16		max1
	2	CARD16		max2
	2	CARD16		max3
	1			dither
		0	Off
		1	On
	1			unused
	4	CARD32		multiplier1
	4	CARD32		multiplier2
	4	CARD32		multiplier3
	\fIfp\fP	FLOAT		weight1
	\fIfp\fP	FLOAT		weight2
	\fIfp\fP	FLOAT		weight3
	4	CARD32		base pixel
.De

.AC "Lookup Table Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreateLookupTable
	1	CARD8		opcode (X11 assigned)
	1	4		PEX opcode
	2	4		request length
	4	CARD32		example drawable ID
	4	CARD32		lookup table ID
	2			table type
		1	LineBundle
		2	MarkerBundle
		3	TextBundle
		4	InteriorBundle
		5	EdgeBundle
		6 	Pattern
		7	TextFont
		8	Color
		9	View
		10	Light
		11	DepthCue
		12	ColorApprox
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopyLookupTable
	1	CARD8		opcode (X11 assigned)
	1	5		PEX opcode
	2	3		request length
	4	CARD32		source lookup table ID
	4	CARD32		destination lookup table ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreeLookupTable
	1	CARD8		opcode (X11 assigned)
	1	6		PEX opcode
	2	2		request length
	4	CARD32		lookup table ID
.De

.AC "Lookup Table Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetTableInfo
	1	CARD8		opcode (X11 assigned)
	1	7		PEX opcode
	2	3		request length
	4	CARD32		example drawable ID
	2			table type
		1	LineBundle
		2	MarkerBundle
		3	TextBundle
		4	InteriorBundle
		5	EdgeBundle
		6 	Pattern
		7	TextFont
		8	Color
		9	View
		10	Light
		11	DepthCue
		12	ColorApprox
	2			unused
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	2			unused
	2	CARD16		definable entries
	2	CARD16		num predefined
	2	CARD16		predefined min
	2	CARD16		predefined max
	14			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetPredefinedEntries
	1	CARD8		opcode (X11 assigned)
	1	8		PEX opcode
	2	5		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		example drawable ID
	2			table type
		1	LineBundle
		2	MarkerBundle
		3	TextBundle
		4	InteriorBundle
		5	EdgeBundle
		6 	Pattern
		7	TextFont
		8	Color
		9	View
		10	Light
		11	DepthCue
		12	ColorApprox
	2	CARD16		start
	2	n		count
	2			unused
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	s/4		reply length
	4			unused
	4	n		number of table entries
	16			unused
				(let s be the sum of the sizes of the n table \
entries in bytes)
				(each entry may be a different size, depending \
on colors
				and floating point format)
	s	LISTofTABLE_ENTRY		entries
				(each entry will be aligned on a 4-byte boundary)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetDefinedIndices
	1	CARD8		opcode (X11 assigned)
	1	9		PEX opcode
	2	2		request length
	4	CARD32		lookup table ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	(2n+p)/4		reply length
	4	n		number of indices returned
	20			unused
	2n	LISTofCARD16		defined indices
	p			unused, p=pad(2n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetTableEntry
	1	CARD8		opcode (X11 assigned)
	1	10		PEX opcode
	2	4		request length
	2	INT16		floating point format
	2			value type
		0	Set
		1	Realized
	4	CARD32		lookup table ID
	2	CARD16		index
	2			unused
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	s/4		reply length
	2			status
		0	Default
		1	Defined
	2	t		table type
		1	LineBundle
		2	MarkerBundle
		3	TextBundle
		4	InteriorBundle
		5	EdgeBundle
		6 	Pattern
		7	TextFont
		8	Color
		9	View
		10	Light
		11	DepthCue
		12	ColorApprox
	20			unused
				(let s be size of entry of type t)
				(entry size differs, depending on colors \
and floating point format)
	s	TABLE_ENTRY		entry
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetTableEntries
	1	CARD8		opcode (X11 assigned)
	1	11		PEX opcode
	2	4		request length
	2	INT16		floating point format
	2			value type
		0	Set
		1	Realized
	4	CARD32		lookup table ID
	2	CARD16		start
	2	n		count
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	s/4		reply length
	2	t		table type
		1	LineBundle
		2	MarkerBundle
		3	TextBundle
		4	InteriorBundle
		5	EdgeBundle
		6 	Pattern
		7	TextFont
		8	Color
		9	View
		10	Light
		11	DepthCue
		12	ColorApprox
				(let s be the sum of the sizes of the n table \
entries in bytes)
				(each entry may be a different size, depending \
on colors
				and floating point format)
	2			unused
	4	n		number of table entries
	16			unused
	s	LISTofTABLE_ENTRY		entries
.De

.AC "Lookup Table Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetTableEntries
	1	CARD8		opcode (X11 assigned)
	1	12		PEX opcode
	2	4+s/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		lookup table ID
	2	CARD16		start
	2	n		count
				(let s be the sum of the sizes of the n table \
entries in bytes)
				(each entry may be a different size, depending \
on colors
				and floating point format)
	s	LISTofTABLE_ENTRY		entries
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXDeleteTableEntries
	1	CARD8		opcode (X11 assigned)
	1	13		PEX opcode
	2	3		request length
	4	CARD32		lookup table ID
	2	CARD16		start
	2	CARD16		count
.De
.bp

.AC "Pipeline Contexts" 1
.LP
The following components may appear in the requests \fBPEXCreatePipelineContext\fP
and \fBPEXChangePipelineContext\fP; they may also appear in replies to
\fBPEXGetPipelineContext\fP.  Because the components are not of a fixed size
(as they are in X11), the list must be parsed serially to know where one
component ends and the next begins.  A bitmap mask of three CARD32s specifies
which components are to be found in a list.  Proceding from the least
significant bit of the first CARD32 through to the last used bit of the
third CARD32, if the bit is set, the next item in the list is of the
specified type.
.sp 1
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
PC_BITMASK
.sp 1
First CARD32:
	#x00000001	marker_type
	#x00000002	marker_scale
	#x00000004	marker_color
	#x00000008	marker_bundle_index
	#x00000010	text_font_index
	#x00000020	text_precision
	#x00000040	char_expansion
	#x00000080	char_spacing
	#x00000100	text_color
	#x00000200	char_height
	#x00000400	char_up_vector
	#x00000800	text_path
	#x00001000	text_alignment
	#x00002000	atext_height
	#x00004000	atext_up_vector
	#x00008000	atext_path
	#x00010000	atext_alignment
	#x00020000	atext_style
	#x00040000	text_bundle_index
	#x00080000	line_type
	#x00100000	line_width
	#x00200000	line_color
	#x00400000	curve_approx
	#x00800000	polyline_interp
	#x01000000	line_bundle_index
	#x02000000	interior_style
	#x04000000	interior_style_index
	#x08000000	surface_color
	#x10000000	reflection_attr
	#x20000000	reflection_model
	#x40000000	surface_interp
	#x80000000	bf_interior_style
Second CARD32:
	#x00000001	bf_interior_style_index
	#x00000002	bf_surface_color
	#x00000004	bf_reflection_attr
	#x00000008	bf_reflection_model
	#x00000010	bf_surface_interp
	#x00000020	surface_approx
	#x00000040	culling_mode
	#x00000080	distinguish
	#x00000100	pattern_size
	#x00000200	pattern_ref_pt
	#x00000400	pattern_ref_vec1
	#x00000800	pattern_ref_vec2
	#x00001000	interior_bundle_index
	#x00002000	surface_edges
	#x00004000	surface_edge_type
	#x00008000	surface_edge_width
	#x00010000	surface_edge_color
	#x00020000	edge_bundle_index
	#x00040000	local_transform
	#x00080000	global_transform
	#x00100000	model_clip
	#x00200000	model_clip_volume
	#x00400000	view_index
	#x00800000	light_state
	#x01000000	depth_cue_index
	#x02000000	asfs
	#x04000000	pick_id
	#x08000000	HLHSR_identifier
	#x10000000	name_set
	#x20000000	color_approx_index
	#x40000000	rdr_color_model
	#x80000000	psurf_char
Third CARD32:
	No bits are yet defined.  Must be zero.
.sp 1
ASF flags bits:
	#x00000001	marker_type_asf
	#x00000002	marker_scale_asf
	#x00000004	marker_color_asf
	#x00000008	text_font_index_asf
	#x00000010	text_precison_asf
	#x00000020	char_expansion_asf
	#x00000040	char_spacing_asf
	#x00000080	text_color_asf
	#x00000100	line_type_asf
	#x00000200	line_width_asf
	#x00000400	line_color_asf
	#x00000800	curve_approx_asf
	#x00001000	polyline_interp_asf
	#x00002000	interior_style_asf
	#x00004000	interior_style_index_asf
	#x00008000	surface_color_asf
	#x00010000	surface_interp_asf
	#x00020000	reflection_model_asf
	#x00040000	reflection_attr_asf
	#x00080000	bf_interior_style_asf
	#x00100000	bf_interior_style_index_asf
	#x00200000	bf_surface_color_asf
	#x00400000	bf_surface_interp_asf
	#x00800000	bf_reflection_model_asf
	#x01000000	bf_reflection_attr_asf
	#x02000000	surface_approx_asf
	#x04000000	surface_edges_asf
	#x08000000	surface_edge_type_asf
	#x10000000	surface_edge_width_asf
	#x20000000	surface_edge_color_asf
.De

.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
.sp 1
marker_type		4	INT16
marker_scale		\fIfp\fP	FLOAT
marker_color		4+4*n	COLOR_SPECIFIER
marker_bundle_index		4	CARD16
text_font_index		4	CARD16
text_precision		4	CARD16
			0	StringPrecision
			1	CharPrecision
			2	StrokePrecision
char_expansion		\fIfp\fP	FLOAT
char_spacing		\fIfp\fP	FLOAT
text_color		4+4*n	COLOR_SPECIFIER
char_height		\fIfp\fP	FLOAT
char_up_vector		2*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
text_path		4	CARD16
			0	PathRight
			1	PathLeft
			2	PathUp
			3	PathDown
text_alignment		4
		2	CARD16		alignment vertical
			0	ValignNormal
			1	ValignTop
			2	ValignCap
			3	ValignHalf
			4	ValignBase
			5	ValignBottom
		2	CARD16		alignment horizontal
			0	HalignNormal
			1	HalignLeft
			2	HalignCenter
			3	HalignRight

atext_height		\fIfp\fP	FLOAT
atext_up_vector		2*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
atext_path		4	CARD16
			0	PathRight
			1	PathLeft
			2	PathUp
			3	PathDown
atext_alignment		4
		2	CARD16		alignment vertical
			0	ValignNormal
			1	ValignTop
			2	ValignCap
			3	ValignHalf
			4	ValignBase
			5	ValignBottom
		2	CARD16		alignment horizontal
			0	HalignNormal
			1	HalignLeft
			2	HalignCenter
			3	HalignRight
atext_style		4	INT16
text_bundle_index		4	CARD16
line_type		4	INT16
line_width		\fIfp\fP	FLOAT
line_color		4+4*n	COLOR_SPECIFIER
curve_approx		4+\fIfp\fP
		4	INT16		curve approximation method
		\fIfp\fP	FLOAT		curve approximation tolerance
polyline_interp		4	INT16
line_bundle_index		4	CARD16
interior_style		4	INT16
interior_style_index		4	INT16 | CARD16
surface_color		4+4*n	COLOR_SPECIFIER
reflection_attr		4+5*\fIfp\fP+4*n
		\fIfp\fP	FLOAT		ambient coefficient
		\fIfp\fP	FLOAT		diffuse coefficient
		\fIfp\fP	FLOAT		specular coefficient
		\fIfp\fP	FLOAT		specular concentration
		\fIfp\fP	FLOAT		transmission coefficient
		4+4*n	COLOR_SPECIFIER	specular color
reflection_model		4	INT16
surface_interp		4	INT16
bf_interior_style		4	INT16
bf_interior_style_index		4	INT16 | CARD16
bf_surface_color		4+4*n	COLOR_SPECIFIER
bf_reflection_attr		4+5*\fIfp\fP+4*n
		\fIfp\fP	FLOAT		ambient coefficient
		\fIfp\fP	FLOAT		diffuse coefficient
		\fIfp\fP	FLOAT		specular coefficient
		\fIfp\fP	FLOAT		specular concentration
		\fIfp\fP	FLOAT		transmission coefficient
		4+4*n	COLOR_SPECIFIER	specular color
bf_reflection_model		4	INT16
bf_surface_interp		4	INT16
surface_approx		4+2*\fIfp\fP
		4	INT16		surface approximation method
		\fIfp\fP	FLOAT		surface approximation u tolerance
		\fIfp\fP	FLOAT		surface approximation v tolerance
culling_mode		4	CARD16
			0	None
			1	BackFaces
			2	FrontFaces
distinguish		4	CARD8
			0	False
			1	True
pattern_size		2*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
pattern_ref_pt		3*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
		\fIfp\fP	FLOAT		z
pattern_ref_vec1		3*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
		\fIfp\fP	FLOAT		z
pattern_ref_vec2		3*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
		\fIfp\fP	FLOAT		z
interior_bundle_index		4	CARD16
surface_edges		4	CARD16
			0	Off
			1	On
surface_edge_type		4	INT16
surface_edge_width		\fIfp\fP	FLOAT
surface_edge_color		4+4*n	COLOR_SPECIFIER
edge_bundle_index		4	CARD16
local_transform		16*\fIfp\fP	FLOAT[4][4]
global_transform		16*\fIfp\fP	FLOAT[4][4]
model_clip		4	CARD16
			0	Clip
			1	NoClip
model_clip_volume		4+6*\fIfp\fP*n
		4	n		number of half spaces
		6*\fIfp\fP*n	LISTofHALF_SPACE
view_index		4	CARD16
light_state		4+2n+p
		4	n		number of lights in active list
		2n	LISTofCARD16	indices of active lights
		p			unused, p=pad(2n)
depth_cue_index		4	CARD16
asf_enables		4	CARD32		(see note below)
asfs		4 	CARD32
			0	Bundled
			1	Individual
pick_id		4	CARD32
HLHSR_identifier		4	CARD32
name_set		4	CARD32
color_approx_index		4	CARD16
rdr_color_model		4	INT16
psurf_char		4+n
		2	INT16		characteristics
		2	n		length of data in bytes
		n	LISTofCARD8	data (explicitly padded)
.sp 1
.De
.LP
\fINote:\fR The asf_enables field is only used for the
\fBPEXCreatePipelineContext\fP and \fBPEXChangePipelineContext\fP requests.
For those requests, if the asf_enables bit for a particular asf is set, the
asf value is taken from the corresponding bit in the asfs field.  For the
\fBPEXGetPipelineContext\fP reply, asf_enables is present, but has no meaning
and must have all defined asf bits set.

.AC "Pipeline Context Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreatePipelineContext
	1	CARD8		opcode (X11 assigned)
	1	14		PEX opcode
	2	6+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		pipeline context ID
	12	PC_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopyPipelineContext
	1	CARD8		opcode (X11 assigned)
	1	15		PEX opcode
	2	6		request length
	4	CARD32		source pipeline context ID
	4	CARD32		destination pipeline context ID
	12	PC_BITMASK		item mask
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreePipelineContext
	1	CARD8		opcode (X11 assigned)
	1	16		PEX opcode
	2	2		request length
	4	CARD32		pipeline context ID
.De

.AC "Pipeline Context Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetPipelineContext
	1	CARD8		opcode (X11 assigned)
	1	17		PEX opcode
	2	6		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		pipeline context ID
	12 	PC_BITMASK		item mask
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Pipeline Context Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangePipelineContext
	1	CARD8		opcode (X11 assigned)
	1	18		PEX opcode
	2	6+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		pipeline context ID
	12	PC_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.bp

.AC "Renderers" 1
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
R_BITMASK
.sp 1
	#x00000001	pipeline_context_id
	#x00000002	current_path
	#x00000004	marker_bundle
	#x00000008	text_bundle
	#x00000010	line_bundle
	#x00000020	interior_bundle
	#x00000040	edge_bundle
	#x00000080	view_table
	#x00000100	color_table
	#x00000200	depth_cue_table
	#x00000400	light_table
	#x00000800	color_approx_table
	#x00001000	pattern_table
	#x00002000	text_font_table
	#x00004000	highlight_inclusion
	#x00008000	highlight_exclusion
	#x00010000	invisibility_inclusion
	#x00020000	invisibility_exclusion
	#x00040000	renderer_state
	#x00080000	HLHSR_mode
	#x00100000	NPC_subvolume
	#x00200000	viewport
	#x00400000	clip_list
	#x00800000	pick_inclusion
	#x01000000	pick_exclusion
	#x02000000	pick_start_path
	#x04000000	background_color
	#x08000000	clear_I
	#x10000000	clear_Z
	#x20000000	echo_mode
.De
.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
.sp 1
pipeline_context		4	CARD32
current_path		4+8n
		4	n		number of element references
		8n	LISTofELEMENT_REF
marker_bundle		4	CARD32
text_bundle		4	CARD32
line_bundle		4	CARD32
interior_bundle		4	CARD32
edge_bundle		4	CARD32
view_table		4	CARD32
color_table		4	CARD32
depth_cue_table		4	CARD32
light_table		4	CARD32
color_approx_table		4	CARD32
pattern_table		4	CARD32
text_font_table		4	CARD32
highlight_inclusion		4	CARD32
highlight_exclusion		4	CARD32
invisibility_inclusion		4	CARD32
invisibility_exclusion		4	CARD32
renderer_state		4	CARD16
			0	Idle
			1	Rendering
			2 	Picking
HLHSR_mode		4	INT16
NPC_subvolume		6*\fIfp\fP
		\fIfp\fP	FLOAT		min x
		\fIfp\fP	FLOAT		min y
		\fIfp\fP	FLOAT		min z
		\fIfp\fP	FLOAT		max x
		\fIfp\fP	FLOAT		max y
		\fIfp\fP	FLOAT		max z
viewport		12+2*\fIfp\fP
		2	INT16		min x
		2	INT16		min y
		\fIfp\fP	FLOAT		min z
		2	INT16		max x
		2	INT16		max y
		\fIfp\fP	FLOAT		max z
		1	CARD8		use drawable
			0	False
			1	True
		3			unused
clip_list		4+8n
		4	n		number of device rectangles
		8n	LISTofDEVICE_RECT
pick_inclusion		4	CARD32
pick_exclusion		4	CARD32
pick_start_path		4+8n
		4	n		number of element refs
		8n	LISTofELEMENT_REF
background_color		4+4n	COLOR_SPECIFIER
clear_I		4	CARD8
			0 False	
			1 True
clear_Z		4	CARD8
			0 False	
			1 True
echo_mode		4	CARD16
			0 NoEcho
			1 Echo
			2 UnEcho

.AC "Renderer Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreateRenderer
	1	CARD8		opcode (X11 assigned)
	1	19		PEX opcode
	2	5+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		renderer ID
	4	CARD32		example drawable ID
	4	R_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreeRenderer
	1	CARD8		opcode (X11 assigned)
	1	20		PEX opcode
	2	2		request length
	4	CARD32		renderer ID
.De

.AC "Renderer Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangeRenderer
	1	CARD8		opcode (X11 assigned)
	1	21		PEX opcode
	2	4+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		renderer ID
	4	R_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Renderer Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetRendererAttributes
	1	CARD8		opcode (X11 assigned)
	1	22		PEX opcode
	2	4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		renderer ID
	4	R_BITMASK		item mask
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetRendererDynamics
	1	CARD8		opcode (X11 assigned)
	1	23		PEX opcode
	2	2		request length
	4	CARD32		renderer ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	4			tables
		#x00000001	DynMarkerBundle
		#x00000002	DynTextBundle
		#x00000004	DynLineBundle
		#x00000008	DynInteriorBundle
		#x00000010	DynEdgeBundle
		#x00000020	DynViewTable
		#x00000040	DynColorTable
		#x00000080	DynDepthCueTable
		#x00000100	DynLightTable
		#x00000200	DynColorApproxTable
		#x00000400	DynPatternTable
		#x00000800	DynTextFontTable
		#x00010000	DynMarkerBundleContents
		#x00020000	DynTextBundleContents
		#x00040000	DynLineBundleContents
		#x00080000	DynInteriorBundleContents
		#x00100000	DynEdgeBundleContents
		#x00200000	DynViewTableContents
		#x00400000	DynColorTableContents
		#x00800000	DynDepthCueTableContents
		#x01000000	DynLightTableContents
		#x02000000	DynColorApproxTableContents
		#x04000000	DynPatternTableContents
		#x08000000	DynTextFontTableContents
	4			namesets
		#x00000001	DynHighlightNameset
		#x00000002	DynInvisibilityNameset
		#x00000004	DynPickNameset
		#x00010000	DynHighlightNamesetContents
		#x00020000	DynInvisibilityNamesetContents
		#x00040000	DynPickNamesetContents
	4			attributes
		#x00000001	DynHLHSRMode
		#x00000002	DynNPCSubvolume
		#x00000004	DynViewport
		#x00000008	DynClipList
		#x00000010	DynEchoMode
	12			unused
.De

.AC "Client-Side Traversal Support" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXBeginRendering
	1	CARD8		opcode (X11 assigned)
	1	24		PEX opcode
	2	3		request length
	4	CARD32		renderer ID
	4	CARD32		drawable ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEndRendering
	1	CARD8		opcode (X11 assigned)
	1	25		PEX opcode
	2	3		request length
	4	CARD32		renderer ID
	1			flush
		0	False
		1	True
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXBeginStructure
	1	CARD8		opcode (X11 assigned)
	1	26		PEX opcode
	2	3		request length
	4	CARD32		renderer ID
	4	CARD32		structure ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEndStructure
	1	CARD8		opcode (X11 assigned)
	1	27		PEX opcode
	2	2		request length
	4	CARD32		renderer ID
.De

.AC "Rendering Commands" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXRenderOutputCommands
	1	CARD8		opcode (X11 assigned)
	1	28		PEX opcode
	2	4+s/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		renderer ID
	4	n		number of commands
				(let s be the total size of the output commands in bytes)
	s	LISTofOUTPUT_COMMAND		cmds
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXRenderElements
	1	CARD8		opcode (X11 assigned)
	1	97		PEX opcode
	2	7		request length
	4	CARD32		renderer ID
	4	CARD32		structure ID
	2			position1 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position1 offset
	2			position2 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position2 offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXAccumulateState
	1	CARD8		opcode (X11 assigned)
	1	98		PEX opcode
	2	3+2n		request length
	4	CARD32		renderer ID
	4	n		number element refs
	8n	LISTofELEMENT_REF	path
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXRenderNetwork
	1	CARD8		opcode (X11 assigned)
	1	29		PEX opcode
	2	4		request length
	4	CARD32		renderer ID
	4	CARD32		drawable ID
	4	CARD32		structure ID
.De
.bp

.AC "Renderer Picking" 1
.LP

.AC "Pick One" 2
.LP

.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXBeginPickOne
	1	CARD8		opcode (X11 assigned)
	1	99		PEX opcode
	2	6+n		request length
	2	INT16		floating point format
	2	INT16		pick one method 
		1 Last 
		2 ClosestZ 
		3 VisibleAny
		4 VisibleClosest	
	4	CARD32		renderer ID
	4	CARD32		drawable ID
	4	INT32		structure identifer (not resource ID)
	4+4n	PICK_RECORD		pick data
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEndPickOne
	1	CARD8		opcode (X11 assigned)
	1	100		PEX opcode
	2	2		request length
	4	CARD32		renderer ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	3n		reply length
	4	n		number of pick element refs
	2			pick status
		0 NoPick
		1 Ok
		2 Aborted
	1			better pick
		0 False
		1 True
	17			unused
	12n	LISTofPICK_ELEMENT_REF	pick path
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXPickOne
	1	CARD8		opcode (X11 assigned)
	1	101		PEX opcode
	2	6+n		request length
	2	INT16		floating point format
	2	INT16		pick one method 
		1 Last 
		2 ClosestZ 
		3 VisibleAny
		4 VisibleClosest	
	4	CARD32		renderer ID
	4	CARD32		drawable ID
	4	CARD32		structure identifer
	4+4n	PICK_RECORD		pick data
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	3n		reply length
	4	n		number of pick element refs
	2			pick status
		0 NoPick
		1 Ok
		2 Aborted
	1			better pick
		0 False
		1 True
	17			unused
	12n	LISTofPICK_ELEMENT_REF	pick path
.De
.LP

.AC "Pick All" 2
.LP

.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXBeginPickAll
	1	CARD8		opcode (X11 assigned)
	1	102		PEX opcode
	2	8+n		request length
	2	INT16		floating point format
	2	INT16		pick all method
		1 All
		2 Visible
	1	CARD8		send_event
		0 False
		1 True
	3			unused
	4	CARD32		renderer ID
	4	CARD32		drawable ID
	4	INT32		structure identifer (not resource ID)
	4	CARD32		pick max hits
	4+4n	PICK_RECORD		pick data
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXEndPickAll
	1	CARD8		opcode (X11 assigned)
	1	103		PEX opcode
	2	2		request length
	4	CARD32		renderer ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n+3m		reply length
				(let m be the number of element refs)
	4	n		number pick paths
	2			pick status
		0 NoPick
		1 Ok
		2 Aborted
	2			more picks
		0 MoreHits
		1 NoMoreHits
		2 MayBeMoreHits
	16			unused
	4n+12m	LISTofLISTofPICK_ELEMENT_REF	pick paths
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXPickAll
	1	CARD8		opcode (X11 assigned)
	1	104		PEX opcode
	2	6+n		request length
	2	INT16		floating point format
	2	INT16		pick all method
		1 All
		2 Visible
	4	CARD32		renderer ID
	4	CARD32		drawable ID
	4	CARD32		pick max hits
	4+4n	PICK_RECORD		pick data
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n+3m		reply length
				(let m be the number of element refs)
	4	n		number pick paths
	2			pick status
		0 NoPick
		1 Ok
		2 Aborted
	2			more picks
		0 MoreHits
		1 NoMoreHits
		2 MayBeMoreHits
	16			unused
	4n+12m	LISTofLISTofPICK_ELEMENT_REF	pick paths
.De
.bp

.AC "Structures" 1
.LP

.AC "Structure Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreateStructure
	1	CARD8		opcode (X11 assigned)
	1	30		PEX opcode
	2	2		request length
	4	CARD32		structure ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopyStructure
	1	CARD8		opcode (X11 assigned)
	1	31		PEX opcode
	2	3		request length
	4	CARD32		source structure ID
	4	CARD32		destination structure ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXDestroyStructures
	1	CARD8		opcode (X11 assigned)
	1	32		PEX opcode
	2	2+n		request length
	4	CARD32		number of structure IDs
	4n	LISTofCARD32		list of structure IDs
.De

.AC "Structure Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetStructureInfo
	1	CARD8		opcode (X11 assigned)
	1	33		PEX opcode
	2	3		request length
	2	INT16		floating point format
	2			item mask
		#x0001	element_ptr
		#x0002	num_elements
		#x0004	total_length
		#x0008	has_refs
		#x0010	editing_mode
	4	CARD32		structure ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	2			editing mode
		0	StructureInsert
		1	StructureReplace
	2			unused
	4	CARD32		element pointer
	4	CARD32		number of elements
	4	CARD32		total length
	2			has references
		0	False
		1	True
	6			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetElementInfo
	1	CARD8		opcode (X11 assigned)
	1	34		PEX opcode
	2	7		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		structure ID
	2			position1 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position1 offset
	2			position2 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position2 offset
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n		reply length
	4	n		number of element info
	20			unused
	4n	LISTofELEMENT_INFO		info
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetStructuresInNetwork
	1	CARD8		opcode (X11 assigned)
	1	35		PEX opcode
	2	3		request length
	4	CARD32		structure ID
	2			which
		0	All
		1	NoCrossRefs
	2			unused
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n		reply length
	8			unused
	4 	n		number of structures
	12			unused
	4n	LISTofSTRUCTURE_ID		structure IDs
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetAncestors
	1	CARD8		opcode (X11 assigned)
	1	36		PEX opcode
	2	4		request length
	4	CARD32		structure ID
	2			path part
		0	TopPart
		1	BottomPart
	2			unused
	4	CARD32		path depth
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n+2s		reply length
	12			unused
	4	n		number of element reference lists
	8	unused
				(let s be the sum of these n numbers)
	4n+8s	LISTofLISTofELEMENT_REF		paths
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetDescendants
	1	CARD8		opcode (X11 assigned)
	1	37		PEX opcode
	2	4		request length
	4	CARD32		structure ID
	2			path part
		0	TopPart
		1	BottomPart
	2			unused
	4	CARD32		path depth
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n+2s		reply length
	12			unused
	4	n		number of element reference lists
	8	unused
				(let s = sum of these n numbers)
	4n+8s	LISTofLISTofELEMENT_REF		paths
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFetchElements
	1	CARD8		opcode (X11 assigned)
	1	38		PEX opcode
	2	7		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		structure ID
	2			position1 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position1 offset
	2			position2 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position2 offset
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	s/4		reply length
	4	CARD32		number of output commands
	20			unused
				(let s be the total size of the output commands in bytes)
	s	LISTofOUTPUT_COMMAND		elements
.De

.AC "Structure Resource Attribute Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetEditingMode
	1	CARD8		opcode (X11 assigned)
	1	39		PEX opcode
	2	3		request length
	4	CARD32		structure ID
	2			mode
		0	StructureInsert
		1	StructureReplace
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetElementPointer
	1	CARD8		opcode (X11 assigned)
	1	40		PEX opcode
	2	4		request length
	4	CARD32		structure ID
	2			position whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetElementPointerAtLabel
	1	CARD8		opcode (X11 assigned)
	1	41		PEX opcode
	2	4		request length
	4	CARD32		structure ID
	4	INT32		label
	4	INT32		offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXElementSearch
	1	CARD8		opcode (X11 assigned)
	1	42		PEX opcode
	2	7+(2i+2e+p+p2)/4		request length
	4	CARD32		structure ID
	2			position whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position offset
	4			direction
		0	Forward
		1	Backward
	4	i		number of elements in inclusion list
	4	e		number of elements in exclusion list
	2i	LISTofCARD16		inclusion list
	p			unused, p=pad(2i)
	2e	LISTofCARD16		exclusion list
	p2			unused, p2=pad(2e)
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	2			status
		1	NotFound
		2	Found
	2			unused
	4	CARD32		found offset
	16			unused
.De

.AC "Structure Editing" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXStoreElements
	1	CARD8		opcode (X11 assigned)
	1	43		PEX opcode
	2	4+s/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		structure ID
	4	n		number of output commands
				(let s be the total size of the output commands in bytes)
	s	LISTofOUTPUT_COMMAND		elements
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXDeleteElements
	1	CARD8		opcode (X11 assigned)
	1	44		PEX opcode
	2	6		request length
	4	CARD32		structure ID
	2			position1 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position1 offset
	2			position2 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position2 offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXDeleteElementsToLabel
	1	CARD8		opcode (X11 assigned)
	1	45		PEX opcode
	2	5		request length
	4	CARD32		structure ID
	2			position whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		position offset
	4	INT32		label
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXDeleteElementsBetweenLabels
	1	CARD8		opcode (X11 assigned)
	1	46		PEX opcode
	2	4		request length
	4	CARD32		structure ID
	4	INT32		label1
	4	INT32		label2
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopyElements
	1	CARD8		opcode (X11 assigned)
	1	47		PEX opcode
	2	9		request length
	4	CARD32		source structure ID
	2			source position1 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		source position1 offset
	2			source position2 whence
		0	Beginning
		1	Current
		2	End
	2			unused
	4	INT32		source position2 offset
	4	CARD32		destination structure ID
	2			destination position whence
		0	Beginning
		1	Current
		2	End
	2 			unused
	4	INT32		destination position offset
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangeStructureReferences
	1	CARD8		opcode (X11 assigned)
	1	48		PEX opcode
	2	3		request length
	4	CARD32		old structure ID
	4	CARD32		new structure ID
.De
.bp

.AC "Name Sets" 1
.LP

.AC "Name Set Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreateNameSet
	1	CARD8		opcode (X11 assigned)
	1	49		PEX opcode
	2	2		request length
	4	CARD32		nameset ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopyNameSet
	1	CARD8		opcode (X11 assigned)
	1	50		PEX opcode
	2	3		request length
	4	CARD32		source nameset ID
	4	CARD32		destination nameset ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreeNameSet
	1	CARD8		opcode (X11 assigned)
	1	51		PEX opcode
	2	2		request length
	4	CARD32		nameset ID
.De

.AC "Name Set Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetNameSet
	1	CARD8		opcode (X11 assigned)
	1	52		PEX opcode
	2	2		request length
	4	CARD32		nameset ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n		reply length
	4	n		number of names
	20			unused
	4n	LISTofNAME		names
.De

.AC "Name Set Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangeNameSet
	1	CARD8		opcode (X11 assigned)
	1	53		PEX opcode
	2	3+n		request length
	4	CARD32		nameset ID
	2			action
		0	Add
		1	Remove
		2	Replace
	2			unused
	4n	LISTofNAME		names (length implicit)
.bp

.AC "Search Contexts" 1
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
SC_BITMASK
.sp 1
	#x0001	search_pos
	#x0002	search_dist
	#x0004	search_ceiling
	#x0008	model_clip_flag
	#x0010	start_path
	#x0020	normal_list
	#x0040	inverted_list
.De
.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
.sp 1
search_pos		3*\fIfp\fP
		\fIfp\fP	FLOAT		x
		\fIfp\fP	FLOAT		y
		\fIfp\fP	FLOAT		z
search_dist		\fIfp\fP	FLOAT
search_ceiling		4	CARD16
model_clip_flag		4	CARD8
			0	False
			1	True
start_path		4+8n
		4	n		length of LIST
		8n	LISTofELEMENT_REF
normal_list		4+8n
		4	n		length of LIST
		8n	LISTofNAME_SET_PAIR
inverted_list		4+8n
		4	n		length of LIST
		8n	LISTofNAME_SET_PAIR

.AC "Search Context Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreateSearchContext
	1	CARD8		opcode (X11 assigned)
	1	54		PEX opcode
	2	4+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		search context ID
	4	SC_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCopySearchContext
	1	CARD8		opcode (X11 assigned)
	1	55		PEX opcode
	2	4		request length
	4	CARD32		source search context ID
	4	CARD32		destination search context ID
	4	SC_BITMASK		item mask
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreeSearchContext
	1	CARD8		opcode (X11 assigned)
	1	56		PEX opcode
	2	2		request length
	4	CARD32		search context ID
.De

.AC "Search Context Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetSearchContext
	1	CARD8		opcode (X11 assigned)
	1	57		PEX opcode
	2	4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		search context ID
	4	SC_BITMASK		item mask
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Search Context Modification" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangeSearchContext
	1	CARD8		opcode (X11 assigned)
	1	58		PEX opcode
	2	4+n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		search context ID
	4	SC_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Structure Network Searching" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSearchNetwork
	1	CARD8		opcode (X11 assigned)
	1	59		PEX opcode
	2	2		request length
	4	CARD32		search context ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	2n		reply length
	4			unused
	4	n		number of element references
	16			unused
	8n	LISTofELEMENT_REF		found path
.De
.bp

.AC "PHIGS Workstations" 1
.LP

.AC "PHIGS Workstation Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreatePhigsWKS
	1	CARD8		opcode (X11 assigned)
	1	60		PEX opcode
	2	19		request length
	4	CARD32		PHIGS workstation ID
	4	CARD32		drawable ID
	4	CARD32		marker bundle ID
	4	CARD32		text bundle ID
	4	CARD32		line bundle ID
	4	CARD32		interior bundle ID
	4	CARD32		edge bundle ID
	4	CARD32		color table ID
	4	CARD32		depth cue table ID
	4	CARD32		light table ID
	4	CARD32		color approximation table ID
	4	CARD32		pattern table ID
	4	CARD32		text font table ID
	4	CARD32		highlight inclusion ID
	4	CARD32		highlight exclusion ID
	4	CARD32		invisibility inclusion ID
	4	CARD32		invisibility exclusion ID
	2			buffer mode
		0	Single
		1	Double
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreePhigsWKS
	1	CARD8		opcode (X11 assigned)
	1	61		PEX opcode
	2	2		request length
	4	CARD32		PHIGS workstation ID
.De

.AC "PHIGS Workstation Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetWKSInfo
	1	CARD8		opcode (X11 assigned)
	1	62		PEX opcode
	2	5		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	8	WKS_BITMASK	item mask (see below)
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (see below)
				(let n be the size of the list, in bytes.)
.LP
The following components may appear in the reply of \fBPEXGetWKSInfo\fP.
Because the components are not of a fixed size
(as they are in X11), the list must be parsed serially to know where one
component ends and the next begins.  A bitmap mask of two CARD32s in the
request specifies which components are to be found in the reply.
Proceding from the least
significant bit of the first CARD32 through to the last used bit of the
second CARD32, if the bit is set, the next item in the list is of the
specified type.
.sp 1
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
WKS_BITMASK
.sp 1
First CARD32:
	#x00000001	display_update
	#x00000002	visual_state
	#x00000004	display_surface
	#x00000008	view_update
	#x00000010	defined_views
	#x00000020	wks_update
	#x00000040	req_NPC_subvolume
	#x00000080	cur_NPC_subvolume
	#x00000100	req_wks_viewpt
	#x00000200	cur_wks_viewpt
	#x00000400	HLHSR_update
	#x00000800	req_HLHSR_mode
	#x00001000	cur_HLHSR_mode
	#x00002000	drawable_id
	#x00004000	marker_bundle
	#x00008000	text_bundle
	#x00010000	line_bundle
	#x00020000	interior_bundle
	#x00040000	edge_bundle
	#x00080000	color_table
	#x00100000	depth_cue_table
	#x00200000	light_table
	#x00400000	color_approx_table
	#x00800000	pattern_table
	#x01000000	text_font_table
	#x02000000	highlight_inclusion
	#x04000000	highlight_exclusion
	#x08000000	invisibility_inclusion
	#x10000000	invisibility_exclusion
	#x20000000	posted_structs
	#x40000000	num_priorities
	#x80000000	buffer_update
Second CARD32:
	#x00000001	req_buffer_mode
	#x00000002	cur_buffer_mode
.De
.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
.sp 1
display_update		4	INT16
visual_state		4	CARD8
			0	Correct
			1	Deferred
			2	Simulated
display_surface		4	CARD8
			0	Empty
			1	NotEmpty
view_update		4	CARD8
			0	NotPending
			1	Pending
defined_views		4+4n
		4	n		length
		4n	LISTofCARD16
wks_update		4	CARD8
			0	NotPending
			1	Pending
req_NPC_subvolume		6*\fIfp\fP
		\fIfp\fP	FLOAT		min x
		\fIfp\fP	FLOAT		min y
		\fIfp\fP	FLOAT		min z
		\fIfp\fP	FLOAT		max x
		\fIfp\fP	FLOAT		max y
		\fIfp\fP	FLOAT		max z
cur_NPC_subvolume		6*\fIfp\fP
		\fIfp\fP	FLOAT		min x
		\fIfp\fP	FLOAT		min y
		\fIfp\fP	FLOAT		min z
		\fIfp\fP	FLOAT		max x
		\fIfp\fP	FLOAT		max y
		\fIfp\fP	FLOAT		max z
req_wks_viewpt		12+2*\fIfp\fP
		2	INT16		min x
		2	INT16		min y
		\fIfp\fP	FLOAT		min z
		2	INT16		max x
		2	INT16		max y
		\fIfp\fP	FLOAT		max z
		1	CARD8		use drawable
			0	False
			1	True
		3			unused
cur_wks_viewpt		12+2*\fIfp\fP
		2	INT16		min x
		2	INT16		min y
		\fIfp\fP	FLOAT		min z
		2	INT16		max x
		2	INT16		max y
		\fIfp\fP	FLOAT		max z
		1	CARD8		use drawable
			0	False
			1	True
		3			unused
HLHSR_update		4	CARD8
			0	NotPending
			1	Pending
req_HLHSR_mode		4	INT16
cur_HLHSR_mode		4	INT16
drawable_id		4	CARD32
marker_bundle		4	CARD32
text_bundle		4	CARD32
line_bundle		4	CARD32
interior_bundle		4	CARD32
edge_bundle		4	CARD32
color_table		4	CARD32
depth_cue_table		4	CARD32
light_table		4	CARD32
color_approx_table		4	CARD32
pattern_table		4	CARD32
text_font_table		4	CARD32
highlight_inclusion		4	CARD32
highlight_exclusion		4	CARD32
invisibility_inclusion		4	CARD32
invisibility_exclusion		4	CARD32
posted_structs		4+(4+\fIfp\fP)*n
		4	n		length of list
		(4+\fIfp\fP)*n	LISTofSTRUCTURE_INFO
num_priorities		4	CARD32
buffer_update		4	CARD8
			0	NotPending
			1	Pending
req_buffer_mode		4	CARD16
			0	Single
			1	Double
cur_buffer_mode		4	CARD16
			0	Single
			1	Double
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetDynamics
	1	CARD8		opcode (X11 assigned)
	1	63		PEX opcode
	2	2		request length
	4	CARD32		drawable ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	0		reply length
	1			view rep
	1			marker bundle
	1			text bundle
	1			line bundle
	1			interior bundle
	1			edge bundle
	1			color table
	1			pattern table
	1			workstation transform
	1			highlight filter
	1			invisibility filter
	1			HLHSR mode
	1			structure modify
	1			post structure
	1			unpost structure
	1			delete structure
	1			reference modify
	1			buffer modify
	1			light table
	1			depth cue table
	1			color approx table
	3			unused
.De
.LP
The following constants are used for the reply fields above:
.LP
.Ds 0
.TA .5i 1i
.ta .5i 1i
.R
	0	IMM
	1	IRG
	2	CBS
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetViewRep
	1	CARD8		opcode (X11 assigned)
	1	64		PEX opcode
	2	3		request length
	2	INT16		floating point format
	2	CARD16		view index
	4	CARD32		PHIGS workstation ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	4+76*\fIfp\fP		reply length
	2			view update
		0	NotPending
		1	Pending
	22			unused
	2	CARD16		requested view index
	2			unused
	2			requested clip flags
		#x0001	clip_xy
		#x0002	clip_back
		#x0004	clip_front
	2			unused
	\fIfp\fP	FLOAT		requested clip limits min x
	\fIfp\fP	FLOAT		requested clip limits min y
	\fIfp\fP	FLOAT		requested clip limits min z
	\fIfp\fP	FLOAT		requested clip limits max x
	\fIfp\fP	FLOAT		requested clip limits max y
	\fIfp\fP	FLOAT		requested clip limits max z
	16*\fIfp\fP	FLOAT[4][4]		requested orientation
	16*\fIfp\fP	FLOAT[4][4]		requested mapping
	2	CARD16		current view index
	2			unused
	2			current clip flags
		#x0001	clip_xy
		#x0002	clip_back
		#x0004	clip_front
	2			unused
	\fIfp\fP	FLOAT		current clip limits min x
	\fIfp\fP	FLOAT		current clip limits min y
	\fIfp\fP	FLOAT		current clip limits min z
	\fIfp\fP	FLOAT		current clip limits max x
	\fIfp\fP	FLOAT		current clip limits max y
	\fIfp\fP	FLOAT		current clip limits max z
	16*\fIfp\fP	FLOAT[4][4]		current orientation
	16*\fIfp\fP	FLOAT[4][4]		current mapping
.De

.AC "PHIGS Workstation Manipulation" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXRedrawAllStructures
	1	CARD8		opcode (X11 assigned)
	1	65		PEX opcode
	2	2		request length
	4	CARD32		PHIGS workstation ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXUpdateWorkstation
	1	CARD8		opcode (X11 assigned)
	1	66		PEX opcode
	2	2		request length
	4	CARD32		PHIGS workstation ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXRedrawClipRegion
	1	CARD8		opcode (X11 assigned)
	1	67		PEX opcode
	2	3+2n		request length
	4	CARD32		PHIGS workstation ID
	4	n		number of device rectangles
	8n	LISTofDEVICE_RECT		clip list
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXExecuteDeferredActions
	1	CARD8		opcode (X11 assigned)
	1	68		PEX opcode
	2	2		request length
	4	CARD32		PHIGS workstation ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetViewPriority
	1	CARD8		opcode (X11 assigned)
	1	69		PEX opcode
	2	4		request length
	4	CARD32		PHIGS workstation ID
	2	CARD16		index1
	2	CARD16		index2
	2			priority
		0	Higher
		1	Lower
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetDisplayUpdateMode
	1	CARD8		opcode (X11 assigned)
	1	70		PEX opcode
	2	3		request length
	4	CARD32		PHIGS workstation ID
	2	INT16		display update
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXMapDCtoWC
	1	CARD8		opcode (X11 assigned)
	1	71		PEX opcode
	2	4+(4+\fIfp\fP)*n/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	4	n		number of coordinates
	(4+\fIfp\fP)*n	LISTofDEVICE_COORD		DC points
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	3*\fIfp\fP*n/4		reply length
	2	CARD16		view index
	2			unused
	4	n		number of coordinates being returned
	16			unused
	3*\fIfp\fP*n	LISTofCOORD_3D		WC points
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXMapWCtoDC
	1	CARD8		opcode (X11 assigned)
	1	72		PEX opcode
	2	4+3*\fIfp\fP*n/4		request length
	2	INT16		floating point format
	2	CARD16		view index
	4	CARD32		PHIGS workstation ID
	4	n		number of coordinates
	3*\fIfp\fP*n	LISTofCOORD_3D		WC points
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	(4+\fIfp\fP)*n/4		reply length
	4			unused
	4	n		number of coordinates being returned
	16			unused
	(4+\fIfp\fP)*n	LISTofDEVICE_COORD		DC points
.De

.AC "PHIGS Workstation Update" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetViewRep
	1	CARD8		opcode (X11 assigned)
	1	73		PEX opcode
	2	5+38*\fIfp\fP/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	2	CARD16		view rep index
	2			unused
	2			view rep clip flags
		#x0001	clip_xy
		#x0002	clip_back
		#x0004	clip_front
	2			unused
	\fIfp\fP	FLOAT		view rep clip limits min x
	\fIfp\fP	FLOAT		view rep clip limits min y
	\fIfp\fP	FLOAT		view rep clip limits min z
	\fIfp\fP	FLOAT		view rep clip limits max x
	\fIfp\fP	FLOAT		view rep clip limits max y
	\fIfp\fP	FLOAT		view rep clip limits max z
	16*\fIfp\fP	FLOAT[4][4]		view rep orientation
	16*\fIfp\fP	FLOAT[4][4]		view rep mapping
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetWKSWindow
	1	CARD8		opcode (X11 assigned)
	1	74		PEX opcode
	2	3+6*\fIfp\fP/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	\fIfp\fP	FLOAT		NPC subvolume min x
	\fIfp\fP	FLOAT		NPC subvolume min y
	\fIfp\fP	FLOAT		NPC subvolume min z
	\fIfp\fP	FLOAT		NPC subvolume max x
	\fIfp\fP	FLOAT		NPC subvolume max y
	\fIfp\fP	FLOAT		NPC subvolume max z
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetWKSViewport
	1	CARD8		opcode (X11 assigned)
	1	75		PEX opcode
	2	6+2*\fIfp\fP/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	2	INT16		viewport min x
	2	INT16		viewport min y
	\fIfp\fP	FLOAT		viewport min z
	2	INT16		viewport max x
	2	INT16		viewport max y
	\fIfp\fP	FLOAT		viewport max z
	1			viewport use drawable
		0	False
		1	True
	3			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetHLHSRMode
	1	CARD8		opcode (X11 assigned)
	1	76		PEX opcode
	2	3		request length
	4	CARD32		PHIGS workstation ID
	2	INT16		mode
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetWKSBufferMode
	1	CARD8		opcode (X11 assigned)
	1	77		PEX opcode
	2	3		request length
	4	CARD32		PHIGS workstation ID
	2			buffer mode
		0	Single
		1	Double
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.AC "Posting/Unposting Structures" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXPostStructure
	1	CARD8		opcode (X11 assigned)
	1	78		PEX opcode
	2	4+\fIfp\fP/4		request length
	2	INT16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	4	CARD32		structure ID
	\fIfp\fP	FLOAT		priority
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXUnpostStructure
	1	CARD8		opcode (X11 assigned)
	1	79		PEX opcode
	2	3		request length
	4	CARD32		PHIGS workstation ID
	4	CARD32		structure ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXUnpostAllStructures
	1	CARD8		opcode (X11 assigned)
	1	80		PEX opcode
	2	2		request length
	4	CARD32		PHIGS workstation ID
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetWKSPostings
	1	CARD8		opcode (X11 assigned)
	1	81		PEX opcode
	2	2		request length
	4	CARD32		structure ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n		reply length
	24			unused
	4n	LISTofPHIGS_WKS_ID		PHIGS workstation IDs (length implicit)
.De
.bp

.AC "Workstation Picking" 1
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
P_BITMASK
.sp 1
	#x0001	pick_status
	#x0002	pick_path
	#x0004	pick_path_order
	#x0008	pick_inclusion
	#x0010	pick_exclusion
	#x0020	pick_data_rec
	#x0040	prompt_echo_type
	#x0080	echo_volume
	#x0100	echo_switch
.De
.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
pick_status		4	CARD16
			0	NoPick
			1	Ok
pick_path		4+12n
		4	n		number of pick element references
		12n	LISTofPICK_ELEMENT_REF
pick_path_order		4	CARD16
			0	TopFirst
			1	BottomFirst
pick_inclusion		4	CARD32
pick_exclusion		4	CARD32
pick_data_rec		4+n+p
		4	n		length
		n	LISTofCARD8
		p			unused, p=pad(n)
prompt_echo_type		4	INT16
echo_volume		12+2*\fIfp\fP
		2	INT16		min x
		2	INT16		min y
		\fIfp\fP	FLOAT		min z
		2	INT16		max x
		2	INT16		max y
		\fIfp\fP	FLOAT		max z
		1	CARD8		use drawable
			0	False
			1	True
		3			unused
echo_switch		4	CARD16
			0	NoEcho
			1	Echo
.De

.AC "Pick Device Descriptors" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetPickDevice
	1	CARD8		opcode (X11 assigned)
	1	82		PEX opcode
	2	4		request length
	2	INT16		floating point format
	2	INT16		device type
	4	CARD32		PHIGS workstation ID
	4	P_BITMASK		item mask
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXChangePickDevice
	1	CARD8		opcode (X11 assigned)
	1	83		PEX opcode
	2	5+n/4		request length
	2	CARD16		floating point format
	2			unused
	4	CARD32		PHIGS workstation ID
	2	INT16		device type
	2			unused
	4	P_BITMASK		item mask
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Pick Measure" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCreatePickMeasure
	1	CARD8		opcode (X11 assigned)
	1	84		PEX opcode
	2	4		request length
	4	CARD32		PHIGS workstation ID
	4	CARD32		pick measure ID
	2	INT16		device type
	2			unused
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXFreePickMeasure
	1	CARD8		opcode (X11 assigned)
	1	85		PEX opcode
	2	2		request length
	4	CARD32		pick measure ID
.De
.LP
.Ds 0
.TA .2i 1i
.ta .2i 1i
PM_BITMASK
.sp 1
	#x0001	pick_status
	#x0002	picked_prim
.De
.LP
Note that entries in the list are all padded to a multiple of 4 bytes.  
If values appear which could be encoded in less space, it is assumed 
that they will be placed in the least significant bits of the word.  
If a component has sub-fields, the sizes, types and interpretations of the
sub-fields are on subsequent lines.
.LP
.Ds 0
.TA 2i 2.3i 2.9i 3.6i 4.2i
.ta 2i 2.3i 2.9i 3.6i 4.2i
.R
\fBAttribute Name		Length	Type\fP
.sp 1
pick_status		4	CARD16
			0	NoPick
			1	Ok
picked_prim		4+12n
		4	n		number of pick element references
		12n	LISTofPICK_ELEMENT_REF
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXGetPickMeasure
	1	CARD8		opcode (X11 assigned)
	1	86		PEX opcode
	2	3		request length
	4	CARD32		pick measure ID
	4	PM_BITMASK		item mask
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	n/4		reply length
	24			unused
	n	LISTofVALUE		item list (let n be the size of the list, in bytes.)
.De

.AC "Pick Operations" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXUpdatePickMeasure
	1	CARD8		opcode (X11 assigned)
	1	87		PEX opcode
	2	3+(n+p)/4		request length
	4	CARD32		pick measure ID
	4	n		length of list
	n	LISTofCARD8		input record
	p			unused, p=pad(n)
.De
.sp 1
The input data records for the registered pick devices are:
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Pick Device 1, DC_HitBox:
	2	INT16		pick position x
	2	INT16		pick position y
	\fIfp\fP	FLOAT		pick distance
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
Pick Device 2, NPC_HitVolume:
	\fIfp\fP	FLOAT		min x
	\fIfp\fP	FLOAT		min y
	\fIfp\fP	FLOAT		min z
	\fIfp\fP	FLOAT		max x
	\fIfp\fP	FLOAT		max y
	\fIfp\fP	FLOAT		max z
.De
.bp

.AC "PEX Fonts" 1
.LP

.AC "PEX Font Resource Management" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXOpenFont
	1	CARD8		opcode (X11 assigned)
	1	88		PEX opcode
	2	3+(n+p)/4		request length
	4	CARD32		font ID
	4	n		length of name
	n	LISTofCARD8		name
	p			unused, p=pad(n)
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXCloseFont
	1	CARD8		opcode (X11 assigned)
	1	89		PEX opcode
	2	2		length
	4	CARD32		font ID
.De

.AC "PEX Font Inquiry" 2
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXQueryFont
	1	CARD8		opcode (X11 assigned)
	1	90		PEX opcode
	2	2		request length
	4	CARD32		font ID
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	f/4		reply length
	4	f		length of FONTINFO
	20			unused
	f	FONTINFO		font info
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXListFonts
	1	CARD8		opcode (X11 assigned)
	1	91		PEX opcode
	2	2+(n+p)/4		request length
	2	CARD16		max names
	2	n		number of bytes in pattern
	n	LISTofCARD8		pattern
	p			unused, p=pad(n)
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	r/4		reply length
	4	CARD32		number of STRINGs in list
	20			unused
	r	LISTofSTRING		font names
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXListFontsWithInfo
	1	CARD8		opcode (X11 assigned)
	1	92		PEX opcode
	2	3+(n+p)/4		request length
	2			unused
	2	CARD16		max names
	2	n		number of characters in pattern
	2			unused
	n	LISTofCARD8		pattern
	p			unused, p=pad(n)
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	1+(r+f)/4		reply length
	4	CARD32		number of STRINGs in list
	20			unused
	r	LISTofSTRING		font names
	4	CARD32		number of FONTINFO in list
	f	LISTofFONTINFO		fonts
.De
.LP
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXQueryTextExtents
	1	CARD8		opcode (X11 assigned)
	1	93		PEX opcode
	2	6+(3*\fIfp\fP+q+p)/4		request length
	2	INT16		floating point format
	2			path
		0	PathRight
		1	PathLeft
		2	PathUp
		3	PathDown
	2	CARD16		font group table index
	2			unused
	4	CARD32		resource ID (renderer, WKS or TextFont table)
	\fIfp\fP	FLOAT		expansion
	\fIfp\fP	FLOAT		spacing
	\fIfp\fP	FLOAT		height
	2			alignment vertical
		0	ValignNormal
		1	ValignTop
		2	ValignCap
		3	ValignHalf
		4	ValignBase
		5	ValignBottom
	2			alignment horizontal
		0	HalignNormal
		1	HalignLeft
		2	HalignCenter
		3	HalignRight
	4	CARD32		number of strings
	q	LISTofLISTofMONO_ENCODING		strings
.De
.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
=>
	1	1		Reply
	1			unused
	2	CARD16		sequence number
	4	6n		reply length
	24			unused
	6*\fIfp\fP*n	LISTofEXTENT_INFO		extents (length implicit)
.De
.bp

.AP "Appendix: Registered PEX Escapes"
.SH
\s+3Appendix: Registered PEX Escapes\s-3
.LP
.RU
.LP

.Ds 0
.TA .2i 1i 2i 3.1i
.ta .2i 1i 2i 3.1i
.R
.PN PEXSetEchoColor
	1	CARD8		opcode (X11 assigned)
	1	95		PEX opcode (see PEXEscape)
	2	5+n		request length
	4	1		escape ID
	2	INT16		floating point format
	2			unused
	4	CARD32		renderer ID
	4+4n	COLOR_SPECIFIER		color
.De
