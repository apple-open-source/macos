.\"#! troff -ms $1		-*- Nroff -*-
.\" "Xterm Control Sequences" document
.\" $Xorg: ctlseqs.ms,v 1.3 2000/08/17 19:42:51 cpqbld Exp $
.\"
.\"
.\"
.\"
.\" $XFree86: xc/doc/specs/xterm/ctlseqs.ms,v 3.51 2003/12/15 00:53:20 dickey Exp $
.\"
.\"
.\" Copyright 1996-2002,2003 by Thomas E. Dickey
.\"
.\"                         All Rights Reserved
.\"
.\" Permission is hereby granted, free of charge, to any person obtaining a
.\" copy of this software and associated documentation files (the
.\" "Software"), to deal in the Software without restriction, including
.\" without limitation the rights to use, copy, modify, merge, publish,
.\" distribute, sublicense, and/or sell copies of the Software, and to
.\" permit persons to whom the Software is furnished to do so, subject to
.\" the following conditions:
.\"
.\" The above copyright notice and this permission notice shall be included
.\" in all copies or substantial portions of the Software.
.\"
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
.\" OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
.\" MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
.\" IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
.\" CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
.\" TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
.\" SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.\"
.\" Except as contained in this notice, the name(s) of the above copyright
.\" holders shall not be used in advertising or otherwise to promote the
.\" sale, use or other dealings in this Software without prior written
.\" authorization.
.\"
.\"
.\" Copyright 1991, 1994 X Consortium
.\"
.\" Permission is hereby granted, free of charge, to any person obtaining
.\" a copy of this software and associated documentation files (the
.\" "Software"), to deal in the Software without restriction, including
.\" without limitation the rights to use, copy, modify, merge, publish,
.\" distribute, sublicense, and/or sell copies of the Software, and to
.\" permit persons to whom the Software is furnished to do so, subject to
.\" the following conditions:
.\"
.\" The above copyright notice and this permission notice shall be
.\" included in all copies or substantial portions of the Software.
.\"
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
.\" EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
.\" MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
.\" IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
.\" OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
.\" ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
.\" OTHER DEALINGS IN THE SOFTWARE.
.\"
.\" Except as contained in this notice, the name of the X Consortium shall
.\" not be used in advertising or otherwise to promote the sale, use or
.\" other dealings in this Software without prior written authorization
.\" from the X Consortium.
.\"
.\" X Window System is a trademark of X Consortium, Inc.
.\"
.\" Originally written by Edward Moy, University of California,
.\" Berkeley, edmoy@violet.berkeley.edu, for the X.V10R4 xterm.
.\" The X Consortium staff has since updated it for X11.
.\" Updated by Thomas E. Dickey for XFree86 3.2 - XFree86 4.3
.\"
.\" Run this file through troff and use the -ms macro package.
.\"
.if n .pl 9999v		\" no page breaks in nroff
.ND
.de St
.sp
.nr PD 0
.nr PI 1.5i
.nr VS 16
..
.de Ed
.nr PD .3v
.nr VS 12
..
.ds CH \" as nothing
.ds LH Xterm Control Sequences
.nr s 6*\n(PS/10
.ds L \s\nsBEL\s0
.ds E \s\nsESC\s0
.ds T \s\nsTAB\s0
.ds X \s\nsETX\s0
.ds N \s\nsENQ\s0
.ds e \s\nsETB\s0
.ds C \s\nsCAN\s0
.ds S \s\nsSUB\s0
.\" space between chars
.ie t .ds s \|
.el .ds s " \"
.nr [W \w'\*L'u
.nr w \w'\*E'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*T'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*X'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*N'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*e'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*C'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*S'u
.if \nw>\n([W .nr [W \nw
.nr [W +\w'\|\|'u
.de []
.nr w \w'\\$2'
.nr H \\n([Wu-\\nwu
.nr h \\nHu/2u
.\" do fancy box in troff
.ie t .ds \\$1 \(br\v'-1p'\(br\v'1p'\h'\\nhu'\\$2\h'\\nHu-\\nhu'\(br\l'-\\n([Wu\(ul'\v'-1p'\(br\l'-\\n([Wu\(rn'\v'1p'\*s
.el .ds \\$1 \\$2\*s
..
.[] Et \v'-1p'\*X\v'1p'
.[] En \v'-1p'\*N\v'1p'
.[] Be \v'-1p'\*L\v'1p'
.[] AP \v'-1p'\s\nsAPC\s0\v'1p'
.[] Bs \v'-1p'\s\nsBS\s0\v'1p'
.[] Cs \v'-1p'\s\nsCSI\s0\v'1p'
.[] S2 \v'-1p'\s\nsSS2\s0\v'1p'
.[] S3 \v'-1p'\s\nsSS3\s0\v'1p'
.[] SS \v'-1p'\s\nsSOS\s0\v'1p'
.[] Eg \v'-1p'\s\nsEPA\s0\v'1p'
.[] Sg \v'-1p'\s\nsSPA\s0\v'1p'
.[] Dc \v'-1p'\s\nsDCS\s0\v'1p'
.[] Ht \v'-1p'\s\nsHTS\s0\v'1p'
.[] ID \v'-1p'\s\nsIND\s0\v'1p'
.[] Nl \v'-1p'\s\nsNEL\s0\v'1p'
.[] Os \v'-1p'\s\nsOSC\s0\v'1p'
.[] RI \v'-1p'\s\nsRI\s0\v'1p'
.[] PM \v'-1p'\s\nsPM\s0\v'1p'
.[] ST \v'-1p'\s\nsST\s0\v'1p'
.[] Ta \v'-1p'\*T\v'1p'
.[] Lf \v'-1p'\s\nsLF\s0\v'1p'
.[] Vt \v'-1p'\s\nsVT\s0\v'1p'
.[] Ff \v'-1p'\s\nsFF\s0\v'1p'
.[] Cr \v'-1p'\s\nsCR\s0\v'1p'
.[] So \v'-1p'\s\nsSO\s0\v'1p'
.[] Sp \v'-1p'\s\nsSP\s0\v'1p'
.[] Si \v'-1p'\s\nsSI\s0\v'1p'
.[] Eb \v'-1p'\*e\v'1p'
.[] Ca \v'-1p'\*C\v'1p'
.[] Su \v'-1p'\*S\v'1p'
.[] Es \v'-1p'\*E\v'1p'
.[] Fs \v'-1p'\s\nsFS\s0\v'1p'
.[] Gs \v'-1p'\s\nsGS\s0\v'1p'
.[] Rs \v'-1p'\s\nsRS\s0\v'1p'
.[] Us \v'-1p'\s\nsUS\s0\v'1p'
.[] XX \v'-1p'\s\nsXX\s0\v'1p'
.[] $ $
.[] ! !
.[] # #
.[] % %
.[] (( (
.[] ) )
.[] * *
.[] + +
.[] , ,
.[] - -
.[] . .
.[] 0 0
.[] 1 1
.[] 2 2
.[] 3 3
.[] 4 4
.[] 5 5
.[] 6 6
.[] 7 7
.[] 8 8
.[] 9 9
.[] : :
.[] ; ;
.[] = =
.[] / /
.[] < <
.[] > >
.[] ? ?
.[] @ @
.[] A A
.[] cB B
.[] C C
.[] D D
.[] E E
.[] F F
.[] G G
.[] H H
.[] I I
.[] J J
.[] K K
.[] L L
.[] M M
.[] N N
.[] O O
.[] P P
.[] Q Q
.[] R R
.[] S S
.[] T T
.[] V V
.[] W W
.[] XX X
.[] Y Y
.[] Z Z
.[] [[ [
.[] ]] ]
.[] bS \\e
.[] { {
.[] ] ]
.[] & &
.[] ^ ^
.[] _ _
.[] qu \&'
.[] ` \`
.[] a a
.[] b b
.[] c c
.[] d d
.[] f f
.[] g g
.[] h h
.[] i i
.[] j j
.[] k k
.[] l l
.[] m m
.[] n n
.[] o o
.[] p p
.[] q q
.[] r r
.[] cs s
.[] t t
.[] u u
.[] v v
.[] w w
.[] x x
.[] y y
.[] z z
.[] | |
.[] } }
.[] ! !
.[] c" \(lq
.[] c~ ~
.[] Sc \fIc\fP
.ds Cc \fIC\fP
.ds Cb \fIC\v'.3m'\h'-.2m'\s-2b\s0\v'-.3m'\fP
.ds Cx \fIC\v'.3m'\h'-.2m'\s-2x\s0\v'-.3m'\fP
.ds Cy \fIC\v'.3m'\h'-.2m'\s-2y\s0\v'-.3m'\fP
.ds Pb \fIP\v'.3m'\h'-.2m'\s-2b\s0\v'-.3m'\fP
.ds Pc \fIP\v'.3m'\h'-.2m'\s-2c\s0\v'-.3m'\fP
.ds Pe \fIP\v'.3m'\h'-.2m'\s-2e\s0\v'-.3m'\fP
.ds Pl \fIP\v'.3m'\h'-.2m'\s-2l\s0\v'-.3m'\fP
.ds Pm \fIP\v'.3m'\h'-.2m'\s-2m\s0\v'-.3m'\fP
.ds Pp \fIP\v'.3m'\h'-.2m'\s-2p\s0\v'-.3m'\fP
.ds Pr \fIP\v'.3m'\h'-.2m'\s-2r\s0\v'-.3m'\fP
.ds Ps \fIP\v'.3m'\h'-.2m'\s-2s\s0\v'-.3m'\fP
.ds Pt \fIP\v'.3m'\h'-.2m'\s-2t\s0\v'-.3m'\fP
.ds Pu \fIP\v'.3m'\h'-.2m'\s-2u\s0\v'-.3m'\fP
.ds Pv \fIP\v'.3m'\h'-.2m'\s-2v\s0\v'-.3m'\fP
.ds Ix \fIx\fP
.ds Iy \fIy\fP
.ds Iw \fIw\fP
.ds Ih \fIh\fP
.ds Ir \fIr\fP
.ds Ic \fIc\fP
.ie t .nr LL 6.5i
.el .nr LL 72m
.if n .na
.TL
Xterm Control Sequences
.AU
Edward Moy
.AI
University of California, Berkeley
.sp
Revised by
.AU
Stephen Gildea
.AI
X Consortium (1994)
.AU
Thomas Dickey
.AI
XFree86 Project (1996-2003)
.AU
.
.am BT                  \" add page numbers after first page
.ds CF %
..
.SH
Definitions
.IP \*(Sc
The literal character \fIc\fP.
.IP \*(Cc
A single (required) character.
.IP \*(Ps
A single (usually optional) numeric parameter, composed of one of more digits.
.IP \*(Pm
A multiple numeric parameter composed of any number of single numeric
parameters, separated by \*; character(s).
Individual values for the parameters are listed with \*(Ps .
.IP \*(Pt
A text parameter composed of printable characters.
.
.SH
C1 (8-Bit) Control Characters
.ds RH C1 (8-Bit) Control Characters
.LP
The \fIxterm\fP program recognizes both 8-bit and 7-bit control characters.
It generates 7-bit controls (by default) or 8-bit if S8C1T is enabled.
The following pairs of 7-bit and 8-bit control characters are equivalent:
.St
.IP \\*(Es\\*D
Index (\*(ID is 0x84)
.IP \\*(Es\\*E
Next Line (\*(Nl is 0x85)
.IP \\*(Es\\*H
Tab Set (\*(Ht is 0x88)
.IP \\*(Es\\*M
Reverse Index (\*(RI is 0x8d)
.IP \\*(Es\\*N
Single Shift Select of G2 Character Set (\*(S2 is 0x8e): affects next character only
.IP \\*(Es\\*O
Single Shift Select of G3 Character Set (\*(S3 is 0x8f): affects next character only
.IP \\*(Es\\*P
Device Control String (\*(Dc is 0x90)
.IP \\*(Es\\*V
Start of Guarded Area (\*(Sg is 0x96)
.IP \\*(Es\\*W
End of Guarded Area (\*(Eg is 0x97)
.IP \\*(Es\\*(XX
Start of String (\*(SS is 0x98)
.IP \\*(Es\\*Z
Return Terminal ID (DECID is 0x9a).  Obsolete form of \*(Cs\*c (DA).
.IP \\*(Es\\*([[
Control Sequence Introducer (\*(Cs is 0x9b)
.IP \\*(Es\\*(bS
String Terminator (\*(ST is 0x9c)
.IP \\*(Es\\*(]]
Operating System Command (\*(Os is 0x9d)
.IP \\*(Es\\*^
Privacy Message (\*(PM is 0x9e)
.IP \\*(Es\\*_
Application Program Command (\*(AP is 0x9f)
.Ed
.sp
.LP
These control characters are used in the vtXXX emulation.
.
.SH
VT100 Mode
.ds RH VT100 Mode
.LP
Most of these control sequences are standard VT102 control sequences,
but there is support for later DEC VT terminals (i.e., VT220 and VT320),
as well as ISO 6429 and \fIaixterm\fP color controls.
VT102 features not supported are
auto-repeat and blinking characters.
There are additional control sequences to provide
\fIxterm-\fPdependent functions, such as the scrollbar or window size.
Where the function is specified by DEC or ISO 6429, the code assigned
to it is given in parentheses.  The escape codes to designate and invoke
character sets are specified by ISO 2022; see that document for a
discussion of character sets.
.
.St
.\"
.LP
.B
Single-character functions
.\"
.IP \\*(Be
Bell (Ctrl-G)
.
.IP \\*(Bs
Backspace (Ctrl-H)
.
.IP \\*(Cr
Carriage Return (Ctrl-M)
.
.IP \\*(En
Return Terminal Status (Ctrl-E).
Default response is an empty string, but may be overridden
by a resource \fIanswerbackString\fP.
.
.IP \\*(Ff
Form Feed or New Page (NP) (Ctrl-L) same as LF
.
.IP \\*(Lf
Line Feed or New Line (NL) (Ctrl-J)
.
.IP \\*(So
Shift Out (Ctrl-N) \(-> Switch to Alternate Character Set: invokes the
G1 character set.
.
.IP \\*(Sp
Space.
.
.IP \\*(Ta
Horizontal Tab (HT) (Ctrl-I)
.
.IP \\*(Vt
Vertical Tab (Ctrl-K) same as LF
.
.IP \\*(Si
Shift In (Ctrl-O) \(-> Switch to Standard Character Set: invokes the
G0 character set (the default).
.Ed
.\"
.\"
.\"
.St
.LP
.B
Controls beginning with ESC (other than those where ESC is part of a 7-bit equivalent to 8-bit C1 controls), ordered by the final character(s).
.\"
.IP \\*(Es\\*(Sp\\*F
7-bit controls (S7C1T).
.
.IP \\*(Es\\*(Sp\\*G
8-bit controls (S8C1T).
.
.IP \\*(Es\\*(Sp\\*L
Set ANSI conformance level 1 (dpANS X3.134.1).
.
.IP \\*(Es\\*(Sp\\*M
Set ANSI conformance level 2 (dpANS X3.134.1).
.
.IP \\*(Es\\*(Sp\\*N
Set ANSI conformance level 3 (dpANS X3.134.1).
.
.IP \\*(Es\\*#\\*3
DEC double-height line, top half (DECDHL)
.
.IP \\*(Es\\*#\\*4
DEC double-height line, bottom half (DECDHL)
.
.IP \\*(Es\\*#\\*5
DEC single-width line (DECSWL)
.
.IP \\*(Es\\*#\\*6
DEC double-width line (DECDWL)
.
.IP \\*(Es\\*#\\*8
DEC Screen Alignment Test (DECALN)
.
.IP \\*(Es\\*%\\*@
Select default character set, ISO 8859-1 (ISO 2022)
.
.IP \\*(Es\\*%\\*G
Select UTF-8 character set (ISO 2022)
.
.IP \\*(Es\\*(((\\*(Cc
Designate G0 Character Set (ISO 2022)
.IP \\*(Es\\*)\\*(Cc
Designate G1 Character Set (ISO 2022)
.
.IP \\*(Es\\**\\*(Cc
Designate G2 Character Set (ISO 2022)
.
.IP \\*(Es\\*+\\*(Cc
Designate G3 Character Set (ISO 2022)
.br
Final character \*(Cc for designating character sets
(\*0, \*A and \*(cB apply to VT100 and up, the remainder to VT220 and up):
  \*(Cc = \*0 \(-> DEC Special Character and Line Drawing Set
  \*(Cc = \*A \(-> United Kingdom (UK)
  \*(Cc = \*(cB \(-> United States (USASCII)
  \*(Cc = \*4 \(-> Dutch
  \*(Cc = \*C or \*5 \(-> Finnish
  \*(Cc = \*R \(-> French
  \*(Cc = \*Q \(-> French Canadian
  \*(Cc = \*K \(-> German
  \*(Cc = \*Y \(-> Italian
  \*(Cc = \*E or \*6 \(-> Norwegian/Danish
  \*(Cc = \*Z \(-> Spanish
  \*(Cc = \*H or \*7 \(-> Swedish
  \*(Cc = \*= \(-> Swiss
.
.IP \\*(Es\\*7
Save Cursor (DECSC)
.
.IP \\*(Es\\*8
Restore Cursor (DECRC)
.
.IP \\*(Es\\*=
Application Keypad (DECPAM)
.
.IP \\*(Es\\*>
Normal Keypad (DECPNM)
.
.IP \\*(Es\\*F
Cursor to lower left corner of screen (if
enabled by the \fBhpLowerleftBugCompat\fP resource).
.
.IP \\*(Es\\*c
Full Reset (RIS)
.
.IP \\*(Es\\*l
Memory Lock (per HP terminals).  Locks memory above the cursor.
.
.IP \\*(Es\\*m
Memory Unlock (per HP terminals)
.
.IP \\*(Es\\*n
Invoke the G2 Character Set (LS2)
.
.IP \\*(Es\\*o
Invoke the G3 Character Set (LS3)
.
.IP \\*(Es\\*|
Invoke the G3 Character Set as GR (LS3R).  Has no visible effect in \fIxterm\fP.
.
.IP \\*(Es\\*}
Invoke the G2 Character Set as GR (LS2R).  Has no visible effect in \fIxterm\fP.
.
.IP \\*(Es\\*(c~
Invoke the G1 Character Set as GR (LS1R).  Has no visible effect in \fIxterm\fP.
.Ed
.
.St
.LP
.B
Application Program-Control functions
.IP \\*(AP\\*(Pt\\*s\\*(ST
\fIxterm\fP implements no \*(AP functions; \*(Pt is ignored.
\*(Pt need not be printable characters.
.Ed
.
.St
.LP
.B
Device-Control functions
.IP \\*(Dc\\*(Ps\\*;\\*(Ps\\*|\\*(Pt\\*s\\*(ST
User-Defined Keys (DECUDK).
The first parameter:
  \*(Ps = \*0 \(-> Clear all UDK definitions before starting (default)
  \*(Ps = \*1 \(-> Erase Below (default)
.br
The second parameter:
  \*(Ps = \*0 \(-> Lock the keys (default)
  \*(Ps = \*1 \(-> Do not lock.
.br
The third parameter is a ';'-separated list of strings denoting
the key-code separated by a '/' from the hex-encoded key value.
The key codes correspond to the DEC function-key codes (e.g., F6=17).
.
.IP \\*(Dc\\*$\\*q\\*(Pt\\*s\\*(ST
Request Status String (DECRQSS).
The string following the "q" is one of the following:
  \*(c"\*q	\(-> DECSCA
  \*(c"\*p	\(-> DECSCL
  \*r		\(-> DECSTBM
  \*m		\(-> SGR
.br
\fIxterm\fP responds with
\*(Dc\*1\*$\*r\*(Pt\*s\*(ST
for valid requests, replacing the \*(Pt with the corresponding \*(Cs
string,
or
\*(Dc\*0\*$\*r\*(Pt\*s\*(ST
for invalid requests.
.
.IP \\*(Dc\\*+\\*q\\*(Pt\\*s\\*(ST
Request Termcap/Terminfo String (xterm, experimental).
The string following the "q" is a list of names
encoded in hexadecimal (2 digits per character)
separated by \*;
which correspond to termcap or terminfo key names.
.br
\fIxterm\fP responds with
\*(Dc\*1\*+\*r\*(Pt\*s\*(ST
for valid requests, adding to \*(Pt an \*=,
and the value of the corresponding string that xterm would send,
or
\*(Dc\*0\*+\*r\*(Pt\*s\*(ST
for invalid requests.
The strings are encoded in hexadecimal (2 digits per character).
.Ed
.\"
.St
.LP
.B
Functions using \*(Cs, ordered by the final character(s)
.IP \\*(Cs\\*(Ps\\*s\\*@
Insert \*(Ps (Blank) Character(s) (default = 1) (ICH)
.
.IP \\*(Cs\\*(Ps\\*s\\*A
Cursor Up \*(Ps Times (default = 1) (CUU)
.
.IP \\*(Cs\\*(Ps\\*s\\*(cB
Cursor Down \*(Ps Times (default = 1) (CUD)
.
.IP \\*(Cs\\*(Ps\\*s\\*C
Cursor Forward \*(Ps Times (default = 1) (CUF)
.
.IP \\*(Cs\\*(Ps\\*s\\*D
Cursor Backward \*(Ps Times (default = 1) (CUB)
.
.IP \\*(Cs\\*(Ps\\*s\\*E
Cursor Next Line \*(Ps Times (default = 1) (CNL)
.
.IP \\*(Cs\\*(Ps\\*s\\*F
Cursor Preceding Line \*(Ps Times (default = 1) (CPL)
.
.IP \\*(Cs\\*(Ps\\*s\\*G
Cursor Character Absolute  [column] (default = [row,1]) (CHA)
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*H
Cursor Position [row;column] (default = [1,1]) (CUP)
.
.IP \\*(Cs\\*(Ps\\*s\\*I
Cursor Forward Tabulation \*(Ps tab stops (default = 1) (CHT)
.
.IP \\*(Cs\\*(Ps\\*s\\*J
Erase in Display (ED)
  \*(Ps = \*0 \(-> Erase Below (default)
  \*(Ps = \*1 \(-> Erase Above
  \*(Ps = \*2 \(-> Erase All
  \*(Ps = \*3 \(-> Erase Saved Lines (xterm)
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*J
Erase in Display (DECSED)
  \*(Ps = \*0 \(-> Selective Erase Below (default)
  \*(Ps = \*1 \(-> Selective Erase Above
  \*(Ps = \*2 \(-> Selective Erase All
.
.IP \\*(Cs\\*(Ps\\*s\\*K
Erase in Line (EL)
  \*(Ps = \*0 \(-> Erase to Right (default)
  \*(Ps = \*1 \(-> Erase to Left
  \*(Ps = \*2 \(-> Erase All
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*K
Erase in Line (DECSEL)
  \*(Ps = \*0 \(-> Selective Erase to Right (default)
  \*(Ps = \*1 \(-> Selective Erase to Left
  \*(Ps = \*2 \(-> Selective Erase All
.
.IP \\*(Cs\\*(Ps\\*s\\*L
Insert \*(Ps Line(s) (default = 1) (IL)
.
.IP \\*(Cs\\*(Ps\\*s\\*M
Delete \*(Ps Line(s) (default = 1) (DL)
.
.IP \\*(Cs\\*(Ps\\*s\\*P
Delete \*(Ps Character(s) (default = 1) (DCH)
.
.IP \\*(Cs\\*(Ps\\*s\\*S
Scroll up \*(Ps lines (default = 1) (SU)
.
.IP \\*(Cs\\*(Ps\\*s\\*T
Scroll down \*(Ps lines (default = 1) (SD)
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*T
Initiate hilite mouse tracking.
Parameters are [func;startx;starty;firstrow;lastrow].
See the section \fBMouse Tracking\fP.
.
.IP \\*(Cs\\*(Ps\\*s\\*(XX
Erase \*(Ps Character(s) (default = 1) (ECH)
.
.IP \\*(Cs\\*(Ps\\*s\\*Z
Cursor Backward Tabulation \*(Ps tab stops (default = 1) (CBT)
.
.IP \\*(Cs\\*(Pm\\*s\\*`
Character Position Absolute  [column] (default = [row,1]) (HPA)
.
.IP \\*(Cs\\*(Ps\\*s\\*b
Repeat the preceding graphic character \*(Ps times (REP)
.
.IP \\*(Cs\\*(Ps\\*s\\*c
Send Device Attributes (Primary DA)
  \*(Ps = \*0, 1 or omitted \(-> request attributes from terminal.
The response depends on the \fBdecTerminalID\fP resource setting.
  \(-> \*(Cs\*?\*1\*;\*2\*c (``VT100 with Advanced Video Option'')
  \(-> \*(Cs\*?\*1\*;\*0\*c (``VT101 with No Options'')
  \(-> \*(Cs\*?\*6\*c (``VT102'')
  \(-> \*(Cs\*?\*6\*0\*;\*1\*;\*2\*;\*6\*;\*8\*;\*9\*;\*1\*5\*;\*c (``VT220'')
.br
The VT100-style response parameters do not mean anything by themselves.
VT220 parameters do, telling the host what features the terminal supports:
  \(-> \*1 132-columns
  \(-> \*2 Printer
  \(-> \*6 Selective erase
  \(-> \*8 User-defined keys
  \(-> \*9 National replacement character sets
  \(-> \*1\*5 Technical characters
  \(-> \*2\*2 ANSI color, e.g., VT525
  \(-> \*2\*9 ANSI text locator (i.e., DEC Locator mode)
.
.IP \\*(Cs\\*>\\*(Ps\\*s\\*c
Send Device Attributes (Secondary DA)
  \*(Ps = \*0, 1 or omitted \(-> request the terminal's identification code.
The response depends on the \fBdecTerminalID\fP resource setting.
It should apply only to VT220 and up, but \fIxterm\fP extends this to VT100.
  \(-> \*(Cs\*s\*>\*(Pp\*s\*;\*(Pv\*s\*;\*(Pc\*s\*c
.br
where \*(Pp denotes the terminal type
  \(-> \*0 (``VT100'')
  \(-> \*1 (``VT220'')
.br
and \*(Pv is the firmware version (for \fIxterm\fP, this is the XFree86
patch number, starting with 95).
In a DEC terminal, \*(Pc indicates the ROM cartridge
registration number and is always zero.
.
.IP \\*(Cs\\*(Pm\\*s\\*d
Line Position Absolute  [row] (default = [1,column]) (VPA)
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*f
Horizontal and Vertical Position [row;column] (default = [1,1]) (HVP)
.
.IP \\*(Cs\\*(Ps\\*s\\*g
Tab Clear (TBC)
  \*(Ps = \*0 \(-> Clear Current Column (default)
  \*(Ps = \*3 \(-> Clear All
.
.IP \\*(Cs\\*(Pm\\*s\\*h
Set Mode (SM)
  \*(Ps = \*2 \(-> Keyboard Action Mode (AM)
  \*(Ps = \*4 \(-> Insert Mode (IRM)
  \*(Ps = \*1\*2 \(-> Send/receive (SRM)
  \*(Ps = \*2\*0 \(-> Automatic Newline (LNM)
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*h
DEC Private Mode Set (DECSET)
  \*(Ps = \*1 \(-> Application Cursor Keys (DECCKM)
  \*(Ps = \*2 \(-> Designate USASCII for character sets G0-G3 (DECANM),
and set VT100 mode.
  \*(Ps = \*3 \(-> 132 Column Mode (DECCOLM)
  \*(Ps = \*4 \(-> Smooth (Slow) Scroll (DECSCLM)
  \*(Ps = \*5 \(-> Reverse Video (DECSCNM)
  \*(Ps = \*6 \(-> Origin Mode (DECOM)
  \*(Ps = \*7 \(-> Wraparound Mode (DECAWM)
  \*(Ps = \*8 \(-> Auto-repeat Keys (DECARM)
  \*(Ps = \*9 \(-> Send Mouse X & Y on button press.
See the section \fBMouse Tracking\fP.
  \*(Ps = \*1\*2 \(-> Start Blinking Cursor (att610)
  \*(Ps = \*1\*8 \(-> Print form feed (DECPFF)
  \*(Ps = \*1\*9 \(-> Set print extent to full screen (DECPEX)
  \*(Ps = \*2\*5 \(-> Show Cursor (DECTCEM)
  \*(Ps = \*3\*0 \(-> Show scrollbar (rxvt).
  \*(Ps = \*3\*5 \(-> Enable font-shifting functions (rxvt).
  \*(Ps = \*3\*8 \(-> Enter Tektronix Mode (DECTEK)
  \*(Ps = \*4\*0 \(-> Allow 80 \z\(<-\(-> 132 Mode
  \*(Ps = \*4\*1 \(-> \fImore\fP(1) fix (see \fIcurses\fP resource)
  \*(Ps = \*4\*2 \(-> Enable Nation Replacement Character sets (DECNRCM)
  \*(Ps = \*4\*4 \(-> Turn On Margin Bell
  \*(Ps = \*4\*5 \(-> Reverse-wraparound Mode
  \*(Ps = \*4\*6 \(-> Start Logging
(normally disabled by a compile-time option)
  \*(Ps = \*4\*7 \(-> Use Alternate Screen Buffer (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*6\*6 \(-> Application keypad (DECNKM)
  \*(Ps = \*6\*7 \(-> Backarrow key sends delete (DECBKM)
  \*(Ps = \*1\*0\*0\*0 \(-> Send Mouse X & Y on button press and release.
See the section \fBMouse Tracking\fP.
  \*(Ps = \*1\*0\*0\*1 \(-> Use Hilite Mouse Tracking.
  \*(Ps = \*1\*0\*0\*2 \(-> Use Cell Motion Mouse Tracking.
  \*(Ps = \*1\*0\*0\*3 \(-> Use All Motion Mouse Tracking.
  \*(Ps = \*1\*0\*1\*0 \(-> Scroll to bottom on tty output (rxvt).
  \*(Ps = \*1\*0\*1\*1 \(-> Scroll to bottom on key press (rxvt).
  \*(Ps = \*1\*0\*3\*5 \(-> Enable special modifiers for Alt and NumLock keys.
  \*(Ps = \*1\*0\*3\*6 \(-> Send ESC when Meta modifies a key
(enables the \fBmetaSendsEscape\fP resource).
  \*(Ps = \*1\*0\*3\*7 \(-> Send DEL from the editing-keypad Delete key
  \*(Ps = \*1\*0\*4\*7 \(-> Use Alternate Screen Buffer (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*4\*8 \(-> Save cursor as in DECSC (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*4\*9 \(-> Save cursor as in DECSC
and use Alternate Screen Buffer, clearing it first (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*5\*1 \(-> Set Sun function-key mode.
  \*(Ps = \*1\*0\*5\*2 \(-> Set HP function-key mode.
  \*(Ps = \*1\*0\*6\*0 \(-> Set legacy keyboard emulation (X11R6).
  \*(Ps = \*1\*0\*6\*1 \(-> Set Sun/PC keyboard emulation of VT220 keyboard.
.
.IP \\*(Cs\\*(Pm\\*s\\*i
Media Copy (MC)
  \*(Ps = \*0 \(-> Print screen (default)
  \*(Ps = \*4 \(-> Turn off printer controller mode
  \*(Ps = \*5 \(-> Turn on printer controller mode
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*i
Media Copy (MC, DEC-specific)
  \*(Ps = \*1 \(-> Print line containing cursor
  \*(Ps = \*4 \(-> Turn off autoprint mode
  \*(Ps = \*5 \(-> Turn on autoprint mode
  \*(Ps = \*1 \*0 \(-> Print composed display, ignores DECPEX
  \*(Ps = \*1 \*1 \(-> Print all pages
.
.IP \\*(Cs\\*(Pm\\*s\\*l
Reset Mode (RM)
  \*(Ps = \*2 \(-> Keyboard Action Mode (AM)
  \*(Ps = \*4 \(-> Replace Mode (IRM)
  \*(Ps = \*1\*2 \(-> Send/receive (SRM)
  \*(Ps = \*2\*0 \(-> Normal Linefeed (LNM)
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*l
DEC Private Mode Reset (DECRST)
  \*(Ps = \*1 \(-> Normal Cursor Keys (DECCKM)
  \*(Ps = \*2 \(-> Designate VT52 mode (DECANM).
  \*(Ps = \*3 \(-> 80 Column Mode (DECCOLM)
  \*(Ps = \*4 \(-> Jump (Fast) Scroll (DECSCLM)
  \*(Ps = \*5 \(-> Normal Video (DECSCNM)
  \*(Ps = \*6 \(-> Normal Cursor Mode (DECOM)
  \*(Ps = \*7 \(-> No Wraparound Mode (DECAWM)
  \*(Ps = \*8 \(-> No Auto-repeat Keys (DECARM)
  \*(Ps = \*9 \(-> Don't Send Mouse X & Y on button press
  \*(Ps = \*1\*2 \(-> Stop Blinking Cursor (att610)
  \*(Ps = \*1\*8 \(-> Don't print form feed (DECPFF)
  \*(Ps = \*1\*9 \(-> Limit print to scrolling region (DECPEX)
  \*(Ps = \*2\*5 \(-> Hide Cursor (DECTCEM)
  \*(Ps = \*3\*0 \(-> Don't show scrollbar (rxvt).
  \*(Ps = \*3\*5 \(-> Disable font-shifting functions (rxvt).
  \*(Ps = \*4\*0 \(-> Disallow 80 \z\(<-\(-> 132 Mode
  \*(Ps = \*4\*1 \(-> No \fImore\fP(1) fix (see \fIcurses\fP resource)
  \*(Ps = \*4\*2 \(-> Disable Nation Replacement Character sets (DECNRCM)
  \*(Ps = \*4\*4 \(-> Turn Off Margin Bell
  \*(Ps = \*4\*5 \(-> No Reverse-wraparound Mode
  \*(Ps = \*4\*6 \(-> Stop Logging
(normally disabled by a compile-time option)
  \*(Ps = \*4\*7 \(-> Use Normal Screen Buffer
  \*(Ps = \*6\*6 \(-> Numeric keypad (DECNKM)
  \*(Ps = \*6\*7 \(-> Backarrow key sends backspace (DECBKM)
  \*(Ps = \*1\*0\*0\*0 \(-> Don't Send Mouse X & Y on button press and
release.
See the section \fBMouse Tracking\fP.
  \*(Ps = \*1\*0\*0\*1 \(-> Don't Use Hilite Mouse Tracking
  \*(Ps = \*1\*0\*0\*2 \(-> Don't Use Cell Motion Mouse Tracking
  \*(Ps = \*1\*0\*0\*3 \(-> Don't Use All Motion Mouse Tracking
  \*(Ps = \*1\*0\*1\*0 \(-> Don't scroll to bottom on tty output (rxvt).
  \*(Ps = \*1\*0\*1\*1 \(-> Don't scroll to bottom on key press (rxvt).
  \*(Ps = \*1\*0\*3\*5 \(-> Disable special modifiers for Alt and NumLock keys.
  \*(Ps = \*1\*0\*3\*6 \(-> Don't send ESC when Meta modifies a key
(disables the \fBmetaSendsEscape\fP resource).
  \*(Ps = \*1\*0\*3\*7 \(-> Send VT220 Remove from the editing-keypad Delete key
  \*(Ps = \*1\*0\*4\*7 \(-> Use Normal Screen Buffer, clearing screen
first if in the Alternate Screen (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*4\*8 \(-> Restore cursor as in DECRC (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*4\*9 \(-> Use Normal Screen Buffer and restore cursor
as in DECRC (unless
disabled by the \fBtiteInhibit\fP resource)
  \*(Ps = \*1\*0\*5\*1 \(-> Reset Sun function-key mode.
  \*(Ps = \*1\*0\*5\*2 \(-> Reset HP function-key mode.
  \*(Ps = \*1\*0\*6\*0 \(-> Reset legacy keyboard emulation (X11R6).
  \*(Ps = \*1\*0\*6\*1 \(-> Reset Sun/PC keyboard emulation of VT220 keyboard.
.
.IP \\*(Cs\\*(Pm\\*s\\*m
Character Attributes (SGR)
  \*(Ps = \*0 \(-> Normal (default)
  \*(Ps = \*1 \(-> Bold
  \*(Ps = \*4 \(-> Underlined
  \*(Ps = \*5 \(-> Blink (appears as Bold)
  \*(Ps = \*7 \(-> Inverse
  \*(Ps = \*8 \(-> Invisible, i.e., hidden (VT300)
  \*(Ps = \*2\*2 \(-> Normal (neither bold nor faint)
  \*(Ps = \*2\*4 \(-> Not underlined
  \*(Ps = \*2\*5 \(-> Steady (not blinking)
  \*(Ps = \*2\*7 \(-> Positive (not inverse)
  \*(Ps = \*2\*8 \(-> Visible, i.e., not hidden (VT300)
  \*(Ps = \*3\*0 \(-> Set foreground color to Black
  \*(Ps = \*3\*1 \(-> Set foreground color to Red
  \*(Ps = \*3\*2 \(-> Set foreground color to Green
  \*(Ps = \*3\*3 \(-> Set foreground color to Yellow
  \*(Ps = \*3\*4 \(-> Set foreground color to Blue
  \*(Ps = \*3\*5 \(-> Set foreground color to Magenta
  \*(Ps = \*3\*6 \(-> Set foreground color to Cyan
  \*(Ps = \*3\*7 \(-> Set foreground color to White
  \*(Ps = \*3\*9 \(-> Set foreground color to default (original)
  \*(Ps = \*4\*0 \(-> Set background color to Black
  \*(Ps = \*4\*1 \(-> Set background color to Red
  \*(Ps = \*4\*2 \(-> Set background color to Green
  \*(Ps = \*4\*3 \(-> Set background color to Yellow
  \*(Ps = \*4\*4 \(-> Set background color to Blue
  \*(Ps = \*4\*5 \(-> Set background color to Magenta
  \*(Ps = \*4\*6 \(-> Set background color to Cyan
  \*(Ps = \*4\*7 \(-> Set background color to White
  \*(Ps = \*4\*9 \(-> Set background color to default (original).
.sp
If 16-color support is compiled, the following apply.
Assume that \fIxterm\fP's resources
are set so that the ISO color codes are the first 8 of a set of 16.
Then the \fIaixterm\fP colors are the bright versions of the ISO colors:
  \*(Ps = \*9\*0 \(-> Set foreground color to Black
  \*(Ps = \*9\*1 \(-> Set foreground color to Red
  \*(Ps = \*9\*2 \(-> Set foreground color to Green
  \*(Ps = \*9\*3 \(-> Set foreground color to Yellow
  \*(Ps = \*9\*4 \(-> Set foreground color to Blue
  \*(Ps = \*9\*5 \(-> Set foreground color to Magenta
  \*(Ps = \*9\*6 \(-> Set foreground color to Cyan
  \*(Ps = \*9\*7 \(-> Set foreground color to White
  \*(Ps = \*1\*0\*0 \(-> Set background color to Black
  \*(Ps = \*1\*0\*1 \(-> Set background color to Red
  \*(Ps = \*1\*0\*2 \(-> Set background color to Green
  \*(Ps = \*1\*0\*3 \(-> Set background color to Yellow
  \*(Ps = \*1\*0\*4 \(-> Set background color to Blue
  \*(Ps = \*1\*0\*5 \(-> Set background color to Magenta
  \*(Ps = \*1\*0\*6 \(-> Set background color to Cyan
  \*(Ps = \*1\*0\*7 \(-> Set background color to White
.sp
If \fIxterm\fP is compiled with the 16-color support disabled, it supports
the following, from \fIrxvt\fP:
  \*(Ps = \*1\*0\*0 \(-> Set foreground and background color to default
.sp
If 88- or 256-color support is compiled, the following apply.
  \*(Ps = \*3\*8 ; \*5 ; \*(Ps \(-> Set foreground color to the second \*(Ps
  \*(Ps = \*4\*8 ; \*5 ; \*(Ps \(-> Set background color to the second \*(Ps
.sp
.
.IP \\*(Cs\\*(Ps\\*s\\*n
Device Status Report (DSR)
  \*(Ps = \*5 \(-> Status Report \*(Cs\*0\*n (``OK'')
  \*(Ps = \*6 \(-> Report Cursor Position (CPR) [row;column] as
.br
\*(Cs\*(Ir\*s\*;\*(Ic\*s\*R
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*n
Device Status Report (DSR, DEC-specific)
  \*(Ps = \*6 \(-> Report Cursor Position (CPR) [row;column] as
\*(Cs\*?\*(Ir\*s\*;\*(Ic\*s\*R
(assumes page is zero).
  \*(Ps = \*1\*5 \(-> Report Printer status as
\*(Cs\*?\*1\*0\*s\*n (ready)
or
\*(Cs\*?\*1\*1\*s\*n (not ready)
  \*(Ps = \*2\*5 \(-> Report UDK status as
\*(Cs\*?\*2\*0\*s\*n (unlocked)
or
\*(Cs\*?\*2\*1\*s\*n (locked)
  \*(Ps = \*2\*6 \(-> Report Keyboard status as
.br
\*(Cs\*?\*2\*7\*s\*;\*s\*1\*s\*;\*s\*0\*s\*;\*s\*0\*s\*n (North American)
.br
The last two parameters apply to VT400 & up, and denote keyboard ready and LK01 respectively.
  \*(Ps = \*5\*3 \(-> Report Locator status as
.br
\*(Cs\*?\*5\*3\*s\*n Locator available, if compiled-in, or
.br
\*(Cs\*?\*5\*0\*s\*n No Locator, if not.
.
.IP \\*(Cs\\*s\\*!\\*p
Soft terminal reset (DECSTR)
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*(c"\\*p
Set conformance level (DECSCL)
Valid values for the first parameter:
  \*(Ps = \*6\*1 \(-> VT100
  \*(Ps = \*6\*2 \(-> VT200
  \*(Ps = \*6\*3 \(-> VT300
.br
Valid values for the second parameter:
  \*(Ps = \*0 \(-> 8-bit controls
  \*(Ps = \*1 \(-> 7-bit controls (always set for VT100)
  \*(Ps = \*2 \(-> 8-bit controls
.
.IP \\*(Cs\\*(Ps\\*s\\*(c"\\*q
Select character protection attribute (DECSCA).
Valid values for the parameter:
  \*(Ps = \*0 \(-> DECSED and DECSEL can erase (default)
  \*(Ps = \*1 \(-> DECSED and DECSEL cannot erase
  \*(Ps = \*2 \(-> DECSED and DECSEL can erase
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*r
Set Scrolling Region [top;bottom] (default = full size of window) (DECSTBM)
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*r
Restore DEC Private Mode Values.  The value of \*(Ps previously saved is
restored.  \*(Ps values are the same as for DECSET.
.
.IP \\*(Cs\\*(cs
Save cursor (ANSI.SYS)
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*(cs
Save DEC Private Mode Values.  \*(Ps values are the same as for DECSET.
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*t
Window manipulation (from \fIdtterm\fP, as well as extensions).
Valid values for the first (and any additional parameters) are:
  \*(Ps = \*1 \(-> De-iconify window.
  \*(Ps = \*2 \(-> Iconify window.
  \*(Ps = \*3 ; \fIx ; y\fP \(-> Move window to [x, y].
  \*(Ps = \*4 ; \fIheight ; width\fP \(-> Resize the \fIxterm\fP window to height and width in pixels.
  \*(Ps = \*5 \(-> Raise the \fIxterm\fP window to the front of the stacking order.
  \*(Ps = \*6 \(-> Lower the \fIxterm\fP window to the bottom of the stacking order.
  \*(Ps = \*7 \(-> Refresh the \fIxterm\fP window.
  \*(Ps = \*8 ; \fIheight ; width\fP \(-> Resize the text area to [height;width] in characters.
  \*(Ps = \*9 ; \*0 \(-> Restore maximized window.
  \*(Ps = \*9 ; \*1 \(-> Maximize window (i.e., resize to screen size).
  \*(Ps = \*1\*1 \(-> Report \fIxterm\fP window state.
If the \fIxterm\fP window is open (non-iconified), it returns \*(Cs\*1\*t.
If the \fIxterm\fP window is iconified, it returns \*(Cs\*2\*t.
  \*(Ps = \*1\*3 \(-> Report \fIxterm\fP window position as
\*(Cs\*3\*;\*(Ix\*;\*(Iy\*t
  \*(Ps = \*1\*4 \(-> Report \fIxterm\fP window in pixels as
\*(Cs\*s\*4\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*1\*8 \(-> Report the size of the text area in characters as
\*(Cs\*s\*8\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*1\*9 \(-> Report the size of the screen in characters as
\*(Cs\*s\*9\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*2\*0 \(-> Report \fIxterm\fP window's icon label as
\*(Os\*s\*L\*s\fIlabel\fP\*s\*(ST
  \*(Ps = \*2\*1 \(-> Report \fIxterm\fP window's title as
\*(Os\*s\*l\*s\fItitle\fP\*s\*(ST
  \*(Ps >= \*2\*4 \(-> Resize to \*(Ps lines (DECSLPP)
.
.IP \\*(Cs\\*u
Save cursor (ANSI.SYS)
.
.IP \\*(Cs\\*(Pt\\*s\\*;\\*(Pl\\*s\\*;\\*(Pb\\*s\\*;\\*(Pr\\*s\\*(qu\\*w
Enable Filter Rectangle (DECEFR)
.br
Parameters are [top;left;bottom;right].
.br
Defines the coordinates of a filter rectangle and activates it.
Anytime the locator is detected outside of the filter rectangle,
an outside rectangle event is generated and the rectangle is disabled.
Filter rectangles are always treated as "one-shot" events.
Any parameters that are omitted default to the current locator position.
If all parameters are omitted, any locator motion will be reported.
DECELR always cancels any prevous rectangle definition.
.
.IP \\*(Cs\\*(Ps\\*s\\*x
Request Terminal Parameters (DECREQTPARM)
.br
if \*(Ps is a "0" (default) or "1", and \fBxterm\fR is emulating VT100,
the control sequence elicits a response of the same form
whose parameters describe the terminal:
  \*(Ps \(-> the given \*(Ps incremented by 2.
  \*1 \(-> no parity
  \*1 \(-> eight bits
  \*1 \*2 \*8 \(-> transmit 38.4k baud
  \*1 \*2 \*8 \(-> receive 38.4k baud
  \*1 \(-> clock multiplier
  \*0 \(-> STP flags
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Pu\\*s\\*(qu\\*z
Enable Locator Reporting (DECELR)
.br
Valid values for the first parameter:
  \*(Ps = \*0 \(-> Locator disabled (default)
  \*(Ps = \*1 \(-> Locator enabled
  \*(Ps = \*2 \(-> Locator enabled for one report, then disabled
.br
The second parameter specifies the coordinate unit for locator reports.
.br
Valid values for the second parameter:
  \*(Pu = \*0 or omitted \(-> default to character cells
  \*(Pu = \*1 \(-> device physical pixels
  \*(Pu = \*2 \(-> character cells
.
.IP \\*(Cs\\*(Pm\\*s\\*(qu\\*{
Select Locator Events (DECSLE)
.br
Valid values for the first (and any additional parameters) are:
  \*(Ps = \*0 \(-> only respond to explicit host requests (DECRQLP)
             (default) also cancels any filter rectangle
  \*(Ps = \*1 \(-> report button down transitions
  \*(Ps = \*2 \(-> do not report button down transitions
  \*(Ps = \*3 \(-> report button up transitions
  \*(Ps = \*4 \(-> do not report button up transitions
.
.IP \\*(Cs\\*(Ps\\*s\\*(qu\\*|
Request Locator Position (DECRQLP)
.br
Valid values for the parameter are:
  \*(Ps = \*0, 1 or omitted \(-> transmit a single DECLRP locator report
.sp
If Locator Reporting has been enabled by a DECELR, xterm will respond
with a DECLRP Locator Report.  This report is also generated on button
up and down events if they have been enabled with a DECSLE, or when
the locator is detected outside of a filter rectangle, if filter rectangles
have been enabled with a DECEFR.
.sp
  \(-> \*(Cs\*(Pe\*s\*;\*(Pb\*s\*;\*(Pr\*s\*;\*(Pc\*s\*;\*(Pp\*s\*&\*s\*w
.sp
Parameters are [event;button;row;column;page].
.br
Valid values for the event:
  \*(Pe = \*0 \(-> locator unavailable - no other parameters sent
  \*(Pe = \*1 \(-> request - xterm received a DECRQLP
  \*(Pe = \*2 \(-> left button down
  \*(Pe = \*3 \(-> left button up
  \*(Pe = \*4 \(-> middle button down
  \*(Pe = \*5 \(-> middle button up
  \*(Pe = \*6 \(-> right button down
  \*(Pe = \*7 \(-> right button up
  \*(Pe = \*8 \(-> M4 button down
  \*(Pe = \*9 \(-> M4 button up
  \*(Pe = \*1\*0 \(-> locator outside filter rectangle
.br
``button'' parameter is a bitmask indicating which buttons are pressed:
  \*(Pb = \*0 \(-> no buttons down
  \*(Pb & \*1 \(-> right button down
  \*(Pb & \*2 \(-> middle button down
  \*(Pb & \*4 \(-> left button down
  \*(Pb & \*8 \(-> M4 button down
.br
``row'' and ``column'' parameters are the coordinates of the locator
position in the xterm window, encoded as ASCII decimal.
.br
The ``page'' parameter is not used by xterm, and will be omitted.
.
.Ed
.
.St
.LP
.B
Operating System Controls
.
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(ST
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(Be
Set Text Parameters.
For colors and font,
if \*(Pt is a "?", the control sequence elicits a response which consists
of the control sequence which would set the corresponding value.
The \fIdtterm\fP control sequences allow you to determine the icon name
and window title.
  \*(Ps = \*0 \(-> Change Icon Name and Window Title to \*(Pt
  \*(Ps = \*1 \(-> Change Icon Name to \*(Pt
  \*(Ps = \*2 \(-> Change Window Title to \*(Pt
  \*(Ps = \*3 \(-> Set X property on top-level window. \*(Pt should be in the form
"\fIprop=value\fP", or just "\fIprop\fP" to delete the property
  \*(Ps = \*4 ; \fIc ; spec\fP \(-> Change Color Number \fIc\fP to
the color specified by \fIspec\fP,
i.e., a name or RGB specification as per \fIXParseColor\fP.
Any number of \fIc name\fP pairs may be given.
The color numbers correspond to the ANSI colors 0-7,
their bright versions 8-15,
and if supported, the remainder of the 88-color or 256-color table.
.sp
If a "?" is given rather than a name or RGB specification,
xterm replies with a control sequence of the same form which can be used
to set the corresponding color.
Because more than one pair of color number and specification can
be given in one control sequence, \fBxterm\fR can make more than one reply.
.sp
The 8 colors which may be set using \*1\*0 through \*1\*7 are
denoted \fIdynamic colors\fR, since the corresponding control sequences
were the first means for setting \fBxterm\fR's colors dynamically,
i.e., after it was started.
They are not the same as the ANSI colors.
One or more parameters is expected for \*(Pt.
Each succesive parameter changes the next color in the list.
The value of \*(Ps tells the starting point in the list.
The colors are specified by name or RGB specification as per \fIXParseColor\fP.
.sp
If a "?" is given rather than a name or RGB specification,
xterm replies with a control sequence of the same form which can be used
to set the corresponding dynamic color.
Because more than one pair of color number and specification can
be given in one control sequence, \fBxterm\fR can make more than one reply.
.sp
  \*(Ps = \*1\*0 \(-> Change VT100 text foreground color to \*(Pt
  \*(Ps = \*1\*1 \(-> Change VT100 text background color to \*(Pt
  \*(Ps = \*1\*2 \(-> Change text cursor color to \*(Pt
  \*(Ps = \*1\*3 \(-> Change mouse foreground color to \*(Pt
  \*(Ps = \*1\*4 \(-> Change mouse background color to \*(Pt
  \*(Ps = \*1\*5 \(-> Change Tektronix foreground color to \*(Pt
  \*(Ps = \*1\*6 \(-> Change Tektronix background color to \*(Pt
  \*(Ps = \*1\*7 \(-> Change highlight color to \*(Pt
.sp
  \*(Ps = \*4\*6 \(-> Change Log File to \*(Pt (normally disabled by a
compile-time option)
  \*(Ps = \*5\*0 \(-> Set Font to \*(Pt
If \*(Pt begins with a "#", index in the font menu, relative (if the
next character is a plus or minus sign) or absolute.  A number is
expected but not required after the sign (the default is the current
entry for relative, zero for absolute indexing).
.Ed
.
.St
.LP
.B
Privacy Message
.IP \\*(PM\\*(Pt\\*s\\*(ST
\fIxterm\fP implements no \*(PM functions; \*(Pt is ignored.
\*(Pt need not be printable characters.
.Ed
.
.SH
PC-Style Function Keys
.ds RH Function Keys
.LP
If \fIxterm\fP does minimal translation of the function keys, it usually does this
with a PC-style keyboard, so PC-style function keys result.
Sun keyboards are similar to PC keyboards.
Both have cursor and scrolling operations printed on the keypad,
which duplicate the smaller cursor and scrolling keypads.
.LP
X does not predefine NumLock (used for VT220 keyboards) or Alt (used as
an extension for the Sun/PC keyboards) as modifiers.
These keys are recognized as modifiers when enabled
by the \fBnumLock\fP resource,
or by the DECSET \*1\*0\*3\*5 control sequence.
.LP
The cursor keys transmit the following escape sequences depending on the
mode specified via the \fBDECCKM\fP escape sequence.
.TS
center;
cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
Cursor Up	\*(Cs\*A	\*(S3\*A
Cursor Down	\*(Cs\*(cB	\*(S3\*(cB
Cursor Right	\*(Cs\*C	\*(S3\*C
Cursor Left	\*(Cs\*D	\*(S3\*D
_
.TE
The home- and end-keys
(unlike PageUp and other keys also on the 6-key editing keypad)
are considered "cursor keys" by \fIxterm\fP.
Their mode is also controlled by the \fBDECCKM\fP escape sequence:
.TS
center;
cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
Home	\*(Cs\*H	\*(S3\*H
End	\*(Cs\*F	\*(S3\*F
_
.TE
.LP
The application keypad transmits the following escape sequences depending on the
mode specified via the \fBDECPNM\fP and \fBDECPAM\fP escape sequences.
Use the NumLock key to override the application mode.
.LP
Not all keys are present on the Sun/PC keypad (e.g., PF1, Tab), but are supported by
the program.
.TS
center;
cf3	cf3	cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application	Terminfo	Termcap
_
Space	\*(Sp	\*(S3\*(Sp	-	-
Tab	\*(Ta	\*(S3\*I	-	-
Enter	\*(Cr	\*(S3\*M	kent	@8
PF1	\*(S3\*P	\*(S3\*P	kf1	k1
PF2	\*(S3\*Q	\*(S3\*Q	kf2	k2
PF3	\*(S3\*R	\*(S3\*R	kf3	k3
PF4	\*(S3\*S	\*(S3\*S	kf4	k4
* \f1(multiply)\fP	\**	\*(S3\*j	-	-
+ \f1(add)\fP	\*+	\*(S3\*k	-	-
, \f1(comma)\fP	\*,	\*(S3\*l	-	-
- \f1(minus)\fP	\*-	\*(S3\*m	-	-
\&. \f1(Delete)\fP	\*.	\*(Cs\*3\*(c~	-	-
/ \f1(divide)\fP	\*/	\*(S3\*o	-	-
0 \f1(Insert)\fP	\*0	\*(Cs\*2\*(c~	-	-
1 \f1(End)\fP	\*1	\*(S3\*F	kc1	K4
2 \f1(DownArrow)\fP	\*2	\*(Cs\*(cB	-	-
3 \f1(PageDown)\fP	\*3	\*(Cs\*6\*(c~	kc3	K5
4 \f1(LeftArrow)\fP	\*4	\*(Cs\*D	-	-
5 \f1(Begin)\fP	\*5	\*(Cs\*E	kb2	K2
6 \f1(RightArrow)\fP	\*6	\*(Cs\*C	-	-
7 \f1(Home)\fP	\*7	\*(S3\*H	ka1	K1
8 \f1(UpArrow)\fP	\*8	\*(Cs\*A	-	-
9 \f1(PageUp)\fP	\*9	\*(Cs\*5\*(c~	ka3	K3
= (equal)	\*=	\*(S3\*(XX	-	-
_
.TE
They also provide 12 function keys, as well as a few other special-purpose keys.
.TS
center;
cf3	cf3
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
F1	\*(S3\*P
F2	\*(S3\*Q
F3	\*(S3\*R
F4	\*(S3\*S
F5	\*(Cs\*1\*5\*(c~
F6	\*(Cs\*1\*7\*(c~
F7	\*(Cs\*1\*8\*(c~
F8	\*(Cs\*1\*9\*(c~
F9	\*(Cs\*2\*0\*(c~
F10	\*(Cs\*2\*1\*(c~
F11	\*(Cs\*2\*3\*(c~
F12	\*(Cs\*2\*4\*(c~
_
.TE
.LP
Older versions of \fIxterm\fP implement different escape sequences for F1 through F4.
These can be activated by setting the \fIoldXtermFKeys\fP resource.
However, since they do not correspond to any hardware terminal, they have been deprecated.
(The DEC VT220 reserves F1 through F5 for local functions such as \fBSetup\fP).
.TS
center;
cf3	cf3
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
F1	\*(Cs\*1\*1\*(c~
F2	\*(Cs\*1\*2\*(c~
F3	\*(Cs\*1\*3\*(c~
F4	\*(Cs\*1\*4\*(c~
_
.TE
In normal mode, i.e., a Sun/PC keyboard
when the \fBsunKeyboard\fP resource is false,
\fIxterm\fP recognizes function key modifiers
which are parameters appended before the final character
of the control sequence.
.TS
center;
cf3	lf3
cf3w(2c)	lf3w(2c) .
Code	Modifiers
_
2	Shift
3	Alt
4	Shift + Alt
5	Control
6	Shift + Control
7	Alt + Control
8	Shift + Alt + Control
_
.TE
For example, shift-F5 would be sent as
\*(Cs\*1\*5\*;\*2\*(c~

If the \fBalwaysUseMods\fP resource is set, the Meta modifier also is
recognized, making parameters 9 through 16.
.SH
VT220-Style Function Keys
.LP
However, \fIxterm\fP is most useful as a DEC VT102 or VT220 emulator.
Set the \fBsunKeyboard\fP resource to true to force a Sun/PC keyboard
to act like a VT220 keyboard.
.LP
The VT102/VT220 application keypad transmits unique escape sequences in
application mode, which are distinct from the cursor and scrolling keypad:
.TS
center;
cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application
_
Space	\*(Sp	\*(S3\*(Sp
Tab	\*(Ta	\*(S3\*I
Enter	\*(Cr	\*(S3\*M
PF1	\*(S3\*P	\*(S3\*P
PF2	\*(S3\*Q	\*(S3\*Q
PF3	\*(S3\*R	\*(S3\*R
PF4	\*(S3\*S	\*(S3\*S
* \f1(multiply)\fP	\**	\*(S3\*j
+ \f1(add)\fP	\*+	\*(S3\*k
, \f1(comma)\fP	\*,	\*(S3\*l
- \f1(minus)\fP	\*-	\*(S3\*m
\&. \f1(period)\fP	\*.	\*(S3\*n
/ \f1(divide)\fP	\*/	\*(S3\*o
0	\*0	\*(S3\*p
1	\*1	\*(S3\*q
2	\*2	\*(S3\*r
3	\*3	\*(S3\*(cs
4	\*4	\*(S3\*t
5	\*5	\*(S3\*u
6	\*6	\*(S3\*v
7	\*7	\*(S3\*w
8	\*8	\*(S3\*x
9	\*9	\*(S3\*y
= (equal)	\*=	\*(S3\*(XX
_
.TE
The VT220 provides a 6-key editing keypad,
which is analogous to that on the PC keyboard.
It is not affected by \fBDECCKM\fP or \fBDECPNM\fP/\fBDECPAM\fP:
.TS
center;
cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
\f1Insert\fP	\*(Cs\*2\*(c~	\*(Cs\*2\*(c~
\f1Delete\fP	\*(Cs\*3\*(c~	\*(Cs\*3\*(c~
\f1Home\fP	\*(Cs\*1\*(c~	\*(Cs\*1\*(c~
\f1End\fP	\*(Cs\*4\*(c~	\*(Cs\*4\*(c~
\f1PageUp\fP	\*(Cs\*5\*(c~	\*(Cs\*5\*(c~
\f1PageDown\fP	\*(Cs\*6\*(c~	\*(Cs\*6\*(c~
_
.TE
.LP
The VT220 provides 8 additional function keys.
With a Sun/PC keyboard, access these keys by Control/F1 for F13, etc.
.TS
center;
cf3	cf3
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
F13	\*(Cs\*2\*5\*(c~
F14	\*(Cs\*2\*6\*(c~
F15	\*(Cs\*2\*8\*(c~
F16	\*(Cs\*2\*9\*(c~
F17	\*(Cs\*3\*1\*(c~
F18	\*(Cs\*3\*2\*(c~
F19	\*(Cs\*3\*3\*(c~
F20	\*(Cs\*3\*4\*(c~
_
.TE
.SH
VT52-Style Function Keys
.LP
A VT52 does not have function keys, but it does have a numeric keypad and cursor keys.
They differ from the other emulations by the prefix.
Also, the cursor keys do not change:
.TS
center;
cf3	cf3
lf3w(2c)	lf3w(2c) .
Key	Normal/Application
_
Cursor Up	\*(Es\*A
Cursor Down	\*(Es\*(cB
Cursor Right	\*(Es\*C
Cursor Left	\*(Es\*D
_
.TE
The keypad is similar:
.TS
center;
cf3	cf3	cf3
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application
_
Space	\*(Sp	\*(Es\*?\*(Sp
Tab	\*(Ta	\*(Es\*?\*I
Enter	\*(Cr	\*(Es\*?\*M
PF1	\*(Es\*P	\*(Es\*P
PF2	\*(Es\*Q	\*(Es\*Q
PF3	\*(Es\*R	\*(Es\*R
PF4	\*(Es\*S	\*(Es\*S
* \f1(multiply)\fP	\**	\*(Es\*?\*j
+ \f1(add)\fP	\*+	\*(Es\*?\*k
, \f1(comma)\fP	\*,	\*(Es\*?\*l
- \f1(minus)\fP	\*-	\*(Es\*?\*m
\&. \f1(period)\fP	\*.	\*(Es\*?\*n
/ \f1(divide)\fP	\*/	\*(Es\*?\*o
0	\*0	\*(Es\*?\*p
1	\*1	\*(Es\*?\*q
2	\*2	\*(Es\*?\*r
3	\*3	\*(Es\*?\*(cs
4	\*4	\*(Es\*?\*t
5	\*5	\*(Es\*?\*u
6	\*6	\*(Es\*?\*v
7	\*7	\*(Es\*?\*w
8	\*8	\*(Es\*?\*x
9	\*9	\*(Es\*?\*y
= (equal)	\*=	\*(Es\*?\*(XX
_
.TE
.SH
Sun-Style Function Keys
.LP
The \fIxterm\fP program provides support for Sun keyboards more directly, by
a menu toggle that causes it to send Sun-style function key codes rather than VT220.
Note, however, that the \fIsun\fP and \fIVT100\fP emulations are not really
compatible.  For example, their wrap-margin behavior differs.
.LP
Only function keys are altered; keypad and cursor keys are the same.
The emulation responds identically.
See the xterm-sun terminfo entry for details.
.SH
HP-Style Function Keys
.LP
Similarly, \fIxterm\fP can be compiled to support HP keyboards.
See the xterm-hp terminfo entry for details.
.SH
Mouse Tracking
.ds RH Mouse Tracking
.LP
The VT widget can be set to send the mouse position and other
information on button presses.  These modes are typically used by
editors and other full-screen applications that want to make use of
the mouse.

There are six mutually exclusive modes.
One is DEC Locator mode, enabled by the
DECELR \*(Cs\*(Ps\*s\*;\*(Ps\*s\*s\*'\*s\*z
control sequence, and is not described here
(control sequences are summarized above).
The remaining five
modes are each enabled (or disabled) by a different parameter in
DECSET \*(Cs\*?\*(Pm\*s\*h
or
DECRST \*(Cs\*?\*(Pm\*s\*l
control sequence.

Manifest constants for the parameter values
are defined in \fBxcharmouse.h\fP as follows:
.
.DS B
#define SET_X10_MOUSE               9
#define SET_VT200_MOUSE             1000
#define SET_VT200_HIGHLIGHT_MOUSE   1001
#define SET_BTN_EVENT_MOUSE         1002
#define SET_ANY_EVENT_MOUSE         1003
.DE
.LP
The motion reporting modes are strictly \fIxterm\fP extensions, and are not
part of any standard, though they are analogous to the DEC VT200 DECELR
locator reports.

Parameters (such as pointer position and button number) for all mouse
tracking escape sequences generated by \fIxterm\fP
encode numeric parameters in a single character as
\fIvalue\fP+32.
For example, \*! specifies the value 1.
The upper left character position on the terminal is denoted as 1,1.

X10 compatibility mode sends an escape sequence only on button press,
encoding the location and the mouse button pressed.
It is enabled by specifying parameter 9 to DECSET.
On button press, \fIxterm\fP sends
\*(Cs\*M\*(Cb\*(Cx\*(Cy (6 characters).
\*(Cb is button\-1.
\*(Cx and \*(Cy are the x and y coordinates of the mouse when the
button was pressed.

Normal tracking mode sends an escape sequence on both button press and
release.  Modifier key (shift, ctrl, meta) information is also sent.
It is enabled by specifying parameter 1000 to DECSET.
On button press or release, \fIxterm\fP sends
\*(Cs\*M\*(Cb\*(Cx\*(Cy.
The low two bits of \*(Cb encode button information:
0=MB1 pressed, 1=MB2 pressed, 2=MB3 pressed, 3=release.
The next three bits encode the modifiers which were down when the button was
pressed and are added together:  4=Shift, 8=Meta, 16=Control.
Note however that the shift and control bits are normally unavailable
because \fIxterm\fP uses the control modifier with mouse for popup menus,
and the shift modifier is used in the default translations for button events.
The \fIMeta\fP modifier recognized by \fIxterm\fP is the \fImod1\fP mask,
and is not necessarily the "Meta" key (see \fIxmodmap\fP).
\*(Cx and \*(Cy are the x and y coordinates of the mouse event, encoded as
in X10 mode.

Wheel mice may return buttons 4 and 5.
Those buttons are represented by the same event codes
as buttons 1 and 2 respectively,
except that 64 is added to the event code.
Release events for the wheel buttons are not reported.

Mouse hilite tracking notifies a program of a button press, receives a
range of lines from the program, highlights the region covered by
the mouse within that range until button release, and then sends the
program the release coordinates.
It is enabled by specifying parameter 1001 to DECSET.
Highlighting is performed only for button 1, though other button events
can be received.
\fBWarning:\fP use of this mode requires a cooperating program or it will
hang \fIxterm.\fP
On button press, the same information as for normal tracking is
generated; \fIxterm\fP then waits for the program to send mouse
tracking information.
\fIAll X events are ignored until the proper escape sequence is
received from the pty:\fP
\*(Cs\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*T.
The parameters are \fIfunc, startx, starty, firstrow,\fP and \fIlastrow.\fP
\fIfunc\fP is non-zero to initiate hilite tracking and zero to abort.
\fIstartx\fP and \fIstarty\fP give the starting x and y location for
the highlighted region.  The ending location tracks the mouse, but
will never be above row \fIfirstrow\fP and will always be above row
\fIlastrow.\fP  (The top of the screen is row 1.)
When the button is released, \fIxterm\fP reports the ending position
one of two ways: if the start and end coordinates are valid text
locations: \*(Cs\*t\*(Cx\*(Cy.  If either coordinate is past
the end of the line:
\*(Cs\*T\*(Cx\*(Cy\*(Cx\*(Cy\*(Cx\*(Cy.
The parameters are \fIstartx, starty, endx, endy, mousex,\fP and \fImousey.\fP
\fIstartx, starty, endx, \fPand\fI endy\fP give the starting and
ending character positions of the region.  \fImousex\fP and \fImousey\fP
give the location of the mouse at button up, which may not be over a
character.

Button-event tracking is essentially the same as normal tracking, but
\fIxterm\fP also reports button-motion events.  Motion events
are reported only if the mouse pointer has moved to a different character
cell.  It is enabled by specifying parameter 1002 to DECSET.
On button press or release, \fIxterm\fP sends the same codes used by normal
tracking mode.  On button-motion events, \fIxterm\fP adds 32 to the event code
(the third character, \*(Cb).
The other bits of the event code specify button and modifier keys as in normal mode.
For example, motion into cell x,y with button 1
down is reported as \*(Cs\*M\*@\*(Cx\*(Cy.
( \*@ = 32 + 0 (button 1) + 32 (motion indicator) ).
Similarly, motion with button 3
down is reported as \*(Cs\*M\*(cB\*(Cx\*(Cy.
( \*(cB = 32 + 2 (button 3) + 32 (motion indicator) ).

Any-event mode is the same as button-event mode, except that all motion
events are reported, even if no mouse button is down.
It is enabled by specifying 1003 to DECSET.
.
.br
.ds RH Tektronix 4014 Mode
.SH
Tektronix 4014 Mode
.LP
Most of these sequences are standard Tektronix 4014 control sequences.
Graph mode supports the 12-bit addressing of the Tektronix 4014.
The major features missing are
the write-through and defocused modes.
This document does not describe the commands used in the various
Tektronix plotting modes but does describe the commands to switch modes.
.St
.IP \\*(Be
Bell (Ctrl-G)
.IP \\*(Bs
Backspace (Ctrl-H)
.IP \\*(Ta
Horizontal Tab (Ctrl-I)
.IP \\*(Lf
Line Feed or New Line (Ctrl-J)
.IP \\*(Vt
Cursor up (Ctrl-K)
.IP \\*(Ff
Form Feed or New Page (Ctrl-L)
.IP \\*(Cr
Carriage Return (Ctrl-M)
.IP \\*(Es\\*(Et
Switch to VT100 Mode (ESC Ctrl-C)
.IP \\*(Es\\*(En
Return Terminal Status (ESC Ctrl-E)
.IP \\*(Es\\*(Ff
PAGE (Clear Screen) (ESC Ctrl-L)
.IP \\*(Es\\*(So
Begin 4015 APL mode (ignored by \fIxterm\fP) (ESC Ctrl-N)
.IP \\*(Es\\*(Si
End 4015 APL mode (ignored by \fIxterm\fP) (ESC Ctrl-O)
.IP \\*(Es\\*(Eb
COPY (Save Tektronix Codes to file COPYyyyy-mm-dd.hh:mm:ss) (ESC Ctrl-W)
.IP \\*(Es\\*(Ca
Bypass Condition (ESC Ctrl-X)
.IP \\*(Es\\*(Su
GIN mode (ESC Ctrl-Z)
.IP \\*(Es\\*(Fs
Special Point Plot Mode (ESC Ctrl-\e)
.IP \\*(Es\\*8
Select Large Character Set
.IP \\*(Es\\*9
Select #2 Character Set
.IP \\*(Es\\*:
Select #3 Character Set
.IP \\*(Es\\*;
Select Small Character Set
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(Be
Set Text Parameters of VT window
  \*(Ps = \*0 \(-> Change Icon Name and Window Title to \*(Pt
  \*(Ps = \*1 \(-> Change Icon Name to \*(Pt
  \*(Ps = \*2 \(-> Change Window Title to \*(Pt
  \*(Ps = \*4\*6 \(-> Change Log File to \*(Pt (normally disabled by a
compile-time option)
.IP \\*(Es\\*`
Normal Z Axis and Normal (solid) Vectors
.IP \\*(Es\\*a
Normal Z Axis and Dotted Line Vectors
.IP \\*(Es\\*b
Normal Z Axis and Dot-Dashed Vectors
.IP \\*(Es\\*c
Normal Z Axis and Short-Dashed Vectors
.IP \\*(Es\\*d
Normal Z Axis and Long-Dashed Vectors
.IP \\*(Es\\*h
Defocused Z Axis and Normal (solid) Vectors
.IP \\*(Es\\*i
Defocused Z Axis and Dotted Line Vectors
.IP \\*(Es\\*j
Defocused Z Axis and Dot-Dashed Vectors
.IP \\*(Es\\*k
Defocused Z Axis and Short-Dashed Vectors
.IP \\*(Es\\*l
Defocused Z Axis and Long-Dashed Vectors
.IP \\*(Es\\*p
Write-Thru Mode and Normal (solid) Vectors
.IP \\*(Es\\*q
Write-Thru Mode and Dotted Line Vectors
.IP \\*(Es\\*r
Write-Thru Mode and Dot-Dashed Vectors
.IP \\*(Es\\*(cs
Write-Thru Mode and Short-Dashed Vectors
.IP \\*(Es\\*t
Write-Thru Mode and Long-Dashed Vectors
.IP \\*(Fs
Point Plot Mode (Ctrl-\e)
.IP \\*(Gs
Graph Mode (Ctrl-])
.IP \\*(Rs
Incremental Plot Mode (Ctrl-^)
.IP \\*(Us
Alpha Mode (Ctrl-_)
.Ed
.
.
.br
.ds RH VT52 Mode
.SH
VT52 Mode
.LP
Parameters for cursor movement are at the end of the \*(Es\*Y escape sequence.
Each ordinate is encoded in a single character as \fIvalue\fP+32.
For example, \*! is 1.
The screen coodinate system is 0-based.
.St
.IP \\*(Es\\*A
Cursor up.
.IP \\*(Es\\*(cB
Cursor down.
.IP \\*(Es\\*C
Cursor right.
.IP \\*(Es\\*D
Cursor left.
.IP \\*(Es\\*F
Enter graphics mode.
.IP \\*(Es\\*G
Exit graphics mode.
.IP \\*(Es\\*H
Move the cursor to the home position.
.IP \\*(Es\\*I
Reverse line feed.
.IP \\*(Es\\*J
Erase from the cursor to the end of the screen.
.IP \\*(Es\\*K
Erase from the cursor to the end of the line.
.IP \\*(Es\\*Y\\*(Ps\\*s\\*(Ps
Move the cursor to given row and column.
.IP \\*(Es\\*Z
Identify
  \(-> \*(Es\*s\*/\*s\*Z (``I am a VT52.'')
.IP \\*(Es\\*=
Enter alternate keypad mode.
.IP \\*(Es\\*>
Exit alternate keypad mode.
.IP \\*(Es\\*<
Exit VT52 mode (Enter VT100 mode).
.Ed
.
.if n .pl \n(nlu+1v
