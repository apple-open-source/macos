/* $Xorg: psgeom.c,v 1.4 2000/08/17 19:54:50 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
/* $XFree86: xc/programs/xkbprint/psgeom.c,v 1.5 2001/07/25 15:05:25 dawes Exp $ */

#define	XK_TECHNICAL
#define	XK_PUBLISHING
#define	XK_KATAKANA
#include <stdio.h>
#include <ctype.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKM.h>
#include <X11/extensions/XKBfile.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#if defined(sgi)
#include <malloc.h>
#endif

#define	DEBUG_VAR_NOT_LOCAL
#define	DEBUG_VAR debugFlags
#include <stdlib.h>

#include "utils.h"
#include "xkbprint.h"
#include "isokeys.h"

#define	FONT_NONE	-1
#define	FONT_TEXT	0
#define	FONT_LATIN1	1
#define	FONT_SYMBOL	2
#define	FONT_ISOCAPS	3
#define	FONT_MOUSECAPS	4

typedef struct {
	Display *	dpy;
	XkbDescPtr	xkb;
	XkbGeometryPtr	geom;
	int		totalKB;
	int		kbPerPage;
	int		black;
	int		white;
	int		color;
	int		font;
	int		fontSize;
	int		nPages;
	int		x1,y1;
	int		x2,y2;
	XKBPrintArgs *	args;
} PSState;

#define	G1L1		0
#define	G1L2		1
#define	G2L1		2
#define	G2L2		3
#define	CENTER		4

#define	G1L1_MASK	(1<<G1L1)
#define	G1L2_MASK	(1<<G1L2)
#define	G2L1_MASK	(1<<G2L1)
#define	G2L2_MASK	(1<<G2L2)
#define	CENTER_MASK	(1<<CENTER)

#define	LABEL_MASK	(0x1f)
#define	GXL1_MASK	(G1L1_MASK|G2L1_MASK)
#define	GXL2_MASK	(G1L2_MASK|G2L2_MASK)
#define	G1LX_MASK	(G1L1_MASK|G1L2_MASK)
#define	G2LX_MASK	(G2L1_MASK|G2L2_MASK)
#define	GXLX_MASK	(0x0f)

#define	NLABELS		5
#define	LABEL_LEN	30

#define	SZ_AUTO		0
#define	SZ_TINY		1
#define	SZ_SMALL	2
#define	SZ_MEDIUM	3
#define	SZ_LARGE	4
#define	SZ_XLARGE	5

typedef struct {
	unsigned	present;
	Bool		alpha[2];
	char 		label[NLABELS][LABEL_LEN];
	int		font[NLABELS];
	int		size[NLABELS];
} KeyTop;

#define	DFLT_LABEL_FONT "Helvetica-Narrow-Bold"
#define DFLT_LABEL_FONT_SIZE 10

/***====================================================================***/

typedef struct _PSFontDef {
    char *name;
    char **def;
} PSFontDef;

static PSFontDef internalFonts[] = {
	{ "IsoKeyCaps", IsoKeyCaps }
};
static int nInternalFonts =	(sizeof(internalFonts)/sizeof(PSFontDef));

static void
ListInternalFonts(FILE *out, int first, int indent)
{
register int i,n,nThisLine;

    for (n=0;n<first;n++) {
	putc(' ',out);
    }

    for (nThisLine=i=0;i<nInternalFonts;i++) {
	if (nThisLine==4) {
	    fprintf(out,",\n");
	    for (n=0;n<indent;n++) {
		putc(' ',out);
	    }
	    nThisLine= 0;
	}
	if (nThisLine==0)
	     fprintf(out,"%s",internalFonts[i].name);
	else fprintf(out,", %s",internalFonts[i].name);
	nThisLine++;
    }
    if (nThisLine!=0)
	fprintf(out,"\n");
    return;
}

static Bool
PSIncludeFont(FILE *out, char *font)
{
char **		pstr;
register int	i;

    pstr= NULL;
    for (i=0;(i<nInternalFonts)&&(pstr==NULL);i++) {
	if (uStringEqual(internalFonts[i].name,font))
	    pstr= internalFonts[i].def;
    }
    if (pstr!=NULL) {
	fprintf(out,"%%%%BeginFont: %s\n",font);
	while (*pstr!=NULL) {
	    fprintf(out,"%s\n",*pstr);
	    pstr++;
	}
	fprintf(out,"%%%%EndFont\n");
	return True;
    }
    return False;
}

Bool
DumpInternalFont(FILE *out, char *fontName)
{
    if (strcmp(fontName,"IsoKeyCaps")!=0) {
	uError("No internal font named \"%s\"\n",fontName);
	uAction("No font dumped\n");
	fprintf(stderr,"Current internal fonts are: ");
	ListInternalFonts(stderr,0,8);
	return False;
    }
    PSIncludeFont(out,fontName);
    return True;
}

/***====================================================================***/

static void
PSColorDef(FILE *out, PSState *state, XkbColorPtr color)
{
int	tmp;

    fprintf(out,"/C%03d ",color->pixel);
    if (uStrCaseEqual(color->spec,"black")) {
	state->black= color->pixel;
	fprintf(out,"{ 0 setgray } def      %% %s\n",color->spec);
    }
    else if (uStrCaseEqual(color->spec,"white")) {
	state->white= color->pixel;
	fprintf(out,"{ 1 setgray } def      %% %s\n",color->spec);
    }
    else if ((sscanf(color->spec,"grey%d",&tmp)==1)||
	(sscanf(color->spec,"gray%d",&tmp)==1)||
	(sscanf(color->spec,"Grey%d",&tmp)==1)||
	(sscanf(color->spec,"Gray%d",&tmp)==1)) {
	fprintf(out,"{ %f setgray } def	    %% %s\n",1.0-(((float)tmp)/100.0),
								color->spec);
    }
    else if ((tmp=(uStrCaseEqual(color->spec,"red")*100))||
	     (sscanf(color->spec,"red%d",&tmp)==1)) {
	 fprintf(out,"{ %f 0 0 setrgbcolor } def %% %s\n",(((float)tmp)/100.0),
								color->spec);
    }
    else if ((tmp=(uStrCaseEqual(color->spec,"green")*100))||
	     (sscanf(color->spec,"green%d",&tmp)==1)) {
	 fprintf(out,"{ 0 %f 0 setrgbcolor } def %% %s\n",(((float)tmp)/100.0),
								color->spec);
    }
    else if ((tmp=(uStrCaseEqual(color->spec,"blue")*100))||
	     (sscanf(color->spec,"blue%d",&tmp)==1)) {
	 fprintf(out,"{ 0 0 %f setrgbcolor } def %% %s\n",(((float)tmp)/100.0),
								color->spec);
    }
    else fprintf(out,"{ 0.9 setgray       } def %% BOGUS! %s\n",color->spec);
}

static void
PSSetColor(FILE *out,PSState *state,int color)
{
    if ((state->args->wantColor)&&(state->color!=color)) {
	fprintf(out,"C%03d %% set color\n",color);
	state->color= color;
    }
    return;
}

static void
PSGSave(FILE *out, PSState *state)
{
    fprintf(out,"gsave\n");
    return;
}

static void
PSGRestore(FILE *out, PSState *state)
{
    fprintf(out,"grestore\n");
    state->color= -1;
    state->font= FONT_NONE;
    state->fontSize= -1;
    return;
}

