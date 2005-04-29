/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#include                "render.h"
#include                "gd.h"
#include                "utils.h"
#include                <fcntl.h>
#ifdef MSWIN32
#include <io.h>
#endif

#ifdef _UWIN
#ifndef DEFAULT_FONTPATH
#define		DEFAULT_FONTPATH	"/win/fonts"
#endif  /* DEFAULT_FONTPATH */
#else
#ifndef MSWIN32
#ifndef DEFAULT_FONTPATH
#define		DEFAULT_FONTPATH	"/usr/share/ttf:/usr/local/share/ttf:/usr/share/fonts/ttf:/usr/local/share/fonts/ttf:/usr/lib/fonts:/usr/local/lib/fonts:/usr/lib/fonts/ttf:/usr/local/lib/fonts/ttf:/usr/common/graphviz/lib/fonts/ttf:/windows/fonts:/dos/windows/fonts:/usr/add-on/share/ttf:."
#endif /* DEFAULT_FONTPATH */
#else
#ifndef DEFAULT_FONTPATH
#define		DEFAULT_FONTPATH	"%WINDIR%/FONTS;C:/WINDOWS/FONTS;C:/WINNT/Fonts;C:/winnt/fonts"
#endif /* DEFAULT_FONTPATH */
#endif /* MSWIN32 */
#endif /* _UWIN */

#define SCALE ((double)GD_RESOLUTION/(double)POINTS_PER_INCH)

#define BEZIERSUBDIVISION 10

/* fontsize at which text is omitted entirely */
#define FONTSIZE_MUCH_TOO_SMALL 0.15
/* fontsize at which text is rendered by a simple line */
#define FONTSIZE_TOO_SMALL 1.5

/* font modifiers */
#define REGULAR 0
#define BOLD		1
#define ITALIC		2

/* patterns */
#define P_SOLID		0
#define P_DOTTED	4
#define P_DASHED	11
#define P_NONE		15

/* bold line constant */
#define WIDTH_NORMAL	1
#define WIDTH_BOLD	3

/* static int	N_pages; */
/* static point	Pages; */
static int	Rot;

static point	Viewport;
static pointf	GraphFocus;
static double   Zoom;

static gdImagePtr im;
static Dict_t	*ImageDict;

typedef struct context_t {
	int	pencolor, fillcolor;
        char	*fontfam, fontopt, font_was_set, pen, fill, penwidth;
	double   fontsz;
} context_t;

#define MAXNEST	4
static context_t cstk[MAXNEST];
static int	SP;
static node_t	*Curnode;

int black, white, transparent;

static void init_gd()
{
	SP = 0;
	/* must create default background color first... */
	/* we have to force the background to be filled for some reason */
	white = gdImageColorResolve(im, gdRedMax,gdGreenMax,gdBlueMax);
	gdImageFilledRectangle(im, 0, 0, im->sx-1, im->sy-1, white);

	black = gdImageColorResolve(im, 0, 0, 0);

	/* in truecolor images we don't need to allocate a color
		for transparent */
	if (! im->trueColor) {
		/* transparent uses an rgb value very close to white
	   	so that formats like GIF that don't support
	   	transparency show a white background */
		transparent = gdImageColorResolveAlpha(im, gdRedMax,gdGreenMax,gdBlueMax-1, gdAlphaTransparent);
		gdImageColorTransparent(im, transparent);
	}

	cstk[0].pencolor = black;		/* set pen black*/
	cstk[0].fillcolor = black;		/* set fill black*/
	cstk[0].fontfam = "times";		/* font family name */
	cstk[0].fontopt = REGULAR;		/* modifier: REGULAR, BOLD or ITALIC */
	cstk[0].pen = P_SOLID;		/* pen pattern style, default is solid */
	cstk[0].fill = P_NONE;
	cstk[0].penwidth = WIDTH_NORMAL;
}

static pointf gdpt(pointf p)
{
	pointf		  rv;

	if (Rot == 0) {
		rv.x = (p.x - GraphFocus.x) * Zoom + Viewport.x/2.;
		rv.y = (p.y - GraphFocus.y) * -Zoom + Viewport.y/2.;
	} else {
		rv.x = (p.y - GraphFocus.y) * Zoom + Viewport.x/2.;
		rv.y = (p.x - GraphFocus.x) * -Zoom + Viewport.y/2.; 
	}
	return rv;
}

