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

.de Ds		\" Start Description of function
.LP
.ti -0.5i
\fBDescription:\fP
.LP
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

.de 2e		\" Description of a 2d primtive
When processed, this command will cause a \\$1 primitive to be drawn.
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
.nr LL 6.5i
.nr PD 0.1i
.nr HM 1.2i
.nr FM 1.0i
.nr PO 1.0i
.nh
.DA "PEX Version 5.1, 31-August-1992"
.EH ''-%-''
.OH ''-%-''
.nr % 1
.AC "Acknowledgements" 1
.nh
.LP
.RU
.LP
On behalf of the M.I.T. X Consortium, we would like to thank all who
have participated in the design, definition, and implementation of
PEX.
.LP
This work began when, at the first X3D meeting at MIT in June, 1987,
Digital Equipment Corporation and Sun Microsystems, Inc. jointly proposed
that a protocol be defined based upon an initial working draft developed
by Digital.  This draft was influenced by comments from professional
colleagues.  We recognize especially the authors of that first document:
Randi J. Rost, Jeffrey Friedberg, Jeffrey S. Saltz, Pete Nishimoto, and
William H. Clifford, Jr., all of Digital Equipment Corporation.  They
provided a head start for this major undertaking.
.LP
We owe a major debt to the PEX architecture team which met frequently
between June, 1987 and August, 1991 to argue and debate issues.  Members 
of the PEX Architecture Team: 
Jeffrey Friedberg, John McConnell, Pete Nishimoto, Randi J. Rost, 
and Jeffrey S. Saltz of Digital Equipment Corporation;
Dave Gorgen, Tom Gross, and Jeff Stevenson of Hewlett-Packard Company;
David Plunkett of Solbourne Computer;
Jan Hardenbergh of Stardent Computer, Inc.;
and Marty Hess, and Jim Van Loo of Sun Microsystems.
.LP
Our sincerest thanks are also extended to Bertram Herzog of the University
of Michigan, who served as chairman of the X3D organization and was
responsible for overseeing a fair and timely process designed to bring the
PEX project to fruition.
.LP
Several members of the X3D group provided valuable comments to the 
PEX architecture team.  We note especially the contributions of
James Michener of Hewlett-Packard Company, and Todd
Newman and Raymond Drewry of Digital Equipment Corporation who attended
architecture team meetings and provided much-needed insight
at critical points during the evolution of PEX.
Others who
have participated actively in technical discussions include Sally C. Barry,
Andy Vesper, and Ray Shapiro of Digital Equipment Corporation.
.LP
We also give thanks to early implementors of PEX, including the
PEX Sample Implementation team at Sun Microsystems, for providing
useful review and sanity checks of the PEX specification documents.
The PEX-SI team at Sun includes Marty Hess, Lisa Chabot, Cheryl Huntington,
Erwin Hom, Nash Aragam, Tom Gaskins, John Recker, and Chris Nicholas.
.LP
Robert Scheifler, Director of the X Consortium at M.I.T., provided
invaluable critique and advice that ensured that our efforts conformed
to the spirit and definition of the X Window System.  Jim Fulton of
the X Consortium also contributed to meetings and to technical discussions.
.LP
To Randi Rost, the first PEX Document Editor, go the thanks of all, for
translating all the decisions into the written word.
Sally Barry and Jeff Friedberg are gratefully acknowledged for providing
thorough and painstaking review of many intermediate drafts of 
these early PEX documents.
Sally Barry, document editor for PEX 5.0, gratefully acknowledges the
invaluable assistance and encouragement of Jeff Friedberg, Jan Hardenbergh,
and Randi Rost, during the production of the PEX 5.0 specifications. 
Paula Womack, document editor for PEX 5.1, acknowledges Ken
Garnett of Shographics, for his thorough and timely review of the
protocol specification. 
.LP
In addition the careful review by the
PEX teams at all the various companies that are working on PEX implementations
has greatly helped the accuracy and precision of this document.
.LP
Thanks also to the individuals that worked on the X11, PHIGS, and PHIGS-PLUS
specification documents, from which some PEX descriptions were obtained.
.LP
Finally, we acknowledge the contributions of all of the other participants
in this effort that have not been explicitly named.  The
contributions of so many individuals has helped to ensure that PEX
will be a useful and long-lived extension to the X Window System.
.bp
.AC "PEX Protocol Specification" 1
.LP
.RU
.LP
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
All PEX\(dg protocol formats are based on the formats specified by the 
\fIX Window System Protocol, Version 11\fP\(dd,
which is referred to as X11 throughout the remainder of this
document.
The PEX protocol will adhere to the philosophy and requirements of X11.
.FS
.br
.sp
\(dg X3D-PEX and PEX are trademarks of the 
Massachusetts Institute of Technology
.FE
.FS
.br
.sp
\(dd"X Window System" is a trademark of the 
Massachusetts Institute of Technology
.FE

.AC "Request Format" 3
.LP
Every X11 request contains a 4-byte header which contains an 8-bit
major opcode, an 8-bit data field, and a 16-bit length field.  The header
is followed by zero or more additional bytes of data, the length of
which is specified by the length field.  Since PEX is a proper extension
of X11, the 8-bit major opcode contains the opcode assigned for
the PEX extension by X11.  The length field contains the length of the
request in units of four bytes,
including the header.  The data (or minor opcode) field contains the
PEX opcode for this request.

.AC "Reply Format" 3
.LP
Every reply consists of at least 32 bytes.  A header is contained in
these 32 bytes.
The header of a reply
consists of
a 32-bit length field, a 16-bit sequence number field, and an 8-bit
type field.  Zero or
more additional bytes follow the header as specified in the length field.  The 
length field specifies the length of the data following the 32 byte reply
header and is in units of four bytes.
Unused bytes within a reply
are not guaranteed to be zero.
The sequence number field 
contains the least significant 16 bits of the sequence number of
the corresponding request.
The type field defines the type of reply generated.

.AC "Error Format" 3
.LP
Error reports are 32 bytes long.  Every error includes an 8-bit 
error code.
This error code is used to signify the specific PEX error that occurred.
Every error reply also includes the major opcode
(the extension reporting the error is identified by the major opcode),
the minor opcode
(the extension opcode which caused the error),
and the least significant 16 bits
of the sequence number of the request which had failed.  Also included
is an 8-bit type field which designates the packet as being an
error packet.  Unused bytes
within an error are not guaranteed to be zero.
               
.AC "Event Format" 3
.LP
Events are 32 bytes long.  Every event contains an 8-bit type code.
The most significant bit in this field is set if the event was generated
from a \fBSendEvent\fP request.  Event codes 64-127 are reserved for extensions.
The core X11 protocol does not define a mechanism for expressing interest
in events generated by extensions.

.bp
.Fs
.AC "Syntax" 2
.LP
Curly braces {...} enclose a set of alternatives.
Square brackets [...] enclose a list of structure components.
When embedded in descriptions, request names are printed in boldface
(e.g., \fBPEXCreateStructure\fP).
Request parameters are lower case, use the underscore (_) for separation,
and are printed in italics (e.g., \fIitem_mask\fP).
Defined constants, registered enumerated type mnemonics, or alternative
values have an initial capital letter, may use capital letters
for separation, and are printed in italics (e.g., \fIRGBFloat\fP).
Defined types are printed in all caps, use the underscore
for separation, and are printed in the standard font (e.g.,
COORD_3D).
.sp
.LP
Requests are described as follows:
.Fs
.Na PEXSampleRequest
.Rq
.Pa arg1 type1
.Pa argN typeN
.Re
.Pa result1 type1
.Pa resultM typeM
.Se                    
kind1,..., kindK
.Ds 
Functional description goes here
.Fe
.LP
If no reply description is given,
then the request has no 
reply (it is asynchronous), but errors may still be reported.

.AC "Naming Conventions" 2
.LP
PEX requests use a consistent naming convention.  The verbs that are
commonly used in request names are described here.
.DS
.ta 1.2i
\fICreate\fP	Create an instance of a resource
\fIFree\fP	Mark a resource as no longer accessible by clients, and deallocate the system
	resources it uses (e.g. memory) if it is not referenced by any other resources
\fICopy\fP	Copy attributes from one resource to another of the same type
\fIGet\fP	Return resource attributes from the server to the client
\fIChange\fP	Modify attributes of a resource
\fISet\fP	Modify a selected attribute of a resource
\fIDestroy\fP	Remove an instance of a resource and all references to it, and deallocate
	the memory associated with it
\fIDelete\fP	Remove some portion of a resource
\fIFetch\fP	Return structure elements from the server to the client
\fIStore\fP	Send structure elements from the client to a structure resource in the server
\fIBegin\fP	Perform an initialization step of some kind
\fIEnd\fP	Perform a termination step of some kind
.ta
.DE
.bp
.AC "Common Types" 2
.LP
The types listed in this section define the common types used in
the PEX protocol specification.
.AC "LISTofFOO" 3
.LP
A type name of the form LISTofFOO means a counted list of elements of
type FOO; the size of the length field may vary (it is not necessarily
the same size as FOO).  In cases where the number of items in the list
is easily computed, the number of items may not be supplied.  In all other cases
in the PEX protocol
(except for LISTofVALUE), the length
field is explicit.
.AC "BITMASK and LISTofVALUE" 3
.LP
The types BITMASK
and LISTofVALUE are somewhat special.  Various requests
contain arguments of the form:
.DS
	\fIitem_mask\fP : BITMASK
	\fIitem_list\fP   : LISTofVALUE
.DE
used to allow the client to specify a subset of a heterogeneous collection
of "arguments".  The \fIitem_mask\fP specifies which 
arguments are to be provided;
each such argument is assigned a unique bit position.  
The representation of BITMASK may contain more bits
than there are defined arguments; unused bits in the  \fIitem_mask\fP 
must be zero (or the extension will generate a \fIValue\fP error).
The \fIitem_list\fP
contains
one item for each one bit in the mask, from least to most significant bit
in the mask.   

.AC "Floating Point Format - FLOAT" 3
.LP
The PEX protocol allows floating-point values to be passed in
various floating-point formats.
All floating-point arguments will
be specified as FLOAT, which is defined to be the
floating-point type contained in the format word associated with the
request.  Furthermore, items such as MATRIX, VECTOR, and COORD
will be in the floating-point format specified
by the format word associated with the request.

.AC "Colors" 3
.LP
In PEX, colors are typically passed as a color type and a value.
The color type specifies whether
the color is an index value or a direct color value of some type.
PEX servers are required to be able to
deal with indexed colors
and at least one type of direct color.
Indexed colors are specified using an index which is used to obtain
the color from a color lookup table.
Direct colors are specified
directly as RGB, HSV, HLS, or CIELUV color values of some form.
The list of registered direct color formats can be found in the
"Extension Information" section.
PEX servers are free to store direct color values
in whatever implementation-dependent format they choose, but
they must be capable of converting those values back into the
originally-specified color type when queried by the client.

.AC "Element Types" 3
.LP
Chapter 3 describes the set of output commands that are recognized
by a PEX implementation.  These output commands are distinguished
by a 16-bit ELEMENT_TYPE value.  This value contains a 16-bit unsigned
short that defines the actual type of output command.  The high-order
bit of the element type is used to signify whether the output command
is a standard PEX output command (high-order bit equals zero) or whether
the output command is a proprietary addition to the set of standard
PEX output commands.  Servers are expected to be able to create structure
elements containing non-standard PEX output commands, but the execution
of such output commands can be a no-op.
Since the contents of these output commands is unknown, no
floating-point conversions or byte-swapping will be performed on non-standard
output commands that are not supported by the server. 
Unlike the use of the PHIGS-style GSE and GDP output commands,
this extension mechanism allows vendors to gracefully add
fully-integrated functionality to the standard PEX extension, and permits
an implementation to ignore output commands
with which it is not familiar.

.AC "Types" 3
.LP
The PEX Protocol types are as follows:

.ID

ASF_ATTRIBUTE		: {\fIMarkerTypeASF, MarkerScaleASF, MarkerColorASF,
						TextFontIndexASF, TextPrecASF, CharExpansionASF,
						CharSpacingASF, TextColorASF, LineTypeASF,
						LineWidthASF, LineColorASF, CurveApproxASF,
						PolylineInterpASF, InteriorStyleASF, InteriorStyleIndexASF,
						SurfaceColorASF, SurfaceInterpASF, ReflectionModelASF,
						ReflectionAttrASF, BFInteriorStyleASF, BFInteriorStyleIndexASF,
						BFSurfaceColorASF, BFSurfaceInterpASF, BFReflectionModelASF,
						BFReflectionAttrASF, SurfaceApproxASF, SurfaceEdgesASF,
						SurfaceEdgeTypeASF, SurfaceEdgeWidthASF, SurfaceEdgeColorASF\fP}
ASF_SPECIFIER			: [enables, asfs : BITMASK]
ASF_VALUE			: {\fIBundled, Individual\fP}
ATEXT_STYLE			: ENUM_TYPE_INDEX (used with \fIATextStyle\fP enumerated type)
BITMASK				: CARD32
BITMASK_SHORT		: CARD16
BOOLEAN				: {\fIFalse, True\fP}
BUFFER_MODE			: {\fISingle, Double\fP}
CARD8				: unsigned 8-bit integer
CARD16				: unsigned 16-bit integer
CARD32				: unsigned 32-bit integer
CHARACTER			: {CARD8, CARD16, CARD32}
COLOR				: {TABLE_INDEX, DIRECT_COLOR\(dg}
CLIP_INDICATOR		: {\fIClip, NoClip\fP}
COLOR_APPROX_MODEL	: ENUM_TYPE_INDEX (used with \fIColorApproxModel\fP enumerated type)
COLOR_APPROX_TYPE	: ENUM_TYPE_INDEX (used with \fIColorApproxType\fP enumerated type)
COLOR_MODEL			: ENUM_TYPE_INDEX (used with \fIRenderingColorModel\fP enumerated type)
COLOR_SPECIFIER		: [color_type : COLOR_TYPE,
						color_value : COLOR]
COLOR_TYPE			: ENUM_TYPE_INDEX (used with \fIColorType\fP enumerated type)
COMPOSITION			: {\fIPreConcatenate, PostConcatenate, Replace\fP} 
CONSTANT_NAME		: CARD16
CONTOUR				: {\fIDisjoint, Nested, Intersecting, Unknown\fP}
COORD				: {COORD_2D, COORD_3D, COORD_4D}
COORD_2D			: [x, y : FLOAT]
COORD_3D			: [x, y, z : FLOAT] 
COORD_4D			: [wx, wy, wz, w : FLOAT]
COORD_TYPE			: {\fIRational, NonRational\fP}
CULL_MODE			: {\fINone, BackFaces, FrontFaces\fP}
CURVE_APPROX		: [approx_method : CURVE_APPROX_METHOD,
						tolerance : FLOAT]
CURVE_APPROX_METHOD : ENUM_TYPE_INDEX (used with \fICurveApproxMethod\fP enumerated type)
DC_HIT_BOX_DATA		: [pick_position: DEVICE_COORD_2D,
						pick_distance: FLOAT]
DEVICE_COORD		: [x, y : INT16, z : FLOAT]
DEVICE_COORD_2D		: [x, y : INT16]
DEVICE_RECT			: [xmin, ymin, xmax, ymax : INT16]
DIRECT_COLOR		: direct color value\(dg
.FS
\(dg See the "Extension Information" section for a list of the registered
color types.
.FE
DISPLAY_STATE		: {\fINotEmpty, Empty\fP}
DISPLAY_UPDATE		: ENUM_TYPE_INDEX (used with \fIDisplayUpdateMode\fP enumerated type)
DRAWABLE_ID			: {WINDOW_ID, PIXMAP_ID}
DYNAMIC_TYPE		: {\fIIMM, IRG, CBS\fP}
ECHO_MODE			: {\fINoEcho, Echo, Unecho\fP}
EDGE				: OPT_SWITCH
EDIT_MODE			: {\fIStructureInsert, StructureReplace\fP}
ELEMENT_INFO		: [type : ELEMENT_TYPE,
						length : CARD16]
ELEMENT_POS			: [whence : {\fIBeginning, Current, End\fP},
						offset : INT32]
ELEMENT_RANGE		: [position1, position2 : ELEMENT_POS]
ELEMENT_REF			: [structure_id : STRUCTURE_ID,
						offset : CARD32]
ELEMENT_TYPE		: CARD16
ENUM_TYPE			:{\fIMarkerType, ATextStyle, InteriorStyle, HatchStyle, 
						LineType, SurfaceEdgeType, PickDeviceType,
						PolylineInterpMethod, CurveApproxMethod, ReflectionModel,
						SurfaceInterpMethod, SurfaceApproxMethod,
						ModelClipOperator, LightType, ColorType,
						FloatFormat, HLHSRMode, PromptEchoType,
						DisplayUpdateMode, ColorApproxType, ColorApproxModel,
						GDP, GDP3, GSE, TrimCurveApproxMethod,
						RenderingColorModel, ParametricSurfaceCharacteristics,
						Escape, PickOneMethod, PickAllMethod\fP}

ENUM_TYPE_INDEX		: INT16
EXTENT_INFO			: [lower_left : COORD_2D,
						upper_right : COORD_2D,
						concatpoint : COORD_2D]
FACET				: [facet_data : OPT_DATA,
						vertices : LISTofVERTEX]
FLOAT				: floating point value\(dg
.FS
\(dg See the "Extension Information" section for a list of the registered
floating point formats.
.FE
FLOAT_FORMAT		: ENUM_TYPE_INDEX (used with \fIFloatFormat\fP enumerated type)
FONT_ID				: {PEX_FONT_ID, X11_FONT_ID}
HALFSPACE			: [point : COORD_3D,
						vector : VECTOR_3D]
HALFSPACE_2D			: [point : COORD_2D,
						vector : VECTOR_2D]
HATCH_STYLE			: ENUM_TYPE_INDEX (used with \fIHatchStyle\fP enumerated type)
HLHSR_MODE			: ENUM_TYPE_INDEX (used with \fIHLHSRMode\fP enumerated type)
INT8					: signed 8-bit integer
INT16				: signed 16-bit integer
INT32				: signed 32-bit integer
INTERIOR_STYLE		: ENUM_TYPE_INDEX (used with \fIInteriorStyle\fP enumerated type)
ISTRING				: LISTofMONO_ENCODING
LIGHT_TYPE			: ENUM_TYPE_INDEX (used with \fILightType\fP enumerated type)
LINE_TYPE			: ENUM_TYPE_INDEX (used with \fILineType\fP enumerated type)
LOOKUP_TABLE_ID		: RESOURCE_ID
MARKER_TYPE			: ENUM_TYPE_INDEX (used with \fIMarkerType\fP enumerated type)
MATCH_DRAW_TYPE	:{\fIDontCare, Window, Pixmap, Buffer\fP}
MATRIX				: FLOAT[4][4]\(dd
MATRIX_3X3			: FLOAT[3][3]\(dd
.FS
\(dd
Matrices are effectively passed as one-dimensional arrays of floating
point values.  
For a 4\(mu4 matrix, the
matrix element used to represent the x translation value will be the
fourth element in the array, the element containing the y translation value
will be the eighth element, etc.  3\(mu3 matrices are handled analogously.
.FE
MONO_ENCODING		: [char_set : CARD16,
						char_set_width : {\fIcsByte, csShort, csLong\fP},
						encoding_state : CARD8,
						string : LISTofCHARACTER]
NAME				: CARD32
NAME_SET_ID			: RESOURCE_ID
NAME_SET_PAIR		: [incl: NAME_SET_ID,
						excl: NAME_SET_ID]
NPC_SUBVOLUME		: [min : COORD_3D,
						max : COORD_3D]
OPERATOR			: ENUM_TYPE_INDEX (used with \fIModelClipOperator\fP enumerated type)
OPT_COLOR			: optional COLOR\(dg
OPT_DATA			: [color : OPT_COLOR,
						normal : OPT_NORMAL,
						edge : OPT_SWITCH ]
OPT_NORMAL			: optional VECTOR_3D\(dg
OPT_SWITCH			: optional SWITCH\(dg
.FS
\(dg
Indicates a parameter (or portion of a parameter) that
may or may not be present in the request.
However, its presence or absence can always be inferred
from previous parameters in the request.
.FE
OUTPUT_CMD			: [element_type : ELEMENT_TYPE,
						size : CARD16,
						data : \(dd ]
.FS
\(dd
See Section 3 - \fIOutput Commands\fP for a description of
each of the data records that can be passed/returned as an output command.
.FE
PC_BITMASK			: CARD32[3]
PEX_FONT_ID			: RESOURCE_ID
PHIGS_WKS_ID			: RESOURCE_ID
PIPELINE_CONTEXT_ID	: RESOURCE_ID
PICK_ALL_METHOD		: ENUM_TYPE_INDEX (used with \fIPickAllMethod\fP enumerated type)
PICK_ALL_STATE		: {MoreHits, NoMoreHits, MayBeMoreHits}
PICK_DATA			: {DC_HIT_BOX_DATA, NPC_SUBVOLUME}
PICK_DEVICE_TYPE		: ENUM_TYPE_INDEX (used with \fIPickDeviceType\fP enumerated type)
PICK_ELEMENT_REF		: [s_id : STRUCTURE_ID,
						offset : CARD32,
						pickid : CARD32]
PICK_MEASURE_ID		: RESOURCE_ID
PICK_ONE_METHOD		: ENUM_TYPE_INDEX (used with \fIPickOneMethod\fP enumerated type)
PICK_RECORD			: [pick_type: PICK_DEVICE_TYPE,
						hit_box: PICK_DATA ]
PICK_STATUS			: {NoPick, Ok, Aborted}
PIXMAP_ID			: RESOURCE_ID
POLYLINE_INTERP		: ENUM_TYPE_INDEX (used with \fIPolylineInterpMethod\fP enumerated type)
PROMPT_ECHO_TYPE	: ENUM_TYPE_INDEX (used with \fIPromptEchoType\fP enumerated type)
PSC_TYPE				: ENUM_TYPE_INDEX (used with \fIParametricSurfaceCharacteristics\fP 
						enumerated type)
PSURF_CHAR			: [psc_type : PSC_TYPE,
						psc_data : LISTofCARD8]
REFLECTION_ATTR		: [ambient_coef : FLOAT,
						diffuse_coef : FLOAT,
						specular_coef : FLOAT,
						specular_conc : FLOAT,
						transmission_coef : FLOAT,
						specular_color : COLOR_SPECIFIER]
REFLECTION_MODEL	: ENUM_TYPE_INDEX (used with \fIReflectionModel\fP enumerated type)
RENDERER_ID			: RESOURCE_ID
RENDERER_STATE		: {\fIIdle, Rendering, Picking\fP}
RENDERER_TARGET		: [depth: CARD8,
						type: MATCH_DRAW_TYPE,
						visual: VISUAL_ID ]
RESOURCE_ID			: 32-bit identifier
SEARCH_CONTEXT_ID	: RESOURCE_ID
SHAPE				: {\fIConvex, Nonconvex, Complex, Unknown\fP}
STRING				: LISTofCARD8
STRUCTURE_ID			: RESOURCE_ID
STRUCTURE_INFO		: [id: RESOURCE_ID,
						priority: FLOAT]
SURFACE_APPROX		: [approx_method : SURFACE_APPROX_METHOD,
						u_tolerance, v_tolerance : FLOAT]
SURFACE_APPROX_METHOD : ENUM_TYPE_INDEX (used with \fISurfaceApproxMethod\fP enumerated type)
SURFACE_EDGE_TYPE	: ENUM_TYPE_INDEX (used with \fISurfaceEdgeType\fP enumerated type)
SURFACE_INTERP		: ENUM_TYPE_INDEX (used with \fISurfaceInterpMethod\fP enumerated type)
SWITCH				: {\fIOff, On\fP}
TABLE_ENTRY			: [data : * ]
.FS
* See the section "Lookup Tables" for a description of
each of the data records that can be passed/returned as a table entry.
.FE
TABLE_INDEX			: CARD16
TABLE_TYPE			: {\fILineBundle, MarkerBundle, TextBundle, InteriorBundle,
						EdgeBundle, Pattern, TextFont, Color, View, Light,
						DepthCue, ColorApprox\fP}
TEXT_ALIGNMENT		: [vertical : TEXT_VALIGNMENT,
						horizontal : TEXT_HALIGNMENT]
TEXT_HALIGNMENT		: {\fIHalignNormal, HalignLeft, HalignRight, HalignCenter\fP}
TEXT_PATH			: {\fIPathRight, PathLeft, PathUp, PathDown\fP}
TEXT_PRECISION		: {\fIString, Char, Stroke\fP}
TEXT_VALIGNMENT		: {\fIValignNormal, ValignTop, ValignCap, ValignHalf,
						ValignBase, ValignBottom\fP}
TRIM_CURVE			: [visibility : SWITCH,
						order : CARD16,
						type : COORD_TYPE,
						approx_method : TRIM_CURVE_APPROX_METHOD,
						tolerance : FLOAT,
						tmin, tmax : FLOAT,
						knots : LISTofFLOAT,
						points : LISTofCOORD]
TRIM_CURVE_APPROX_METHOD : ENUM_TYPE_INDEX (used with \fITrimCurveApproxMethod\fP 
						enumerated type)
TYPE_OR_TABLE_INDEX	: {ENUM_TYPE_INDEX, TABLE_INDEX}
UPDATE_STATE		: {\fINotPending, Pending\fP}
VECTOR_2D			: [x, y : FLOAT]          
VECTOR_3D			: [x, y, z : FLOAT]          
VERTEX				: [point : COORD_3D,
						data : OPT_DATA]
VIEWPORT			: [min : DEVICE_COORD,
						max : DEVICE_COORD,
						use_drawable : BOOLEAN]
VIEW_REP				: [index : TABLE_INDEX,
						clip_flags : BITMASK,
						clip_limits : NPC_SUBVOLUME,
						orientation : MATRIX,
						mapping : MATRIX]
VISUAL_ID			: RESOURCE_ID
VISUAL_STATE			: {\fICorrect, Deferred, Simulated\fP}
WINDOW_ID			: RESOURCE_ID
WKS_BITMASK			: CARD32[2]
X11_FONT_ID			: RESOURCE_ID
				
.DE

.bp
.AC "Errors" 3
.LP
If an error occurs while processing a request that modifies a PEX
resource then it is possible that some portion of the resource 
(or some subset of the resource attributes) will have been altered.

The PEX Protocol uses the same set of error codes as the X11 Protocol
when applicable.
Additional error codes are provided for PEX-specific errors.
The following error codes can be returned by the various PEX requests:

.Bl "ColorType"
The specified color type is not supported.

.Bl "FloatingPointFormat"
The specified floating point format is not supported.

.Bl "Label      "
The specified label does not exist in the structure.

.Bl "LookupTable"
A value for a lookup table argument is illegal or does not name a defined
lookup table resource.

.Bl "NameSet"
A value for a name set argument is illegal or does not name a defined
name set resource.

.Bl "OutputCommand"
A value for some parameter of an output command is illegal, out of range,
or otherwise inappropriate.

.Bl "Path         "
A value for a structure network path contains inappropriate or
illegal values.

.Bl "PEXFont"
A value for a PEX font argument is illegal or does not name a defined
PEX font resource.

.Bl "PhigsWKS"
A value for a PHIGS workstation argument is illegal or does not name a defined
PHIGS workstation resource.

.Bl "PickMeasure"
A value for a pick measure argument is illegal or does not name a defined
pick measure resource.

.Bl "PipelineContext"
A value for a pipeline context argument is illegal or does not name a defined
pipeline context resource.

.Bl "Renderer"
A value for a renderer argument is illegal or does not name a defined
renderer resource.

.Bl "RendererState"
A renderer was in the
\fIRendering\fP state when a \fBPEXBeginRendering\fP request was received.

.Bl "SearchContext"
A value for a search context argument is illegal or does not name a defined
search context resource.

.Bl "Structure"
A value for a structure argument is illegal or does not name a defined
structure resource.

.AC "Events" 2
.LP
All PEX events will use the same mechanisms as X events.  
.LP
PEX defines a new event, \fBMaxHitsReached\fP.  \fBPEXGetImpDepConstants\fP
can be used to determine whether this event is supported.
This event is returned during a client side pick all traversal to  
indicate that the maximum number of hits has been reached.
The event includes the renderer identifier.
Upon receiving the event, the client should terminate the
pick traversal and decide whether an additional pass is necessary. 

.AC "Padding" 2
.LP
Certain values that must line up on 2- or 4-byte boundaries may necessitate
the insertion of pad bytes in some requests.  The value of pad bytes is
undefined.
.bp
.AC "Extension Information" 2
.LP
These requests return static information
about the PEX extension and what it supports.
Information about specific capabilities and tradeoffs should be
found in the documentation describing a particular PEX server implementation
(e.g., what is the "best" HLHSR method or floating point format or direct
color format to use, whether quick update really does anything, what
range of line and surface edge widths are supported, etc.)

.AC "Get Extension Information" 3
.Fs
.Na "PEXGetExtensionInfo"
.Rq                 
.Pa client_protocol_major_version CARD16
.Pa client_protocol_minor_version CARD16
.Re
.Pa protocol_major_version CARD16
.Pa protocol_minor_version CARD16
.Pa vendor STRING
.Pa release_number CARD32
.Pa subset_info CARD32
.Se
None
.Ds              
The \fIclient_protocol_major_version\fP and the 
\fIclient_protocol_minor_version\fP
indicate what version of the protocol the client
expects the server to implement.
The protocol version numbers
returned indicate the protocol the PEX extension actually supports.
This might not equal the version sent by the client.
A PEX extension can (but need
not) support more than one version simultaneously.
The \fIprotocol_major_version\fP and the \fIprotocol_minor_version\fP are
a mechanism to support future revisions of the PEX protocol which
may be necessary.
In general, the major version would increment for incompatible changes,
and the minor version would increment for small, upward-compatible changes.
Servers that support the protocol defined in this document
will return a \fIprotocol_major_version\fP of five, and a
\fIprotocol_minor_version\fP of one.
The \fIvendor\fP parameter is a string of ISO-LATIN1 characters that
describes the vendor that supplied the PEX extension.  The release number
is a 32-bit value whose semantics are controlled by the vendor.
The top 16 bits of \fIsubset_info\fP are reserved for use by vendors and
the bottom 16 bits contain information
about whether the PEX server is a full PEX implementation or whether it 
supports some combination of the standard subsets.  
.LP
If the 16 low-order bits of \fIsubset_info\fP are zero, the extension
is a complete PEX implementation.
If the lowest-order bit of \fIsubset_info\fP
is set, then the PEX extension supports "immediate rendering".
If the next-to-lowest-order bit of \fIsubset_info\fP
is set, then the PEX extension supports "PHIGS workstation".
If the third-lowest bit of \fIsubset_info\fP
is set, then the PEX extension supports "structure rendering".
If a server is sent a request that is not in the PEX subset supported by that
server, it will return a \fIRequest\fP error.
See Appendix A for the definition of "immediate rendering", "PHIGS
workstation", and "structure rendering" subsets.
.LP
The string "X3D-PEX" should be returned by the X request
\fBListExtensions\fP to indicate the presence of the PEX extension.
The same string should be used by clients in the X request
\fBQueryExtension\fP.
.Fe
.bp


.AC "Get Enumerated Type Information" 3
.Fs
.Na "PEXGetEnumeratedTypeInfo"
.Rq
.Pa drawable_id DRAWABLE_ID
.Pa enum_types LISTofENUM_TYPE
.Pa item_mask BITMASK
.Re
.Pa types LISTofLISTofVALUE
.Se
Drawable, Match, Value
.Ds
This request returns information about the enumerated types specified
by \fIenum_types\fP.
It returns information about the enumerated
types that are supported for drawables that have the same root window
and depth as the drawable indicated by
\fIdrawable_id\fP.  The \fIitem_mask\fP indicates the data that is to
be returned to describe each enumerated type.  The components
of an enumerated type descriptor (and the corresponding bits of
\fIitem_mask\fP) are:
.ID
    index		: ENUM_TYPE_INDEX
    mnemonic	: STRING
.DE
If only the \fIindex\fP bit is set in \fIitem_mask\fP, a list of index
values (type ENUM_TYPE_INDEX)
will be returned for the defined values for each enumerated type
specified in the \fIenum_types\fP list.
If only the \fImnemonic\fP bit is set in \fIitem_mask\fP,
only descriptor strings that use the ISO-Latin1 encoding
will be returned for the defined values (type STRING).
If both the \fIindex\fP and \fImnemonic\fP bits are set,
an index/mnemonic pair
will be returned for each of the defined values for each of the
requested enumerated types.
If neither bit is set, a list of counts
will be returned, where each count represents the number of supported
types for each entry in \fIenum_types\fP.
.LP
The various enumerated types and registered values are listed below.
Each registered value is followed by the mnemonic string that is
returned and a brief description.
Strings are returned using the ISO-Latin1 character set.
The strings are returned exactly as shown below.
Any enumerated type values less than zero are implementation-dependent (consult
the implementation documentation for their descriptions), and any numbers
greater than the listed values are reserved for future registration.


.Bl "MarkerType"
The marker type specifies the shape of the marker primitive that
is to be drawn when rendering marker primitives.  The registered values
are:
.Es
1	Dot	"." which is always displayed as the smallest displayable
		dot (the \fImarker_scale\fP attribute is ignored) with the dot
		at the marker position.
2	Cross	"+" (cross or plus sign) with intersection at the marker position.
3	Asterisk	"*" with intersection at the marker position.
4	Circle	"o" with center at marker position.
5	X	"x" with intersection at the marker position.
.Ee


.Bl "ATextStyle"
The annotation text style specifies the style that
is to be used when rendering annotation text primitives.  The registered values
are:
.Es
1	NotConnected	The annotation text primitive will be drawn with no
		line connecting it to the annotation text reference point.

2	Connected	The annotation text primitive will be connected to
		the annotation text reference point with a line, which will be
		drawn using the current set of line attributes.
.Ee


.Bl "LineType"
The line type specifies the style that
is to be used when rendering polyline and curve primitives.
The registered values are:
.Es
1	Solid	Draw the polyline or curve with a solid, unbroken line.
2	Dashed	Draw the polyline or curve with a line that is dashed.
3	Dotted	Draw the polyline or curve with a line that is dotted.
4	DashDot	Draw the polyline or curve with a line that contains
		alternating dots and dashes.
.Ee
It is implementation-dependent whether the sequence for the
\fIDashed\fP, \fIDotted\fP, and \fIDashDot\fP line types is restarted
or continued at the start of the polyline, at the start of a clipped
segment of a polyline, and at each vertex of a polyline.


.Bl "PolylineInterpMethod"
The polyline interpolation method specifies the style that
is to be used when rendering polyline primitives that have colors
specified per-vertex.
Depth-cueing is applied as a post-process
to polylines regardless of the polyline interpolation method.
The registered values are:
.Es
1	None	No interpolation will be performed between polyline vertices.
		If color values are supplied that differ for the endpoints
		of a polyline segment, it is implementation-dependent whether
		the color of the \fIi\fPth vertex will be used to draw the
		line between the \fIi\fPth and \fI(i+1)\fPth vertices (if \fIn\fP
		is the number of vertices, the color at the \fIn\fPth will be
		ignored), or whether they will be used to compute an average
		color which will be used for the entire segment.

2	Color	The polyline's vertex colors (if present) are used.  Color
		values along each polyline segment are then computed by
		linearly interpolating between the color values at the
		vertices.
.Ee


.Bl "CurveApproxMethod"
The curve approximation method specifies the method that
is to be used when rendering non-uniform rational B-spline (NURB)
curve primitives.
The registered values are:
.Es
1	(imp. dep.)	This value for \fICurveApproxMethod\fP is supported on
		every implementation, but may differ from one to the next.
		It may have the same mnemonic and definition as one of the
		other types, or it may be a method that is not in the list
		of registered types.  \fItolerance\fP is not used for 
		this method.

2	ConstantBetweenKnots	This technique tessellates the curve with equal parametric
		increments between successive pairs of knots.  The tolerance
		value controls tesselation of the curve.  If the tolerance
		value is not an integer value, it is truncated and only the
		integer portion will be used.  If \fItolerance\fP is less than
		or equal to zero, the curve will be evaluated only at the
		parameter limits, and at the knots that are within the specified
		parameter range.  If \fItolerance\fP is greater than zero, the
		curve will be evaluated at the  parameter limits, at the knots
		that are within the specified parameter range, and at the number
		of positions specified by \fItolerance\fP between each pair
		of knots.

3	WCS_ChordalSize	This technique tessellates the curve until the length of each line
		segment (chord) in world coordinates is less than the tolerance.

4	NPC_ChordalSize	This technique tessellates the curve until the length of each line
		segment (chord) in normalized project coordinates is less than
		the tolerance.

5	DC_ChordalSize	This technique tessellates the curve until the length of each line
		segment (chord) in device coordinates is less than the tolerance.

6	WCS_ChordalDev	This technique tessellates the curve until the maximum
		deviation (in world coordinates) between the line and
		the curve is less than the tolerance.

7	NPC_ChordalDev	This technique tessellates the curve until the maximum
		deviation (in normalized projection coordinates) between
		the line and the curve is less than the tolerance.

8	DC_ChordalDev	This technique tessellates the curve until the maximum
		deviation (in device coordinates) between the line and the
		curve is less than the tolerance.

9	WCS_Relative	This technique maintains a relative level of quality based on the
		tolerance value independent of scaling in world coordinates.
		The tolerance must be between 0 and 1 with values closer to 1 
		specifying  a better relative quality.

10	NPC_Relative	This technique maintains a relative level of quality based on the
		tolerance value independent of scaling in normalized projection
		coordinates.  The tolerance must be between 0 and 1 with 
		values closer to 1 specifying  a better relative quality.

11	DC_Relative	This technique maintains a relative level of quality based on the
		tolerance value independent of scaling in device coordinates.
		The tolerance must be between 0 and 1 with values
		closer to 1 specifying  a better relative quality.
.Ee


.Bl "InteriorStyle"
The interior style specifies the style that
is to be used when rendering surface primitives.
The registered values are:
.Es
1	Hollow	The interiors of surface primitives are not filled, but the
		boundary is drawn using the surface color.  If the surface
		primitive is clipped as a result of modeling, view, or workstation
		clipping, the boundary must be drawn along the clipped boundary
		as well.

2	Solid	The interiors of surface primitives are filled using the
		surface color.

3	Pattern	The interiors of surface primitives are filled using the
		pattern table entry specified by the interior style index.

4	Hatch	The interiors of surface primitives are filled using the
		surface color and the hatch style whose index is specified
		by the interior style index.

5	Empty	The interior of the surface primitive is not drawn at all.
.Ee


.Bl "HatchStyle"
The hatch style specifies the method that
is to be used to render surface primitives when the interior
style is set to \fIHatch\fP.  There are currently no registered hatch styles.


.Bl "SurfaceEdgeType"
The surface edge type specifies the style that
is to be used when rendering surface edges.
The registered values are:
.Es
1	Solid	Draw the surface edge with a solid, unbroken line.
2	Dashed	Draw the surface edge with a line that is dashed.
3	Dotted	Draw the surface edge with a line that is dotted.
4	DashDot	Draw the surface edge with a line that contains alternating
		dots and dashes.
.Ee
It is implementation-dependent whether the sequence for the
\fIDashed\fP, \fIDotted\fP, and \fIDashDot\fP edge types is restarted
or continued at the start of the edge, at the start of a clipped
segment of an edge, and at each vertex.



.Bl "ReflectionModel"
The reflection model specifies the method that
is used to perform the light source shading computation
when rendering surface primitives.  The input to the light source shading
computation is known as the \fIintrinsic color\fP and the output is known
as the \fIshaded color\fP.
If a normal exists at the
point at which the reflection model is to be evaluated, it will be
used.  Otherwise, if a normal exists for the facet containing the point,
it will be used to evaluate the reflection model.  If no normal exists,
the reflection model is evaluated, if possible, without a normal.
The registered values are:
.Es
1	NoShading	No light source shading computation is performed.  The
		surface color is not affected by light source illumination
		(effectively, shaded color \(== intrinsic color).

2	Ambient	Only the ambient terms of the lighting equation are used.
		The shaded color will be the intrinsic color as seen under
		ambient light.

3	Diffuse	Only the ambient and diffuse terms of the lighting equation
		are used.  The shaded color will be the intrinsic color as
		seen under ambient light, plus a diffuse reflection
		component from each light source.

4	Specular	The ambient, diffuse, and specular terms of the lighting
		equation are all used during the light source shading
		computation.  The shaded color will be the same as for
		\fIDiffuse\fP, plus a specular reflection component from
		each light source.
.Ee


.Bl "SurfaceInterpMethod"
The surface interpolation method specifies the method that
is used to compute color values in surface interiors
when rendering surface primitives.
Depth-cueing is applied as a post-process
to surface primitives regardless of the surface interpolation method.
The registered values are:
.Es
1	None	The color resulting from a single light source computation is
		used for the entire surface.  No interpolation will be
		performed across surface interiors or edges.

2	Color	The colors are computed at the vertices of the surface according
		to the current \fIreflection_model\fP.  These color values
		are then linearly interpolated across the interior of the
		surface or the edges.

3	DotProduct	The lighting equation dot products are computed at the
		vertices.  These dot products are linearly interpolated
		and the light source shading computation is applied using
		these values to compute the color value at each pixel in
		the interior of a surface or along a surface edge.

4	Normal	An attempt is made to interpolate the normal across the facet
		and perform the light source shading computation as accurately
		as possible at each pixel in the interior of a surface or
		along a surface edge.
.Ee


.Bl "SurfaceApproxMethod"
The surface approximation method specifies how to display non-uniform
rational B-spline surface 
primitives.
The registered values are:
.Es
1	(imp. dep.)	This value for \fISurfaceApproxMethod\fP is supported on
		every implementation, but may differ from one to the next.
		It may have the same mnemonic and definition as one of the
		other types, or it may be a method that is not in the list
		of registered types.  The tolerance values are not used for 
		this method.

2	ConstantBetweenKnots	This technique tessellates the surface with equal parametric
		increments between successive pairs of knots.  The two tolerance
		values control tesselation in each of the two parameter dimensions. 
		If the tolerance values are not integer values, they are truncated
		and only the integer portions of each will be used.  If \fIu_tolerance\fP
		is less than or equal to zero, the surface will be evaluated only at
		the \fIu\fP parameter limits in the \fIu\fP direction, and at the \fIu\fP knots
		that are within the specified parameter range.  If \fIu_tolerance\fP is
		greater than zero, the surface will be evaluated at the \fIu\fP parameter
		limits in the \fIu\fP direction, at the \fIu\fP knots that are within
		the specified parameter range, and at the number of positions
		specified by \fIu_tolerance\fP between each pair of knots.  The
		value of \fIv_tolerance\fP is used similarly to control the evaluation
		in the \fIv\fP direction.

3	WCS_ChordalSize	This technique tessellates the surface until the length of each line
		segment (chord) in world coordinates in the \fIu\fP parameter
		direction is less than the specified \fIu\fP tolerance value,
		and the length of every line segment in world coordinates in
		the \fIv\fP parameter direction is less than the specified
		\fIv\fP tolerance value.

4	NPC_ChordalSize	This technique tessellates the surface until the length of each line
		segment (chord) in normalized projection coordinates in the
		\fIu\fP parameter direction is less than the specified \fIu\fP
		tolerance value, and the length of every line segment in
		normalized projection coordinates in the \fIv\fP parameter
		direction is less than the specified \fIv\fP tolerance value.

5	DC_ChordalSize	This technique tessellates the surface until the length of each line
		segment (chord) in device coordinates in the \fIu\fP parameter
		direction is less than the specified \fIu\fP tolerance value,
		and the length of every line segment in device coordinates in
		the \fIv\fP parameter direction is less than the specified
		\fIv\fP tolerance value.

6	WCS_PlanarDev	This technique tessellates the surface into facets.  The
		technique subdivides the surface until the absolute value of the
		maximum deviation, in world coordinates, between any facet and
		the surface is less than \fIu_tolerance\fP. 

7	NPC_PlanarDev	This technique tessellates the surface into facets.  The
		technique subdivides the surface until the absolute value of the
		maximum deviation, in normalized projection coordinates, between
		any facet and the surface is less than \fIu_tolerance\fP. 

8	DC_PlanarDev	This technique tessellates the surface into facets.  The
		technique subdivides the surface until the absolute value of the
		maximum deviation, in device coordinates, between any facet and
		the surface is less than \fIu_tolerance\fP. 

9	WCS_Relative	This technique maintains a relative level of quality based on the
		specified \fIu_tolerance\fP value independent of scaling in
		world coordinates.  \fIu_tolerance\fP must be between 0 and 1 
		with values closer to 1 specifying  a better relative quality.
		\fIv_tolerance\fP is not used for this method.

10	NPC_Relative	This technique maintains a relative level of quality based on the
		specified \fIu_tolerance\fP value independent of scaling in
		normalized projection coordinates.  \fIu_tolerance\fP must be 
		between 0 and 1 with values closer to 1 specifying a better 
		relative quality.  \fIv_tolerance\fP is not used for this method.

11	DC_Relative	This technique maintains a relative level of quality based on the
		specified \fIu_tolerance\fP value independent of scaling in
		device coordinates.  \fIu_tolerance\fP must be between 0 and 1 
		with values closer to 1 specifying  a better relative quality.
		\fIv_tolerance\fP is not used for this method.


.Bl "TrimCurveApproxMethod"
The trim curve approximation method specifies the method that
is to be used for trim curves
when rendering non-uniform rational B-spline (NURB)
surface primitives with trim curves.
The registered values are:
.Es
1	(imp. dep.)	This value for \fITrimCurveApproxMethod\fP is supported on
		every implementation, but may differ from one to the next.
		It may have the same mnemonic and definition as one of the
		other types, or it may be a method that is not in the list
		of registered types.

2	ConstantBetweenKnots	This technique tessellates the trim curve with equal parametric
		increments between successive pairs of knots.  The tolerance
		value controls tesselation of the trim curve.  If the tolerance
		value is not an integer value, it is truncated and only the
		integer portion will be used.  If \fItolerance\fP is less than
		or equal to zero, the trim curve will be evaluated only at the
		parameter limits, and at the knots that are within the specified
		parameter range.  If \fItolerance\fP is greater than zero, the
		trim curve will be evaluated at the  parameter limits, at the
		knots that are within the specified parameter range, and at the
		number of positions specified by \fItolerance\fP between each
		pair of knots.
.Ee



.Bl "ModelClipOperator"
The model clip operator defines the operation that is to be used
to combine the specified halfspaces with the current composite
modeling clipping volume.
The registered values are:
.Es
1	Replace	The specified halfspaces are used to create a new composite
		modeling clipping volume that replaces the current composite
		modeling clipping volume.
2	Intersection	The specified halfspaces are intersected with the current
		composite modeling clipping volume to compute a new composite
		modeling clipping volume.
.Ee


.Bl "LightType"
The light type defines the characteristics of the light sources
that can be used in light source shading computations.
The registered values are:
.Es
1	Ambient	A light source that affects all surface primitives uniformly.
		Ambient light sources have only a color attribute.
2	WCS_Vector	A light source that is specified in world coordinates
		with a color and a direction vector.
3	WCS_Point	A light source that is specified in world coordinates
		with a color, a position, and two attenuation coefficients.
4	WCS_Spot	A light source that is specified in world coordinates
		with a color, a position, a direction vector, a concentration
		exponent, two attenuation coefficients and a spread angle.
.Ee


.Bl "ColorType"
The color type defines the format of color values.
The registered values are:
.Es
0	Indexed	A color that is passed as an unsigned 16-bit integer (i.e.,
		it is of type TABLE_INDEX).  The integer value is used as an
		index into a color lookup table.  Dereferencing of an indexed
		color value occurs at the time of rendering, at the time when
		the actual color value is needed for rendering an output primitive.

1	RGBFloat	A color that is passed as three floating point values,
		in the order red [0-1], green [0-1], blue [0-1].  A color in
		this format has a type defined by:
		COLOR_RGB_FLOAT	: [r, g, b : FLOAT]

2	CIEFloat	A color that is passed as three floating point values,
		in the order u [0-1], v  [0-1] (CIELUV diagram coefficients), and
		luminance [0-1].  A color in this format has a type defined by:
		COLOR_CIE_FLOAT	: [u, v, luminance : FLOAT]

3	HSVFloat	A color that is passed as three floating point values,
		in the order hue [0-1] (angle in fractions of a circle, with
		red being zero), saturation [0-1], and value [0-1].  A
		color in this format has a type defined by:
		COLOR_HSV_FLOAT	: [hue, saturation, value : FLOAT]

4	HLSFloat	A color that is passed as three floating point values,
		in the order hue [0-1] (angle in fractions of a circle, with
		red being zero), lightness [0-1], and saturation [0-1].
		A color in this format has a type defined by:
		COLOR_HLS_FLOAT	: [hue, lightness, saturation : FLOAT]

5	RGBInt8	A color that is passed as a unit of four bytes, in the
		order red, green, blue.  A color in this format has a type
		defined by:
		COLOR_RGB_INT8	: [r, g, b, pad : CARD8]

6	RGBInt16	A color that is passed as a unit of eight bytes,
		in the order red, green, blue.  A color in this format has
		a type defined by:
		COLOR_RGB_INT16	: [r, g, b, pad : CARD16]
.Ee


.Bl "FloatFormat"
The floating point format defines the format of floating point values.
The registered values are:
.Es
1	IEEE_754_32	An IEEE 754 standard 32-bit floating point value.
2	DEC_F_Floating	A DEC F-floating value.
3	IEEE_754_64	An IEEE 754 standard 64-bit floating point value.
4	DEC_D_Floating	A DEC D-floating value.
.Ee


.Bl "HLHSRMode"
The HLHSR mode defines the method used to do hidden line/hidden surface
removal.
The registered values are:
.Es
1	Off	All output primitives are drawn in the order they are
		processed.  No attempt will be made to remove hidden surfaces.

2	ZBuffer	Visibility is resolved at each pixel using a depth-,
		or z-buffering technique.  The z-buffering method and
		the number of bits of precision in the z values is
		device-dependent.  This technique permits visibility
		to be computed without an intermediate storage area for
		transformed data, can be used to incrementally add primitives
		to an image, and is an HLHSR method which is of linear order.

3	Painters	Output primitives are buffered as they are processed.
		When an "end rendering" occurs with flush=\fITrue\fP, the primitives
		in the buffer are sorted based on the average depth and
		rendered back-to-front.  This technique is fairly fast
		for small numbers of primitives, but requires an intermediate
		storage area.  This technique does not guarantee totally
		correct results, since it fails in cases involving cyclically
		overlapping or interpenetrating objects, and in other, even
		simpler, cases.

4	Scanline	Output primitives are buffered as they are received.
		When an "end rendering" occurs with flush=\fITrue\fP, the primitives
		in the buffer are sorted and visibility is computed in scan
		line order.  This technique can be fairly fast for small
		numbers of polygons, but uses an intermediate storage area
		to buffer output primitives and must perform a sorting step.

5	HiddenLineOnly	Only visible lines will be drawn.  Output primitives
		may be buffered as they are received.  When an "end rendering"
		occurs with flush=\fITrue\fP, the primitives in the buffer are 
		sorted and a hidden line computation is performed.

6 	ZBufferId	This is the same as \fIZBuffer\fP, except that the 
		\fIHLHSR_identifier\fP is used to enable and disable 
		z-buffering during traversal.  An \fIHLHSR_id\fP of zero 
		disables z-buffering and an \fIHLHSR_identifier\fP
		of one enables z-buffering.
.Ee


.Bl "PickDeviceType"
The pick device type specifies the type of pick device that
is to be used for workstation picking (via a pick device
and a pick measure) or renderer picking.
If a pick measure is created using one of the registered pick device
types then the \fIpick_data_rec\fP component of the pick device descriptor 
is ignored and the default prompt and echo type is \fIEchoPrimitive\fP.
The \fIpick_data_rec\fP component of the pick device descriptor is used
in conjunction with non-registered pick device types.
The registered values are:
.Es
1	DC_HitBox	The pick aperature is specified by a pick 
		position and a pick distance, both in device coordinates.  
		The shape of the hit box (square, circle, etc.) is 
		implementation-dependent.  The pick distance defines 
		the half-width or radius of the hit box.  

2	NPC_HitVolume	The pick aperature is specified by 
		two points that describe a parallelpiped in NPC space.
.Ee

.Bl "PickOneMethod"
The pick one method specifies the pick criteria that is to 
be used for renderer pick one traversals.
The registered values are:
.Es
1	Last	The last picked primitive is returned

2	ClosestZ	The primitive which has the z value closest to the front
 		clipping plane in the pick aperature is returned.  If several 
		primitives are equally close to the front clipping plane then 
		any of these primitives may be returned.

3	VisibleAny	Any picked primitive which is visible (after taking the current
		HLHSR mode into account) may be returned.

4	VisibleClosest	The primitive which is both visible (after taking the current
		HLHSR mode into account) and closest to the pick position is returned.
		The algorithm for determining which primitive is closest to the pick 
		position is implementation dependent.
.Ee


.Bl "PickAllMethod"
The pick all method specifies the pick criteria that is to 
be used for renderer pick all traversals.
The registered values are:
.Es
1	All	All primitives which are contained in or intersect the pick aperature
		are returned.

2	Visible	Only visible primitives which are contained in or intersect the pick
		aperature are returned.
.Ee


.Bl "PromptEchoType"
The prompt echo type defines the method used to do prompting and echoing
during picking operations.
The registered values are:
.Es
1	EchoPrimitive	Use an implementation-dependent technique that at
		least highlights the picked primitive for a short period
		of time.

2	EchoStructure	Echo the contiguous group of primitives with the same
		pick ID as the picked primitive, or all of the primitives
		of the structure with the same pick ID as the picked primitive
		(the extension is free to implement either semantic for this type).

3	EchoNetwork	Echo the entire posted structure network that contains
		the picked primitive.
.Ee


.Bl "DisplayUpdateMode"
The display update mode defines the manner in which changes will affect
the displayed image.
The registered values are:
.Es
1	VisualizeEach	Visualize each change as it occurs. (PHIGS - ASAP)

2	VisualizeEasy	Visualize only the changes that are "easy to do" (PHIGS -
		WAIT/UWOR).  Things that are "easy to do" are those that
		have a dynamic modification of \fIIMM\fP or can be updated
		without a regeneration of the displayed image.  The effective 
		result of such an action is equivalent to having performed a 
		regeneration, but without the expense of a complete retraversal 
		and without clearing the display space.

3	VisualizeNone	Visualize none of the changes (PHIGS - WAIT/NIVE).  The
		changes are applied, but the image is not regenerated until
		there is an explicit request to do so.

4	SimulateSome	Visualize the easy changes and simulate those changes
		that can be simulated.  (PHIGS - WAIT/UQUM)

5	VisualizeWhenever	All changes will eventually be visualized.  If regenerations
		are necessary, they will be performed at the server's
		convenience.  One regeneration may cause a number of changes
		to be visualized.  The client can issue an update workstation
		request to guarantee that all changes have been visualized.
		(PHIGS - ASTI/NIVE)
.Ee
It should be noted that implicit image regenerations may be performed when
the display update is one of
\fIVisualizeEach\fP or \fIVisualizeWhenever\fP.
If such a regeneration occurs, the display surface will be cleared
and any output that was not generated by traversing the posted structure
list (such as output from core X) will be lost.  \fIVisualizeEasy\fP,
\fIVisualizeNone\fP, and \fISimulateSome\fP
will not cause implicit regenerations to occur.

.Bl "ColorApproxType"
The color approximation type describes the way that a renderer will
transform rendering pipeline color values into displayable pixel values.
The registered values are:
.Es
1	ColorSpace	The rendering pipeline color is converted into
		a color with three individual color components.

2	ColorRange	The rendering pipeline color is converted into
		a single color index.
.Ee
This enumerated type allows applications to control whether
the color value produced through illumination and depth-cueing computations
is transformed into a single value (e.g., for display on an 8-bit
pseudo color display) or into three values (e.g., for display on a 24-bit
direct color display).

.Bl "ColorApproxModel"
The color approximation model describes the space in which any color
filtering or sampling will be performed during the color approximation
phase of rendering.  The registered values are:
.Es
1	RGB	red, green, blue
2	CIE	CIELUV diagram u, v coordinates plus luminance
3	HSV	hue, saturation, value
4	HLS	hue, lightness, saturation
5	YIQ	(NTSC) luminance (Y), inphase (wideband orange-cyan), and
		quadrature (narrowband magenta-green)
.Ee

.Bl "RenderingColorModel"
The rendering color model defines the color model to be used for color
interpolation within the rendering pipeline.  Reflectance equations should
have the appearance of being performed in the color space specified by
the rendering color model.
.Es
0	(imp. dep.)	An implementation-dependent color space
1	RGB	red, green, blue color model
2	CIE	CIELUV diagram u, v coordinates plus luminance color model
3	HSV	hue, saturation, value color model
4	HLS	hue, lightness, saturation color model
.Ee

.Bl "ParametricSurfaceCharacteristics"
.Es
1	None	No additional surface characteristics beyond the current
		surface attributes

2	(imp. dep.)	An implementation-dependent method that displays the shape
		of the surface.  This method does not distinguish between front
		and back facing portions of the surface.  The appearance of the
		representation is controlled by the appropriate set of primitive
		attributes for the representation.  It is implementation-dependent
		how the representation interacts with any interior rendering
		indicated by the interior attributes.  The data record is 
		ignored for this type.

3	IsoparametricCurves	Isoparametric curves are drawn on the surface.  The data
		record contains the number of curves to draw in each of the
		parameter dimensions and their placement.  If the placement
		is \fIUniform\fP, the specified number of curves are evenly
		spaced between the parameter limits of the surface; curves
		are also drawn at the parameter limits.  If the placement
		is \fINonUniform\fP, the specified number of curves are evenly
		spaced between each pair of knots; curves are also drawn at
		the knots.  In both cases only the portions of isoparametric
		curves are drawn that are within the interior of the surface
		as defined by any trimming curves.  This method does not
		distinguish between front and back facing portions of the
		surface.  The tessellation and appearance of the isoparametric
		curves are controlled by the surface approximation criteria and
		the polyline attributes, respectively.  The isoparametric
		curves are drawn in addition to any interior rendering
		indicated by the interior style or back interior style
		attributes.  Isoparametric curves have higher visual priority
		than the primitive's filled or hollow interiors, but lower
		priority than the primitive's edges.
.EQ
delim $$
.EN

4	MC_LevelCurves	Level curves are drawn on the surface.  The curves correspond
		to the intersections of the surface and a finite set of planes
		perpendicular to a modelling coordinate direction vector.  The
		positions of the planes are specified by a sequence of
		intersection points along an infinite line defined by a
		modelling coordinate origin point, $P sub 0$, and a direction
		vector, $V vec$.
 
		       $P sub i ~=~ P sub 0 ~+~ t sub i V vec$
 
		The $t sub i$ are a sequence of parameters specifying the
		intersection points.  They are in the range:
		
		       $- infinity ~<~ t sub i ~<~ infinity$

		The $P sub i$ are the intersection points of the perpendicular
		planes with the infinite line.  $P sub 0$ is a specified origin
		point in modelling coordinates, and $V vec$ is the specified
		direction vector in modelling coordinates.  The $i$-th plane
		is perpendicular to the direction vector, $V vec$, and inter-
		sects the infinite line at point $P sub i$.  The data record
		consists of the origin point, $P sub 0$; the direction vector,
		$V vec$; and the list of parameters, $t sub i$.
 
		This method does not distinguish between front and back facing
		portions of the surface.  The tessellation and appearance of the
		level curves are controlled by the surface approximation criteria
		and the polyline attributes, respectively.  The curves are drawn in
		addition to any interior rendering indicated by the interior style
		or back interior style attributes.  Level curves have higher visual
		priority than the primitive's filled or hollow interiors, but lower
		priority than the primitive's edges.
.EQ
delim $$
.EN

5	WC_LevelCurves	Level curves are drawn on the surface.  The curves correspond
		to the intersections of the surface and a finite set of planes
		perpendicular to a modelling coordinate direction vector.  The
		positions of the planes are specified by a sequence of
		intersection points along an infinite line defined by a
		modelling coordinate origin point, $P sub 0$, and a direction
		vector, $V vec$.
 
		       $P sub i ~=~ P sub 0 ~+~ t sub i V vec$
 
		The $t sub i$ are a sequence of parameters specifying the
		intersection points.  They are in the range:
		
		       $- infinity ~<~ t sub i ~<~ infinity$

		The $P sub i$ are the intersection points of the perpendicular
		planes with the infinite line.  $P sub 0$ is a specified origin
		point in world coordinates, and $V vec$ is the specified
		direction vector in world coordinates.  The $i$-th plane
		is perpendicular to the direction vector, $V vec$, and inter-
		sects the infinite line at point $P sub i$.  The data record
		consists of the origin point, $P sub 0$; the direction vector,
		$V vec$; and the list of parameters, $t sub i$.
 
		This method does not distinguish between front and back facing
		portions of the surface.  The tessellation and appearance of the
		level curves are controlled by the surface approximation criteria
		and the polyline attributes, respectively.  The curves are drawn in
		addition to any interior rendering indicated by the interior style
		or back interior style attributes.  Level curves have higher visual
		priority than the primitive's filled or hollow interiors, but lower
		priority than the primitive's edges.

.Ee

.Bl "GDP        "
The GDP type specifies the (2D) Generalized Drawing Primitives (GDPs) that are
supported by the PEX extension implementation.
There are currently no registered GDPs.

.Bl "GDP3        "
The GDP3 type specifies the (3D) Generalized Drawing Primitives (GDP3s) that
are supported by the PEX extension implementation.
There are currently no registered GDP3s.

.Bl "GSE        "
The GSE type specifies the Generalized Structure Elements (GSEs) that are
supported by the PEX extension implementation.
There are currently no registered GSEs.

.Bl "Escape	"
.br
The Escape type specifies the Escape Requests that are
supported by the PEX extension implementation.  The registered escapes
are described in appendix E, they are: 
.Es
1	SetEchoColor	modify the renderer's echo color 
.Ee

.Fe
.bp


.AC "Get Implementation-Dependent Constants" 3
.Fs
.Na "PEXGetImpDepConstants"
.Rq                 
.Pa fp_format FLOAT_FORMAT
.Pa drawable_example DRAWABLE_ID
.Pa names LISTofCONSTANT_NAME
.Re
.Pa constants LISTofVALUE
.Se
Drawable, Match, Value, FloatingPointFormat
.Ds              
This request allows a client to query one or more of the
implementation-dependent constants in a PEX server extension.
A single CARD32 or FLOAT is returned for each value requested.
These values are returned in order, with one return value in
\fIconstants\fP for each requested value in \fInames\fP.  Floating-point
values will be returned in the format specified by
\fIfp_format\fP.  The implementation-dependent constants that
are returned are based on the values that would be used for
a drawable with the same root and depth as \fIdrawable_example\fP.
.LP
PEX defines a number of standard constant names that all PEX
extensions must be able to return.  These standard constant
names are 16-bit integers with the high order bit equal to zero.
Additional proprietary implementation-dependent constants can
be defined and returned by PEX server extensions using 16-bit
integers with the high order bit equal to one.  The standard
constant names consist of:
.LD
.ta 1.6i 2.4i
\fINominalLineWidth\fP	CARD32	Width (in pixels) of "standard" line or curve.

\fINumSupportedLineWidths\fP	CARD32	Number of supported line or curve widths (a value of 0
		indicates that all line widths, including fractional widths,
		between min and max line width are supported).

\fIMinLineWidth\fP	CARD32	Width (in pixels) of thinnest line or curve that can be drawn.

\fIMaxLineWidth\fP	CARD32	Width (in pixels) of thickest line or curve that can be drawn.

\fINominalEdgeWidth\fP	CARD32	Width (in pixels) of "standard" edge.

\fINumSupportedEdgeWidths\fP	CARD32	Number of supported edge widths (a value of 0 indicates that
		all edge widths, including fractional widths, between min and
		max edge width are supported).

\fIMinEdgeWidth\fP	CARD32	Width (in pixels) of thinnest edge that can be drawn.

\fIMaxEdgeWidth\fP	CARD32	Width (in pixels) of thickest edge that can be drawn.

\fINominalMarkerSize\fP	CARD32	Largest dimension (either height or width, in pixels) of
		"standard" marker.

\fINumSupportedMarkerSizes\fP	CARD32	Number of supported marker sizes (a value of 0 indicates that
		all marker sizes, including fractional values, between min and
		max marker size are supported).

\fIMinMarkerSize\fP	CARD32	Largest dimension (either height or width, in pixels) of
		smallest marker that may be drawn.  (This minimum is exclusive
		of the marker type \fIDot\fP which is always drawn as the smallest
		displayable point).

\fIMaxMarkerSize\fP	CARD32	Largest dimension (either height or width, in pixels) of
		largest marker that may be drawn.  (This maximum is exclusive
		of the marker type \fIDot\fP which is always drawn as the smallest
		displayable point).

\fIChromaticityRedU\fP	FLOAT	Returns the CIEYUV \fIu\fP chromaticity coefficient for the red channel
		of the (properly adjusted) display device.

\fIChromaticityRedV\fP	FLOAT	Returns the CIEYUV \fIv\fP chromaticity coefficient for the red channel
		of the (properly adjusted) display device.

\fILuminanceRed\fP	FLOAT	Returns the CIEYUV luminance value for the red channel of the
		(properly adjusted) display device.

\fIChromaticityGreenU\fP	FLOAT	Returns the CIEYUV \fIu\fP chromaticity coefficient for the green channel
		of the (properly adjusted) display device.

\fIChromaticityGreenV\fP	FLOAT	Returns the CIEYUV \fIv\fP chromaticity coefficient for the green channel
		of the (properly adjusted) display device.

\fILuminanceGreen\fP	FLOAT	Returns the CIEYUV luminance value for the green channel of the
		(properly adjusted) display device.

\fIChromaticityBlueU\fP	FLOAT	Returns the CIEYUV \fIu\fP chromaticity coefficient for the blue channel
		of the (properly adjusted) display device.

\fIChromaticityBlueV\fP	FLOAT	Returns the CIEYUV \fIv\fP chromaticity coefficient for the blue channel
		of the (properly adjusted) display device.

\fILuminanceBlue\fP	FLOAT	Returns the CIEYUV luminance value for the blue channel of the
		(properly adjusted) display device.

\fIChromaticityWhiteU\fP	FLOAT	Returns the CIEYUV \fIu\fP chromaticity coefficient for the reference
		white of the (properly adjusted) display device.

\fIChromaticityWhiteV\fP	FLOAT	Returns the CIEYUV \fIv\fP chromaticity coefficient for the reference
		white of the (properly adjusted) display device.

\fILuminanceWhite\fP	FLOAT	Returns the CIEYUV luminance value for the reference white of the
		(properly adjusted) display device.

\fIMaxNamesetNames\fP	CARD32	Maximum number of names allowed in a name set.
				The names are between zero and (\fIMaxNamesetNames\fP - 1)

\fIMaxModelClipPlanes\fP	CARD32	Maximum number of modeling clipping planes that may be defined.

\fITransparencySupported\fP	CARD32	Returns \fITrue\fP or \fIFalse\fP, depending on whether the transmission
		coefficient is utilized in the light source shading computations.

\fIDitheringSupported\fP	CARD32	Returns \fITrue\fP or \fIFalse\fP, depending on whether the dithering hint
		actually causes dithering to occur.

\fIMaxNonAmbientLights\fP	CARD32	Maximum number of non-ambient light sources that may be enabled
		at one time.

\fIMaxNURBOrder\fP	CARD32	Maximum non-uniform rational B-spline order supported.

\fIMaxTrimCurveOrder\fP	CARD32	Maximum order for trim curves.

\fIBestColorApproxValues\fP	CARD32	Returns the constant \fIColorApproxPowersOf2\fP
		to indicate whether it is a significant performance win if the
		color approximation multiplier values are powers of two so that
		pixels can be composed using shifts and adds, or \fIColorApproxAnyValues\fP
		if it makes little or no difference.

\fIDoubleBufferingSupported\fP	CARD32	Returns \fITrue\fP or \fIFalse\fP depending on
		whether or not the server supports double-buffering.

\fIMaxHitsEventSupported\fP	CARD32	Returns \fITrue\fP or \fIFalse\fP, depending on whether the 
		\fIMaxHitsReached\fP event is supported.


.ta
.Fe
.bp


.AC "Match Renderer Targets" 3
.Fs
.Na "PEXMatchRendererTargets"
.Rq                 
.Pa drawable_id DRAWABLE_ID
.Pa depth CARD8
.Pa type MATCH_DRAW_TYPE
.Pa visual VISUAL_ID
.Pa max_targets CARD32
.Re
.Pa targets LISTofRENDERER_TARGETS
.Se
Drawable, Value
.Ds              
This returns information about the visual, depth and drawables the server
can support. \fIdrawable_id\fP tells the server which screen the client 
is interested in and, \fIdepth\fP, \fIvisual\fP and \fItype\fP 
specify which data is of interest.  Wildcarding can be 
accomplished by 
specifying zero for the \fIdepth\fP, zero for the \fIvisual\fP 
and \fIDontCare\fP for the \fItype\fP. 
This matches all depths, visuals and/or drawable types.  
\fImax_targets\fP is the maximum number of targets the client wants returned.
.Fe


.AC "Escape" 3
.Fs
.Na "PEXEscape"
.Rq                 
.Pa escape_id CARD32
.Pa escape_data LISTofCARD8
.Se
Value, \fIescape-specific\fP
.Ds              
This request can be used to send registered PEX escapes or vendor-specific
escapes that do not generate a reply.  Registered escapes are 
defined in Appendix E.
For registered escapes, \fIescape_id\fP is a positive number.
For vendor-specific escapes the \fIescape_id\fP is encoded as follows:
the high bit is set, the next 15 bits
contain the \fIvendor_id\fP and the low-order 16 bits contain
the vendor-specific \fIrequest_id\fP.
\fIvendor_id\fP is a unique number, assigned on request by the MIT registry,
that identifies the vendor.  
The supported escapes can be determined using \fBPEXGetEnumeratedTypeInfo\fP.
.LP
For vendor-specific escapes, it is up to 
the vendor to specify how \fIescape_data\fP is encoded. For example, one vendor
may choose to always pass a floating point format as the first value in
\fIescape_data\fP while another may choose to pass a sub-request number.  
As with all X requests, this request must be a multiple of 4 bytes.
.LP
If the \fIescape_id\fP is not recognized by the server, or if 
the \fIescape_id\fP refers to an escape that generates a reply,
a Value error will be generated.  All other errors returned by this
request are escape-specific.  
This request will not generate any new events or errors.
The X extension mechanism must be used for 
vendor-specific requests that generate new events or new errors.
.Fe

.AC "Escape With Reply" 3
.Fs
.Na "PEXEscapeWithReply"
.Rq                 
.Pa escape_id CARD32
.Pa escape_data LISTofCARD8
.Re
.Pa escape_id CARD32
.Pa escape_reply_data LISTofCARD8
.Se
Value, \fIescape-specific\fP
.Ds              
This request can be used to send registered PEX escapes or vendor-specific
escapes that generate a reply.  
The supported escapes can be determined using \fBPEXGetEnumeratedTypeInfo\fP.
At this point in time there
are no registered escapes that have replies.
.LP
For vendor-specific escapes, it is up to 
the vendor to specify how \fIescape_data\fP and \fIescape_reply_data\fP
are encoded.  The vendor should 
define encodings which follow the conventions 
in the encoding document.
.LP
The encoding of \fIescape_id\fP is described in \fBPEXEscape\fP.
If the \fIescape_id\fP is not recognized by the server, or if 
the \fIescape_id\fP refers to an escape that does not generate a reply,
a Value error will be generated.  All other errors returned by this
request are escape-specific.  
This request will not generate any new events or errors.
The X extension mechanism must be used for 
vendor-specific requests that generate new events or new errors.
.Fe
.bp
.AC "Output Commands" 1
.LP
.RU
.LP
This section defines output commands.  Output commands are commands
that are capable of being processed by a renderer or
stored as structure elements.  The format of each of the
commands is listed below.
Output commands may be passed
to the server to be processed immediately by a renderer with the
\fBPEXRenderOutputCommands\fP request.
Output commands may be passed
to the server to be stored in a structure with the
\fBPEXStoreElements\fP request.
Output commands may be retrieved by a client from a structure resource
with the
\fBPEXFetchElements\fP request.
.LP
Output commands are always executed in exactly the same fashion, no
matter whether
they are processed immediately by a renderer or processed as part of
a structure traversal.  When sent to the server via a
\fBPEXRenderOutputCommands\fP request, output commands are processed
until one is found to be in error, or until the entire list has been
processed.  If an output command is discovered to contain an error,
it is discarded, as are all others following it in the list of output
commands and an \fIOutputCommand\fP error is returned to the client.
Similarly, if a \fBPEXStoreElements\fP command is used to transmit
a list of output commands to the server, the first erroneous output
command and all output commands following it in the list will be discarded,
and an \fIOutputCommand\fP error will be reported to the client.  Thus
it is not possible for a structure resource to contain any elements with
illegal or inappropriate values.
.LP
\fIOutputCommand\fP errors are only generated by the 
\fBPEXRenderOutputCommands\fP or \fBPEXStoreElements\fP requests.  The
error checking performed for these two types of requests is identical.
When output commands are processed by a renderer, attribute specifications
that are not supported by the server, out-of-range table indices, 
or undefined table indices are mapped to their default values; no
errors will be reported.
.AC "Data Formats" 2
.LP
Each of the requests listed above takes a format parameter 
of type FLOAT_FORMAT.
For those requests sending data from the client to the server,
this format word is used to indicate to the server the format of any
floating point values that are contained in the request.
For those requests requiring data to be sent back to the client,
the format is used to indicate to the server how it should format
the floating point data in the reply sent back to the client.
.LP
Color values are typically passed as a color type and a value.
The color type specifies whether
the color is an index value or a direct color value of some type.
In the case of the "with data" output primitives (which may contain
many color values), the color type is specified just once and all
of the color values in the output command must be of the indicated
color type.
PEX servers are required to be able to
deal with indexed colors
and at least one type of direct color.
Indexed colors are specified using an index which is used to obtain
the color from a color lookup table.
Direct colors are specified
directly as RGB, HSV, HLS, or CIELUV color values of some form.
The list of registered direct color formats can be found in the
"Extension Information" section.
PEX servers are free to store direct color values
in whatever implementation-dependent format they choose, but
they must be capable of converting those values back into the
originally-specified color type when queried by the client.

.AC "Errors" 2
.LP
Errors that can be reported when passing a list of output commands to a
PEX server are described in the following sections.  Specific errors
that the PEX server checks for when storing or processing a particular
type of output command are explained in the "Output Command
Descriptions" section.

.AC "FloatingPointFormat Errors" 3
.LP
The floating point format is specified once in each \fBPEXStoreElements\fP
or \fBPEXRenderOutputCommands\fP request.  If the request specifies
a floating point format that is not supported by the PEX server, a
\fIFloatingPointFormat\fP error is reported and none of the output commands
are processed, even if they do not contain floating point values.  All
output commands in the list are ignored.

.AC "ColorType Errors" 3
.LP
The color type is generally specified with each color or, in some cases,
once per output command.  If an output command specifies a color type
that is not supported by the PEX server, an \fIOutputCommand\fP error
is reported.
All previous output commands in the list are processed, and the output
command containing the unsupported color type and any subsequent
output commands are ignored.

.AC "Length Errors" 3
.LP
If an output command exceeds the length of the output command list,
processing of the list stops and an \fIOutputCommand\fP error is
reported.  All previous output commands in the list are processed,
and the output command that exceeds the request length and any
subsequent output commands are ignored.

.AC "OutputCommand Errors" 3
.LP
If an illegal value is specified in an output command, all processing
of the list stops and an \fIOutputCommand\fP error is reported.  All
previous output commands in the list are processed, and the output
command that contains the illegal value and any subsequent output commands
are ignored.
.LP
Not all unsupported values are illegal.  Some enumerated types allow for
implementation dependent values (for example, negative line types).  In
general, output commands that contain these types can have arbitrary
values specified.  When the output command is rendered, values that are
not supported by the PEX server are rendered with a default value and
do not report errors.
.LP
A second category of enumerated types have a fixed set of legal values
which are all required to be implemented by a conforming PEX server
and cannot be inquired (for example, text path).  If the PEX server
finds a value outside the range of legal values, an \fIOutputCommand\fP
error is reported, as described above.
.LP
A third category of enumerated types have a fixed set of legal values,
but they are not all required to be implemented by a PEX server.  Values
supported by the PEX server can be inquired (for example, interior style).
When the output command is rendered, values that are not supported by
the PEX server are rendered with a default value and do not report
errors.
.LP
If an output command contains a bitmask value, the PEX server must return
an \fIOutputCommand\fP error if any undefined bits are set.  Usually,
these errors are not specifically mentioned in the output command
descriptions below.
.LP
The client is not required to pass unit length normal vectors to the
PEX server in output commands.  The effect of rendering output primitives
with normal vectors that are not unit length is implementation dependent.

.AC "Output Command Descriptions" 2
.LP
The list below describes the format of the output commands that are supported.
Each output command is a structure of type OUTPUT_COMMAND, which
contains a 16-bit opcode that uniquely defines the output command (as well
as uniquely identifying the structure element if the command is stored
in a structure), a 16-bit size field which specifies the length of the
output command in units of four bytes, and
the data needed to specify the output command.  The high-order bit of
the opcode field is reserved to indicate whether the output command
is one of the standard PEX output commands (high-order bit equals zero)
or a non-standard or proprietary output command (high-order bit equals one).

.Or "Marker type"
.Op marker_type MARKER_TYPE
.IP
When processed by a renderer,
this command will
modify the renderer's \fImarker_type\fP attribute.
If the specified marker type is not supported by the PEX server, marker
type 3 (\fIMarkerAsterisk\fP) is used.
.IP
Any integer value may be specified as the marker type in this output command.

.Or "Marker scale"
.Op scale FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fImarker_scale\fP attribute.
The specified scale is multiplied by the nominal marker size (see
\fBPEXGetImpDepConstants\fP) and the result is mapped to the nearest
marker size supported by the server.

.Or "Marker color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fImarker_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Marker color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fImarker_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.


.Or "Marker bundle index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fImarker_bundle_index\fP attribute.
If an undefined marker bundle index is specified by this output command,
then default bundle index one is used.  If table index one is not defined,
the default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the bundle index in this 
output command is zero.

.Or "Text font index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_font_index\fP attribute.
The \fItext_font_index\fP selects which entry in the
text font table (i.e., which font group) will be used
to render text primitives.
If an undefined text font index is specified by this output command,
the default index one is used.  If table index one is not defined, the
default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the text font
index in this output command is zero.

.Or "Text precision"
.Op precision TEXT_PRECISION
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_precision\fP attribute.
.IP
When a text or annotation text output primitive is interpreted, all of
the ISTRING fragments in the text string are rendered in the same text
precision.
That is, if the font group selected by the current text font index
consists of both X and PEX fonts, and if some of the ISTRING fragments
in the string are rendered in X fonts, the text precision of the
entire string must be dropped to at least \fIChar\fP precision.
.IP
If a \fIchar_set\fP value is not available in the current font group,
then the entire string is rendered using the default font group.
If a \fIchar_set\fP value is not available in the default font group,
then that portion of the string is rendered in an implemenation dependent
manner.

.Or "Character expansion"
.Op expansion FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIchar_expansion\fP attribute.
Only the magnitude of the specified expansion is considered.
The specified expansion is compared to the minimum and maximum
character expansion factors.
These values depend on the font files that are in the font groups in
the selected font table entry, which in turn depend on which X or PEX
font files the client opened.  For example, if the client opens all PEX
font files (that is, all scalable and rotatable stroke fonts), then a
continuous number of expansions are supported.
If the
expansion is smaller than the minimum character expansion factor, the
minimum value is used.  If the expansion is larger than the maximum
character expansion factor, the maximum value is used.

.Or "Character spacing"
.Op spacing FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIchar_spacing\fP attribute.
.IP
No errors or defaults are defined.

.Or "Text color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index one is used.
If color index one is not defined, the default values listed in Appendix
D are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Text color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.

.Or "Character height"
.Op height FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIchar_height\fP attribute.
If the specified
height or the computed width is not supported, the height or width is
mapped to the nearest character height or width supported.
These values depend on the font files that are in the font groups in
the selected font table entry, which in turn depend on which X or PEX
font files the client opened.  For example, if the client opens all PEX
font files (that is, all scalable and rotatable stroke fonts), then a
continuous number of character sizes are supported.

.Or "Character up vector"
.Op up VECTOR_2D
.IP
When processed by a renderer,
this command will
modify the renderer's \fIchar_up_vector\fP attribute.
If the character up
vector is degenerate (it has a length of zero), the value <0, 1> is used.

.Or "Text path"
.Op path TEXT_PATH
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_path\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the path is not
\fIPathRight\fP, \fIPathLeft\fP, \fIPathUp\fP, or \fIPathDown\fP.

.Or "Text alignment"
.Op alignment TEXT_ALIGNMENT
.IP
When processed by a renderer,
this command will
modify the renderer's
\fItext_alignment\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the horizontal
alignment is not \fIHalignNormal\fP, \fIHalignLeft\fP, \fIHalignCenter\fP,
or \fIHalignRight\fP, or if the vertical alignment is not \fIValignNormal\fP,
\fIValignTop\fP, \fIValignCap\fP, \fIValignHalf\fP, \fIValignBase\fP, or
\fIValignBottom\fP.

.Or "Annotation text height"
.Op height FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIatext_height\fP attribute.
If the specified
height or the computed width is not supported, the height or width is
mapped to the nearest annotation character height or width supported.
These values depend on the font files that are in the font groups in
the selected font table entry, which in turn depend on which X or PEX
font files the client opened.  For example, if the client opens all PEX
font files (that is, all scalable and rotatable stroke fonts), then a
continuous number of character sizes are supported.

.Or "Annotation text up vector"
.Op up VECTOR_2D
.IP
When processed by a renderer,
this command will
modify the renderer's \fIatext_up_vector\fP attribute.
If the annotation
text up vector is degenerate (it has a length of zero), the value <0, 1>
is used.

.Or "Annotation text path"
.Op path TEXT_PATH
.IP
When processed by a renderer,
this command will
modify the renderer's \fIatext_path\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the path is not
\fIPathRight\fP, \fIPathLeft\fP, \fIPathUp\fP, or \fIPathDown\fP.

.Or "Annotation text alignment"
.Op alignment TEXT_ALIGNMENT
.IP
When processed by a renderer,
this command will
modify the renderer's
\fIatext_alignment\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the horizontal
alignment is not \fIHalignNormal\fP, \fIHalignLeft\fP, \fIHalignCenter\fP,
or \fIHalignRight\fP, or if the vertical alignment is not \fIValignNormal\fP,
\fIValignTop\fP, \fIValignCap\fP, \fIValignHalf\fP, \fIValignBase\fP, or
\fIValignBottom\fP.

.Or "Annotation text style"
.Op index ATEXT_STYLE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIatext_style\fP attribute.
If the specified
style is not supported by the PEX server, annotation style 1
(\fIATextNotConnected\fP) is used.
.IP
Any integer value may be specified as the style in this output command.

.Or "Text bundle index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fItext_bundle_index\fP attribute.
If an undefined
text bundle index is specified by this output command, then default
bundle index one is used.  If table index one is not defined, the default
values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the bundle index
in this output command is zero.

.Or "Line type"
.Op line_type LINE_TYPE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIline_type\fP attribute.
If the specified line type is not supported by the PEX server, line type 1
(\fILineTypeSolid\fP) is used.
.IP
Any integer value may be specified as the line type in this output command.

.Or "Line width"
.Op width FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIline_width\fP attribute.
The specified line width is multiplied by the nominal line width (see
\fBPEXGetImpDepConstants\fP).  The result is mapped to the nearest line
width supported by the server.

.Or "Line color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIline_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index
one is used.  If color index one is not defined, the default values listed
in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Line color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fIline_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.

.Or "Curve approximation"
.Op approx CURVE_APPROX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIcurve_approx\fP
attribute.
If the specified method is not supported by the PEX server, method 1
(implementation dependent) is used.
.IP
Any integer value may be specified as the curve approximation method in this
output command.

.Or "Polyline interpolation method"
.Op polyline_interp POLYLINE_INTERP
.IP
When processed by a renderer,
this command will
modify the renderer's \fIpolyline_interp\fP attribute.
If the specified interpolation method is not supported by the PEX server,
method 1 (\fIPolylineInterpNone\fP) is used.
.IP
Any integer value may be specified as the interpolation method in this
output command.

.Or "Line bundle index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIline_bundle_index\fP attribute.
If an undefined
line bundle index is specified by this output command, then default bundle
index one is used.  If table index one is not defined, the default values
listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the bundle index
in this output command is zero.

.Or "Surface interior style"
.Op interior_style INTERIOR_STYLE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIinterior_style\fP attribute.
If the specified style is not supported, style 1 (\fIInteriorStyleHollow\fP)
is used.
.IP
An \fIOutputCommand\fP error is reported if the style is not
\fIInteriorStyleHollow\fP, \fIInteriorStyleSolid\fP, \fIInteriorStylePattern\fP,
\fIInteriorStyleHatch\fP, or \fIInteriorStyleEmpty\fP.

.Or "Surface interior style index"
.Op index TYPE_OR_TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIinterior_style_index\fP attribute.
If the current \fIinterior_style\fP is \fIInteriorStylePattern\fP or
\fIInteriorStyleHatch\fP, the specified index is used to further define
the rendering style of surface primitives.
For \fIInteriorStylePattern\fP, if the specified pattern table index is not
defined, table index one is used.\(dg
.FS
\(dg PHIGS requires that the interior style index must be greater than zero
for \fIInteriorStylePattern\fP, but this is difficult for PEX to enforce, so
a default action is defined instead.
.FE
For \fIInteriorStyleHatch\fP, the index determines the hatch style and may be
positive or negative.  If the specified hatch style is not supported, style
one is used.  If hatch style one is not supported, the result is
implementation dependent.

.Or "Surface color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index one is used.
If color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Surface color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.

.Or "Surface reflection attributes"
.Op attr REFLECTION_ATTR
.IP
When processed by a renderer,
this command will
modify the renderer's
\fIreflection_attr\fP attribute.
.IP
No errors or defaults are defined.

.Or "Surface reflection model"
.Op reflection_model REFLECTION_MODEL
.IP
When processed by a renderer,
this command will
modify the renderer's \fIreflection_model\fP attribute.
If the specified reflection model is not supported by the PEX server,
method 1 (\fIReflectionNoShading\fP) is used.
.IP
Any integer value may be specified as the reflection model in this output
command.

.Or "Surface interpolation method"
.Op surface_interp SURFACE_INTERP
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_interp\fP attribute.
If the specified interpolation method is not supported by the PEX server,
method 1 (\fISurfaceInterpNone\fP) is used.
.IP
Any integer value may be specified as the interpolation method in this output
command.

.Or "Backface surface interior style"
.Op interior_style INTERIOR_STYLE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_interior_style\fP attribute.
If the specified style is not supported, style 1 (\fIInteriorStyleHollow\fP)
is used.
.IP
An \fIOutputCommand\fP error is reported if the style is not
\fIInteriorStyleHollow\fP, \fIInteriorStyleSolid\fP, \fIInteriorStylePattern\fP,
\fIInteriorStyleHatch\fP, or \fIInteriorStyleEmpty\fP.

.Or "Backface surface interior style index"
.Op index TYPE_OR_TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_interior_style_index\fP attribute.
If the current \fIbf_interior_style\fP is
\fIInteriorStylePattern\fP or \fIInteriorStyleHatch\fP, the specified
index is used to further define the rendering style of backfacing
surface primitives.
For \fIInteriorStylePattern\fP, if the specified pattern table index is not
defined, table index one is used.\(dd
.FS
\(dd PHIGS requires that the backface
interior style index must be greater than zero
for \fIInteriorStylePattern\fP, but this is difficult for PEX to enforce, so
a default action is defined instead.
.FE
For \fIInteriorStyleHatch\fP, the index determines the hatch style and may
be positive or negative.  If the specified hatch style is not supported,
style one is used.  If hatch style one is not supported, the result is
implementation dependent.

.Or "Backface surface color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_surface_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Backface surface color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_surface_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.

.Or "Backface surface reflection attributes"
.Op attr REFLECTION_ATTR
.IP
When processed by a renderer,
this command will
modify the renderer's
\fIbf_reflection_attr\fP attribute.
.IP
No errors or default are defined.

.Or "Backface surface reflection model"
.Op reflection_model REFLECTION_MODEL
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_reflection_model\fP attribute.
If the specified reflection model is not supported
by the PEX server, method 1 (\fIReflectionNoShading\fP) is used.
.IP
Any integer value may be specified as the reflection model in this output
command.

.Or "Backface surface interpolation method"
.Op surface_interp SURFACE_INTERP
.IP
When processed by a renderer,
this command will
modify the renderer's \fIbf_surface_interp\fP attribute.
If the specified interpolation method is not supported
by the PEX server, method 1 (\fISurfaceInterpNone\fP) is used.
.IP
Any integer value may be specified as the interpolation method in this output
command.

.Or "Surface approximation"
.Op approx SURFACE_APPROX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_approx\fP
attribute.
If the specified method is not supported by the PEX server, method 1
(implementation dependent) is used.
.IP
Any integer value may be specified as the surface approximation method in
this output command.

.Or "Facet culling mode"
.Op culling_mode CULL_MODE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIculling_mode\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the mode is not
\fINone\fP, \fIBackFaces\fP, or \fIFrontFaces\fP.

.Or "Facet distinguish flag"
.Op distinguish BOOLEAN
.IP
When processed by a renderer,
this command will
modify the renderer's \fIdistinguish\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the mode is not \fITrue\fP or \fIFalse\fP.

.Or "Pattern size"
.Op size VECTOR_2D
.IP
When processed by a renderer,
this command will
modify the renderer's \fIpattern_size\fP attribute.
Only the magnitudes of the specified pattern size components are considered.
If the current \fIinterior_style\fP is
\fIInteriorStylePattern\fP, the specified pattern size components are used.
If either component is zero, the output command is ignored.

.Or "Pattern reference point"
.Op point COORD_2D
.IP
When processed by a renderer,
this command will
modify the renderer's \fIpattern_ref_pt\fP,
\fIpattern_ref_vec1\fP, and \fIpattern_ref_vec2\fP
attributes.
The \fIz\fP coordinate of the reference point will be set
to zero, \fIpattern_ref_vec1\fP will be set to <1,0,0>,
and \fIpattern_ref_vec2\fP will be set to <0,1,0>.
.IP
No errors or defaults are defined.

.Or "Pattern reference point and vectors"
.Op ref_pt COORD_3D
.Op vector1 VECTOR_3D
.Op vector2 VECTOR_3D
.IP
When processed by a renderer,
this command will
modify the renderer's \fIpattern_ref_pt\fP, \fIpattern_ref_vec1\fP,
and \fIpattern_ref_vec2\fP attributes.
If the pattern vectors
define a degenerate case (that is, if one of the vectors is zero length or
if the vectors are parallel), the output command is ignored.

.Or "Interior bundle index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIinterior_bundle_index\fP attribute.
If an undefined interior bundle index is specified by this output command, then
default bundle index one is used.  If table index one is not defined, the
default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the bundle index
in this output command is zero.

.Or "Surface edge flag"
.Op onoff SWITCH
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_edges\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the flag is not
\fIOn\fP or \fIOff\fP.

.Or "Surface edge type"
.Op edge_type SURFACE_EDGE_TYPE
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_edge_type\fP attribute.
If the specified edge type is not supported by the PEX server, edge type
1 (\fISurfaceEdgeSolid\fP) is used.
.IP
Any integer value may be specified as the edge type in this output command.

.Or "Surface edge width"
.Op width FLOAT
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_edge_width\fP attribute.
The specified edge width is multiplied by the nominal edge width (see
\fBPEXGetImpDepConstants\fP).  The result is mapped to the nearest edge
width supported by the PEX server.

.Or "Surface edge color index"
.Op color TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_edge_color\fP attribute,
setting the
color type to \fIIndexed\fP and the color value to the index specified by
\fIcolor\fP.
If the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color index in this
output command is 65535.

.Or "Surface edge color"
.Op color COLOR_SPECIFIER
.IP
When processed by a renderer,
this command will
modify the renderer's \fIsurface_edge_color\fP attribute,
setting the
color type and value as specified.
If the color type is \fIIndexed\fP and
the specified color index is not defined, color index one is used.  If
color index one is not defined, the default values listed in Appendix D
are used.
.IP
An \fIOutputCommand\fP error is reported if the color type is 
\fIIndexed\fP and the color index is 65535.

.Or "Edge bundle index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIedge_bundle_index\fP attribute.
If an undefined
edge bundle index is specified by this output command, then default bundle
index one is used.  If table index one is not defined, the default values
listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the bundle index
in this output command is zero.

.Or "Set individual ASF"
.Op attribute ASF_ATTRIBUTE
.Op source ASF_VALUE
.IP
When processed by a renderer,
this command will
modify the specified ASF (aspect source flag) attribute in the
renderer.  Depending on the value of \fIattribute\fP,
one of the following rendering pipeline attributes will be modified:
.IP
.ta 1.8i 3.5i
\fImarker_type_asf		interior_style_asf\fP
\fImarker_scale_asf		interior_style_index_asf\fP
\fImarker_color_asf		surface_color_asf\fP
\fItext_font_index_asf		surface_interp_asf\fP
\fItext_prec_asf		reflection_model_asf\fP
\fIchar_expansion_asf		reflection_attr_asf\fP
\fIchar_spacing_asf		bf_interior_style_asf\fP
\fItext_color_asf		bf_interior_style_index_asf\fP
\fIline_type_asf		bf_surface_color_asf\fP
\fIline_width_asf		bf_surface_interp_asf\fP
\fIline_color_asf		bf_reflection_model_asf\fP
\fIcurve_approx_asf		bf_reflection_attr_asf\fP
\fIpolyline_interp_asf		surface_approx_asf\fP
				\fIsurface_edges_asf\fP
				\fIsurface_edge_type_asf\fP
				\fIsurface_edge_width_asf\fP
				\fIsurface_edge_color_asf\fP
.ta
.IP
An \fIOutputCommand\fP error is reported if \fIattribute\fP does
not refer to a valid ASF attribute or if \fIsource\fP
is not set to \fIBundled\fP or \fIIndividual\fP.

.Or "Local transform 3D"
.Op comp_type COMPOSITION
.Op matrix MATRIX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIlocal_transform\fP attribute.
If \fIcomp_type\fP is \fIPreConcatenate\fP, the specified matrix is 
pre-concatenated
to the local model transformation matrix.  If \fIcomp_type\fP is 
\fIPostConcatentate,\fP the specified matrix is post-concatenated to the 
local modeling transform.  If \fIcomp_type\fP is \fIReplace\fP, the
specified matrix replaces the local modeling transform.
.IP
An \fIOuputCommand\fP error is reported if the composition
type is not \fIPreConcatenate\fP, \fIPostConcatenate\fP or \fIReplace\fP.

.Or "Local transform 2D"
.Op comp_type COMPOSITION
.Op matrix MATRIX_3X3
.IP
When processed by a renderer,
this command will
modify the renderer's \fIlocal_transform\fP attribute.  This command is
identical to \fIlocal transform 3D\fP except that the specified matrix
is a 3 \(mu 3 matrix instead of a 4 \(mu 4 matrix.  Before the concatenation
occurs, the 3 \(mu 3 matrix represented by
.IP
.EQ
left [
    {
    ~~~ pile { a above d above g }
    ~~~ pile { b above e above h }
    ~~~ pile { c above f above j }
    }
    ~~~ right ]
.EN
.sp
will be expanded to a 4 \(mu 4 matrix as follows:
.IP
.EQ
left [
    {
    ~~~ pile { a above d above g }
    ~~~ pile { b above e above h }
    ~~~ pile { c above f above j }
    }
    ~~~ right ] ->
left [
    {
    ~~~ pile { a above d above 0 above g }
    ~~~ pile { b above e above 0 above h }
    ~~~ pile { 0 above 0 above 1 above 0 }
    ~~~ pile { c above f above 0 above j }
    }
    ~~~ right ]
.EN
.IP
An \fIOutputCommand\fP error is reported if the composition
type is not \fIPreConcatenate\fP, \fIPostConcatenate\fP or \fIReplace\fP.

.Or "Global transform 3D"
.Op matrix MATRIX
.IP
When processed by a renderer,
this command will
replace the renderer's \fIglobal_transform\fP attribute.
.IP
No errors or defaults are defined.

.Or "Global transform 2D"
.Op matrix MATRIX_3X3
.IP
When processed by a renderer,
this command will
replace the renderer's \fIglobal_transform\fP attribute.  This command is
identical to "Global transform 3D" except that the specified matrix
is a 3 \(mu 3 matrix instead of a 4 \(mu 4 matrix.  Before the replacement
occurs, the 3 \(mu 3 matrix represented by
.IP
.EQ
left [
    {
    ~~~ pile { a above d above g }
    ~~~ pile { b above e above h }
    ~~~ pile { c above f above j }
    }
    ~~~ right ]
.EN
.sp
will be expanded to a 4 \(mu 4 matrix as follows:
.IP
.EQ
left [
    {
    ~~~ pile { a above d above g }
    ~~~ pile { b above e above h }
    ~~~ pile { c above f above j }
    }
    ~~~ right ] ->
left [
    {
    ~~~ pile { a above d above 0 above g }
    ~~~ pile { b above e above 0 above h }
    ~~~ pile { 0 above 0 above 1 above 0 }
    ~~~ pile { c above f above 0 above j }
    }
    ~~~ right ]
.EN
.IP
No errors or defaults are defined.

.Or "Model clip"
.Op clip_switch CLIP_INDICATOR
.IP
When processed by a renderer,
this command will
modify the renderer's \fImodel_clip\fP attribute.
.IP
An \fIOutputCommand\fP error is reported if the model clip
indicator is not \fIClip\fP or \fINoClip\fP.

.Or "Set model clip volume 3D"
.Op operator OPERATOR
.Op halfspaces LISTofHALFSPACE
.IP
When processed by a renderer, this command will
modify the renderer's \fImodel_clip_volume\fP attribute.
Each halfspace is defined by a point in modeling coordinates and a 
vector defining the normal to the plane which is the boundary of the 
half space.  The normal points in the direction of the halfspace,
and the point is considered to be on the plane.  

Each half-space is transformed by the current
composite modeling transformation and combined with the
current model clipping volume using the operater specified
by \fIoperator\fP.  The model clip volume which results
is not affected by subsequent changes to the
composite modeling transformation.  If the specified
operator is not supported by the PEX server or if any halfspace 
is degenerate, the output command is ignored.

.Or "Set model clip volume 2D"
.Op operator OPERATOR
.Op halfspaces LISTofHALFSPACE_2D
.IP
When processed by a renderer, this command will
modify the renderer's \fImodel_clip_volume\fP attribute.
Each halfspace is defined by a point in modeling coordinates and a 
vector defining the normal to the plane which is the boundary of the 
half space.  The normal points in the direction of the halfspace,
and the point is considered to be on the plane. 
The z component of each point and vector are assumed to be zero.

Each half-space is transformed by the current
composite modeling transformation and combined with the
current model clipping volume using the operater specified
by \fIoperator\fP.  The model clip volume which results
is not affected by subsequent changes to the
composite modeling transformation.  If the specified
operator is not supported by the PEX server or if any halfspace 
is degenerate, the output command is ignored.

.Or "Restore model clip volume"
.IP
When processed by a renderer,
this command will
modify the renderer's \fImodel_clip_volume\fP attribute.
The modeling clipping volume will be restored to
its state as of the last structure invocation, or to the
default state if no structure was invoked.
.IP
No errors or defaults are defined.

.Or "View index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIview_index\fP attribute.
If the specified view
index is not defined, index zero is used.  If view index zero is not defined,
the default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the index is 65535.

.Or "Light source state"
.Op enable LISTofCARD16
.Op disable LISTofCARD16
.IP
When processed by a renderer,
this command will
modify the renderer's \fIlight_state\fP attribute.  The
current \fIlight_state\fP is modified by activating ("turning on")
each light source whose index is specified in the \fIenable\fP list, and
by deactivating ("turning off")
each light source whose index is specified in the \fIdisable\fP list.
If any light in the \fIenable\fP or \fIdisable\fP list references an 
undefined \fILight\fP table entry, the light is ignored.
.IP
An \fIOutputCommand\fP error is reported if a light in either the
\fIenable\fP or \fIdisable\fP list is zero, or if the same light is
specified in both the lists.

.Or "Depth cue index"
.Op index TABLE_INDEX
.IP
When processed by a renderer,
this command will
modify the renderer's \fIdepth_cue_index\fP attribute.
If the specified depth
cue index is not defined, index zero is used.  If depth cue index zero is
not defined, the default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the index is 65535.

.Or "Pick ID       "
.Op pickid CARD32
.IP
When processed by a renderer,
this command will
modify the renderer's \fIpick_id\fP attribute.
.IP
No errors or defaults are defined.

.Or "HLHSR identifier"
.Op id CARD32
.IP
When processed by a renderer, this command will
modify the renderer's \fIHLHSR_identifier\fP attribute.
This output command is a noop for all registered HLHSR modes except
\fIZBufferId\fP.  If the renderer's \fIHLHSR_mode\fP is set to \fIZBufferId\fP
then a zero value will disable z-buffering and a value of one will
enable it.  For non-registered HLHSR modes the effect of this command 
is implementation dependent.  If the specified HLHSR
identifier is not supported by the PEX server, the HLHSR identifier used is
implementation dependent.

.Or "Color approximation index"
.Op index TABLE_INDEX
.IP
When processed by a renderer, this command will modify the renderer's
\fIcolor_approx_index\fP attribute.
If the specified color
approximation index is not defined, index zero is used.  If index zero is
not defined, the default values listed in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if the index is 65535.

.Or "Rendering color model"
.Op model COLOR_MODEL
.IP
When processed by a renderer, this command will modify the renderer's
\fIrdr_color_model\fP attribute.
If the specified
color model is not supported by the PEX server, model 0 (implementation
dependent) is used.
.IP
Any integer value may be specified as the color model in this output
command.

.Or "Parametric surface characteristics"
.Op psc PSURF_CHAR
.IP
When processed by a renderer, this command will modify the renderer's
\fIpsurf_char\fP attribute.
If the specified type
is not supported by the PEX server, type 1 (\fINone\fP) is used.
.IP
Any integer value may be specified as the characteristics type in this
output command.  If an inconsistent value is specified in the data
record, an \fIOutputCommand\fP error is reported.  (For example, if the
type is \fIIsoparametricCurves\fP and the curve placement is not
\fIUniform\fP or \fINonUniform\fP, or if the type is \fIMC_LevelCurves\fP
or \fIWC_LevelCurves\fP and the direction vector has zero length.)

.Or "Add names to name set"
.Op names LISTofNAME
.IP
When processed by a renderer,
this command will
add names to the renderer's name set.
If any name in the set
of normal or inverted name set names is outside the supported range
(see \fBPEXGetImpDepConstants\fP), that name is ignored.\(dg
.FS
\(dg PHIGS defines no errors for Add Names To Set.
.FE

.Or "Remove names from name set"
.Op names LISTofNAME
.IP
When processed by a renderer,
this command will
remove names from the renderer's name set.
If any name in the set
of normal or inverted name set names is outside the supported range
(see \fBPEXGetImpDepConstants\fP), that name is ignored.\(dd
.FS
\(dd PHIGS defines no errors for Remove Names From Set.
.FE

.Or "Execute structure"
.Op s_id STRUCTURE_ID
.IP
When processed by a renderer,
this output command transfers flow-of-control to the specified
structure.  
Processing of output commands
will then commence with the first structure element in the structure
specified by \fIs_id\fP.
When all of the elements in the called
structure have been processed, control will be returned.
When 
this command is executed, all of the rendering pipeline
attribute values in the renderer are saved.  Then,
the current global modeling transform is set to the current composite
modeling transform and the current local modeling matrix is set
to the identity matrix.
When control is returned,
the saved rendering pipeline
attribute values are restored.
.IP
If \fIs_id\fP
references a non-existent structure, an \fIOutputCommand\fP error is
reported.*
.FS
* PHIGS specifies that if the specified structure is non-existent, a
new empty structure is created.  In PEX, it is the client's responsibility 
to ensure that references to non-existent structure resource identifiers
do not occur.
.FE
Note that it is not possible to have traversal time errors due to non-existent
structures, since \fBPEXDestroyStructures\fP removes all references to
structures it deletes.

.Or "Label"
.Op label INT32
.IP
When processed by a renderer,
this output command is a no-op.  Its main usefulness
is when used as a structure element to maintain the specified
\fIlabel\fP as an aid to navigation during structure editing.
.IP
No errors or defaults are defined.

.Or "Application data"
.Op data LISTofCARD8
.IP
When processed by a renderer,
this output command is a no-op.  Its main usefulness
is when used as a structure element in order to maintain the specified
client application data.
.IP
No errors or defaults are defined.  Note that byte-swapping and floating
point conversion are not done on this type of output command.

.Or "GSE"
.Op id INT32
.Op data LISTofCARD8
.IP
When processed by a renderer, the effect of
this command is implementation-dependent.
Because of floating point and color format discrepancies across
a network interface, it is not anticipated that the GSE will provide
a useful extension mechanism, but
it is provided for PHIGS compatibility purposes.
If the specified GSE
identifier is not supported by the PEX server, the output command is ignored.
Note that byte-swapping and floating point conversions may not be done on this
type of output command.

.Or "Marker 3D"
.Op points LISTofCOORD_3D
.IP
When processed by a renderer,
this command will cause marker primitives to be rendered.
A marker is a geometric primitive with only one geometric attribute -
its position.  
The list \fIpoints\fP contains a list of 
3D coordinates, each of which specifies the position
of a marker in modeling coordinates.
.IP
During the rendering process, the marker's position is transformed
to a position in device coordinates.  A marker has no geometric size,
so geometric transformations will not affect the displayed size of the marker.
The marker's color is transformed only
by the depth-cueing computation (the light-source shading
stage of the rendering pipeline affects only surfaces)
and mapped to a device color.  The clipping of markers whose
position is inside the clipping volume
but whose rendering is outside the clipping volume
is implementation-dependent.
.IP
Depending on the setting of the marker attribute ASF values,
the \fImarker_color\fP,
\fImarker_type\fP, and \fImarker_scale\fP attributes are
either obtained directly from the current marker attribute values
or from the \fImarker_bundle_index\fP'th entry in the
renderer's \fImarker_bundle\fP.
.IP
No errors or defaults are defined.

.Or "Marker 2D"
.Op points LISTofCOORD_2D
.IP
.2d "marker"
.IP
No errors or defaults are defined.

.Or "Text 3D"
.Op origin COORD_3D
.Op vector1 VECTOR_3D
.Op vector2 VECTOR_3D
.Op string ISTRING
.IP
When processed by a renderer,
this command will cause a text string to be rendered.
The parameter
\fIstring\fP contains the text string to be rendered.
A string is a list of data records, each of which contains
\fIchar_set\fP, \fIchar_set_width\fP and \fIencoding_state\fP
fields, as well as a list of the characters that
actually make up that portion of the string.  The \fIchar_set\fP
field is an index into the current font group.  Font groups have
individual fonts numbered starting at one, so a value of three would
select the third font in the font group currently being used.
If a \fIchar_set\fP value is not available in
the current font group, then the entire string will be rendered using
the default font group.  If a \fIchar_set\fP value is not available
in the default font group, then that portion of the string will be
rendered in an implementation-dependent manner.
The \fIchar_set_width\fP indicates whether characters in the string are
8-, 16-, or 32-bit values.  Characters in the string are byte-swapped
by the PEX server, if necessary.  The
\fIencoding_state\fP field is provided for use by clients and is
not interpreted by the server.
.IP
The text string is located on a plane defined by its position and
direction vectors.
The origin of the string is a point in modeling
coordinates indicated by \fIorigin\fP, and the string's
direction is indicated by the direction vectors \fIvector1\fP and
\fIvector2\fP.
\fIVector1\fP defines the positive \fIx\fP-axis of
the text local coordinate system.
\fIVector2\fP defines the positive \fIy\fP-axis of
the text local coordinate system.
.IP
Depending on the setting of the text attribute ASF values,
the \fItext_color\fP,
\fItext_precision\fP,
\fIchar_expansion\fP,
\fIchar_spacing\fP, and
\fItext_font_index\fP attributes are
either obtained directly from the current text attribute values or from the
\fItext_bundle_index\fP'th entry in the renderer's
\fItext_bundle\fP.
The
\fIchar_height\fP,
\fIchar_up_vector\fP,
\fItext_path\fP, and
\fItext_alignment\fP attributes
are also used when drawing the text primitive.
An attempt is made to render the text string as accurately
as possible with the font group named by the current \fItext_font_index\fP.
The directions specified by
\fIchar_up_vector\fP and \fItext_path\fP
will be relative to the text local coordinate system.
.IP
During the rendering process, a string's position is transformed
to a position in device coordinates.  The string's color is transformed 
only
by the depth-cueing computation (the light-source shading
stage of the rendering pipeline affects only surfaces)
and mapped to a device color.  
The text string is clipped depending on the current text precision value.
If the text precision is \fIString\fP, clipping is done in an
implementation-dependent fashion.
If the text precision is \fIChar\fP, clipping is done on at least a
character-by-character basis.
If the text precision is \fIStroke\fP, clipping is performed at the clipping
boundaries for each character.
.IP
If either of the text
direction vectors is zero length or if the vectors are parallel, the vectors
<1,0,0> and <0,1,0> are used.

.Or "Text 2D"
.Op origin COORD_2D
.Op string ISTRING
.IP
When processed, this command will cause a text primitive to be drawn.
This primitive
functions exactly as the 3D text primitive
except that it has no direction vectors, the modeling
coordinate position is specified using only \fIx-\fP and \fIy-\fP
coordinates, and the \fIz\fP-coordinate is always assumed to be zero.
.IP
No errors or defaults are defined.

.Or "Annotation text 3D"
.Op origin COORD_3D
.Op offset COORD_3D
.Op string ISTRING
.IP
When processed by a renderer,
this command will cause an annotation text string to be rendered.
The parameter
\fIstring\fP contains the text string to be rendered.
A string is a list of data records, each of which contains
\fIchar_set\fP, \fIchar_set_width\fP and \fIencoding_state\fP
fields, as well as a list of the characters that
actually make up that portion of the string.  The \fIchar_set\fP
field is an index into the current font group.  Font groups have
individual fonts numbered starting at one, so a value of three would
select the third font in the font group currently being used.
If a \fIchar_set\fP value is not available in
the current font group, then the entire string will be rendered using
the default font group.  If a \fIchar_set\fP value is not available
in the default font group, then that portion of the string will be
rendered in an implementation-dependent manner.
The \fIchar_set_width\fP indicates whether characters in the string are
8-, 16-, or 32-bit values.  Characters in the string are byte-swapped
by the PEX server, if necessary.  The
\fIencoding_state\fP field is provided for use by clients and is
not interpreted by the server.
.IP
The origin of the string is a point in modeling
coordinates indicated by \fIorigin\fP.
An offset value in normalized projection coordinates is specified
by \fIoffset\fP.
The point at which the annotation text string is to be placed is
called the annotation point, and is
computed by adding \fIoffset\fP to the
transformed \fIorigin\fP point.
The \fIz\fP-component of the annotation point specifies the
the \fIx-y\fP plane in normalized projection coordinate space on
which the annotation text string will be placed.
.IP
Depending on the setting of the text attribute ASF values,
the \fItext_color\fP,
\fItext_precision\fP,
\fIchar_expansion\fP,
\fIchar_spacing\fP, and
\fItext_font_index\fP attributes are
either obtained directly from the current text attribute values or from the
\fItext_bundle_index\fP'th entry in the renderer's
\fItext_bundle\fP.
The
\fIatext_height\fP,
\fIatext_path\fP,
\fIatext_alignment\fP,
\fIatext_up_vector\fP, and
\fIatext_style\fP
attributes are also used when rendering the text string.
.IP
The annotation text string's color is transformed 
only
by the depth-cueing computation (the light-source shading
stage of the rendering pipeline affects only surfaces)
and mapped to a device color.  
The entire annotation text primitive is clipped if \fIorigin\fP is clipped.  If
\fIorigin\fP is not clipped by modeling, view, or workstation clipping, 
the annotation text will be clipped
according to text clipping rules and the connection line, if present,
will be clipped according to polyline clipping rules, except that
modeling clipping will be ignored.  The current set of polyline attributes
will be used to draw the connection line if it is to be drawn.
.IP
No errors or defaults are defined.

.Or "Annotation text 2D"
.Op origin COORD_2D
.Op offset COORD_2D
.Op string ISTRING
.IP
When processed, this command will cause an annotation text primitive
to be drawn.  This primitive
functions exactly as the 3D annotation text primitive
except that origin and offset
positions are specified using only \fIx-\fP and \fIy-\fP
coordinates, and the \fIz\fP-coordinate is always assumed to be zero.
.IP
No errors or defaults are defined.

.Or "Polyline 3D"
.Op vertices LISTofCOORD_3D
.IP
When processed by a renderer,
this command will cause a polyline to be rendered.
The polyline is defined by the list of vertices that are specified
in \fIvertices\fP, each of which is a coordinate in the modeling
coordinate system.
The vertices are joined together
by line segments.  The first vertex of a polyline is connected to the
second, the second connected to the third, and so on.  The last vertex
is \fInot\fP connected to the first.
.IP
Depending on the setting of the line attribute ASF values,
\fIline_color\fP,
\fIline_type\fP, and
\fIline_width\fP attributes are
either obtained directly from the current line attribute values or from the
\fIline_bundle_index\fP'th entry in the renderer's
\fIline_bundle\fP.
.IP
Polylines have no geometric width,
only length, so transformations will affect only the displayed length
of a polyline.
The polyline colors are transformed only
by the depth-cueing computation (the light-source shading
stage of the rendering pipeline affects only surfaces)
and mapped to device colors.  
Polylines are not displayed if they are
outside the currently defined clipping volume.
Polylines that cross the
clipping volume are clipped, and only the portion(s) inside the clipping
volume is (are) displayed.
.IP
At structure creation time, if the polyline has fewer than two vertices, it is
stored in the structure, but when this output command is interpreted, it has
no visual effect.  In immediate mode, such a primitive is ignored.

.Or "Polyline 2D"
.Op vertices LISTofCOORD_2D
.IP
.2e "polyline"
.IP
At structure creation time, if the polyline has fewer than two vertices, it is
stored in the structure, but when this output command is interpreted, it has
no visual effect.  In immediate mode, such a primitive is ignored.

.Or "Polyline set 3D with data"
.Op color_type COLOR_TYPE
.Op vert_attributes BITMASK_SHORT
.Op vertices LISTofLISTofVERTEX
.IP
When processed by a renderer,
this command will cause a series of polylines to be rendered.
The behavior of this primitive is identical to that of the
3D polyline primitive, except that multiple polylines can be drawn,
and additional information can be specified at each polyline vertex.
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
The parameter \fIvert_attributes\fP indicates the attributes which are
specified at each polyline vertex.  The components of the vertex attributes
bitmask are, in order:
.DS
	color		COLOR
.DE
If any of the attribute bits is set, the corresponding attributes
must be present for each vertex, and they must
be passed after the coordinate data for each vertex in the
order that they appear in the list above.
.IP
If color values are passed per vertex, they are considered to be part
of the primitive and are used instead of the \fIline_color\fP attribute.
In addition, the use of per-vertex colors is affected by the
\fIpolyline_interp\fP attribute, which is obtained directly
from the \fIpolyline_interp\fP value if the
\fIpolyline_interp_asf\fP attribute is set to \fIIndividual\fP or from the
\fIline_bundle_index\fP'th entry in the renderer's \fIline_bundle\fP.
The \fIpolyline_interp\fP attribute defines how color values between
the vertices are to be computed.
.IP
At structure creation time, if any polyline in the set has fewer than
two vertices, it is
stored in the structure, but when this output command is interpreted,
the polyline with insufficient data has
no visual effect.  In immediate mode, such a polyline is ignored.

.Or "Non-uniform B-spline curve" 3
.Op order CARD16
.Op type COORD_TYPE
.Op tmin FLOAT
.Op tmax FLOAT
.Op knots LISTofFLOAT
.Op points LISTofCOORD
.IP
When processed by a renderer,
this command will cause a non-uniform B-spline
curve to be rendered.
The \fIorder\fP is specified as a positive integer.
The spline shape is specified using
a list of knots in the parametric coordinate 
space, plus a list of control points that are specified
in modeling coordinates.
In general, the number of control points must be at least as large as
the order.  The number of control points plus the spline order must
equal the number of knots.
The \fIknots\fP sequence must form
a non-decreasing sequence of numbers.
.IP
The \fItype\fP parameter specifies whether the curve is \fIRational\fP
or \fINonRational\fP.
If the \fItype\fP is \fIRational\fP, then
the point list must be provided as homogeneous modeling coordinates (COORD_4D),
otherwise the the point list must be provided as non-homogeneous
modeling coordinates (COORD_3D). 
.IP
The parameter range values \fItmin\fP and \fItmax\fP specify the range
over which the B-spline curve is evaluated.  \fITmin\fP must be less than
\fItmax\fP, \fItmin\fP must be greater than or equal to the \fIorder\fP'th
knot value, and \fItmax\fP must be less than or equal to the
(k+1-\fIorder\fP)'th knot value, where k is the number of knots.
.IP
Depending on the setting of the line attribute ASF values,
\fIline_color\fP,
\fIline_type\fP,
\fIline_width\fP, and
\fIcurve_approx\fP attributes are
either obtained directly from the current line attribute values or from the
\fIline_bundle_index\fP'th entry in the renderer's
\fIline_bundle\fP.
.IP
When an output command of this type is interpreted, if the curve order is
not supported by the PEX server, the output command has no visual effect.
In immediate mode, such a primitive is ignored.
.IP
Several conditions may
cause an \fIOutputCommand\fP error to be reported:
\fItype\fP is not \fIRational\fP or \fINonRational\fP,
\fIorder\fP is less than one,
there are fewer control points than the order,
the order is inconsistent with the number of knots and control points,
the knots are not non-decreasing,
\fItmin\fP and \fItmax\fP are inconsistent with the knots,
or a \fIRational\fP control point has a w coordinate that is less than or
equal to zero.

.Or "Fill area 3D"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op vertices LISTofCOORD_3D
.IP
When processed by a renderer,
this command will cause a fill area primitive to be rendered.
A fill area is defined by a list of vertices which are to be joined together
to form a planar surface.  (Fill areas are not strictly
required to be planar, but strange shading artifacts can occur if
a fill area is not planar or nearly so.)
The first vertex of a fill area is connected to the
second, the second connected to the third, and so on.  The last vertex
is implicitly connected to the first.
.IP
During the rendering process, the fill area vertices are transformed
to positions in device coordinates.
The surface colors are transformed
by the light source shading computation (which utilizes the interior style
and the reflection model) and are further modified by the
depth-cueing computation
and mapped to device colors.  
Fill areas are not displayed if they are
outside the currently-defined clipping volume.
Fill areas that cross the
clipping volume are clipped, and only the portion(s) inside the clipping
volume is (are) displayed.
.IP
A fill area can
cross over itself to create a complex shape.  The odd-even rule is
used for determining the points that lie in the interior of a fill area.
The \fIshape\fP parameter is passed as a hint as to the type of fill
area that is defined by the \fIvertices\fP.
A shape hint of \fIUnknown\fP means that nothing is known about the
shape of the fill area.  A shape of \fIComplex\fP means that the
fill area edges may self-intersect.  A shape of \fINonconvex\fP means
that the edges do not self-intersect, but the fill area is not wholly
convex.  \fIConvex\fP means that all of the interior angles of the fill
area are convex.  Fill areas that are of a higher complexity than indicated
by their shape hint are drawn in an implementation-dependent manner.
PEX server extensions may ignore the shape
hint and treat all fill area primitives
as shape \fIUnknown\fP.
The \fIignore_edges\fP parameter specifies
whether or not surface edge attributes are to be applied to the fill
area primitives.  If it is \fITrue\fP, no surface edges will ever be
drawn for the \fIfill area\fP.  If \fIFalse\fP, surface edges will be
drawn using the current surface edge attributes if the surface edge flag
is \fIOn\fP.
Depending on the setting of the surface edge attribute ASF values, the
\fIsurface_edges\fP,
\fIsurface_edge_color\fP,
\fIsurface_edge_type\fP, and 
\fIsurface_edge_width\fP attributes are
either obtained directly from the current surface edge
attribute values or from the
\fIedge_bundle_index\fP'th entry in the renderer's
\fIedge_bundle\fP.
.IP
Depending on the setting of the surface attribute ASF values, the
\fIsurface_color\fP,
\fIinterior_style\fP,
\fIinterior_style_index\fP,
\fIsurface_interp\fP, and
\fIreflection_model\fP attributes are
either obtained directly from the current surface attribute values or from the
\fIinterior_bundle_index\fP'th entry in the renderer's
\fIinterior_bundle\fP.
If, when rendered, the fill area is determined to be front-facing with
respect to the point of view, the
\fIsurface_color\fP (obtained as previously described) and
\fIreflection_attr\fP attributes are used to compute the color(s) of the
fill area.
If the fill area is determined to be back-facing with
respect to the point of view, the
\fIbf_surface_color\fP and
\fIbf_reflection_attr\fP attributes are used instead.
Regardless of the orientation of the fill area, if the \fIinterior_style\fP
is \fIPattern\fP, the
\fIpattern_size\fP,
\fIpattern_ref_pt\fP,
\fIpattern_ref_vec1\fP, and
\fIpattern_ref_vec2\fP attributes may be used to
pattern the fill area.
.IP
At structure creation time, if the fill area has fewer than three vertices,
it is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
or \fIignore_edges\fP parameter is invalid.

.Or "Fill area 2D"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op vertices LISTofCOORD_2D
.IP
.2e "fill area"
.IP
At structure creation time, if the fill area has fewer than three vertices,
it is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
or \fIignore_edges\fP parameter is invalid.

.Or "Fill area 3D with data"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op color_type COLOR_TYPE
.Op facet_attributes BITMASK_SHORT
.Op vert_attributes BITMASK_SHORT
.Op facet FACET
.IP
When processed by a renderer,
this command will cause a fill area to be rendered.
The behavior of this primitive is identical to that of the
3D fill area primitive, except that
additional information can be specified
for the fill area itself and for
each vertex.
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
.IP
The parameter \fIfacet_attributes\fP indicates the attributes which are
specified for the fill area.  The components of the facet attributes
bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute
must be present in the data that defines the fill area facet and they
must be passed in the
order that they appear in the list above.  If a color value is
passed per facet, it is taken to be the intrinsic color of the front face
of the facet.  If a normal is passed per facet, it is taken to be
the normal to the facet.  
.IP
The parameter \fIvert_attributes\fP specifies the attributes which are
specified at each fill area vertex.  The components of the vertex attributes
bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attributes
must be present for each vertex, and they must
be passed after the coordinate data for each vertex in the
order that they appear in the list above.
.IP
If color values are passed per vertex, they are considered to be part
of the primitive and are used instead of the \fIsurface_color\fP attribute.
Vertex colors will be utilized rather than facet colors
if both are provided.
If normals are passed per vertex, they are taken to be
the normals at the vertices of the fill area.
In addition, the use of per-vertex colors is affected by the
\fIsurface_interp\fP attribute, which is obtained directly
from the \fIsurface_interp\fP value if the
\fIsurface_interp_asf\fP attribute is set to \fIIndividual\fP or from the
\fIinterior_bundle_index\fP'th entry in the renderer's
\fIinterior_bundle\fP.
The \fIsurface_interp\fP attribute defines how color values between
the vertices are to be computed.
.IP
All normals are assumed to be unit vectors.  The effect of rendering 
with facet or vertex normals that are not unit vectors is implementation
dependent.
.IP
The \fIshape\fP parameter is passed as a hint as to the type of fill
area that is defined by the vertices.
A shape hint of \fIUnknown\fP means that nothing is known about the
shape of the fill area.  A shape of \fIComplex\fP means that the
fill area edges may self-intersect.  A shape of \fINonconvex\fP means
that the edges do not self-intersect, but the fill area is not wholly
convex.  \fIConvex\fP means that all of the interior angles of the fill
area are convex.  Fill areas that are of a higher complexity than indicated
by their shape hint are drawn in an implementation-dependent manner.
PEX server extensions may ignore the shape
hint and treat all fill area primitives
as shape \fIUnknown\fP.
The \fIignore_edges\fP parameter specifies
whether or not surface edge attributes are to be applied to the fill
area primitives.  If it is \fITrue\fP, no surface edges will ever be
drawn for the \fIfill area\fP.  If \fIFalse\fP, surface edges will be
drawn using the current surface edge attributes if the surface edge flag
is \fIOn\fP.
.IP
At structure creation time, if the fill area has fewer than three vertices,
it is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
or \fIignore_edges\fP parameter is invalid.


.Or "Fill area set 3D"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op contour_hint CONTOUR
.Op vertices LISTofLISTofCOORD_3D
.IP
When processed by a renderer,
this command will cause a fill area set primitive to
be drawn.  This primitive is essentially a set of fill area
primitives which, together, define a polygon with
islands or holes.  The planarity of the resulting polygon is not 
verified, but the results of processing a non-planar fill area set 
are implementation dependent.
The \fIignore_edges\fP parameter will be applied to all of
the fill areas in the fill area set.
.IP
The \fIshape\fP parameter is passed as a hint as to the type of contours
that comprise the fill area set.  A shape hint of \fIUnknown\fP
means that nothing is known about the shape of the constituent contours.
A shape of \fIComplex\fP means that some contours of the fill
area set may have edges that self-intersect.  A shape of \fINonconvex\fP
means that none of the contours of the fill area set have edges that
self-intersect, but some may not be wholly convex.  \fIConvex\fP means
that all of the interior angles of all of the contours are convex.
Contours that are of a higher complexity than indicated by their shape
hint are drawn in an implementation-dependent manner.  PEX server
extensions may ignore the shape hint and treat all constituent contours
as shape \fIUnknown\fP.
.IP
The \fIcontour_hint\fP parameter provides further information as to
the relationships between contours in the fill area set.
If \fIcontour_hint\fP is \fIDisjoint\fP, all contours will be spatially
disjoint.  No overlapping or intersection occurs between any contours
in the fill area set.
If \fIcontour_hint\fP is \fINested\fP, contours will either be disjoint
or wholly contained within another contour.  No contour will have edges
that intersect or are coincident with edges of any other contour.
If \fIcontour_hint\fP is \fIIntersecting\fP, separate contours may
have edges that are coincident or overlap. 
If \fIcontour_hint\fP is \fIUnknown\fP, nothing is known about the
interrelationships between contours.
Fill area sets with contours that have higher complexity interrelationships
than that indicated by their contour
hint are drawn in an implementation-dependent manner.  PEX server
extensions may ignore the contour hint and treat all fill area sets
as \fIUnknown\fP.
.IP
At structure creation time, if any of the contours of the fill area set
has fewer than three vertices, or if there are no contours defined, the
output command is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
or \fIignore_edges\fP parameter is invalid.

.Or "Fill area set 2D"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op contour_hint CONTOUR
.Op vertices LISTofLISTofCOORD_2D
.IP
.2e "fill area set"
.IP
At structure creation time, if any of the contours of the fill area set
has fewer than three vertices, or if there are no contours defined, the
output command is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
or \fIignore_edges\fP parameter is invalid.

.Or "Fill area set 3D with data"
.Op shape SHAPE
.Op ignore_edges BOOLEAN
.Op contour_hint CONTOUR
.Op color_type COLOR_TYPE
.Op facet_attributes BITMASK_SHORT
.Op vert_attributes BITMASK_SHORT
.Op facet_data OPT_DATA
.Op vertices LISTofLISTofVERTEX
.IP
When processed by a renderer,
this command will cause a fill area set to be rendered.
The behavior of this primitive is identical to that of the
3D fill area set primitive, except that
additional information can be specified
for each of the fill area sets, for each edge, and for
each vertex.
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
.IP
The parameter \fIfacet_attributes\fP indicates the attributes which are
specified in the \fIfacet_data\fP parameter.
The components of the facet attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attributes
must be present in the \fIfacet_data\fP parameter and
must be passed in the
order that they appear in the list above.  If a color value is
passed as part of \fIfacet_data\fP, it is taken to be the intrinsic color
of the front face of the fill area set.
.IP
The parameter \fIvert_attributes\fP specifies the attributes which are
specified at each fill area set vertex.  The components of the vertex
attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
	edges		SWITCH
.DE
If any of the attribute bits is set, the corresponding attributes
must be present for each vertex, and they must
be passed after the coordinate data for each vertex in the
order that they appear in the list above.
.IP
If color values are passed as part of \fIfacet_data\fP or as part of the
\fIvertices\fP list, they are considered to be part
of the primitive and are used instead of the \fIsurface_color\fP attribute.
Vertex colors will be utilized rather than facet colors
if both are provided.
If normals are passed per vertex, they are taken to be
the normals at the vertices of the fill area.
If surface edge flags are specified per vertex, each flag specifies
whether to draw the edge from the vertex with which the flag is specified
to the next vertex.  (E.g., for a facet with four vertices, the edge
flag associated with vertex #1 indicates whether to draw edge #1-#2,
edge flag #2 specifies edge #2-#3, edge flag #3 specifies edge #3-#4,
and edge flag #4 specifies edge #4-#1.)  Surface edges are always drawn
with the surface edge color, never with per facet or per vertex colors.
.IP
In addition, the use of per-vertex colors is affected by the
\fIsurface_interp\fP attribute, which is obtained directly
from the \fIsurface_interp\fP value if the
\fIsurface_interp_asf\fP attribute is set to \fIIndividual\fP or from the
\fIinterior_bundle_index\fP'th entry in the renderer's
\fIinterior_bundle\fP.
The \fIsurface_interp\fP attribute defines how color values between
the vertices are to be computed.
.IP
At structure creation time, if any of the contours of the fill area set
has fewer than three vertices, or if there are no contours defined, the
output command is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP,
\fIignore_edges\fP, or \fIcontour_hint\fP parameter is invalid.

.Or "Triangle strip"
.Op color_type COLOR_TYPE
.Op facet_attributes BITMASK_SHORT
.Op vert_attributes BITMASK_SHORT
.Op facet_data LISTofOPT_DATA
.Op vertices LISTofVERTEX
.IP
When processed by a renderer,
this command will cause a triangle strip primitive to
be drawn.
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
The parameter \fIfacet_attributes\fP indicates the attributes which are
specified for each facet of the triangle strip.
The components of the facet attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute
must be present in \fIfacet_data\fP, which is
the data that defines each triangular facet.
The attributes that are passed in this way must be passed in the
order that they appear in the list above.  If a color value is
passed per facet, it is taken to be the intrinsic color of the front face
of the facet.  If a normal is passed per facet, it is taken to be
the normal to the facet.
There will be n-2 entries in the \fIfacet_data\fP list, where n is
the number of entries in the \fIvertices\fP list.
.IP
The parameter \fIvert_attributes\fP specifies the attributes which are
specified at each triangle strip vertex.  The components of the vertex
attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute
must be present for each vertex, and it must
be passed after the coordinate data for each vertex.  The
attributes that are passed in this way must be passed in the
order that they appear in the list above.
.IP
If color values are passed per vertex, they are considered to be part
of the primitive and are used instead of the \fIsurface_color\fP attribute.
Vertex colors will be utilized rather than facet colors
if both are provided.
If normals are passed per vertex, they are taken to be
the normals at the vertices of the facet.
.IP
All normals are assumed to be unit vectors.  The effect of rendering 
with facet or vertex normals that are not unit vectors is implementation
dependent.
.IP
The triangle strip is created
from the vertex array.  The strip is composed
of n-2 triangles, where n is the number of vertices.  The first triangle
is formed from the first three vertices in the list, the second triangle
is formed by the second through the fourth vertices in the list, etc., up
to the last triangle, which is formed by the last three vertices in the
list.
.IP
All attributes that affect the representation of fill area sets also
affect the representation of the triangle strip primitive.
.IP
At structure creation time, if the triangle strip
has fewer than three vertices, the
output command is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.

.Or "Quadrilateral mesh"
.Op shape SHAPE
.Op color_type COLOR_TYPE
.Op m_pts CARD16
.Op n_pts CARD16
.Op facet_attributes BITMASK_SHORT
.Op vert_attributes BITMASK_SHORT
.Op facet_data LISTofOPT_DATA
.Op vertices LISTofVERTEX
.IP
When processed by a renderer,
this command will cause a quadrilateral mesh primitive
to be rendered.
The \fIshape\fP parameter is passed as a hint as to the type of quadrilaterals
that comprise the primitive.
A shape hint of \fIUnknown\fP means that nothing is known about the
shape of the constituent quadrilaterals.
A shape of \fIComplex\fP means that the
some quadrilaterals may have edges that self-intersect.
A shape of \fINonconvex\fP means that none of the quadrilaterals
have edges that self-intersect, but some may not be wholly
convex.  \fIConvex\fP means that all of the interior angles of all of
the quadrilaterals are convex.
Quadrilaterals that are of a higher complexity than indicated
by their shape hint are drawn in an implementation-dependent manner.
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
.IP
The parameter \fIfacet_attributes\fP indicates the attributes which are
specified for each facet of the quadrilateral mesh.
The components of the facet attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute
must be present in \fIfacet_data\fP, which is
the data that defines each quadrilateral facet.
The attributes that are passed in this way must be passed in the
order that they appear in the list above.  If a color value is
passed per facet, it is taken to be the intrinsic color of the front face
of the facet.  If a normal is passed per facet, it is taken to be
the normal to the facet.
.IP
The parameter \fIvert_attributes\fP specifies the attributes which are
specified at each quadrilateral mesh vertex.  The components of the vertex
attributes bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute
must be present for each vertex, and it must
be passed after the coordinate data for each vertex.  The
attributes that are passed in this way must be passed in the
order that they appear in the list above.
.IP
If color values are passed per vertex, they are considered to be part
of the primitive and are used instead of the \fIsurface_color\fP attribute.
Vertex colors will be utilized rather than facet colors
if both are provided.
If normals are passed per vertex, they are taken to be
the normals at the vertices of the facet.
.IP
All normals are assumed to be unit vectors.  The effect of rendering 
with facet or vertex normals that are not unit vectors is implementation
dependent.
.IP
The surface will be created from a vertex array that is stored in
row major order
(i.e., the column number varies fastest as vertices are stored
in the array).  The
(ith,jth), (i+1th,jth), (i+1th,j+1th) and (ith,j+1th) vertices are connected
to create a single facet. 
Adjacent vertices are interconnected until the entire
facet network is processed.
There are \fIm_pts \(mu n_pts\fP entries in the \fIvertices\fP array, and there
are \fI(m_pts-1) \(mu (n_pts-1)\fP entries in the \fIfacet_data\fP array if any
per-facet attributes are passed.
\fIm_pts\fP is the number of columns in the vertex array and \fIn_pts\fP
is the number of rows.
.IP
It is allowable for the boundary of a single facet to 
not reside in a single plane.  The treatment of the vertex attributes
in this case is implementation-dependent.  
.IP
All attributes that affect the representation of fill area sets also
affect the representation the quadrilateral mesh primitive.
.IP
At structure creation time, if either \fIm_pts\fP or \fIn_pts\fP is less
than two, the
output command is stored in the structure, but when this output command is
interpreted, it has no visual effect.  In immediate mode, such a primitive
is ignored.
.IP
An \fIOutputCommand\fP error is reported if the \fIshape\fP
parameter is invalid.

.Or "Set of fill area sets"
.Op shape SHAPE
.Op color_type COLOR_TYPE
.Op fas_attributes BITMASK_SHORT
.Op vert_attributes BITMASK_SHORT
.Op edge_attributes BITMASK_SHORT
.Op contour_hint CONTOUR
.Op contours_all_1 BOOLEAN
.Op num_fas CARD16
.Op num_verts CARD16
.Op num_edges CARD16
.Op num_contours CARD16
.Op per_fas_data LISTofOPT_DATA
.Op per_vert_data LISTofVERTEX
.Op per_edge_data LISTofSWITCH
.Op contours LISTofLISTofLISTofCARD16
.IP
When processed by a renderer, this command will draw a set of fill
area sets that may be connected (i.e., individual
fill area sets may share geometry and
attribute information at vertices).  Shading calculations and
transformations need only be performed once per shared vertex
instead of once for every fill area set that shares the vertex.
Similarly, data can be transmitted across the network once per
unique vertex instead of once for every fill area set sharing the
vertex.
.IP
The \fIshape\fP parameter is passed as a hint as to the type of contours
that comprise each of the fill area sets.  A shape hint of \fIUnknown\fP
means that nothing is known about the shape of the constituent contours.
A shape of \fIComplex\fP means that some contours of the fill
area sets may have edges that self-intersect.  A shape of \fINonconvex\fP
means that none of the contours of the fill area sets have edges that
self-intersect, but some may not be wholly convex.  \fIConvex\fP means
that all of the interior angles of all of the contours are convex.
(Note that a fill area set with more than one contour is always
allowed to have contours that intersect.  It is quite possible that
the only times that the fastest rendering code path can be taken are if
the number of contours in a fill area set is equal to one or if
\fIcontours_all_1\fP is \fITrue\fP, and the
shape flag for the set of fill area sets primitive is \fIConvex\fP.)
Contours that are of a higher complexity than indicated by their shape
hint are drawn in an implementation-dependent manner.  PEX server
extensions may ignore the shape hint and treat all constituent contours
as shape \fIUnknown\fP.
.IP
The \fIcontour_hint\fP parameter provides further information as to
the relationships between contours in each of the fill area sets.
If \fIcontour_hint\fP is \fIDisjoint\fP, all contours will be spatially
disjoint.  No overlapping or intersection occurs between any contours
in any of the fill area sets.
If \fIcontour_hint\fP is \fINested\fP, contours will either be disjoint
or wholly contained within another contour.  No contour will have edges
that intersect or are coincident with edges of any other contour.
If \fIcontour_hint\fP is \fIIntersecting\fP, separate contours may
have edges that are coincident or overlap. 
If \fIcontour_hint\fP is \fIUnknown\fP, nothing is known about the
interrelationships between contours.
Fill area sets with contours that have higher complexity interrelationships
than that indicated by their contour
hint are drawn in an implementation-dependent manner.  PEX server
extensions may ignore the contour hint and treat all fill area sets
as \fIUnknown\fP.
.IP
Color values that are passed will be of the type specified by
\fIcolor_type\fP.
.IP
The parameter \fIfas_attributes\fP indicates the attributes that are
specified for each fill area set.  The components of the
\fIfas_ attributes\fP bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute must
be present in \fIper_fas_data\fP, which contains one data record for each
fill area set.  The attributes that are passed in this way must be
passed in the order that they appear in the list above.  If a color
value is passed, it is taken to be the intrinsic color of the front
face of the fill area set.  If a normal is passed, it is taken to be
the normal to the fill area set.
If \fIfas_attributes\fP is null, the \fIper_fas_data\fP list will be null
as well.  Otherwise, there will be \fInum_fas\fP entries in
\fIper_facet_data\fP.
.IP
The parameter \fIvert_attributes\fP indicates the attributes that are
provided at each vertex in the list of unique vertices specified
by \fIper_vert_data\fP.  The components of the vertex attributes
bitmask are, in order:
.DS
	color		COLOR
	normal	VECTOR_3D
.DE
If any of the attribute bits is set, the corresponding attribute must
be present for each vertex in the \fIper_vert_data\fP list, and it must be
passed after the coordinate data for each vertex.  The attributes that
are passed in this way must be passed in the order that they appear in
the list above.  If color values are passed per vertex, they are
considered to be part of the primitive and are used instead of the
\fIsurface_color\fP attribute.  If normals are passed per vertex, they
are taken to be the normal at the indicated vertex, and will be used
by all contours that share the vertex.  There will always be
\fInum_verts\fP entries in the \fIper_vert_data\fP list.
.IP
All normals are assumed to be unit vectors.  The effect of rendering 
with facet or vertex normals that are not unit vectors is implementation
dependent.
.IP
The parameter \fIedge_attributes\fP specifies the attributes that are
specified at each edge.  The components of the edge attributes
bitmask are, in order:
.DS
	edges		SWITCH
.DE
If any of the attribute bits is set, the corresponding attribute
must be present for each edge in the list \fIper_edge_data\fP.  If
none of the attribute bits are set, the list \fIper_edge_data\fP
will be an empty list.  Otherwise, it will contain \fInum_edges\fP
entries, each of which contains a switch indicating whether or not
the corresponding edge should be drawn.  This list is a flattened
list without counts, so if it is nonempty, it is up to the PEX
server extension to maintain a pointer to the proper position in
this list while processing the data in \fIcontours\fP.  If edge
switches are supplied, each flag specifies whether to draw the edge
from the vertex with which the flag is specified to the next vertex.
(E.g., for a contour with four vertices, the edge flag associated
with vertex #1 indicates whether to draw edge #1-#2, edge flag #2
specifies edge #2-#3, edge flag #3 specifies edge #3-#4, and edge
flag #4 specifies edge #4-#1.)  Surface edges are always drawn with
the surface edge color, never with per-fill-area-set or per-vertex
colors.
.IP
The connectivity of the primitive is defined by the \fIcontours\fP array.
The number of contours in the first fill area set is contained as
the first entry in the \fIcontours\fP array.  The number of contours
is followed by a list of data records, one for each of the contours
in the fill area set.  If this number is \fIn\fP, then the next \fIn\fP
data records in the \fIcontours\fP array are used to define the first
fill area set.  The value following the contour count contains
the number of vertices in the first contour of the first fill area
set.  If this number is \fIm\fP, then the next \fIm\fP values in the
array comprise the data record for the first contour.  This data
record contains the indices for the vertices of the first contour.
Depending on \fIn\fP, the next value in the list is either the number
of vertices in the second contour for the first fill area set,
or it is the number of contours in the second fill area set.
As a special case, if \fIcontours_all_1\fP is \fITrue\fP, then the
contour count field in each fill area set is guaranteed to be one.
.IP
Vertices are numbered with indices starting from zero (i.e., the first
vertex is referenced as vertex 0).
.IP
All attributes that affect the representation of fill area sets also
affect the representation of the set of fill area sets primitive.
.IP
An \fIOutputCommand\fP error is reported if any of the edge
flags in the \fIper_edge_data\fP array is not \fIOn\fP or \fIOff\fP, or
if any of the \fIshape\fP, \fIcontour_hint\fP, or \fIcontours_all_1\fP
parameters is invalid, 
or if any of the vertex indices in the \fIcontours\fP array is out of range,
or if there are not as many edge
flags as there are vertex indices in the lists in the contours array.

.Or "Non-uniform B-spline surface" 3
.Op type COORD_TYPE
.Op u_order CARD16
.Op v_order CARD16
.Op u_knots LISTofFLOAT
.Op v_knots LISTofFLOAT
.Op mpts CARD16
.Op npts CARD16
.Op points LISTofCOORD
.Op trim_curves LISTofLISTofTRIM_CURVE
.IP
When processed by a renderer,
this command will draw a non-uniform B-spline surface.
It generates the spline surface as a function of
the parametric variables u and v.  \fIU_order\fP and \fIv_order\fP
indicate the order of the parametric variables and 
are specified as positive integers.
The spline shape is specified using
two lists of knots in the parametric coordinate 
space, plus an array of control points that are specified
in modeling coordinates.
The \fIu_knots\fP sequence and the \fIv_knots\fP sequence must each form
a non-decreasing sequence of numbers.  \fIMpts\fP indicates the number
of points in the \fIu\fP direction and \fInpts\fP indicates the number
of points in the \fIv\fP direction.  The points are stored in the vertex
array in row major order
(i.e., the column number varies fastest as vertices are stored
in the array) and the rows increase in the direction of increasing \fIv\fP.
.IP
The minimum and maximum knot values in \fIu_knots\fP define the range
over which the B-spline surface is evaluated in the \fIu\fP parametric 
direction, and
the minimum and maximum knot values in \fIv_knots\fP define the range
over which the B-spline surface is evaluated in the \fIv\fP parametric 
direction.
.IP
The \fItype\fP parameter specifies whether the surface is \fIRational\fP
or \fINonRational\fP.
If the \fIsurface_type\fP is \fIRational\fP, then
the point list must be provided as homogeneous modeling coordinates (COORD_4D),
otherwise the point list must be provided as non-homogeneous
modeling coordinates (COORD_3D). 
.IP
In addition to the parametric bounds, a list of
trimming loops may be specified.
Trimming loops serve to further restrict the region in 
parametric coordinate space over which the 
B-spline surface is to be evaluated.
Each trimming loop is defined as
a list of one or more B-spline trimming
curves that are connected head-to-tail.
Each trim curve can be
\fIRational\fP or \fINonRational\fP, have a different order, etc.
The list must be explicitly closed, so that the tail of the last B-spline curve 
joins the head of the first B-spline curve 
in each trimming loop.
Each trimming curve
is parameterized independently.  If there is a floating point
inaccuracy in closure or in head-to-tail connectivity between B-spline curves
, closure or connectivity will be assumed.
B-spline curves for trimming loops are defined in the
parameter space of the surface and may not go outside the parameter
space of the surface.
When no trimming loops are
specified, the rectangular parameter limits of the surface are rendered
as the edges of the surface based on the edge flag attribute.
.IP
Trimming loops define the region of the surface that is to be rendered
based on the following two rules:  (1) a point is in the portion of the
surface to be rendered if any ray projected from it to infinity
has an odd number of intersections with trimming loops, and (2)
traveling in the direction of a trimming loop, the portion of the
surface to be trimmed away should be on the right and the portion to be
retained should be on the left.  In other words, a loop defined in
counterclockwise
order will cause the interior of the loop to be retained and the exterior
to be clipped away.  A clockwise loop will cause the exterior of
the loop to be retained and the interior to be trimmed away.  If loops
are nested, they must alternate in direction. In all cases, the outermost loop
must be counterclockwise.
No B-spline curve may 
intersect itself and no trimming loop may intersect itself or any other
trimming loop. 
Trimming loops that do not obey these rules
will result in an implementation-dependent rendering.
.IP
Each B-spline curve has a visibility flag that controls its visibility
for the purposes of surface edge display.  Depending on the settings of
a renderer's surface edge attributes and the visibility
flags associated with trim curves, the B-spline curves in trimming
loops may be drawn as surface edges.
.IP
All attributes that affect the representation of fill area sets also
affect the representation of the non-uniform B-spline surface primitive.
In addition, the \fIsurface_approx\fP attribute
is used to determine how to approximate the B-spline surface and the
\fIpsurf_char\fP attribute is used to specify the appearance of the
B-spline surface.
.IP
If either of the
surface orders or any of the trim curve orders is not supported by the
PEX server, the output command has no visual effect.
In immediate mode, such a primitive is ignored.
.IP
Several conditions may cause an \fIOutputCommand\fP error to be reported:
\fItype\fP is not \fIRational\fP or \fINonRational\fP,
\fIorder\fP is less than one,
there are fewer control points in either the u or v direction than the order,
the order is inconsistent with the number of knots and control points,
the knots are not non-decreasing,
a \fIRational\fP control point has a w coordinate that is less than or
equal to zero,
a trim curve order is less than 2,
a trim curve does not have enough control points for its order,
a trim curve's order is inconsistent with its number of knots and control points,
a trim curve's knot sequence is not non-decreasing,
or a trim curve's parameter range is inconsistent with its knots.

.Or "Cell array 3D"
.Op point1 COORD_3D
.Op point2 COORD_3D
.Op point3 COORD_3D
.Op dx CARD32
.Op dy CARD32
.Op colors LISTofTABLE_INDEX
.IP
When processed by a renderer,
this command will cause a cell array primitive to be rendered.
Color values that are passed will be of type \fIIndexed\fP.
A cell array is a parallelogram of equal-sized cells, each of which is
a parallelogram and has a single color.
Each cell has a width defined by:
.DS C
.EQ
width ~=~ sqrt {(point1.x ~-~ point2.x) sup 2 ~+~ (point1.y ~-~ point2.y) sup 2 ~+~ (point1.z ~-~ point2.z) sup 2} over dx
.EN
.DE
.IP
and a height defined by:
.DS C
.EQ
height ~=~ sqrt {(point1.x ~-~ point3.x) sup 2 ~+~ (point1.y ~-~ point3.y) sup 2 ~+~ (point1.z ~-~ point3.z) sup 2} over dy
.EN
.DE
The colors are specified in a one-dimensional array of size \fIdx \(mu dy\fP.
The color of each cell is specified by the index of the corresponding
element in the \fIcolors\fP array.
Colors are stored in this array by rows, that is, the column number varies
fastest as colors are stored into the array.  The first color in the
array is the color at the cell at the corner of \fIpoint1\fP, and subsequent
colors represent the colors of cells proceeding to \fIpoint2\fP.
.IP
If any color index is
not defined, color index one is used.  If color index one is not defined, the
default values in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if \fIdx\fP or \fIdy\fP is zero.

.Or "Cell array 2D"
.Op point1 COORD_2D
.Op point2 COORD_2D
.Op dx CARD32
.Op dy CARD32
.Op colors LISTofTABLE_INDEX
.IP
.2e "cell array"
In addition, the cell array is defined by two points which define a
rectangle that is taken to be aligned with the modeling coordinate axes.
.IP
If any color index is
not defined, color index one is used.  If color index one is not defined, the
default values in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if \fIdx\fP or \fIdy\fP is zero.

.Or "Extended cell array 3D"
.Op color_type COLOR_TYPE
.Op point1 COORD_3D
.Op point2 COORD_3D
.Op point3 COORD_3D
.Op dx CARD32
.Op dy CARD32
.Op colors LISTofCOLOR
.IP
When processed by a renderer,
this command has the same effect as the "cell array 3D" primitive,
except that the colors may be passed as indexed or direct
color values, depending on the setting of \fIcolor_type\fP.
.IP
If any color index is
not defined, color index one is used.  If color index one is not defined, the
default values in Appendix D are used.
.IP
An \fIOutputCommand\fP error is reported if \fIdx\fP or \fIdy\fP is zero.

.Or "GDP 3D"
.Op gdp_id INT32
.Op points LISTofCOORD_3D
.Op data LISTofCARD8
.IP
When processed by a renderer,
the effect of this command is implementation-dependent.
Because of floating point and color format discrepancies across
a network interface, it is not anticipated that the GDP 3D will provide
a useful extension mechanism, but
it is provided for PHIGS
compatibility purposes.
.IP
If the specified GDP
identifier is not supported, the output command is ignored.  Note that
byte-swapping and floating point conversion may not be done on this type
of output command.

.Or "GDP 2D"
.Op gdp_id INT32
.Op points LISTofCOORD_2D
.Op data LISTofCARD8
.IP
When processed by a renderer,
the effect of this command is implementation-dependent.
Because of floating point and color format discrepancies across
a network interface, it is not anticipated that the GDP 2D will provide
a useful extension mechanism, but
it is provided for PHIGS
compatibility purposes.
.IP
If the specified GDP
identifier is not supported, the output command is ignored.  Note that
byte-swapping and floating point conversion may not be done on this type
of output command.

.Or "Noop"
.IP
When processed by a renderer, this command will
only update the current path.  
It is useful for client-side accumulate state traversals
since it does not generate any output.  

.bp
.AC "Lookup Tables" 1
.LP
.RU
.LP
A \fIlookup table\fP is a PEX resource that allows clients
to create tables of values for various purposes.  Lookup
tables are used to support a level of indirection for some
output primitive  attributes as well as for storing view information,
light source descriptions, depth-cue information, etc.
.EQ
delim $$
.EN
.LP
Tables may be sparse, therefore tables consist of index/entry
pairs.  The \fIindex\fP is the number that will be used to reference
that entry.  Since indices are 16-bit values, index values are
allowed to be any value in the range [0..65535], with the possible
exceptions of 0 (some tables allow an index of zero, others do not)
and 65535 (tables can have at most 
$2 sup 16 ~-~ 1$
definable entries, so if
a table begins at zero, the maximum index value is 65534).
An \fIentry\fP is the collection of information (or the data record) that
is defined for each type of table.  The table descriptions in this section
include the definition of an entry for each type of table.
A table index refers to an \fIundefined entry\fP if no table entry
has ever been associated with that index, or if the associated table
entry has been deleted.
A table entry may contain one or more data items, depending on the table
type.  For instance, in the \fIMarkerBundle\fP table type, each entry
in the table consists of a marker type value, a marker scale
value, and a marker color value.
.LP
A lookup table may have \fIpredefined entries\fP.  Predefined entries
are a set of contiguous table entries that are 
automatically filled in by the server when the table is created.
\fBPEXGetTableInfo\fP returns the range of predefined 
entries.  Predefined table entries may be deleted or overwritten.
.LP
Each type of table has a specific index value that indicates the
\fIdefault table entry\fP.
If a table index references an undefined table entry, the contents
of the default table
entry will be used.  If the
default table entry is undefined, then
the default attribute values (as listed in Appendix D)
will be used as the default entry.
The default entry for all tables
is one, except for the view, depth cue,
and color approximation
tables whose default entry is zero.
.LP
PEX lookup tables are designed to support PHIGS set/realized
semantics.  A lookup table entry may be set to a value that is impossible
to represent exactly.
Such a value will be silently mapped
to a reasonable default when rendering to the drawable.
When
retrieving entry values from a lookup table,
clients may request the value as originally specified
by the client (\fIvalue_type\fP = \fISet\fP), or the value
that is actually used when rendering, whether it is the value
originally specified or an approximation (\fIvalue_type\fP = \fIRealized\fP).
Since all predefined entries are by definition realizable,
predefined lookup table entries will return the
same value regardless of whether the \fISet\fP or the \fIRealized\fP value
is requested.
.LP
The allowable table types, the range of allowable index values,
the default entry,
and the format of a table entry for each table type are as follows:
 

.Bl "LineBundle   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain attributes for drawing polyline and curve primitives.
Depending on the setting of the aspect source flag
attributes, polyline and curve attributes may be obtained from a line
bundle table.
Each entry in this type of table consists of the following:
.DS
.ta 1.0i 2.5i
	line_type	LINE_TYPE
	polyline_interp	POLYLINE_INTERP
	curve_approx	CURVE_APPROX
	line_width	FLOAT
	line_color	COLOR_SPECIFIER
.ta
.DE
The attributes stored in a line bundle table are defined and used in
the same fashion as the pipeline context attributes of the same name.


.Bl "MarkerBundle   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain attributes for drawing marker primitives.
Depending on the setting of the aspect source flag
attributes, marker attributes may be obtained from a marker bundle table.
Each entry in this type of table consists of the following:
.DS
.ta 1.0i 2.5i
	marker_type	MARKER_TYPE
	marker_scale	FLOAT
	marker_color	COLOR_SPECIFIER
.ta
.DE
The attributes stored in a marker bundle table are defined and used in
the same fashion as the pipeline context attributes of the same name.


.Bl "TextBundle   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain attributes for drawing text and annotation text primitives.
Depending on the setting of the aspect source flag
attributes, text and annotation text attributes may be obtained from a
text bundle table.
Each entry in this type of table consists of the following:
.DS
.ta 1.0i 2.5i
	text_font_index	TABLE_INDEX
	text_precision	TEXT_PRECISION
	char_expansion	FLOAT
	char_spacing	FLOAT
	text_color	COLOR_SPECIFIER
.ta
.DE
The attributes stored in a text bundle table are defined and used in
the same fashion as the pipeline context attributes of the same name.


.Bl "InteriorBundle   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain attributes for drawing surface primitives.
Depending on the setting of the aspect source flag
attributes, surface attributes may be obtained from an interior bundle table.
Each entry in this type of table consists of the following:
.DS
.ta 1.0i 2.5i
	interior_style	INTERIOR_STYLE
	interior_style_index	TYPE_OR_TABLE_INDEX
	surface_color	COLOR_SPECIFIER
	reflection_attr	REFLECTION_ATTR
	reflection_model	REFLECTION_MODEL
	surface_interp	SURFACE_INTERP
	bf_interior_style	INTERIOR_STYLE
	bf_interior_style_index	TYPE_OR_TABLE_INDEX
	bf_surface_color	COLOR_SPECIFIER
	bf_reflection_attr	REFLECTION_ATTR
	bf_reflection_model	REFLECTION_MODEL
	bf_surface_interp	SURFACE_INTERP
	surface_approx	SURFACE_APPROX
.ta
.DE
The attributes stored in an interior bundle table are defined and used in
the same fashion as the pipeline context attributes of the same name.


.Bl "EdgeBundle   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain attributes for drawing edges of surface primitives.
Depending on the setting of the aspect source flag
attributes, surface edge attributes may be obtained from an edge bundle table.
Each entry in this type of table consists of the following:
.DS
.ta 1.0i 2.5i
	surface_edges	SWITCH
	surface_edge_type	SURFACE_EDGE_TYPE
	surface_edge_width	FLOAT
	surface_edge_color	COLOR_SPECIFIER
.ta
.DE
The attributes stored in an edge bundle table are defined and used in
the same fashion as the pipeline context attributes of the same name.


.Bl "Pattern   (1..65535, default entry = 1)"
This type of lookup table can be used to maintain patterns for use
when \fIinterior_style\fP is set to \fIPattern\fP.
.DS
.ta 1.0i 2.5i
	color_type	COLOR_TYPE
	numx	CARD16
	numy	CARD16
	colors	LISTofCOLOR
.ta
.DE
A pattern rectangle is comprised of \fInumx\fP \(mu \fInumy\fP cells.
\fIColor_type\fP indicates whether the color values are stored as
indexed or direct color values.  The colors are stored in the array
row-by-row.  The upper left hand cell in the pattern rectangle is the
first one in the list of colors, followed by the remaining cells in
the first row.  The color values for cells in the second row follow,
and so on.


.Bl "Color   (0..65534, default entry = 1)"
This type of lookup table can be used to resolve indirect
color references.  Consequently, all color values in this type
of table must be specified as direct colors.
.DS
.ta 1.0i 2.5i
	color_type	COLOR_TYPE
	color	DIRECT_COLOR
.ta
.DE
A \fIColorType\fP error is generated if an attempt is made to set an
entry with a \fIcolor_type\fP of \fIIndexed\fP.


.Bl "TextFont   (1..65535, default entry = 1)"
This type of lookup table is used to maintain a list of font
groups.  Each font group is a list of resource IDs for either PEX fonts
or X11 fonts.  Resource IDs for fonts can appear multiple times in
a font table, or even within a single table entry.
Only PEX fonts are guaranteed to fully realize all of the PEX text attributes.
Specifically, scaling and rotation operations on text strings are not
guaranteed to affect text primitives
if an X11 font is used, but they are guaranteed to work if a PEX font is used.
Font values are specified as indices when using
output commands.
A font index can be used with a table of this type
in order to obtain the actual font group to be used.
Switching between fonts in the font group is accomplished by means
of a switching mechanism embedded within strings that are to be rendered.
It is up to the client to ensure that all of the character sets that
are available in a table of this type are available in the default
table entry as well.
.DS
.ta 1.0i 2.5i
	font	LISTofFONT_ID
.ta
.DE
If a font is opened, bound to a font table, and then closed, the
contents of the font will remain, since
the contents are still being referenced by a font table.
However, when the font table is
queried, the value \fIAlreadyFreed\fP will
be returned for such a font, since it no longer
has a valid resource ID by which it can be referenced.


.Bl "View   (0..65534, default entry = 0)"
This type of lookup table is used to maintain viewing information.
"Views" are then specified as indices, which are used to look up the
appropriate information in a view lookup table.
.DS
.ta 1.0i 2.5i
	clip_flags	BITMASK_SHORT
	clip_limits	NPC_SUBVOLUME
	orientation	MATRIX
	mapping	MATRIX
.ta
.DE
\fIOrientation\fP is a matrix which maps geometry from the world coordinate
system to the viewing reference coordinate system.  (This is a right-handed 
coordinate system and is also known as the eye or viewing coordinate system.)
\fIMapping\fP is a matrix which maps geometry from the viewing reference 
coordinate system
to the normalized projection coordinate system (also a right-handed
coordinate system).  This transformation is typically a
parallel or perspective projection.
The \fIclip_limits\fP specify the minimum and maximum of a rectangular
volume in normalized projection coordinates and,
\fIclip_flags\fP contains three bits that indicate whether or not clipping
should be performed
against the sides, back, and front planes of the
volume specified by \fIclip_limits\fP.
The actual clipping volume for each view is a rectangular parallelpiped
that is computed by taking the intersection of the view
clipping volume (determined by \fIclip_limits\fP and \fIclip_flags\fP)
and the NPC subvolume.



.Bl "Light   (1..65535, default entry = 1)"
This type of lookup table is used
to maintain light source definitions for use in light source shading
computations.
.DS
.ta 1.0i 2.5i
	light_type	LIGHT_TYPE
	direction	VECTOR_3D
	point	COORD_3D
	concentration	FLOAT
	spread_angle	FLOAT
	attenuation	[factor1, factor2 : FLOAT]
	color 	COLOR_SPECIFIER
.ta
.DE
There are four types of light sources currently supported in PEX: 
\fIambient\fP, \fIWCS vector\fP, \fIWCS point\fP and \fIWCS spot\fP.
Depending on the type of light, some of the values in a table entry
may be ignored.

An ambient light source is defined by \fIcolor\fP.  It is a non-directional
light that affects the color of all surfaces regardless of their orientation.
Each ambient light causes a constant amount of intensity to be added to each
point in the scene.

A WCS vector light source (or directional light source) is defined by 
\fIdirection\fP and \fIcolor\fP.  A light source of this type is 
located at infinity so all light rays which emanate from it 
are parallel when they strike an objects surface.

A WCS point light source (or a positional light source) illuminates equally
in all directions and is specified by \fIpoint\fP, \fIattenuation\fP, and
\fIcolor\fP.  \fIpoint\fP indicates the world coordinate source of the light
and \fIattenuation\fP indicates how the intensity of the point light source
drops off as a function of distance.

A WCS spot light source simulates the behavior of a spotlight and is specified
by \fIdirection\fP, \fIpoint\fP, \fIconcentration\fP, \fIspread_angle\fP,
\fIattenuation\fP, and \fIcolor\fP.  \fIpoint\fP indicates the world 
coordinate source of the light and \fIdirection\fP and \fIspread_angle\fP
specify a cone of influence.  The direction vector is relative to the 
specified light source position and \fIspread_angle\fP is 
the half-angle of the cone.  The contribution of a spotlight is 
zero if the point in question lies outside of the cone of influence.

.Bl "DepthCue   (0..65534, default entry = 0)"
This type of lookup table is used
to maintain depth-cueing information.
.DS
.ta 1.0i 2.5i
	mode	SWITCH
	front_plane	FLOAT
	back_plane	FLOAT
	front_scaling	FLOAT
	back_scaling	FLOAT
	color	COLOR_SPECIFIER
.ta
.DE
The depth cueing computation modifies colors according to the distance from the
viewing position.  \fIcolor\fP indicates the color to which objects will tend
when going from the \fIfront_plane\fP to the \fIback_plane\fP.
\fIfront_scaling\fP and \fIback_scaling\fP
represent values by which the colors will be scaled at the front and back
depth cueing planes.  The depth cueing computation is performed 
in NPC space-- \fIfront_plane\fP is the distance to the front depth-cueing
plane in NPC coordinates and \fIback_plane\fP is the distance to the back
depth-cueing plane in NPC coordinates. 


.Bl "ColorApprox   (0..65534, default entry = 0)" 
This type of lookup table is used to define the way that a
renderer will transform rendering pipeline color values
into displayable pixel values.  Each entry in this type
of table contains the following data:
.DS
.ta 1.0i 2.5i
	type	COLOR_APPROX_TYPE
	color_model	COLOR_APPROX_MODEL
	max1	CARD16
	max2	CARD16
	max3	CARD16
	mult1	CARD32
	mult2	CARD32
	mult3	CARD32
	weight1	FLOAT
	weight2	FLOAT
	weight3	FLOAT
	base_pixel	CARD32
	dither	SWITCH
.ta
.DE
After a renderer has performed illumination, depth-cueing, and clipping
operations, it is left with a rendering pipeline color that must be
converted to a displayable pixel value.  The renderer's current
\fIcolor_approx_index\fP is used to determine which entry in a table
of this type is to be used to perform the conversion to a displayable
pixel value.  As the color value emerges from the rendering pipeline,
it is first converted to a color in the color space specified by
\fIcolor_model\fP.  (In the case where the rendering pipeline colors
are already in the specified color space, this is a null mapping.)

If \fItype\fP is \fIColorSpace\fP,
each component of the converted color
(\fIc1, c2, c3\fP) is scaled by the corresponding maximum value (\fImax1, max2,
max3\fP).  (\fImax1, max2, max3\fP can be used
by the client to indicate the number of entries in the colormap
for each color axis, minus 1.  For example, to compute a pixel value for
a 3-3-2 RGB colormap allocation, the max values would be 7, 7, and 3).
The resulting color components are packed into a single pixel value 
by multiplying the first
component by \fImult1\fP, the second by \fImult2\fP, the third by
\fImult3\fP, and then adding this to the \fIbase_pixel\fP.
When the \fItype\fP is \fIColorSpace\fP, \fIweight1\fP, \fIweight2\fP, and 
\fIweight3\fP values are not used.

For a more concrete example, assume that the \fIcolor_model\fP 
is set to \fIRGB\fP.  During the color approximation stage,
each color value that emerges from the rendering pipeline must first
be converted to an RGB triple.  If the rendering pipeline 
performs color computations in RGB space then this conversion
is a no-op.
Each of the three components are then mapped to an integer value
as follows:
.DS I
red = red intensity component mapped into the range [0, \fImax1\fP]
green = green intensity component mapped into the range [0, \fImax2\fP]
blue = blue intensity component mapped into the range [0, \fImax3\fP]
.DE
and a single pixel value is formed by computing:
.DS
pixel = \fImult1\fP \(mu red + \fImult2\fP \(mu green + \fImult3\fP \(mu blue + \fIbase_pixel\fP
.DE
If \fIcolor_type\fP is \fIColorRange\fP, the values are combined in
a different fashion during the color approximation stage.  Once again,
the rendering pipeline color is first converted into a color in the
color space specified by \fIcolor_model\fP.
First,
the color components (\fIc1, c2, c3\fP) that emerge from the rendering
pipeline are multiplied by their corresponding normalized
weight values (\fIweight1,
weight2, weight3\fP) and the terms are added together to form a single
value.  The weight values can be adjusted to allow equal weighting of the
components (weights are all equal) or to minimize or eliminate one
or more of the components (one or more weights equal to 0).  For instance,
the weight values of 0.30, 0.59, 0.11 can be used to convert an RGB value
to a single-valued intensity according to the NTSC color standard.

Next, the computed value is multiplied by \fImax1 \fP(\fImax2 \fPand
\fImax3 \fPare not used) which should be set by the client to represent
the number of color map entries in the range, minus 1.
For instance,
if a client desires to display its computed image on a pseudo-color
display using gray scale, it could allocate 100 contiguous color cells
in the color map, and set \fImax1 \fPto the value of 99, so that intensity
values would be mapped into the range [0,99].  
This value is then replicated for each of the three color components,
which are then multiplied by \fImult1\fP, \fImult2\fP, and \fImult3\fP
respectively.
The values are then added together with \fIbase_pixel\fP to form the
pixel value that is to be written.  It is up to the client to provide
reasonable multipliers for drawables that are not three-channel in
nature.  For instance, multipliers of (1.0, 0.0, 0.0) could be used
if the target drawable was known to be a window of visual type
\fIPseudoColor\fP.  PEX implementations are free to optimize the
case where one or more of the multipliers is zero.

The use of addition rather than logical OR for composing pixel values
permits allocations where the primary components are not allocated
into distinct bitplanes.  Since some hardware allows a performance improvement
if the multiplication values (\fImult1, mult2, mult3\fP) are powers of two, the
\fBPEXGetImpDepConstants\fP request can be used to obtain the
constant \fIBestColorApproxValues\fP in order to get an indication
as to whether powers of two are preferred.

\fIDither\fP is treated as a hint to the renderer as to whether
or not some attempt at dithering should be performed.  Whether
or not dithering is supported and the dithering algorithm that
is used are implementation-dependent (see \fBPEXGetImpDepConstants\fP).
.bp

.AC "Lookup Table Resource Management" 2
.LP                   
The lookup table is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
freeing, and copying of lookup table resources.

.AC "Create Lookup Table" 3
.Fs
.Na PEXCreateLookupTable
.Rq
.Pa drawable_example DRAWABLE_ID
.Pa lut_id LOOKUP_TABLE_ID
.Pa table_type TABLE_TYPE
.Se
IDChoice, Drawable, Match, Value, Alloc, LookupTable
.Ds                     
This request creates a lookup table resource for the specified
\fIlut_id\fP, for use with drawables with the same root window and depth
as the example drawable specified by \fIdrawable_example\fP. 
The \fItable_type\fP parameter indicates the type of lookup
table that is to be created.
Some entries of a lookup table may be defined at the time the resource
is created.  The number of predefined entries and their contents are
dependent on the type of table and are
implementation-dependent.
.Fe



.AC "Copy Lookup Table" 3
.Fs
.Na PEXCopyLookupTable
.Rq
.Pa src_lut_id LOOKUP_TABLE_ID
.Pa dest_lut_id LOOKUP_TABLE_ID
.Se
LookupTable, Match
.Ds            
This request copies the source lookup table
\fIsrc_lut_id\fP to a destination lookup table \fIdest_lut_id\fP,
after first deleting all the entries in the destination lookup table.
The \fIdest_lut_id\fP must already exist as a valid resource and
it must be of the same type as \fIsrc_lut_id\fP.
.Fe
.bp

.AC "Free Lookup Table" 3
.Fs
.Na PEXFreeLookupTable
.Rq
.Pa lut_id LOOKUP_TABLE_ID
.Se
LookupTable
.Ds            
This request deletes the association between the resource ID
and the lookup table.  The lookup table storage will be
freed when no other resource references it.
.Fe
.bp

.AC "Lookup Table Inquiry" 2
.LP
These requests inquire lookup table attributes.
.AC "Get Table Info" 3
.Fs
.Na PEXGetTableInfo
.Rq
.Pa drawable_example DRAWABLE_ID
.Pa table_type TABLE_TYPE
.Re
.Pa definable_entries CARD16
.Pa num_predefined CARD16
.Pa predefined_min TABLE_INDEX
.Pa predefined_max TABLE_INDEX
.Se
Drawable, Match, Value, LookupTable
.Ds       
This request will return information about lookup tables of the
specified \fItable_type\fP if they were to be used with drawables
of the same root and depth as \fIdrawable_example\fP.
\fIDefinable_entries\fP is the maximum number of entries that
can be defined in this type of table, and
includes the number of predefined entries.  Predefined entries
can be redefined by the client.  All the entries between
\fIpredefined_min\fP and \fIpredefined_max\fP are guaranteed to
be defined initially (i.e., predefined entries must have contiguous indices).
If \fInum_predefined\fP is zero, then the values for \fIpredefined_min\fP
and \fIpredefined_max\fP are meaningless.
.Fe
.bp

.AC "Get Predefined Entries" 3
.Fs
.Na PEXGetPredefinedEntries
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa drawable_example DRAWABLE_ID
.Pa table_type TABLE_TYPE
.Pa start TABLE_INDEX
.Pa count CARD16
.Re
.Pa entries LISTofTABLE_ENTRY
.Se
Drawable, Match, Value, FloatingPointFormat, LookupTable
.Ds       
This request will return the values for predefined table
entries of the specified \fItable_type\fP if they were to be
used with drawables
of the same root and depth as \fIdrawable_example\fP.
\fICount\fP table entries will be returned in \fIentries\fP, starting
with the table entry specified by \fIstart\fP.  The values in
\fIentries\fP will be in the format defined for \fItable_type\fP.
Floating point values in \fIentries\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe


.AC "Get Defined Indices" 3
.Fs
.Na PEXGetDefinedIndices
.Rq
.Pa lut_id LOOKUP_TABLE_ID
.Re
.Pa defined_indices LISTofTABLE_INDEX
.Se
LookupTable
.Ds       
This request will return in \fIdefined_indices\fP
a list of all the indices that are currently defined in the lookup
table resource specified by \fIlut_id\fP.  The entries returned
include those predefined by the server and those that have been
defined by clients.
.Fe
.bp


.AC "Get Table Entry" 3
.Fs
.Na PEXGetTableEntry
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa lut_id LOOKUP_TABLE_ID
.Pa index TABLE_INDEX
.Pa value_type "{\fISet, Realized\fP}"
.Re
.Pa status "{\fIDefined, Default\fP}"
.Pa table_type TABLE_TYPE
.Pa entry TABLE_ENTRY
.Se
LookupTable, FloatingPointFormat, Value
.Ds       
This request will return the type of lookup table and
the lookup table entry specified by \fIindex\fP.
The entry will be obtained from the lookup table specified
by \fIlut_id\fP and will be of the format indicated by \fIlut_id\fP's
table type.
If the specified entry in the lookup table
is not defined, the values for the default entry for that table type
will be returned
in \fIentry\fP and \fIstatus\fP will be set to \fIDefault\fP.
If the specified entry
is defined, its contents will be returned
in \fIentry\fP and \fIstatus\fP will be set to \fIDefined\fP.
If \fIvalue_type\fP is \fISet\fP, the values returned
will be those originally specified by the client.
If \fIvalue_type\fP is \fIRealized\fP, the values returned
will be the values that are actually used during rendering (i.e.,
the default values, which are used if the specified value is not
supported on the drawable).
Floating point values in \fIentry\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe


.AC "Get Table Entries" 3
.Fs
.Na PEXGetTableEntries
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa lut_id LOOKUP_TABLE_ID
.Pa start TABLE_INDEX
.Pa count CARD16
.Pa value_type "{\fISet, Realized\fP}"
.Re
.Pa table_type TABLE_TYPE
.Pa entries LISTofTABLE_ENTRY
.Se
LookupTable, Value, FloatingPointFormat
.Ds       
This request will return the type of table
and \fIcount\fP table entries from the lookup
table specified by \fIlut_id\fP, starting at the entry specified
by \fIstart\fP.  The default entry will be returned for any entry
in the requested range that is not defined.
If \fIvalue_type\fP is \fISet\fP, the values returned
will be those originally specified by the client.
If \fIvalue_type\fP is \fIRealized\fP, the values returned
will be the values that are actually used during rendering (i.e.,
the default values, which are used if the specified value is not
supported on the drawable).
Floating point values in \fIentries\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe
.bp

.AC "Lookup Table Modification" 2
.LP
This section contains requests that modify lookup table resources.


.AC "Set Table Entries" 3
.Fs
.Na PEXSetTableEntries
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa lut_id LOOKUP_TABLE_ID
.Pa start TABLE_INDEX
.Pa count CARD16
.Pa entries LISTofTABLE_ENTRY
.Se
LookupTable, Value, FloatingPointFormat, ColorType, Alloc
.Ds       
This request will set \fIcount\fP lookup table entries in
the lookup table resource specified by \fIlut_id\fP, starting
at the entry indicated by \fIstart\fP.
The values to use when setting the entries in the lookup table
are provided in \fIentries\fP, and are in the format specified
for a lookup table of \fIlut_id\fP's type.
Floating point values in \fIentries\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe




.AC "Delete Table Entries" 3
.Fs
.Na PEXDeleteTableEntries
.Rq
.Pa lut_id LOOKUP_TABLE_ID
.Pa start TABLE_INDEX
.Pa count CARD16
.Se
LookupTable, Value
.Ds       
This request will delete the defined table entries in the lookup table
resource specified by \fIlut_id\fP, between and including the entry
specified by \fIstart\fP and the entry specified by
\fIstart\fP+\fIcount\fP-1.
.Fe
.bp


.AC "Pipeline Contexts" 1
.LP
.RU
.LP
A \fIpipeline context\fP is a PEX resource that contains an instance of the
attributes that describe a rendering pipeline.  The attributes
in a pipeline context are copied to a renderer resource whenever
a \fBPEXBeginRendering\fP request is executed.
This section
describes pipeline context attributes and the operations that can be performed
on pipeline context resources.
.LP
Some of the requests in this section affect attributes of a pipeline
context.
The \fIitem_mask\fP and \fIitem_list\fP parameters
specify which components are to be affected.
Each bit in the \fIitem_mask\fP indicates whether or not
the corresponding attribute is affected.  In the cases where
pipeline context attributes are being set or queried,
there is a corresponding entry in the \fIitem_list\fP
for each set bit in \fIitem_mask\fP.
It is therefore possible
to affect one or many pipeline context attributes with a single request.
.LP
A name set resource ID is one of the attributes of a pipeline context.
If a name set is created, bound to a pipeline context, and then freed, the
contents of the name set will remain, since
the contents are still being referenced by the pipeline context.
However, when a
pipeline context's attributes are queried, the value \fIAlreadyFreed\fP will
be returned for the name set ID, since it no longer
has a valid resource ID by which it can be referenced.
.LP
The pipeline context components are listed in the following table.
.LP
.ID
.ta 0.4i 2.4i 3.9i
	\fBAttribute Name	Data Type	Default Value\fP
.ta
.ta 0.5i 2.2i 4.0i
    \fIMarker attributes\fP
	marker_type	MARKER_TYPE	\fIMarkerAsterisk\fP
	marker_scale	FLOAT	1.0
	marker_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	marker_bundle_index	TABLE_INDEX	1
    \fIText attributes\fP
	text_font_index	TABLE_INDEX	1
	text_precision	TEXT_PRECISION	\fIString\fP
	char_expansion	FLOAT	1.0
	char_spacing	FLOAT	0.0
	text_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	char_height	FLOAT	0.01
	char_up_vector	VECTOR_2D	<0.0, 1.0>
	text_path	TEXT_PATH	\fIPathRight\fP
	text_alignment	TEXT_ALIGNMENT	{\fIHalignNormal, ValignNormal\fP}
	atext_height	FLOAT	0.01
	atext_up_vector	VECTOR_2D	<0.0, 1.0>
	atext_path	TEXT_PATH	\fIPathRight\fP
	atext_alignment	TEXT_ALIGNMENT	{\fIHalignNormal, ValignNormal\fP}
	atext_style	ATEXT_STYLE	\fIATextNotConnected\fP
	text_bundle_index	TABLE_INDEX	1
    \fILine and curve attributes\fP
	line_type	LINE_TYPE	\fILineTypeSolid\fP
	line_width	FLOAT	1.0
	line_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	curve_approx	CURVE_APPROX	{1, 1.0}
	polyline_interp	POLYLINE_INTERP	\fIPolylineInterpNone\fP
	line_bundle_index	TABLE_INDEX	1
    \fISurface attributes\fP
	interior_style	INTERIOR_STYLE	\fIInteriorStyleHollow\fP
	interior_style_index	TYPE_OR_TABLE_INDEX	1
	surface_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	reflection_attr	REFLECTION_ATTR	{1.0, 1.0, 1.0, 0.0, 0.0, {\fIIndexed\fP, 1}}
	reflection_model	REFLECTION_MODEL	\fIReflectionNoShading\fP
	surface_interp	SURFACE_INTERP	\fISurfaceInterpNone\fP
	bf_interior_style	INTERIOR_STYLE	\fIInteriorStyleHollow\fP
	bf_interior_style_index	TYPE_OR_TABLE_INDEX	1
	bf_surface_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	bf_reflection_attr	REFLECTION_ATTR	{1.0, 1.0, 1.0, 0.0, 0.0, {\fIIndexed\fP, 1}}
	bf_reflection_model	REFLECTION_MODEL	\fIReflectionNoShading\fP
	bf_surface_interp	SURFACE_INTERP	\fISurfaceInterpNone\fP
	surface_approx	SURFACE_APPROX	{1, 1.0, 1.0}
	culling_mode	CULL_MODE	\fINone\fP
	distinguish	BOOLEAN	\fIFalse\fP
	pattern_size	VECTOR_2D	{1.0, 1.0}
	pattern_ref_pt	COORD_3D	{0.0, 0.0, 0.0}
	pattern_ref_vec1	VECTOR_3D	<1.0, 0.0, 0.0>
	pattern_ref_vec2	VECTOR_3D	<0.0, 1.0, 0.0>
	interior_bundle_index	TABLE_INDEX	1
    \fISurface edge attributes\fP
	surface_edges	SWITCH	\fIOff\fP
	surface_edge_type	SURFACE_EDGE_TYPE	\fISurfaceEdgeSolid\fP
	surface_edge_width	FLOAT	1.0
	surface_edge_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	edge_bundle_index	TABLE_INDEX	1
    \fIGeometry transformation attributes\fP
	local_transform	MATRIX	identity
	global_transform	MATRIX	identity
	model_clip	CLIP_INDICATOR	\fINoClip\fP
	model_clip_volume	LISTofHALF_SPACE	\fINull\fP
	view_index	TABLE_INDEX	0
    \fIColor transformation attributes\fP
	light_state	LISTofTABLE_INDEX	\fINull\fP
	depth_cue_index	TABLE_INDEX	0
	color_approx_index	TABLE_INDEX	0
	rdr_color_model	COLOR_MODEL	0
	psurf_char	PSURF_CHAR	{1, \fINULL\fP}
    \fIASF attributes\fP
	asfs	ASF_SPECIFIER	{0, all \fIIndividual\fP}
    \fIMiscellaneous attributes\fP
	pick_id	CARD32	0
	HLHSR_identifier	CARD32	0
	name_set \(dg	NAME_SET_ID	\fINone\fP
.FS
\(dg When pipeline context attributes are copied to a renderer (e.g., whenever
a \fBPEXBeginRendering\fP request occurs),
the actual \fIcontents\fP of the name set resource is copied, and
not the resource ID of the name set.
.FE

The bits in the \fIasfs\fP field of an ASF_SPECIFIER are defined as:

	marker_type_asf	ASF_VALUE	\fIIndividual\fP
	marker_scale_asf	ASF_VALUE	\fIIndividual\fP
	marker_color_asf	ASF_VALUE	\fIIndividual\fP
	text_font_index_asf	ASF_VALUE	\fIIndividual\fP
	text_precision_asf	ASF_VALUE	\fIIndividual\fP
	char_expansion_asf	ASF_VALUE	\fIIndividual\fP
	char_spacing_asf	ASF_VALUE	\fIIndividual\fP
	text_color_asf	ASF_VALUE	\fIIndividual\fP
	line_type_asf	ASF_VALUE	\fIIndividual\fP
	line_width_asf	ASF_VALUE	\fIIndividual\fP
	line_color_asf	ASF_VALUE	\fIIndividual\fP
	curve_approx_asf	ASF_VALUE	\fIIndividual\fP
	polyline_interp_asf	ASF_VALUE	\fIIndividual\fP
	interior_style_asf	ASF_VALUE	\fIIndividual\fP
	interior_style_index_asf	ASF_VALUE	\fIIndividual\fP
	surface_color_asf	ASF_VALUE	\fIIndividual\fP
	reflection_model_asf	ASF_VALUE	\fIIndividual\fP
	surface_interp_asf	ASF_VALUE	\fIIndividual\fP
	reflection_attr_asf	ASF_VALUE	\fIIndividual\fP
	bf_interior_style_asf	ASF_VALUE	\fIIndividual\fP
	bf_interior_style_index_asf	ASF_VALUE	\fIIndividual\fP
	bf_surface_color_asf	ASF_VALUE	\fIIndividual\fP
	bf_reflection_model_asf	ASF_VALUE	\fIIndividual\fP
	bf_surface_interp_asf	ASF_VALUE	\fIIndividual\fP
	bf_reflection_attr_asf	ASF_VALUE	\fIIndividual\fP
	surface_approx_asf	ASF_VALUE	\fIIndividual\fP
	surface_edges_asf	ASF_VALUE	\fIIndividual\fP
	surface_edge_type_asf	ASF_VALUE	\fIIndividual\fP
	surface_edge_width_asf	ASF_VALUE	\fIIndividual\fP
	surface_edge_color_asf	ASF_VALUE	\fIIndividual\fP
.ta

.DE

.LP
The attributes of the pipeline context resource are defined as follows:

.Bl "marker_type"
This attribute contains the marker type to use when drawing marker primitives.
See the "Extension Information" section for descriptions of the registered
marker types.

.Bl "marker_scale"
This attribute contains the marker scale factor
to use when drawing marker primitives.

.Bl "marker_color"
This attribute contains the color value to use when drawing marker primitives.

.Bl "marker_bundle_index"
This attribute contains the lookup table index to be used to obtain
bundled marker attributes from the marker bundle table.

.Bl "text_font_index"
This attribute contains the lookup table index to be used to
obtain the font ID for drawing text
and annotation text primitives.

.Bl "text_precision"
This attribute contains the text precision to use when drawing text
and annotation text primitives.

.Bl "char_expansion"
This attribute contains the character expansion
to use when drawing text primitives.
The character 
expansion factor is the deviation of the width to height
ratio of the characters from the ratio indicated by the 
font designer.

.Bl "char_spacing"
This attribute contains the character spacing
to use when drawing text primitives.
Character spacing
specifies how much additional space is to be inserted between
two adjacent character bodies and is specified as a fraction of the
font-nominal character height.

.Bl "text_color"
This attribute contains the color value to use when drawing text
and annotation text primitives.

.Bl "char_height"
This attribute contains the character height
to use when drawing text primitives.
The character height
is specified in modeling coordinates.

.Bl "char_up_vector"
This attribute contains the character up vector
to use when drawing text primitives.
The up vector
is specified in the text local coordinate system.  The axes of this
coordinate system are determined by the direction vectors specified
with the text primitive.

.Bl "text_path "
This attribute contains the text path to use when drawing text primitives
(i.e., the writing path of the text string).

.Bl "text_alignment"
This attribute contains the horizontal and vertical alignment
to use when drawing text primitives.

.Bl "atext_height"
This attribute contains the character height
to use when drawing annotation text primitives.
The character height
is specified in normalized projection coordinates.

.Bl "atext_up_vector"
This attribute contains the character up vector
to use when drawing annotation text primitives.
The up vector is specified in the annotation text local coordinate system,
which is parallel to the display surface.

.Bl "atext_path"
This attribute contains the text path
to use when drawing annotation text primitives
(i.e. the writing path of the annotation text string).

.Bl "atext_alignment"
This attribute contains the horizontal and vertical alignment
to use when drawing annotation text primitives.

.Bl "atext_style"
This attribute contains the annotation text style
to use when drawing annotation text primitives.
See the "Extension Information" section for descriptions of the registered
annotation text styles.

.Bl "text_bundle_index"
This attribute contains the lookup table index to be used to obtain
bundled text attributes from the text bundle table.

.Bl "line_type"
This attribute contains the type to use when drawing polyline
and curve primitives.
See the "Extension Information" section for descriptions of the registered
line types.

.Bl "line_width"
This attribute contains the width to use when drawing polyline or
curve primitives.
This is the scale factor applied to the width of the polyline or curve
primitive when it
is to be rendered.  Line width is applied in 2D raster
coordinates after the primitive has been transformed from 3D
space to 2D raster space.

.Bl "line_color"
This attribute contains the color value to use when drawing polyline
and curve primitives.

.Bl "curve_approx"
This attribute contains the curve approximation
to use when drawing curve primitives.  It sets the curve approximation
method and
the tolerance value for drawing curves.
See the "Extension Information" section for descriptions of the registered
curve approximation methods and how the curve tolerance is used with each type.

.Bl "polyline_interp"
This attribute contains the polyline interpolation method
to use when drawing polyline primitives.
See the "Extension Information" section for descriptions of the registered
polyline interpolation methods.

.Bl "line_bundle_index"
This attribute contains the lookup table index to be used to obtain
bundled polyline and curve attributes from the line bundle table.

.Bl "interior_style"
This attribute contains the interior style
to use when drawing surface primitives.
See the "Extension Information" section for descriptions of the registered
interior styles.

.Bl "interior_style_index"
This attribute contains the index to use
if the interior style is of
type \fIPattern\fP or \fIHatch\fP.
The interior style index contains the table index of
the pattern table entry to be used
when the interior
style is \fIPattern\fP,
and the hatch table enumerated type index to be used
when the interior
style is \fIHatch\fP.

.Bl "surface_color"
This attribute contains the color value to use when drawing surface primitives.

.Bl "reflection_attr"
This attribute contains the ambient coefficient; the diffuse coefficient;
the specular coefficient, concentration, and color; and the transmission
coefficient that are to be used in
the light source shading computations when rendering surfaces
(area-defining primitives).
The specular color attribute provides an additional coefficient per primary
for use in the specular reflection computation.  This allows highlights
to be computed that are some color other than that of the light source.
The specular concentration
defines the sharpness
of the specular highlights or the "shininess" of a surface.  This
value is typically used as the exponent in the specular reflection
term of lighting equations and it ranges from
zero to the maximum floating point value.
If \fIspecular_conc\fP is zero, specular
highlights are very broad.  If \fIspecular_conc\fP is much greater than zero,
the highlights are very small and sharp, as if the surface
was very shiny.
The transmission coefficient indicates the amount of light that passes
through a surface.
A transmission coefficient of zero indicates that
the surface is opaque (lets no light through).
A transmission coefficient of 1.0 indicates that
the surface is totally invisible (lets all light through).

.Bl "reflection_model"
This attribute contains the reflection model
to use when drawing surface primitives.
See the "Extension Information" section for descriptions of the registered
reflection models.

.Bl "surface_interp"
This attribute contains the surface interpolation method
to use when drawing surface primitives.
See the "Extension Information" section for descriptions of the registered
surface interpolation methods.

.Bl "bf_interior_style"
This attribute contains the interior style
to use when drawing backfacing surface primitives.
See the "Extension Information" section for descriptions of the registered
interior styles.

.Bl "bf_interior_style_index"
This attribute contains the index to use for backfacing surface primitives
if the interior style is of
type \fIPattern\fP or \fIHatch\fP.
The interior style index contains the table index of
the pattern table entry to be used
when the interior
style is \fIPattern\fP,
and the hatch table enumerated type index to be used
when the interior
style is \fIHatch\fP.

.Bl "bf_surface_color"
This attribute contains the color value to use when rendering
backfacing surface primitives.

.Bl "bf_reflection_attr"
This attribute contains the reflection values to be used when rendering
backfacing surfaces.

.Bl "bf_reflection_model"
This attribute contains the reflection model
to use when drawing backfacing surface primitives.
See the "Extension Information" section for descriptions of the registered
reflection models.

.Bl "bf_surface_interp"
This attribute contains the surface interpolation method
to use when drawing backfacing surface primitives.
See the "Extension Information" section for descriptions of the registered
surface interpolation methods.

.Bl "surface_approx"
This attribute contains the surface approximation and the tolerance
to use when drawing surface primitives.
See the "Extension Information" section for descriptions of the registered
surface approximation methods and of how the surface tolerance is used with
each of them.

.Bl "culling_mode"
This attribute contains the culling mode that is used in processing
backfacing surfaces.
If the culling mode
is \fIBackFaces\fP, all back-facing surfaces will be culled and
only front-facing surfaces will be rendered.
If the culling mode
is \fIFrontFaces\fP, all front-facing surfaces will be culled and
only back-facing surfaces will be rendered.
If the culling mode is
\fINone\fP, both front- and back-facing polygons will be rendered.

.Bl "distinguish"
This attribute contains the distinguish mode that is used in processing
backfacing surfaces.
This flag
selects whether back-facing surfaces are rendered with the back-face
surface attributes or the front-face surface attributes.
If distinguish is \fITrue\fP,
then back-face attributes are used to render the surface.
If \fIdistinguish\fP is \fIFalse\fP,
then front-face attributes are used to render the surface.

.Bl "pattern_size"
This attribute contains the pattern size
to use when drawing surface primitives. 
It is specified in modeling coordinates.
The value <\fIx\fP,0> will be used as the pattern width 
vector and
the value <0,\fIy\fP> will be used as the pattern height
vector.
If the interior style is \fIPattern\fP, the renderer
attempts to use 
these values, plus the pattern reference point and the pattern reference 
vectors, to position, scale, and rotate the pattern on the surface. 

.Bl "pattern_ref_pt"
This attribute contains the pattern reference point
to use when drawing surface primitives.
It is specified in modeling coordinates.
When the interior style
is \fIPattern\fP, the renderer attempts
to use the pattern reference point, reference vectors, and
the pattern size to position and scale the pattern on the surface. 

.Bl "pattern_ref_vec1"
.IP
This attribute contains the first of two pattern reference vectors
to be used when interior style is \fIPattern\fP.
It is specified in modeling coordinates.
When the interior style is \fIPattern\fP,
the renderer attempts to use the two pattern reference vectors,
the pattern reference point, and
the pattern size to position, scale, and rotate the pattern on the 
surface. 

.Bl "pattern_ref_vec2"
.IP
This attribute contains the second of two pattern reference vectors
to be used when interior style is \fIPattern\fP.
It is specified in modeling coordinates.
When the interior style is \fIPattern\fP,
the renderer attempts to use the two pattern reference vectors,
the pattern reference point, and
the pattern size to position, scale, and rotate the pattern on the 
surface. 

.Bl "interior_bundle_index"
This attribute contains the lookup table index to be used to obtain
bundled interior attributes from the interior bundle table.

.Bl "surface_edges"
This attribute contains the surface edge flag attribute, which
is used to enable or disable surface edge drawing.  
If \fIsurface_edges\fP is
\fIOff\fP, surface edge drawing is disabled.  If \fIsurface_edges\fP 
is \fIOn\fP, surface edge
drawing is enabled.  Surface edges are drawn using the surface
edge color, surface edge type, and surface edge width.

.Bl "surface_edge_type"
This attribute contains the edge type
to use when drawing surface edges.
See the "Extension Information" section for descriptions of the registered
surface edge types.

.Bl "surface_edge_width"
This attribute contains the edge width
to use when drawing surface edges.
This is the scale factor applied to the width of a surface edge when a
surface edge is to be rendered.  It is applied in 2D raster
coordinates after a surface edge primitive has been transformed from 3D
space to 2D raster space.

.Bl "surface_edge_color"
This attribute contains the color value to use when drawing surface edges.

.Bl "edge_bundle_index"
This attribute contains the lookup table index to be used to obtain
bundled surface edge attributes from the surface edge bundle table.

.Bl "local_transform"
This attribute contains the local modeling transformation matrix that is used
when drawing output primitives.

.Bl "global_transform"
This attribute contains the global modeling transformation matrix that is used
when drawing output primitives.

.Bl "model_clip"
This attribute contains the model clipping flag that indicates whether
or not to perform modeling clipping
when drawing output primitives.

.Bl "model_clip_volume"
This attribute contains the model clipping volume that is used
whenever modeling clipping is enabled.

.Bl "view_index"
This attribute contains the lookup table index to be used to obtain
viewing attributes from the view table.

.Bl "light_state"
This attribute contains a list of table indices that specify those
light sources that are enabled ("turned on").  Any light whose table
index is not in this list is considered disabled ("turned off").

.Bl "depth_cue_index"
This attribute contains the lookup table index to be used to obtain
bundled depth-cue attributes from the depth-cue bundle table.

.Bl "color_approx_index"
This attribute contains the lookup table index to be used to obtain
the color approximation parameters from the color approximation table.

.Bl "rdr_color_model"
This attribute contains the rendering color model that is to be used
during the interpolation of shaded primitives.
See the "Extension Information" section for descriptions of the registered
rendering color models.

.Bl "psurf_char"
This attribute contains the parametric surface characteristics that
are to be used when rendering surfaces.  The \fIpsc_type\fP field
specifies the parametric surface characteristics type to be used
when rendering surfaces.  The \fIpsc_data\fP field supplies additional
data that may be used.
See the "Extension Information" section for descriptions of the registered
parametric surface characteristics types.

.Bl "asfs        "
This attribute contains an aspect source flag (asf) for each attribute
that can be obtained from a bundle lookup table.
When rendering, if the value for an asf
is set to \fIIndividual\fP, the value for the corresponding
attribute will be obtained directly from the current value within
the renderer.
If the value for the asf is set to \fIBundled\fP, the value for the
attribute will be obtained from the bundle lookup table.
When setting asfs, a separate mask (\fIenables\fP) is used to
indicate which asfs are actually to be modified.  The value to which an
asf is to be modified will then be taken from the corresponding
bit in the \fIasfs\fP bitmask.  The \fIenables\fP field of the
ASF_SPECIFIER is meaningful only when creating or changing attributes
of a pipeline context.  During copying, all asf values are copied,
and when queried, all asf values are returned and a value with all
defined asfs set
is returned for the \fIenables\fP bitmask.  (This implies that it is
not necessary to consider the \fIenables\fP field to be part of the
state that is saved and restored while rendering.)

.Bl "pick_id     "
This attribute contains the pick ID, which
is used in conjunction with picking operations.

.Bl "HLHSR_identifier"
This attribute contains an HLHSR identifier.  It is a noop for all
registered HLHSR modes except \fIZBufferId\fP.
When the renderer's \fIHLHSR_mode\fP is set to \fIZBufferId\fP then a
zero value will disable z-buffering and a value of one will enable
z-buffering.  For non-registered HLHSR modes the meaning is
implementation dependent.  Conceptually, this attribute is bound to
all output primitives as they enter the rendering pipeline.

.Bl "name_set"
This attribute contains a reference to a name set resource.
When attributes of a pipeline context are copied to a renderer,
the \fIcontents\fP of this name set are copied to the renderer.

.bp

.AC "Pipeline Context Resource Management" 2
.LP                   
The pipeline context is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
freeing, and copying of pipeline contexts.

.AC "Create Pipeline Context" 3
.Fs
.Na PEXCreatePipelineContext
.Rq      
.Pa fp_format FLOAT_FORMAT
.Pa pc_id PIPELINE_CONTEXT_ID
.Pa item_mask PC_BITMASK
.Pa item_list LISTofVALUE
.Se
IDChoice, Value, FloatingPointFormat, ColorType, Alloc, NameSet
.Ds                     
This request creates a pipeline context resource for the specified
\fIpc_id\fP.  
The \fIitem_mask\fP defines those pipeline context attributes
that are to be explicitly set at the time the resource is created.
The \fIitem_list\fP 
contains the corresponding
list of values used to modify the newly-created pipeline context.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
Similarly, color values in \fIitem_list\fP will be
the color type specified by \fIcolor_type\fP.
.Fe



.AC "Copy Pipeline Context" 3
.Fs
.Na PEXCopyPipelineContext
.Rq                 
.Pa src_pc_id PIPELINE_CONTEXT_ID
.Pa dest_pc_id PIPELINE_CONTEXT_ID
.Pa item_mask PC_BITMASK
.Se
PipelineContext, Value
.Ds            
This request copies the source pipeline context
\fIsrc_pc_id\fP to a destination pipeline context \fIdest_pc_id\fP.
The \fIdest_pc_id\fP must already exist as a valid resource.
The \fIitem_mask\fP     
indicates which values
in the pipeline context will be copied.
.Fe
.bp


.AC "Free Pipeline Context" 3
.Fs
.Na PEXFreePipelineContext
.Rq                 
.Pa pc_id PIPELINE_CONTEXT_ID
.Se
PipelineContext
.Ds            
This request deletes the association between the resource ID
and the pipeline context.  The pipeline context storage will be
freed when no other resource references it.
.Fe
.bp


.AC "Pipeline Context Inquiry" 2
.LP
This section defines the requests that can be used
to inquire pipeline context attributes.
.AC "Get Pipeline Context" 3
.Fs
.Na PEXGetPipelineContext
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pc_id PIPELINE_CONTEXT_ID
.Pa item_mask PC_BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
PipelineContext, FloatingPointFormat, Value
.Ds       
This request will return components of
the pipeline context specified by \fIpc_id\fP.
The \fIitem_mask\fP specifies which components are to be inquired and returned.
The specified attributes of the pipeline context will be returned in
\fIitem_list\fP.
Floating point values in \fIitem_list\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe


.AC "Pipeline Context Modification" 2
.LP
This section defines the requests that can be used
to modify attributes of pipeline context resources.
.AC "Change Pipeline Context" 3
.Fs
.Na PEXChangePipelineContext
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pc_id PIPELINE_CONTEXT_ID
.Pa item_mask PC_BITMASK
.Pa item_list LISTofVALUE
.Se
PipelineContext, Value, FloatingPointFormat, ColorType, NameSet
.Ds       
This request will modify components of
the pipeline context specified by \fIpc_id\fP.
The \fIitem_mask\fP specifies which components are to be modified.
The values for the attributes that are to be modified are contained in
\fIitem_list\fP.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
Similarly, color values in \fIitem_list\fP will be
the color type specified by \fIcolor_type\fP.
.Fe
.bp

.AC "Renderers" 1
.LP
.RU
.LP
A \fIrenderer\fP is a PEX resource that can be created for
the purpose of doing 3D rendering.  A renderer consists of resource IDs
for various tables and name sets,
the resource ID of a pipeline context from
which the initial
rendering pipeline attributes will be copied,
and other attributes.
A renderer also manages the HLHSR
buffer needed for some hidden line/hidden surface algorithms.
A renderer is made ready for rendering by
the \fBPEXBeginRendering\fP request (a \fBPEXBeginRendering\fP
is also performed implicitly
as part of the \fBPEXRenderNetwork\fP request).
A PEX implementation may choose to allow
certain renderer attributes to be bound only at the time of a
\fBPEXBeginRendering\fP request or at anytime during the lifetime
of the renderer.  To obtain information about how renderer attributes
are bound for a particular PEX implementation, the
\fBPEXGetRendererDynamics\fP request should be used.
.LP
Some renderer resource requests require an
\fIitem_mask\fP parameter.
Each bit in the \fIitem_mask\fP indicates whether or not
the corresponding attribute is to be set/queried.
There is a corresponding entry in the \fIitem_list\fP
for each set bit in \fIitem_mask\fP.
It is therefore possible
to set/query one or many renderer attributes with a single request.
.LP
Any of a renderer's attributes that are resource IDs that are not
explicitly specified will be set to the
default value of \fINull\fP when the renderer is created.
During rendering operations,
if a renderer makes an attempt to access an attribute from a lookup table
whose resource ID is \fINull\fP, the default attributes
for the lookup table
(as listed in Appendix D)
will be used.
If a renderer makes an attempt to read from a name set whose resource ID
is \fINull\fP, the result will be as if the name set was empty.
If the renderer's pipeline context attribute contains the resource ID
\fINull\fP, the default values for pipeline context attributes will be copied
into the renderer whenever a \fBPEXBeginRendering\fP request is performed.
.LP
Pipeline context, lookup table, and
name set resources can be bound to a renderer
and then freed.  When this happens,
the contents of these resources will remain, since
they are still being referenced by the renderer.  However, when that
renderer's attributes are queried, the value \fIAlreadyFreed\fP will
be returned for those resources that have been freed and thus no longer
have a valid resource ID by which they can be referenced.
.LP
If a window that is associated with a renderer is destroyed or
resized while
the renderer is in the \fIRendering\fP state, an implicit
\fBPEXEndRendering\fP is performed by the server
with \fIflush\fP equal to \fIFalse\fP
in order to return the renderer to the \fIIdle\fP state.
All subsequent output and traversal commands
(e.g., \fBPEXRenderOutputCommands\fP, \fBPEXBeginStructure\fP,
\fBPEXEndStructure\fP, \fBPEXRenderElements\fP, and \fBPEXAccumulateState\fP)
are ignored, up to and including the final \fBPEXEndRendering\fP.
No errors are generated; however, at a subsequent \fBPEXBeginRendering\fP,
an \fIAlloc\fP error may be generated if an attempt to reallocate the
z-buffer fails.
.LP
If a window that is associated with a renderer is moved, exposed, 
or occluded while the
renderer is in the \fIRendering\fP state, the server must continue to
process output commands using the newly-modified window hierarchy
until the next explicit or implicit \fBPEXEndRendering\fP occurs.
.LP
Whenever an implicit or explicit \fBPEXBeginRendering\fP occurs
and the \fIHLHSR_mode\fP indicates
z-buffering should be performed, a z-buffer suitable for
use with the specified drawable 
will be allocated, if necessary, and bound to the renderer.
(A new z-buffer is only allocated
if a z-buffer does not exist for the renderer or, if the z-buffer
which is bound to the renderer is not suitable for the current
drawable.)
If the z-buffer could not be allocated,
an \fIAlloc\fP error is generated and the rendering is aborted.
If \fIclear_Z\fP is enabled or, if a new z-buffer was allocated,
then the z-buffer will be cleared to infinity
in the region specified by the renderer's current clip list.
The value of infinity is implementation-dependent.
.LP
During rendering, primitives will be clipped by the renderer's
clip list.  If z-buffering is enabled, primitives closer to the
eye will be drawn over primitives that are further away.
When primitives have the same z values, it is implementation-dependent
which primitive will get drawn.
.LP
The z-buffer will not be deallocated when an explicit or implicit
\fBPEXEndRendering\fP occurs.
It remains bound to the renderer
until the renderer is freed.  When the next \fBPEXBeginRendering\fP occurs,
an attempt will be made to reuse the z-buffer.  If it cannot be reused
(for instance, if the previous and current drawable are a different size),
it will be deallocated and a z-buffer suitable for the new
drawable will be allocated.
.LP
The renderer components are listed in the following table.
The abbreviation "imp. dep." means that the default value is
implementation-dependent.
.LP
.ID
.ta 0.5i 2.4i 3.8i
	\fBAttribute Name	Data Type	Default Value\fP
.ta
.ta 0.5i 2.2i 4.0i
	pipeline_context	PIPELINE_CONTEXT_ID	\fINull\fP
	current_path	LISTofELEMENT_REF	\fINull\fP
	renderer_state	RENDERER_STATE	\fIIdle\fP
	marker_bundle	LOOKUP_TABLE_ID	\fINull\fP
	text_bundle	LOOKUP_TABLE_ID	\fINull\fP
	line_bundle	LOOKUP_TABLE_ID	\fINull\fP
	interior_bundle	LOOKUP_TABLE_ID	\fINull\fP
	edge_bundle	LOOKUP_TABLE_ID	\fINull\fP
	view_table	LOOKUP_TABLE_ID	\fINull\fP
	color_table	LOOKUP_TABLE_ID	\fINull\fP
	depth_cue_table	LOOKUP_TABLE_ID	\fINull\fP
	light_table	LOOKUP_TABLE_ID	\fINull\fP
	color_approx_table	LOOKUP_TABLE_ID	\fINull\fP
	pattern_table	LOOKUP_TABLE_ID	\fINull\fP
	text_font_table	LOOKUP_TABLE_ID	\fINull\fP
	highlight_inclusion	NAME_SET_ID	\fINull\fP
	highlight_exclusion	NAME_SET_ID	\fINull\fP
	invisibility_inclusion	NAME_SET_ID	\fINull\fP
	invisibility_exclusion	NAME_SET_ID	\fINull\fP
	pick_inclusion	NAME_SET_ID	\fINull\fP
	pick_exclusion	NAME_SET_ID	\fINull\fP
	pick_start_path	LISTofELEMENT_REF	\fINull\fP
	background_color	COLOR_SPECIFIER	{Indexed, 0}
	clear_I	BOOLEAN	\fIFalse\fP
	clear_Z	BOOLEAN	\fITrue\fP
	echo_mode	ECHO_MODE	\fINoEcho\fP
	HLHSR_mode	HLHSR_MODE	1
	NPC_subvolume	NPC_SUBVOLUME	{(0.0,0.0,0.0),(1.0,1.0,1.0)}
	viewport	VIEWPORT	{imp. dep., imp. dep., True}
	clip_list	LISTofDEVICE_RECT	\fINull\fP
.ta
.DE
.LP
The attributes of the renderer resource are defined as follows:

.Bl "pipeline_context"
This attribute specifies the resource ID of the pipeline context
from which initial rendering pipeline attribute values will be copied whenever
an explicit or implicit \fBPEXBeginRendering\fP request is executed.

.Bl "current_path"
This attribute contains a list of element references for
keeping track of paths for client-side traversal.

.Bl "renderer_state"
This attribute contains the current state of the renderer.  The
\fIrenderer_state\fP is set to \fIRendering\fP whenever an explicit or implicit
\fBPEXBeginRendering\fP request is processed, to \fIPicking\fP
whenever an explicit or implicit \fBPEXBeginPickOne\fP or
\fBPEXBeginPickAll\fP request is processed, and to \fIIdle\fP whenever a
explicit or implicit \fBPEXEndRendering\fP, \fBPEXEndPickOne\fP or
\fBPEXEndPickAll\fP request is processed.

.Bl "marker_bundle"
This attribute contains the resource ID of the marker bundle lookup table
to be used when rendering.

.Bl "text_bundle"
This attribute contains the resource ID of the text bundle lookup table to
be used when rendering.

.Bl "line_bundle	"
This attribute contains the resource ID of the line bundle lookup table to
be used when rendering.

.Bl "interior_bundle"
This attribute contains the resource ID of the interior bundle lookup table to
be used when rendering.

.Bl "edge_bundle"
This attribute contains the resource ID of the edge bundle lookup table to
be used when rendering.

.Bl "view_table"
This attribute contains the resource ID of the view lookup table to
be used when rendering.

.Bl "color_table"
This attribute contains the resource ID of the color lookup table to
be used to resolve references to indexed colors.

.Bl "depth_cue_table"
This attribute contains the resource ID of the depth-cue lookup table to
be used when rendering.

.Bl "light_table"
This attribute contains the resource ID of the light lookup table to
be used when rendering.

.Bl "color_approx_table"
This attribute contains the resource ID of the color approximation
lookup table to be used during the color approximation stage of the
rendering pipeline.

.Bl "pattern_table"
This attribute contains the resource ID of the lookup table to
be used when referencing patterns.

.Bl "text_font_table"
This attribute contains the resource ID of the lookup table to
be used when referencing text fonts.

.Bl "highlight_inclusion"
This attribute contains the resource ID of the name set to be used
as the highlight inclusion set.

.Bl "highlight_exclusion"
This attribute contains the resource ID of the name set to be used
as the highlight exclusion set.

.Bl "invisibility_inclusion"
This attribute contains the resource ID of the name set to be used
as the invisibility inclusion set.

.Bl "invisibility_exclusion"
This attribute contains the resource ID of the name set to be used
as the invisibility exclusion set.

.Bl "pick_inclusion"
This attribute contains the resource ID of the name set to be used
as the pick inclusion filter during picking.

.Bl "pick_exclusion"
This attribute contains the resource ID of the name set to be used
as the pick exclusion filter during picking.

.Bl "pick_start_path"
This attribute contains a hierarchical path that indicates where to
start the next server side "pick all" traversal.  \fBPEXPickAll\fP
starts hit testing at the next element after the one specified by this path.  
\fBPEXPickAll\fP
and \fBPEXEndPickAll\fP both set this attribute to the path of the
last element processed during the "pick all" traversal. 

.Bl "background_color"
This attribute contains the color value to use when clearing
the image buffer.  The image buffer is cleared by \fBPEXBeginRendering\fP
when \fIclear_I\fP is enabled.  The \fIcolor_approx_index\fP in the
initial pipeline context is used to map the color to a pixel value.

.Bl "clear_I"
.br
This attribute indicates whether the image buffer should be cleared to
the background color at \fBPEXBeginRendering\fP.  Only pixels
within the renderer's clip list are cleared.

.Bl "clear_Z"
This attribute indicates whether the z-buffer should be cleared
to infinity (the value used for infinity is platform dependent)
at the start of rendering.  Only pixels within the renderer's clip list  
are cleared. This attribute is ignored if the \fIHLHSR_mode\fP
does not require a z-buffer.

.Bl "echo_mode"
This attribute indicates whether primitives should be echoed, unechoed 
or drawn normally.  The method used for echoing and
unechoing primitives is implementation dependent and there is no
guarantee that the method employed will preserve temporal
priority.  Echoing takes precedence over highlighting.  It is recommended,
but not required, that the echo and highlight methods are visually distinct.

.Bl "HLHSR_mode"
This attribute contains the hidden line/hidden surface method
used when resolving visibility of overlapping primitives.
The \fBPEXGetRendererDynamics\fP request can be used to determine
whether changing the \fIHLHSR_mode\fP
while the \fIrenderer_state\fP attribute is set to \fIRendering\fP
has any effect.

.Bl "NPC_subvolume"
This attribute contains the normalized project coordinates for the
sub-volume space that is to be mapped to the viewport.

.Bl "viewport"
This attribute
is used to describe the area within a drawable in which 3D graphics
primitives may appear.  The viewport coordinates are specified
in device coordinates, which have an
x,y offset (in pixels) from the lower left corner of the drawable.
It is permissible to specify a viewport with boundaries outside of the
drawable.  Geometry that is mapped to values outside the drawable will
be clipped.
Viewport z values must be in the range [0-1.0], where z=0 maps to the
device-coordinate z value representing objects that are furthest from
the viewing position, and z=1 maps to the device-coordinate value representing
objects that are closest to the viewing position.
Depending on the dynamics of the \fIviewport\fP attribute (see
\fBPEXGetRendererDynamics\fP), the values that define a viewport may
be bound at any time, or they may take effect only at the time of
a \fBPEXBeginRendering\fP.  Whenever the viewport values are bound,
the viewport's \fIuse_drawable\fP flag is examined first.
If it is set to \fITrue\fP, the viewport width and height
are obtained from the drawable's current width and height and the
viewport z values are obtained from the values specified in the 
viewport structure.
If the \fIuse_drawable\fP flag is \fIFalse\fP,
the viewport width, height and z values are 
all set to the values in the viewport structure.
The viewport is then set to the largest rectangle, anchored at the
lower left corner, that achieves an isotropic mapping
to the renderer's \fINPC_subvolume\fP.
The viewport will remain in the same position relative to the 
lower left hand
corner of the drawable after a resize event has occurred.

.Bl "clip_list"
This attribute contains
a list of rectangles in device coordinates that define the
portions of the drawable in which rendering is enabled.  If the list
is \fINull\fP, then all of the pixels on the drawable may be overwritten
during rendering.  If the list is not \fINull\fP,
the renderer
may only render on those pixels that are within
the rectangles in the list.
The rectangles should be non-overlapping, or the graphics results
will be undefined.
Pixels that are outside of all of the rectangles in \fIclip_list\fP are
effectively "write-protected".  
The rectangles in the clip list are
defined by \fIxmin\fP, \fIymin\fP, \fIxmax\fP, and \fIymax\fP which
are in device coordinates.  (0,0) is at the lower left hand corner
of the window and the \fIxmax\fP and \fIymax\fP values are inclusive.
These rectangles will remain in the same position relative to the 
lower left hand corner of the drawable after a resize has occurred.
(Note: If a z-buffering algorithm is used, only those pixels under
the rectangles in the clip list will have their z values initialized when a
\fBPEXBeginRendering\fP or a \fBPEXRenderNetwork\fP request is issued.)
.bp

.AC "Renderer Resource Management" 2
.LP                   
The renderer is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation and
freeing of renderer resources.

.AC "Create Renderer" 3
.Fs
.Na PEXCreateRenderer
.Rq      
.Pa fp_format FLOAT_FORMAT
.Pa rdr_id RENDERER_ID
.Pa drawable_example DRAWABLE_ID
.Pa item_mask BITMASK
.Pa item_list LISTofVALUE
.Se
IDChoice, Drawable, PipelineContext, NameSet, LookupTable, FloatingPointFormat,
Value, Alloc, Match
.Ds                     
This request creates a renderer resource for the specified
\fIrdr_id\fP.  It can be used to render onto drawables with the
same root window and depth as the example drawable specified
by \fIdrawable_example\fP.
The \fIitem_mask\fP defines those renderer attributes
that are to be explicitly set at the time the resource is created.
The \fIitem_list\fP 
contains the corresponding
list of values used to modify the newly-created renderer.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe


.AC "Free Renderer" 3
.Fs
.Na PEXFreeRenderer
.Rq                 
.Pa rdr_id RENDERER_ID
.Se
Renderer
.Ds
This request
deletes the specified renderer resource and frees the storage
associated with it.
If the renderer's \fIrenderer_state\fP attribute is set to \fIRendering\fP
when this request is processed, an implicit \fBPEXEndRendering\fP
request with \fIflush\fP equal to \fIFalse\fP will be performed
before the renderer is freed.
If the renderer's \fIrenderer_state\fP attribute is set to \fIPicking\fP
when this request is processed, the pick operation will be aborted. 
.Fe
.bp
.AC "Renderer Modification" 2
.LP
The requests in this section
can be used to modify attributes of renderer resources.
.AC "Change Renderer" 3
.Fs
.Na PEXChangeRenderer
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa rdr_id RENDERER_ID
.Pa item_mask BITMASK
.Pa item_list LISTofVALUE
.Se
Renderer, Match, Value, FloatingPointFormat, NameSet, LookupTable,
PipelineContext
.Ds       
This request changes components of a renderer.  
The \fIitem_mask\fP and \fIitem_list\fP
specify which components are to be changed.
Each bit in the \fIitem_mask\fP indicates whether or not
there is a corresponding entry in the \fIitem_list\fP.
It is therefore possible
to modify one or many renderer attributes with a
\fBPEXChangeRenderer\fP request.
A renderer's \fIcurrent_path\fP and \fIrenderer_state\fP attribute are
read-only, therefore
attempts to modify them will be ignored.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe

.AC "Renderer Inquiry" 2
.LP
The requests in this section can be used to
inquire renderer attributes.
.AC "Get Renderer Attributes" 3
.Fs
.Na PEXGetRendererAttributes
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa rdr_id RENDERER_ID
.Pa item_mask BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
Renderer, FloatingPointFormat, Value
.Ds       
This request will return components of
the renderer specified by \fIrdr_id\fP.
The \fIitem_mask\fP specifies which components are to be inquired and returned.
The specified attributes of the renderer will be returned in
\fIitem_list\fP.
Floating point values in \fIitem_list\fP will be returned in the floating
point format specified in \fIfp_format\fP.  (The values returned are those
that were last set by the client.  Depending on the dynamics of the renderer,
these may or may not be the values currently in use.  See
\fBPEXGetRendererDynamics.)
.Fe
.bp
.AC "Get Renderer Dynamics" 3
.Fs
.Na PEXGetRendererDynamics
.Rq
.Pa rdr_id RENDERER_ID
.Re
.Pa tables BITMASK
.Pa namesets BITMASK
.Pa attributes BITMASK
.Se
Renderer
.Ds       
This request will return the dynamics (binding times) for all of
the attributes of the specified renderer.  The \fItables\fP bitmask
has the following bits defined:
.DS
.ta 2.0i
\fIMarkerBundle	MarkerBundleContents
TextBundle	TextBundleContents
LineBundle	LineBundleContents
InteriorBundle	InteriorBundleContents
EdgeBundle	EdgeBundleContents
ViewTable	ViewTableContents
ColorTable	ColorTableContents
DepthCueTable	DepthCueTableContents
LightTable	LightTableContents
ColorApproxTable	ColorApproxTableContents
PatternTable	PatternTableContents
TextFontTable	TextFontTableContents\fP
.ta
.DE
The \fInamesets\fP bitmask has the following bits defined:
.DS
.ta 2.0i
\fIHighlightNameset	HighlightNamesetContents
InvisibilityNameset	InvisibilityNamesetContents
PickNameset	PickNamesetContents\fP
.ta
.DE
The \fIattributes\fP bitmask has the following bits defined:
.DS
.ta 2.0i
\fIHLHSRMode
NPCSubvolume
Viewport
ClipList
EchoMode\fP
.ta
.DE
For each defined bit, a value of zero indicates that the specified
attribute is \fIdynamic\fP and a value of one indicates that it
is not.  Changes to \fIdynamic\fP attributes take effect immediately.  
Attributes which are not \fIdynamic\fP can be modified at any time
but the new value will not take effect until the next
explicit or implicit \fBPEXBeginRendering\fP occurs.  Implementations that
allow attributes such as \fIHLHSRMode\fP to be \fIdynamic\fP 
must specify the semantics of changing between all possible supported
values for that attribute.
.Fe
.bp

.AC "Client-Side Traversal Support" 2
.LP
These requests provide support for client-side structure
traversal.  See section on \fBRenderer Picking\fP for requests
to do client side picking.  Client-side search traversals are not 
supported.

.AC "Begin Rendering" 3
.Fs
.Na PEXBeginRendering
.Rq
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Se
Renderer, Drawable, Match, Alloc, RendererState
.Ds 
This request causes rendering to begin on the drawable specified by
\fIdrawable_id\fP.  The output of the renderer specified by \fIrdr_id\fP
is bound to that drawable until a \fBPEXEndRendering\fP request is
processed.  The initial rendering pipeline attributes are copied into
the renderer
from the pipeline context specified by the renderer's \fIpipeline_context\fP
attribute.  If \fIpipeline_context\fP is \fINull\fP, the default values
for all of the attributes in a pipeline context are copied into the renderer
instead.  The
renderer's \fIrenderer_state\fP attribute is set to \fIRendering\fP and
its \fIcurrent_path\fP attribute is initialized to structure id zero with
an element offset of zero.  If \fIclear_I\fP
is enabled then the image buffer will be cleared to \fIbackground_color\fP.
If \fIclear_Z\fP is enabled and the \fIHLHSR_mode\fP indicates that 
z-buffering should be performed, then the z-buffer will be cleared to infinity.
The image and
z-buffer are cleared only in the region specified by \fIclip_list\fP.
.LP
If the specified drawable does not have the same root and depth as
the drawable that was passed in to create the renderer, or, if the
specified drawable is not one of the supported drawables returned
by \fBPEXMatchRendererTargets\fP,
a \fIMatch\fP error will be generated.
.LP
If \fIrenderer_state\fP is currently \fIRendering\fP or \fIPicking\fP,
the operation is aborted,
the \fBPEXBeginRendering\fP request is executed, and a
\fIRendererState\fP error is returned.

.Fe


.AC "End Rendering" 3
.Fs
.Na PEXEndRendering
.Rq
.Pa rdr_id RENDERER_ID
.Pa flush BOOLEAN
.Se
Renderer, RendererState 
.Ds       
This request terminates the current rendering operation
for the renderer specified by \fIrdr_id\fP.
If \fIflush\fP is \fITrue\fP,
all pending output will be rendered onto the drawable.
If \fIflush\fP is \fIFalse\fP, all pending output is discarded.
In either case, the \fIrenderer_state\fP
attribute of the renderer is set to \fIIdle\fP.
If the \fIrenderer_state\fP attribute is currently \fIIdle\fP (i.e., no
rendering is in progress or the rendering was aborted due to a resize),
the request is ignored and no error is generated.  If the \fIrenderer_state\fP
is currently \fIPicking\fP then a \fIRendererState\fP error will be generated.
.Fe

.AC "Begin Structure" 3
.Fs
.Na PEXBeginStructure
.Rq
.Pa rdr_id RENDERER_ID
.Pa s_id CARD32
.Se
Renderer
.Ds       
If the \fIrenderer_state\fP attribute of the
renderer specified by \fIrdr_id\fP is set to \fIRendering\fP or \fIPicking\fP,
then this request causes the pipeline attributes of the
renderer to be saved.
If the \fIrenderer_state\fP attribute is set to \fIIdle\fP, then this
request is ignored.
The attributes of the renderer resource itself (i.e., the
attributes of a renderer that can be set/queried,
including table and name set resource IDs)
are not saved.
The element offset of the last entry in the renderer's
\fIcurrent_path\fP is incremented (to account for the
client-side "execute structure" command), then
the name specified by \fIs_id\fP together
with an element offset of zero (indicating an empty structure) is appended to the
\fIcurrent_path\fP attribute.  Each subsequent output command
will cause the element offset to be incremented by one until the
next \fBPEXBeginStructure\fP request or the next
\fBPEXEndStructure\fP request.
.LP
After saving the current rendering pipeline attributes,
the \fIglobal_transform\fP attribute is set
to the matrix computed by concatenating
the current \fIlocal_transform\fP and the
current \fIglobal_transform\fP matrices.  Then
the \fIlocal_transform\fP matrix is set
to the identity matrix.
.Fe



.AC "End Structure" 3
.Fs
.Na PEXEndStructure
.Rq
.Pa rdr_id RENDERER_ID
.Se
Renderer, RendererState
.Ds       
If the \fIrenderer_state\fP attribute of the
renderer specified by \fIrdr_id\fP is set to \fIRendering\fP or \fIPicking\fP,
then this request restores the last-saved pipeline attributes
in the renderer.
If the \fIrenderer_state\fP attribute is set to \fIIdle\fP, then this
request is ignored.
In addition, the last element reference in the renderer's \fIcurrent_path\fP
is removed, and subsequent output commands will cause the element
offset of the element reference at the end of the list to be incremented.
This request itself does not cause an increment to \fIcurrent_path\fP.
.Fe
.bp


.AC "Rendering Commands" 2
.LP
These requests cause output commands to be processed by a renderer.

.AC "Render Output Commands" 3
.Fs
.Na PEXRenderOutputCommands
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa rdr_id RENDERER_ID
.Pa cmds LISTofOUTPUT_CMD
.Se
Renderer, FloatingPointFormat, OutputCommand
.Ds       
If the \fIrenderer_state\fP attribute of the
renderer specified by \fIrdr_id\fP is set to \fIRendering\fP or \fIPicking\fP,
the output commands in \fIcmds\fP will be immediately processed.
If the \fIrenderer_state\fP attribute is set to \fIIdle\fP, the
commands will be ignored.
Floating point values in \fIcmds\fP will be in the floating
point format specified in \fIfp_format\fP.
The formats for the various types of output commands
can be found Section 3 - \fIOutput Commands\fP.
Output commands are processed by the server in order until one is found to
be in error, or until the entire list has been processed.
The erroneous output command and all others following it in the request
are ignored, and an \fIOutputCommand\fP error is returned to the client.
The \fIcurrent_path\fP attribute of the renderer is updated to reflect
only those output commands actually processed.
.Fe

.AC "Render Elements" 3
.Fs
.Na PEXRenderElements
.Rq
.Pa rdr_id RENDERER_ID
.Pa s_id STRUCTURE_ID
.Pa range ELEMENT_RANGE
.Se
Renderer, Structure, Value
.Ds       
If the \fIrenderer_state\fP attribute of the
renderer specified by \fIrdr_id\fP is set to \fIRendering\fP or \fIPicking\fP,
then this request causes a range of structure elements in the structure
specified by \fIs_id\fP to be processed using the specified renderer.
If the \fIrenderer_state\fP attribute is set to \fIIdle\fP, then this
request is ignored.
The semantics of this request are the same as for
\fBPEXRenderOutputCommands\fP except that the output commands are
taken from the specified structure.
.LP
The offsets that define the range of elements to be rendered 
are computed using the positions in \fIrange\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIs_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If either computed offset is less than zero, it will be set to zero before
the rendering occurs.
If either computed offset is greater than the number
of elements in the structure, it will be set to the offset
of the last structure element.
The structure's element pointer is not affected by this request.
.Fe

.AC "Accumulate State" 3
.Fs
.Na PEXAccumulateState
.Rq
.Pa rdr_id RENDERER_ID
.Pa path LISTofELEMENT_REF
.Se
Renderer, Path
.Ds       
If the \fIrenderer_state\fP attribute of the
renderer specified by \fIrdr_id\fP is set to \fIRendering\fP or \fIPicking\fP,
this request accumulates the state that would be in
effect if a traversal were done to the element specified by \fIpath\fP.
If the \fIrenderer_state\fP attribute is set to \fIIdle\fP, then this
request is ignored.
.LP
Accumulation of state begins with the current pipeline
attributes.  A linear descent down the specified path is
then made and the structure elements that lie along the
specified path are examined in order.  Any element that
contains an output command that would modify pipeline state
(such as \fBSet Line Color\fP) is sent to the renderer for
processing.  All other output commands (such as \fBPolyLine 3D,
Execute Structure, Label, etc\fP) are skipped.
The traversal is flat meaning that
the current pipeline attributes will not get "pushed"
when a structure in the path is executed.  However,
the current path offset is bumped for each output command that
is encountered during accumulate state.
.Fe

.AC "Render Network" 3
.Fs
.Na PEXRenderNetwork
.Rq
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Pa s_id STRUCTURE_ID
.Se
Renderer, Drawable, Structure, Match, Alloc, RendererState
.Ds       
This request causes the structure network rooted at \fIs_id\fP
to be traversed and rendered on \fIdrawable_id\fP, using the renderer
specified by \fIrdr_id\fP.
.LP
This functionality is equivalent to the following (pseudo-)requests:
.ID
\fBPEXBeginRendering\fP(\fIrdr_id\fP, \fIdrawable_id\fP)
\fBPEXRenderElements\fP(\fIrdr_id\fP, \fIs_id\fP, all elements of \fIs_id\fP)
\fBPEXEndRendering\fP(\fIrdr_id\fP, \fITrue\fP)
.DE
.Fe
.bp


.AC "Renderer Picking" 1
.LP
.RU
.LP
The semantics of picking are identical to rendering except that
during picking primitives are hit tested instead of converted to pixels.
In other words, all primitives which are passed to the rendering pipeline
(ie: they do not satisfy the invisibility filter and they are not
culled backfaces)
are eligible for picking.  Picking uses the geometric definition
of primitives so it is possible for hollow, empty and transparent
polygons to be picked.  However it is implementation dependent
whether rendering effects which increase the
area of the primitive (such as linewidth) are considered.
.LP
Two types of hit test traversals are supported: "pick one" and "pick all".  
A pick one traversal will return at most one primitive
which satisfies the pick criteria.  A "pick all" traversal will return
all primitives which satisfy the pick criteria,
up to a user specified maximum number of hits.
For both types of traversals
the spatial test which is performed on the primitives
is controlled by the hit box which is specified in \fIpick_data\fP.
.LP
The hit box may be specified in device coordinates (if the server supports
\fIDC_HitBox\fP pick devices) or in NPC space (if the server supports
\fINPC_HitVolume\fP pick devices).  The supported pick device types can be 
inquired via \fBPEXGetEnumeratedTypeInfo\fP.
A device coordinate hit box is specified by a 
\fIpick_position\fP and a \fIpick_distance\fP.
The \fIpick_position\fP is the device coordinate center for the hit box
and the shape of the hit box (ie: square, rectangle, circle) is implementation
dependent.  \fIpick_distance\fP, also measured in device coordinates,
defines the half-width or radius of the hit box.  
The NPC hit volume is specified by two points in NPC space.
Primitives that lie within or intersect the hit box are considered "hit".
.LP
The \fIpick_method\fP determines which "hit" primitives will be picked.  
For pick one, \fIpick_method\fP can be set to:
\fILast\fP, \fIClosestZ\fP, \fIVisibleAny\fP, or \fIVisibleClosest\fP.
For pick all, \fIpick_method\fP can be set to:
\fIAll\fP or \fIVisible\fP.  An error will be returned if the 
specified \fIpick_method\fP is not supported on the connection.  The supported
pick one and pick all methods can be inquired using
\fBPEXGetEnumeratedTypeInfo\fP.  PEX servers are required to support
\fILast\fP for pick one and \fIAll\fP for pick all.
.LP
The \fIpick_inclusion\fP and \fIpick_exclusion\fP renderer attributes specify 
the name sets to be used to filter picked primitives after a pick
traversal.  The pick requests will only return primitives which are not 
excluded by the pick filter.  If possible, pick one will return 
a primitive which most closely satisfies the pick criteria and also passes
the pick filter test.  A flag is returned from pick one requests to indicate whether
a primitive which did not satisfy the pick filter was a better candidate
with the specified \fIpick_method\fP.  The dynamics of
\fIpick_inclusion\fP and \fIpick_exclusion\fP can be determined by
calling \fBPEXGetRendererDynamics\fP.
.LP
During a pick one traversal at most one primitive is picked.  If the 
\fIpick_method\fP is \fILast\fP then the last "hit" primitive which 
satisfies the pick filter test is returned.  If the \fIpick_method\fP is
\fIClosestZ\fP then the "hit" primitive which has a z value closest to
the front clipping plane and satisfies the pick filter is returned.
If multiple primitives satisfy this criteria then any of them may be
returned.
If the \fIpick_method\fP is \fIVisibleAny\fP then any "hit" primitive
which is visible (taking the current HLHSR mode into account)
and satisfies the pick filter may be returned.  If the \fIpick_method\fP is 
\fIVisibleClosest\fP then all "hit" primitives which are visible
are first selected.
Of the selected primitives, the one which is closest to the 
\fIpick_position\fP and satisfies the pick filter is returned.  If
an NPC hit volume was specified then the \fIpick_position\fP is the
center of the volume.   The z buffer contents may not be preserved
when using the "visible" pick methods.
.LP
The "visible" pick methods only return primitives
which would be visible in 
the pick aperature if they were rendered using the renderer's current HLHSR
mode.  For example, if the HLHSR mode is set to
\fIZBuffer\fP, the pick one method \fIClosestZ\fP may return a primitive
which is occluded by another primitive which does not satisfy the pick filter
test.  However, pick one method \fIVisibleAny\fP guarantees that the 
picked primitive is not occluded.
.LP
During a "pick all" traversal multiple primitives may be picked.  If the 
\fIpick_method\fP is \fIAll\fP then the first \fImax_hits\fP primitives,
which lie within or intersect the hit box, are returned.
If the \fIpick_method\fP is \fIVisible\fP then 
the first \fImax_hits\fP primitives which are visible 
(taking the current HLHSR mode into account)
and lie within or intersect the hit box, are returned.
Pick first can be accomplished by setting \fImax_hits\fP equal
to one.
.LP
\fIPickAll\fP is further controlled by the
\fIpick_start_path\fP renderer attribute.  \fIpick_start_path\fP 
indicates where to begin the next \fIPickAll\fP traversal and it
is bound at the start of a \fIPickAll\fP traversal.
.LP
If the drawable that is associated with a renderer is destroyed or
resized while the renderer is performing a pick one or pick all 
hit test, the pick operation is terminated and the renderer is returned to the 
\fIIdle\fP state.  All subsequent output and traversal commands
(e.g., \fBPEXRenderOutputCommands\fP, \fBPEXBeginStructure\fP,
\fBPEXEndStructure\fP, \fBPEXRenderElements\fP, and \fBPEXAccumulateState\fP)
are ignored until the final \fBPEXEndPickOne\fP
or \fBPEXEndPickAll\fP request is received.  No errors are generated but
\fIpick_status\fP will be set to \fIAborted\fP in the pick reply.
.LP
If the drawable that is associated with a renderer is moved, exposed,
or occluded while the
renderer is performing a pick one or pick all hit test, the server must 
continue to process output commands using the newly-modified window hierarchy
until the pick operation is explicitly or implicitly terminated.
.LP
.bp

.AC "Pick One" 2
.LP

.AC "Begin Pick One" 3
.Fs
.Na PEXBeginPickOne
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pick_method PICK_ONE_METHOD
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Pa s_id INT32
.Pa pick_data PICK_RECORD
.Se
Renderer, Drawable, Match, Alloc, RendererState, Value
.Ds
This request starts a client side pick one traversal.
When a renderer is in "pick one" mode, primitives will be hit
tested instead of converted to pixels and, a hierarchical path 
to the picked primitive will be maintained. \fIpick_method\fP
specifies the pick criteria to use so that only one primitive
will be selected.  It may be set to
\fILast\fP, \fIClosestZ\fP, \fIVisibleAny\fP, or \fIVisibleClosest\fP.
(\fBPEXBeginPickAll\fP will return the
\fIFirst\fP picked primitive when \fImax_hits\fP is set to one.)
If the specified \fIpick_method\fP is not supported then a \fIValue\fP
error will be returned. 
.LP
The pick aperature is defined by \fIpick_data\fP. Either a \fIDC_HitBox\fP
or \fINPC_HitVolume\fP can be specified.
.LP
\fIs_id\fP is the root of the client structure network.  The 
current path is initialized to structure \fIs_id\fP and element
offset zero before the pick traversal begins.  
The pick path which is returned from a pick one traversal 
is equivalent to the renderer's
current path at the time the picked primitive was processed.
.LP
If the specified drawable does not have the same root and depth as
the drawable that was passed in to create the renderer, or, if the
specified drawable is not one of the supported drawables returned
by \fBPEXMatchRendererTargets\fP,
a \fIMatch\fP error will be generated.
If the \fIrenderer_state\fP is set to \fIRendering\fP or \fIPicking\fP, 
then the operation will be aborted,
the \fBPEXBeginPickOne\fP request will be executed, and a
\fIRendererState\fP error returned.
.LP
The existing PEX requests that process output commands or manipulate
the current attributes (ie: \fBPEXRenderOutputCommands\fP,
\fBPEXBeginStructure\fP, \fBPEXEndStructure\fP, \fBPEXRenderElements\fP,
and \fBPEXAccumulateState\fP) can be issued
when the renderer is in \fIPicking\fP mode. They will have
the same semantics except that primitives are hit tested instead
of converted to pixels.
.Fe
.bp

.AC "End Pick One" 3
.Fs
.Na PEXEndPickOne
.Rq
.Pa rdr_id RENDERER_ID
.Re
.Pa pick_status PICK_STATUS
.Pa better_pick BOOLEAN
.Pa pick_path LISTofPICK_ELEMENT_REF
.Se
Renderer, RendererState
.Ds
This request places the renderer in \fIIdle\fP mode
and returns the pick status along with the hierarchical pick path.
If a primitive was picked, then \fIpick_status\fP will be set to \fIOk\fP
and \fIpick_path\fP will contain the renderer's current path at the time the 
picked primitive was processed.
If no primitive was picked then \fIpick_status\fP will be set to \fINoPick\fP
and \fIpick_path\fP will be empty.
If the renderer's drawable was destroyed
or resized during the pick operation then \fIpick_status\fP will be set 
to \fIAborted\fP and \fIpick_path\fP will be empty.
.LP
If there was a primitive which more closely satisfied the pick criteria
but did not pass the pick filter test then \fIbetter_pick\fP will be
set to \fITrue\fP.  Otherwise \fIbetter_pick\fP will be set to \fIFalse\fP. 
.LP
If the \fIrenderer_state\fP is \fIIdle\fP when this request is received
(i.e., no picking is in progress or picking was aborted due to a resize),
then this request is ignored but no error is generated.  If the 
\fIrenderer_state\fP is \fIRendering\fP or a "pick all" is in progress then
a \fIRendererState\fP error will be generated.
.Fe

.AC "Pick One" 3
.Fs
.Na PEXPickOne
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pick_method PICK_ONE_METHOD
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Pa s_id STRUCTURE_ID
.Pa pick_data PICK_RECORD
.Re
.Pa pick_status PICK_STATUS
.Pa better_pick BOOLEAN
.Pa pick_path LISTofPICK_ELEMENT_REF
.Se
Renderer, Drawable, Structure, Match, Alloc, RendererState, Value
.Ds
This request causes a pick one traversal to be performed on the
structure network rooted at \fIs_id\fP.  
It returns the pick status along with the hierarchical pick path.
.LP
This request is equivalent to the following (pseudo-)requests:
.ID
\fBPEXBeginPickOne\fP(\fIpick_method\fP, \fIrdr_id\fP, \fIdrawable_id\fP, \fIs_id\fP, \fIpick_data\fP)
\fBPEXRenderElements\fP(\fIrdr_id\fP, \fIs_id\fP, all elements of \fIs_id\fP)
\fBPEXEndPickOne\fP(\fIrdr_id\fP)
.DE
.Fe

.AC "Pick All" 2
.LP

.AC "Begin Pick All" 3
.Fs
.Na PEXBeginPickAll
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pick_method PICK_ALL_METHOD
.Pa send_event BOOLEAN
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Pa s_id INT32
.Pa pick_max_hits CARD32 
.Pa pick_data PICK_RECORD
.Se
Renderer, Drawable, Match, Alloc, RendererState, Value
.Ds
This request starts a client side "pick all" traversal.
When the renderer is in "pick all" mode, primitives are hit
tested instead of converted to pixels.  All picked primitives are
recorded until \fIpick_max_hits\fP is reached.  Additional
primitives will not be recorded.  Once the 
the maximum number of hits is reached, the server may
choose to ignore all subsequent primitives.  When \fImax_hits\fP
is set to one this request will return the 
\fIFirst\fP picked primitive.
.LP
\fIpick_method\fP specifies the pick criteria to use during the
"pick all" traversal.  It may be set to \fIAll\fP or \fIVisible\fP.
If the specified \fIpick_method\fP is not supported then a \fIValue\fP
error will be returned. 
.LP
The pick aperature is defined by \fIpick_data\fP. Either a \fIDC_HitBox\fP
or \fINPC_HitVolume\fP can be specified.
.LP
\fIs_id\fP is the root of the client structure network.  The 
current path is initialized to structure \fIs_id\fP and element
offset zero before the pick traversal begins.  
All pick paths which are returned from a "pick all" traversal 
are equivalent to the renderer's
current path at the time the picked primitive was processed.
.LP
If \fIsend_event\fP is \fITrue\fP and \fIpick_method\fP is \fIAll\fP, 
then a \fBMaxHitsReached\fP event will be sent
to the client whenever the maximum number of hits is reached by the server.
(If the \fIpick_method\fP is
\fIVisible\fP then a complete traversal must be performed before any
primitives can be picked.)
Upon receiving the event, the client should end the traversal and
see if the recorded hits are sufficient.  \fBPEXGetImpDepConstants\fP
returns a flag indicating whether this event is supported by the server.
.LP
If the specified drawable does not have the same root and depth as
the drawable that was passed in to create the renderer, or, if the
specified drawable is not one of the supported drawables returned
by \fBPEXMatchRendererTargets\fP, a \fIMatch\fP error will be generated.
If the \fIrenderer_state\fP is \fIRendering\fP or \fIPicking\fP,
then the operation will be aborted,
the \fBPEXBeginPickAll\fP request will be executed, and a
\fIRendererState\fP error returned.
.Fe

.AC "End Pick All" 3
.Fs
.Na PEXEndPickAll
.Rq
.Pa rdr_id RENDERER_ID
.Re
.Pa pick_status PICK_STATUS
.Pa more_picks PICK_ALL_STATE
.Pa pick_paths LISTofLISTofPICK_ELEMENT_REF
.Se
Renderer, RendererState
.Ds
This request terminates the current client side "pick all"
traversal, places the renderer in \fIIdle\fP mode, and returns 
the pick status along with the pick paths of any picked primitives.
If one or more primitives were picked, then \fIpick_status\fP will 
be set to \fIOk\fP and \fIpick_paths\fP will contain a hierarchical
path for each picked primitive, stored in traversal order.  
The hierarchical pick path is equivalent to 
the renderer's current path at the time the picked primitive was processed.
If no primitives were picked then \fIpick_status\fP will be set to \fINoPick\fP
and \fIpick_paths\fP will be empty.
If the renderer's drawable was destroyed
or resized during the pick operation then \fIpick_status\fP will be set 
to \fIAborted\fP and \fIpick_paths\fP will be empty.
.LP
If all picks were recorded then \fImore_picks\fP will be
set to \fINoMoreHits\fP and the renderer's \fIpick_start_path\fP 
attribute will be empty.
If the maximum number of hits was reached and additional
picks were detected, then \fImore_picks\fP will be set to \fIMoreHits\fP
and \fIpick_start_path\fP will be set to the last picked primitive
which was recorded.
If the server, after reaching the maximum number of hits,
started ignoring all subsequent output commands,
then \fImore_picks\fP will be set to \fIMayBeMoreHits\fP and 
\fIpick_start_path\fP
will be set to the last element processed by the renderer before
it started ignoring primitives.
.LP
If the \fIrenderer_state\fP is \fIIdle\fP when this request is received
(i.e., no picking is in progress or picking was aborted due to a resize),
then this request is ignored but no error is generated.  If the 
\fIrenderer_state\fP is \fIRendering\fP or a "pick one" is in progress then
a \fIRendererState\fP error will be generated.
.Fe

.AC "Pick All" 3
.Fs
.Na PEXPickAll
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa pick_method PICK_ALL_METHOD
.Pa rdr_id RENDERER_ID
.Pa drawable_id DRAWABLE_ID
.Pa pick_max_hits CARD32 
.Pa pick_data 	PICK_RECORD
.Re
.Pa pick_status PICK_STATUS
.Pa more_picks PICK_ALL_STATE
.Pa pick_paths LISTofLISTofPICK_ELEMENT_REF
.Se
Renderer, Drawable, Match, Alloc, RendererState, Value, Path
.Ds
This request traverses the structure network specified in the 
renderer's \fIpick_start_path\fP attribute.  Hit testing
begins after the last element specified in \fIpick_start_path\fP
and continues until one of two conditions is met\: 
\fIpick_max_hits\fP primitives are picked or the last element of the 
first structure in pick_start_path is processed.
If the start path
does not define a valid hierarchical path, a \fIPath\fP error is
generated and no results are returned.
.LP
The pick aperature is defined by \fIpick_data\fP. Either a \fIDC_HitBox\fP
or \fINPC_HitVolume\fP can be specified.
.LP
The paths of all hit primitives are recorded until \fIpick_max_hits\fP 
is reached.  These paths are returned in traversal order in \fIpick_paths\fP.
Once the maximum number of paths is recorded, the server may choose
to ignore subsequent primitives and return the results found.
When \fImax_hits\fP is set to one this request will return the 
\fIFirst\fP picked primitive.
.LP
If all possible hits were recorded then \fImore_picks\fP will be
set to \fINoMoreHits\fP and the renderer's \fIpick_start_path\fP 
attribute will be empty.
If the maximum number of hits was reached and additional
hits were detected, then \fImore_picks\fP will be set to \fIMoreHits\fP
and \fIpick_start_path\fP will be set to the last picked
primitive which was recorded.
If the server, after reaching the maximum number of hits,
started ignoring all subsequent output commands,
then \fImore_picks\fP will be set to \fIMayBeMoreHits\fP and 
\fIpick_start_path\fP
will be set to the last element processed by the renderer before
it started ignoring primitives.
.Fe

.bp
.AC "Structures" 1
.LP
.RU
.LP
This section details the usage and management of structure resources.
A structure is a resource which stores output commands for later
execution.
Structures have two settable attributes and a number of attributes
that can only be inquired.  The modifiable attributes are:

.Bl "element_ptr"
This attribute contains the offset of an element in the structure.
Elements are numbered consecutively, starting at offset one.
The element pointer
determines the position in the structure at which the next editing
or element query operation will occur.
Whenever a structure
is used as a destination in a structure editing
request, its element pointer attribute
will be updated as a side effect of the operation.
When a structure is created, its element pointer is set to zero.

.Bl "editing_mode"
The editing mode attribute specifies how editing operations will affect
the structure.
If the mode is \fIStructureInsert\fP, 
subsequent requests to create structure elements
will cause elements to be inserted into
the structure.  Elements will be
\fIinserted\fP into the structure
\fIafter\fP the structure element specified by the
element pointer.  The element pointer will then be
incremented by the number of elements inserted.
If the mode is \fIStructureReplace\fP, output
requests used to create structure elements
will cause structure elements to
\fIreplace\fP elements starting at the location specified by the
element pointer.  When a structure is created, its editing mode
is set to \fIStructureInsert\fP.

.LP
A structure's non-modifiable attributes include its size
(number of elements and length in four byte units if it were to be
returned to the client) and the number of times it is
referenced by other structures.  Additional information
about structures (ancestors, descendants, etc.) is also available
and can be inquired separately.

.AC "Structure Resource Management" 2
.LP
A structure is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
deletion, and general manipulation of structures.


.AC "Create Structure" 3
.Fs
.Na PEXCreateStructure
.Rq                  
.Pa s_id STRUCTURE_ID
.Se
IDChoice, Alloc
.Ds      
This request creates a structure resource with the specified \fIs_id\fP.
.Fe
.bp
.AC "Copy Structure" 3
.Fs
.Na PEXCopyStructure
.Rq                  
.Pa src_s_id STRUCTURE_ID
.Pa dest_s_id STRUCTURE_ID
.Se
Structure
.Ds      
The structure elements in \fIsrc_s_id\fP are copied to \fIdest_s_id\fP.
Any structure elements in \fIdest_s_id\fP are deleted prior to the copy
operation.  The destination structure must already exist as a valid resource.
\fISrc_s_id\fP's element pointer and editing mode are also copied to
\fIdest_s_id\fP.
.Fe

.AC "Destroy Structures" 3
.Fs
.Na PEXDestroyStructures
.Rq                  
.Pa list LISTofSTRUCTURE_ID
.Se
Structure
.Ds 
This request destroys each of the structure resources that
is specified in \fIlist\fP and removes all references to it
in the server.  Any structure elements that reference a structure
in \fIlist\fP will be destroyed, and any structure in \fIlist\fP that
is posted to a PHIGS workstation resource will be unposted.
Any paths in search contexts or pick measures that contain the resource ID of
any structure in \fIlist\fP may still be queried.  Such paths may still
contain the resource ID of a structure after it has been destroyed.
However, any attempts to perform a \fBPEXSearchNetwork\fP or a
\fBPEXUpdatePickMeasure\fP using such a path will result in a \fIPath\fP error
being generated.
.LP
Any structure that had structure elements removed due to a
\fBPEXDestroyStructures\fP request will have their element pointer
modified in the following way.  If the element pointer is at a
position before any of the elements that were deleted, the element
pointer remains unchanged.
If the element pointer was at the position of an element that
was deleted, it will be set to the previous element.
If the element pointer was after elements that were deleted, it
will be updated so as to point at the same element it pointed to
prior to the \fBPEXDestroyStructures\fP request.

.Fe
.bp

.AC "Structure Inquiry" 2
.LP
These requests inquire attributes and other information about structures.

.AC "Get Structure Info" 3
.Fs
.Na PEXGetStructureInfo
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa s_id STRUCTURE_ID
.Pa item_mask BITMASK_SHORT
.Re
.Pa editing_mode EDIT_MODE
.Pa element_ptr CARD32
.Pa num_elements CARD32
.Pa total_length CARD32
.Pa has_refs BOOLEAN
.Se
Structure, FloatingPointFormat, Value
.Ds       
This request returns information about the structure specified by \fIs_id\fP.
\fIItem_mask\fP indicates which fields in the reply are required by the client.
Items indicated by \fIitem_mask\fP will have valid values returned, and the
remaining values will be returned as undefined values.
The current value of the structure's editing mode attribute will be returned
in \fIediting_mode\fP. 
The current value of the structure's element pointer
attribute will be returned in \fIelement_ptr\fP.
The number of structure elements in the specified
structure will be returned in \fInum_elements\fP.
If the structure
is empty, \fInum_elements\fP will be zero.
The total number of four-byte units necessary to store
all elements of the structure (on the client side) will be returned
in \fItotal_length\fP.
(This provides the
client the information needed to allocate memory prior
to doing a \fBPEXFetchElements\fP request.)
If there are any "execute structure" elements in other structure resources
that reference \fIs_id\fP, \fIhas_refs\fP will be returned as \fITrue\fP,
otherwise it will be returned as \fIFalse\fP.
The information returned will be computed as if the floating point
type in \fIfp_format\fP were being used to return
the data.
.Fe
.bp


.AC "Get Element Info" 3
.Fs
.Na PEXGetElementInfo
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa s_id STRUCTURE_ID
.Pa range ELEMENT_RANGE
.Re
.Pa info LISTofELEMENT_INFO
.Se
Structure, Value, FloatingPointFormat
.Ds       
This request returns information about elements in
the structure specified by \fIs_id\fP.
The offsets that define the range of elements about which information is
to be obtained
are computed using the positions in \fIrange\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIs_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If either computed offset is less than zero, it will be set to zero before
obtaining the element information.
If either computed offset is greater than the number
of elements in the structure, it will be set to the offset
of the last structure element in the structure.
The request returns each element's ELEMENT_TYPE field, and the length
of the element in units of four bytes.  The sum of the lengths of all
elements in the structure will be equal to the length that can be inquired with
the \fBPEXGetStructureInfo\fP request.
The information returned will be computed as if the floating point
type in \fIfp_format\fP were being used to return
the data.
No information will be returned for inquiries on element offset zero.
The element pointer attribute of \fIs_id\fP is not affected by this request.
.Fe

.AC "Get Structures In Network" 3
.Fs
.Na PEXGetStructuresInNetwork
.Rq
.Pa s_id STRUCTURE_ID
.Pa which "{\fIAll, NoCrossRefs\fP}"
.Re
.Pa structures LISTofSTRUCTURE_ID
.Se
Structure, Value
.Ds
This request returns a list of unique structure resource IDs that are
referenced in the structure network rooted at \fIs_id\fP.
If \fIwhich\fP is \fIAll\fP, all of the structure resources
referenced in the structure network rooted at \fIs_id\fP will
be returned in the list \fIstructures\fP.
If \fIwhich\fP is \fINoCrossRefs\fP, 
only the IDs of structures in the network that aren't referenced
outside the specified structure network are returned.  \fIS_id\fP
will always be returned in the list, unless \fIs_id\fP is an invalid
structure ID, in which case the returned list will be empty.
.Fe
.AC "Get Ancestors" 3
.Fs
.Na PEXGetAncestors
.Rq
.Pa s_id STRUCTURE_ID
.Pa path_part "{\fITopPart, BottomPart\fP}"
.Pa path_depth CARD32
.Re
.Pa paths LISTofLISTofELEMENT_REF
.Se
Structure, Value
.Ds
This request returns unique paths in the structure hierarchy
that lead to \fIs_id\fP.
Paths are returned as a list of structure id/element offset pairs that
are in the order from the root structure down to \fIs_id\fP.
For full paths, the last entry in each path would have the structure
ID equal to \fIs_id\fP and the offset equal to zero.
The \fIpath_depth\fP indicates the maximum length
(number of structure id/element offset pairs)
of each path, except that \fIpath_depth\fP = 0 means
that the full path is to be returned.  The \fIpath_part\fP indicates whether
the head or the tail of each path is to be returned.
If \fIpath_part\fP = \fITopPart\fP, only the first \fIpath_depth\fP
structure id/element offset pairs in each path will be returned.
If \fIpath_part\fP = \fIBottomPart\fP, only the last \fIpath_depth\fP
structure id/element offset pairs in each path will be returned.
Only unique paths
will be returned (i.e., there will be no duplicates in the list of
returned paths).
.LP
For instance, specifying \fIpath_depth\fP = 0 and \fIpath_part\fP =
\fITopPart\fP would cause all paths leading to \fIs_id\fP to be returned.
Specifying \fIpath_depth\fP = 1 and \fIpath_part\fP =
\fITopPart\fP would cause all the structures at the top of each structure
network containing \fIs_id\fP to be returned.
Specifying \fIpath_depth\fP = 1 and \fIpath_part\fP =
\fIBottomPart\fP would allow you to determine whether or not
\fIs_id\fP is referenced.
Specifying \fIpath_depth\fP = 2 and \fIpath_part\fP =
\fIBottomPart\fP would allow you to determine the number of immediate ancestors
for \fIs_id\fP as well as returning their resource IDs.
.Fe
.bp

.AC "Get Descendants" 3
.Fs
.Na PEXGetDescendants
.Rq
.Pa s_id STRUCTURE_ID
.Pa path_part "{\fITopPart, BottomPart\fP}"
.Pa path_depth CARD32
.Re
.Pa paths LISTofLISTofELEMENT_REF
.Se
Structure, Value
.Ds
This request returns unique paths in the structure hierarchy
from \fIs_id\fP to leaf nodes in the hierarchy.
Paths are returned as a list of structure id/element offset pairs that
are in the order from \fIs_id\fP down to the leaf nodes.
For full paths, the first entry in each path would have the structure
ID equal to \fIs_id\fP and the offset equal to the offset of the element
that references a descendant in the hierarchy, and the last entry
would have the structure ID of a leaf node in the structure hierarchy
and an offset of zero.
The \fIpath_depth\fP indicates the maximum length
(number of structure id/element offset pairs)
of each path, except that \fIpath_depth\fP = 0 means
that the full path is to be returned.  The \fIpath_part\fP indicates whether
the head or the tail of each path is to be returned.
If \fIpath_part\fP = \fITopPart\fP, only the first \fIpath_depth\fP
structure id/element offset pairs in each path will be returned.
If \fIpath_part\fP = \fIBottomPart\fP, only the last \fIpath_depth\fP
structure id/element offset pairs in each path will be returned.
Only unique paths
will be returned (i.e., there will be no duplicates in the list of
returned paths).
.LP
For instance, specifying \fIpath_depth\fP = 0 and \fIpath_part\fP =
\fITopPart\fP would cause all paths leading from \fIs_id\fP to leaf nodes
to be returned.
Specifying \fIpath_depth\fP = 1 and \fIpath_part\fP =
\fITopPart\fP would cause all the direct
references to other structures in \fIs_id\fP
to be returned.
Specifying \fIpath_depth\fP = 1 and \fIpath_part\fP =
\fIBottomPart\fP could be used to determine the leaf nodes for the structure
hierarchy rooted at \fIs_id\fP.
.Fe
.bp
.AC "Fetch Elements" 3
.Fs
.Na PEXFetchElements
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa s_id STRUCTURE_ID
.Pa range ELEMENT_RANGE
.Re
.Pa elements LISTofOUTPUT_CMD
.Se
Structure, FloatingPointFormat, Value
.Ds       
This request returns a range of structure elements
in the structure specified by \fIs_id\fP.
The offsets that define the range of elements to be fetched
are computed using the positions in \fIrange\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIs_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If either computed offset is less than zero, it will be set to zero before
the fetch occurs.  If either computed offset is greater than the number
of elements in the source structure, it will be set to the offset
of the last structure element in the source structure.
The element pointer attribute of \fIs_id\fP is not affected by the
fetch operation.
The data for each structure element will be returned in
the same format as described Section 3 - \fIOutput Commands\fP section.
.LP
Floating point values in \fIelements\fP will be returned in the floating
point format specified in \fIfp_format\fP (if it is supported by the PEX
implementation).
.LP
No information will be returned for inquiries on element offset zero.
.Fe
.bp

.AC "Structure Resource Attribute Modification" 2
.LP
This section contains requests that can be used to modify structure resource
attributes.

.AC "Set Editing Mode" 3
.Fs
.Na PEXSetEditingMode
.Rq                  
.Pa s_id STRUCTURE_ID
.Pa mode EDIT_MODE
.Se
Structure, Value
.Ds 
This request modifies \fIs_id\fP's editing mode attribute.
The editing mode attribute specifies how editing operations will affect
the structure.
If the editing mode is set to \fIStructureInsert\fP, 
subsequent requests to create structure elements
will cause elements to be inserted into
the structure.  Elements will be
\fIinserted\fP into the structure
\fIafter\fP the structure element specified by the
element pointer.  The element pointer will then be
incremented by the number of elements inserted.
If the mode is \fIStructureReplace\fP, output
requests used to create structure elements
will cause structure elements to
\fIreplace\fP elements starting at the location specified by the
element pointer.
.Fe


.AC "Set Element Pointer" 3
.Fs
.Na PEXSetElementPointer
.Rq           
.Pa s_id STRUCTURE_ID
.Pa position ELEMENT_POS
.Se
Structure, Value
.Ds       
This request
sets the element pointer attribute of the structure specified by \fIs_id\fP.
The element pointer attribute of the structure will be set to
the position specified by \fIposition\fP, which contains a \fIwhence\fP
and \fIoffset\fP pair.
If \fIwhence\fP is \fIBeginning\fP, the element pointer is set to
the specified \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the new element pointer value
is computed by adding \fIoffset\fP
to the current value of the element pointer.
If \fIwhence\fP is \fIEnd\fP, the new element pointer value
is computed by adding the specified \fIoffset\fP
to the offset of the last element in the structure.
\fIOffset\fP can be a negative number.
.LP            
If the resultant value of the element pointer
is less than zero, the element pointer is set to zero.  If the resultant value
of the element pointer 
is greater than the
number of elements in the structure, then the element pointer
is set to the offset of the last element in the structure.
.Fe            
.bp


.AC "Set Element Pointer At Label" 3
.Fs
.Na PEXSetElementPointerAtLabel
.Rq
.Pa s_id STRUCTURE_ID
.Pa label INT32
.Pa offset INT32
.Se
Structure, Label
.Ds
This request
sets the element pointer attribute of the structure specified by \fIs_id\fP.
A search is conducted for
the next occurrence of a "label" structure element containing \fIlabel\fP.
The search for the label starts
at the current element pointer plus one, and proceeds in the forward direction.
If a "label" structure element containing \fIlabel\fP is found,
the element pointer for the structure is set
to the offset of the located label plus \fIoffset\fP.
.LP            
If the resultant value of the element pointer
is less than zero, the element pointer is set to zero.  If the resultant value
of the element pointer 
is greater than the
number of elements in the structure, then the element pointer
is set to the offset of the last element in the structure.
This is a non-descending search (i.e., the search does not include
any structures referenced by "execute structure" elements).
If no occurrence of the specified label is found, an error is
generated and the structure's
element pointer is left unchanged.
.Fe
.bp

.AC "Element Search" 3
.Fs
.Na PEXElementSearch
.Rq
.Pa s_id STRUCTURE_ID
.Pa position ELEMENT_POS
.Pa direction "{\fIForward, Backward\fP}"
.Pa incl LISTofELEMENT_TYPE
.Pa excl LISTofELEMENT_TYPE
.Re
.Pa status "{\fIFound, NotFound\fP}"
.Pa found_offset CARD32
.Se
Structure, Value
.Ds
This request conducts a search
for the first occurrence of the specified
element type in the structure specified by \fIs_id\fP.
The offset at which to begin searching is computed using the
\fIwhence\fP and \fIoffset\fP found in \fIposition\fP.
If \fIwhence\fP is \fIBeginning\fP, the search will begin at
the element specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the specified \fIoffset\fP
is added to the current value of \fIs_id\fP's element pointer to determine
the element at which to begin the search.
If \fIwhence\fP is \fIEnd\fP, the element at which to begin the search
is computed by adding the specified \fIoffset\fP
to the offset of the last element in the structure.
\fIOffset\fP can be a negative number.
.LP
If the computed offset is less than zero, the search will begin at
the position preceding the first element in the structure.
If the computed offset
is greater than the
number of elements in the structure, then the search will begin at
the last element in the structure.
The search always includes the starting element.
.LP
An element will be selected if its element type is not contained in
\fIexcl\fP
and its element type is included in 
\fIincl\fP.
An element type of \fIAll\fP causes all element types to match.
If a
structure element type is in both the inclusion and exclusion sets, it will
be excluded.
.LP
The search terminates if a match is found or
if the limits of the structure are reached.  The search progresses from
the start point and proceeds either forward in the structure or backward,
depending on \fIdirection\fP.
This is a non-descending search (i.e., the search does not include
any structures referenced by "execute structure" elements).
If the search succeeds in finding a match, a status of \fIFound\fP is
returned, and the offset of the matching element is returned in
\fIfound_offset\fP.
If the search is unsuccessful, a status of \fINotFound\fP is
returned, and a value of zero is returned in
\fIfound_offset\fP.
.LP
The element pointer attribute of \fIs_id\fP is not modified as a
result of this request.
.Fe
.bp

.AC "Structure Editing" 2
.LP
This section contains requests that can be used to modify structure resources.


.AC "Store Elements" 3
.Fs
.Na PEXStoreElements
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa s_id STRUCTURE_ID
.Pa elements LISTofOUTPUT_CMD
.Se
Structure, FloatingPointFormat, OutputCommand
.Ds       
This request causes elements to be stored in the
structure whose resource ID is specified by \fIs_id\fP.
If the \fIediting_mode\fP attribute of the structure is
set to \fIStructureInsert\fP, each entry
in \fIelements\fP is used to create a structure element
in the structure after the element specified
by the structure's \fIelement_ptr\fP attribute.
Output commands in the request are processed until one is found to be in
error, or until the entire list has been processed.
The erroneous output command
and all others following it in the request are ignored
(they will \fInot\fP be added to the structure), and
an \fIOutputCommand\fP error is returned to the client.
The element pointer is
incremented by one for each such insertion.  Therefore,
at the conclusion of the request, the element pointer is
left pointing at the last inserted element.
.LP
If the \fIediting_mode\fP attribute is
set to \fIStructureReplace\fP, a range of elements is
first deleted from the structure as if \fBPEXDeleteElements\fP
were called with a \fIposition1\fP whence/offset of {\fICurrent\fP, 0}
and a \fIposition2\fP whence/offset of {\fICurrent\fP, \fIn\fP\|-\|1}
where \fIn\fP is the number of elements to be deleted.
(If element zero is included as part of the range to be deleted,
all elements in the range except for element zero will be deleted.)
If there are fewer elements between
the structure's element pointer and the end of the structure
than there are output commands in \fIelements\fP, then
only the elements between the element pointer and the
end of the structure will be deleted.
The element pointer
will be left pointing at the element preceding the deleted
range.
After the deletion,
the output commands specified in \fIelements\fP are 
inserted at the element pointer position as described above.
.LP
Floating point values in \fIelements\fP will be in the floating
point format specified in \fIfp_format\fP.
The formats for the various types of output commands
can be found in the "Output Commands" section.
.Fe
.bp

.AC "Delete Elements" 3
.Fs
.Na PEXDeleteElements
.Rq
.Pa s_id STRUCTURE_ID
.Pa range ELEMENT_RANGE
.Se
Structure, Value
.Ds                              
This request
deletes elements in \fIrange\fP from
the structure whose ID is specified in \fIs_id\fP.
The range of elements to be deleted is computed using
the \fIwhence\fP and \fIoffset\fP values of \fIposition1\fP and
\fIposition2\fP.
.LP
The offsets that define the range of elements to be deleted
are each computed using the positions in \fIrange\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIs_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If either computed offset is less than zero, it will be set to zero before
the deletion occurs.  If either computed offset is greater than the number
of elements in the structure, it will be set to the offset
of the last structure element.
It is not necessary for the first offset to be less than the second.
The deletion is inclusive, meaning that
the elements at the boundary of the deletion range are also deleted.
It does not matter which of the two offsets determines the lower end
of the range to be deleted and which determines the higher end of the
range.
Deleting element zero
is effectively a noop.
After the deletion operation, the structure's element pointer
will be set to the element
preceding the range of deleted elements.
.Fe
.bp


.AC "Delete Elements To Label" 3
.Fs
.Na PEXDeleteElementsToLabel
.Rq
.Pa s_id STRUCTURE_ID
.Pa position ELEMENT_POS
.Pa label INT32
.Se
Structure, Label, Value
.Ds
This request deletes all elements between a computed offset and
a specified \fIlabel\fP in the structure whose ID is specified in \fIs_id\fP.
The offset is computed using \fIposition\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIs_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If the computed offset is less than zero, it will be set to zero before
the deletion occurs.  If the computed offset is greater than the number
of elements in the structure, it will be set to the offset
of the last structure element.
Elements are deleted starting at the element immediately after the computed
offset up to the
next occurrence of the \fIlabel\fP.
The label itself is not deleted.  The
structure's element pointer is set to point at the element immediately
preceding the range of deleted elements.
If the specified label is not found, no deletion occurs and the
structure's element pointer is left unchanged.
.Fe

.AC "Delete Elements Between Labels" 3
.Fs
.Na PEXDeleteElementsBetweenLabels
.Rq
.Pa s_id STRUCTURE_ID
.Pa label1 INT32
.Pa label2 INT32
.Se
Structure, Label
.Ds
This request
deletes all elements between \fIlabel1\fP and \fIlabel2\fP
in the structure whose ID is specified in \fIs_id\fP.
A search for
\fIlabel1\fP is first performed starting at the current element
pointer position plus one.
A search
for \fIlabel2\fP is then performed, starting at the element after
\fIlabel1\fP.  The range of elements between the two labels is then
deleted.
The two label elements themselves are not deleted.  The
structure's element pointer is set to point at the element immediately
preceding the range of deleted elements (the element containing \fIlabel1\fP).
.LP
If either of the two specified labels is not found between starting
point of the search and the end of the structure,
no deletion occurs and the
structure's element pointer is left unchanged.
.Fe
.bp


.AC "Copy Elements" 3
.Fs
.Na PEXCopyElements
.Rq
.Pa src_s_id STRUCTURE_ID
.Pa src_range ELEMENT_RANGE
.Pa dest_s_id STRUCTURE_ID
.Pa dest_position ELEMENT_POS
.Se
Structure, Value
.Ds       
This request copies elements in \fIsrc_range\fP
from the structure specified in \fIsrc_s_id\fP
to the \fIdest_position\fP in the structure specified in \fIdest_s_id\fP.
The range of elements to be copied is computed using
the \fIwhence\fP and \fIoffset\fP values of \fIposition1\fP and
\fIposition2\fP in \fIsrc_range\fP.
.LP
The offsets that define the range of elements to be copied
are each computed using the positions in
\fIsrc_range\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIsrc_s_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the structure.
\fIOffset\fP can be a negative number.
.LP
If either computed offset is less than zero, it will be set to zero before
the copy occurs.  If either computed offset is greater than the number
of elements in the source structure, it will be set to the offset
of the last structure element in the source structure.
It does not matter which of the two offsets determines the lower end
of the range to be copied and which determines the higher end of the
range (i.e., the element order will not be reversed if \fIoffset1\fP is greater
than \fIoffset2\fP).
.LP
The position to which the elements will be copied is computed using
\fIdest_position\fP in the following fashion.
If \fIwhence\fP is \fIBeginning\fP, the computed destination offset is just
the value specified by \fIoffset\fP.
If \fIwhence\fP is \fICurrent\fP, the offset is computed by adding
\fIoffset\fP to \fIdest_s_id\fP's element pointer.
If \fIwhence\fP is \fIEnd\fP, the offset is computed
by adding \fIoffset\fP to the number of elements in the destination structure.
\fIOffset\fP can be a negative number.
.LP
It is permissible for \fIsrc_s_id\fP and \fIdest_s_id\fP to be the same.
If this is the case, the copy operation performs
as if the indicated range is copied to a temporary location,
and then inserted relative to the destination position.
.LP
After the copy operation, the destination structure's element pointer
is updated to point at the last element inserted in the destination
structure.  The editing mode attribute of \fIdest_s_id\fP is ignored
during this request; copied elements are always inserted into the
destination structure, never used to replace existing structure elements.
.Fe
.bp


.AC "Change Structure References" 3
.Fs
.Na PEXChangeStructureReferences
.Rq
.Pa old_s_id STRUCTURE_ID
.Pa new_s_id STRUCTURE_ID
.Se
Structure
.Ds              
This request changes any "execute structure" elements in the server
that reference the structure specified by
\fIold_s_id\fP into "execute structure" elements that 
reference the structure specified by
\fInew_s_id\fP.
.LP
Any references to the structure \fInew_s_id\fP that existed before this
request are not affected.
On all PHIGS workstation resources where \fInew_s_id\fP is already
posted, it remains posted and \fIold_s_id\fP, if posted there, is unposted.
On all PHIGS workstation resources where \fInew_s_id\fP is not already
posted and \fIold_s_id\fP is posted, \fInew_s_id\fP is posted with the
same priority as \fIold_s_id\fP and \fIold_s_id\fP is unposted.
If there were references to the structure
resource specified by \fIold_s_id\fP and the structure resource specified by
\fInew_s_id\fP does not exist, an error is returned and no action is taken.
.Fe
.bp                                                     
.AC "Name Sets" 1
.LP
.RU
.LP
A \fIname set\fP is a PEX resource that is used to filter output
commands during rendering, picking, and searching operations.
Name sets typically make fairly small demand on server memory resources,
but they are of variable size
so this can vary depending on their contents.

.AC "Name Set Resource Management" 2
.LP                   
The name set is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
freeing, and copying of name set resources.

.AC "Create Name Set" 3
.Fs
.Na PEXCreateNameSet
.Rq      
.Pa ns_id NAME_SET_ID
.Se
IDChoice, Alloc
.Ds                     
This request creates a name set resource for the specified
\fIns_id\fP.  The name set is initialized to an empty list when
the resource is created.
.Fe




.AC "Copy Name Set" 3
.Fs
.Na PEXCopyNameSet
.Rq                 
.Pa src_ns_id NAME_SET_ID
.Pa dest_ns_id NAME_SET_ID
.Se
NameSet
.Ds            
This request copies the source name set
\fIsrc_ns_id\fP to a destination name set \fIdest_ns_id\fP
after first emptying the contents of \fIdest_ns_id\fP.
The \fIdest_ns_id\fP must already exist as a valid resource.
.Fe
.bp


.AC "Free Name Set" 3
.Fs
.Na PEXFreeNameSet
.Rq                 
.Pa ns_id NAME_SET_ID
.Se
NameSet
.Ds            
This request deletes the association between the resource ID
and the name set.  The name set storage will be
freed when no other resource references it.
.Fe
.bp

.AC "Name Set Inquiry" 2
.LP
These requests inquire the contents of name set resources.

.AC "Get Name Set" 3
.Fs
.Na PEXGetNameSet
.Rq
.Pa ns_id NAME_SET_ID
.Re
.Pa names LISTofNAME
.Se
NameSet
.Ds       
This request will return the contents of the name set specified by
\fIns_id\fP.
.Fe




.AC "Name Set Modification" 2
.LP
These requests can be used to modify the contents of name set resources.

.AC "Change Name Set" 3
.Fs
.Na PEXChangeNameSet
.Rq
.Pa ns_id NAME_SET_ID
.Pa action "{\fIReplace, Add, Remove\fP}"
.Pa names LISTofNAME
.Se
NameSet, Value
.Ds       
This request changes the contents of a name set resource.
If \fIaction\fP is \fIAdd\fP, the specified list of names
is added to the name set.
If \fIaction\fP is \fIRemove\fP, the specified list of names
is removed from the name set.
If \fIaction\fP is \fIReplace\fP, all the names in the name set are
removed, then the specified list of names
is added to the name set.  If an attempt is
made to remove a name which is not in the nameset, then the
name will be ignored and no error will be generated.
.Fe
.bp
.AC "Search Contexts" 1
.LP
.RU
.LP
A \fIsearch context\fP is a PEX resource that allows clients
to perform searching operations on structure networks.
This section
describes the operations that can be performed
on search context resources
and the operations that can be performed using search context resources.
PEX currently provides no support for searching of client-side
structure networks.
.LP
Some of the requests in this section affect attributes of a search
context.
The \fIitem_mask\fP and \fIitem_list\fP parameters
specify which components are to be affected.
Each bit in the \fIitem_mask\fP indicates whether or not
the corresponding attribute is affected.  In the cases where
search context attributes are being set or queried,
there is a corresponding entry in the \fIitem_list\fP
for each set bit in \fIitem_mask\fP.
It is therefore possible
to affect one or many search context attributes with a single request.
.LP
Two of the search context attributes contain name set resource IDs.
If a name set is created, bound to a search context, and then freed,
the contents of the name set will remain, since
it is still being referenced by the search context.  However, when a
search context is queried, the value \fIAlreadyFreed\fP will
be returned for the name set ID, since it no longer
has a valid resource ID by which it can be referenced.
.LP
When a text primitive or annotation text primitive is encountered
during a search operation, only the origin of the text string is used
to determine if the search was successful.\(dg
.FS
\(dg The criteria of closeness to the origin matches PHIGS semantics for
incremental spatial search on annotation text.  However, PHIGS specifies that
the enclosing rectangle of text primitives be used when searching.
The traversal-time
values of the geometric text attributes, together with text font 1, text
precision \fIStroke\fP, character expansion factor 1 and character spacing
0, determine the spatial extent of the enclosing rectangle.
At this time, PEX does not follow the PHIGS semantics for searching on text
primitives.
In order to release the current PEX documents in a timely manner,
this issue has been deferred until a future version of PEX.
.FE
.LP
The search context components, in order, are listed in the following table.
.ID
.ta 0.05i 2.1i 3.5i
	\fBAttribute Name	Data Type	Default Value\fP
.ta
.ta 0.2i 1.9i 3.7i
	search_pos	COORD_3D	(0.0,0.0,0.0)
	search_dist	FLOAT	0.0
	search_ceiling	CARD16	1
	model_clip_flag	BOOLEAN	\fIFalse\fP
	start_path	LISTofELEMENT_REF	\fINull\fP
	normal_list	LISTofNAME_SET_PAIR	\fINull\fP
	inverted_list	LISTofNAME_SET_PAIR	\fINull\fP
.ta
.DE
The attributes of the search context resource are defined as follows:

.Bl "search_pos"
This attribute specifies the search reference position in world coordinates.

.Bl "search_dist"
This attribute specifies a distance from the search reference position
in world coordinates.
A successful search occurs only when an output primitive element is found
that satisfies the search filter criteria \fIand\fP is within the
specified distance from the search reference point.  In order to satisfy
the search criteria if \fIsearch_dist\fP is less than or equal to zero,
the primitive must intersect the \fIsearch_pos\fP.

.Bl "search_ceiling"
This attribute defines the ceiling of the search operation.
The search ceiling is an index into the list contained in \fIstart_path\fP
and searching stops when the end of the structure specified by
\fIsearch_ceiling\fP is reached.
For example, if there are \fIn\fP items in \fIstart_path\fP, and 
\fIsearch_ceiling\fP is set to \fIn\fP, the search traversal will
terminate at the end of the last structure in the list.  On the other hand, if
\fIsearch_ceiling\fP is equal to one, then the search will effectively 
operate without a ceiling, terminating at the end of the first structure 
in \fIstart_path\fP.

.Bl "model_clip_flag"
This attribute specifies whether modeling clipping must be performed
during the search operation.  If \fITrue\fP, modeling clipping is
performed using the modeling clipping attributes as they are encountered
during the traversal.  If \fIFalse\fP, no modeling clipping is performed
and modeling clipping attributes that are encountered during the traversal
are effectively ignored.

.Bl "start_path"
This attribute defines the structure network path that is to be used
as the starting point for the search.  Searching begins at the element
\fIfollowing\fP the one indicated by the start path.

.Bl "normal_list"
This attribute contains the list of name set resource ID pairs to
be used as filters in the search operation.  If the \fInormal_list\fP
is \fINull\fP, all name sets are considered accepted by the normal
filter list.

.Bl "inverted_list"
This attribute contains the list of name set resource ID pairs to
be inverted and used as filters in the search operation.
If the \fIinverted_list\fP
is \fINull\fP, all name sets are considered accepted by the inverted
filter list.
.bp
.bp

.AC "Search Context Resource Management" 2
.LP                   
The search context is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
freeing, and copying of search contexts.

.AC "Create Search Context" 3
.Fs
.Na PEXCreateSearchContext
.Rq      
.Pa fp_format FLOAT_FORMAT
.Pa sc_id SEARCH_CONTEXT_ID
.Pa item_mask BITMASK
.Pa item_list LISTofVALUE
.Se
IDChoice, Value, FloatingPointFormat, Alloc, Path, NameSet
.Ds                     
This request creates a search context resource for the specified
\fIsc_id\fP.  
The \fIitem_mask\fP defines those search context attributes
that are to be explicitly set at the time the resource is created.
The \fIitem_list\fP 
contains the corresponding
list of values used to modify the newly-created search context.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe




.AC "Copy Search Context" 3
.Fs
.Na PEXCopySearchContext
.Rq                 
.Pa src_sc_id SEARCH_CONTEXT_ID
.Pa dest_sc_id SEARCH_CONTEXT_ID
.Pa item_mask BITMASK
.Se
SearchContext, Value
.Ds            
This request copies the source search context
\fIsrc_sc_id\fP to a destination search context \fIdest_sc_id\fP.
The \fIdest_sc_id\fP must already exist as a valid resource.
The \fIitem_mask\fP     
indicates which values
in the search context will be copied.
.Fe
.bp


.AC "Free Search Context" 3
.Fs
.Na PEXFreeSearchContext
.Rq                 
.Pa sc_id SEARCH_CONTEXT_ID
.Se
SearchContext
.Ds            
This request
deletes the specified search context resource and frees the storage
associated with it.
.Fe
.bp
.AC "Search Context Inquiry" 2
.LP
The requests in this section can be used to
inquire search context attributes.

.AC "Get Search Context" 3
.Fs
.Na PEXGetSearchContext
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa sc_id SEARCH_CONTEXT_ID
.Pa item_mask BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
SearchContext, FloatingPointFormat, Value
.Ds       
This request will return components of
the search context specified by \fIsc_id\fP.
The \fIitem_mask\fP specifies which components are to be inquired and returned.
The specified attributes of the search context will be returned in
\fIitem_list\fP.
Floating point values in \fIitem_list\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe

.AC "Search Context Modification" 2
.LP
The requests in this section can be used to
modify attributes of search context resources.

.AC "Change Search Context" 3
.Fs
.Na PEXChangeSearchContext
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa sc_id SEARCH_CONTEXT_ID
.Pa item_mask BITMASK
.Pa item_list LISTofVALUE
.Se
SearchContext, Value, FloatingPointFormat, Path, NameSet
.Ds       
This request changes components of a search context.  
The \fIitem_mask\fP and \fIitem_list\fP
specify which components are to be changed.
Each bit in the \fIitem_mask\fP indicates whether or not
there is a corresponding entry in the \fIitem_list\fP.
It is therefore possible
to modify one or many search context attributes with a
\fBPEXChangeSearchContext\fP request.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe
.bp

.AC "Structure Network Searching" 2
.LP
This section describes requests that use search context resources to
perform structure network searching operations.

.AC "Search Network" 3
.Fs
.Na PEXSearchNetwork
.Rq
.Pa sc_id SEARCH_CONTEXT_ID
.Re
.Pa found_path LISTofELEMENT_REF
.Se
SearchContext, Path
.Ds       
This request causes a spatial search in world coordinates
to be performed on a structure
network.  The parameters of the searching operation are found in the
search context \fIsc_id\fP.
The search begins at the element \fIfollowing\fP
the one indicated by the \fIstart_path\fP attribute of \fIsc_id\fP.
The search is terminated once the end of the structure indicated
by \fIsearch_ceiling\fP has been reached.
The first element that meets the search criteria is returned in
\fIfound_path\fP.  If no element is found, \fIfound_path\fP will be
null.  After the search has completed, the \fIstart_path\fP attribute
of \fIsc_id\fP will be set to the value that is returned in \fIfound_path\fP.

For a structure element to be considered a candidate for the search,
the current name set has to be accepted by all the name set pairs in the
search context's \fInormal_list\fP, and has to be rejected by
all of the name set pairs in the \fIinverted_list\fP.  If the
\fInormal_list\fP is \fINull\fP, all possible name sets are accepted
by the normal list.
If the \fIinverted_list\fP is \fINull\fP, all possible name sets are
accepted by the inverted list.  Therefore the default case is that
all output primitives are considered.
.Fe
.bp

.AC "PHIGS Workstations" 1
.LP
.RU
.LP
A \fIPHIGS workstation\fP is a PEX resource that combines
other resources into a single entity that behaves in a manner
similar to the PHIGS abstraction of a "workstation".
The PHIGS workstation has built into it all of the capabilities
of the renderer resource described earlier.  A PHIGS workstation,
with its lookup tables and name sets, effectively contains an
instance of a renderer, but it also contains functionality above and
beyond that found in a renderer resource.
.LP
This section
describes the operations that can be performed
on PHIGS workstation resources
and the operations that can be performed using PHIGS workstation resources.
In addition to the
attributes listed below, the PHIGS workstation resource also
contains an implementation-dependent number of 
pick device descriptors that support picking operations.
These can be set and queried with
requests found in the next section, "Picking".
.LP
There are several differences between PHIGS workstations and renderers.
Due to the desire to match PHIGS semantics for dealing with views,
a PHIGS workstation resource has a built-in view table that can only
be accessed through the defined PHIGS workstation requests.  However,
this view table is functionally the same as that used by a renderer,
and information about it may be obtained by using the lookup table
requests to obtain information about tables of type \fIView\fP.
A PHIGS workstation resource that has its associated drawable destroyed will
be freed implicitly.
.LP
Lookup table and name set resources can be bound to a PHIGS
workstation resource
and then freed.  The contents of these resources will remain, since
they are still being referenced by the PHIGS workstation.  However, when a
PHIGS workstation's attributes are queried, the value \fIAlreadyFreed\fP will
be returned for those resources that have been freed and thus no longer
have a valid resource ID by which they can be referenced.
.bp
.AC "PHIGS Workstation Resource Management" 2
.LP                   
The PHIGS workstation is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the creation,
freeing, and copying of PHIGS workstation resources.
.AC "Create PHIGS Workstation" 3
.Fs
.Na PEXCreatePhigsWKS
.Rq 
.Pa wks_id PHIGS_WKS_ID
.Pa drawable_id DRAWABLE_ID
.Pa marker_bundle LOOKUP_TABLE_ID
.Pa text_bundle LOOKUP_TABLE_ID
.Pa line_bundle LOOKUP_TABLE_ID
.Pa interior_bundle LOOKUP_TABLE_ID
.Pa edge_bundle LOOKUP_TABLE_ID
.Pa color_table LOOKUP_TABLE_ID
.Pa depth_cue_table LOOKUP_TABLE_ID
.Pa light_table LOOKUP_TABLE_ID
.Pa color_approx_table LOOKUP_TABLE_ID
.Pa pattern_table LOOKUP_TABLE_ID
.Pa text_font_table LOOKUP_TABLE_ID
.Pa highlight_inclusion NAME_SET_ID
.Pa highlight_exclusion NAME_SET_ID
.Pa invisibility_inclusion NAME_SET_ID
.Pa invisibility_exclusion NAME_SET_ID
.Pa buffer_mode BUFFER_MODE
.Se
IDChoice, Drawable, Match, LookupTable, NameSet, Alloc, Value
.Ds                     
This request creates a PHIGS workstation resource for the specified
\fIwks_id\fP.  
The window or pixmap specified by \fIdrawable_id\fP is associated with
the newly-created PHIGS workstation resource.
The named tables and name sets are also bound to the PHIGS workstation
resource for use during rendering.
A view table that supports current and requested view table entries
is allocated for the PHIGS workstation automatically at creation time.
The requests \fBPEXSetViewRep\fP and \fBPEXGetViewRep\fP can be used
to modify and query the PHIGS workstation view table.
The \fIbuffer_mode\fP attribute is used to specify whether the workstation
will operate in single-buffered or double-buffered mode.  If double-buffered,
an additional image buffer will be created for the drawable in an
implementation-dependent fashion in order to support double-buffering.
If \fIdrawable_id\fP is a pixmap, then no double buffering will be
performed.
.Fe
.bp

.AC "Free PHIGS Workstation" 3
.Fs
.Na PEXFreePhigsWKS
.Rq                 
.Pa wks_id PHIGS_WKS_ID
.Se
PhigsWKS
.Ds            
This request deletes the
PHIGS workstation resource and any image buffers that were created
by the PEX extension in order to support double-buffering.
The storage associated with the resource is then freed as well.
.Fe
.bp

.AC "PHIGS Workstation Inquiry" 2
.LP
This section defines requests that can be used to get information
about a PHIGS workstation resource.

.AC "Get PHIGS Workstation Info" 3
.Fs
.Na PEXGetWKSInfo
.Rq                 
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa item_mask WKS_BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
PhigsWKS, FloatingPointFormat, Value
.Ds
This request returns attributes of the PHIGS workstation
resource indicated by \fIwks_id\fP.  Floating point values will be returned
in the floating point format specified by \fIfp_format\fP.  The parameter
\fIitem_mask\fP indicates which attributes are to be returned.  For each
bit that is set in \fIitem_mask\fP, a corresponding value is returned
in \fIitem_list\fP.
.LP
In the table below, the abbreviation
"create wks req." indicates that the parameter is set by the
\fBPEXCreatePhigsWKS\fP request.
The abbreviation
"imp. dep." means that the default value is implementation-dependent.
The attributes that may be returned, in order, are:
.ID
.ta 0.2i 1.9i 3.5i
	\fBAttribute Name	Data Type	Default\fP
.ta
.ta 0.3i 1.7i 3.3i
	display_update	DISPLAY_UPDATE	\fIVisualizeEach\fP
	visual_state	VISUAL_STATE	\fICorrect\fP
	display_surface	DISPLAY_STATE	\fIEmpty\fP
	view_update	UPDATE_STATE	\fINotPending\fP
	defined_views	LISTofTABLE_INDEX	View 0 defined
	wks_update	UPDATE_STATE	\fINotPending\fP
	req_NPC_subvolume	NPC_SUBVOLUME	(0.0,0.0,0.0),(1.0,1.0,1.0)
	cur_NPC_subvolume	NPC_SUBVOLUME	(0.0,0.0,0.0),(1.0,1.0,1.0)
	req_wks_viewpt	VIEWPORT	{imp. dep., imp. dep., True}
	cur_wks_viewpt	VIEWPORT	{imp. dep., imp. dep., True}
	HLHSR_update	UPDATE_STATE	\fINotPending\fP
	req_HLHSR_mode	HLHSR_MODE	1
	cur_HLHSR_mode	HLHSR_MODE	1
	drawable_id	DRAWABLE_ID	create wks req.
	marker_bundle	LOOKUP_TABLE_ID	create wks req.
	text_bundle	LOOKUP_TABLE_ID	create wks req.
	line_bundle	LOOKUP_TABLE_ID	create wks req.
	interior_bundle	LOOKUP_TABLE_ID	create wks req.
	edge_bundle	LOOKUP_TABLE_ID	create wks req.
	color_table	LOOKUP_TABLE_ID	create wks req.
	depth_cue_table	LOOKUP_TABLE_ID	create wks req.
	light_table	LOOKUP_TABLE_ID	create wks req.
	color_approx_table	LOOKUP_TABLE_ID	create wks req.
	pattern_table	LOOKUP_TABLE_ID	create wks req.
	text_font_table	LOOKUP_TABLE_ID	create wks req.
	highlight_inclusion	NAME_SET_ID	create wks req.
	highlight_exclusion	NAME_SET_ID	create wks req.
	invisibility_inclusion	NAME_SET_ID	create wks req.
	invisibility_exclusion	NAME_SET_ID	create wks req.
	posted_structs	LISTofSTRUCTURE_INFO	 \fINull\fP
	num_priorities	CARD32	imp. dep.
	buffer_update	UPDATE_STATE	\fINotPending\fP
	req_buffer_mode	BUFFER_MODE	create wks req.
	cur_buffer_mode	BUFFER_MODE	create wks req.
.DE

.Bl "display_update"
Returns the PHIGS workstation's current display update mode.  This mode
specifies how the PHIGS workstation attempts to visualize changes,
and can be set with the \fBPEXSetDisplayUpdateMode\fP request.

.Bl "visual_state"
Returns the PHIGS workstation's current visual state.  The visual state
will be \fICorrect\fP if there are no deferred actions and no changes have
been simulated on the display surface.
The visual state will be \fIDeferred\fP if there are deferred
actions, or if there are deferred actions and some changes have
been simulated on the display surface.
The visual state will be \fISimulated\fP if there are no deferred
actions, but some changes have
been simulated on the display surface.

.Bl "display_surface"
Returns the current status of the PHIGS workstation's display surface.
The display surface will be \fIEmpty\fP if nothing has been rendered on the
drawable, \fINotEmpty\fP otherwise.

.Bl "view_update"
Returns \fIPending\fP or \fINotPending\fP, depending on whether there are
view modification requests that have yet to be made current.

.Bl "defined_views"
Returns the list of view table indices that are defined in the PHIGS
workstation's current view table.
The list of defined view indices will be returned
in the order of view transformation input priority.

.Bl "wks_update"
Returns \fIPending\fP or \fINotPending\fP, depending on whether there are
workstation transformation requests that have yet to be made current.

.Bl "req_NPC_subvolume"
Returns the value for the PHIGS workstation's NPC subvolume specification
that has been requested but has not yet been made current.

.Bl "cur_NPC_subvolume"
Returns the current value for the PHIGS workstation's NPC
subvolume specification.

.Bl "req_wks_viewpt"
Returns the value for the PHIGS workstation's viewport specification
that has been requested but has not yet been made current.

.Bl "cur_wks_viewpt"
Returns the current value for the PHIGS workstation's viewport specification.

.Bl "HLHSR_update"
Returns \fIPending\fP or \fINotPending\fP, depending on whether there are
HLHSR mode requests that have yet to be made current.

.Bl "req_HLHSR_mode"
Returns the value for the PHIGS workstation's HLHSR mode
that has been requested but has not yet been made current.

.Bl "cur_HLHSR_mode"
Returns the current value for the PHIGS workstation's HLHSR mode.

.Bl "drawable_id"
Returns the resource ID of the drawable that is being used as the
display surface.

.Bl "marker_bundle"
Returns the resource ID of the PHIGS workstation's marker bundle table.

.Bl "text_bundle"
Returns the resource ID of the PHIGS workstation's text bundle table.

.Bl "line_bundle"
Returns the resource ID of the PHIGS workstation's line bundle table.

.Bl "interior_bundle"
Returns the resource ID of the PHIGS workstation's interior bundle table.

.Bl "edge_bundle"
Returns the resource ID of the PHIGS workstation's edge bundle table.

.Bl "color_table"
Returns the resource ID of the color lookup table that will be used
by the PHIGS workstation to dereference indexed color values.

.Bl "depth_cue_table"
Returns the resource ID of the PHIGS workstation's depth-cue table.

.Bl "light_table"
Returns the resource ID of the PHIGS workstation's light table.

.Bl "color_approx_table"
Returns the resource ID of the color approximation table that
will be used by the PHIGS workstation.

.Bl "pattern_table"
Returns the resource ID of the PHIGS workstation's pattern table.

.Bl "text_font_table"
Returns the resource ID of the PHIGS workstation's text font table.

.Bl "highlight_inclusion"
Returns the resource ID of the PHIGS workstation's highlight inclusion
name set.

.Bl "highlight_exclusion"
Returns the resource ID of the PHIGS workstation's highlight exclusion
name set.

.Bl "invisibility_inclusion"
Returns the resource ID of the PHIGS workstation's invisibility inclusion
name set.

.Bl "invisibility_exclusion"
Returns the resource ID of the PHIGS workstation's invisibility exclusion
name set.

.Bl "posted_structs"
Returns the resource ID and associated priority of each of the
structures in the PHIGS workstation's
posted structure list.
The list will be returned in structure priority order.

.Bl "num_priorities"
Returns the number of display priorities that are supported.  A value
of zero indicates that a continuous range of priorities is supported.

.Bl "buffer_update"
Returns \fIPending\fP or \fINotPending\fP, depending on whether there are
buffer mode requests that have yet to be made current.

.Bl "req_buffer_mode"
Returns the value for the PHIGS workstation's buffer mode
that has been requested but has not yet been made current.

.Bl "cur_buffer_mode"
Returns the value for the PHIGS workstation's
buffer mode, either \fISingle\fP or \fIDouble\fP.  If
the drawable associated with the PHIGS workstation resource is a pixmap,
then no double-buffering will be performed.

.Fe
.bp

.AC "Get Dynamics" 3
.Fs
.Na PEXGetDynamics
.Rq                 
.Pa drawable_id DRAWABLE_ID
.Re
.Pa view_rep DYNAMIC_TYPE
.Pa marker_bundle DYNAMIC_TYPE
.Pa text_bundle DYNAMIC_TYPE
.Pa line_bundle DYNAMIC_TYPE
.Pa interior_bundle DYNAMIC_TYPE
.Pa edge_bundle DYNAMIC_TYPE
.Pa color_table DYNAMIC_TYPE
.Pa pattern_table DYNAMIC_TYPE
.Pa wks_transform DYNAMIC_TYPE
.Pa highlight_filter DYNAMIC_TYPE
.Pa invisibility_filter DYNAMIC_TYPE
.Pa HLHSR_mode DYNAMIC_TYPE
.Pa structure_modify DYNAMIC_TYPE
.Pa post_structure DYNAMIC_TYPE
.Pa unpost_structure DYNAMIC_TYPE
.Pa delete_structure DYNAMIC_TYPE
.Pa reference_modify DYNAMIC_TYPE
.Pa buffer_modify DYNAMIC_TYPE
.Pa light_table DYNAMIC_TYPE
.Pa depth_cue_table DYNAMIC_TYPE
.Pa color_approx_table DYNAMIC_TYPE
.Se
Drawable, Match
.Ds
This request returns information about the dynamics that
are supported by PHIGS workstations associated with drawables
of the type specified by \fIdrawable_id\fP.
The list of dynamics, in order, is as follows:
.DS
.ta 0.2i 2.0i
	\fBAttribute Name	Description\fP
.ta
.ta 0.3i 1.5i
	view_rep	Changes to view table
	marker_bundle	Changes to marker bundle
	text_bundle	Changes to text bundle
	line_bundle	Changes to line bundle
	interior_bundle	Changes to interior bundle
	edge_bundle	Changes to edge bundle
	color_table	Changes to color table
	pattern_table	Changes to pattern table
	wks_transform	Changes to workstation transformations
	highlight_filter	Changes to highlight name set
	invisibility_filter	Changes to invisibility name set
	HLHSR_mode	Changes to HLHSR mode
	structure_modify	Structure modifications (edits)
	post_structure	Additions to posted structure list
	unpost_structure	Deletions from posted structure list
	delete_structure	Deletion of structure resources
	reference_modify	Structure reference modifications
	buffer_modify	Changes to buffering mode
	light_table	Changes to light table
	depth_cue_table	Changes to depth cue table
	color_approx_table	Changes to color approximation table
.ta
.DE
The value returned for each of the items in the list above can
be one of \fIIMM, IRG,\fP or \fICBS\fP, where
\fIIMM\fP
means that the specified action can be performed and the correct
image displayed immediately,
\fIIRG\fP
means that the specified action requires a regeneration of the image,
and \fICBS\fP
means that the specified action can be simulated immediately if the PHIGS
workstation's \fIdisplay_update\fP mode is set to \fISimulateSome\fP.
.Fe
.bp


.AC "Get View Representation" 3
.Fs
.Na PEXGetViewRep
.Rq                 
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa index TABLE_INDEX
.Re
.Pa view_update "{Pending, NotPending}"
.Pa requested VIEW_REP
.Pa current VIEW_REP
.Se
PhigsWKS, FloatingPointFormat, Value
.Ds
This request returns
the value of the view update state and
the specified entries in the requested and current view
tables in the PHIGS workstation specified by \fIwks_id\fP.
The \fIview_update\fP will be \fIPending\fP
if a view change has been requested but not established.
If the specified entry is not defined, an error will
be generated and the contents of the reply parameters
will be undefined.  Floating point values will be returned
in the floating point format specified by \fIfp_format\fP.
.Fe
.bp
	
.AC "PHIGS Workstation Manipulation" 2
.LP
This section contains requests that modify PHIGS workstation resources.

.AC "Redraw All Structures" 3
.Fs
.Na PEXRedrawAllStructures
.Rq
.Pa wks_id PHIGS_WKS_ID
.Se
PhigsWKS
.Ds       
This request redraws all the posted structures contained in the
PHIGS workstation resource specified by \fIwks_id\fP.
First, if the PHIGS workstation's \fIdisplay_surface\fP attribute
is \fINotEmpty\fP,
its drawable is cleared to the color stored in
entry zero of its color lookup table.
Then, if any of its \fIview_update\fP, \fIwks_update\fP, \fIHLHSR_update\fP,
or \fIbuffer_update\fP
attributes is set to \fIPending\fP, the requested values are made
the current values and the attribute is set to \fINotPending\fP.
After this, all the posted structures are traversed
and rendered (in priority order).
Finally, the PHIGS workstation's \fIvisual_state\fP attribute is set
to \fICorrect\fP.
.Fe



.AC "Update Workstation" 3
.Fs
.Na PEXUpdateWorkstation
.Rq
.Pa wks_id PHIGS_WKS_ID
.Se
PhigsWKS
.Ds
This request will perform actions identical to \fBPEXRedrawAllStructures\fP
on the PHIGS workstation specified by \fIwks_id\fP
if its \fIvisual_state\fP attribute
is currently set to \fIDeferred\fP or \fISimulated\fP.
.Fe
.bp


.AC "Redraw Clip Region" 3
.Fs
.Na PEXRedrawClipRegion
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa clip_list LISTofDEVICE_RECT
.Se
PhigsWKS
.Ds
This request will perform actions similar to the \fBPEXRedrawAllStructures\fP
request, except that no PHIGS workstation state attributes are
modified or updated by this request.
Rendering will occur only in the region defined by \fIclip_list\fP.
The color stored in entry zero of the \fIwks_id's\fP
color lookup table is used to clear the region under the rectangles
defined by \fIclip_list\fP.
The rectangles in \fIclip_list\fP should be
non-overlapping, or the graphics results
will be undefined.
(If a z-buffering algorithm is used, only those pixels under
the rectangles in the clip list will have their z values initialized.)
All of the posted structures for \fIwks_id\fP are then redrawn, but
only pixels under the rectangles in \fIclip_list\fP are affected.
Pending changes are \fInot\fP made current by this request, nor is
the \fIvisual_state\fP attribute modified.
.Fe

.AC "Execute Deferred Actions" 3
.Fs
.Na PEXExecuteDeferredActions
.Rq
.Pa wks_id PHIGS_WKS_ID
.Se
PhigsWKS
.Ds       
This request causes all the deferred actions on the PHIGS workstation specified
by \fIwks_id\fP to be executed.
.Fe
.bp

.AC "Set View Priority" 3
.Fs
.Na PEXSetViewPriority
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa index1 TABLE_INDEX
.Pa index2 TABLE_INDEX
.Pa priority "{\fIHigher, Lower\fP}"
.Se
PhigsWKS, Value
.Ds       
This request
sets the relative priorities of entries in \fIwks_id\fP's current view table.
The priority
of view table entry \fIindex1\fP with respect
to view table entry \fIindex2\fP is set to the next
\fIHigher\fP or \fILower\fP priority.
These priorities are used to determine the order in which view
table entries are tested when selecting the inverse viewing transformation
to use for transformation from device coordinates to world coordinates.
.Fe

.AC "Set Display Update Mode" 3
.Fs
.Na PEXSetDisplayUpdateMode
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa display_update DISPLAY_UPDATE
.Se
PhigsWKS, Value
.Ds       
This request sets the \fIdisplay_update\fP
attribute of the PHIGS workstation resource specified by \fIwks_id\fP.
This attribute defines how changes to the display surface will be visualized.
The list of permissible display update modes is
defined in the "Extension Information" section that describes
enumerated types.
.LP
If double buffering is enabled (see \fBPEXSetWKSBufferMode\fP), the display
update mode affects which buffer is rendered into.
If the display update mode is \fIVisualizeNone\fP, 
\fIVisualizeEach\fP or \fIVisualizeWhenever\fP,
output primitives are rendered into the back (undisplayed) buffer while the
structure network is being traversed.  When the traversal is complete, the
front and back buffers are swapped, so the rendered image is displayed.
If the display update mode is \fIVisualizeEasy\fP or \fISimulateSome\fP,
output primitives are always rendered into the front (displayed) buffer.
.Fe
.bp

.AC "Map DC to WC" 3
.Fs
.Na PEXMapDCtoWC
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa dc_points LISTofDEVICE_COORD
.Re
.Pa wc_points LISTofCOORD_3D
.Pa view_index TABLE_INDEX
.Se
PhigsWKS, FloatingPointFormat
.Ds       
This request maps the device coordinate points in \fIdc_points\fP
to the world coordinate points in \fIwc_points\fP
using the PHIGS workstation resource specified
by \fIwks_id\fP.  (The client must convert pointer position values
in drawable coordinates into device coordinates.)
Each view in the PHIGS workstation's current view table, which
has an inverse, is checked to
see if it contains all the specified device coordinate
points. (If the view transform has no inverse then it is not
considered).  The index of the view with the highest
view transformation input priority that contains all of the points is
returned in \fIview_index\fP.  If no view contains all of the points,
the index of the view containing the most points is returned.
The points are transformed to world
coordinates by passing them through the inverse of
the view transform associated with the view index and are returned in
\fIwc_points\fP.  Floating point values are passed and will be returned
in the floating point format specified by \fIfp_format\fP.
Points that are clipped (outside the viewport) will not be transformed
and returned in the \fIwc_points\fP list,
so the number of points returned may be less than the number sent.
.Fe
.AC "Map WC to DC" 3
.Fs
.Na PEXMapWCtoDC
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa wc_points LISTofCOORD_3D
.Pa view_index TABLE_INDEX
.Re
.Pa dc_points LISTofDEVICE_COORD
.Se
PhigsWKS, FloatingPointFormat
.Ds       
This request maps the world coordinate points in \fIwc_points\fP
to the device coordinate points in \fIdc_points\fP using the PHIGS
workstation resource specified by \fIwks_id\fP and the view
specified by \fIview_index\fP.  The points are transformed to
device coordinates by passing them through the view transform
associated with the \fIview_index\fP.
Floating point values sent and received will be in the
floating point format specified by \fIfp_format\fP.
Points that are clipped will not be returned in the \fIdc_points\fP list,
so the number of points returned may be less than the number sent.
.Fe
.bp


.AC "PHIGS Workstation Update" 2
.LP
This section defines requests that can be used to set "requested"
values for PHIGS workstation resources.

.AC "Set View Representation" 3
.Fs
.Na PEXSetViewRep
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa view_rep VIEW_REP
.Se
PhigsWKS, FloatingPointFormat, Alloc
.Ds       
This request sets the requested values of the specified view table entry of the
view table in the PHIGS workstation specified by \fIwks_id\fP
to the view representation indicated by \fIview_rep\fP.
The \fIview_update\fP attribute in the PHIGS workstation is
set to \fIPending\fP if the change cannot be visualized immediately;
otherwise it is set to \fINotPending\fP.  If the \fIview_update\fP
attribute is \fINotPending\fP, the current view table entry
is set to the requested values; otherwise the current values are not
changed until the PHIGS workstation is updated.
.Fe



.AC "Set Workstation Window" 3
.Fs
.Na PEXSetWKSWindow
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa NPC_subvolume NPC_SUBVOLUME
.Se
PhigsWKS, FloatingPointFormat
.Ds       
This request sets the \fIreq_NPC_subvolume\fP
of the PHIGS workstation specified by \fIwks_id\fP
to the values in \fINPC_subvolume\fP.
The \fIwks_update\fP attribute in the PHIGS workstation is
set to \fIPending\fP if the change cannot be visualized immediately;
otherwise it is set to \fINotPending\fP.  If the \fIwks_update\fP
attribute is \fINotPending\fP, the \fIcur_NPC_subvolume\fP 
is set to the requested values; otherwise the current values are not
changed until the PHIGS workstation is updated.
.Fe
.bp

.AC "Set Workstation Viewport" 3
.Fs
.Na PEXSetWKSViewport
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa viewport VIEWPORT
.Se
PhigsWKS, FloatingPointFormat, Value
.Ds      
This request sets the \fIreq_wks_viewpt\fP
of the PHIGS workstation specified by \fIwks_id\fP
to the values in \fIviewport\fP.
The \fIwks_update\fP attribute in the PHIGS workstation is
set to \fIPending\fP if the change cannot be visualized immediately;
otherwise it is set to \fINotPending\fP.  If the \fIwks_update\fP
attribute is \fINotPending\fP, the \fIcur_wks_viewpt\fP 
is set to the requested values; otherwise the current values are not
changed until the PHIGS workstation is updated.
.Fe




.AC "Set HLHSR Mode" 3
.Fs
.Na PEXSetHLHSRMode
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa mode HLHSR_MODE
.Se
PhigsWKS, Value, Alloc
.Ds       
This request sets the \fIreq_HLHSR_mode\fP
of the PHIGS workstation specified by \fIwks_id\fP
to the values in \fImode\fP.
If the PHIGS workstation's \fIdisplay_surface\fP attribute is \fIEmpty\fP,
or if the dynamic modification for its \fIHLHSR_mode\fP is \fIIMM\fP,
its \fIcur_HLHSR_mode\fP attribute is modified with the value contained
in \fImode\fP and its \fIHLHSR_update\fP is set to \fINotPending\fP; otherwise,
its \fIHLHSR_update\fP is set to \fIPending\fP and
the current value is not changed until the PHIGS workstation is updated.
.Fe
.bp


.AC "Set Buffer Mode" 3
.Fs
.Na PEXSetWKSBufferMode
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa buffer_mode BUFFER_MODE
.Se
PhigsWKS, Value, Alloc
.Ds
This request sets the \fIreq_buffer_mode\fP
of the PHIGS workstation specified by \fIwks_id\fP
to the values in \fIbuffer_mode\fP.
If the PHIGS workstation's \fIdisplay_surface\fP attribute is \fIEmpty\fP,
or if the dynamic modification for its \fIbuffer_mode\fP is \fIIMM\fP,
its \fIcur_buffer_mode\fP attribute is modified with the value contained
in \fIbuffer_mode\fP and its \fIbuffer_update\fP is set to
\fINotPending\fP; otherwise,
its \fIbuffer_update\fP is set to \fIPending\fP and
the current value is not changed until the PHIGS workstation is updated.
.LP
\fIBuffer_mode\fP
may be one of the constants \fISingle\fP (rendering is to be single-buffered)
or \fIDouble\fP (rendering is to be double-buffered).
An \fIAlloc\fP error will
be returned if the requested mode is \fIDouble\fP and the server
cannot allocate any more image buffers.
.Fe
.bp


.AC "Posting/Unposting Structures" 2
.LP
This section describes the requests that can be used to post and
unpost structures on a PHIGS workstation resource.

.AC "Post Structure" 3
.Fs
.Na PEXPostStructure
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa s_id STRUCTURE_ID
.Pa priority FLOAT
.Se
PhigsWKS, Structure, FloatingPointFormat
.Ds 
This request adds the structure specified by \fIs_id\fP
to the list of posted structures
in the PHIGS workstation \fIwks_id\fP.  A priority
is also provided to indicate the priority of the newly-posted
structure with respect to the structures
already in the posted structure list.
If multiple structures are posted for display to the same display
space location, the implementation will ensure the display of the
higher priority structure.
If two structures have the same priority, the last posted structure
has the higher priority.  If \fIs_id\fP is not a valid structure resource ID,
the request is ignored, and an error is generated.
.Fe




.AC "Unpost Structure" 3
.Fs
.Na PEXUnpostStructure
.Rq
.Pa wks_id PHIGS_WKS_ID
.Pa s_id STRUCTURE_ID
.Se
PhigsWKS, Structure
.Ds       
This request
removes the structure specified by \fIs_id\fP from 
\fIwks_id\fP's posted structure list.  If \fIs_id\fP is not found in
the posted structure list,
the request is ignored, and an error is generated.
.Fe
.bp

.AC "Unpost All Structures" 3
.Fs
.Na PEXUnpostAllStructures
.Rq
.Pa wks_id PHIGS_WKS_ID
.Se
PhigsWKS
.Ds       
This request
removes all structures from
\fIwks_id\fP's posted structure list.
.Fe




.AC "Get PHIGS Workstation Postings" 3
.Fs
.Na PEXGetWKSPostings
.Rq
.Pa s_id STRUCTURE_ID
.Re
.Pa wks_id LISTofPHIGS_WKS_ID
.Se
Structure
.Ds       
This request returns a list of all of the PHIGS workstation
resources to which
\fIs_id\fP has been posted.
.Fe
.bp
.AC "Workstation Picking" 1
.LP
.RU
.LP
The discussion of workstation picking includes 
\fIpick device descriptors\fP which
are found in PHIGS workstation resources and \fIpick measures\fP.

.AC "Pick Device Descriptors" 2
.LP
Each PHIGS workstation resource maintains a list of \fIpick device\fP
descriptors.  Each entry in this list maintains state
values for a particular type of pick device, such as a mouse or a 3D tablet.
Together, the entries in the pick device descriptor list maintain
state values for all of the pick devices that are supported by the workstation
resource.  This list of values can be set or inquired for each of the
supported pick devices.
.LP
Two of the pick device descriptor components are name set resource IDs.
If a name set is created, bound to a pick device descriptor, and then freed,
the contents of the name set will remain, since
it is still being referenced by the pick device descriptor.  However, when a
pick device descriptor is queried, the value \fIAlreadyFreed\fP will
be returned for the name set ID, since it no longer
has a valid resource ID by which it can be referenced.
.LP
The pick device descriptor components, in order,
are listed in the following table.
The abbreviation
"pick dev. dep." stands for "pick-device-dependent", meaning that the
default value is determined by the pick device type.
The abbreviation "imp. dep."
means that the default value is implementation-dependent.
.LP
.ID
.ta 0.1i 1.7i 3.3i
	\fBAttribute Name	Data Type	Default Value\fP
.ta
.ta 0.2i 1.5i 3.4i
	pick_status	{\fINoPick, Ok\fP}	\fINoPick\fP
	pick_path	LISTofPICK_ELEMENT_REF	\fINull\fP
	pick_path_order	{\fITopFirst, BottomFirst\fP}	\fITopFirst\fP
	pick_inclusion	NAME_SET_ID	\fINull\fP
	pick_exclusion	NAME_SET_ID	\fINull\fP
	pick_data_rec	LISTofCARD8	pick dev. dep.
	prompt_echo_type	PROMPT_ECHO_TYPE	pick dev. dep.
	echo_volume	VIEWPORT	{imp. dep., imp. dep., True}
	echo_switch	{\fINoEcho, Echo\fP}	\fINoEcho\fP
.ta
.DE
.LP
The components of the pick device descriptor are defined as follows:

.Bl "pick_status"
This attribute contains the initial pick status that will be
bound to a pick measure resource.

.Bl "pick_path"
This attribute contains the initial pick path that will be
bound to a pick measure resource.

.Bl "pick_path_order"
This attribute specifies
the order in which elements of the picked path are to be written
into a pick measure resource.  If the order is \fITopFirst\fP,
elements of the pick measure's \fIpicked_path\fP attribute will
be listed in the order they would have been encountered during
a traversal, while if the order is \fIBottomFirst\fP they will
be listed in the opposite order.

.Bl "pick_inclusion"
This attribute specifies the resource ID of the name set resource that is to be
used as the pick inclusion filter during picking operations.

.Bl "pick_exclusion"
This attribute specifies the resource ID of the name set resource that is to be
used as the pick exclusion filter during picking operations.

.Bl "pick_data_rec"
This attribute contains
a pick-device-dependent data record used to initialize a pick measure
resource when it is created.

.Bl "prompt_echo_type"
This attribute
contains an enumerated type value that specifies the prompt/echo type
to be used during picking operations.  The allowable types are described
in the "Extension Information" section.

.Bl "echo_volume"
This attribute specifies
where prompting and/or echoing is to occur.
The default is that the echo volume will be defined to be
the size of the drawable that was bound to the the PHIGS workstation 
resource at the time it was created.

.Bl "echo_switch"
This attribute specifies the
initial echo state for a pick measure.

.LP
The requests in this section allow clients to set and inquire the values of
pick device descriptors that are stored in a PHIGS workstation resource.
.bp

.AC "Get Pick Device Descriptor" 3
.Fs
.Na PEXGetPickDevice
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa dev_type PICK_DEVICE_TYPE
.Pa item_mask BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
PhigsWKS, Value, FloatingPointFormat
.Ds       
This request will return components of
a pick descriptor for the PHIGS workstation resource specified by \fIwks_id\fP.
The descriptor returned will be the currently-defined descriptor for
the pick device of the type specified by \fIdev_type\fP.
The \fIitem_mask\fP specifies which components are to be inquired and
returned.  The specified attributes of the pick device descriptor
will be returned in \fIitem_list\fP.
Floating point values in \fIitem_list\fP will be returned in the floating
point format specified in \fIfp_format\fP.
.Fe


.AC "Change Pick Device Descriptor" 3
.Fs
.Na PEXChangePickDevice
.Rq
.Pa fp_format FLOAT_FORMAT
.Pa wks_id PHIGS_WKS_ID
.Pa dev_type PICK_DEVICE_TYPE
.Pa item_mask BITMASK
.Pa item_list LISTofVALUE
.Se
PhigsWKS, Value, FloatingPointFormat, Path, NameSet
.Ds       
This request will modify components of
a pick descriptor for the PHIGS workstation resource specified by \fIwks_id\fP.
The descriptor to be modified will be the currently-defined descriptor for
the pick device of the type specified by \fIdev_type\fP.
The \fIitem_mask\fP specifies which components are to be changed.
The specified attributes of the pick device descriptor
will be set to the values contained in \fIitem_list\fP.
Floating point values in \fIitem_list\fP will be in the floating
point format specified in \fIfp_format\fP.
.Fe
.bp

.AC "Pick Measure" 2
.LP
A \fIpick measure\fP resource must be created to actually perform
a pick operation.  A pick device type is specified at the time
a pick measure resource is created in order to provide the parameters
for the picking operation.  The pick measure resource accepts input
in the form of input records which are defined for each type of pick
device.  When a pick measure resource is passed a valid input record,
its attributes will be updated.
Operations on a pick measure
resource are potentially lengthy since a great number of structure
elements may have to be processed in order to produce the pick results.
.LP
The pick measure resource components, in order,
are listed in the following table.
The abbreviation "from wks" indicates that the default
value is inherited from the pick device descriptor stored
in the PHIGS workstation resource when the pick measure is created.
.LP
.ID
.ta 0.1i 1.6i 3.2i
	\fBAttribute Name	Data Type	Default Value\fP
.ta
.ta 0.2i 1.3i 3.4i
	pick_status	{\fINoPick, Ok\fP}	from wks
	picked_prim	LISTofPICK_ELEMENT_REF	from wks
.ta
.DE
.LP
The attributes of the pick measure resource are defined as follows:

.Bl "pick_status"
This attribute contains the result of the last update operation that
was performed.  It is set to \fIOk\fP if a primitive was successfully
picked, and \fINoPick\fP if the pick operation was unsuccessful.

.Bl "picked_prim"
This attribute contains the path of the structure element that was
picked as a result of the last update operation.

.LP
The pick measure is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  The requests in this section
manage the creation and
freeing of pick measures.
.bp

.AC "Create Pick Measure" 3
.Fs
.Na PEXCreatePickMeasure
.Rq      
.Pa pm_id PICK_MEASURE_ID
.Pa wks_id PHIGS_WKS_ID
.Pa dev_type PICK_DEVICE_TYPE
.Se
IDChoice, PhigsWKS, Alloc, Value
.Ds                     
This request creates a pick measure resource of the type specified
by \fIdev_type\fP.  The resource ID of the pick measure is specified
by \fIpm_id\fP.  The newly-created pick measure is initialized with the
values contained in the appropriate pick device descriptor stored
in the PHIGS workstation resource specified by \fIwks_id\fP.
.Fe


.AC "Free Pick Measure" 3
.Fs
.Na PEXFreePickMeasure
.Rq                 
.Pa pm_id PICK_MEASURE_ID
.Se
PickMeasure
.Ds            
This request deletes the
pick measure resource
and frees the storage associated with it.
.Fe

.AC "Get Pick Measure Attributes" 3
.Fs
.Na PEXGetPickMeasure
.Rq
.Pa pm_id PICK_MEASURE_ID
.Pa item_mask BITMASK
.Re
.Pa item_list LISTofVALUE
.Se
PickMeasure, Value
.Ds       
This request will return components of
the pick measure specified by \fIpm_id\fP.
The \fIitem_mask\fP specifies which components are to be inquired and returned.
The specified attributes of the pick measure will be returned in
\fIitem_list\fP.
.Fe

.AC "Update Pick Measure" 3
.Fs
.Na PEXUpdatePickMeasure
.Rq
.Pa pm_id PICK_MEASURE_ID
.Pa input_record LISTofCARD8
.Se
PickMeasure, Path
.Ds       
This request will update the pick measure specified by \fIpm_id\fP using
the information found in \fIinput_record\fP.  If this update causes a
primitive to be picked, the pick measure's \fIpick_status\fP will be
set to \fIOk\fP and the \fIpicked_prim\fP will be set to the path of the
picked primitive.  If no primitive was picked, the \fIpick_status\fP
will be set to \fINoPick\fP.
.LP
\fIpicked_prim\fP can be used for echoing when the pick measure is created.
However it is not used as a start path from which to start picking.
.LP
The data in \fIinput_record\fP is dependent on the type of pick device for
which the pick measure resource was created,
that is, the \fIdev_type\fP specified to \fBPEXCreatePickMeasure\fP.
\fBPEXGetPickMeasure\fP can be used to return the results
of a \fBPEXUpdatePickMeasure\fP request.
.LP
The pick measure input data records for the registered pick device types are:
.ID
.ta 1.5i 2.0i
DC_HitBox	: [pick_position : DEVICE_COORD_2D,
		pick_distance : FLOAT]

NPC_HitVolume	: [pick_volume : NPC_SUBVOLUME]
.ta
.DE
.Fe
.bp

.AC "PEX Fonts" 1
.LP
.RU
.LP
The PEX font manipulation mechanisms are the same mechanisms as those 
used for X11 fonts.  The PEX font resource is very similar to the
font resource created
by (and managed by) X11.  PEX fonts that are used with PEX stroke precision
text contain more functionality than is currently found in X11 fonts.
Specifically, stroke precision text requires scalable and rotatable
text fonts.  Because of the added capabilities of PEX fonts, PEX
has defined its own PEX font open, close, and query requests.
The X11 font manipulation requests and the corresponding PEX font
manipulation requests are listed below:
.DS
 
	OpenFont augmented by PEXOpenFont
	CloseFont augmented by PEXCloseFont
	ListFonts augmented by PEXListFonts
	ListFontsWithInfo augmented by PEXListFontsWithInfo
	QueryFont augmented by PEXQueryFont
	QueryTextExtents augmented by PEXQueryTextExtents
 
.DE           
.LP
X11 fonts and PEX fonts can be stored in the same location on the server.
PEX uses the X11 font path in order to find where PEX font files are located.
(The X11 request \fBSetFontPath\fP is used to set the current
font path and the X11
request \fBGetFontPath\fP is used to query the current font path.)
.bp
.AC "PEX Font Resource Management" 2
.LP
The PEX font is an X11 resource and carries all of the 
responsibilities and
access rights of X11 resources.  These requests manage the opening and closing
of PEX font resources.

.AC "Open PEX Font" 3
.Fs
.Na PEXOpenFont
.Rq      
.Pa f_id PEX_FONT_ID
.Pa name STRING
.Se
PEXFont, IDChoice, Alloc
.Ds                     
This request loads the specified PEX font, if necessary, and associates
identifier \fIf_id\fP with it.  The font name should use the ISO Latin-1
encoding, and upper/lower case does not matter.  PEX fonts are not associated
with a particular screen, and can be used with any renderer or PHIGS
workstation resources.
An error will be generated if the specified font
is not "PEX usable", that is, it is not capable of supporting the full
range of PEX text attributes.
.Fe



.AC "Close PEX Font" 3
.Fs
.Na PEXCloseFont
.Rq      
.Pa f_id PEX_FONT_ID
.Se
PEXFont
.Ds                     
This request deletes the association between the resource ID
and the PEX font.  The PEX font itself will be
freed when no other resource references it.
.Fe
.bp

.AC "PEX Font Inquiry" 2
.LP
The PEX font requests generate replies with logical information specific
to a font.  The information is encoded in the following data structures.
.LP
.LP
PEX_FONTPROP is defined as:
.DS
 
	name			: ATOM
	value			: CARD32
 
.DE           
.LP
PEX_FONTINFO is defined as:
.DS
 
	first_glyph		: CARD32
	last_glyph		: CARD32
 	default_glyph	: CARD32
	all_glyphs_exist	: BOOLEAN
	stroke_font		: BOOLEAN
	properties		: LISTofPEX_FONTPROP
 
.DE
.LP
The \fIfirst_glyph\fP, \fIlast_glyph\fP, and \fIdefault_glyph\fP
are indices to the first glyph, last
glyph, and default glyph of the font.  The \fIdefault_glyph\fP
specifies the glyph that will be displayed when an undefined or non-existent
glyph is used.
\fIDefault_glyph\fP may specify a nonexistent glyph.  In this case, if a
client tries to display an undefined or non-existent glyph, nothing is
drawn by the server (rather than having a default glyph drawn). 
If \fIall_glyphs_exist\fP is \fITrue\fP, then all glyphs
within the range of \fIfirst_glyph\fP and \fIlast_glyph\fP have non-zero
extents.
\fIStroke_font\fP is a flag indicating if the font is a PEX font and is
provided so that the client can build font groups that all have the same text
precision.
.LP
A font is
not guaranteed to have any \fIproperties\fP.  
Whether a property value
is a signed or unsigned, and what units it is in
must be derived from a priori 
knowledge of the property.  It is strongly recommended
that all fonts have at least a CHARSET_REGISTRY property (e.g., "ISO8859")
and a CHARSET_ENCODING property (e.g., "1").
.bp

.AC "Query PEX Font" 3
.Fs
.Na PEXQueryFont
.Rq      
.Pa f_id PEX_FONT_ID
.Re
.Pa font_info PEX_FONTINFO
.Se
PEXFont
.Ds                     
This request generates a reply which contains the logical information
about a PEX font.
.Fe


.AC "List PEX Fonts" 3
.Fs
.Na PEXListFonts
.Rq                    
.Pa pattern STRING
.Pa max_names CARD16
.Re
.Pa font_names LISTofSTRING
.Se
None
.Ds 
Like X11 \fBListFonts\fP except that this request only returns the
names of fonts that can support the full range of PEX text attributes
(i.e., those fonts that are "PEX usable").  This list may or may not
contain some of the same fonts returned by the X11 \fBListFonts\fP request.
This request returns a list of at most \fImax_names\fP
entries, each of which contains information about a font matching the
\fIpattern\fP.  \fIPattern\fP is a string that uses the ISO Latin-1
encoding, and upper/lower case does not matter.  In the pattern, the '?'
character (octal value 77) will match any single character, and the
character '*' (octal value 52) will match any number of characters.
The returned names are in lower case, and are also ISO Latin-1 encoded
strings.
.Fe
.bp
 

.AC "List PEX Fonts with Info" 3
.Fs
.Na PEXListFontsWithInfo
.Rq                    
.Pa pattern STRING
.Pa max_names CARD16
.Re
.Pa font_names LISTofSTRING
.Pa fonts LISTofPEX_FONTINFO
.Se
None
.Ds 
Like X11 \fBListFontsWithInfo\fP except that this request only returns
information about fonts that can support the full range of PEX text attributes
(i.e., those fonts that are "PEX usable").  This list may or may not
contain some of the same fonts returned by the X11 \fBListFonts\fP request.
This request returns a list of at most \fImax_names\fP
entries, each of which contains information about a font matching the
\fIpattern\fP.  \fIPattern\fP is a string that uses the ISO Latin-1
encoding, and upper/lower case does not matter.  In the pattern, the '?'
character (octal value 77) will match any single character, and the
character '*' (octal value 52) will match any number of characters.
The returned names are in lower case, and are also ISO Latin-1 encoded
strings.
.LP
The information returned for each font is identical
to what \fBPEXQueryFont\fP would return.
.Fe
 
 
.bp
.AC "Query PEX Text Extents" 3
.Fs
.Na PEXQueryTextExtents
.Rq         
.Pa fp_format FLOAT_FORMAT
.Pa resource_id RESOURCE_ID
.Pa font_group TABLE_INDEX
.Pa path CARD16
.Pa expansion FLOAT
.Pa spacing FLOAT
.Pa height FLOAT
.Pa alignment TEXT_ALIGNMENT
.Pa strings LISTofISTRING
.Re                  
.Pa extents LISTofEXTENT_INFO
.Se
FloatingPointFormat, Value, Match
.Ds                     
This request generates a reply which  contains extent information
in the local 2D text coordinate system for each of the specified strings.
The extents of each string in \fIstrings\fP is computed independently
of the other strings in the list.
If \fIresource_id\fP is a renderer or PHIGS workstation resource, the
\fITextFont\fP table used to perform the extents computation will be
the \fITextFont\fP table associated with the renderer or PHIGS workstation.
If \fIresource_id\fP is a lookup table resource of type \fITextFont\fP,
it is used directly.  \fIFont_group\fP provides the index of the entry
that is to be used to obtain the font group.
.LP
Stroke precision 
is assumed.  The text position is (0,0) in the local 2D 
text coordinate system.
\fIConcat_point\fP returns the point where the next glyph
should go if the string is to be extended.
This position is given in the 2D text coordinate system.
(A suitable modeling transformation to account for the 
character up vector will still need to be applied by the client.)
.LP
If a specified font
has no defined \fIdefault_glyph\fP
(that is, if \fIdefault_glyph\fP refers to a non-existent glyph),
then undefined glyphs
in \fIstrings\fP are taken to have all zero metrics.
.Fe
.bp
.AP "Appendix A: Definition of Standard PEX Subsets"
.SH
\s+3\f6Appendix A: Definition of Standard PEX Subsets\fP\s-3
.LP
.RU
.LP
\fBPEXGetExtensionInfo\fP returns a 32-bit value (\fIsubset_info\fP)
containing information
about whether the PEX server is a full PEX implementation or whether it
supports some combination of the
standard subsets.  The top 16 bits of this 32-bit value are
reserved for use by vendors.  The bottom 16 bits contain information
about how fully the PEX extension implementation supports the PEX protocol.
Three PEX subsets are currently defined: "immediate rendering",
"PHIGS workstation" and "structure rendering".
.LP
If all of the 16 low-order bits of \fIsubset_info\fP are zero, then the 
extension is a complete PEX implementation, otherwise the bits are
interpreted as follows:
.ID
    bit 0 set - server supports all requests for "immediate rendering"
    bit 1 set - server supports all requests for "PHIGS workstation"
    bit 2 set - server supports all requests for "structure rendering"
.DE
For backward compatibility with PEX 5.0, a PEX implementation is not
allowed to return with both bit 0 and bit 1 set.
.LP
If a server is sent a request that is \fInot\fP part of the subset(s) 
it supports, it will return a \fIRequest\fP error.
.sp
.SH
\s+2\f6Immediate Rendering Subset\fP\s-2
.LP
An "immediate rendering" subset should fully support the following PEX
resources:
.ID
\(bu  lookup tables
\(bu  name sets
\(bu  pipeline contexts
\(bu  renderers
\(bu  PEX fonts
.DE
and all of the output commands (the "execute structure" output command 
will be treated as a no-op).
To qualify as an immediate rendering
PEX subset, an implementation must support
at least the protocol requests in the following list.  Protocol requests
that are not supported should return an \fIImplementation\fP error.
.ID 0.6i
.ta 2.5i
\fBPEXGetExtensionInfo	PEXCreateRenderer
PEXGetEnumeratedTypeInfo	PEXFreeRenderer
PEXGetImpDepConstants	PEXChangeRenderer
PEXMatchRendererTargets	PEXGetRendererAttributes
	PEXGetRendererDynamics
	PEXBeginRendering
PEXCreateLookupTable	PEXEndRendering
PEXCopyLookupTable	PEXBeginStructure
PEXFreeLookupTable	PEXEndStructure
PEXGetTableInfo	PEXRenderOutputCommands
PEXGetPredefinedEntries
PEXGetDefinedIndices	PEXCreateNameSet	
PEXGetTableEntry	PEXCopyNameSet
PEXGetTableEntries	PEXFreeNameSet
PEXSetTableEntries	PEXGetNameSet
PEXDeleteTableEntries	PEXChangeNameSet

PEXCreatePipelineContext	PEXQueryFont
PEXCopyPipelineContext	PEXListFontsWithInfo
PEXFreePipelineContext	PEXQueryTextExtents
PEXGetPipelineContext	PEXListFonts
PEXChangePipelineContext	PEXOpenFont
	PEXCloseFont
PEXBeginPickOne
PEXEndPickOne
PEXBeginPickAll
PEXEndPickAll\fP
.ta
.DE
.bp
.ta 3.0i
.SH
\s+2\f6PHIGS Workstation Subset\fP\s-2
.LP
A "PHIGS workstation" subset should fully support the following PEX 
resources:
.ID
\(bu  lookup tables
\(bu  name sets
\(bu  structures
\(bu  search contexts
\(bu  PHIGS workstations
\(bu  pick measures
\(bu  PEX fonts
.DE
and all of the output commands.
To qualify as a PHIGS workstation
PEX subset, an implementation must support
at least the protocol requests in the following list.  Protocol requests
that are not supported should return an \fIImplementation\fP error.
.ID 0.6i
.ta 2.5i
\fBPEXGetExtensionInfo	PEXCreateNameSet
PEXGetEnumeratedTypeInfo	PEXCopyNameSet
PEXGetImpDepConstants	PEXFreeNameSet
PEXMatchRendererTargets	PEXGetNameSet
	PEXChangeNameSet

PEXCreateLookupTable	PEXCreateSearchContext
PEXCopyLookupTable	PEXCopySearchContext
PEXFreeLookupTable	PEXFreeSearchContext
PEXGetTableInfo	PEXGetSearchContext
PEXGetPredefinedEntries	PEXChangeSearchContext
PEXGetDefinedIndices	PEXSearchNetwork
PEXGetTableEntry	
PEXGetTableEntries
PEXSetTableEntries	
PEXDeleteTableEntries	

	PEXCreatePhigsWKS
PEXCreateStructure	PEXFreePhigsWKS
PEXCopyStructure	PEXGetWKSInfo
PEXDestroyStructures	PEXGetDynamics
PEXGetStructureInfo	PEXGetViewRep
PEXGetElementInfo	PEXRedrawAllStructures
PEXGetStructuresInNetwork	PEXUpdateWorkstation
PEXGetAncestors	PEXRedrawClipRegion
PEXGetDescendants	PEXExecuteDeferredActions
PEXFetchElements	PEXSetViewPriority
PEXSetEditingMode	PEXSetDisplayUpdateMode
PEXSetElementPointer	PEXMapDCtoWC
PEXSetElementPointerAtLabel	PEXMapWCtoDC
PEXElementSearch	PEXSetViewRep
PEXStoreElements	PEXSetWKSWindow
PEXDeleteElements	PEXSetWKSViewport
PEXDeleteElementsToLabel	PEXSetHLHSRMode
PEXDeleteElementsBetweenLabels	PEXSetWKSBufferMode
PEXCopyElements	PEXPostStructure
PEXChangeStructureReferences	PEXUnpostStructure
	PEXUnpostAllStructures
	PEXGetWKSPostings

PEXGetPickDevice	PEXQueryFont
PEXChangePickDevice	PEXListFontsWithInfo
PEXCreatePickMeasure	PEXQueryTextExtents
PEXFreePickMeasure	PEXListFonts
PEXGetPickMeasure	PEXOpenFont
PEXUpdatePickMeasure	PEXCloseFont\fP
.ta
.DE
.bp
.ta 3.0i
.SH
\s+2\f6Structure Rendering Subset\fP\s-2
.LP
An "structure rendering" subset should fully support the following PEX
resources:
.ID
\(bu  lookup tables
\(bu  name sets
\(bu  pipeline contexts
\(bu  renderers
\(bu  structures
\(bu  search contexts
\(bu  PEX fonts
.DE
and all of the output commands.
To qualify as an structure rendering
PEX subset, an implementation must support
at least the protocol requests in the following list.  Protocol requests
that are not supported should return an \fIImplementation\fP error.
.ID 0.6i
.ta 2.5i
\fBPEXGetExtensionInfo	PEXCreateStructure
PEXGetEnumeratedTypeInfo	PEXCopyStructure
PEXGetImpDepConstants	PEXDestroyStructures
PEXMatchRendererTargets	PEXGetStructureInfo
	PEXGetElementInfo
	PEXGetStructuresInNetwork
PEXCreateLookupTable	PEXGetAncestors
PEXCopyLookupTable	PEXGetDescendants
PEXFreeLookupTable	PEXFetchElements
PEXGetTableInfo	PEXSetEditingMode
PEXGetPredefinedEntries	PEXSetElementPointer
PEXGetDefinedIndices	PEXSetElementPointerAtLabel
PEXGetTableEntry	PEXElementSearch
PEXGetTableEntries	PEXStoreElements
PEXSetTableEntries	PEXDeleteElements
PEXDeleteTableEntries	PEXDeleteElementsToLabel
	PEXDeleteElementsBetweenLabels
PEXCreatePipelineContext	PEXCopyElements
PEXCopyPipelineContext	PEXChangeStructureReferences
PEXFreePipelineContext	
PEXGetPipelineContext	PEXCreateNameSet
PEXChangePipelineContext	PEXCopyNameSet
	PEXFreeNameSet
PEXCreateRenderer	PEXGetNameSet
PEXFreeRenderer	PEXChangeNameSet
PEXChangeRenderer
PEXGetRendererAttributes	PEXCreateSearchContext
PEXGetRendererDynamics	PEXCopySearchContext
PEXBeginRendering	PEXFreeSearchContext
PEXEndRendering	PEXGetSearchContext
PEXRenderNetwork	PEXChangeSearchContext
PEXRenderElements	PEXSearchNetwork
PEXAccumulateState
	PEXQueryFont
PEXBeginPickOne	PEXListFontsWithInfo
PEXEndPickOne	PEXQueryTextExtents
PEXPickOne	PEXListFonts
PEXBeginPickAll	PEXOpenFont
PEXEndPickAll	PEXCloseFont
PEXPickAll\fP
.ta
.DE
.bp
.AP "Appendix B: Minimum Support for PHIGS/PHIGS-PLUS"
.SH
\s+3Appendix B: Minimum Support for PHIGS/PHIGS-PLUS\s-3
.LP
.RU
.LP
In order to fully support a PHIGS client-side implementation and the
targeted PHIGS-PLUS functionality, a PEX extension
implementation must meet or exceed the
following support criteria.  Other than the minimum support criteria
listed here and the
subset information listed in Appendix A, a
PEX extension implementation must fully support the functionality described
in the \fIPEX Protocol Specification\fP document.
.IP "\fBEnumerated Types\fP"
.nf
.ta 1.5i
MarkerType	at least types 1-5 (\fIDot, Cross, Asterisk, Circle, X\fP) must be supported
ATextStyle	at least types 1-2 (\fINotConnected, Connected\fP) must be supported
InteriorStyle	at least types 1,2,5 (\fIHollow, Solid, Empty\fP) must be supported
HatchStyle	it is not required that an implementation support any hatch styles
LineType	at least types 1-4 (\fISolid, Dashed, Dotted, DashDot\fP) must be supported
SurfaceEdgeType	at least type 1 (\fISolid\fP) must be supported
PickDeviceType	at least pick device type 1 (\fIDC_HitBox\fP) must be supported (for full
	implementation or for the "PHIGS workstation only" subset)
PolylineInterpMethod	at least types 1-2 (\fINone, Color\fP) must be supported
CurveApproxMethod	at least types 1-8 (\fIimp. dep., ConstantBetweenKnots, WCS_ChordalSize, 
	NPC_ChordalSize, DC_ChordalSize, WCS_ChordalDev, NPC_ChordalDev, 
	DC_ChordalDev\fP) must be supported
ReflectionModel	at least types 1-4 (\fINoShading, Ambient, Diffuse, Specular\fP) must be supported
SurfaceInterpMethod	at least types 1-2 (\fINone, Color\fP) must be supported
SurfaceApproxMethod	at least types 1-8 (\fIimp. dep., ConstantBetweenKnots, WCS_ChordalSize, 
	NPC_ChordalSize, DC_ChordalSize, WCS_PlanarDev, NPC_PlanarDev, 
	DC_PlanarDev\fP) must be supported
TrimCurveApproxMethod	at least types 1-2 (\fIimp. dep., ConstantBetweenKnots\fP) must be supported
ModelClipOperator	at least types 1-2 (\fIReplace, Intersection\fP) must be supported
LightType	at least types 1-4 (\fIAmbient, WCS_Vector, WCS_Point, WCS_Spot\fP)
	must be supported
ColorType	type 0 (\fIIndexed\fP) and types 1 (\fIRGBFloat\fP) must be supported
FloatFormat	at least one of types 1-2 (\fIIEEE_754_32, DEC_F_Floating\fP) must be supported
HLHSRMode	at least type 1 (\fIOff\fP) and one other type (\fIZBuffer, Painters, Scanline,
	HiddenLineOnly,\fP or an implementation-dependent type) must be supported
PromptEchoType	at least type 1 (\fIEchoPrimitive\fP) must be supported
DisplayUpdateMode	at least types 1-3 (\fIVisualizeEach, VisualizeEasy, VisualizeNone\fP) must be supported
ColorApproxType	at least one of types 1-2 {\fIColorSpace, ColorRange\fP} must be supported
ColorApproxModel	at least one of types 1-5 {\fIRGB, HSV, HLS, CIE, YIQ\fP} must
	be supported
RenderingColorModel	at least type 0 (implementation-dependent) must be supported
ParametricSurfaceCharacteristics at least types 1-3 (\fINone, imp. dep., IsoparametricCurves\fP) must be supported
BufferMode	it is not required that an implementation support double buffering
GDP	it is not required that an implementation support any GDPs
GDP3	it is not required that an implementation support any GDP3s
GSE	it is not required that an implementation support any GSEs

.IP "\fBOutput Primitive Attributes\fP"
.nf
.ta 1.5i
marker_scale	marker scale factors other than 1.0 may be ignored
text_precision	all types (\fIStroke, Char, String\fP) must be supported
char_expansion	character expansion other than 1.0 may be ignored
char_height	character height other than 0.01 may be ignored
atext_height	annotation text character height other than 0.01 may be ignored
line_width	line widths other than 1.0 may be ignored
interior_style_index	this attribute may be ignored if neither \fIPattern\fP nor \fIHatch\fP style is supported
reflection_attr	transmission coefficient attribute may be ignored
bf_interior_style_index	this attribute may be ignored if neither \fIPattern\fP nor \fIHatch\fP style is supported
bf_reflection_attr	transmission coefficient attribute may be ignored
pattern_size	this attribute may be ignored if \fIPattern\fP style is not supported
pattern_ref_pt	this attribute may be ignored if \fIPattern\fP style is not supported
pattern_ref_vec1	this attribute may be ignored if \fIPattern\fP style is not supported
pattern_ref_vec2	this attribute may be ignored if \fIPattern\fP style is not supported
surface_edge_width	surface edge widths other than 1.0 may be ignored
model_clip_volume	must support the combining of at least six halfspaces to compute the
	modeling clipping volume
depth_cue attributes	depth cue attributes other than the default (depth cue off) may be ignored
HLHSR_identifier 	at least one must be available	
specific GSEs	may be ignored

.IP "\fBOutput Primitives\fP"
.nf
.ta 1.5i
NURB surfaces	must be supported up to order six
NURB curves	must be supported up to order six
trim curves	must be supported up to order four
cell arrays	may be simulated by drawing the outline of the cell array
specific GDPs	may be ignored

.IP "\fBLookup Tables\fP"
.nf
.ta 1.5i
LineBundle	must support at least 5 entries
MarkerBundle	must support at least 5 entries
TextBundle	must support at least 6 entries
InteriorBundle	must support at least 5 entries
EdgeBundle	must support at least 5 entries
Pattern	must support at least 10 entries (if interior style \fIPattern\fP supported)
Color		must support at least 2 entries
TextFont	must support at least 2 entries
View		must support at least 6 entries
Light		must support at least 2 entries
DepthCue	must support at least 2 entries
ColorApprox	must support at least 1 entry

.IP "\fBMiscellaneous\fP"
.nf
.ta 1.5i
fonts	an implementation need not support drawing text primitives with X11 fonts
	font \fIone\fP must be mono-spaced
namesets	at least 64 names must be supported
search contexts	at least one normal and one inverted filter list must be supported
ws display priorities	at least two ws display priorities must be supported
.ta
.bp
.AP "Appendix C: Definition of PEX Errors"
.SH
\s+3Appendix C: Definition of PEX Errors\s-3
.LP
.RU
.nf
\fBPEXGetExtensionInfo\fP
	none

\fBPEXGetEnumeratedTypeInfo\fP
	Drawable: specified drawable resource ID is invalid
	Value: specified enumerated type number is invalid
	Match: specified drawable is not supported

\fBPEXGetImpDepConstants\fP
	Value: a specified constant name is invalid
	FloatingPointFormat: device does not support the specified fp format
	Drawable: specified drawable resource ID is invalid
	Match: specified drawable is not supported

\fBPEXMatchRendererTargets\fP
	Drawable: specified drawable resource ID is invalid
	Value: specified visual is invalid

\fBPEXEscape\fP
	Value: a specified constant name is invalid

\fBPEXEscapeWithReply\fP
	Value: a specified constant name is invalid

\fBPEXCreateLookupTable\fP
	IDChoice: ID already in use or not in range assigned to client
	Drawable: specified drawable resource ID is invalid
	Match: specified drawable is not supported
	Value: table_type value does not name a valid table type
	Alloc: server failed to allocate the requested resource
	LookupTable: table type not supported by implementation

\fBPEXCopyLookupTable\fP
	LookupTable: either src_lut_id or dest_lut_id is an invalid resource ID
	LookupTable: table type not supported by implementation
	Match: src_lut_id and dest_lut_id must have been created for use on the
		same class of drawables, and must be the same type of lookup
		table

\fBPEXFreeLookupTable\fP
	LookupTable: lut_id contains an invalid lut resource ID

\fBPEXGetTableInfo\fP
	Drawable: specified drawable resource ID is invalid
	Match: specified drawable is not supported
	Value: table_type value does not name a valid table type
	LookupTable: table type not supported by implementation
	
\fBPEXGetPredefinedEntries\fP
	Drawable: specified drawable resource ID is invalid
	Match: specified drawable is not supported
	Value: table_type value does not name a valid table type
	Value: start < min predefined entry
	Value: start + count > max predefined entry
	Value: entry 0 not valid for this table type
	FloatingPointFormat: device does not support the specified fp format
	LookupTable: table type not supported by implementation

\fBPEXGetDefinedIndices\fP
	LookupTable: lut_id contains an invalid lut resource ID
	LookupTable: table type not supported by implementation

\fBPEXGetTableEntry\fP
	LookupTable: lut_id contains an invalid lut resource ID
	LookupTable: table type not supported by implementation
	FloatingPointFormat: device does not support the specified fp format
	Value: entry 0 not valid for this table type

\fBPEXGetTableEntries\fP
	LookupTable: lut_id contains an invalid lut resource ID
	LookupTable: table type not supported by implementation
	Value: start + count is greater than 65535
	Value: entry 0 not valid for this table type
	FloatingPointFormat: device does not support the specified fp format

\fBPEXSetTableEntries\fP
	LookupTable: lut_id contains an invalid lut resource ID
	LookupTable: table type not supported by implementation
	Value: start + count is greater than 65535
	Value: illegal value in one of the fields of a table entry
	Value: entry 0 not valid for this table type
	FloatingPointFormat: device does not support the specified fp format
	ColorType: device does not support the specified color type
	ColorType: color type may not be \fIIndexed\fP for color lookup table
	Alloc: table is full

\fBPEXDeleteTableEntries\fP
	LookupTable: lut_id contains an invalid lut resource ID
	LookupTable: table type not supported by implementation
	Value: start + count is greater than 65535
	Value: entry 0 not valid for this table type

\fBPEXCreatePipelineContext\fP
	IDChoice: ID already in use or not in range assigned to client
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	ColorType: device does not support the specified color type
	Alloc: server failed to allocate the requested resource
	NameSet: a specified name set resource ID is invalid

\fBPEXCopyPipelineContext\fP
	PipelineContext: pc_id contains an invalid pipeline context ID
	Value: illegal bit set in item mask parameter

\fBPEXFreePipelineContext\fP
	PipelineContext: pc_id contains an invalid pipeline context ID

\fBPEXGetPipelineContext\fP
	PipelineContext: pc_id contains an invalid pipeline context ID
	FloatingPointFormat: device does not support the specified fp format
	Value: illegal bit set in item mask parameter

\fBPEXChangePipelineContext\fP
	PipelineContext: pc_id contains an invalid pipeline context ID
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	ColorType: device does not support the specified color type
	NameSet: a specified name set resource ID is invalid

\fBPEXCreateRenderer\fP
	IDChoice: ID already in use or not in range assigned to client
	Drawable: specified drawable resource ID is invalid
	PipelineContext: specified pipeline context resource ID is invalid
	NameSet: a specified name set resource ID is invalid
	LookupTable: a specified lookup table resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	Alloc: server failed to allocate the requested resource
	Match: lookup table root/depth does not match example drawable's root/depth
	Match: specified drawable is not supported

\fBPEXFreeRenderer\fP
	Renderer: specified renderer resource ID is invalid

\fBPEXChangeRenderer\fP
	Renderer: specified renderer resource ID is invalid
	Match: specified lookup table resource was not created for drawables
		of the same root and depth as the specified renderer
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	NameSet: a specified name set resource ID is invalid
	LookupTable: a specified lookup table resource ID is invalid
	PipelineContext: specified pipeline context resource ID is invalid

\fBPEXGetRendererAttributes\fP
	Renderer: specified renderer resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: illegal bit set in item mask parameter

\fBPEXGetRendererDynamics\fP
	Renderer: specified renderer resource ID is invalid

\fBPEXBeginRendering\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable 
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do rendering
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state

\fBPEXEndRendering\fP
	Renderer: specified renderer resource ID is invalid
	RendererState: renderer was in the \fIPicking\fP state

\fBPEXBeginStructure\fP
	Renderer: specified renderer resource ID is invalid

\fBPEXEndStructure\fP
	Renderer: specified renderer resource ID is invalid
	RendererState: no matching begin structure

\fBPEXRenderOutputCommands\fP
	Renderer: specified renderer resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	OutputCommand: illegal value in output commands 

\fBPEXRenderElements\fP
	Renderer: specified renderer resource ID is invalid
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" parameter

\fBPEXAccumulateState\fP
	Renderer: specified renderer resource ID is invalid
	Path: illegal or poorly-formed search path (includes invalid structure
		IDs, invalid element offset values)

\fBPEXRenderNetwork\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Structure: specified structure resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable 
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do rendering
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state

\fBPEXBeginPickOne\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do picking
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state
	Value: bad value given for "pick_type" or "pick_data"

\fBPEXEndPickOne\fP
	Renderer: specified renderer resource ID is invalid
	RendererState: renderer was in the \fIRendering\fP state

\fBPEXPickOne\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Structure: specified structure resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do picking
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state
	Value: bad value given for "pick_type" or "pick_data"

\fBPEXBeginPickAll\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do picking
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state
	Value: bad value given for "pick_data"

\fBPEXEndPickAll\fP
	Renderer: specified renderer resource ID is invalid
	RendererState: renderer was in the \fIRendering\fP state

\fBPEXPickAll\fP
	Renderer: specified renderer resource ID is invalid
	Drawable: specified drawable resource ID is invalid
	Structure: specified structure resource ID is invalid
	Match: specified renderer resource was not created for drawables
		of the same root and depth as the specified drawable
	Match: specified drawable is not supported
	Alloc: server was unable to allocate the resources necessary to do picking
	RendererState: renderer was in the \fIRendering\fP or \fIPicking\fP state
	Value: bad value given for "pick_data"
	Path: initial pick path is invalid

\fBPEXCreateStructure\fP
	IDChoice: ID already in use or not in range assigned to client
	Alloc: server failed to allocate the requested resource

\fBPEXCopyStructure\fP
	Structure: specified structure resource ID is invalid

\fBPEXDestroyStructures\fP
	Structure: specified structure resource ID is invalid

\fBPEXGetStructureInfo\fP
	Structure: specified structure resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: illegal bit set in item_mask parameter

\fBPEXGetElementInfo\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" parameter
	FloatingPointFormat: device does not support the specified fp format

\fBPEXGetStructuresInNetwork\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "which" parameter

\fBPEXGetAncestors\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "path_part" parameter

\fBPEXGetDescendants\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "path_part" parameter

\fBPEXFetchElements\fP
	Structure: specified structure resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: bad value for "whence" parameter

\fBPEXSetEditingMode\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "mode" parameter

\fBPEXSetElementPointer\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" parameter

\fBPEXSetElementPointerAtLabel\fP
	Structure: specified structure resource ID is invalid
	Label: no occurrences of specified label in structure

\fBPEXElementSearch\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" or "direction" parameters

\fBPEXStoreElements\fP
	Structure: specified structure resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	OutputCommand: illegal value in output commands 

\fBPEXDeleteElements\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" parameter

\fBPEXDeleteElementsToLabel\fP
	Structure: specified structure resource ID is invalid
	Label: no occurrences of specified label in structure
	Value: bad value for "whence" parameter

\fBPEXDeleteElementsBetweenLabels\fP
	Structure: specified structure resource ID is invalid
	Label: no occurrences of specified label in structure

\fBPEXCopyElements\fP
	Structure: specified structure resource ID is invalid
	Value: bad value for "whence" parameter

\fBPEXChangeStructureReferences\fP
	Structure: specified structure resource ID is invalid

\fBPEXCreateNameSet\fP
	IDChoice: ID already in use or not in range assigned to client
	Alloc: server failed to allocate the requested resource

\fBPEXCopyNameSet\fP
	NameSet: specified name set resource ID is invalid

\fBPEXFreeNameSet\fP
	NameSet: specified name set resource ID is invalid

\fBPEXGetNameSet\fP
	NameSet: specified name set resource ID is invalid

\fBPEXChangeNameSet\fP
	NameSet: specified name set resource ID is invalid
	Value: bad value for "action" parameter

\fBPEXCreateSearchContext\fP
	IDChoice: ID already in use or not in range assigned to client
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	Alloc: server failed to allocate the requested resource
	Path: illegal or poorly-formed search path (includes invalid structure
		IDs, invalid element offset values)
	NameSet: specified name set resource ID is invalid

\fBPEXCopySearchContext\fP
	SearchContext: specified search context resource ID is invalid
	Value: illegal bit set in item mask parameter

\fBPEXFreeSearchContext\fP
	SearchContext: specified search context resource ID is invalid

\fBPEXGetSearchContext\fP
	SearchContext: specified search context resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: illegal bit set in item mask parameter

\fBPEXChangeSearchContext\fP
	SearchContext: specified search context resource ID is invalid
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	Path: illegal or poorly-formed search path (includes invalid structure
		IDs, invalid element offset values)
	NameSet: specified name set resource ID is invalid

\fBPEXSearchNetwork\fP
	SearchContext: specified search context resource ID is invalid
	Path: illegal or poorly-formed search path (includes invalid structure
		IDs, invalid element offset values)

\fBPEXCreatePhigsWKS\fP
	IDChoice: ID already in use or not in range assigned to client
	Drawable: specified drawable resource ID is invalid
	Match: specified lookup table resource was not created for drawables
		of the same root and depth as the specified drawable
	Match: specified drawable is not supported
	LookupTable: a specified lookup table resource ID is invalid
	NameSet: a specified name set resource ID is invalid
	Alloc: server failed to allocate the requested resource
	Alloc: server cannot allocate resources necessary for double-buffering
	Value: bad value for buffer_mode parameter

\fBPEXFreePhigsWKS\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXGetWKSInfo\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: illegal bit set in item mask parameter

\fBPEXGetDynamics\fP
	Drawable: specified drawable resource ID is invalid
	Match: specified drawable is not supported

\fBPEXGetViewRep\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: specified view table entry is not defined

\fBPEXRedrawAllStructures\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXUpdateWorkstation\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXRedrawClipRegion\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXExecuteDeferredActions\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXSetViewPriority\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad value for "priority" parameter
	Value: specified table entry is not defined

\fBPEXSetDisplayUpdateMode\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad value for "display_update" parameter

\fBPEXMapWCtoDC\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format

\fBPEXMapDCtoWC\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format

\fBPEXSetViewRep\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Alloc: table is full

\fBPEXSetWKSWindow\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format

\fBPEXSetWKSViewport\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format
	Value: bad value for "use_drawable" parameter

\fBPEXSetHLHSRMode\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad value for "mode" parameter
	Alloc: server cannot allocate necessary resources 

\fBPEXSetWKSBufferMode\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad or unsupported value for "buffer_mode" parameter
	Alloc: server cannot allocate resources necessary for double-buffering

\fBPEXPostStructure\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Structure: specified structure resource ID is invalid
	FloatingPointFormat: device does not support the specified fp format

\fBPEXUnpostStructure\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Structure: specified structure resource ID is invalid

\fBPEXUnpostAllStructures\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid

\fBPEXGetWKSPostings\fP
	Structure: specified structure resource ID is invalid

\fBPEXGetPickDevice\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad value for "dev_type" parameter
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format

\fBPEXChangePickDevice\fP
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Value: bad value for "dev_type" parameter
	Value: an item in the item_list is out of range
	Value: illegal bit set in item mask parameter
	FloatingPointFormat: device does not support the specified fp format
	Path: illegal or poorly-formed pick path (includes invalid structure
		IDs, invalid element offset values)
	NameSet: a specified name set resource ID is invalid

\fBPEXCreatePickMeasure\fP
	IDChoice: ID already in use or not in range assigned to client
	PhigsWKS: specified PHIGS workstation resource ID is invalid
	Alloc: server failed to allocate the requested resource
	Value: bad value for "dev_type" parameter

\fBPEXFreePickMeasure\fP
	PickMeasure: specified pick measure resource ID is invalid

\fBPEXGetPickMeasure\fP
	PickMeasure: specified pick measure resource ID is invalid
	Value: illegal bit set in item mask parameter

\fBPEXUpdatePickMeasure\fP
	PickMeasure: specified pick measure resource ID is invalid
	Path: illegal or poorly-formed search path (includes invalid structure
		IDs, invalid element offset values)

\fBPEXOpenFont\fP
	PEXFont: string does not name a useable PEX font
	IDChoice: ID already in use or not in range assigned to client
	Alloc: server failed to allocate the requested resource

\fBPEXCloseFont\fP
	PEXFont: specified PEX font resource ID is invalid

\fBPEXQueryFont\fP
	PEXFont: specified PEX font resource ID is invalid

\fBPEXListFonts\fP
	none

\fBPEXListFontsWithInfo\fP
	none

\fBPEXQueryTextExtents\fP
	FloatingPointFormat: device does not support the specified fp format
	Value: input text attribute values are illegal or out of range
	Value: resource ID does not name a valid renderer, PHIGS workstation
		or lookup table
	Match: resource ID specifies a table of type other than TextFont
.fi
.bp
.AP "Appendix D: Definition of Table Default Values"
.SH
\s+3Appendix D: Definition of Table Default Values\s-3
.LP
.RU
.LP
If a table entry that is not defined is referenced, the contents of
the default table entry will be used.
The default entry for all tables
is one, except for the view, depth cue,
and color approximation
tables whose default entry is zero.
If the contents of the default table entry is not defined, then
the default attribute values listed below
will be used to define a default table entry that will be used instead.
For each type of table, the attribute name, data type, and default value
are listed.  Each PEX implementation should provide documentation describing
the choices it made for those attributes listed as "implementation-dependent".
.sp 2
.LP
.Bl "LineBundle   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	line_type	LINE_TYPE	\fILineTypeSolid\fP
	polyline_interp	POLYLINE_INTERP	\fIPolylineInterpNone\fP
	curve_approx	CURVE_APPROX	{1, 1.0}\(dg
	line_width	FLOAT	1.0
	line_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
.ta
.DE
.FS
\(dg PHIGS-PLUS defines the default curve approximation type to be 1, which
is an implementation-dependent method.
.FE

.Bl "MarkerBundle   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	marker_type	MARKER_TYPE	\fIMarkerAsterisk\fP
	marker_scale	FLOAT	1.0
	marker_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
.ta
.DE


.Bl "TextBundle   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	text_font_index	TABLE_INDEX	1
	text_precision	TEXT_PRECISION	\fIString\fP
	char_expansion	FLOAT	1.0
	char_spacing	FLOAT	0.0
	text_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
.ta
.DE


.Bl "InteriorBundle   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	interior_style	INTERIOR_STYLE	\fIInteriorStyleHollow\fP
	interior_style_index	TYPE_OR_TABLE_INDEX	1
	surface_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	reflection_attr	REFLECTION_ATTR	{1.0, 1.0, 1.0, 0.0, 0.0, (\fIIndexed\fP, 1)}
	reflection_model	REFLECTION_MODEL	\fIReflectionNoShading\fP
	surface_interp	SURFACE_INTERP	\fISurfaceInterpNone\fP
	bf_interior_style	INTERIOR_STYLE	\fIInteriorStyleHollow\fP
	bf_interior_style_index	TYPE_OR_TABLE_INDEX	1
	bf_surface_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
	bf_reflection_attr	REFLECTION_ATTR	{1.0, 1.0, 1.0, 0.0, 0.0, (\fIIndexed\fP, 1)}
	bf_reflection_model	REFLECTION_MODEL	\fIReflectionNoShading\fP
	bf_surface_interp	SURFACE_INTERP	\fISurfaceInterpNone\fP
	surface_approx	SURFACE_APPROX	{1, 1.0, 1.0}
.ta
.DE


.Bl "EdgeBundle   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	surface_edges	SWITCH	\fIOff\fP
	surface_edge_type	SURFACE_EDGE_TYPE	\fISurfaceEdgeSolid\fP
	surface_edge_width	FLOAT	1.0
	surface_edge_color	COLOR_SPECIFIER	{\fIIndexed\fP, 1}
.ta
.DE


.Bl "Pattern   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	color_type	COLOR_TYPE	implementation-dependent
	numx	CARD16	implementation-dependent
	numy	CARD16	implementation-dependent
	colors	LISTofCOLOR	implementation-dependent
.ta
.DE

.Bl "Color   (0..65534, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	color_type	COLOR_TYPE	implementation-dependent
	color	DIRECT_COLOR	implementation-dependent
.ta
.DE


.Bl "TextFont   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	font	LISTofFONT_ID	implementation-dependent
.ta
.DE


.Bl "View   (0..65534, default entry = 0)"
.LD
.ta 0.2i 1.7i 3.7i
	clip_flags	BITMASK_SHORT	all \fIOn\fP
	clip_limits	NPC_SUBVOLUME	(0,0,0),(1,1,1)
	orientation	MATRIX	identity matrix
	mapping	MATRIX	identity matrix
.ta
.DE


.Bl "Light   (1..65535, default entry = 1)"
.LD
.ta 0.2i 1.7i 3.7i
	light_type	LIGHT_TYPE	implementation-dependent
	direction	VECTOR_3D	implementation-dependent
	point	COORD_3D	implementation-dependent
	concentration	FLOAT	implementation-dependent
	spread_angle	FLOAT	implementation-dependent
	attenuation	[factor1, factor2 : FLOAT]	implementation-dependent
	color 	COLOR_SPECIFIER	implementation-dependent
.ta
.DE
Depending on the type of light, some of the values in a table entry
may be ignored.  Undefined entries in the light table that are referenced
are treated as lights that are set to \fIOff\fP.


.Bl "DepthCue   (0..65534, default entry = 0)"
.LD
.ta 0.2i 1.7i 3.7i
	mode	SWITCH	\fIOff\fP
	front_plane	FLOAT	implementation-dependent
	back_plane	FLOAT	implementation-dependent
	front_scaling	FLOAT	implementation-dependent
	back_scaling	FLOAT	implementation-dependent
	color	COLOR_SPECIFIER	implementation-dependent
.ta
.DE


.Bl "ColorApprox   (0..65534, default entry = 0)" 
.LD
.ta 0.2i 1.7i 3.7i
	type	COLOR_APPROX_TYPE	implementation-dependent
	color_model	COLOR_APPROX_MODEL	implementation-dependent
	max1	CARD16	implementation-dependent
	max2	CARD16	implementation-dependent
	max3	CARD16	implementation-dependent
	mult1	CARD32	implementation-dependent
	mult2	CARD32	implementation-dependent
	mult3	CARD32	implementation-dependent
	weight1	FLOAT	implementation-dependent
	weight2	FLOAT	implementation-dependent
	weight3	FLOAT	implementation-dependent
	base_pixel	CARD32	implementation-dependent
	dither	SWITCH	implementation-dependent
.ta
.DE
It is suggested that PEX implementations
provide default entries that correlate
with the X definition of RGB_DEFAULT_MAP for the device.  For instance,
if a device has eight-bit pixels which index into a 24-bit hardware colormap,
and its notion of an RGB_DEFAULT_MAP
is a 6\(mu6\(mu6 color cube encoded in 216 colormap entries starting with cell
number 16, then the suggested values might be:
.LD
.ta 0.2i 1.7i 3.7i
	type	COLOR_APPROX_TYPE	\fIColorSpace\fP
	color_model	COLOR_APPROX_MODEL	\fIRGB\fP
	max1	CARD16	5
	max2	CARD16	5
	max3	CARD16	5
	mult1	CARD32	1
	mult2	CARD32	6
	mult3	CARD32	36
	weight1	FLOAT	1.0
	weight2	FLOAT	1.0
	weight3	FLOAT	1.0
	base_pixel	CARD32	16
	dither	SWITCH	\fIOff\fP
.ta
.DE
.bp
.AP "Appendix E: Registered PEX Escapes"
.SH
\s+3Appendix E: Registered PEX Escapes\s-3
.LP
.RU
.LP
A PEX implementation is not required to support any of the registered PEX 
escapes.  The list of supported escapes which a particular PEX implementation
supports can be inquired by calling \fBPEXGetEnumeratedTypeInfo\fP.  All
registered escapes contain an \fIescape_id\fP-- this is a positive number
which identifies the escape request.
.LP



\fBSet Echo Color\fP
.Fs
.Na "PEXSetEchoColor"
.Rq
.Pa escape_id CARD32
.Pa fp_format FLOAT_FORMAT
.Pa rdr_id RENDERER_ID
.Pa echo_color COLOR_SPECIFIER
.Se
Renderer, ColorType, FloatingPointFormat
.Ds
This request changes the renderer's \fIecho_color\fP attribute.  The 
modification is dynamic and takes effect immediately.  The echo color
is used to renderer primitives when the renderer's \fIecho_mode\fP is 
set to \fIEcho\fP.  The lighting and shading for echoed primitives is 
implementation-dependent.
.LP
\fIecho_color\fP is a new renderer attribute introduced by this request
and the concept of an echo color may not be supported by PEX implementations
which do not support this escape.  The \fIecho_color\fP renderer attribute 
cannot be inquired and the default value is implementation-dependent.
.LP
If the specified renderer identifier is invalid then a \fIRenderer\fP
error is generated.  A \fIColorType\fP error is returned if the specified
color type (in the \fIecho_color\fP) is not supported and
a \fIFloatingPointFormat\fP error is returned if the specified floating 
point format is not supported.  The supported color types and floating
point formats can be inquired by calling \fBPEXGetEnumeratedTypeInfo\fI.
.Fe