static void
PSShapeDef(FILE *out, PSState *state, XkbShapePtr shape)
{
int		o,p;
XkbOutlinePtr	ol;

    fprintf(out,"/%s {\n",XkbAtomGetString(state->dpy,shape->name));
    fprintf(out,"	gsave translate rotate /SOLID exch def\n");
    for (o=0,ol=shape->outlines;o<shape->num_outlines;o++,ol++) {
	XkbPointPtr	pt;
	if ((shape->num_outlines>1)&&(ol==shape->approx))
	    continue;
	pt=ol->points;
	fprintf(out,"%%	Outline %d\n",o+1);
	if (ol->num_points==1) {
	    if (ol->corner_radius<1) {
		fprintf(out,"	  0   0 moveto\n");
		fprintf(out,"	%3d   0 lineto\n",pt->x);
		fprintf(out,"	%3d %3d lineto\n",pt->x,pt->y);
		fprintf(out,"	  0 %3d lineto\n",pt->y);
		fprintf(out,"	  0   0 lineto\n");
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
	    }
	    else {
		fprintf(out,"	mark\n");
		fprintf(out,"	%3d   0 moveto\n",ol->corner_radius);
		fprintf(out,"	%3d   0 %3d %3d %3d arcto\n",pt->x,pt->x,pt->y,
							ol->corner_radius);
		fprintf(out,"	%3d %3d   0 %3d %3d arcto\n",pt->x,pt->y,pt->y,
							ol->corner_radius);
		fprintf(out,"	  0 %3d   0   0 %3d arcto\n",pt->y,
							ol->corner_radius);
		fprintf(out,"     0   0 %3d   0 %3d arcto\n",pt->x,
							ol->corner_radius);
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
		fprintf(out,"	cleartomark\n");
	    }
	}
	else if (ol->num_points==2) {
	    if (ol->corner_radius<1) {
		fprintf(out,"	%3d %3d moveto\n",pt[0].x,pt[0].y);
		fprintf(out,"	%3d %3d lineto\n",pt[1].x,pt[0].y);
		fprintf(out,"	%3d %3d lineto\n",pt[1].x,pt[1].y);
		fprintf(out,"	%3d %3d lineto\n",pt[0].x,pt[1].y);
		fprintf(out,"	%3d %3d lineto\n",pt[0].x,pt[0].y);
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
	    }
	    else {
		fprintf(out,"	mark\n");
		fprintf(out,"	%3d %3d moveto\n",pt[0].x,pt[0].y);
		fprintf(out,"	%3d %3d %3d %3d %3d arcto\n",pt[1].x,pt[0].y,
							pt[1].x,pt[1].y,
							ol->corner_radius);
		fprintf(out,"	%3d %3d %3d %3d %3d arcto\n",pt[1].x,pt[1].y,
							pt[0].x,pt[1].y,
							ol->corner_radius);
		fprintf(out,"	%3d %3d %3d %3d %3d arcto\n",pt[0].x,pt[1].y,
							pt[0].x,pt[0].y,
							ol->corner_radius);
		fprintf(out,"   %3d %3d %3d %3d %3d arcto\n",pt[0].x,pt[0].y,
							pt[1].x,pt[0].y,
							ol->corner_radius);
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
		fprintf(out,"	cleartomark\n");
	    }
	}
	else {
	    if (ol->corner_radius<1) {
		fprintf(out,"	%3d %3d moveto\n",pt->x,pt->y);
		pt++;
		for (p=1;p<ol->num_points;p++,pt++) {
		    fprintf(out,"	%3d %3d lineto\n",pt->x,pt->y);
		}
		if ((pt->x!=ol->points[0].x)||(pt->y!=ol->points[0].y))
		    fprintf(out,"	closepath\n");
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
	    }
	    else {
		XkbPointPtr	last;
		last= &pt[ol->num_points-1];
		if ((last->x==pt->x)&&(last->y==pt->y))
		    last--;
		fprintf(out,"	mark\n");
		fprintf(out,"	%% Determine tangent point of first corner\n");
		fprintf(out,"	%3d %3d moveto %d %d %d %d %d arcto\n",
					last->x,last->y,
					pt[0].x,pt[0].y,pt[1].x,pt[1].y,
					ol->corner_radius);
		fprintf(out,"	/TY exch def /TX exch def pop pop newpath\n");
		fprintf(out,"	%% Now draw the shape\n");
		fprintf(out,"	TX TY moveto\n");
		for (p=1;p<ol->num_points;p++) {
		    if (p<(ol->num_points-1))	last= &pt[p+1];
		    else			last= &pt[0];
		    fprintf(out,"	%3d %3d %3d %3d %3d arcto\n",
						pt[p].x,pt[p].y,
						last->x,last->y,
						ol->corner_radius);
		}
		last= &pt[ol->num_points-1];
		if ((last->x!=pt->x)||(last->y!=pt->y)) {
		    fprintf(out,"	%3d %3d %3d %3d %3d arcto\n",
						pt[0].x,pt[0].y,
						pt[1].x,pt[1].y,
						ol->corner_radius);
		}
		fprintf(out,"	SOLID { fill } { stroke } ifelse\n");
		fprintf(out,"	cleartomark\n");
	    }
	}
    }
    fprintf(out,"	grestore\n");
    fprintf(out,"} def\n");
    return;
}

/***====================================================================***/

typedef	struct {
	char *		foundry;
	char *		face;
	char *		weight;
	char *		slant;
	char *		setWidth;
	char *		variant;
	int		pixelSize;
	int		ptSize;
	int		resX;
	int		resY;
	char *		spacing;
	int		avgWidth;
	char *		encoding;
} FontStuff;

static void
ClearFontStuff(FontStuff *stuff)
{
    if (stuff && stuff->foundry)
	uFree(stuff->foundry);
    bzero(stuff,sizeof(FontStuff));
    return;
}

static Bool
CrackXLFDName(char *name, FontStuff *stuff)
{
char *tmp;
    if ((name==NULL)||(stuff==NULL))
	return False;
    if (name[0]=='-')	tmp= uStringDup(&name[1]);
    else		tmp= uStringDup(name);
    if (tmp==NULL)
	return False;
    stuff->foundry= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else 						*tmp++= '\0';
    stuff->face= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else 						*tmp++= '\0';
    stuff->weight= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    stuff->slant= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    stuff->setWidth= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    stuff->variant= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    if (*tmp=='*')					stuff->pixelSize= 0;
    else if (sscanf(tmp,"%i",&stuff->pixelSize)!=1)	goto BAILOUT;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else 						*tmp++= '\0';
    if (*tmp=='*')					stuff->ptSize= 0;
    else if (sscanf(tmp,"%i",&stuff->ptSize)!=1)	goto BAILOUT;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    if (*tmp=='*')					stuff->resX= 0;
    else if (sscanf(tmp,"%i",&stuff->resX)!=1)		goto BAILOUT;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else 						*tmp++= '\0';
    if (*tmp=='*')					stuff->resY= 0;
    else if (sscanf(tmp,"%i",&stuff->resY)!=1)		goto BAILOUT;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    stuff->spacing= tmp;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						*tmp++= '\0';
    if (*tmp=='*')					stuff->avgWidth= 0;
    else if (sscanf(tmp,"%i",&stuff->avgWidth)!=1)	goto BAILOUT;

    if ((tmp= index(tmp,'-'))==NULL)			goto BAILOUT;
    else						 *tmp++= '\0';
    stuff->encoding= tmp;
    return True;
BAILOUT:
    ClearFontStuff(stuff);
    return False;
}

static void
PSSetUpForLatin1(FILE *out, PSState *state)
{
    fprintf(out,"save\n");
    fprintf(out,"/ISOLatin1Encoding where {pop save true}{false} ifelse\n");
    fprintf(out,"/ISOLatin1Encoding [\n");
    fprintf(out,"   /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n");
    fprintf(out,"   /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n");
    fprintf(out,"   /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n");
    fprintf(out,"   /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n");
    fprintf(out,"   /.notdef /.notdef /.notdef /.notdef /.notdef /.notdef\n");
    fprintf(out,"   /.notdef /.notdef /space /exclam /quotedbl /numbersign\n");
    fprintf(out,"   /dollar /percent /ampersand /quoteright /parenleft\n");
    fprintf(out,"   /parenright /asterisk /plus /comma /minus /period\n");
    fprintf(out,"   /slash /zero /one /two /three /four /five /six /seven\n");
    fprintf(out,"   /eight /nine /colon /semicolon /less /equal /greater\n");
    fprintf(out,"   /question /at /A /B /C /D /E /F /G /H /I /J /K /L /M\n");
    fprintf(out,"   /N /O /P /Q /R /S /T /U /V /W /X /Y /Z /bracketleft\n");
    fprintf(out,"   /backslash /bracketright /asciicircum /underscore\n");
    fprintf(out,"   /quoteleft /a /b /c /d /e /f /g /h /i /j /k /l /m\n");
    fprintf(out,"   /n /o /p /q /r /s /t /u /v /w /x /y /z /braceleft\n");
    fprintf(out,"   /bar /braceright /asciitilde /guilsinglright /fraction\n");
    fprintf(out,"   /florin /quotesingle /quotedblleft /guilsinglleft /fi\n");
    fprintf(out,"   /fl /endash /dagger /daggerdbl /bullet /quotesinglbase\n");
    fprintf(out,"   /quotedblbase /quotedblright /ellipsis /trademark\n");
    fprintf(out,"   /perthousand /grave /scaron /circumflex /Scaron /tilde\n");
    fprintf(out,"   /breve /zcaron /dotaccent /dotlessi /Zcaron /ring\n");
    fprintf(out,"   /hungarumlaut /ogonek /caron /emdash /space /exclamdown\n");
    fprintf(out,"   /cent /sterling /currency /yen /brokenbar /section\n");
    fprintf(out,"   /dieresis /copyright /ordfeminine /guillemotleft\n");
    fprintf(out,"   /logicalnot /hyphen /registered /macron /degree\n");
    fprintf(out,"   /plusminus /twosuperior /threesuperior /acute /mu\n");
    fprintf(out,"   /paragraph /periodcentered /cedilla /onesuperior\n");
    fprintf(out,"   /ordmasculine /guillemotright /onequarter /onehalf\n");
    fprintf(out,"   /threequarters /questiondown /Agrave /Aacute\n");
    fprintf(out,"   /Acircumflex /Atilde /Adieresis /Aring /AE /Ccedilla\n");
    fprintf(out,"   /Egrave /Eacute /Ecircumflex /Edieresis /Igrave /Iacute\n");
    fprintf(out,"   /Icircumflex /Idieresis /Eth /Ntilde /Ograve /Oacute\n");
    fprintf(out,"   /Ocircumflex /Otilde /Odieresis /multiply /Oslash\n");
    fprintf(out,"   /Ugrave /Uacute /Ucircumflex /Udieresis /Yacute /Thorn\n");
    fprintf(out,"   /germandbls /agrave /aacute /acircumflex /atilde\n");
    fprintf(out,"   /adieresis /aring /ae /ccedilla /egrave /eacute\n");
    fprintf(out,"   /ecircumflex /edieresis /igrave /iacute /icircumflex\n");
    fprintf(out,"   /idieresis /eth /ntilde /ograve /oacute /ocircumflex\n");
    fprintf(out,"   /otilde /odieresis /divide /oslash /ugrave /uacute\n");
    fprintf(out,"   /ucircumflex /udieresis /yacute /thorn /ydieresis\n");
    fprintf(out,"] def {restore} if\n");
    fprintf(out,"/reencodeISO-1 {\n");
    fprintf(out,"    dup length dict begin\n");
    fprintf(out,"        {1 index /FID ne {def}{pop pop} ifelse} forall\n");
    fprintf(out,"        /Encoding ISOLatin1Encoding def\n");
    fprintf(out,"        currentdict\n");
    fprintf(out,"    end\n");
    fprintf(out,"} def\n");
}

