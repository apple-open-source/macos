/* $XConsortium: lex.l,v 1.8 94/03/31 20:40:44 rws Exp $ */
/* $XFree86: xc/programs/xgc/lex.l,v 1.4 2002/05/31 18:46:13 dawes Exp $ */
/*
** lex file for xgc syntax
*/

/* Lots of stuff stolen from gwm's wool.lex */

%{

#include <X11/X.h>
#include "gram.h"
#include "constants.h"
#if defined(FLEX_SCANNER) && !defined(YY_FLEX_LEX_COMPAT) && !defined(__UNIXOS2__)
int yylineno = 0;
#endif
%}

%p				4000
%a				3000

number				[0-9]+
word				[^\n\t ]+
string				\"([^"]|\\.)*\"

%%

[Rr]un				{ return (RUN); }

[Ff]unction			{ return (FUNCTION); }
[Cc]lear			{ yylval.num = GXclear;
				  return (FUNCTIONTYPE); }
[Aa]nd				{ yylval.num = GXand;
				  return (FUNCTIONTYPE); }
[Aa]ndReverse			{ yylval.num = GXandReverse;
				  return (FUNCTIONTYPE); }
[Cc]opy				{ yylval.num = GXcopy;
				  return (FUNCTIONTYPE); }
[Aa]ndInverted			{ yylval.num = GXandInverted;
				  return (FUNCTIONTYPE); }
[Nn]oop				{ yylval.num = GXnoop;
				  return (FUNCTIONTYPE); }
[Xx]or				{ yylval.num = GXxor;
				  return (FUNCTIONTYPE); }
[Oo]r				{ yylval.num = GXor;
				  return (FUNCTIONTYPE); }
[Nn]or				{ yylval.num = GXnor;
				  return (FUNCTIONTYPE); }
[Ee]quiv			{ yylval.num = GXequiv;
				  return (FUNCTIONTYPE); }
[Ii]nvert			{ yylval.num = GXinvert;
				  return (FUNCTIONTYPE); }
[Oo]rReverse			{ yylval.num = GXorReverse;
				  return (FUNCTIONTYPE); }
[Cc]opyInverted			{ yylval.num = GXcopyInverted;
				  return (FUNCTIONTYPE); }
[Oo]rInverted			{ yylval.num = GXorInverted;
				  return (FUNCTIONTYPE); }
[Nn]and				{ yylval.num = GXnand;
				  return (FUNCTIONTYPE); }
[Ss]et				{ yylval.num = GXset;
				  return (FUNCTIONTYPE); }

[Tt]est				{ return (TEST); }
CopyArea			{ yylval.num = CopyArea;
				  return (TESTTYPE); }
CopyPlane			{ yylval.num = CopyPlane;
				  return (TESTTYPE); }
PolyPoint			{ yylval.num = PolyPoint;
				  return (TESTTYPE); }
PolyLine			{ yylval.num = PolyLine;
				  return (TESTTYPE); }
PolySegment			{ yylval.num = PolySegment;
				  return (TESTTYPE); }
PolyRectangle			{ yylval.num = PolyRectangle;
				  return (TESTTYPE); }
PolyArc				{ yylval.num = PolyArc;
				  return (TESTTYPE); }
FillPolygon			{ yylval.num = FillPolygon;
				  return (TESTTYPE); }
PolyFillRect			{ yylval.num = PolyFillRect;
				  return (TESTTYPE); }
PolyFillArc			{ yylval.num = PolyFillArc;
				  return (TESTTYPE); }
PutImage			{ yylval.num = PutImage;
				  return (TESTTYPE); }
GetImage			{ yylval.num = GetImage;
				  return (TESTTYPE); }
PolyText8			{ yylval.num = PolyText8;
				  return (TESTTYPE); }
ImageText8			{ yylval.num = ImageText8;
				  return (TESTTYPE); }
PolyText16			{ yylval.num = PolyText16;
				  return (TESTTYPE); }
ImageText16			{ yylval.num = ImageText16;
				  return (TESTTYPE); }

[Ll]inestyle			{ return (LINESTYLE); }
OnOffDash			{ yylval.num = LineOnOffDash;
				  return (LINESTYLETYPE); }
DoubleDash			{ yylval.num = LineDoubleDash;
				  return (LINESTYLETYPE); }

[Cc]apstyle			{ return (CAPSTYLE); }
NotLast				{ yylval.num = CapNotLast;
				  return (CAPSTYLETYPE); }
Butt				{ yylval.num = CapButt;
				  return (CAPSTYLETYPE); }
Projecting			{ yylval.num = CapProjecting;
				  return (CAPSTYLETYPE); }

[Jj]oinstyle			{ return (JOINSTYLE); }
Miter				{ yylval.num = JoinMiter;
				  return (JOINSTYLETYPE); }
Bevel				{ yylval.num = JoinBevel;
				  return (JOINSTYLETYPE); }

Round				{ return (ROUND); }

[Ff]illstyle			{ return (FILLSTYLE); }
Tiled				{ yylval.num = FillTiled;
				  return (FILLSTYLETYPE); }
Stippled			{ yylval.num = FillStippled;
				  return (FILLSTYLETYPE); }
OpaqueStippled			{ yylval.num = FillOpaqueStippled;
				  return (FILLSTYLETYPE); }

Solid				{ return (SOLID); }

[Ff]illrule			{ return (FILLRULE); }
EvenOdd				{ yylval.num = EvenOddRule;
				  return (FILLRULETYPE); }
Winding				{ yylval.num = WindingRule;
				  return (FILLRULETYPE); }

[Aa]rcmode			{ return (ARCMODE); }
Chord				{ yylval.num = ArcChord;
				  return (ARCMODETYPE); }
PieSlice			{ yylval.num = ArcPieSlice;
				  return (ARCMODETYPE); }

[Ff]oreground			{ return (FOREGROUND); }
[Bb]ackground			{ return (BACKGROUND); }
[Ll]inewidth			{ return (LINEWIDTH); }
[Pp]lanemask			{ return (PLANEMASK); }
[Dd]ashlist			{ return (DASHLIST); }
[Ff]ont				{ return (FONT); }
[Pp]ercent			{ return (PERCENT); }

{number}			{ (void) sscanf ((char *)yytext, "%d", &yylval.num);
				  return (NUMBER); }
{string}			{ yylval.ptr = (char *) yytext;
				  return (STRING); }
\#[^\n]*\n			{ ; }
[\t ]				{ ; }
\n				{ return ('\n'); }

{word}				{ yylval.ptr = (char *) yytext;
				  return (STRING); }

.				{ fprintf(stderr,
					"xgc: bad character `%s', line %d\n",
					yytext, yylineno); }

%%

#ifndef yywrap
int yywrap() { return (1); }
#endif