void gd_font(context_t* cp)
{
/*
	char		   *fw, *fa;

	fw = fa = "Regular";
	switch (cp->fontopt) {
	case BOLD:
		fw = "Bold";
		break;
	case ITALIC:
		fa = "Italic";
		break;
	}
*/
}

static void gd_begin_job(FILE *ofp, graph_t *g, char **lib, char *user,
char *info[], point pages)
{
/*	Pages = pages; */
/*	N_pages = pages.x * pages.y; */
#ifdef MYTRACE
fprintf(stderr,"gd_begin_job\n");
#endif
}

static void gd_end_job(void)
{
#ifdef MYTRACE
fprintf(stderr,"gd_end_job\n");
#endif
}

static void gd_begin_graph(graph_t* g, box bb, point pb)
{
	Viewport = GD_drawing(g)->viewport.size;
	if (Viewport.x) {
		GraphFocus = GD_drawing(g)->viewport.focus;
		Zoom = GD_drawing(g)->viewport.zoom;
	}
	else {
		Viewport.x = (bb.UR.x - bb.LL.x + 2*GD_drawing(g)->margin.x) * SCALE + 1;
		Viewport.y = (bb.UR.y - bb.LL.y + 2*GD_drawing(g)->margin.y) * SCALE + 1;
		GraphFocus.x = (GD_bb(g).UR.x - GD_bb(g).LL.x) / 2.;
		GraphFocus.y = (GD_bb(g).UR.y - GD_bb(g).LL.y) / 2.;
		Zoom = SCALE;
	}
}

static void gd_begin_graph_to_file(graph_t* g, box bb, point pb)
{
	char *truecolor_p;

	gd_begin_graph(g, bb, pb);
	if (Verbose)
		fprintf(stderr,"%s: allocating a %dK GD image\n",
			CmdName, ROUND( Viewport.x * Viewport.y / 1024. ));

	truecolor_p = agget(g,"truecolor");
	/* automatically decide color mode if "truecolor" not specified */
	if (truecolor_p == NULL || *truecolor_p == '\0') {
		truecolor_p="false";  /* indexed color maps unless ... */

		if (agfindattr(g->proto->n, "shapefile")) {
			/* ...  custom shapefiles are used */
			truecolor_p = "true";
		}
	}
	if (mapbool(truecolor_p))
		im = gdImageCreateTrueColor(Viewport.x, Viewport.y);
	else
		im = gdImageCreate(Viewport.x, Viewport.y);
	init_gd();
#ifdef MYTRACE
fprintf(stderr,"gd_begin_graph_to_file\n");
#endif
}

static void gd_begin_graph_to_memory(graph_t* g, box bb, point pb)
{
	gd_begin_graph(g, bb, pb);
	if (Verbose)
		fprintf(stderr,"%s: using existing GD image\n", CmdName);
	im = *(gdImagePtr *)Output_file;
	init_gd();
#ifdef MYTRACE
fprintf(stderr,"gd_begin_graph_to_memory\n");
#endif
}

static void gd_end_graph_to_file(void)
{
/*
 * Windows will do \n -> \r\n  translations on stdout unless told otherwise.
 */
#ifdef HAVE_SETMODE
#ifdef O_BINARY
	setmode(fileno(Output_file), O_BINARY);
#endif
#endif

/*
 * Write IM to OUTFILE as a JFIF-formatted JPEG image, using quality
 * JPEG_QUALITY.  If JPEG_QUALITY is in the range 0-100, increasing values
 * represent higher quality but also larger image size.  If JPEG_QUALITY is
 * negative, the IJG JPEG library's default quality is used (which
 * should be near optimal for many applications).  See the IJG JPEG
 * library documentation for more details.  */

#define JPEG_QUALITY -1

	if (Output_lang == GD) {
		gdImageGd(im, Output_file);
#ifdef HAVE_LIBZ
	} else if (Output_lang == GD2) {
#define GD2_CHUNKSIZE 128
#define GD2_RAW 1
#define GD2_COMPRESSED 2
		gdImageGd2(im, Output_file, GD2_CHUNKSIZE, GD2_COMPRESSED);
#endif
#ifdef WITH_GIF
	} else if (Output_lang == GIF) {
		gdImageGif(im, Output_file);
#endif
#ifdef HAVE_LIBPNG
#ifdef HAVE_LIBZ
	} else if (Output_lang == PNG) {
		gdImagePng(im, Output_file);
#endif
#endif
#ifdef HAVE_LIBJPEG
	} else if (Output_lang == JPEG) {
		gdImageJpeg(im, Output_file, JPEG_QUALITY);
#endif
	} else if (Output_lang == WBMP) {
        	/* Use black for the foreground color for the B&W wbmp image. */
		gdImageWBMP(im, black, Output_file);
#ifdef HAVE_LIBXPM
	} else if (Output_lang == XBM) {
		gdImageXbm(im, Output_file);
#endif
	}
	if (ImageDict) {
		dtclose(ImageDict); ImageDict = 0;
	}
	gdImageDestroy(im);
#ifdef MYTRACE
fprintf(stderr,"gd_end_graph_to_file\n");
#endif
}