static void
PSReencodeLatin1Font(FILE *out, char *font)
{
    fprintf(out,"/%s findfont reencodeISO-1\n",font);
    fprintf(out,"	/%s-8859-1 exch definefont pop\n",font);
    return;
}

static void
PSSetUpFonts(FILE *out, char *textFont, int size)
{
    fprintf(out,"/F%d { /%s findfont exch scalefont setfont } def\n",
						FONT_TEXT,textFont);
    fprintf(out,"/F%d { /%s-8859-1 findfont exch scalefont setfont } def\n",
						FONT_LATIN1,textFont);
    fprintf(out,"/F%d { /%s findfont exch scalefont setfont } def\n",
						FONT_SYMBOL,"Symbol");
    fprintf(out,"/F%d { /%s findfont exch scalefont setfont } def\n",
						FONT_ISOCAPS,"IsoKeyCaps");
    return;
}

static void
PSSetFont(FILE *out, PSState *state, int font, int size, int pts)
{
    if ((state->font!=font)||(state->fontSize!=size)) {
	fprintf(out,"%d %sF%d\n",size,(pts?"pts ":""),font);
	state->font= font;
	state->fontSize= size;
    }
    return;
}


static void
PSProlog(FILE *out, PSState *state)
{
register int	i;

    if (!state->args->wantEPS) {
	fprintf(out,"%%!PS-Adobe-2.0\n");
	fprintf(out,"%%%%Creator: xkbprint\n");
	if (state->geom->name!=None)
	    fprintf(out,"%%%%Title: %s\n",
				XkbAtomGetString(state->dpy,state->geom->name));
	fprintf(out,"%%%%BoundingBox: (atend)\n");
	fprintf(out,"%%%%Pages: 1\n");
	fprintf(out,"%%%%PageOrder: Ascend\n");
	fprintf(out,"%%%%DocumentFonts: (atend)\n");
	fprintf(out,"%%%%DocumentData: Clean7Bit\n");
	fprintf(out,"%%%%Orientation: Landscape\n");
	fprintf(out,"%%%%EndComments\n");
	fprintf(out,"%%%%BeginProlog\n");
	fprintf(out,"%% Resolution is 1/10mm -- need pt sizes for fonts\n");
	fprintf(out,"clippath pathbbox\n");
	fprintf(out,"    /ury exch def /urx exch def\n");
	fprintf(out,"    /llx exch def /lly exch def\n");
	fprintf(out,"    newpath\n");
	fprintf(out,"/devwidth  urx llx sub def\n");
	fprintf(out,"/devheight ury lly sub def\n");
    }
    else {
	int w,h;
	int pw,ph;

	w= (((state->geom->width_mm*72)/254)*11)/10;
	h= (((state->geom->height_mm*72)/254)*11)/10;
	if (state->kbPerPage>1)
	    h*= (state->kbPerPage+1);

	if (w<=h) {
	    pw= 7.5*72;
	    ph= 10*72;
	}
	else {
	    pw= 10*72;
	    ph= 7.5*72;
	}
	while ((w>pw)||(h>ph)) {
	   w= (w*9)/10;
	   h= (h*9)/10;
	}

	fprintf(out,"%%!PS-Adobe-2.0 EPSF-2.0\n");
	fprintf(out,"%%%%BoundingBox: 0 0 %d %d\n",w,h);
	fprintf(out,"%%%%Creator: xkbprint\n");
	if (state->geom->name!=None)
	    fprintf(out,"%%%%Title: %s\n",
				XkbAtomGetString(state->dpy,state->geom->name));
	fprintf(out,"%%%%Pages: 1\n");
	fprintf(out,"%%%%EndComments\n");
	fprintf(out,"%%%%BeginProlog\n");
	fprintf(out,"/ury 0 def /urx 0 def\n");
	fprintf(out,"/llx %d def /lly %d def\n",w,h);
	fprintf(out,"/devwidth %d def /devheight %d def\n",w,h);
    }
    fprintf(out,"/kbdwidth %d def\n",state->geom->width_mm);
    fprintf(out,"/kbdheight %d def\n",state->geom->height_mm);
    fprintf(out,"/pts { 254 mul 72 div } def\n");
    fprintf(out,"/mm10 { 72 mul 254 div } def\n");
    fprintf(out,"/landscape? {\n");
    fprintf(out,"	devheight devwidth gt {\n");
    fprintf(out,"		/pwidth devheight def /pheight devwidth def\n");
    fprintf(out,"		0 devheight translate\n");
    fprintf(out,"		-90 rotate\n");
    fprintf(out,"	} {\n");
    fprintf(out,"		/pwidth devwidth def /pheight devheight def\n");
    fprintf(out,"	} ifelse\n");
    fprintf(out,"	0 pheight translate\n");
    fprintf(out,"	1 -1 scale\n");
    fprintf(out,"} def\n");
    fprintf(out,"/centeroffset {\n");
    fprintf(out,"    /S     exch def\n");
    fprintf(out,"    /HEIGHT exch def\n");
    fprintf(out,"    /WIDTH exch def\n");
    fprintf(out,"    S stringwidth /SH exch def /SW exch def\n");
    fprintf(out,"    WIDTH SW sub 2 div\n");
    fprintf(out,"    HEIGHT SH sub 2 div\n");
    fprintf(out,"} def\n");
    PSSetUpForLatin1(out,state);
    PSReencodeLatin1Font(out,DFLT_LABEL_FONT);
    if (state->args->wantColor) {
	XkbGeometryPtr geom= state->geom;
	for (i=0;i<geom->num_colors;i++) {
	    PSColorDef(out,state,&geom->colors[i]);
	}
	if (state->black<0) {
	    XkbColorPtr	color;
	    if (!(color= XkbAddGeomColor(geom,"black",geom->num_colors)))
		uFatalError("Couldn't allocate black color!\n");
	    PSColorDef(out,state,color);
	}
	if (state->white<0) {
	    XkbColorPtr	color;
	    if (!(color= XkbAddGeomColor(geom,"white",geom->num_colors)))
		uFatalError("Couldn't allocate white color!\n");
	    PSColorDef(out,state,color);
	}
    }
    for (i=0;i<state->geom->num_shapes;i++) {
	PSShapeDef(out,state,&state->geom->shapes[i]);
    }
    if (state->args->label==LABEL_SYMBOLS) {
	PSIncludeFont(out,"IsoKeyCaps");
    }
    PSSetUpFonts(out,DFLT_LABEL_FONT, DFLT_LABEL_FONT_SIZE);
    fprintf(out,"%%%%EndProlog\n");
    return;
}

static void
PSFileTrailer(FILE *out, PSState *state)
{
    fprintf(out,"restore\n");
    if (!state->args->wantEPS)
	fprintf(out,"%%%%Trailer\n");
    fprintf(out,"%%%%EOF\n");
#ifdef NOTYET
    fprintf(out,"%%%%BoundingBox %d %d\n");
    fprintf(out,"%%%%DocumentFonts: \n");
#endif
    return;
}

