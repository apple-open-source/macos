.\" Abbreviated xtest assertions list macros, can be substituted for head.t
.\" $XConsortium: abbrev.t,v 1.1 94/03/06 19:48:11 rws Exp $
'\" Simple table specification - to avoid the need to use tbl.
.de tL \" table line
.nf
\\&\\$1\\h'|10n'\\$2\\h'|26n'\\$3\\h'|30n'\\$4\\h'|34n'\\$5\\h'|43n'\\$6\\h'|50n'\\$7\\h'|61n'\\$8
.fi
..
.hy 0
'\" Font for arguments
.ds fA \f(CO
'\" Font for structure member names
.ds fM \fI
'\" Font for symbols.
.ds fS \fC
'\" Font for function names
.ds fF \fC
'\"
'\"
'\" ###	.TH - Header for a test set.
.de TH
.nr Ac 0 1 	\" Set the assertion counter to zero.
'\" Save the name
.ds Na \\$1
.ds Ch \\$2	\" Save the chapter name
.ne 3
.sp
.ps +2
\fB\\*(Na\fP
.ps
..
'\" ###	.TI - Start assertion.
.de TI
.ds Ty \\$1
.if '\\*(Ty'' .ds Ty <class>
.br
\fB\\n+(Ac(\\*(Ty).\fR
..
'\" ### .A - Argument
.de A
\&\\*(fA\\$1\fR\\$2\\*(fA\\$3\fR\\$4\\*(fA\\$5\fR\\$6
..
'\" ### .M - Structure member name
.de M
\&\\*(fM\\$1\fR\\$2\\*(fM\\$3\fR\\$4\\*(fM\\$5\fR\\$6
..
'\" ### .S - Symbol name
.de S
\&\\*(fS\\$1\fR\\$2\\*(fS\\$3\fR\\$4\\*(fS\\$5\fR\\$6
..
'\" ### .F - Function name
.de F
.ie '\\$1'' \\*(fF\\*(Na\fR
.el \&\\*(fF\\$1\fR\\$2\\*(fF\\$3\fR\\$4\\*(fF\\$5\fR\\$6
..
'\" ### .SM - Make argument smaller
.de SM
.ie \\n(.$-2 \&\\$1\s-1\\$2\s0\\$3
.el \&\s-1\\$1\s0\\$2
..
'\" The following macros NS and NE are for internal review purposes.
'\" ### .NS - Start a note from a comment in the source.
.de NS
.br
.ft I
.ps -1
.in 1c
..
'\" ### .NE - End a note from a comment in the source.
.de NE
.ft P
.ps
.in
.br
..
'\" ### .)h - header macro
.de )h
'ev 1
'sp .5i
'nr P +1
.tl \\*({h
'sp .25i
.br
'ev
..
'\" ### .)f - footer macro
.de )f
'ev 1
'sp .5i
.tl \\*({f
.br
'ev
'bp
..
.if !\nS .nr S 5
.ps \nS			
.vs \nS+.5p
.ev 1
.ps 12
.vs 12
.ev
.if !\nO .nr O 7
.po \nO
.ll 7i
.wh 0 )h		
.wh -1i )f		
.ds {h "'\\*(Na''\\*(Ch'"
.ds {f "''%''"
.nr P 0 1