static void gd_end_graph_to_memory(void)
{
/* leave image in memory to be handled by Gdtclft output routines */
#ifdef MYTRACE
fprintf(stderr,"gd_end_graph_to_memory\n");
#endif
}

static void
gd_begin_page(graph_t *g, point page, double scale, int rot, point offset)
{
/*	int		page_number; */
/*	point		sz; */

	Zoom *= scale;
	Rot = rot;

/*	page_number = page.x + page.y * Pages.x + 1; */
#ifdef MYTRACE
fprintf(stderr,"gd_begin_page\n");
fprintf(stderr," page=%d,%d offset=%d,%d\n",page.x,page.y,offset.x,offset.y);
fprintf(stderr," page_number=%d\n",page_number);
#endif
}

void gd_end_page(void) {
#ifdef MYTRACE
fprintf(stderr,"gd_end_page\n");
#endif
}

static void
gd_begin_node(node_t* n)
{
        Curnode = n;
}

static void
gd_end_node()
{
        Curnode = NULL;
}

static  void
gd_begin_context(void)
{
	assert(SP + 1 < MAXNEST);
	cstk[SP + 1] = cstk[SP];
	SP++;
}

static  void
gd_end_context(void)
{
	int			 psp = SP - 1;
	assert(SP > 0);
	if (cstk[SP].font_was_set)
		gd_font(&(cstk[psp]));
	SP = psp;
}

static  void
gd_set_font(char* name, double size)
{
	char		   *p, *q;
	context_t	  *cp;

	cp = &(cstk[SP]);
	cp->font_was_set = TRUE;
	cp->fontsz = size * Zoom / SCALE;
	p = strdup(name);
	if ((q = strchr(p, '-'))) {
		*q++ = 0;
		if (strcasecmp(q, "italic") == 0)
			cp->fontopt = ITALIC;
		else if (strcasecmp(q, "bold") == 0)
			cp->fontopt = BOLD;
	}
	cp->fontfam = p;
	gd_font(&cstk[SP]);
}

static int gd_resolve_color(char* name)
{
	color_t	color;
 
	if (!(strcmp(name,"transparent"))) {
	    	/* special case for "transparent" color */
		return gdImageGetTransparent(im);
	} 
	else {
		colorxlate(name,&color,RGBA_BYTE);
		return gdImageColorResolve(im,color.u.rgba[0],color.u.rgba[1],color.u.rgba[2]);
	}
}

static void gd_set_pencolor(char* name)
{
	cstk[SP].pencolor = gd_resolve_color(name);
}

static void gd_set_fillcolor(char* name)
{
	cstk[SP].fillcolor = gd_resolve_color(name);
}

static  void
gd_set_style(char** s)
{
	char		*line,*p;
	context_t	*cp;

	cp = &(cstk[SP]);
	while ((p = line = *s++)) {
		if (streq(line, "solid")) cp->pen = P_SOLID;
		else if (streq(line, "dashed")) cp->pen = P_DASHED;
                else if (streq(line, "dotted")) cp->pen = P_DOTTED;
		else if (streq(line, "invis")) cp->pen = P_NONE;
 		else if (streq(line, "bold")) cp->penwidth = WIDTH_BOLD;
		else if (streq(line, "setlinewidth")) {
			while (*p) p++;
			p++;
			cp->penwidth = atol(p);
		}
		else if (streq(line, "filled")) cp->fill = P_SOLID;
		else if (streq(line, "unfilled")) cp->fill = P_NONE;
		else agerr (AGWARN, "gd_set_style: unsupported style %s - ignoring\n",
			line);
	}
}

