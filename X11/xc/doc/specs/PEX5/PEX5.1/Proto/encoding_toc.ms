.\" $Xorg: encoding_toc.ms,v 1.3 2000/08/17 19:42:12 cpqbld Exp $
.nr LL 6.5i
.nr PD 0.1i
.nr HM 1.2i
.nr FM 1.0i
.nr PO 1.0i
.nh
.LP
\ 
.sp 12
.PP
.TL
\fB\s+9PEX Protocol Encoding \s-9\fP
.sp 2
.AU
Version 5.1

31-August-1992

.nf
.sp 0.4i
.ce
\fBPEX Document Editor\fP
.ta 1.5i 2.8i 4.6i
.sp 0.05i
	\fRPaula Womack\fP	\fIDigital Equipment Corp.	December,1991 to present\fP
	\fRJeffrey Friedberg\fP	\fIDigital Equipment Corp.	September,1990 to December,1991\fP
	\fRSally C. Barry\fP	\fIDigital Equipment Corp.	April,1990 to September,1990\fP
	\fRRandi J. Rost\fP	\fIDigital Equipment Corp.	May,1988 to April,1990\fP
	\fRTodd Newman\fP	\fIDigital Equipment Corp.	June,1987 to May,1988\fP
.nf
.sp 0.4i
.ce
\fBPEX Chief Architect\fP
.ta 1.5i 2.8i 4.6i
.sp 0.05i
	\fRCheryl Huntington\fP	\fISun Microsystems	September,1991 to present\fP
.nf
.sp 0.4i
.ce
\fBPEX Issues Librarian\fP
.ta 1.5i 2.8i 4.6i
.sp 0.05i
	\fRKarl Schultz\fP	\fIIBM	September,1991 to present\fP
.nf
.sp 0.4i
.ce
\fBPEX Secretary\fP
.ta 1.5i 2.8i 4.6i
.sp 0.05i
	\fRJeff Stevenson\fP	\fIHewlett-Packard Company	September,1991 to present\fP	
