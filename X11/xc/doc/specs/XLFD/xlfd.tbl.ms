.\" Use tbl and -ms and macros.t	-*- Nroff -*-
.\" $Xorg: xlfd.tbl.ms,v 1.3 2000/08/17 19:42:22 cpqbld Exp $
.nr sM 4		\" section on Matrix Transformations
.nr sS 5		\" section on Scalable fonts
.nr sP 6		\" section on Polymorphic font support
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 11
.nr PS 11
.if n .nr LL 72m
\&
.sp 8
.ce 100
\s+2\fBX Logical Font Description Conventions\fP\s-2

\fBVersion 1.5\fP

\fBX Consortium Standard\fP

\fBX Version 11, Release 6.4\fP
.sp 6
\s+1Jim Flowers\s-1
.sp 6p
\s+1Digital Equipment Corporation\s-1
.sp 6
\s+1Version 1.5 edited by Stephen Gildea\s0
.sp 6p
\s+1X Consortium, Inc.\s0
.ce 0
.bp
\&
.ps 9
.nr PS 9
.sp 8
.LP
\fIX Window System\fP is a trademark of X Consortium, Inc.
.LP             
Helvetica and Times are registered trademarks of Linotype Company.
.LP
ITC Avant Garde Gothic is a registered trademark of International 
Typeface Corporation.
.LP
Times Roman is a registered trademark of Monotype Corporation.
.LP
Bitstream Amerigo is a registered trademark of Bitstream Inc.
.LP             
Stone is a registered trademark of Adobe Systems Inc.
.LP
Copyright \(co 1988, 1994 X Consortium
.LP
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
\*QSoftware\*U), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
.LP
The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.
.LP
THE SOFTWARE IS PROVIDED \*QAS IS\*U, WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.
.LP
Copyright \(co 1988, 1989 
Digital Equipment Corporation, Maynard MA.  All rights reserved.
.LP 
Permission to use, copy, modify, and distribute this documentation 
for any purpose and without fee is hereby granted, provided 
that the above copyright notice and this permission 
notice appear in all copies.
Digital Equipment Corporation makes no representations
about the 
suitability for any purpose of the information in this document. 
This documentation is provided as is without express or implied warranty. 
.ps 11
.nr PS 11
.bp 1
.EH '\fBX Logical Font Description Conventions\fP''\fBX11, Release 6.4\fP'
.OH '\fBX Logical Font Description Conventions\fP''\fBX11, Release 6.4\fP'
.EF ''\fB\\\\n(PN\fP''
.OF ''\fB\\\\n(PN\fP''
.NH 1 
Introduction
.XS
\*(SN Introduction
.XE
.LP 
It is a requirement that X client applications must be portable across server
implementations, with very different file systems, naming conventions, and 
font libraries.
However, font access requests, 
as defined by the \fIX Window System Protocol\fP,
neither specify server-independent conventions for font names 
nor provide adequate font properties for logically describing typographic fonts.
.LP
X clients must be able to dynamically determine the fonts available 
on any given server so that understandable information can be presented 
to the user or so that intelligent font fallbacks can be chosen.
It is desirable for the most common queries to be accomplished 
without the overhead of opening each font and inspecting font properties, 
by means of simple 
.PN ListFonts 
requests.
For example, if a user selected a Helvetica typeface family, 
a client application should be able to query the server 
for all Helvetica fonts and present only those setwidths, weights, slants, 
point sizes, and character sets available for that family.
.LP
This document gives a standard logical font description 
(hereafter referred to as XLFD) and the conventions to be used 
in the core protocol so that clients can query and access screen type libraries
in a consistent manner across all X servers.
In addition to completely specifying a given font by means of its 
.PN FontName ,
the XLFD also provides for a standard set of key 
.PN FontProperties
that describe the font in more detail.
.LP
The XLFD provides an adequate set of typographic font properties, 
such as \s-1CAP_HEIGHT\s+1, \s-1X_HEIGHT\s+1,
and \s-1RELATIVE_SETWIDTH\s+1, 
for publishing and other applications to do intelligent font matching 
or substitution when handling documents created on some foreign server 
that use potentially unknown fonts.
In addition, 
this information is required by certain clients 
to position subscripts automatically and determine small capital heights, 
recommended leading, word-space values, and so on.
.NH 1
Requirements and Goals
.XS
\*(SN Requirements and Goals
.XE
.LP 
The XLFD meets the short-term and long-term goals to have a 
standard logical font description that:
.IP \(bu 5
Provides unique, descriptive font names that support simple pattern
matching
.IP \(bu 5
Supports multiple font vendors, arbitrary character sets, and encodings
.IP \(bu 5
Supports naming and instancing of scalable and polymorphic fonts
.IP \(bu 5
Supports transformations and subsetting of fonts
.IP \(bu 5
Is independent of X server and operating or file system implementations
.IP \(bu 5
Supports arbitrarily complex font matching or substitution
.IP \(bu 5
Is extensible
.NH 2
Provide Unique and Descriptive Font Names
.XS
\*(SN Provide Unique and Descriptive Font Names
.XE
.LP
It should be possible to have font names that are long enough and 
descriptive enough to have a reasonable probability of being unique 
without inventing a new registration organization.
Resolution and size-dependent font masters, multivendor font libraries, 
and so on must be anticipated and handled by the font name alone.
.LP
The name itself should be structured to be amenable to simple pattern 
matching and parsing, thus allowing X clients to restrict font queries to 
some subset of all possible fonts in the server.
.NH 2
Support Multiple Font Vendors and Character Sets
.XS
\*(SN Support Multiple Font Vendors and Character Sets
.XE
.LP
The font name and properties should distinguish between fonts 
that were supplied by different font vendors 
but that possibly share the same name.
We anticipate a highly competitive font market where users will be able to 
buy fonts from many sources according to their particular requirements.
.LP
A number of font vendors deliver each font with all glyphs designed for that
font, where charset mappings are defined by encoding vectors.
Some server implementations may force these mappings to proprietary 
or standard charsets statically in the font data.
Others may desire to perform the mapping dynamically in the server.
Provisions must be made in the font name 
that allows a font request to specify or identify specific charset mappings 
in server environments where multiple charsets are supported.
.NH 2
Support Scalable and Polymorphic Fonts
.XS
\*(SN Support Scalable and Polymorphic Fonts
.XE
.LP
If a font source can be scaled to an arbitrary size or varied in other
ways, it should be possible for an application to determine
that fact from the font name, and the
application should be able to construct a font name for any specific
instance.
.NH 2
Support Transformations and Subsetting of Fonts
.XS
\*(SN Support Transformations and Subsetting of Fonts
.XE
.LP
Arbitrary two-dimensional linear transformations of fonts should be
able to be requested by applications.  Since such transformed fonts
may be used for special effects requiring a few characters from each
of many differently transformed fonts, it should be possible to
request only a few characters from a font for efficiency.
.NH 2
Be Independent of X Server and Operating or File System Implementations
.XS
\*(SN Be Independent of X Server and Operating or File System Implementations
.XE
.LP
X client applications that require a particular font should be able to use 
the descriptive name without knowledge of the file system or other 
repository in use by the server.
However, 
it should be possible for servers to translate a given font name 
into a file name syntax that it knows how to deal with,
without compromising the uniqueness of the font name.
This algorithm should be reversible (exactly how this translation is done is 
implementation dependent).
.NH 2
Support Arbitrarily Complex Font Matching and Substitution
.XS
\*(SN Support Arbitrarily Complex Font Matching and Substitution
.XE
.LP
In addition to the font name, 
the XLFD should define a standard list of descriptive font properties,
with agreed-upon fallbacks for all fonts.
This allows client applications to derive font-specific formatting 
or display data and to perform font matching or substitution 
when asked to handle potentially unknown fonts, as required.
.NH 2
Be Extensible
.XS
\*(SN Be Extensible
.XE
.LP
The XLFD must be extensible so that new and/or private descriptive font 
properties can be added to conforming fonts without making existing 
X client or server implementations obsolete.
.NH 1
X Logical Font Description
.XS
\*(SN X Logical Font Description
.XE
.LP
XLFD is divided into two basic components: 
the 
.PN FontName , 
which gives all font information needed to uniquely identify a font 
in X protocol requests (for example,
.PN OpenFont , 
.PN ListFonts , 
and so on) and a variable list of optional 
.PN FontProperties ,
which describe a font in more detail.
.LP
The 
.PN FontName 
is used in font queries and is returned as data in certain X protocol requests.
It is also specified as the data value for the 
.PN FONT
item in the X Consortium Character Bitmap Distribution Format Standard
(BDF V2.1).
.LP
The 
.PN FontProperties 
are supplied on a font-by-font basis and are returned 
as data in certain X protocol requests as part of the 
.PN XFontStruct
data structure.
The names and associated data values for each of the 
.PN FontProperties 
may also appear as items of the 
\s-1\fBSTARTPROPERTIES\fP\s+1...\s-1\fBENDPROPERTIES\fP\s+1 list 
in the BDF V2.1 specification.
.NH 2
FontName
.XS
\*(SN FontName
.XE
.LP
Each 
.PN FontName 
is logically composed of two strings: a 
.PN FontNameRegistry
prefix that is followed by a 
.PN FontNameSuffix .
The
.PN FontName
uses the ISO 8859-1 encoding.
The 
.PN FontNameRegistry
is an
.IN x-registered-name
x-registered-name (a name that has been registered with the X Consortium)
that identifies the registration authority that owns the specified 
.PN FontNameSuffix
syntax and semantics.
.LP
All font names that conform to this specification are to use a 
.PN FontNameRegistry
prefix, which is defined to be the string \*Q\-\*U
(HYPHEN).
All 
.PN FontNameRegistry 
prefixes of the form: +\fIversion\fP\-,
where the specified version indicates some future XLFD specification, 
are reserved by the X Consortium for future extensions to XLFD font names.
If required, extensions to the current XLFD font name shall be constructed 
by appending new fields to the current structure, 
each delimited by the existing field delimiter.
The availability of other 
.PN FontNameRegistry
prefixes or fonts that support other registries 
is server implementation dependent.
.LP
In the X protocol specification, 
the 
.PN FontName 
is required to be a string; 
hence, numeric field values are represented in the name as string equivalents.
All 
.PN FontNameSuffix 
fields are also defined as 
.PN FontProperties ; 
numeric property values are represented as signed or unsigned integers,
as appropriate.
.NH 3
FontName Syntax
.XS
\*(SN FontName Syntax
.XE
.LP
The
.PN FontName 
is a structured, parsable string (of type STRING8) 
whose Backus-Naur Form syntax description is as follows:
.IN "FontName Syntax"
.SM
.TS 
rw(1.5i) lw(3.75i).
.sp 6p
T{
FontName ::=
T}	T{
XFontNameRegistry XFontNameSuffix | 
PrivFontNameRegistry PrivFontNameSuffix
T}
T{
XFontNameRegistry ::=
T}	T{
XFNDelim | XFNExtPrefix Version XFNDelim
T}
T{
XFontNameSuffix ::=
T}	T{
FOUNDRY XFNDelim FAMILY_NAME XFNDelim WEIGHT_NAME
XFNDelim SLANT XFNDelim SETWIDTH_NAME XFNDelim ADD_
STYLE_NAME XFNDelim PIXEL_SIZE XFNDelim POINT_SIZE 
XFNDelim RESOLUTION_X XFNDelim RESOLUTION_Y XFNDelim 
SPACING XFNDelim AVERAGE_WIDTH XFNDelim CHARSET_REGISTRY
XFNDelim CHARSET_ENCODING
T}
T{
Version ::=
T}	T{
STRING8 \- the XLFD version that defines an extension 
to the font name syntax (for example, \*Q1.4\*U)
T}
XFNExtPrefix ::=	OCTET \- \*Q+\*U (PLUS)
XFNDelim ::=	OCTET \- \*Q\-\*U (HYPHEN)
T{
PrivFontNameRegistry ::=
T}	T{
STRING8 \- other than those strings reserved by XLFD
T}
PrivFontNameSuffix ::=	STRING8
.TE
.NL
.LP
Field values are constructed as strings of ISO 8859-1 graphic characters, 
excluding the following:
.IP \(bu 5
\*Q\-\*U (HYPHEN), the XLFD font name delimiter character
.IP \(bu 5
\*Q?\*U (QUESTION MARK) and \*Q*\*U (ASTERISK), the X protocol 
font name wildcard characters
.IP \(bu 5
\*Q\^,\^\*U (COMMA), used by Xlib to separate XLFD font names in a font set.
.IP \(bu 5
\*Q\fC"\fP\*U (QUOTATION MARK), used by some commercial products to quote a
font name.
.LP
Alphabetic case distinctions are allowed but are for human readability 
concerns only.
Conforming X servers will perform matching on font name query or open requests 
independent of case.
The entire font name string must have no more than 255 characters.
It is recommended that clients construct font name query patterns 
by explicitly including all field delimiters to avoid unexpected results.
Note that SPACE is a valid character of a 
.PN FontName 
field; for example, the string \*QITC Avant Garde Gothic\*U might be a
FAMILY_NAME.
.NH 3
FontName Field Definitions
.XS
\*(SN FontName Field Definitions
.XE
.LP
This section discusses the
.PN FontName :
.IP \(bu 5
FOUNDRY field
.IP \(bu 5
FAMILY_NAME field
.IP \(bu 5
WEIGHT_NAME field
.IP \(bu 5
SLANT field
.IP \(bu 5
SETWIDTH_NAME field
.IP \(bu 5
ADD_STYLE_NAME field
.IP \(bu 5
PIXEL_SIZE field
.IP \(bu 5
POINT_SIZE field
.IP \(bu 5
RESOLUTION_X and RESOLUTION_Y fields
.IP \(bu 5
SPACING field
.IP \(bu 5
AVERAGE_WIDTH field
.IP \(bu 5
CHARSET_REGISTRY and CHARSET_ENCODING fields
.NH 4
FOUNDRY Field
.XS
\*(SN FOUNDRY Field
.XE
.LP
FOUNDRY is an x-registered-name,
the name or identifier of the digital type foundry 
that digitized and supplied the font data, 
or if different, the identifier of the organization that last modified 
the font shape or metric information.
.LP
The reason this distinction is necessary is 
that a given font design may be licensed from one source (for example, ITC) 
but digitized and sold by any number of different type suppliers.
Each digital version of the original design, in general, will be somewhat 
different in metrics and shape from the idealized original font data, 
because each font foundry, for better or for worse, has its own standards 
and practices for tweaking a typeface for a particular generation 
of output technologies or has its own perception of market needs.
.LP
It is up to the type supplier to register with the X Consortium a 
suitable name for this 
.PN FontName 
field according to the registration procedures defined by the Consortium.
.LP
The X Consortium shall define procedures for registering foundry 
and other names and shall maintain and publish, 
as part of its public distribution, 
a registry of such registered names for use in XLFD font names and properties.
.LP
.NH 4
FAMILY_NAME Field
.XS
\*(SN FAMILY_NAME Field
.XE
.LP
FAMILY_NAME is a string that identifies the range or family of 
typeface designs that are all variations of one basic typographic style.
This must be spelled out in full,
with words separated by spaces, as required.
This name must be human-understandable and suitable for presentation to a 
font user to identify the typeface family.
.LP
It is up to the type supplier to supply and maintain a suitable string for 
this field and font property, to secure the proper legal title to a given 
name, and to guard against the infringement of other's copyrights or 
trademarks.
By convention, FAMILY_NAME is not translated.
FAMILY_NAME may include an indication of design ownership 
if considered a valid part of the 
typeface family name.
.LP
The following are examples of FAMILY_NAME:
.IP \(bu 5
Helvetica
.IP \(bu 5
ITC Avant Garde Gothic 
.IP \(bu 5
Times
.IP \(bu 5
Times Roman
.IP \(bu 5
Bitstream Amerigo
.IP \(bu 5
Stone
.NH 4
WEIGHT_NAME Field
.XS
\*(SN WEIGHT_NAME Field
.XE
.LP
WEIGHT_NAME is a string that identifies the font's typographic weight, 
that is, the nominal blackness of the font, 
according to the FOUNDRY's judgment.
This name must be human-understandable and suitable for presentation to a 
font user.
The value \*Q0\*U is used to indicate a polymorphic font (see section \n(sP).
.LP
The interpretation of this field is somewhat problematic 
because the typographic judgment of weight has traditionally 
depended on the overall design of the typeface family in question;
that is, it is possible that the DemiBold weight of one font could be 
almost equivalent in typographic feel to a Bold font from another family.
.LP
WEIGHT_NAME is captured as an arbitrary string 
because it is an important part of a font's complete human-understandable name.
However, it should not be used for font matching or substitution.
For this purpose,
X client applications should use the weight-related font properties 
(RELATIVE_WEIGHT and WEIGHT) that give the coded relative weight 
and the calculated weight, respectively.
.NH 4
SLANT Field
.XS
\*(SN SLANT Field
.XE
.LP
SLANT is a code-string that indicates the overall posture of the 
typeface design used in the font.
The encoding is as follows:
.TS H
lw(.5i) lw(1.25i) lw(3.5i).
_
.sp 6p
.B
Code	English Translation	Description
.sp 6p
_
.sp 6p
.TH
.R
\*QR\*U	Roman	Upright design
\*QI\*U	Italic	T{
Italic design, slanted clockwise from the vertical
T}
\*QO\*U	Oblique	T{
Obliqued upright design, slanted clockwise from the vertical
T}
\*QRI\*U	Reverse Italic	T{
Italic design, slanted counterclockwise from the vertical
T}
\*QRO\*U	Reverse Oblique	T{
Obliqued upright design, slanted counterclockwise from the vertical
T}
\*QOT\*U	Other	Other
numeric	Polymorphic	See section \n(sP on polymorphic font support.
.sp 6p
_
.TE
.LP
The SLANT codes are for programming convenience only and usually are 
converted into their equivalent human-understandable form before being 
presented to a user.
.NH 4
SETWIDTH_NAME Field
.XS
\*(SN SETWIDTH_NAME Field
.XE
.LP 
SETWIDTH_NAME is a string that gives the font's typographic 
proportionate width, that is, the nominal width per horizontal unit of the 
font, according to the FOUNDRY's judgment.
The value \*Q0\*U is used to indicate a polymorphic font (see section \n(sP).
.LP
As with WEIGHT_NAME, the interpretation of this field or font property is 
somewhat problematic, because the designer's judgment of setwidth has 
traditionally depended on the overall design of the typeface family in 
question.
For purposes of font matching or substitution,
X client applications should either use the RELATIVE_SETWIDTH font property 
that gives the relative coded proportionate width or calculate 
the proportionate width.
.LP
The following are examples of SETWIDTH_NAME:
.IP \(bu 5
Normal 
.IP \(bu 5
Condensed 
.IP \(bu 5
Narrow 
.IP \(bu 5
Double Wide
.NH 4
ADD_STYLE_NAME Field
.XS
\*(SN ADD_STYLE_NAME Field
.XE
.LP
ADD_STYLE_NAME is a string that identifies additional typographic 
style information that is not captured by other fields but is needed 
to identify the particular font.
The character \*Q[\*U anywhere in the field is used to indicate a
polymorphic font (see section \n(sP).
.LP
ADD_STYLE_NAME is not a typeface classification field 
and is only used for uniqueness.
Its use, as such, is not limited to typographic style distinctions.
.LP
The following are examples of ADD_STYLE_NAME:
.IP \(bu 5
Serif
.IP \(bu 5
Sans Serif
.IP \(bu 5
Informal
.IP \(bu 5
Decorated
.NH 4
PIXEL_SIZE Field
.XS
\*(SN PIXEL_SIZE Field
.XE
.LP 
PIXEL_SIZE
gives the body size of the font at a particular 
POINT_SIZE and RESOLUTION_Y.
PIXEL_SIZE is either an integer-string or a string beginning
with \*Q[\*U.  A string beginning with \*Q[\*U represents a matrix
(see section \n(sM).
PIXEL_SIZE usually incorporates additional vertical spacing 
that is considered part of the font design.
(Note, however, that this value is not necessarily equivalent to the height 
of the font bounding box.)
Zero is used to indicate a scalable font (see section \n(sS).
.LP
PIXEL_SIZE usually is used by X client applications that need to 
query fonts according to device-dependent size, 
regardless of the point size or vertical resolution 
for which the font was designed.
.NH 4
POINT_SIZE Field
.XS
\*(SN POINT_SIZE Field
.XE
.LP 
POINT_SIZE gives the body size 
for which the font was designed.
POINT_SIZE is either an integer-string or a string beginning
with \*Q[\*U.  A string beginning with \*Q[\*U represents a matrix
(see section \n(sM).
This field usually incorporates additional vertical spacing 
that is considered part of the font design.
(Note, however, that POINT_SIZE is not necessarily equivalent to the height 
of the font bounding box.) 
POINT_SIZE is expressed in decipoints (where points are as defined 
in the X protocol or 72.27 points equal 1 inch).
Zero is used to indicate a scalable font (see section \n(sS).
.LP
POINT_SIZE and RESOLUTION_Y are used by X clients to query fonts 
according to device-independent size to maintain constant text 
size on the display regardless of the PIXEL_SIZE used for the font.
.NH 4
RESOLUTION_X and RESOLUTION_Y Fields
.XS
\*(SN RESOLUTION_X and RESOLUTION_Y Fields
.XE
.LP 
RESOLUTION_X and RESOLUTION_Y are unsigned integer-strings that give 
the horizontal and vertical resolution,
measured in pixels or dots per inch (dpi),
for which the font was designed.
Zero is used to indicate a scalable font (see section \n(sS).
Horizontal and vertical values are required 
because a separate bitmap font must be designed 
for displays with very different aspect ratios
(for example, 1:1, 4:3, 2:1, and so on).
.LP 
The separation of pixel or point size and resolution is necessary 
because X allows for servers with very different video characteristics 
(for example, horizontal and vertical resolution, screen and pixel size, 
pixel shape, and so on) to potentially access the same font library.
The font name, for example, must differentiate between a 14-point font designed
for 75 dpi (body size of about 14 pixels) or a 14-point font designed 
for 150 dpi (body size of about 28 pixels).
Further, in servers that implement some or all fonts as continuously scaled 
and scan-converted outlines,
POINT_SIZE and RESOLUTION_Y will help the server to differentiate 
between potentially separate font masters for text, title,
and display sizes or for other typographic considerations.
.NH 4 
SPACING Field
.XS
\*(SN SPACING Field
.XE
.LP 
SPACING is a code-string that indicates the escapement class of the font, 
that is, monospace (fixed pitch), proportional (variable pitch), 
or charcell (a special monospaced font that conforms to the traditional 
data-processing character cell font model).
The encoding is as follows:
.ne 5
.SM
.TS H
lw(.5i) lw(1.25i) lw(3.5i).
_
.sp 6p
.B
Code	English Translation	Description
.sp 6p
_
.sp 6p
.TH
.R
\*QP\*U	Proportional	T{
A font whose logical character widths vary for each glyph.
Note that no other restrictions are placed on the metrics 
of a proportional font.
T}
\*QM\*U	Monospaced	T{
A font whose logical character widths are constant 
(that is, every glyph in the font has the same logical width).
No other restrictions are placed on the metrics of a monospaced font.
T}
\*QC\*U	CharCell	T{
A monospaced font that follows the standard typewriter character cell model
(that is, the glyphs of the font can be modeled by X clients as \*Qboxes\*U 
of the same width and height that are imaged side-by-side 
to form text strings or top-to-bottom to form text lines).
By definition, 
all glyphs have the same logical character width, 
and no glyphs have \*Qink\*U outside of the character cell.
There is no kerning (that is, on a per-character basis with positive metrics: 
0 <= left-bearing <= right-bearing <= width; 
with negative metrics: width <= left-bearing <= right-bearing <= zero).
Also, the vertical extents of the font do not exceed the vertical spacing 
(that is, on a per-character basis: 
ascent <= font-ascent and descent <= font-descent).
The cell height = font-descent + font-ascent, and the width = AVERAGE_WIDTH.
T}
.sp 6p
_
.TE
.NL
.NH 4
AVERAGE_WIDTH Field
.XS
\*(SN AVERAGE_WIDTH Field
.XE
.LP 
AVERAGE_WIDTH is an integer-string typographic metric value 
that gives the unweighted arithmetic mean of the absolute value of the
width of each glyph in the font 
(measured in tenths of pixels), multiplied by \-1 if the dominant
writing direction for the font is right-to-left.
A leading \*Q\^~\^\*U (TILDE) indicates a negative value.
For monospaced and character cell fonts, 
this is the width of all glyphs in the font.
Zero is used to indicate a scalable font (see section \n(sS).
.NH 4
CHARSET_REGISTRY and CHARSET_ENCODING Fields
.XS
\*(SN CHARSET_REGISTRY and CHARSET_ENCODING Fields
.XE
.LP
The character set used to encode the glyphs of the font (and implicitly 
the font's glyph repertoire), as maintained by the X Consortium character
set registry.
CHARSET_REGISTRY is an x-registered-name that identifies 
the registration authority that owns the specified encoding.
CHARSET_ENCODING is a registered name that identifies the coded character set 
as defined by that registration authority
and, optionally, a subsetting hint.
.LP
Although the X protocol does not explicitly have any knowledge about 
character set encodings, 
it is expected that server implementors will prefer to embed knowledge 
of certain proprietary or standard charsets into their font library 
for reasons of performance and convenience.
The CHARSET_REGISTRY and CHARSET_ENCODING fields or properties allow 
an X client font request to specify a specific charset mapping 
in server environments where multiple charsets are supported.
The availability of any particular 
character set is font and server implementation dependent.
.LP
To prevent collisions when defining character set names, 
it is recommended that CHARSET_REGISTRY and CHARSET_ENCODING name pairs 
be constructed according to the following conventions:
.IN "CHARSET Syntax"
.SM
.TS
rw(1.5i) lw(3.75i).
.sp 6p
CharsetRegistry ::=	T{
StdCharsetRegistryName | PrivCharsetRegistryName
T}
CharsetEncoding ::=	T{
StdCharsetEncodingName | PrivCharsetEncodingName
T}
StdCharsetRegistryName ::=	T{
StdOrganizationId StdNumber | StdOrganizationId StdNumber Dot Year
T}
PrivCharsetRegistryName ::=	OrganizationId STRING8
StdCharsetEncodingName ::=	T{
STRING8\-numeric part number of referenced standard
T}
PrivCharsetEncodingName ::=	STRING8
StdOrganizationId ::=	T{
STRING8\-the registered name or acronym of the referenced standard organization
T}
StdNumber ::=	STRING8\-referenced standard number
OrganizationId ::=	T{
STRING8\-the registered name or acronym of the organization
T}
Dot ::=	OCTET\-\*Q\^.\^\*U (FULL STOP)
Year ::=	STRING8\-numeric year (for example, 1989)
.TE
.NL
.LP
The X Consortium shall maintain and publish a registry 
of such character set names for use in X protocol font names and properties 
as specified in XLFD.
.LP
The ISO Latin-1 character set shall be registered by the X Consortium as the 
CHARSET_REGISTRY-CHARSET_ENCODING value pair: \*QISO8859-1\*U.
.LP
If the CHARSET_ENCODING contains a \*Q[\*U (LEFT SQUARE BRACKET),
the \*Q[\*U and the characters after it up to a \*Q]\*U (RIGHT SQUARE
BRACKET) are a
subsetting hint telling the font source that the client is interested
only in a subset of the characters of the font.
The font source can, optionally, return a font that
contains only those characters or any superset of those characters.  The
client can expect to obtain valid glyphs and metrics only for those
characters, and not for any other characters in the font.
The font properties may optionally be calculated by considering only
the characters in the subset.
.LP
The BNF for the subsetting hint is
.SM
.TS
rw(1.5i) l.
Subset ::=	LeftBracket RangeList RightBracket
RangeList ::=	Range | Range Space RangeList
Range ::=	Number | Number Underscore Number
Number ::=	\*Q0x\*U HexNumber | DecNumber
HexNumber ::=	HexDigit | HexDigit HexNumber
DecNumber ::=	DecDigit | DecDigit DecNumber
DecDigit ::=	\*Q0\*U | \*Q1\*U | \*Q2\*U | \*Q3\*U | \*Q4\*U | \*Q5\*U | \*Q6\*U | \*Q7\*U | \*Q8\*U | \*Q9\*U
HexDigit ::=	DecDigit | \*Qa\*U | \*Qb\*U | \*Qc\*U | \*Qd\*U | \*Qe\*U | \*Qf\*U
LeftBracket ::=	\*Q[\*U (LEFT SQUARE BRACKET)
RightBracket ::=	\*Q]\*U (RIGHT SQUARE BRACKET)
Space ::=	\*Q\0\*U (SPACE)
Underscore ::=	\*Q_\*U (LOW LINE)
.TE
.NL
.LP
Each Range specifies characters that are to be part of the subset
included in the font.
A Range containing two Numbers specifies the first and last character,
inclusively, of a range of characters.
A Range that is a single Number specifies a single character to be
included in the font.
A HexNumber is interpreted as a hexadecimal number.
A DecNumber is interpreted as a decimal number.
The font consists of the union of all the Ranges in the
RangeList.
.LP
For example,
.br
.ft C
.SM
	-misc-fixed-medium-r-normal--0-0-0-0-c-0-iso8859-1[65 70 80_90]
.NL
.ft P
.br
tells the font source that the client is interested only in characters
65, 70, and 80\-90.
.NH 3
Examples
.XS
\*(SN Examples
.XE
.LP
The following examples of font names are derived from the screen fonts 
shipped with the X Consortium distribution.
.\" why is this table so long?  I took out some fonts in v1.5
.\" to make the page breaks better.
.SM
.TS H
lw(1.45i) lw(4.45i).
_
.sp 6p
.B
Font	X FontName
.sp 6p
_
.sp 6p
.TH
.R
\fB75-dpi Fonts\fP
.sp 3p
T{
Charter 12 pt
T}	T{
-Bitstream-Charter-Medium-R-Normal--12-120-75-75-P-68-ISO8859-1
T}
T{
Charter Bold 12 pt
T}	T{
-Bitstream-Charter-Bold-R-Normal--12-120-75-75-P-76-ISO8859-1
T}
T{
Charter Bold Italic 12 pt
T}	T{
-Bitstream-Charter-Bold-I-Normal--12-120-75-75-P-75-ISO8859-1
T}
T{
Charter Italic 12 pt
T}	T{
-Bitstream-Charter-Medium-I-Normal--12-120-75-75-P-66-ISO8859-1
T}
Courier 8 pt	-Adobe-Courier-Medium-R-Normal--8-80-75-75-M-50-ISO8859-1
Courier 10 pt	-Adobe-Courier-Medium-R-Normal--10-100-75-75-M-60-ISO8859-1
Courier 12 pt	-Adobe-Courier-Medium-R-Normal--12-120-75-75-M-70-ISO8859-1
Courier 24 pt	-Adobe-Courier-Medium-R-Normal--24-240-75-75-M-150-ISO8859-1
T{
Courier Bold 10 pt
T}	T{
-Adobe-Courier-Bold-R-Normal--10-100-75-75-M-60-ISO8859-1
T}
T{
Courier Bold Oblique 10 pt
T}	T{
-Adobe-Courier-Bold-O-Normal--10-100-75-75-M-60-ISO8859-1
T}
T{
Courier Oblique 10 pt
T}	T{
-Adobe-Courier-Medium-O-Normal--10-100-75-75-M-60-ISO8859-1
T}
.sp 3p
.ne 2
\fB100-dpi Fonts\fP
.sp 3p
T{
Symbol 10 pt
T}	T{
-Adobe-Symbol-Medium-R-Normal--14-100-100-100-P-85-Adobe-FONTSPECIFIC
T}
T{
Symbol 14 pt
T}	T{
-Adobe-Symbol-Medium-R-Normal--20-140-100-100-P-107-Adobe-FONTSPECIFIC
T}
T{
Symbol 18 pt
T}	T{
-Adobe-Symbol-Medium-R-Normal--25-180-100-100-P-142-Adobe-FONTSPECIFIC
T}
T{
Symbol 24 pt
T}	T{
-Adobe-Symbol-Medium-R-Normal--34-240-100-100-P-191-Adobe-FONTSPECIFIC
T}
T{
Times Bold 10 pt
T}	T{
-Adobe-Times-Bold-R-Normal--14-100-100-100-P-76-ISO8859-1
T}
T{
Times Bold Italic 10 pt
T}	T{
-Adobe-Times-Bold-I-Normal--14-100-100-100-P-77-ISO8859-1
T}
T{
Times Italic 10 pt
T}	T{
-Adobe-Times-Medium-I-Normal--14-100-100-100-P-73-ISO8859-1
T}
T{
Times Roman 10 pt
T}	T{
-Adobe-Times-Medium-R-Normal--14-100-100-100-P-74-ISO8859-1
T}
_
.TE
.NL
.NH 2
Font Properties
.XS
\*(SN Font Properties
.XE
.LP
All font properties are optional but will generally include the 
font name fields and, on a font-by-font basis, any other useful font 
descriptive and use information that may be required to use the font 
intelligently.
The XLFD specifies an extensive set of standard X font properties,
their interpretation, and fallback rules when the property is not defined 
for a given font.
The goal is to provide client applications with enough font information 
to be able to make automatic formatting and display decisions 
with good typographic results.
.LP
Font property names use the ISO 8859-1 encoding.
.LP
Additional standard X font property definitions may be defined in the 
future and private properties may exist in X fonts at any time.
Private font properties should be defined to conform to the general mechanism 
defined in the X protocol to prevent overlap of name space and ambiguous 
property names, that is, private font property names are of the form: 
\*Q_\*U (LOW LINE), 
followed by the organizational identifier, followed by \*Q_\*U (LOW LINE), 
and terminated with the property name.
.LP
The Backus-Naur Form syntax description of X font properties is as follows:
.IN "Font Properties" "BNF Syntax"
.SM
.TS
rw(1.5i) lw(3.75i).
.sp 6p
Properties ::=	OptFontPropList
OptFontPropList ::=	NULL | OptFontProp OptFontPropList
OptFontProp ::=	PrivateFontProp | XFontProp
PrivateFontProp ::=	T{
STRING8 | Underscore OrganizationId Underscore STRING8
T}
XFontProp ::=	T{
FOUNDRY | FAMILY_NAME | WEIGHT_NAME | SLANT | SETWIDTH_NAME | ADD_STYLE_NAME 
| PIXEL_SIZE | POINT_SIZE | RESOLUTION_X | RESOLUTION_Y | SPACING | 
AVERAGE_WIDTH | CHARSET_REGISTRY | CHARSET_ENCODING | QUAD_WIDTH | 
RESOLUTION | MIN_SPACE | NORM_SPACE | MAX_SPACE | END_SPACE | SUPERSCRIPT_X | 
SUPERSCRIPT_Y | SUBSCRIPT_X | SUBSCRIPT_Y | UNDERLINE_POSITION | 
UNDERLINE_THICKNESS | STRIKEOUT_ASCENT | STRIKEOUT_DESCENT | ITALIC_ANGLE 
| X_HEIGHT | WEIGHT | FACE_NAME |
FULL_NAME | FONT |
COPYRIGHT | AVG_CAPITAL_WIDTH | 
AVG_LOWERCASE_WIDTH | RELATIVE_SETWIDTH | RELATIVE_WEIGHT | CAP_HEIGHT | 
SUPERSCRIPT_ SIZE | FIGURE_WIDTH | SUBSCRIPT_SIZE | SMALL_CAP_SIZE | 
NOTICE | DESTINATION
| FONT_TYPE | FONT_VERSION | RASTERIZER_NAME | RASTERIZER_VERSION |
RAW_ASCENT | RAW_DESCENT | RAW_* | AXIS_NAMES | AXIS_LIMITS |
AXIS_TYPES
T}
Underscore ::=	OCTET\-\*Q_\*U (LOW LINE)
OrganizationId ::=	T{
STRING8\-the registered name of the organization
T}
.TE
.NL
.NH 3
FOUNDRY
.XS
\*(SN FOUNDRY
.XE
.LP
FOUNDRY is as defined in the
.PN FontName 
except that the property type is ATOM.
.LP
FOUNDRY cannot be calculated or defaulted if not supplied as a font property.
.NH 3
FAMILY_NAME
.XS
\*(SN FAMILY_NAME
.XE
.LP
FAMILY_NAME is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
FAMILY_NAME cannot be calculated or defaulted if not supplied as a font 
property.
.NH 3
WEIGHT_NAME
.XS
\*(SN WEIGHT_NAME
.XE
.LP
WEIGHT_NAME is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
WEIGHT_NAME can be defaulted if not supplied as a font property, as follows:
.LP
.DS
if (WEIGHT_NAME undefined) then 
   WEIGHT_NAME = ATOM(\*QMedium\*U)
.DE
.NH 3
SLANT
.XS
\*(SN SLANT
.XE
.LP
SLANT is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
SLANT can be defaulted if not supplied as a font property, as follows:
.LP
.DS
if (SLANT undefined) then 
   SLANT = ATOM(\*QR\*U)
.DE
.NH 3
SETWIDTH_NAME
.XS
\*(SN SETWIDTH_NAME
.XE
.LP
SETWIDTH_NAME is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
SETWIDTH_NAME can be defaulted if not supplied as a font property, as follows:
.LP
.DS
if (SETWIDTH_NAME undefined) then
   SETWIDTH_NAME = ATOM(\*QNormal\*U)
.DE
.NH 3
ADD_STYLE_NAME
.XS
\*(SN ADD_STYLE_NAME
.XE
.LP
ADD_STYLE_NAME is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
ADD_STYLE_NAME can be defaulted if not supplied as a font property, as follows:
.LP
.DS
if (ADD_STYLE_NAME undefined) then
   ADD_STYLE_NAME = ATOM(\*Q\*U)
.DE
.NH 3
PIXEL_SIZE
.XS
\*(SN PIXEL_SIZE
.XE
.LP
PIXEL_SIZE is as defined in the 
.PN FontName
except that the property type is INT32.
.LP
X clients requiring pixel values for the various typographic fixed 
spaces (em space, en space, and thin space) can use the following 
algorithm for computing these values from other properties specified 
for a font:
.LP
.DS
DeciPointsPerInch = 722.7
EMspace = ROUND ((RESOLUTION_X * POINT_SIZE) / DeciPointsPerInch)
ENspace = ROUND (EMspace / 2)
THINspace = ROUND (EMspace / 3)\fP
.DE
.LP
where a slash (\^/\^) denotes real division, 
an asterisk (\^*\^) denotes real multiplication,
and ROUND denotes a function that rounds its real argument
\fIa\fP up or down
to the next integer.
This rounding is done according to X = FLOOR (\fIa\fP + 0.5),
where FLOOR is a function that rounds its real argument down to the
nearest integer.
.LP
PIXEL_SIZE can be approximated if not supplied as a font property, 
according to the following algorithm:
.LP
.DS
DeciPointsPerInch = 722.7
if (PIXEL_SIZE undefined) then
   PIXEL_SIZE = ROUND ((RESOLUTION_Y * POINT_SIZE) / DeciPointsPerInch)
.DE
.NH 3
POINT_SIZE
.XS
\*(SN POINT_SIZE
.XE
.LP
POINT_SIZE is as defined in the 
.PN FontName
except that the property type is INT32.
.LP
X clients requiring device-independent values for em space, 
en space, and thin space can use the following algorithm:
.LP
.DS I
EMspace = ROUND (POINT_SIZE / 10)
ENspace = ROUND (POINT_SIZE / 20)
THINspace = ROUND (POINT_SIZE / 30)
.DE
.LP
Design POINT_SIZE cannot be calculated or approximated.
.NH 3
RESOLUTION_X
.XS
\*(SN RESOLUTION_X
.XE
.LP
RESOLUTION_X is as defined in the 
.PN FontName
except that the property type is CARD32.
.LP
RESOLUTION_X cannot be calculated or approximated.
.NH 3
RESOLUTION_Y
.XS
\*(SN RESOLUTION_Y
.XE
.LP
RESOLUTION_Y is as defined in the 
.PN FontName 
except that the property type is CARD32.
.LP
RESOLUTION_X cannot be calculated or approximated.
.NH 3
SPACING
.XS
\*(SN SPACING
.XE
.LP
SPACING is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
SPACING can be calculated if not supplied as a font property, 
according to the definitions given above for the 
.PN FontName .
.NH 3
AVERAGE_WIDTH
.XS
\*(SN AVERAGE_WIDTH
.XE
.LP
AVERAGE_WIDTH is as defined in the 
.PN FontName
except that the property type is INT32.
.LP
AVERAGE_WIDTH can be calculated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (AVERAGE_WIDTH undefined) then
   AVERAGE_WIDTH = ROUND (MEAN (ABS (width of each glyph in font)) * 10)
	* (if (dominant writing direction L-to-R) then 1 else \-1)
.DE
.LP
where MEAN is a function that returns the arithmetic mean of its arguments.
.LP
X clients that require values for the number of characters per inch (pitch) 
of a monospaced font can use the following algorithm using the 
AVERAGE_WIDTH and RESOLUTION_X font properties:
.LP
.DS
if (SPACING not proportional) then
   CharPitch = (RESOLUTION_X * 10) / AVERAGE_WIDTH
.DE
.NH 3
CHARSET_REGISTRY
.XS
\*(SN CHARSET_REGISTRY
.XE
.LP
CHARSET_REGISTRY is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
CHARSET_REGISTRY cannot be defaulted if not supplied as a font property.
.NH 3
CHARSET_ENCODING
.XS
\*(SN CHARSET_ENCODING
.XE
.LP
CHARSET_ENCODING is as defined in the 
.PN FontName
except that the property type is ATOM.
.LP
CHARSET_ENCODING cannot be defaulted if not supplied as a font property.
.NH 3
MIN_SPACE
.XS
\*(SN MIN_SPACE
.XE
.LP
MIN_SPACE is an integer value (of type INT32)
that gives the recommended minimum word-space value to be used with this font.
.LP
MIN_SPACE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS I
if (MIN_SPACE undefined) then
   MIN_SPACE = ROUND(0.75 * NORM_SPACE)
.DE
.NH 3
NORM_SPACE
.XS
\*(SN NORM_SPACE
.XE
.LP
NORM_SPACE is an integer value (of type INT32)
that gives the recommended normal word-space value to be used with this font.
.LP
NORM_SPACE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS 0
DeciPointsPerInch = 722.7
if (NORM_SPACE undefined) then
   if (SPACE glyph exists) then
      NORM_SPACE = width of SPACE
   else NORM_SPACE = ROUND((0.33 * RESOLUTION_X * POINT_SIZE)/ DeciPointsPerInch)
.DE
.NH 3
MAX_SPACE
.XS
\*(SN MAX_SPACE
.XE
.LP
MAX_SPACE is an integer value (of type INT32)
that gives the recommended maximum word-space value to be used with this font.
.LP
MAX_SPACE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (MAX_SPACE undefined) then
   MAX_SPACE = ROUND(1.5 * NORM_SPACE)
.DE
.NH 3
END_SPACE
.XS
\*(SN END_SPACE
.XE
.LP
END_SPACE is an integer value (of type INT32)
that gives the recommended spacing at the end of sentences.
.LP
END_SPACE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS 
if (END_SPACE undefined) then
   END_SPACE = NORM_SPACE
.DE
.NH 3
AVG_CAPITAL_WIDTH 
.XS
\*(SN AVG_CAPITAL_WIDTH
.XE
.LP
AVG_CAPITAL_WIDTH is an integer value (of type INT32)
that gives the unweighted arithmetic mean of the absolute value of the
width of each capital glyph in the font, in tenths of pixels,
multiplied by \-1 if the dominant
writing direction for the font is right-to-left.
This property applies to both Latin and non-Latin fonts.
For Latin fonts, 
capitals are the glyphs A through Z.
This property is usually used for font matching or substitution.
.LP
AVG_CAPITAL_WIDTH can be calculated if not provided as a font property, 
according to the following algorithm:
.LP
.DS I
if (AVG_CAPITAL_WIDTH undefined) then
   if (capitals exist) then
      AVG_CAPITAL_WIDTH = ROUND (MEAN
		     (ABS (width of each capital glyph)) * 10)
	   * (if (dominant writing direction L-to-R) then 1 else \-1)
   else AVG_CAPITAL_WIDTH undefined
.DE
.NH 3
AVG_LOWERCASE_WIDTH
.XS
\*(SN AVG_LOWERCASE_WIDTH
.XE
.LP
AVG_LOWERCASE_WIDTH is an integer value (of type INT32)
that gives the unweighted arithmetic mean width of the absolute value
of the width of each lowercase glyph in the font in tenths of pixels,
multiplied by \-1 if the dominant
writing direction for the font is right-to-left.
For Latin fonts, 
lowercase are the glyphs a through z.
This property is usually used for font matching or substitution.
.LP
Where appropriate, 
AVG_LOWERCASE_WIDTH can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (AVG_LOWERCASE_WIDTH undefined) then
   if (lowercase exists) then
      AVG_LOWERCASE_WIDTH = ROUND (MEAN
                       (ABS (width of each lowercase glyph)) * 10)
	* (if (dominant writing direction L-to-R) then 1 else \-1)
   else AVG_LOWERCASE_WIDTH undefined
.DE
.NH 3
QUAD_WIDTH 
.XS
\*(SN QUAD_WIDTH
.XE
.LP
QUAD_WIDTH is an integer typographic metric (of type INT32) 
that gives the width of a quad (em) space.
.NT Note
Because all typographic fixed spaces (em, en, and thin) are constant 
for a given font size (that is, they do not vary according to setwidth),
the use of this font property has been deprecated.
X clients that require typographic fixed space values are encouraged 
to discontinue use of QUAD_WIDTH and compute these values 
from other font properties (for example, PIXEL_SIZE).
X clients that require  a font-dependent width value should use either 
the FIGURE_WIDTH or one of the average character width font properties
(AVERAGE_WIDTH, AVG_CAPITAL_WIDTH or AVG_LOWERCASE_WIDTH).
.NE
.NH 3
FIGURE_WIDTH
.XS
\*(SN FIGURE_WIDTH
.XE
.LP
FIGURE_WIDTH is an integer typographic metric (of type INT32)
that gives the width of the tabular figures and the dollar sign,
if suitable for tabular setting (all widths equal).
For Latin fonts, these tabular figures are the Arabic numerals 0 through 9.
.LP
FIGURE_WIDTH can be approximated if not supplied as a font property, 
according to the following algorithm:
.LP
.DS I
if (numerals and DOLLAR sign are defined & widths are equal) then
   FIGURE_WIDTH = width of DOLLAR
else FIGURE_WIDTH property undefined
.DE
.NH 3
SUPERSCRIPT_X 
.XS
\*(SN SUPERSCRIPT_X
.XE
.LP
SUPERSCRIPT_X is an integer value (of type INT32)
that gives the recommended horizontal offset in pixels 
from the position point to the X origin of synthetic superscript text.
If the current position point is at [X,Y], 
then superscripts should begin at [X + SUPERSCRIPT_X, Y \- SUPERSCRIPT_Y].
.LP
SUPERSCRIPT_X can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SUPERSCRIPT_X undefined) then
   if (TANGENT(ITALIC_ANGLE) defined) then
      SUPERSCRIPT_X = ROUND((0.40 * CAP_HEIGHT) / TANGENT(ITALIC_ANGLE))
   else SUPERSCRIPT_X = ROUND(0.40 * CAP_HEIGHT)
.DE
.LP
where TANGENT is a trigonometric function that returns the tangent of 
its argument, which is in 1/64 degrees.
.NH 3
SUPERSCRIPT_Y
.XS
\*(SN SUPERSCRIPT_Y
.XE
.LP
SUPERSCRIPT_Y is an integer value (of type INT32)
that gives the recommended vertical offset in pixels 
from the position point to the Y origin of synthetic superscript text.
If the current position point is at [X,Y], 
then superscripts should begin at [X + SUPERSCRIPT_X, Y \- SUPERSCRIPT_Y].
.LP
SUPERSCRIPT_Y can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SUPERSCRIPT_Y undefined) then
   SUPERSCRIPT_Y = ROUND(0.40 * CAP_HEIGHT)
.DE
.NH 3
SUBSCRIPT_X
.XS
\*(SN SUBSCRIPT_X
.XE
.LP
SUBSCRIPT_X is an integer value (of type INT32)
that gives the recommended horizontal offset in pixels 
from the position point to the X origin of synthetic subscript text.
If the current position point is at [X,Y], 
then subscripts should begin at [X + SUBSCRIPT_X, Y + SUBSCRIPT_Y].
.LP
SUBSCRIPT_X can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SUBSCRIPT_X undefined) then
   if (TANGENT(ITALIC_ANGLE) defined) then
      SUBSCRIPT_X = ROUND((0.40 * CAP_HEIGHT) / TANGENT(ITALIC_ANGLE))
   else SUBSCRIPT_X = ROUND(0.40 * CAP_HEIGHT)
.DE
.NH 3
SUBSCRIPT_Y 
.XS
\*(SN SUBSCRIPT_Y
.XE
.LP
SUBSCRIPT_Y is an integer value (of type INT32)
that gives the recommended vertical offset in pixels 
from the position point to the Y origin of synthetic subscript text.
If the current position point is at [X,Y], 
then subscripts should begin at [X + SUBSCRIPT_X, Y + SUBSCRIPT_Y].
.LP
SUBSCRIPT_Y can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SUBSCRIPT_Y undefined) then
   SUBSCRIPT_Y = ROUND(0.40 * CAP_HEIGHT)
.DE
.NH 3
SUPERSCRIPT_SIZE 
.XS
\*(SN SUPERSCRIPT_SIZE
.XE
.LP
SUPERSCRIPT_SIZE is an integer value (of type INT32)
that gives the recommended body size of synthetic superscripts 
to be used with this font, in pixels.
This will generally be smaller than the size of the current font;
that is, superscripts are imaged from a smaller font
offset according to SUPERSCRIPT_X and SUPERSCRIPT_Y.
.LP
SUPERSCRIPT_SIZE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SUPERSCRIPT_SIZE undefined) then
   SUPERSCRIPT_SIZE = ROUND(0.60 * PIXEL_SIZE)
.DE
.NH 3
SUBSCRIPT_SIZE
.XS
\*(SN SUBSCRIPT_SIZE
.XE
.LP
SUBSCRIPT_SIZE is an integer value (of type INT32)
that gives the recommended body size of synthetic subscripts 
to be used with this font, in pixels.
As with SUPERSCRIPT_SIZE, 
this will generally be smaller than the size of the current font; 
that is, subscripts are imaged from a smaller 
font offset according to SUBSCRIPT_X and SUBSCRIPT_Y.
.LP
SUBSCRIPT_SIZE can be approximated if not provided as a font property, 
according to the algorithm:
.LP
.DS
if (SUBSCRIPT_SIZE undefined) then
   SUBSCRIPT_SIZE = ROUND(0.60 * PIXEL_SIZE)
.DE
.NH 3
SMALL_CAP_SIZE
.XS
\*(SN SMALL_CAP_SIZE
.XE
.LP
SMALL_CAP_SIZE is an integer value (of type INT32)
that gives the recommended body size of synthetic small capitals 
to be used with this font, in pixels.
Small capitals are generally imaged from a smaller font 
of slightly more weight.
No offset [X,Y] is necessary.
.LP
SMALL_CAP_SIZE can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (SMALL_CAP_SIZE undefined) then
   SMALL_CAP_SIZE = ROUND(PIXEL_SIZE * ((X_HEIGHT 
                              + ((CAP_HEIGHT \- X_HEIGHT) / 3)) / CAP_HEIGHT))
.DE
.NH 3
UNDERLINE_POSITION
.XS
\*(SN UNDERLINE_POSITION
.XE
.LP
UNDERLINE_POSITION is an integer value (of type INT32)
that gives the recommended vertical offset in pixels
from the baseline to the top of the underline.
If the current position point is at [X,Y], 
the top of the baseline is given by [X, Y + UNDERLINE_POSITION].
.LP
UNDERLINE_POSITION can be approximated if not provided as a font 
property, according to the following algorithm:
.LP
.DS
if (UNDERLINE_POSITION undefined) then
   UNDERLINE_POSITION = ROUND((maximum descent) / 2)
.DE
where \fImaximum descent\fP is the maximum descent (below the baseline)
in pixels of any glyph in the font.
.NH 3
UNDERLINE_THICKNESS 
.XS
\*(SN UNDERLINE_THICKNESS
.XE
.LP
UNDERLINE_THICKNESS is an integer value (of type INT32)
that gives the recommended underline thickness, in pixels.
.LP
UNDERLINE_THICKNESS can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
CapStemWidth = average width of the stems of capitals
if (UNDERLINE_THICKNESS undefined) then
   UNDERLINE_THICKNESS = CapStemWidth
.DE
.NH 3
STRIKEOUT_ASCENT
.XS
\*(SN STRIKEOUT_ASCENT
.XE
.LP
STRIKEOUT_ASCENT is an integer value (of type INT32)
that gives the vertical ascent for boxing or voiding glyphs in this font.
If the current position is at [X,Y] and the string extent is EXTENT, 
the upper-left corner of the strikeout box is at [X, Y \- STRIKEOUT_ASCENT] 
and the lower-right corner of the box is at [X + EXTENT, Y + STRIKEOUT_DESCENT].
.LP
STRIKEOUT_ASCENT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (STRIKEOUT_ASCENT undefined)
   STRIKEOUT_ASCENT = maximum ascent
.DE
where \fImaximum ascent\fP is the maximum ascent (above the baseline)
in pixels of any glyph in the font.
.NH 3
STRIKEOUT_DESCENT
.XS
\*(SN STRIKEOUT_DESCENT
.XE
.LP
STRIKEOUT_DESCENT is an integer value (of type INT32)
that gives the vertical descent for boxing or voiding glyphs in this font.
If the current position is at [X,Y] and the string extent is EXTENT,
the upper-left corner of the strikeout box is at [X, Y \- STRIKEOUT_ASCENT] 
and the lower-right corner of the box is at [X + EXTENT, Y + STRIKEOUT_DESCENT].
.LP
STRIKEOUT_DESCENT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (STRIKEOUT_DESCENT undefined)
   STRIKEOUT_DESCENT = maximum descent
.DE
where \fImaximum descent\fP is the maximum descent (below the baseline)
in pixels of any glyph in the font.
.NH 3
ITALIC_ANGLE
.XS
\*(SN ITALIC_ANGLE
.XE
.LP
ITALIC_ANGLE is an integer value (of type INT32)
that gives the nominal posture angle of the typeface design, in 1/64 degrees, 
measured from the glyph origin counterclockwise from the three o'clock position.
.LP
ITALIC_ANGLE can be defaulted if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (ITALIC_ANGLE undefined) then
   ITALIC_ANGLE = (90 * 64)
.DE
.NH 3
CAP_HEIGHT 
.XS
\*(SN CAP_HEIGHT
.XE
.LP
CAP_HEIGHT is an integer value (of type INT32)
that gives the nominal height of the capital letters contained in the font, 
as specified by the FOUNDRY or typeface designer.
.LP
Certain clients require CAP_HEIGHT to compute scale factors and 
positioning offsets for synthesized glyphs where this 
information or designed glyphs are not explicitly provided by the font 
(for example, small capitals, superiors, inferiors, and so on).
CAP_HEIGHT is also a critical factor in font matching and substitution.
.LP
CAP_HEIGHT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (CAP_HEIGHT undefined) then
   if (Latin font) then
      CAP_HEIGHT = XCharStruct.ascent[glyph X]
   else if (capitals exist) then
       CAP_HEIGHT = XCharStruct.ascent[some unaccented capital glyph]
   else CAP_HEIGHT undefined
.DE
.NH 3
X_HEIGHT
.XS
\*(SN X_HEIGHT
.XE
.LP
X_HEIGHT is an integer value (of type INT32)
that gives the nominal height above the baseline of the lowercase glyphs 
contained in the font, 
as specified by the FOUNDRY or typeface designer.
.LP
As with CAP_HEIGHT, 
X_HEIGHT is required by certain clients to compute scale factors 
for synthesized small capitals where this information is not explicitly 
provided by the font resource.
X_HEIGHT is a critical factor in font matching and substitution.
.LP
X_HEIGHT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS I
if (X_HEIGHT undefined) then
   if (Latin font) then
      X_HEIGHT = XCharStruct.ascent[glyph x]
   else if (lowercase exists) then
        X_HEIGHT = XCharStruct.ascent[some unaccented lc glyph without an ascender]
   else X_HEIGHT undefined
.DE
.NH 3
RELATIVE_SETWIDTH
.XS
\*(SN RELATIVE_SETWIDTH
.XE
.LP
RELATIVE_SETWIDTH is an unsigned integer value (of type CARD32)
that gives the coded proportionate width of the font,
relative to all known fonts of the same typeface family, 
according to the type designer's or FOUNDRY's judgment.
.LP
RELATIVE_SETWIDTH ranges from 10 to 90 or is 0 if undefined or unknown.
The following reference values are defined:
.TS H
lw(.5i) lw(1i) lw(2.75i).
_
.sp 6p
.B
Code	English Translation	Description
.sp 6p
_
.sp 6p
.TH
.R
0	Undefined	Undefined or unknown
10	UltraCondensed	The lowest ratio of average width to height
20	ExtraCondensed
30	Condensed	Condensed, Narrow, Compressed, ...
40	SemiCondensed
50	Medium	Medium, Normal, Regular, ...
60	SemiExpanded	SemiExpanded, DemiExpanded, ...
70	Expanded
80	ExtraExpanded	ExtraExpanded, Wide, ...
90	UltraExpanded	The highest ratio of average width to height
.sp 6p
_
.TE
.LP
RELATIVE_SETWIDTH can be defaulted if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (RELATIVE_SETWIDTH undefined) then
   RELATIVE_SETWIDTH = 50
.DE
.LP
For polymorphic fonts, RELATIVE_SETWIDTH is not necessarily a
linear function of the font's setwidth axis.
.LP
X clients that want to obtain a calculated proportionate width of the 
font (that is, a font-independent way of identifying the proportionate 
width across all fonts and all font vendors) can use the following algorithm: 
.LP
.DS
SETWIDTH = AVG_CAPITAL_WIDTH / (CAP_HEIGHT * 10)
.DE
.LP
where SETWIDTH is a real number with zero being the narrowest 
calculated setwidth.
.NH 3
RELATIVE_WEIGHT
.XS
\*(SN RELATIVE_WEIGHT
.XE
.LP
RELATIVE_WEIGHT is an unsigned integer value (of type CARD32)
that gives the coded weight of the font, 
relative to all known fonts of the same typeface family, 
according to the type designer's or FOUNDRY's judgment.
.LP
RELATIVE_WEIGHT ranges from 10 to 90 or is 0 if undefined or unknown.
The following reference values are defined:
.TS H
lw(.5i) lw(1i) lw(3.75i).
_
.sp 6p
.B
Code	English Translation	Description
.sp 6p
_
.sp 6p
.TH
.R
0	Undefined	Undefined or unknown
10	UltraLight	The lowest ratio of stem width to height
20	ExtraLight
30	Light
40	SemiLight	SemiLight, Book, ...
50	Medium	Medium, Normal, Regular,...
60	SemiBold	SemiBold, DemiBold, ...
70	Bold
80	ExtraBold	ExtraBold, Heavy, ...
90	UltraBold	T{
UltraBold, Black, ..., the highest ratio of stem width to height
T}
.sp 6p
_
.TE
.LP
RELATIVE_WEIGHT can be defaulted if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (RELATIVE_WEIGHT undefined) then
   RELATIVE_WEIGHT = 50
.DE
.LP
For polymorphic fonts, RELATIVE_WEIGHT is not necessarily a
linear function of the font's weight axis.
.NH 3 
WEIGHT
.XS
\*(SN WEIGHT
.XE
.LP
Calculated WEIGHT is an unsigned integer value (of type CARD32)
that gives the calculated weight of the font, 
computed as the ratio of capital stem width to CAP_HEIGHT, 
in the range 0 to 1000, where 0 is the lightest weight.
.LP
WEIGHT can be calculated if not supplied as a font property, 
according to the following algorithm:
.LP
.DS
CapStemWidth = average width of the stems of capitals
if (WEIGHT undefined) then
   WEIGHT = ROUND ((CapStemWidth * 1000) / CAP_HEIGHT)
.DE
.LP
A calculated value for weight is necessary when matching fonts from 
different families because both the RELATIVE_WEIGHT and the WEIGHT_NAME are 
assigned by the typeface supplier, according to its tradition and practice, 
and therefore, are somewhat subjective.
Calculated WEIGHT provides a font-independent way of identifying 
the weight across all fonts and all font vendors.
.NH 3
RESOLUTION 
.XS
\*(SN RESOLUTION 
.XE
.LP
RESOLUTION is an integer value (of type INT32)
that gives the resolution for which this font was created,
measured in 1/100 pixels per point.
.NT Note
As independent horizontal and vertical design resolution components
are required to accommodate displays with nonsquare aspect ratios,
the use of this font property has been deprecated,
and independent RESOLUTION_X and RESOLUTION_Y font name fields/properties 
have been defined (see sections 3.1.2.9 and 3.1.2.10).
X clients are encouraged to discontinue use of the RESOLUTION property
and are encouraged to use the appropriate X,Y resolution properties,
as required.
.NE                     \" Note End
.NH 3
FONT
.XS
\*(SN FONT
.XE
.LP
FONT is a string (of type ATOM) that gives the full XLFD name of the
font\*-that is, the value can be used to open another
instance of the same font.
.LP
If not provided, the FONT property cannot be calculated.
.NH 3
FACE_NAME 
.XS
\*(SN FACE_NAME
.XE
.LP
FACE_NAME is a human-understandable string (of type ATOM)
that gives the full device-independent typeface name, 
including the owner, weight, slant, set, and so on 
but not the resolution, size, and so on.
This property may be used as feedback during font selection.
.LP
FACE_NAME cannot be calculated or approximated if not provided as a font 
property.
.NH 3
FULL_NAME
.XS
\*(SN FULL_NAME
.XE
.LP
FULL_NAME is the same as FACE_NAME.
Its use is deprecated, but it is found on some old fonts.
.NH 3
COPYRIGHT 
.XS
\*(SN COPYRIGHT
.XE
.LP
COPYRIGHT is a human-understandable string (of type ATOM)
that gives the copyright information of the legal owner 
of the digital font data.
.LP
This information is a required component of a font
but is independent of the particular format used to represent it 
(that is, it cannot be captured as a comment that could later 
be thrown away for efficiency reasons).
.LP
COPYRIGHT cannot be calculated or approximated if not provided as a font 
property.
.NH 3
NOTICE
.XS
\*(SN NOTICE
.XE
.LP
NOTICE is a human-understandable string (of type ATOM)
that gives the copyright information of the legal owner of the font design
or, if not applicable, the trademark information for the typeface FAMILY_NAME.
.LP
Typeface design and trademark protection laws vary from country to country, 
the USA having no design copyright protection currently
while various countries in Europe offer both design and typeface family name 
trademark protection.
As with COPYRIGHT, 
this information is a required component of a font 
but is independent of the particular format used to represent it.
.LP
NOTICE cannot be calculated or approximated if not provided as a font property.
.NH 3
DESTINATION 
.XS
\*(SN DESTINATION
.XE
.LP
DESTINATION is an unsigned integer code (of type CARD32)
that gives the font design destination, 
that is, whether it was designed as a screen proofing font to match 
printer font glyph widths (WYSIWYG), as an optimal video font (possibly with 
corresponding printer font) for extended screen viewing (video text), and so on.
.LP
The font design considerations are very different, 
and at current display resolutions, 
the readability and legibility of these two kinds of screen fonts 
are very different.
DESTINATION allows publishing clients that use X to model the printed page
and video text clients, such as on-line documentation browsers, 
to query for X screen fonts that suit their particular requirements.
.LP
The encoding is as follows:
.TS H
lw(.5i) lw(1i) lw(3.75i).
_
.sp 6p
.B
Code	English Translation	Description
.sp 6p
_
.sp 6p
.TH
.R
0	WYSIWYG	T{
The font is optimized to match the typographic design and metrics of an 
equivalent printer font.
T}
1	Video text	T{
The font is optimized for screen legibility and readability.
T}
.sp 6p
_
.TE

.NH 3
FONT_TYPE
.XS
\*(SN FONT_TYPE
.XE
.LP
FONT_TYPE is a human-understandable string (of type ATOM) that
describes the format of
the font data as they are read from permanent storage by the current font source.
It is a static attribute of the source data.  It can be used
by clients to select a type of bitmap or outline font
without regard to the rasterizer used to render the font.
.LP
Predefined values are as follows:
.TS H
l lw(5i).
_
.sp 6p
.B
Value	When applicable
.sp 6p
_
.sp 6p
.TH
.R
\*QBitmap\*U	T{
Hand-tuned bitmap fonts.  Some
attempt has been made to optimize
the visual appearance of the font
for the requested size and
resolution.
T}
\*QPrebuilt\*U	T{
All bitmap format fonts that
cannot be described as \*QBitmap\*U,
that is, hand-tuned.  For example,
a bitmap format font that was
generated mechanically using a
scalable font rasterizer
would be considered \*QPrebuilt\*U, not \*QBitmap\*U.
T}
\*QType 1\*U	Any Type 1 font.
\*QTrueType\*U	Any TrueType font.
\*QSpeedo\*U	Any Speedo font.
\*QF3\*U	Any F3 font.
.sp 6p
_
.TE
.LP
Other values may be registered with the X Consortium.
.NH 3
FONT_VERSION
.XS
\*(SN FONT_VERSION
.XE
.LP
FONT_VERSION is a human-understandable string (of type ATOM)
that describes the formal or informal version of the font.
\fBNone\fP is a valid value.
.NH 3
RASTERIZER_NAME
.XS
\*(SN RASTERIZER_NAME
.XE
.LP
RASTERIZER_NAME is a human-understandable string (of type ATOM)
that is the specific name of the
rasterizer that has performed some rasterization operation
(such as scaling from outlines) on this font.
.LP
To define a RASTERIZER_NAME, the following format is
recommended:
.SM
.TS
rw(1.5i) lw(3.75i).
RasterizerName ::=	OrganizationId Space Rasterizer
OrganizationId ::=	T{
STRING8\*-the X Registry ORGANIZATION name
of the rasterizer implementor or maintainer.
T}
Rasterizer ::=	T{
the case-sensitive, human-understandable product name
of the rasterizer.  Words within this
name should be separated by a single SPACE.
T}
Space ::=	OCTET\-\*Q\0\*U (SPACE)
.TE
.NL
.LP
Examples:
.nf
		X Consortium Bit Scaler
		X Consortium Type 1 Rasterizer
		X Consortium Speedo Rasterizer
		Adobe Type Manager
		Sun TypeScaler
.fi
.LP
If RASTERIZER_NAME is not defined, or is \fBNone\fP, no
rasterization operation has been applied to the FONT_TYPE.
.NH 3
RASTERIZER_VERSION
.XS
\*(SN RASTERIZER_VERSION
.XE
.LP
RASTERIZER_VERSION is a human-understandable string (of type
ATOM) that represents the formal or informal version of a
font rasterizer.
The RASTERIZER_VERSION should match the corresponding
product version number known to users, when applicable.
.NH 3
RAW_ASCENT
.XS
\*(SN RAW_ASCENT
.XE
.LP
For a font with a transformation matrix, RAW_ASCENT is the font ascent
in 1000 pixel metrics
(see section \n(sM.1).
.NH 3
RAW_DESCENT
.XS
\*(SN RAW_DESCENT
.XE
.LP
For a font with a transformation matrix, RAW_DESCENT is the font
descent in 1000 pixel metrics
(see section \n(sM.1).
.NH 3
RAW_*
.XS
\*(SN RAW_*
.XE
.LP
For a font with a transformation matrix, 
all font properties that represent horizontal or vertical sizes or
displacements will be accompanied by a new property, named as the
original except prefixed with \*QRAW_\*U, that is computed as
described in section \n(sM.1.
.NH 3
AXIS_NAMES
.XS
\*(SN AXIS_NAMES
.XE
.LP
AXIS_NAMES is a list of all the
names of the axes for a polymorphic font, separated by a null (0) byte.
These names are suitable for presentation in a user interface
(see section \n(sP).
.NH 3
AXIS_LIMITS
.XS
\*(SN AXIS_LIMITS
.XE
.LP
AXIS_LIMITS is a list of integers, two for each axis,
giving the minimum and maximum allowable values for that axis of a
polymorphic font
(see section \n(sP).
.NH 3
AXIS_TYPES
.XS
\*(SN AXIS_TYPES
.XE
.LP
AXIS_TYPES is like AXIS_NAMES,
but can be registered as having specific semantics
(see section \n(sP).
.NH 2
Built-in Font Property Atoms
.XS
\*(SN Built-in Font Property Atoms
.XE
.LP
The following font property atom definitions were predefined in the initial 
version of the core protocol:
.TS H
l l.
_
.sp 6p
.B
Font Property/Atom Name	Property Type
.sp 6p
_
.sp 6p
.TH
.R
MIN_SPACE	INT32
NORM_SPACE	INT32
MAX_SPACE	INT32
END_SPACE	INT32
SUPERSCRIPT_X	INT32
SUPERSCRIPT_Y	INT32
SUBSCRIPT_X	INT32
SUBSCRIPT_Y	INT32
UNDERLINE_POSITION	INT32
UNDERLINE_THICKNESS	INT32
STRIKEOUT_ASCENT	INT32
STRIKEOUT_DESCENT	INT32
FONT_ASCENT	INT32
FONT_DESCENT	INT32
ITALIC_ANGLE	INT32
X_HEIGHT	INT32
QUAD_WIDTH	INT32 \- deprecated
WEIGHT	CARD32
POINT_SIZE	INT32
RESOLUTION	CARD32 \- deprecated
COPYRIGHT	ATOM
FULL_NAME	ATOM \- deprecated
FAMILY_NAME	ATOM
DEFAULT_CHAR	CARD32
.sp 6p
_
.TE
.br
.\" section \n(sM
.NH 1
Matrix Transformations
.XS
\*(SN Matrix Transformations
.XE
.LP
An XLFD name presented to the server can have the POINT_SIZE or PIXEL_SIZE
field begin with the character \*Q[\*U.  If the first character of the field
is \*Q[\*U, the character must be followed with ASCII representations of
four floating point numbers and a trailing \*Q]\*U, with white space
separating the numbers and optional white space separating the numbers
from the \*Q[\*U and \*Q]\*U characters.  Numbers use standard floating point
syntax but use the character \*Q\^~\^\*U to represent a minus sign in the mantissa
or exponent.
.LP
The BNF for a matrix transformation string is as follows:
.SM
.TS
rw(1.5i) l.
MatrixString ::=	T{
LeftBracket OptionalSpace Float Space Float Space
Float Space Float OptionalSpace RightBracket
T}
OptionalSpace ::=	\*Q\*U | Space
Space ::=	SpaceChar | SpaceChar Space
Float ::=	Mantissa | Mantissa Exponent
Mantissa ::=	Sign Number | Number
Sign ::=	Plus | Tilde
Number ::=	Integer | Integer Dot Integer | Dot Integer
Integer ::=	Digit | Digit Integer
Digit ::=	\*Q0\*U | \*Q1\*U | \*Q2\*U | \*Q3\*U | \*Q4\*U | \*Q5\*U | \*Q6\*U | \*Q7\*U | \*Q8\*U | \*Q9\*U
Exponent ::=	\*Qe\*U SignedInteger | \*QE\*U SignedInteger
SignedInteger ::=	Sign Integer | Integer
LeftBracket ::=	OCTET \- \*Q[\*U (LEFT SQUARE BRACKET)
RightBracket ::=	OCTET \- \*Q]\*U (RIGHT SQUARE BRACKET)
SpaceChar ::=	OCTET \- \*Q\0\*U (SPACE)
Tilde ::=	OCTET \- \*Q\^~\^\*U (TILDE)
Plus ::=	OCTET \- \*Q+\*U (PLUS)
Dot ::=	OCTET \- \*Q\^.\^\*U (FULL STOP)
.TE
.NL
.LP
The string \*Q[a b c d]\*U represents a graphical transformation of the glyphs
in the font by the matrix
.TS
c c c c c.
[	a	b	0	]
[	c	d	0	]
[	0	0	1	]
.TE
.LP
All transformations occur around the origin of the glyph.  The
relationship between the current scalar values and the matrix
transformation values is that the scalar value \*QN\*U in the POINT_SIZE field
produces the same glyphs as the matrix \*Q[N/10 0 0 N/10]\*U in that field,
and the scalar value \*QN\*U in the PIXEL_SIZE field produces the same glyphs
as the matrix \*Q[N*RESOLUTION_X/RESOLUTION_Y 0 0 N]\*U in that field.
.LP
If matrices are specified for both the POINT_SIZE and PIXEL_SIZE, they
must bear the following relationship to each other within an
implementation-specific tolerance:
.br
	PIXEL_SIZE_MATRIX = [Sx 0 0 Sy] * POINT_SIZE_MATRIX
.br
where
.br
	Sx = RESOLUTION_X / 72.27
.br
	Sy = RESOLUTION_Y / 72.27
.LP
If either the POINT_SIZE or PIXEL_SIZE field is unspecified (either \*Q0\*U or
wildcarded), the preceding formulas can be used to compute one from the
other.
.\"
.NH 2
Metrics and Font Properties
.XS
\*(SN Metrics and Font Properties
.XE
.LP
In this section, the phrase \*Q1000 pixel metrics\*U means the
metrics that would be obtained if the rasterizer took the base untransformed
design used to generate the transformed font and scaled it linearly to a
height of 1000 pixels, with no rotation component.  Note that there may be no
way for the application to actually request this font since the rasterizer
may use different outlines or rasterization techniques at that size from the
ones used to generate the transformed font.
.LP
Notes on properties and metrics:
.LP
The per-char ink metrics (lbearing, rbearing, ascent, and descent)
represent the ink extent of the transformed glyph around its origin.
.LP
The per-char width is the x component of the transformed character width.
.LP
The font ascent and descent are the y component of the transformed font
ascent or descent.
.LP
The FONT property returns a name reflecting the matrix being
used\*-that is, the name returned can be used to open another
instance of the same font.  The returned name is not necessarily an
exact copy of the requested name.  If, for example, the user
requests
.br
.ft C
.SM
   \-misc\-fixed\-medium\-r\-normal\-\-0\-[2e1 0 0.0 +10.0]\-72\-72\-c\-0\-iso8859\-1
.NL
.ft P
.br
the resulting FONT property might be
.br
.ft C
.SM
   \-misc\-fixed\-medium\-r\-normal\-\-[19.9 0 0 10]\-[20 0 0 10]\-72\-72\-c\-0\-iso8859\-1
.NL
.ft P
.br
The FONT property will always include matrices in both the PIXEL_SIZE
and the POINT_SIZE fields.
.LP
To allow accurate client positioning of transformed characters, the
attributes field of the XCharInfo contains the width of the character in
1000 pixel metrics.  This attributes field should be interpreted as a signed
integer.
.LP
There will always be 2 new font properties defined, RAW_ASCENT and
RAW_DESCENT, that hold the ascent and descent in 1000 pixel metrics.
.LP
All font properties that represent horizontal widths or displacements
have as their value the x component of the transformed width or
displacement.  All font properties that represent vertical heights or
displacements have as their value the y component of the transformed
height or displacement.  Each such property will be accompanied by a new
property, named as the original except prefixed with \*QRAW_\*U, that gives
the value of the width, height, or displacement in 1000 pixel metrics.
.NH 1
Scalable Fonts
.XS
\*(SN Scalable Fonts
.XE
.LP
The XLFD is designed to support scalable fonts.  A scalable font is a
font source from which instances of arbitrary size can be derived.
A scalable font source might be one or more outlines
together with zero or more hand-tuned bitmap fonts at specific sizes and
resolutions, or it might be a programmatic description together with
zero or more bitmap fonts, or some other format
(perhaps even just a single bitmap font).
.LP
The following definitions are useful for discussing scalable fonts:
.LP
\fBWell-formed XLFD pattern\fP
.IP
A pattern string containing 14 hyphens, one of which is
the first character of the pattern.  Wildcard characters are permitted
in the fields of a well-formed XLFD pattern.
.LP
\fBScalable font name\fP
.IP
A well-formed XLFD pattern containing no wildcards and containing the
digit \*Q0\*U in the PIXEL_SIZE, POINT_SIZE, and AVERAGE_WIDTH fields.
.LP
\fBScalable fields\fP
.IP
The XLFD fields PIXEL_SIZE, POINT_SIZE, RESOLUTION_X,
RESOLUTION_Y, and AVERAGE_WIDTH.
.LP
\fBDerived instance\fP
.IP
The result of replacing the scalable fields of a font name
with values to yield a font name that could actually be
produced from the font source.  A scaling engine is
permitted, but not required, to interpret the scalable
fields in font names to support anamorphic scaling.
.LP
\fBGlobal list\fP
.IP
The list of names that would be returned by an X server for a
.PN ListFonts
protocol request on the pattern \*Q*\*U if there were no protocol
restrictions on the total number of names returned.
.sp
.LP
The global list consists of font names derived from font sources.
If a single font source can support multiple character sets (specified
in the CHARSET_REGISTRY and CHARSET_ENCODING fields), each such character
set should be used to form a separate font name in the list.
For a nonscalable font source, the simple font name
for each character set is included in the global list.
For a scalable font source, a scalable font name for each character set
is included in the list.  In addition to the scalable font name,
specific derived instance names may also be included in the list.
The relative order of derived instances with respect to the scalable
font name is not constrained.  Finally, font name aliases may also be included
in the list.  The relative order of aliases
with respect to the real font name is not constrained.
.LP
The values of the RESOLUTION_X and RESOLUTION_Y fields of a scalable font name
are implementation dependent,
but to maximize backward compatibility, they
should be reasonable nonzero values, for example, a resolution close to that
provided by the screen (in a single-screen server).
Because some existing
applications rely on seeing a collection of point and pixel sizes,
server vendors are strongly encouraged in the near term to
provide a mechanism for including, for each scalable font name,
a set of specific derived instance names.  For font sources that contain
a collection of hand-tuned bitmap fonts, including names of these instances
in the global list is recommended and sufficient.
.LP
The X protocol request
.PN OpenFont
on a scalable font name returns a font corresponding to an
implementation-dependent derived instance of that font name.
.LP
The X protocol request
.PN ListFonts
on a well-formed XLFD pattern returns the following.
Starting with the global list, if the actual pattern argument
has values containing no wildcards in scalable fields,
then substitute each such field into the corresponding
field in each scalable font name in the list.  For each resulting font name,
if the remaining scalable fields cannot be replaced with values to produce a
derived instance, remove the font name from the list.  Now take the modified
list, and perform a simple pattern match against the pattern argument.
.PN ListFonts
returns the resulting list.
.LP
For example, given the global list:
.DS
-Linotype-Times-Bold-I-Normal--0-0-100-100-P-0-ISO8859-1
-Linotype-Times-Bold-R-Normal--0-0-100-100-P-0-ISO8859-1
-Linotype-Times-Medium-I-Normal--0-0-100-100-P-0-ISO8859-1
-Linotype-Times-Medium-R-Normal--0-0-100-100-P-0-ISO8859-1
.DE
.LP
a
.PN ListFonts
request with the pattern:
.LP
.DS
-*-Times-*-R-Normal--*-120-100-100-P-*-ISO8859-1
.DE
.LP
would return:
.DS
-Linotype-Times-Bold-R-Normal--0-120-100-100-P-0-ISO8859-1
-Linotype-Times-Medium-R-Normal--0-120-100-100-P-0-ISO8859-1
.DE
.LP
.PN ListFonts
on a pattern containing wildcards that is not a well-formed XLFD
pattern is only required to return the list obtained by performing
a simple pattern match against the global list.
X servers are permitted, but not required,
to use a more sophisticated matching algorithm.
.br
.\" section \n(sP
.NH 1
Polymorphic Fonts
.XS
\*(SN Polymorphic Fonts
.XE
.LP
Fonts that can be varied in ways other than size or resolution are called
\fIpolymorphic fonts.\fP  Multiple Master Type 1 font programs are one type of
a polymorphic font.  Current examples of axes along which the fonts can be
varied are width, weight, and optical size; others might include formality
or x-height.
.LP
To support polymorphic fonts, special values indicating variability are
defined for the following XLFD fields:
.nf
	WEIGHT_NAME
	SLANT
	SETWIDTH_NAME
	ADD_STYLE_NAME
.fi
.LP
The string \*Q0\*U is the special polymorphic value.  In the
WEIGHT_NAME, SLANT, or SETWIDTH_NAME field, \*Q0\*U must be the
entire field.
There may be multiple polymorphic values
in the ADD_STYLE_NAME field.
They are surrounded by \*Q[\*U and \*Q]\*U and separated by a Space,
as \*Q[0\00]\*U.  The polymorphic values may coexist with
other data in the field.
It is recommended that the polymorphic values
be at the end of the ADD_STYLE_NAME field.
.LP
The font-matching algorithms for a font with polymorphic fields are
identical to the matching algorithms for a font with scalable fields.
.LP
There are three new font properties to describe the axes of variation,
AXIS_NAMES, AXIS_LIMITS, and AXIS_TYPES.  AXIS_NAMES is a list of all the
names of the axes for the font, separated by a null (0) byte.
These names are suitable for presentation in
a user interface.  AXIS_LIMITS is a list of integers, two for each axis,
giving the minimum and maximum allowable values for that axis.
AXIS_TYPES is like AXIS_NAMES,
but can be registered as having specific semantics.
.LP
The axes are listed in the properties in the same order as they
appear in the font name.  They are matched with font name fields by
looking for the special polymorphic values in the font name.
.LP
Examples:
.LP
The Adobe Myriad MM font program has width and weight axes.  Weight can
vary from 215 to 830, and width from 300 to 700.
.\" indented display
.ID
.SM
Name:
.ft C
	-Adobe-Myriad MM-0-R-0--0-0-0-0-P-0-ISO8859-1
.ft P
AXIS_NAMES:
	Weight, Width
AXIS_LIMITS:
	215, 830, 300, 700
AXIS_TYPES:
	Adobe-Weight, Adobe-Width
Sample derived instance:
.ft C
	-Adobe-Myriad MM-412-R-575--*-120-100-100-P-*-ISO8859-1
.ft P
.NL
.DE                     \" display end
.LP
The Adobe Minion MM Italic font program has width, weight, and optical
size axes.
.ID
.SM
Name:
.ft C
	-Adobe-Minion MM-0-I-0-[0]-0-0-0-0-P-0-ISO8859-1
.ft P
AXIS_NAMES:
	Weight, Width, Optical size
AXIS_LIMITS:
	345, 620, 450, 600, 6, 72
AXIS_TYPES:
	Adobe-Weight, Adobe-Width, Adobe-OpticalSize
Sample derived instance:
.ft C
	-Adobe-Minion MM-550-I-480-[18]-*-180-100-100-P-*-ISO8859-1
.ft P
.NL
.DE
.LP
The Adobe Minion MM Swash Italic font program has the same axes and
values.  This shows how
\*Q[0]\*U in the ADD_STYLE_NAME field can
coexist with other words.
.ID
.SM
Name:
.ft C
	-Adobe-Minion MM-0-I-0-Swash[0]-0-0-0-0-P-0-ISO8859-1
.ft P
AXIS_NAMES:
	Weight, Width, Optical size
AXIS_LIMITS:
	345, 620, 450, 600, 6, 72
AXIS_TYPES:
	Adobe-Weight, Adobe-Width, Adobe-OpticalSize
Sample derived instance:
.ft C
	-Adobe-Minion MM-550-I-480-Swash[18]-*-180-100-100-P-*-ISO8859-1
.ft P
.NL
.DE
.LP
The XYZ Abc font, a hypothetical font, has optical size and x-height axes.
This shows how there can be more than one polymorphic value in the
ADD_STYLE_NAME field.
.ID
.SM
Name:
.ft C
	-XYZ-Abc-Medium-R-Normal-[0 0]-0-0-0-0-P-0-ISO8859-1
.ft P
AXIS_NAMES:
	Optical size, X-height
AXIS_LIMITS:
	6, 72, 400, 600
AXIS_TYPES:
	XYZ-OpticalSize, XYZ-Xheight
Sample derived instance:
.ft C
	-XYZ-Abc-Medium-R-Normal-[14 510]-*-140-100-100-P-*-ISO8859-1
.ft P
.NL
.DE
.LP
If an axis allows negative values, a client requests a negative value by
using \*Q\^~\^\*U (TILDE) as a minus sign.
.LP
Axis types can be registered with the X Consortium, along with their
semantics.
.LP
If a font name that contains the polymorphic value or a wildcard in a
polymorphic field is presented to a font source, the font source is free
to substitute any value that is convenient.  However, font sources should
try to use a value that would be considered \fInormal\fP or \fImedium\fP for the
particular font.  For example, if an optical size variable is unresolved,
the font source should provide a value appropriate to the size of the
font.
.LP
The result of specifying an out-of-range value for a polymorphic field is
undefined.  The font source may treat this as a \fBBadName\fP error, treat the
value as if it were the closest legal value, or extrapolate to try to
accommodate the value.
.NH 1
Affected Elements of Xlib and the X Protocol
.XS
\*(SN Affected Elements of Xlib and the X Protocol
.XE
.LP
The following X protocol requests must support the XLFD conventions:
.IP \(bu 5
.PN OpenFont
\- for the name argument
.IP \(bu 5
.PN ListFonts
\- for the pattern argument
.IP \(bu 5
.PN ListFontsWithInfo
\- for the pattern argument
.LP
In addition, 
the following Xlib functions must support the XLFD conventions:
.IP \(bu 5
.PN XLoadFont
\- for the name argument
.IP \(bu 5
.PN XListFontsWithInfo
\- for the pattern argument
.IP \(bu 5
.PN XLoadQueryFont
\- for the name argument
.IP \(bu 5
.PN XListFonts
\- for the pattern argument
.NH 1
BDF Conformance
.XS
\*(SN BDF Conformance
.XE
.LP
The bitmap font distribution and interchange format adopted by the 
X Consortium (BDF V2.1) provides a general mechanism for identifying the 
font name of an X font and a variable list of font properties, 
but it does not mandate the syntax or semantics of the font name 
or the semantics of the font properties that might be provided in a BDF font.
This section identifies the requirements for BDF fonts that conform to XLFD.
.NH 2
XLFD Conformance Requirements
.XS
\*(SN XLFD Conformance Requirements
.XE
.LP
A BDF font conforms to the XLFD specification if and only if the 
following conditions are satisfied:
.IP \(bu 5
The value for the BDF item \fBFONT\fP conforms to the syntax 
and semantic definition of a XLFD 
.PN FontName 
string.
.IP \(bu 5
The 
.PN FontName 
begins with the X 
.PN FontNameRegistry 
prefix: \*Q\-\*U.
.IP \(bu 5
All XLFD 
.PN FontName 
fields are defined.
.IP \(bu 5
Any FontProperties provided conform in name and semantics to the XLFD 
.PN FontProperty 
definitions.
.LP             
A simple method of testing for conformance would entail verifying that the 
.PN FontNameRegistry 
prefix is the string \*Q\-\*U, 
that the number of field delimiters in the string and coded field values 
are valid, 
and that each font property name either matches a standard XLFD property name 
or follows the definition of a private property.
.NH 2
FONT_ASCENT, FONT_DESCENT, and DEFAULT_CHAR
.XS
\*(SN FONT_ASCENT, FONT_DESCENT, and DEFAULT_CHAR
.XE
.LP
FONT_ASCENT, FONT_DESCENT, and DEFAULT_CHAR are provided in the BDF 
specification as properties that are moved to the 
.PN XFontStruct 
by the BDF font compiler in generating the X server-specific 
binary font encoding.
If present, 
these properties shall comply with the following semantic definitions.
.NH 3
FONT_ASCENT
.XS
\*(SN FONT_ASCENT
.XE
.LP
FONT_ASCENT is an integer value (of type INT32)
that gives the recommended typographic ascent above the baseline 
for determining interline spacing.
Specific glyphs of the font may extend beyond this.
If the current position point for line \fIn\fP is at [X,Y], 
then the origin of the next line \fIm = n + 1\fP
(allowing for a possible font change) is 
[X, Y + FONT_DESCENTn + FONT_ASCENTm].
.LP
FONT_ASCENT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (FONT_ASCENT undefined) then
   FONT_ASCENT = maximum ascent
.DE
where maximum ascent is the maximum ascent (above the baseline)
in pixels of any glyph in the font.
.NH 3
FONT_DESCENT
.XS
\*(SN FONT_DESCENT
.XE
.LP
FONT_DESCENT is an integer value (of type INT32)
that gives the recommended typographic descent below the baseline
for determining interline spacing.
Specific glyphs of the font may extend beyond this.
If the current position point for line \fIn\fP is at [X,Y],
then the origin of the next line \fIm = n+1\fP
(allowing for a possible font change) is 
[X, Y + FONT_DESCENTn + FONT_ASCENTm].
.LP
The logical extent of the font is inclusive between the Y-coordinate values: 
Y \- FONT_ASCENT and Y + FONT_DESCENT + 1.
.LP
FONT_DESCENT can be approximated if not provided as a font property, 
according to the following algorithm:
.LP
.DS
if (FONT_DESCENT undefined) then
   FONT_DESCENT = maximum descent
.DE
where maximum descent is the maximum descent (below the baseline)
in pixels of any glyph in the font.
.NH 3
DEFAULT_CHAR
.XS
\*(SN DEFAULT_CHAR
.XE
.LP
The DEFAULT_CHAR is an unsigned integer value (of type CARD32)
that specifies the index
of the default character to be used by the X server when an attempt
is made to display an undefined or nonexistent character in the font.
(For a font using a 2-byte matrix format,
the index bytes are encoded in the integer as byte1 * 65536 + byte2.)
If the DEFAULT_CHAR itself specifies an undefined or nonexistent character 
in the font, 
then no display is performed.
.LP
DEFAULT_CHAR cannot be approximated if not provided as a font property.
.\"
.\" print Table of Contents
.if o .bp \" blank page to make count even
.bp 1
.af PN i
.PX