/* sometimes fonts are stored under a different name */
char *
gd_alternate_fontlist(char *font) 
{
	char *fontlist;

	fontlist = font;
	if (strcasecmp(font,"Times-Roman")==0)
		fontlist = "Times-Roman;Times_New_Roman;Times-New-Roman;TimesNewRoman;Times;times";
	else if (strcasecmp(font,"Times-New-Roman")==0)
		fontlist = "Times-New-Roman;Times_New_Roman;TimesNewRoman;Times-Roman;Times;times";
	else if (strcasecmp(font,"Times_New_Roman")==0)
		fontlist = "Times_New_Roman;Times-New-Roman;TimesNewRoman;Times-Roman;Times;times";
	else if (strcasecmp(font,"TimesNewRoman")==0)
		fontlist = "TimesNewRoman;Times_New_Roman;Times-New-Roman;Times-Roman;Times;times";
	else if (strcasecmp(font,"Times")==0)
		fontlist = "Times;times;Times-Roman;Times_New_Roman;Times-New-Roman;TimesNewRoman";
	else if (strcasecmp(font,"Helvetica")==0)
		fontlist = "Helvetica;arial";
	else if (strcasecmp(font,"Arial")==0)
		fontlist = "Arial;arial";
	else if (strcasecmp(font,"arialb")==0)
		fontlist = "arialb;Arial-Bold";
	else if (strcasecmp(font, "ariali")==0)
		fontlist = "ariali;Arial-Italic";
	else if (strcasecmp(font,"Courier")==0)
		fontlist = "Courier;cour;Courier-New;Courier_New";
	else if (strcasecmp(font,"Courier-New")==0)
		fontlist = "Courier-New;Courier_New;Courier cour";
	else if (strcasecmp(font,"Courier_New")==0)
		fontlist = "Courier_New;Courier-New;Courier;cour";
	return fontlist;
}

void gd_missingfont(char *err, char *fontreq)
{
	static char     *lastmissing = 0;
	static int      n_errors = 0;
	char		*p;

	if (n_errors >= 20) return;
	if ((lastmissing == 0) || (strcmp(lastmissing,fontreq))) {
		if (!(p=getenv("GDFONTPATH"))) p = DEFAULT_FONTPATH;
		agerr(AGERR, "%s : %s in %s\n",err,fontreq,p);
		if (lastmissing) free(lastmissing);
		lastmissing = strdup(fontreq);
		n_errors++;
		if (n_errors >= 20) agerr(AGWARN, "(font errors suppressed)\n");
	}
}

extern gdFontPtr gdFontTiny, gdFontSmall, gdFontMediumBold, gdFontLarge, gdFontGiant;