static void
PSPageSetup(FILE *out, PSState *state, Bool drawBorder)
{
XkbGeometryPtr	geom;

    geom= state->geom;
    if (state->kbPerPage==1) {
	fprintf(out,"%%%%Page: %d %d\n",state->nPages+1,state->nPages+1);
	fprintf(out,"%%%%BeginPageSetup\n");
    }
    else if ((state->nPages&1)==0) { /* even page */
	int realPage;
	realPage= state->nPages/2+1;
	fprintf(out,"%%%%Page: %d %d\n",realPage,realPage);
	fprintf(out,"%%%%BeginPageSetup\n");
	fprintf(out,"%% Keyboard %d\n",state->nPages+1);
	if (state->nPages==0) {
	    fprintf(out,"/realwidth devwidth def\n");
	    fprintf(out,"/realheight devheight def\n");
	    fprintf(out,"/devheight realheight 3 div def\n");
	}
	fprintf(out,"0 devheight dup 2 div add translate\n");
    }
    else  {
	fprintf(out,"%% Keyboard %d\n",state->nPages+1);
    }
    fprintf(out,"save\n");
    fprintf(out,"landscape?\n");
    if (state->args->scaleToFit) {
	fprintf(out,"%% Scale keyboard to fit on the page\n");
	fprintf(out,"/kbdscale pwidth 72 sub kbdwidth div def\n");
	fprintf(out,"/kbdscalewidth kbdwidth kbdscale mul def\n");
	fprintf(out,"/kbdscaleheight kbdheight kbdscale mul def\n");
	fprintf(out,"/kbx 36 def\n");
	fprintf(out,"/kby pheight kbdscaleheight sub 2 div def\n");
	PSGSave(out,state);
	fprintf(out,"kbx kby translate\n");
	fprintf(out,"kbdscale kbdscale scale\n");
    }
    else {
	fprintf(out,"%% Draw keyboard full size\n");
	fprintf(out,"/kbdscale 1 def\n");
	fprintf(out,"/kbdscalewidth kbdwidth mm10 def\n");
	fprintf(out,"/kbdscaleheight kbdheight mm10 def\n");
	fprintf(out,"/kbx pwidth kbdscalewidth sub 2 div def\n");
	fprintf(out,"/kby pheight kbdscaleheight sub 2 div def\n");
	PSGSave(out,state);
	fprintf(out,"kbx kby translate\n");
	fprintf(out,"72 254 div dup scale\n");
    }
    if (drawBorder) {
	if (state->args->wantColor) {
	    PSSetColor(out,state,geom->base_color->pixel);
	    fprintf(out,"  0   0 moveto\n");
	    fprintf(out,"%3d   0 lineto\n",geom->width_mm);
	    fprintf(out,"%3d %3d lineto\n",geom->width_mm,geom->height_mm);
	    fprintf(out,"  0 %3d lineto\n",geom->height_mm);
	    fprintf(out,"closepath fill\n");
	}
	PSSetColor(out,state,state->black);
	fprintf(out,"  0   0 moveto\n");
	fprintf(out,"%3d   0 lineto\n",geom->width_mm);
    	fprintf(out,"%3d %3d lineto\n",geom->width_mm,geom->height_mm);
	fprintf(out,"  0 %3d lineto\n",geom->height_mm);
	fprintf(out,"closepath stroke\n");
    }
    fprintf(out,"%%%%EndPageSetup\n");
    return;
}

static void
PSPageTrailer(FILE *out, PSState *state)
{
char *		name;
XkbDescPtr	xkb;
XkbGeometryPtr	geom;
XkbPropertyPtr	prop;
int		p,baseline;

    xkb= state->xkb;
    geom= state->geom;
    if (state->args->grid>0) {
	fprintf(out,"%% Draw a %dmm grid\n",state->args->grid);
	fprintf(out,"0 setlinewidth\n");
	fprintf(out,"0.25 setgray\n");
	fprintf(out," 0 %d %d {\n",state->args->grid*10,geom->width_mm);
	fprintf(out,"    /GX exch def\n");
	fprintf(out,"    GX 0 moveto GX %d lineto stroke\n",geom->height_mm);
	fprintf(out,"} for\n");
	fprintf(out," 0 %d %d {\n",state->args->grid*10,geom->height_mm);
	fprintf(out,"    /GY exch def\n");
	fprintf(out,"    0 GY moveto %d GY lineto stroke\n",geom->width_mm);
	fprintf(out,"} for\n");
    }
    PSGRestore(out,state);
    name= NULL;
    for (p=0,prop=geom->properties;p<geom->num_properties;p++,prop++) {
	if ((prop->value!=NULL)&&(uStrCaseEqual(prop->name,"description"))) {
	    name= prop->value;
	    break;
	}
    }
    if ((!state->args->wantEPS)&&
			((state->kbPerPage==1)||((state->nPages&1)==1)||
					(state->nPages==state->totalKB))) {
	if ((name==NULL)&&(geom->name!=None))
	    name= XkbAtomGetString(state->dpy,geom->name);

	baseline= 16;
	if ((name!=NULL)||(state->args->label==LABEL_SYMBOLS)) {
	    PSSetColor(out,state,state->black);
	    PSSetFont(out,state,FONT_LATIN1,14,False);
	}
	if (state->args->label==LABEL_SYMBOLS) {
	    char buf[40],*sName= NULL;
	    Atom sAtom;

	    if (state->args->nLabelGroups==1)
		sprintf(buf,"Group %d",state->args->baseLabelGroup+1);
	    else sprintf(buf,"Groups %d-%d",state->args->baseLabelGroup+1,
		state->args->baseLabelGroup+state->args->nLabelGroups);
	    fprintf(out,"kbx kbdscalewidth 0 (%s) centeroffset pop add\n",buf);
	    fprintf(out,"    kby kbdscaleheight add %d add\n",baseline);
	    fprintf(out,"    moveto\n");
	    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",buf);
	    baseline+= 16;

	    if (xkb->names!=NULL)	sAtom= xkb->names->symbols;
	    else			sAtom= None;
	    if (sAtom!=None)
		sName= XkbAtomGetString(state->dpy,sAtom);
	    if (sName==NULL)
		sName= "(unknown)";

	    sprintf(buf,"Layout: %s",sName);
	    fprintf(out,"kbx kbdscalewidth 0 (%s) centeroffset pop add\n",buf);
	    fprintf(out,"    kby kbdscaleheight add %d add\n",baseline);
	    fprintf(out,"    moveto\n");
	    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",buf);
	    baseline+= 16;
	}
	if (name!=NULL) {
	    fprintf(out,"kbx kbdscalewidth 0 (%s) centeroffset pop add\n",name);
	    fprintf(out,"    kby kbdscaleheight add %d add\n",baseline);
	    fprintf(out,"    moveto\n");
	    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",name);
	    baseline+= 16;
	}
	if (state->args->label==LABEL_KEYCODE) {
	    char buf[40],*sName= NULL;
	    Atom sAtom;

	    if (xkb->names!=NULL)	sAtom= xkb->names->keycodes;
	    else			sAtom= None;
	    if (sAtom!=None)
		sName= XkbAtomGetString(state->dpy,sAtom);
	    if (sName==NULL)
		sName= "(unknown)";

	    sprintf(buf,"Keycodes: %s",sName);
	    fprintf(out,"kbx kbdscalewidth 0 (%s) centeroffset pop add\n",buf);
	    fprintf(out,"    kby kbdscaleheight add %d add\n",baseline);
	    fprintf(out,"    moveto\n");
	    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",buf);
	    baseline+= 16;
	}
	if (state->args->copies>1) {
	    for (p=1;p<state->args->copies;p++)
		fprintf(out,"copypage\n");
	}
	fprintf(out,"showpage\n");
	fprintf(out,"restore\n");
	fprintf(out,"%% Done with keyboard/page %d\n",state->nPages+1);
    }
    else {
	if ((!state->args->wantEPS)&&(state->args->label==LABEL_SYMBOLS)) {
	    char buf[40];
	    baseline= 16;
	    PSSetColor(out,state,state->black);
	    PSSetFont(out,state,FONT_LATIN1,14,False);
	    if (state->args->nLabelGroups==1)
		sprintf(buf,"Group %d",state->args->baseLabelGroup+1);
	    else sprintf(buf,"Groups %d-%d",state->args->baseLabelGroup+1,
		state->args->baseLabelGroup+state->args->nLabelGroups+1);
	    fprintf(out,"kbx kbdscalewidth 0 (%s) centeroffset pop add\n",buf);
	    fprintf(out,"    kby kbdscaleheight add %d add\n",baseline);
	    fprintf(out,"    moveto\n");
	    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",buf);
	    baseline+= 16;
	}
	fprintf(out,"restore\n");
	fprintf(out,"%% Done with keyboard %d\n",state->nPages+1);
	fprintf(out,"0 devheight -1 mul translate %% next keyboard\n");
    }
    state->nPages++;
    state->color= state->black;
    state->font= -1;
    return;
}