.sp 0.3i
.ta
.bp 1
\ 
.LP
.sp 10
This document is a companion to the \fIPEX Protocol Specification\fP document,
Version 5.1, and the sections in this document are numbered to match.
.ps 9
.nr PS 9
.vs 10
.nr VS 10
.sp 24
.LP
.nf
.ce 2
Copyright \(co 1988, 1989, 1990, 1991, 1992 X Consortium
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
.bp 1
.nr Pg 3		\" starting page number of toc
.\" This header provides macro definitions and register assignments
.\" for creating a table of contents.
.\"
.\" 	Number Registers which are not commented in the code itself:
.\"		Lv  "level" -- the level of the entry
.\"		Lc  "level change" -- amount extra space for changeing levels
.\" The commented lines can be altered to change the format in the
.\" manner described in the comments
.\"
.\" 				--  Doug Kraft, April, 1985
.\"
.\" *****  The next 2 lines are dependentant upon the mu macro package  *****
.\" *****  and should be eliminated or changed if this in not desired.  *****
.\"
.LP
\ 
.\" ***** hack to get roman numerals in toc footer (JDF 10/90)
.\"
.\" Apparently, just setting the page register % to roman format
.\" doesn't work.  It seems the value of a roman % in an "if" expression
.\" always returns zero (troff bug?).  This behavior breaks -ms when
.\" it tests % during header and footer printing.  So, instead of reformatting %,
.\" we create a private roman format register Pg that will be set to % at Bg and Ed.
.\" We might just miss a page change but, hey, this is a hack.
.\" To start at a page other than 1, set the Pg register to the start page
.\" number prior to sourcing this file.
.\"
.if !\n(Pg .nr Pg 1	\" assume start page of 1 if not set by user 
.nr % \n(Pg 1		\" force real page number to user start number
.af Pg i		\" format private count to small roman i, ii, iii, iv ...
.EH ''   ''		\" no even header
.OH ''   ''		\" no odd header
.EF '' \\\\n(Pg ''	\" print roman count in even footer
.OF '' \\\\n(Pg ''	\" print roman count in odd footer
.\"
.\" *****  invoke this at end to make sure you end up on a even page (JDF)
.de Pe
.fi
.in 0i
.ll \n(LLu
.nr Pg \\n%
.LP
\\ 
.if o \{\
.  bp
.  nr Pg \\n%
\\ 
.\}
.nr Pg \\n%
..
.\"
.\" *****  these global variable can only be set here *****
.nr Ll \n(.lu     	\" line length
.nr Xs 0.0i		\" extra space between all content entries
.nr Nl 0.5v		\" extra space when entry level changes in troff
.if n .nr Nl 1v		\" extra space when entry level changes in nroff
.nr Lw 0.6i		\" extra distance to line wrap on left
.nr Rw 0.6i		\" space between end of entry line and page number entry
.nr Ll \n(Llu-\n(Rwu
.ll \n(Llu
.af p# 1		\" assign format to entry page number
.\" *****  the following macros set level specific variables  *****
.\"        level 1 
.de l1
.ne 6			\" space needed to end of page
.ps 11			\" point size
.vs 15			\" vertical spacing
.ft B			\" font
.in 0			\" indent
.nr Sp 1v		\" extra space before this level entry
..
.\"        level 2 
.de l2
.ne 5			\" space needed to end of page
.ps 10			\" point size
.vs 12			\" vertical spacing
.ft R			\" font
.in 0			\" indent
.if t .nr Sp 0.5v	\" extra space before this level entry
.if n .nr Sp 1v
..
.\"        level 3 
.de l3
.ne 5			\" space needed to end of page
.ps 10			\" point size
.vs 12			\" vertical spacing
.ft R			\" font
.in 0.35i		\" indent
.nr Sp 0		\" extra space before this level entry
..
.\"        level 4 
.de l4
.ne 4			\" space needed to end of page
.ps 10			\" point size
.vs 12			\" vertical spacing
.ft R			\" font
.in 0.35i		\" indent
.nr Sp 0		\" extra space before this level entry
..
.\"        level 5 
.de l5
.ne 4			\" space needed to end of page
.ps 10			\" point size
.vs 12			\" vertical spacing
.ft R			\" font
.in 0.35i		\" indent
.nr Sp 0		\" extra space before this level entry
..
.\"        level 6 
.de l6
.ne 4			\" space needed to end of page
.ps 10			\" point size
.vs 12			\" vertical spacing
.ft R			\" font
.in 1.35i		\" indent
.nr Sp 0		\" extra space before this level entry
..
.\"	   levels 7-9 = level 6
.de l7
.l6
..
.de l8
.l6
..
.de l9
.l6
..
.\" *****  Begin contents entry  *****
.de Bg
.nr Pg \\n%		\" page format hack (jdf)
.l\\$1
.ie \\$1=\\n(Lv .nr Lc 0
.el .nr Lc \\n(Nlu
.nr Lv \\$1
.sp (\\n(Spu+\\n(Xsu+\\n(Lcu)u
.in +\\n(Lwu
.ti -\\n(Lwu
.nr T1 \\n(Llu-\\n(.iu+0.2i
.nr T2 \\n(Llu+\\n(Rwu-\\n(.iu
.ta \\n(T1u \\n(T2uR
.mk V1 \\n(nl
..
.\" ***** End Contents Entry *****
.de Ed
.nr Pg \\n%		\" page format hack (jdf)
.mk V2
\kH
.sp -1
.if \\n(.$ .if \\n(Lv>1 \{\
.				\" NOTE: a page number is printed only if
.				\" the page number in not 0 and the level
.				\" is 2 or greater
.	ie \\n(.$=1 .ds Pn \\$1
.	el .ds Pn \\$1-\\$2
.	tc .
.	nf
.	ie \\n(V1=\\n(V2 .nr Mv \\n(.n-\\n(Lw
.	el .nr Mv \\n(.n
.	sp |\\n(V2u
\\0\h'\\n(Mvu'\t\kH
.	tc
.	sp |\\n(V2u
\h'\\nHu'\t\\*(Pn
.	fi
.\}
.br
.ne 0
.if \\n(.t<2.5v .bp
..
.\" This is the end of the header.  Your table of contents data starts
.\" immediately below this line.
.Bg 1
PEX Protocol Encoding
.Ed 1
.Bg 1
1. Acknowledgements
.Ed 1
.Bg 1
2. PEX Protocol Specification
.Ed 2
.Bg 2
2.1. Request Processing
.Ed 2
.Bg 2
2.2. Protocol Format
.Ed 2
.Bg 3
2.2.1. Request Format
.Ed 2
.Bg 3
2.2.2. Reply Format
.Ed 2
.Bg 3
2.2.3. Error Format
.Ed 2
.Bg 3
2.2.4. Event Format
.Ed 2
.Bg 2
2.3. Syntax
.Ed 2
.Bg 2
2.4. Naming Conventions
.Ed 2
.Bg 2
2.5. Common Types
.Ed 3
.Bg 3
2.5.1. LISTofFOO
.Ed 3
.Bg 3
2.5.2. BITMASK and LISTofVALUE
.Ed 4
.Bg 3
2.5.3. Floating Point Format - FLOAT
.Ed 4
.Bg 3
2.5.4. Colors
.Ed 4
.Bg 3
2.5.5. Element Types
.Ed 5
.Bg 3
2.5.6. Types
.Ed 5
.Bg 3
2.5.7. Errors
.Ed 9
.Bg 3
2.5.8. Resource IDs
.Ed 12
.Bg 2
2.6. Events
.Ed 12
.Bg 2
2.7. Padding
.Ed 12
.Bg 2
2.8. Extension Information
.Ed 12
.Bg 1
3. Output Commands
.Ed 17
.Bg 2
3.1. Data Formats
.Ed 17
.Bg 2
3.2. Output Command Descriptions
.Ed 17
.Bg 3
MarkerType
.Ed 17
.Bg 3
MarkerScale
.Ed 17
.Bg 3
MarkerColorIndex
.Ed 17
.Bg 3
MarkerColor
.Ed 17
.Bg 3
MarkerBundleIndex
.Ed 17
.Bg 3
TextFontIndex
.Ed 17
.Bg 3
TextPrecision
.Ed 17
.Bg 3
CharacterExpansion
.Ed 18
.Bg 3
CharacterSpacing
.Ed 18
.Bg 3
TextColorIndex
.Ed 18
.Bg 3
TextColor
.Ed 18
.Bg 3
CharacterHeight
.Ed 18
.Bg 3
CharacterUpVector
.Ed 18
.Bg 3
TextPath
.Ed 18
.Bg 3
TextAlignment
.Ed 18
.Bg 3
AnnotationTextHeight
.Ed 19
.Bg 3
AnnotationTextUpVector
.Ed 19
.Bg 3
AnnotationTextPath
.Ed 19
.Bg 3
AnnotationTextAlignment
.Ed 19
.Bg 3
AnnotationTextStyle
.Ed 19
.Bg 3
TextBundleIndex
.Ed 19
.Bg 3
LineType
.Ed 20
.Bg 3
LineWidth
.Ed 20
.Bg 3
LineColorIndex
.Ed 20
.Bg 3
LineColor
.Ed 20
.Bg 3
CurveApproximation
.Ed 20
.Bg 3
PolylineInterpolationMethod
.Ed 20
.Bg 3
LineBundleIndex
.Ed 20
.Bg 3
SurfaceInteriorStyle
.Ed 20
.Bg 3
SurfaceInteriorStyleIndex
.Ed 20
.Bg 3
SurfaceColorIndex
.Ed 21
.Bg 3
SurfaceColor
.Ed 21
.Bg 3
SurfaceReflectionAttributes
.Ed 21
.Bg 3
SurfaceReflectionModel
.Ed 21
.Bg 3
SurfaceInterpolationMethod
.Ed 21
.Bg 3
BackfaceSurfaceInteriorStyle
.Ed 21
.Bg 3
BackfaceSurfaceInteriorStyleIndex
.Ed 21
.Bg 3
BackfaceSurfaceColorIndex
.Ed 21
.Bg 3
BackfaceSurfaceColor
.Ed 22
.Bg 3
BackfaceSurfaceReflectionAttributes
.Ed 22
.Bg 3
BackfaceSurfaceReflectionModel
.Ed 22
.Bg 3
BackfaceSurfaceInterpolationMethod
.Ed 22
.Bg 3
SurfaceApproximation
.Ed 22
.Bg 3
FacetCullingMode
.Ed 22
.Bg 3
FacetDistinguishFlag
.Ed 22
.Bg 3
PatternSize
.Ed 23
.Bg 3
PatternReferencePoint
.Ed 23
.Bg 3
PatternReferencePointAndVectors
.Ed 23
.Bg 3
InteriorBundleIndex
.Ed 23
.Bg 3
SurfaceEdgeFlag
.Ed 23
.Bg 3
SurfaceEdgeType
.Ed 23
.Bg 3
SurfaceEdgeWidth
.Ed 23
.Bg 3
SurfaceEdgeColorIndex
.Ed 23
.Bg 3
SurfaceEdgeColor
.Ed 24
.Bg 3
EdgeBundleIndex
.Ed 24
.Bg 3
SetIndividualASF
.Ed 24
.Bg 3
LocalTransform3D
.Ed 25
.Bg 3
LocalTransform2D
.Ed 25
.Bg 3
GlobalTransform3D
.Ed 25
.Bg 3
GlobalTransform2D
.Ed 25
.Bg 3
ModelClip
.Ed 25
.Bg 3
SetModelClipVolume3D
.Ed 25
.Bg 3
SetModelClipVolume2D
.Ed 25
.Bg 3
RestoreModelClipVolume
.Ed 26
.Bg 3
ViewIndex
.Ed 26
.Bg 3
LightSourceState
.Ed 26
.Bg 3
DepthCueIndex
.Ed 26
.Bg 3
PickID
.Ed 26
.Bg 3
HLHSRIdentifier
.Ed 26
.Bg 3
ColorApproximationIndex
.Ed 26
.Bg 3
RenderingColorModel
.Ed 26
.Bg 3
ParametricSurfaceCharacteristics
.Ed 26
.Bg 3
AddNamesToNameSet
.Ed 27
.Bg 3
RemoveNamesFromNameSet
.Ed 27
.Bg 3
ExecuteStructure
.Ed 28
.Bg 3
Label
.Ed 28
.Bg 3
ApplicationData
.Ed 28
.Bg 3
GSE
.Ed 28
.Bg 3
Marker3D
.Ed 28
.Bg 3
Marker2D
.Ed 28
.Bg 3
Text3D
.Ed 28
.Bg 3
Text2D
.Ed 29
.Bg 3
AnnotationText3D
.Ed 29
.Bg 3
AnnotationText2D
.Ed 29
.Bg 3
Polyline3D
.Ed 29
.Bg 3
Polyline2D
.Ed 29
.Bg 3
PolylineSet3DWithData
.Ed 29
.Bg 3
NonUniformBSplineCurve
.Ed 30
.Bg 3
FillArea3D
.Ed 30
.Bg 3
FillArea2D
.Ed 30
.Bg 3
FillArea3DWithData
.Ed 30
.Bg 3
FillAreaSet3D
.Ed 31
.Bg 3
FillAreaSet2D
.Ed 31
.Bg 3
FillAreaSet3DWithData
.Ed 32
.Bg 3
TriangleStrip
.Ed 32
.Bg 3
QuadrilateralMesh
.Ed 32
.Bg 3
SetOfFillAreaSets
.Ed 33
.Bg 3
NonUniformBSplineSurface
.Ed 34
.Bg 3
CellArray3D
.Ed 34
.Bg 3
CellArray2D
.Ed 34
.Bg 3
ExtendedCellArray3D
.Ed 35
.Bg 3
GDP3D
.Ed 35
.Bg 3
GDP2D
.Ed 35
.Bg 3
Noop
.Ed 35
.Bg 1
4. Lookup Tables
.Ed 36
.Bg 2
4.1. Lookup Table Resource Management
.Ed 38
.Bg 2
4.2. Lookup Table Inquiry
.Ed 39
.Bg 2
4.3. Lookup Table Modification
.Ed 42
.Bg 1
5. Pipeline Contexts
.Ed 43
.Bg 2
5.1. Pipeline Context Resource Management
.Ed 48
.Bg 2
5.2. Pipeline Context Inquiry
.Ed 48
.Bg 2
5.3. Pipeline Context Modification
.Ed 49
.Bg 1
6. Renderers
.Ed 50
.Bg 2
6.1. Renderer Resource Management
.Ed 52
.Bg 2
6.2. Renderer Modification
.Ed 52
.Bg 2
6.3. Renderer Inquiry
.Ed 52
.Bg 2
6.4. Client-Side Traversal Support
.Ed 54
.Bg 2
6.5. Rendering Commands
.Ed 54
.Bg 1
7. Renderer Picking
.Ed 56
.Bg 2
7.1. Pick One
.Ed 56
.Bg 2
7.2. Pick All
.Ed 57
.Bg 1
8. Structures
.Ed 59
.Bg 2
8.1. Structure Resource Management
.Ed 59
.Bg 2
8.2. Structure Inquiry
.Ed 59
.Bg 2
8.3. Structure Resource Attribute Modification
.Ed 62
.Bg 2
8.4. Structure Editing
.Ed 63
.Bg 1
9. Name Sets
.Ed 66
.Bg 2
9.1. Name Set Resource Management
.Ed 66
.Bg 2
9.2. Name Set Inquiry
.Ed 66
.Bg 2
9.3. Name Set Modification
.Ed 66
.Bg 1
10. Search Contexts
.Ed 68
.Bg 2
10.1. Search Context Resource Management
.Ed 68
.Bg 2
10.2. Search Context Inquiry
.Ed 69
.Bg 2
10.3. Search Context Modification
.Ed 69
.Bg 2
10.4. Structure Network Searching
.Ed 69
.Bg 1
11. PHIGS Workstations
.Ed 71
.Bg 2
11.1. PHIGS Workstation Resource Management
.Ed 71
.Bg 2
11.2. PHIGS Workstation Inquiry
.Ed 71
.Bg 2
11.3. PHIGS Workstation Manipulation
.Ed 76
.Bg 2
11.4. PHIGS Workstation Update
.Ed 78
.Bg 2
11.5. Posting/Unposting Structures
.Ed 79
.Bg 1
12. Workstation Picking
.Ed 81
.Bg 2
12.1. Pick Device Descriptors
.Ed 82
.Bg 2
12.2. Pick Measure
.Ed 82
.Bg 2
12.3. Pick Operations
.Ed 83
.Bg 1
13. PEX Fonts
.Ed 84
.Bg 2
13.1. PEX Font Resource Management
.Ed 84
.Bg 2
13.2. PEX Font Inquiry
.Ed 84
.Bg 1
Appendix: Registered PEX Escapes
.Ed 87
.Pe			\" end on even page