static  void
gd_textline(point p, textline_t *line)
{
	char		*fontlist, *err;
	pointf		mp,ep;
	int		brect[8];
	char		*str = line->str;
	double		fontsz = cstk[SP].fontsz;

	if (cstk[SP].pen == P_NONE) return;
	fontlist = gd_alternate_fontlist(cstk[SP].fontfam); 

	switch(line->just) {
		case 'l':
			mp.x = p.x;
			break;
		case 'r':
			mp.x = p.x - line->width;
			break;
		default:
		case 'n':
			mp.x = p.x - line->width / 2;
		break;
	}
	ep.y = mp.y = p.y;
        ep.x = mp.x + line->width;

	mp = gdpt(mp);
	if (fontsz <= FONTSIZE_MUCH_TOO_SMALL) {
                /* ignore entirely */
	} else if (fontsz <= FONTSIZE_TOO_SMALL) {
                /* draw line in place of text */ 
		ep = gdpt(ep);
		gdImageLine(im, ROUND(mp.x), ROUND(mp.y),
                        ROUND(ep.x), ROUND(ep.y),
			cstk[SP].pencolor);
        } else {
		err = gdImageStringFT(im, brect, cstk[SP].pencolor,
			fontlist, fontsz, (Rot? 90.0 : 0.0) * PI / 180.0,
			ROUND(mp.x), ROUND(mp.y), str);
		if (err) {
			/* revert to builtin fonts */
	    		gd_missingfont (err, cstk[SP].fontfam);
			if (fontsz <= 8.5) {
				gdImageString(im, gdFontTiny,
					ROUND(mp.x), ROUND(mp.y-9.),
					(unsigned char *)str, cstk[SP].pencolor);
			} else if (fontsz <= 9.5) {
				gdImageString(im, gdFontSmall,
					ROUND(mp.x), ROUND(mp.y-12.),
					(unsigned char *)str, cstk[SP].pencolor);
			} else if (fontsz <= 10.5) {
				gdImageString(im, gdFontMediumBold,
					ROUND(mp.x), ROUND(mp.y-13.),
					(unsigned char *)str, cstk[SP].pencolor);
			} else if (fontsz <= 11.5) {
				gdImageString(im, gdFontLarge,
					ROUND(mp.x), ROUND(mp.y-14.),
					(unsigned char *)str, cstk[SP].pencolor);
			} else {
				gdImageString(im, gdFontGiant,
					ROUND(mp.x), ROUND(mp.y-15.),
					(unsigned char *)str, cstk[SP].pencolor);
			}
		}
	}
}

point gd_textsize(char *str, char *fontname, double fontsz)
{
	char		*fontlist,*err;
	point		rv;
	int		brect[8];

	fontlist = gd_alternate_fontlist(fontname);
	
	rv.x = rv.y = 0.0;
	if (fontlist && *str) {
		if (fontsz <= FONTSIZE_MUCH_TOO_SMALL) {
                	/* ignore entirely */
			rv.x = rv.y = 0;
			return rv;
		} else if (fontsz <= FONTSIZE_TOO_SMALL) {
                	/* draw line in place of text */ 
                        /* fake a finite fontsize so that line length is calculated */
			fontsz = FONTSIZE_TOO_SMALL;
		} 
		/* call gdImageStringFT with null *im to get brect */
		err = gdImageStringFT(NULL, brect, -1, fontlist, 
			fontsz, 0, 0, 0, str);
		if (!err) {
			rv.x = (brect[4] - brect[0]);
	/*		rv.y = (brect[5] - brect[1]); */
			rv.y = (brect[5] - 0       ); /* ignore descenders */
			rv.x /= SCALE; rv.y /= SCALE;
		}
	}
	return rv;
}

static  void
gd_bezier(point* A, int n, int arrow_at_start, int arrow_at_end)
{
	pointf		p0, p1, V[4];
	int		i, j, step;
	int		style[20]; 
	int		pen, width;
	gdImagePtr	brush = NULL;

	if (cstk[SP].pen != P_NONE) {
		if (cstk[SP].pen == P_DASHED) {
			for (i = 0; i < 10; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 20; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 20);
			pen = gdStyled;
		} else if (cstk[SP].pen == P_DOTTED) {
			for (i = 0; i < 2; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 12; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 12);
			pen = gdStyled;
		} else {
			pen = cstk[SP].pencolor;
		}
#if 0
                if (cstk[SP].penwidth != WIDTH_NORMAL) {
			width=cstk[SP].penwidth;
                        brush = gdImageCreate(width,width);
                        gdImagePaletteCopy(brush, im);
                        gdImageFilledRectangle(brush,
                           0,0,width-1, width-1, cstk[SP].pencolor);
                        gdImageSetBrush(im, brush);
			if (pen == gdStyled) pen = gdStyledBrushed;      
			else pen = gdBrushed;      
		}
#else
		width = cstk[SP].penwidth;
		gdImageSetThickness(im, width);
#endif
		V[3].x = A[0].x; V[3].y = A[0].y;
		for (i = 0; i+3 < n; i += 3) {
			V[0] = V[3];
			for (j = 1; j <= 3; j++) {
				V[j].x  = A[i+j].x; V[j].y = A[i+j].y;
			}
			p0 = gdpt(V[0]); 
			for (step = 1; step <= BEZIERSUBDIVISION; step++) {
				p1 = gdpt(Bezier(V, 3, (double)step/BEZIERSUBDIVISION, NULL, NULL));
				gdImageLine(im, ROUND(p0.x), ROUND(p0.y),
					ROUND(p1.x), ROUND(p1.y), pen);
				p0 = p1;
			}
		}
		if (brush)
			gdImageDestroy(brush);
	}
}