static void
PSDoodad(FILE *out, PSState *state, XkbDoodadPtr doodad)
{
XkbDescPtr	xkb;
char		*name,*dname;
int		sz,leading;

    xkb= state->xkb;
    if (doodad->any.name!=None)
	 dname= XkbAtomGetString(xkb->dpy,doodad->any.name);
    else dname= "NoName";
    switch (doodad->any.type) {
	case XkbOutlineDoodad:
	case XkbSolidDoodad:
	    name= XkbAtomGetString(xkb->dpy,
			XkbShapeDoodadShape(xkb->geom,&doodad->shape)->name);
	    if (state->args->wantColor) {
		PSSetColor(out,state,doodad->shape.color_ndx);
		if (doodad->any.type!=XkbOutlineDoodad) {
		    fprintf(out,"true %d %d %d %s %% Doodad %s\n",
					doodad->shape.angle,
					doodad->shape.left,doodad->shape.top,
					name,dname);
		    PSSetColor(out,state,state->black);
		}
		fprintf(out,"false %d %d %d %s %% Doodad %s\n",
			doodad->shape.angle,
			doodad->shape.left,doodad->shape.top,
			name,dname);
	    }
	    else {
		fprintf(out,"false %d %d %d %s %% Doodad %s\n",
			doodad->shape.angle,
			doodad->shape.left,doodad->shape.top,
			name,dname);
	    }
	    break;
	case XkbTextDoodad:
	    fprintf(out,"%% Doodad %s\n",dname);
	    PSSetColor(out,state,doodad->text.color_ndx);
	    PSGSave(out,state);
	    fprintf(out,"%d %d translate\n",doodad->text.left,doodad->text.top);
	    if (doodad->text.angle!=0)
		fprintf(out,"%s rotate\n",
				XkbGeomFPText(doodad->text.angle,XkbMessage));
	    sz= 14;
	    if (doodad->text.font) {
		FontStuff stuff;
		if (CrackXLFDName(doodad->text.font,&stuff)) {
		    if (stuff.ptSize>0)
			sz= stuff.ptSize/10;
		    ClearFontStuff(&stuff);
		}
	    }
	    PSSetFont(out,state,FONT_LATIN1,sz,True);
	    leading= (sz*12)/10;
	    if (strchr(doodad->text.text,'\n')==NULL) {
		fprintf(out,"0 %d pts moveto 1 -1 scale\n",(leading*8)/10);
		fprintf(out,"(%s) show\n",doodad->text.text);
	    }
	    else {
		char *tmp,*buf,*end;
		int   offset= (leading*8/10);
		tmp= buf= uStringDup(doodad->text.text);
		while (tmp!=NULL) {
		    end= strchr(tmp,'\n');
		    if (end!=NULL)
			*end++= '\0';
		    fprintf(out,"0 %d pts moveto 1 -1 scale\n",offset);
		    fprintf(out,"(%s) show 1 -1 scale\n",tmp);
		    offset+= leading;
		    tmp= end;
		}
		free(buf);
	    }
	    PSGRestore(out,state);
	    break;
	case XkbIndicatorDoodad:
	    name= XkbAtomGetString(xkb->dpy,
		XkbIndicatorDoodadShape(xkb->geom,&doodad->indicator)->name);
	    if (state->args->wantColor) {
		PSSetColor(out,state,doodad->indicator.off_color_ndx);
		fprintf(out,"true 0 %d %d %s %% Doodad %s\n",
			doodad->indicator.left,doodad->indicator.top,
			name,dname);
		PSSetColor(out,state,state->black);
	    }
	    fprintf(out,"false 0 %d %d %s %% Doodad %s\n",
			doodad->indicator.left,doodad->indicator.top,
			name,dname);
	    break;
	case XkbLogoDoodad:
	    name= XkbAtomGetString(xkb->dpy,
			XkbLogoDoodadShape(xkb->geom,&doodad->logo)->name);
	    if (state->args->wantColor)
		PSSetColor(out,state,doodad->shape.color_ndx);
	    fprintf(out,"false %d %d %d %s %% Doodad %s\n",
			doodad->shape.angle,
			doodad->shape.left,doodad->shape.top,
			name,dname);
	    break;
    }
    return;
}

/***====================================================================***/

static Bool
PSKeycapsSymbol(KeySym sym, unsigned char *buf, 
		int *font_rtrn, int *sz_rtrn, PSState *state)
{
    if (state->args->wantSymbols==NO_SYMBOLS)
	return False;

    if (font_rtrn!=NULL)
	*font_rtrn= FONT_ISOCAPS;
    if (sz_rtrn!=NULL)
	*sz_rtrn= SZ_LARGE;
    buf[1]= '\0';
    switch (sym) {
	case XK_Shift_L: case XK_Shift_R:
		buf[0]= XKC_ISO_Shift;		return True;
	case XK_Shift_Lock:
		buf[0]= XKC_ISO_Shift_Lock;	return True;
	case XK_ISO_Lock:
		buf[0]= XKC_ISO_Caps_Lock;	return True;
	case XK_BackSpace:
		buf[0]= XKC_ISO_Backspace;	return True;
	case XK_Return:	
		buf[0]= XKC_ISO_Return;		return True;
	case XK_Up: case XK_KP_Up:
		buf[0]= XKC_ISO_Up;		return True;
	case XK_Down: case XK_KP_Down:
		buf[0]= XKC_ISO_Down;		return True;
	case XK_Left: case XK_KP_Left:
		buf[0]= XKC_ISO_Left;		return True;
	case XK_Right: case XK_KP_Right:
		buf[0]= XKC_ISO_Right;		return True;
	case XK_Tab:
		buf[0]= XKC_ISO_Tab;		return True;
	case XK_ISO_Left_Tab:
		buf[0]= XKC_ISO_Left_Tab;	return True;
    }
    if (state->args->wantSymbols!=ALL_SYMBOLS)
	return False;
    switch (sym) {
	case XK_Caps_Lock:	
		buf[0]= XKC_ISO_Caps_Lock;	return True;
	case XK_Num_Lock:
		buf[0]= XKC_ISO_Num_Lock;	return True;
	case XK_ISO_Level3_Shift:
		buf[0]= XKC_ISO_Level3_Shift;	return True;
	case XK_ISO_Level3_Lock:
		buf[0]= XKC_ISO_Level3_Lock;	return True;
	case XK_ISO_Next_Group: case XK_ISO_Group_Shift:
		buf[0]= XKC_ISO_Next_Group;	return True;
	case XK_ISO_Next_Group_Lock:
		buf[0]= XKC_ISO_Next_Group_Lock;return True;
	case XK_space:
		buf[0]= XKC_ISO_Space;		return True;
	case XK_nobreakspace:
		buf[0]= XKC_ISO_No_Break_Space;	return True;
	case XK_Insert:
		buf[0]= XKC_ISO_Insert;		return True;
	case XK_ISO_Continuous_Underline:
		buf[0]= XKC_ISO_Continuous_Underline;return True;
	case XK_ISO_Discontinuous_Underline:
		buf[0]= XKC_ISO_Discontinuous_Underline;return True;
	case XK_ISO_Emphasize:
		buf[0]= XKC_ISO_Emphasize;	return True;
	case XK_Multi_key:
		buf[0]= XKC_ISO_Compose;	return True;
	case XK_ISO_Center_Object:
		buf[0]= XKC_ISO_Center_Object;	return True;
	case XK_Delete:
		buf[0]= XKC_ISO_Delete;		return True;
	case XK_Clear:
		buf[0]= XKC_ISO_Clear_Screen;	return True;
	case XK_Scroll_Lock:
		buf[0]= XKC_ISO_Scroll_Lock;	return True;
	case XK_Help:
		buf[0]= XKC_ISO_Help;		return True;
	case XK_Print:
		buf[0]= XKC_ISO_Print_Screen;	return True;
	case XK_ISO_Enter:	
		buf[0]= XKC_ISO_Enter;		return True;
	case XK_Alt_L: case XK_Alt_R:	
		buf[0]= XKC_ISO_Alt;		return True;
	case XK_Control_L: case XK_Control_R:	
		buf[0]= XKC_ISO_Control;	return True;
	case XK_Pause:
		buf[0]= XKC_ISO_Pause;		return True;
	case XK_Break:
		buf[0]= XKC_ISO_Break;		return True;
	case XK_Escape:
		buf[0]= XKC_ISO_Escape;		return True;
	case XK_Undo:
		buf[0]= XKC_ISO_Undo;		return True;
	case XK_ISO_Fast_Cursor_Up:
		buf[0]= XKC_ISO_Fast_Cursor_Up;	return True;
	case XK_ISO_Fast_Cursor_Down:
		buf[0]= XKC_ISO_Fast_Cursor_Down;return True;
	case XK_ISO_Fast_Cursor_Left:
		buf[0]= XKC_ISO_Fast_Cursor_Left;return True;
	case XK_ISO_Fast_Cursor_Right:
		buf[0]= XKC_ISO_Fast_Cursor_Right;return True;
	case XK_Home:
		buf[0]= XKC_ISO_Home;		return True;
	case XK_End:
		buf[0]= XKC_ISO_End;		return True;
	case XK_Page_Up:
		buf[0]= XKC_ISO_Page_Up;	return True;
	case XK_Page_Down:
		buf[0]= XKC_ISO_Page_Down;	return True;
	case XK_ISO_Move_Line_Up:
		buf[0]= XKC_ISO_Move_Line_Up;	return True;
	case XK_ISO_Move_Line_Down:
		buf[0]= XKC_ISO_Move_Line_Down;	return True;
	case XK_ISO_Partial_Line_Up:
		buf[0]= XKC_ISO_Partial_Line_Up;return True;
	case XK_ISO_Partial_Line_Down:
		buf[0]= XKC_ISO_Partial_Line_Down;return True;
	case XK_ISO_Partial_Space_Left:
		buf[0]= XKC_ISO_Partial_Space_Left;return True;
	case XK_ISO_Partial_Space_Right:
		buf[0]= XKC_ISO_Partial_Space_Right;return True;
	case XK_ISO_Set_Margin_Left:
		buf[0]= XKC_ISO_Set_Margin_Left;return True;
	case XK_ISO_Set_Margin_Right:
		buf[0]= XKC_ISO_Set_Margin_Right;return True;
	case XK_ISO_Release_Margin_Left:
		buf[0]= XKC_ISO_Release_Margin_Left;return True;
	case XK_ISO_Release_Margin_Right:
		buf[0]= XKC_ISO_Release_Margin_Right;return True;
	case XK_ISO_Release_Both_Margins:
		buf[0]= XKC_ISO_Release_Both_Margins;return True;
	case XK_ISO_Prev_Group:
		buf[0]= XKC_ISO_Prev_Group;	return True;
	case XK_ISO_Prev_Group_Lock:
		buf[0]= XKC_ISO_Prev_Group_Lock;	return True;
    }
#ifdef NOTYET
    if (font_rtrn!=NULL)
	font_rtrn= FONT_MOUSECAPS;
    switch (sym) {
	case XK_Pointer_Up:
		buf[0]= XKC_Mouse_Pointer_Up;	return True;
	case XK_Pointer_Down:
		buf[0]= XKC_Mouse_Pointer_Down;	return True;
	case XK_Pointer_Left:
		buf[0]= XKC_Mouse_Pointer_Left;	return True;
	case XK_Pointer_Right:
		buf[0]= XKC_Mouse_Pointer_Right;return True;
	case XK_Pointer_Button1:
		buf[0]= XKC_Mouse_Pointer_Button1;return True;
	case XK_Pointer_Button2:
		buf[0]= XKC_Mouse_Pointer_Button2;return True;
	case XK_Pointer_Button3:
		buf[0]= XKC_Mouse_Pointer_Button3;return True;
	case XK_Pointer_Button4:
		buf[0]= XKC_Mouse_Pointer_Button4;return True;
    }
#endif
    return False;
}

