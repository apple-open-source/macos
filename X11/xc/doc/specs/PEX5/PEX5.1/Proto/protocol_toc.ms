.\" $Xorg: protocol_toc.ms,v 1.3 2000/08/17 19:42:12 cpqbld Exp $
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
\fB\s+9PEX Protocol Specification\s-9\fP
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
	\fRRandi J. Rost\fP	\fIDigital Equipment Corp.	June,1987 to April,1990\fP
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
.sp 12
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
PEX Protocol Specification
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
.Ed 3
.Bg 2
2.3. Syntax
.Ed 4
.Bg 2
2.4. Naming Conventions
.Ed 4
.Bg 2
2.5. Common Types
.Ed 5
.Bg 3
2.5.1. LISTofFOO
.Ed 5
.Bg 3
2.5.2. BITMASK and LISTofVALUE
.Ed 5
.Bg 3
2.5.3. Floating Point Format - FLOAT
.Ed 5
.Bg 3
2.5.4. Colors
.Ed 5
.Bg 3
2.5.5. Element Types
.Ed 5
.Bg 3
2.5.6. Types
.Ed 6
.Bg 3
2.5.7. Errors
.Ed 11
.Bg 2
2.6. Events
.Ed 11
.Bg 2
2.7. Padding
.Ed 12
.Bg 2
2.8. Extension Information
.Ed 13
.Bg 3
2.8.1. Get Extension Information
.Ed 13
.Bg 3
2.8.2. Get Enumerated Type Information
.Ed 14
.Bg 3
2.8.3. Get Implementation-Dependent Constants
.Ed 28
.Bg 3
2.8.4. Match Renderer Targets
.Ed 31
.Bg 3
2.8.5. Escape
.Ed 31
.Bg 3
2.8.6. Escape With Reply
.Ed 32
.Bg 1
3. Output Commands
.Ed 33
.Bg 2
3.1. Data Formats
.Ed 33
.Bg 2
3.2. Errors
.Ed 33
.Bg 3
3.2.1. FloatingPointFormat Errors
.Ed 33
.Bg 3
3.2.2. ColorType Errors
.Ed 34
.Bg 3
3.2.3. Length Errors
.Ed 34
.Bg 3
3.2.4. OutputCommand Errors
.Ed 34
.Bg 2
3.3. Output Command Descriptions
.Ed 34
.Bg 3
Marker type
.Ed 35
.Bg 3
Marker scale
.Ed 35
.Bg 3
Marker color index
.Ed 35
.Bg 3
Marker color
.Ed 35
.Bg 3
Marker bundle index
.Ed 35
.Bg 3
Text font index
.Ed 35
.Bg 3
Text precision
.Ed 35
.Bg 3
Character expansion
.Ed 36
.Bg 3
Character spacing
.Ed 36
.Bg 3
Text color index
.Ed 36
.Bg 3
Text color
.Ed 36
.Bg 3
Character height
.Ed 36
.Bg 3
Character up vector
.Ed 36
.Bg 3
Text path
.Ed 37
.Bg 3
Text alignment
.Ed 37
.Bg 3
Annotation text height
.Ed 37
.Bg 3
Annotation text up vector
.Ed 37
.Bg 3
Annotation text path
.Ed 37
.Bg 3
Annotation text alignment
.Ed 37
.Bg 3
Annotation text style
.Ed 37
.Bg 3
Text bundle index
.Ed 37
.Bg 3
Line type
.Ed 38
.Bg 3
Line width
.Ed 38
.Bg 3
Line color index
.Ed 38
.Bg 3
Line color
.Ed 38
.Bg 3
Curve approximation
.Ed 38
.Bg 3
Polyline interpolation method
.Ed 38
.Bg 3
Line bundle index
.Ed 38
.Bg 3
Surface interior style
.Ed 39
.Bg 3
Surface interior style index
.Ed 39
.Bg 3
Surface color index
.Ed 39
.Bg 3
Surface color
.Ed 39
.Bg 3
Surface reflection attributes
.Ed 39
.Bg 3
Surface reflection model
.Ed 39
.Bg 3
Surface interpolation method
.Ed 39
.Bg 3
Backface surface interior style
.Ed 40
.Bg 3
Backface surface interior style index
.Ed 40
.Bg 3
Backface surface color index
.Ed 40
.Bg 3
Backface surface color
.Ed 40
.Bg 3
Backface surface reflection attributes
.Ed 40
.Bg 3
Backface surface reflection model
.Ed 40
.Bg 3
Backface surface interpolation method
.Ed 41
.Bg 3
Surface approximation
.Ed 41
.Bg 3
Facet culling mode
.Ed 41
.Bg 3
Facet distinguish flag
.Ed 41
.Bg 3
Pattern size
.Ed 41
.Bg 3
Pattern reference point
.Ed 41
.Bg 3
Pattern reference point and vectors
.Ed 41
.Bg 3
Interior bundle index
.Ed 41
.Bg 3
Surface edge flag
.Ed 42
.Bg 3
Surface edge type
.Ed 42
.Bg 3
Surface edge width
.Ed 42
.Bg 3
Surface edge color index
.Ed 42
.Bg 3
Surface edge color
.Ed 42
.Bg 3
Edge bundle index
.Ed 42
.Bg 3
Set individual ASF
.Ed 42
.Bg 3
Local transform 3D
.Ed 43
.Bg 3
Local transform 2D
.Ed 43
.Bg 3
Global transform 3D
.Ed 44
.Bg 3
Global transform 2D
.Ed 44
.Bg 3
Model clip
.Ed 44
.Bg 3
Set model clip volume 3D
.Ed 44
.Bg 3
Set model clip volume 2D
.Ed 44
.Bg 3
Restore model clip volume
.Ed 45
.Bg 3
View index
.Ed 45
.Bg 3
Light source state
.Ed 45
.Bg 3
Depth cue index
.Ed 45
.Bg 3
Pick ID       
.Ed 45
.Bg 3
HLHSR identifier
.Ed 45
.Bg 3
Color approximation index
.Ed 45
.Bg 3
Rendering color model
.Ed 46
.Bg 3
Parametric surface characteristics
.Ed 46
.Bg 3
Add names to name set
.Ed 46
.Bg 3
Remove names from name set
.Ed 46
.Bg 3
Execute structure
.Ed 46
.Bg 3
Label
.Ed 47
.Bg 3
Application data
.Ed 47
.Bg 3
GSE
.Ed 47
.Bg 3
Marker 3D
.Ed 47
.Bg 3
Marker 2D
.Ed 47
.Bg 3
Text 3D
.Ed 47
.Bg 3
Text 2D
.Ed 48
.Bg 3
Annotation text 3D
.Ed 48
.Bg 3
Annotation text 2D
.Ed 49
.Bg 3
Polyline 3D
.Ed 49
.Bg 3
Polyline 2D
.Ed 50
.Bg 3
Polyline set 3D with data
.Ed 50
.Bg 3
Non-uniform B-spline curve
.Ed 50
.Bg 3
Fill area 3D
.Ed 51
.Bg 3
Fill area 2D
.Ed 52
.Bg 3
Fill area 3D with data
.Ed 52
.Bg 3
Fill area set 3D
.Ed 53
.Bg 3
Fill area set 2D
.Ed 53
.Bg 3
Fill area set 3D with data
.Ed 54
.Bg 3
Triangle strip
.Ed 55
.Bg 3
Quadrilateral mesh
.Ed 55
.Bg 3
Set of fill area sets
.Ed 57
.Bg 3
Non-uniform B-spline surface
.Ed 59
.Bg 3
Cell array 3D
.Ed 60
.Bg 3
Cell array 2D
.Ed 60
.Bg 3
Extended cell array 3D
.Ed 61
.Bg 3
GDP 3D
.Ed 61
.Bg 3
GDP 2D
.Ed 61
.Bg 3
Noop
.Ed 61
.Bg 1
4. Lookup Tables
.Ed 62
.Bg 2
4.1. Lookup Table Resource Management
.Ed 68
.Bg 3
4.1.1. Create Lookup Table
.Ed 68
.Bg 3
4.1.2. Copy Lookup Table
.Ed 68
.Bg 3
4.1.3. Free Lookup Table
.Ed 69
.Bg 2
4.2. Lookup Table Inquiry
.Ed 70
.Bg 3
4.2.1. Get Table Info
.Ed 70
.Bg 3
4.2.2. Get Predefined Entries
.Ed 71
.Bg 3
4.2.3. Get Defined Indices
.Ed 71
.Bg 3
4.2.4. Get Table Entry
.Ed 72
.Bg 3
4.2.5. Get Table Entries
.Ed 72
.Bg 2
4.3. Lookup Table Modification
.Ed 74
.Bg 3
4.3.1. Set Table Entries
.Ed 74
.Bg 3
4.3.2. Delete Table Entries
.Ed 74
.Bg 1
5. Pipeline Contexts
.Ed 75
.Bg 2
5.1. Pipeline Context Resource Management
.Ed 83
.Bg 3
5.1.1. Create Pipeline Context
.Ed 83
.Bg 3
5.1.2. Copy Pipeline Context
.Ed 83
.Bg 3
5.1.3. Free Pipeline Context
.Ed 84
.Bg 2
5.2. Pipeline Context Inquiry
.Ed 85
.Bg 3
5.2.1. Get Pipeline Context
.Ed 85
.Bg 2
5.3. Pipeline Context Modification
.Ed 85
.Bg 3
5.3.1. Change Pipeline Context
.Ed 85
.Bg 1
6. Renderers
.Ed 86
.Bg 2
6.1. Renderer Resource Management
.Ed 90
.Bg 3
6.1.1. Create Renderer
.Ed 90
.Bg 3
6.1.2. Free Renderer
.Ed 90
.Bg 2
6.2. Renderer Modification
.Ed 91
.Bg 3
6.2.1. Change Renderer
.Ed 91
.Bg 2
6.3. Renderer Inquiry
.Ed 91
.Bg 3
6.3.1. Get Renderer Attributes
.Ed 91
.Bg 3
6.3.2. Get Renderer Dynamics
.Ed 92
.Bg 2
6.4. Client-Side Traversal Support
.Ed 93
.Bg 3
6.4.1. Begin Rendering
.Ed 93
.Bg 3
6.4.2. End Rendering
.Ed 93
.Bg 3
6.4.3. Begin Structure
.Ed 94
.Bg 3
6.4.4. End Structure
.Ed 94
.Bg 2
6.5. Rendering Commands
.Ed 95
.Bg 3
6.5.1. Render Output Commands
.Ed 95
.Bg 3
6.5.2. Render Elements
.Ed 95
.Bg 3
6.5.3. Accumulate State
.Ed 96
.Bg 3
6.5.4. Render Network
.Ed 96
.Bg 1
7. Renderer Picking
.Ed 97
.Bg 2
7.1. Pick One
.Ed 99
.Bg 3
7.1.1. Begin Pick One
.Ed 99
.Bg 3
7.1.2. End Pick One
.Ed 100
.Bg 3
7.1.3. Pick One
.Ed 100
.Bg 2
7.2. Pick All
.Ed 101
.Bg 3
7.2.1. Begin Pick All
.Ed 101
.Bg 3
7.2.2. End Pick All
.Ed 102
.Bg 3
7.2.3. Pick All
.Ed 102
.Bg 1
8. Structures
.Ed 104
.Bg 2
8.1. Structure Resource Management
.Ed 104
.Bg 3
8.1.1. Create Structure
.Ed 104
.Bg 3
8.1.2. Copy Structure
.Ed 105
.Bg 3
8.1.3. Destroy Structures
.Ed 105
.Bg 2
8.2. Structure Inquiry
.Ed 106
.Bg 3
8.2.1. Get Structure Info
.Ed 106
.Bg 3
8.2.2. Get Element Info
.Ed 107
.Bg 3
8.2.3. Get Structures In Network
.Ed 107
.Bg 3
8.2.4. Get Ancestors
.Ed 108
.Bg 3
8.2.5. Get Descendants
.Ed 109
.Bg 3
8.2.6. Fetch Elements
.Ed 110
.Bg 2
8.3. Structure Resource Attribute Modification
.Ed 111
.Bg 3
8.3.1. Set Editing Mode
.Ed 111
.Bg 3
8.3.2. Set Element Pointer
.Ed 111
.Bg 3
8.3.3. Set Element Pointer At Label
.Ed 112
.Bg 3
8.3.4. Element Search
.Ed 113
.Bg 2
8.4. Structure Editing
.Ed 114
.Bg 3
8.4.1. Store Elements
.Ed 114
.Bg 3
8.4.2. Delete Elements
.Ed 115
.Bg 3
8.4.3. Delete Elements To Label
.Ed 116
.Bg 3
8.4.4. Delete Elements Between Labels
.Ed 116
.Bg 3
8.4.5. Copy Elements
.Ed 117
.Bg 3
8.4.6. Change Structure References
.Ed 118
.Bg 1
9. Name Sets
.Ed 119
.Bg 2
9.1. Name Set Resource Management
.Ed 119
.Bg 3
9.1.1. Create Name Set
.Ed 119
.Bg 3
9.1.2. Copy Name Set
.Ed 119
.Bg 3
9.1.3. Free Name Set
.Ed 120
.Bg 2
9.2. Name Set Inquiry
.Ed 121
.Bg 3
9.2.1. Get Name Set
.Ed 121
.Bg 2
9.3. Name Set Modification
.Ed 121
.Bg 3
9.3.1. Change Name Set
.Ed 121
.Bg 1
10. Search Contexts
.Ed 122
.Bg 2
10.1. Search Context Resource Management
.Ed 124
.Bg 3
10.1.1. Create Search Context
.Ed 124
.Bg 3
10.1.2. Copy Search Context
.Ed 124
.Bg 3
10.1.3. Free Search Context
.Ed 125
.Bg 2
10.2. Search Context Inquiry
.Ed 126
.Bg 3
10.2.1. Get Search Context
.Ed 126
.Bg 2
10.3. Search Context Modification
.Ed 126
.Bg 3
10.3.1. Change Search Context
.Ed 126
.Bg 2
10.4. Structure Network Searching
.Ed 127
.Bg 3
10.4.1. Search Network
.Ed 127
.Bg 1
11. PHIGS Workstations
.Ed 128
.Bg 2
11.1. PHIGS Workstation Resource Management
.Ed 129
.Bg 3
11.1.1. Create PHIGS Workstation
.Ed 129
.Bg 3
11.1.2. Free PHIGS Workstation
.Ed 130
.Bg 2
11.2. PHIGS Workstation Inquiry
.Ed 131
.Bg 3
11.2.1. Get PHIGS Workstation Info
.Ed 131
.Bg 3
11.2.2. Get Dynamics
.Ed 135
.Bg 3
11.2.3. Get View Representation
.Ed 137
.Bg 2
11.3. PHIGS Workstation Manipulation
.Ed 138
.Bg 3
11.3.1. Redraw All Structures
.Ed 138
.Bg 3
11.3.2. Update Workstation
.Ed 138
.Bg 3
11.3.3. Redraw Clip Region
.Ed 139
.Bg 3
11.3.4. Execute Deferred Actions
.Ed 139
.Bg 3
11.3.5. Set View Priority
.Ed 140
.Bg 3
11.3.6. Set Display Update Mode
.Ed 140
.Bg 3
11.3.7. Map DC to WC
.Ed 141
.Bg 3
11.3.8. Map WC to DC
.Ed 141
.Bg 2
11.4. PHIGS Workstation Update
.Ed 143
.Bg 3
11.4.1. Set View Representation
.Ed 143
.Bg 3
11.4.2. Set Workstation Window
.Ed 143
.Bg 3
11.4.3. Set Workstation Viewport
.Ed 144
.Bg 3
11.4.4. Set HLHSR Mode
.Ed 144
.Bg 3
11.4.5. Set Buffer Mode
.Ed 145
.Bg 2
11.5. Posting/Unposting Structures
.Ed 146
.Bg 3
11.5.1. Post Structure
.Ed 146
.Bg 3
11.5.2. Unpost Structure
.Ed 146
.Bg 3
11.5.3. Unpost All Structures
.Ed 147
.Bg 3
11.5.4. Get PHIGS Workstation Postings
.Ed 147
.Bg 1
12. Workstation Picking
.Ed 148
.Bg 2
12.1. Pick Device Descriptors
.Ed 148
.Bg 3
12.1.1. Get Pick Device Descriptor
.Ed 150
.Bg 3
12.1.2. Change Pick Device Descriptor
.Ed 150
.Bg 2
12.2. Pick Measure
.Ed 151
.Bg 3
12.2.1. Create Pick Measure
.Ed 152
.Bg 3
12.2.2. Free Pick Measure
.Ed 152
.Bg 3
12.2.3. Get Pick Measure Attributes
.Ed 152
.Bg 3
12.2.4. Update Pick Measure
.Ed 153
.Bg 1
13. PEX Fonts
.Ed 154
.Bg 2
13.1. PEX Font Resource Management
.Ed 155
.Bg 3
13.1.1. Open PEX Font
.Ed 155
.Bg 3
13.1.2. Close PEX Font
.Ed 155
.Bg 2
13.2. PEX Font Inquiry
.Ed 156
.Bg 3
13.2.1. Query PEX Font
.Ed 157
.Bg 3
13.2.2. List PEX Fonts
.Ed 157
.Bg 3
13.2.3. List PEX Fonts with Info
.Ed 158
.Bg 3
13.2.4. Query PEX Text Extents
.Ed 159
.Bg 1
Appendix A: Definition of Standard PEX Subsets
.Ed 160
.Bg 1
Appendix B: Minimum Support for PHIGS/PHIGS-PLUS
.Ed 166
.Bg 1
Appendix C: Definition of PEX Errors
.Ed 168
.Bg 1
Appendix D: Definition of Table Default Values
.Ed 179
.Bg 1
Appendix E: Registered PEX Escapes
.Ed 182
.Pe			\" end on even page