static  void
gd_polygon(point *A, int n, int filled)
{
	pointf		p;
	int		i;
	gdPoint		*points;
	int		style[20];
	int		pen, width;
	gdImagePtr	brush = NULL;

	if (cstk[SP].pen != P_NONE) {
		if (cstk[SP].pen == P_DASHED) {
			for (i = 0; i < 10; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 20; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 20);
			pen = gdStyled;
		} else if (cstk[SP].pen == P_DOTTED) {
			for (i = 0; i < 2; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 12; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 12);
			pen = gdStyled;
		} else {
			pen = cstk[SP].pencolor;
		}
#if 1
		/* use brush instead of Thickness to improve end butts */
		gdImageSetThickness(im, WIDTH_NORMAL);
                if (cstk[SP].penwidth != WIDTH_NORMAL) {
			width=cstk[SP].penwidth * Zoom;
                        brush = gdImageCreate(width,width);
                        gdImagePaletteCopy(brush, im);
                        gdImageFilledRectangle(brush,
                           0,0,width-1, width-1, cstk[SP].pencolor);
                        gdImageSetBrush(im, brush);
			if (pen == gdStyled) pen = gdStyledBrushed;      
			else pen = gdBrushed;      
		}
#else
		width = cstk[SP].penwidth;
		gdImageSetThickness(im, width);
#endif
		points = N_GNEW(n,gdPoint);
		for (i = 0; i < n; i++) {
			p.x = A[i].x; p.y = A[i].y;
			p = gdpt(p);
			points[i].x = ROUND(p.x); points[i].y = ROUND(p.y);
		}
		if (filled) gdImageFilledPolygon(im, points, n, cstk[SP].fillcolor);
		gdImagePolygon(im, points, n, pen);
		free(points);
		if (brush)
			gdImageDestroy(brush);
	}
}

static  void
gd_ellipse(point p, int rx, int ry, int filled)
{
	pointf		mp;
	int		i;
	int		style[40];  /* need 2* size for arcs, I don't know why */
	int		pen, width;
	gdImagePtr	brush = NULL;

	if (cstk[SP].pen != P_NONE) {
		if (cstk[SP].pen == P_DASHED) {
			for (i = 0; i < 20; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 40; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 40);
			pen = gdStyled;
		} else if (cstk[SP].pen == P_DOTTED) {
			for (i = 0; i < 2; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 24; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 24);
			pen = gdStyled;
		} else {
			pen = cstk[SP].pencolor;
		}
#if 1
		/* use brush instead of Thickness to improve outline appearance */
		gdImageSetThickness(im, WIDTH_NORMAL);
                if (cstk[SP].penwidth != WIDTH_NORMAL) {
			width = cstk[SP].penwidth;
                        brush = gdImageCreate(width,width);
                        gdImagePaletteCopy(brush, im);
                        gdImageFilledRectangle(brush,
                           0,0,width-1, width-1, cstk[SP].pencolor);
                        gdImageSetBrush(im, brush);
			if (pen == gdStyled) pen = gdStyledBrushed;      
			else pen = gdBrushed;      
		}
#else
		width = cstk[SP].penwidth;
		gdImageSetThickness(im, width);
#endif
		if (Rot) {int t; t = rx; rx = ry; ry = t;}
		mp.x = p.x; mp.y = p.y;
		mp = gdpt(mp);
		if (filled) {
			gdImageFilledEllipse(im, ROUND(mp.x), ROUND(mp.y),
				ROUND(Zoom*(rx + rx)), ROUND(Zoom*(ry + ry)),
				cstk[SP].fillcolor);
		}
		gdImageArc(im, ROUND(mp.x), ROUND(mp.y),
			ROUND(Zoom*(rx + rx)), ROUND(Zoom*(ry + ry)), 0, 360, pen);
		if (brush)
			gdImageDestroy(brush);
	}
}