static Bool
PSNonLatin1Symbol(KeySym sym, unsigned char *buf,
		  int *font_rtrn, int *sz_rtrn, PSState *state)
{
    if (state->args->wantSymbols==NO_SYMBOLS)
	return False;

    if (font_rtrn!=NULL)
	*font_rtrn= FONT_TEXT;
    if (sz_rtrn!=NULL)
	*sz_rtrn= SZ_LARGE;
    buf[1]= '\0';
    switch (sym) {
	case XK_breve:		buf[0]= 0xC6;	return True;
	case XK_abovedot:	buf[0]= 0xC7;	return True;
	case XK_doubleacute:	buf[0]= 0xCD;	return True;
	case XK_ogonek:		buf[0]= 0xCE;	return True;
	case XK_caron:		buf[0]= 0xCF;	return True;
	case XK_Lstroke:	buf[0]= 0xE8;	return True;
	case XK_idotless:	buf[0]= 0xF5;	return True;
	case XK_lstroke:	buf[0]= 0xF8;	return True;
    }
    if (font_rtrn!=NULL)
	*font_rtrn= FONT_SYMBOL;
    if (sz_rtrn!=NULL)
	*sz_rtrn= SZ_MEDIUM;
    if ((sym&(~0xff))==0x700) {
	switch (sym) {
	    /* Greek symbol */
	    case XK_Greek_ALPHA:	buf[0]= 0x41;	return True;
	    case XK_Greek_BETA:		buf[0]= 0x42;	return True;
	    case XK_Greek_CHI:		buf[0]= 0x43;	return True;
	    case XK_Greek_DELTA:	buf[0]= 0x44;	return True;
	    case XK_Greek_EPSILON:	buf[0]= 0x45;	return True;
	    case XK_Greek_PHI:		buf[0]= 0x46;	return True;
	    case XK_Greek_GAMMA:	buf[0]= 0x47;	return True;
	    case XK_Greek_ETA:		buf[0]= 0x48;	return True;
	    case XK_Greek_IOTA:		buf[0]= 0x49;	return True;
	    case XK_Greek_KAPPA:	buf[0]= 0x4B;	return True;
	    case XK_Greek_LAMDA:	buf[0]= 0x4C;	return True;
	    case XK_Greek_MU:		buf[0]= 0x4D;	return True;
	    case XK_Greek_NU:		buf[0]= 0x4E;	return True;
	    case XK_Greek_OMICRON:	buf[0]= 0x4F;	return True;
	    case XK_Greek_PI:		buf[0]= 0x50;	return True;
	    case XK_Greek_THETA:	buf[0]= 0x51;	return True;
	    case XK_Greek_RHO:		buf[0]= 0x52;	return True;
	    case XK_Greek_SIGMA:	buf[0]= 0x53;	return True;
	    case XK_Greek_TAU:		buf[0]= 0x54;	return True;
	    case XK_Greek_UPSILON:	buf[0]= 0x55;	return True;
	    case XK_Greek_OMEGA:	buf[0]= 0x57;	return True;
	    case XK_Greek_XI:		buf[0]= 0x58;	return True;
	    case XK_Greek_PSI:		buf[0]= 0x59;	return True;
	    case XK_Greek_ZETA:		buf[0]= 0x5A;	return True;

	    case XK_Greek_alpha:	buf[0]= 0x61;	return True;
	    case XK_Greek_beta:		buf[0]= 0x62;	return True;
	    case XK_Greek_chi:		buf[0]= 0x63;	return True;
	    case XK_Greek_delta:	buf[0]= 0x64;	return True;
	    case XK_Greek_epsilon:	buf[0]= 0x65;	return True;
	    case XK_Greek_phi:		buf[0]= 0x66;	return True;
	    case XK_Greek_gamma:	buf[0]= 0x67;	return True;
	    case XK_Greek_eta:		buf[0]= 0x68;	return True;
	    case XK_Greek_iota:		buf[0]= 0x69;	return True;
	    case XK_Greek_kappa:	buf[0]= 0x6B;	return True;
	    case XK_Greek_lamda:	buf[0]= 0x6C;	return True;
	    case XK_Greek_mu:		buf[0]= 0x6D;	return True;
	    case XK_Greek_nu:		buf[0]= 0x6E;	return True;
	    case XK_Greek_omicron:	buf[0]= 0x6F;	return True;
	    case XK_Greek_pi:		buf[0]= 0x70;	return True;
	    case XK_Greek_theta:	buf[0]= 0x71;	return True;
	    case XK_Greek_rho:		buf[0]= 0x72;	return True;
	    case XK_Greek_sigma:	buf[0]= 0x73;	return True;
	    case XK_Greek_tau:		buf[0]= 0x74;	return True;
	    case XK_Greek_upsilon:	buf[0]= 0x75;	return True;
	    case XK_Greek_omega:	buf[0]= 0x77;	return True;
	    case XK_Greek_xi:		buf[0]= 0x78;	return True;
	    case XK_Greek_psi:		buf[0]= 0x79;	return True;
	    case XK_Greek_zeta:		buf[0]= 0x7A;	return True;
	}
    }
    switch (sym) {
	case XK_leftarrow:	buf[0]= 0xAC;	return True;
	case XK_uparrow:	buf[0]= 0xAD;	return True;
	case XK_rightarrow:	buf[0]= 0xAE;	return True;
	case XK_downarrow:	buf[0]= 0xAF;	return True;
	case XK_horizconnector:	buf[0]= 0xBE;	return True;
	case XK_trademark:	buf[0]= 0xE4;	return True;
    }
    return False;
}

static KeySym
CheckSymbolAlias(KeySym sym, PSState *state)
{
    if (XkbKSIsKeypad(sym)) {
	if ((sym>=XK_KP_0)&&(sym<=XK_KP_9))
	     sym= (sym-XK_KP_0)+XK_0;
	else switch (sym) {
		case XK_KP_Space:	return XK_space;
		case XK_KP_Tab:		return XK_Tab;
		case XK_KP_Enter:	return XK_Return;
		case XK_KP_F1:		return XK_F1;
		case XK_KP_F2:		return XK_F2;
		case XK_KP_F3:		return XK_F3;
		case XK_KP_F4:		return XK_F4;
		case XK_KP_Home:	return XK_Home;
		case XK_KP_Left:	return XK_Left;
		case XK_KP_Up:		return XK_Up;
		case XK_KP_Right:	return XK_Right;
		case XK_KP_Down:	return XK_Down;
		case XK_KP_Page_Up:	return XK_Page_Up;
		case XK_KP_Page_Down:	return XK_Page_Down;
		case XK_KP_End:		return XK_End;
		case XK_KP_Begin:	return XK_Begin;
		case XK_KP_Insert:	return XK_Insert;
		case XK_KP_Delete:	return XK_Delete;
		case XK_KP_Equal:	return XK_equal;
		case XK_KP_Multiply:	return XK_asterisk;
		case XK_KP_Add:		return XK_plus;
		case XK_KP_Subtract:	return XK_minus;
		case XK_KP_Divide:	return XK_slash;
	}
    }
    else if (XkbKSIsDeadKey(sym)) {
	switch (sym) {
		     case XK_dead_grave:	sym= XK_grave; break;
		     case XK_dead_acute:	sym= XK_acute; break;
		     case XK_dead_circumflex:	sym= XK_asciicircum; break;
		     case XK_dead_tilde:	sym= XK_asciitilde; break;
		     case XK_dead_macron:	sym= XK_macron; break;
		     case XK_dead_breve:	sym= XK_breve; break;
		     case XK_dead_abovedot:	sym= XK_abovedot; break;
		     case XK_dead_diaeresis:	sym= XK_diaeresis; break;
		     case XK_dead_abovering:	sym= XK_degree; break;
		     case XK_dead_doubleacute:	sym= XK_doubleacute; break;
		     case XK_dead_caron:	sym= XK_caron; break;
		     case XK_dead_cedilla:	sym= XK_cedilla; break;
		     case XK_dead_ogonek:	sym= XK_ogonek; break;
		     case XK_dead_iota:		sym= XK_Greek_iota; break;
		     case XK_dead_voiced_sound:	
				sym= XK_voicedsound; 
				break;
		     case XK_dead_semivoiced_sound:	
				sym= XK_semivoicedsound;
				break;
	}
    }
    return sym;
}