static  void
gd_polyline(point* A, int n)
{
	pointf		p, p1;
	int		i;
	int		style[20];
	int		pen, width;
	gdImagePtr	brush = NULL;

	if (cstk[SP].pen != P_NONE) {
		if (cstk[SP].pen == P_DASHED) {
			for (i = 0; i < 10; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 20; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 20);
			pen = gdStyled;
		} else if (cstk[SP].pen == P_DOTTED) {
			for (i = 0; i < 2; i++)
				style[i] = cstk[SP].pencolor;
			for (; i < 12; i++)
				style[i] = gdTransparent;
			gdImageSetStyle(im, style, 12);
			pen = gdStyled;
		} else {
			pen = cstk[SP].pencolor;
		}
#if 0
                if (cstk[SP].penwidth != WIDTH_NORMAL) {
			width = cstk[SP].penwidth;
                        brush = gdImageCreate(width,width);
                        gdImagePaletteCopy(brush, im);
                        gdImageFilledRectangle(brush,
                           0,0,width-1,width-1,cstk[SP].pencolor);
                        gdImageSetBrush(im, brush);
			if (pen == gdStyled) pen = gdStyledBrushed;      
			else pen = gdBrushed;      
		}
#else
		width = cstk[SP].penwidth;
		gdImageSetThickness(im, width);
#endif
		p.x = A[0].x;
		p.y = A[0].y;
		p = gdpt(p);
		for (i = 1; i < n; i++) {
			p1.x = A[i].x;
			p1.y = A[i].y;
			p1 = gdpt(p1);
			gdImageLine(im, ROUND(p.x), ROUND(p.y),
				ROUND(p1.x), ROUND(p1.y), pen);
			p.x = p1.x;
			p.y = p1.y;
		}
		if (brush)
			gdImageDestroy(brush);
	}
}

static gdImagePtr loadshapeimage(char *name)
{
	gdImagePtr	rv = 0;
	char	*shapeimagefile,*suffix;
	FILE	*in = NULL;

	if ((shapeimagefile=safefile(name))) {
#ifndef MSWIN32
		in = fopen (shapeimagefile, "r");
#else
		in = fopen (shapeimagefile, "rb");
#endif
	}
	if (!in) 
		agerr(AGERR, "couldn't open image file %s\n",shapeimagefile);
	else {
		suffix = strrchr(shapeimagefile,'.');
		if (!suffix) suffix = shapeimagefile; else suffix++;
		if (!strcasecmp(suffix,"wbmp")) rv = gdImageCreateFromWBMP(in);
#ifdef WITH_GIF
		else if (!strcasecmp(suffix,"gif")) rv = gdImageCreateFromGif(in);
#endif
#ifdef HAVE_LIBPNG
		else if (!strcasecmp(suffix,"png")) rv = gdImageCreateFromPng(in);
#endif
#ifdef HAVE_LIBJPEG
		else if (!strcasecmp(suffix,"jpeg")||!strcasecmp(suffix,"jpg")) rv = gdImageCreateFromJpeg(in);
#endif
		else if (!strcasecmp(suffix,"xbm")) rv = gdImageCreateFromXbm(in);
		else agerr(AGERR, "image file %s suffix not recognized\n",name);
		fclose(in);
		if (!rv) agerr(AGERR, "image file %s contents were not recognized\n",name);
	}
	return rv;
}

typedef struct imagerec_s {
    Dtlink_t        link;
    char            *name;
	gdImagePtr		im;
} imagerec_t;


static void imagerec_free(Dict_t *dict, Void_t *p, Dtdisc_t *disc)
{
	gdImagePtr im = ((imagerec_t*)p)->im;
	if (im) gdImageDestroy(im);
}

static Dtdisc_t ImageDictDisc = {
    offsetof(imagerec_t,name),   /* key */
    -1,                     /* size */
    0,                      /* link offset */
    NIL(Dtmake_f),
	imagerec_free,
    NIL(Dtcompar_f),
    NIL(Dthash_f),
    NIL(Dtmemory_f),
    NIL(Dtevent_f)
};

static gdImagePtr getshapeimage(char *name)
{
	imagerec_t	probe, *val;
    if (!name) return 0;  /* cdt does not like NULL keys */
	if (!ImageDict) ImageDict = dtopen(&ImageDictDisc,Dttree);
	probe.name = name;
	val = dtsearch(ImageDict,&probe);
	if (!val) {
		val = GNEW(imagerec_t);
		val->name = name;
		val->im = loadshapeimage(name);
		dtinsert(ImageDict,val);
	}
	return val->im;
}