static Bool
FindKeysymsByName(XkbDescPtr xkb, char *name, PSState *state, KeyTop *top)
{
static unsigned char buf[30];
int	kc;
KeySym	sym,*syms,topSyms[NLABELS];
int	level,group;
int	eG,nG,gI,l,g;

    bzero(top,sizeof(KeyTop));
    kc= XkbFindKeycodeByName(xkb,name,True);
    if (state->args!=NULL) {
	level= state->args->labelLevel;
	group= state->args->baseLabelGroup;
    }
    else level= group= 0;
    syms= XkbKeySymsPtr(xkb,kc);
    eG= group;
    nG= XkbKeyNumGroups(xkb,kc);
    gI= XkbKeyGroupInfo(xkb,kc);
    if ((state->args->wantDiffs)&&(eG>=XkbKeyNumGroups(xkb,kc)))
	return False;		/* XXX was a return with no value */
    if (nG==0) {
	return False;
    }
    else if (nG==1) {
	eG= 0;
    }
    else if (eG>=XkbKeyNumGroups(xkb,kc)) {
	switch (XkbOutOfRangeGroupAction(gI)) {
	    default:
		eG%= nG;
		break;
	    case XkbClampIntoRange:
		eG= nG-1;
	        break;
	    case XkbRedirectIntoRange:
		eG= XkbOutOfRangeGroupNumber(gI);
		if (eG>=nG)
		    eG= 0;
		break;
	}
    }
    for (g=0;g<state->args->nLabelGroups;g++) {
	if ((eG+g)>=nG)
	    continue;
	for (l=0;l<2;l++) {
	    int font,sz;
	    if (level+l>=XkbKeyGroupWidth(xkb,kc,(eG+g)))
		continue;
	    sym= syms[((eG+g)*XkbKeyGroupsWidth(xkb,kc))+(level+l)];

	    if (state->args->wantSymbols!=NO_SYMBOLS)
		sym= CheckSymbolAlias(sym,state);
	    topSyms[(g*2)+l]= sym;

	    if (PSKeycapsSymbol(sym,buf,&font,&sz,state)) {
		top->font[(g*2)+l]= font;
		top->size[(g*2)+l]= sz;
	    }
	    else if (((sym&(~0xff))==0)&&isprint(sym)&&(!isspace(sym))) {
		if (sym=='(')		sprintf((char *)buf,"\\(");
		else if (sym==')')	sprintf((char *)buf,"\\)");
		else if (sym=='\\')	sprintf((char *)buf,"\\\\");
		else			sprintf((char *)buf,"%c",(char)sym);
		top->font[(g*2)+l]= FONT_LATIN1;
		top->size[(g*2)+l]= SZ_MEDIUM;
		switch (buf[0]) {
		    case '.': case ':': case ',': case ';':
		    case '\'': case '"': case '`': case '~':
 		    case '^': case '\250': case '\270': case '\267':
 		    case '\260': case '\252': case '\272': case '\271':
 		    case '\262': case '\263': case '\264': case '\255':
 		    case '\254': case '\257':
			top->size[(g*2)+l]= SZ_LARGE;
			break;
		}
	    }
	    else if (PSNonLatin1Symbol(sym,buf,&font,&sz,state)) {
		top->font[(g*2)+l]= font;
		top->size[(g*2)+l]= sz;
	    }
	    else {
		char 		*tmp;
		tmp= XKeysymToString(sym);
		if (tmp!=NULL)	strcpy((char *)buf,tmp);
		else		sprintf((char *)buf,"(%ld)",sym);
		top->font[(g*2)+l]= FONT_LATIN1;
		if (strlen((char *)buf)<9)
		     top->size[(g*2)+l]= SZ_SMALL;
		else top->size[(g*2)+l]= SZ_TINY;
	    }
	    top->present|= (1<<((g*2)+l));
	    strncpy(top->label[(g*2)+l],(char *)buf,LABEL_LEN-1);
	    top->label[(g*2)+l][LABEL_LEN-1]= '\0';
	}
	if (((g==0)&&(top->present&G1LX_MASK)==G1LX_MASK)||
	    ((g==1)&&(top->present&G2LX_MASK)==G2LX_MASK)) {
	    KeySym	lower,upper;
	    XConvertCase(topSyms[(g*2)],&lower,&upper);
	    if ((topSyms[(g*2)]==lower)&&(topSyms[(g*2)+1]==upper)) {
		top->alpha[g]= True;
	    }
	}
    }
    return True;
}

static void
PSDrawLabel(FILE *out, char *label, int x, int y, int w, int h)
{
    fprintf(out,"%d %d (%s) centeroffset\n",w,h,label);
    fprintf(out,"%d add exch\n",y); 
    fprintf(out,"%d add exch moveto\n",x);
    fprintf(out,"1 -1 scale (%s) show 1 -1 scale\n",label);
    return;
}

#define	TOP_ROW		0
#define	BOTTOM_ROW	1
#define	CENTER_ROW	2

#define	LEFT_COL	0
#define	RIGHT_COL	1
#define	CENTER_COL	2

static void
PSLabelKey(FILE *out, PSState *state, KeyTop *top, int x, int y,
	   XkbBoundsPtr bounds, int kc, int btm)
{
char	keycode[10];
int	w,h,i;
int	row_y[3];
int	col_x[3];
int	row_h[3];
int	col_w[3];
Bool	present[NLABELS];
int	sym_row[NLABELS];
int	sym_col[NLABELS];

    w= XkbBoundsWidth(bounds);
    h= XkbBoundsHeight(bounds);
    row_y[TOP_ROW]=	y+bounds->y1+(h/10);
    row_y[BOTTOM_ROW]=	y+bounds->y1+(h/2)+(h/10);
    row_y[CENTER_ROW]=	y+bounds->y1+(h/10);
    row_h[TOP_ROW]=	h/2;
    row_h[BOTTOM_ROW]=	h/2;
    row_h[CENTER_ROW]=	h;

    col_x[LEFT_COL]= 	x+bounds->x1;
    col_x[RIGHT_COL]= 	x+bounds->x1+w/2;
    col_x[CENTER_COL]= 	x+bounds->x1;
    col_w[LEFT_COL]= 	w/2;
    col_w[RIGHT_COL]= 	w/2;
    col_w[CENTER_COL]= 	w;

    present[G1L1]= False;
    sym_row[G1L1]= BOTTOM_ROW;
    sym_col[G1L1]= LEFT_COL;

    present[G1L2]= False;
    sym_row[G1L2]= TOP_ROW;
    sym_col[G1L2]= LEFT_COL;

    present[G2L1]= False;
    sym_row[G2L1]= BOTTOM_ROW;
    sym_col[G2L1]= RIGHT_COL;

    present[G2L2]= False;
    sym_row[G2L2]= TOP_ROW;
    sym_col[G2L2]= RIGHT_COL;

    present[CENTER]= False;
    sym_row[CENTER]= CENTER_ROW;
    sym_col[CENTER]= CENTER_COL;

    if (top->present&CENTER_MASK) {
	present[CENTER]= True;
    }
    else switch (top->present&GXLX_MASK) {
	case G1L1_MASK:
	    present[G1L1]= True;
	    sym_row[G1L1]= CENTER_ROW;
	    sym_col[G1L1]= CENTER_COL;
	    break;
	case G1LX_MASK:
	    present[G1L2]= True;
	    if (!top->alpha[0]) {
		present[G1L1]= True;
		if ((strlen(top->label[G1L1])>1)&&(top->label[G1L1][0]!='\\'))
		    sym_col[G1L1]= CENTER_COL;
		if ((strlen(top->label[G1L2])>1)&&(top->label[G1L1][0]!='\\'))
		    sym_col[G1L2]= CENTER_COL;
	    }
	    break;
	default:
	    if ((top->present&G1LX_MASK)==G1LX_MASK) {
		present[G1L2]= True;
		if (!top->alpha[0])
		    present[G1L1]= True;
	    }
	    else if ((top->present&G1LX_MASK)==G1L1_MASK) {
		present[G1L1]= True;
	    }
	    else if ((top->present&G1LX_MASK)==G1L2_MASK) {
		present[G1L2]= True;
	    }
	    if ((top->present&G2LX_MASK)==G2LX_MASK) {
		present[G2L2]= True;
		if (!top->alpha[1])
		    present[G2L1]= True;
	    }
	    else if ((top->present&G2LX_MASK)==G2L1_MASK) {
		present[G2L1]= True;
	    }
	    else if ((top->present&G2LX_MASK)==G2L2_MASK) {
		present[G2L2]= True;
	    }
	    break;
	case 0:
	    return;
    }
    for (i=0;i<NLABELS;i++) {
	if (present[i]) {
	    int len,size;
	    if (top->size[i]==SZ_AUTO) {
		len= strlen(top->label[i]);
		if (len==1) {
		    if (top->font[i]==FONT_ISOCAPS)
			 size= 18;
		    else size= 14;
		}
		else if (len<10)	size= 12;
		else		size= 10;
	    }
	    else if (top->size[i]==SZ_TINY)
		 size= 10;
	    else if (top->size[i]==SZ_SMALL)
		 size= 12;
	    else if (top->size[i]==SZ_LARGE)
		 size= 18;
	    else if (top->size[i]==SZ_XLARGE)
		 size= 24;
	    else size= 14;
	    PSSetFont(out,state,top->font[i],size,True);
	    PSDrawLabel(out,top->label[i],col_x[sym_col[i]],row_y[sym_row[i]],
					  col_w[sym_col[i]],row_h[sym_row[i]]);
	}
    }
    if (state->args->wantKeycodes) {
	sprintf(keycode,"%d",kc);
	PSSetFont(out,state,FONT_LATIN1,8,True);
	PSDrawLabel(out,keycode,x+bounds->x1,y+btm-5,w,0);
    }
    return;
}