static  void
gd_user_shape(char *name, point *A, int n, int filled)
{
	gdImagePtr im2 = 0;
	pointf	destul, destlr;
	pointf	ul, lr;		/* upper left, lower right */
	double	sx, sy;		/* target size */
	double  scalex, scaley;  /* scale factors */
	int		i;
	char *shapeimagefile;

	shapeimagefile = agget(Curnode,"shapefile");
	im2 = getshapeimage(shapeimagefile);
	if (im2) {
		/* compute dest origin and size */
		ul.x = lr.x = A[0].x; ul.y = lr.y = A[0].y;
		for (i = 1; i < n; i++) {
			if (ul.x > A[i].x) ul.x = A[i].x;
			if (ul.y < A[i].y) ul.y = A[i].y;
			if (lr.y > A[i].y) lr.y = A[i].y;
			if (lr.x < A[i].x) lr.x = A[i].x;
		}
		destul = gdpt(ul);
		destlr = gdpt(lr);
		scalex = (destlr.x - destul.x)/(double)(im2->sx);
		scaley = (destlr.y - destul.y)/(double)(im2->sy);
		/* keep aspect ratio fixed by just using the smaller scale */
		if (scalex < scaley) {
			sx = ROUND(im2->sx * scalex);
			sy = ROUND(im2->sy * scalex);
		}
		else {
			sx = ROUND(im2->sx * scaley);
			sy = ROUND(im2->sy * scaley);
		}
		gdImageCopyResized(im,im2,ROUND(destul.x),ROUND(destul.y),0,0,sx,sy,im2->sx,im2->sy);
	}
}

point gd_user_shape_size(node_t *n, char *shapeimagefile)
{
	point		rv;
	gdImagePtr	im;

	Curnode = n;
	im = getshapeimage(shapeimagefile);
	if (im) {rv.x = im->sx / SCALE; rv.y = im->sy / SCALE; }
	else rv.x = rv.y = 0;
	return rv;
}

codegen_t	GD_CodeGen = {
	0, /* gd_reset */
	gd_begin_job, gd_end_job,
	gd_begin_graph_to_file, gd_end_graph_to_file, 
	gd_begin_page, gd_end_page,
	0, /* gd_begin_layer */ 0, /* gd_end_layer */
	0, /* gd_begin_cluster */ 0, /* gd_end_cluster */
	0, /* gd_begin_nodes */ 0, /* gd_end_nodes */
	0, /* gd_begin_edges */ 0, /* gd_end_edges */
	gd_begin_node, gd_end_node,
	0, /* gd_begin_edge */ 0, /* gd_end_edge */
	gd_begin_context, gd_end_context,
	gd_set_font, gd_textline,
	gd_set_pencolor, gd_set_fillcolor, gd_set_style,
	gd_ellipse, gd_polygon,
	gd_bezier, gd_polyline,
	0, /* bezier_has_arrows */
	0, /* gd_comment */
	gd_textsize,
	gd_user_shape,
	gd_user_shape_size
};

codegen_t	memGD_CodeGen = {		/* see tcldot */
	0, /* gd_reset*/
	gd_begin_job, gd_end_job,
	gd_begin_graph_to_memory, gd_end_graph_to_memory,
	gd_begin_page, gd_end_page,
	0, /* gd_begin_layer */ 0, /* gd_end_layer */
	0, /* gd_begin_cluster */ 0, /* gd_end_cluster */
	0, /* gd_begin_nodes */ 0, /* gd_end_nodes */
	0, /* gd_begin_edges */ 0, /* gd_end_edges */
	0, /* gd_begin_node */ 0, /* gd_end_node */
	0, /* gd_begin_edge */ 0, /* gd_end_edge */
	gd_begin_context, gd_end_context,
	gd_set_font, gd_textline,
	gd_set_pencolor, gd_set_fillcolor, gd_set_style,
	gd_ellipse, gd_polygon,
	gd_bezier, gd_polyline,
	0, /* bezier_has_arrows */
	0, /* gd_comment */
	gd_textsize,
	gd_user_shape,
	gd_user_shape_size
};