static void
PSSection(FILE *out, PSState *state, XkbSectionPtr section)
{
int		r,offset;
XkbRowPtr	row;
Display *	dpy;
XkbDescPtr	xkb;

    xkb= state->xkb;
    dpy= xkb->dpy;
    fprintf(out,"%% Begin Section '%s'\n",(section->name!=None?
					XkbAtomGetString(dpy,section->name):
					"NoName"));
    PSGSave(out,state);
    fprintf(out,"%d %d translate\n",section->left,section->top);
    if (section->angle!=0)
	fprintf(out,"%s rotate\n",XkbGeomFPText(section->angle,XkbMessage));
    if (section->doodads) {
	XkbDrawablePtr	first,draw;
	first= draw= XkbGetOrderedDrawables(NULL,section);
	while (draw) {
	    if (draw->type==XkbDW_Section)
		 PSSection(out,state,draw->u.section);
	    else PSDoodad(out,state,draw->u.doodad);
	    draw= draw->next;
	}
	XkbFreeOrderedDrawables(first);
    }
    for (r=0,row=section->rows;r<section->num_rows;r++,row++) {
	int 		k;
	XkbKeyPtr	key;
	XkbShapePtr	shape;
	if (row->vertical)	offset= row->top;
	else			offset= row->left;
	fprintf(out,"%% Begin %s %d\n",row->vertical?"column":"row",r+1);
	for (k=0,key=row->keys;k<row->num_keys;k++,key++) {
	    shape= XkbKeyShape(xkb->geom,key);
	    offset+= key->gap;
	    if (row->vertical) {
		if (state->args->wantColor) {
		    if (key->color_ndx!=state->white) {
			PSSetColor(out,state,key->color_ndx);
			fprintf(out,"true 0 %d %d %s %% %s\n",
				row->left,offset,
	     			XkbAtomGetString(dpy,shape->name),
				XkbKeyNameText(key->name.name,XkbMessage));
		    }
		    PSSetColor(out,state,state->black);
		}
		fprintf(out,"false 0 %d %d %s %% %s\n",row->left,offset,
	     			XkbAtomGetString(dpy,shape->name),
				XkbKeyNameText(key->name.name,XkbMessage));
		offset+= shape->bounds.y2;
	    }
	    else {
		if (state->args->wantColor) {
		    if (key->color_ndx!=state->white) {
			PSSetColor(out,state,key->color_ndx);
			fprintf(out,"true 0 %d %d %s %% %s\n",offset,row->top,
	     			XkbAtomGetString(dpy,shape->name),
				XkbKeyNameText(key->name.name,XkbMessage));
		    }
		    PSSetColor(out,state,state->black);
		}
		fprintf(out,"false 0 %d %d %s %% %s\n",offset,row->top,
	     			XkbAtomGetString(dpy,shape->name),
				XkbKeyNameText(key->name.name,XkbMessage));
		offset+= shape->bounds.x2;
	    }
	}
    }
    for (r=0,row=section->rows;r<section->num_rows;r++,row++) {
	int 		k,kc=0;
	XkbKeyPtr	key;
	XkbShapePtr	shape;
	XkbBoundsRec	bounds;
	if (state->args->label==LABEL_NONE)
	    break;
	if (row->vertical)	offset= row->top;
	else			offset= row->left;
	fprintf(out,"%% Begin %s %d labels\n",row->vertical?"column":"row",r+1);
	PSSetColor(out,state,xkb->geom->label_color->pixel);
	PSSetFont(out,state,FONT_LATIN1,12,True);
	for (k=0,key=row->keys;k<row->num_keys;k++,key++) {
	    char *	name,*name2,buf[30],buf2[30];
	    int		x,y;
	    KeyTop	top;

	    shape= XkbKeyShape(xkb->geom,key);
	    XkbComputeShapeTop(shape,&bounds);
	    offset+= key->gap;
	    name= name2= NULL;
	    if (state->args->label==LABEL_SYMBOLS) {
		if (!FindKeysymsByName(xkb,key->name.name,state,&top)) {
		    fprintf(out,"%% No label for %s\n",
				XkbKeyNameText(key->name.name,XkbMessage));
		}
	    }
	    else {
		char *olKey;

		if (section->num_overlays>0)
		     olKey= XkbFindOverlayForKey(xkb->geom,section,
							   key->name.name);
		else olKey= NULL;

		if (state->args->label==LABEL_KEYNAME) {
		    name= XkbKeyNameText(key->name.name,XkbMessage);
		    if (olKey)
			name2= XkbKeyNameText(olKey,XkbMessage);
		}
		else if (state->args->label==LABEL_KEYCODE) {
		    name= buf;
		    sprintf(name,"%d",XkbFindKeycodeByName(xkb,key->name.name,
									True));
		    if (olKey) {
			name2= buf2;
			sprintf(name,"%d",XkbFindKeycodeByName(xkb,olKey,True));
		    }
		}
		bzero(&top,sizeof(KeyTop));
		if (name2!=NULL) {
		    top.present|= G1LX_MASK;
		    strncpy(top.label[G1L1],name,LABEL_LEN-1);
		    top.label[G1L1][LABEL_LEN-1]= '\0';
		    strncpy(top.label[G1L2],name2,LABEL_LEN-1);
		    top.label[G1L2][LABEL_LEN-1]= '\0';
		}
		else if (name!=NULL) {
		    top.present|= CENTER_MASK;
		    strncpy(top.label[CENTER],name,LABEL_LEN-1);
		    top.label[CENTER][LABEL_LEN-1]= '\0';
		}
		else {
		    fprintf(out,"%% No label for %s\n",
				XkbKeyNameText(key->name.name,XkbMessage));
		}
	    }
	    if (row->vertical) {
		x= row->left;
		y= offset;
		offset+= shape->bounds.y2;
	    }
	    else {
		x= offset;
		y= row->top;
		offset+= shape->bounds.x2;
	    }
	    name= key->name.name;
	    fprintf(out,"%% %s\n",XkbKeyNameText(name,XkbMessage));
	    if (state->args->wantKeycodes)
		kc= XkbFindKeycodeByName(xkb,key->name.name,True);
	    PSLabelKey(out,state,&top,x,y,&bounds,kc,shape->bounds.y2);
	}
    }
    PSGRestore(out,state);
    return;
}

Bool
GeometryToPostScript(FILE *out, XkbFileInfo *pResult, XKBPrintArgs *args)
{
XkbDrawablePtr	first,draw;
PSState		state;
Bool		dfltBorder;
int		i;

    if ((!pResult)||(!pResult->xkb)||(!pResult->xkb->geom))
	return False;
    state.xkb=   pResult->xkb;
    state.dpy=   pResult->xkb->dpy;
    state.geom=  pResult->xkb->geom;
    state.color= state.black= state.white= -1;
    state.font=  -1;
    state.nPages= 0;
    state.totalKB= 1;
    state.kbPerPage= 1;
    state.x1= state.y1= state.x2= state.y2= 0;
    state.args= args;

    if ((args->label==LABEL_SYMBOLS)&&(pResult->xkb->ctrls)) {
	if (args->nTotalGroups==0) 
	     state.totalKB= pResult->xkb->ctrls->num_groups/args->nLabelGroups;
	else state.totalKB= args->nTotalGroups;
	if (state.totalKB<1)
	    state.totalKB= 1;
	else if (state.totalKB>1)
	    state.kbPerPage= 2;
    }
    if (args->nKBPerPage!=0)
	state.kbPerPage= args->nKBPerPage;

    PSProlog(out,&state);
    first= XkbGetOrderedDrawables(state.geom,NULL);

    for (draw=first,dfltBorder=True;draw!=NULL;draw=draw->next) {
	if ((draw->type!=XkbDW_Section)&&
	    ((draw->u.doodad->any.type==XkbOutlineDoodad)||
	     (draw->u.doodad->any.type==XkbSolidDoodad))) {
	    char *name;
	    name= XkbAtomGetString(state.dpy,draw->u.doodad->any.name);
	    if ((name!=NULL)&&(uStrCaseEqual(name,"edges"))) {
		dfltBorder= False;
		break;
	    }
	}
    }
    for (i=0;i<state.totalKB;i++) {
	PSPageSetup(out,&state,dfltBorder);
	for (draw=first;draw!=NULL;draw=draw->next) {
	    if (draw->type==XkbDW_Section)
		PSSection(out,&state,draw->u.section);
	    else {
		PSDoodad(out,&state,draw->u.doodad);
	    }
	}
	PSPageTrailer(out,&state);
	state.args->baseLabelGroup+= state.args->nLabelGroups;
    }
    XkbFreeOrderedDrawables(first);
    PSFileTrailer(out,&state);
    return True;
}
