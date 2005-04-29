/* $Id: gvre_svg.c,v 1.1 2004/05/04 21:50:47 yakimoto Exp $ $Revision: 1.1 $ */
/* vim:set shiftwidth=4 ts=8: */
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

#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
                                                                                                 
#include "macros.h"
#include "const.h"
#include "types.h"
#include "graph.h"
#include "globals.h"
#include "utils.h"

/* FIXME - needed for colorxlate() - this has got to go */
#include "renderprocs.h"

#include "gvre.h"

#ifdef HAVE_LIBZ
#include "zlib.h"
#ifdef MSWIN32
#include <io.h>
#endif
#endif


/* SVG font modifiers */
#define REGULAR 0
#define BOLD	1
#define ITALIC	2

/* SVG patterns */
#define P_SOLID	0
#define P_NONE  15
#define P_DOTTED 4		/* i wasn't sure about this */
#define P_DASHED 11		/* or this */

/* SVG bold line constant */
#define WIDTH_NORMAL 1
#define WIDTH_BOLD 3

#ifdef TESTFAILED
/* sodipodi doesn't understand style sheets */
#define DEFAULT_STYLE\
"<style type='text/css'>\n" \
"<![CDATA[\n" \
".node ellipse {fill:none;filter:URL(#MyFilter)}\n" \
".node polygon {fill:none;filter:URL(#MyFilter)}\n" \
".cluster polygon {fill:none;filter:URL(#MyFilter)}\n" \
".edge path {fill:none;stroke:black;stroke-width:1;}\n" \
"text {font-family:Times;stroke:black;stroke-width:.4;font-size:12px}\n" \
"]]>\n" \
"</style>"

#define DEFAULT_FILTER \
"<filter id=\"MyFilter\" x=\"-20%\" y=\"-20%\" width=\"160%\" height=\"160%\">\n" \
"<feGaussianBlur in=\"SourceAlpha\" stdDeviation=\"4\" result=\"blur\"/>\n" \
"<feOffset in=\"blur\" dx=\"4\" dy=\"4\" result=\"offsetBlur\"/>\n" \
"<feSpecularLighting in=\"blur\" surfaceScale=\"5\" specularConstant=\"1\"\n" \
"specularExponent=\"10\" style=\"lighting-color:white\" result=\"specOut\">\n" \
"<fePointLight x=\"-5000\" y=\"-10000\" z=\"20000\"/>\n" \
"</feSpecularLighting>\n" \
"<feComposite in=\"specOut\" in2=\"SourceAlpha\" operator=\"in\" result=\"specOut\"/>\n" \
"<feComposite in=\"SourceGraphic\" in2=\"specOut\" operator=\"arithmetic\"\n" \
"k1=\"0\" k2=\"1\" k3=\"1\" k4=\"0\" result=\"litPaint\"/>\n" \
"<feMerge>\n" \
"<feMergeNode in=\"offsetBlur\"/>\n" \
"<feMergeNode in=\"litPaint\"/>\n" \
"</feMerge>\n" \
"</filter>"
#endif

#define SCALE 1

/* SVG dash array */
static char *sdarray = "5,2";
/* SVG dot array */
static char *sdotarray = "1,5";

static int N_pages;
static pointf Offset;
static box PB;

static int GraphURL, ClusterURL, NodeURL, EdgeURL;

typedef struct context_t {
    char *pencolor, *fillcolor, *fontfam, fontopt, font_was_set;
    char pen, fill, penwidth, style_was_set;
    double fontsz;
} context_t;

#define MAXNEST 4
static context_t cstk[MAXNEST];
static int SP;

#ifdef HAVE_LIBZ
static gzFile Zfile;
#endif

static int svg_fputs(GVC_t *gvc, char *s)
{
    int len;

    len = strlen(s);
#ifdef HAVE_LIBZ
    if (Output_lang == SVGZ)
	return gzwrite(Zfile, s, (unsigned) len);
    else
#endif
	return fwrite(s, sizeof(char), (unsigned) len, gvc->job->output_file);
}

/* svg_printf:
 * Note that this function is unsafe due to the fixed buffer size.
 * It should only be used when the caller is sure the input will not
 * overflow the buffer. In particular, it should be avoided for
 * input coming from users. Also, if vsnprintf is available, the
 * code should check for return values to use it safely.
 */
static int svg_printf(GVC_t *gvc, const char *format, ...)
{
    char buf[BUFSIZ];
    va_list argp;
    int len;

    va_start(argp, format);
#ifdef HAVE_VSNPRINTF
    (void) vsnprintf(buf, sizeof(buf), format, argp);
#else
    (void) vsprintf(buf, format, argp);
#endif
    va_end(argp);
    len = strlen(buf);		/* some *sprintf (e.g C99 std)
				   don't return the number of
				   bytes actually written */

#ifdef HAVE_LIBZ
    if (Output_lang == SVGZ)
	return gzwrite(Zfile, buf, (unsigned) len);
    else
#endif
	return fwrite(buf, sizeof(char), (unsigned) len, gvc->job->output_file);
}

static void init_svg(void)
{
    SP = 0;
    cstk[0].pencolor = DEFAULT_COLOR;	/* SVG pencolor */
    cstk[0].fillcolor = "";	/* SVG fillcolor */
    cstk[0].fontfam = DEFAULT_FONTNAME;	/* font family name */
    cstk[0].fontsz = DEFAULT_FONTSIZE;	/* font size */
    cstk[0].fontopt = REGULAR;	/* modifier: REGULAR, BOLD or ITALIC */
    cstk[0].pen = P_SOLID;	/* pen pattern style, default is solid */
    cstk[0].fill = P_NONE;
    cstk[0].penwidth = WIDTH_NORMAL;
}

/* can't hack a transform directly in SVG preamble, alas */
static point svgpt(GVC_t *gvc, pointf p)
{
    pointf rv;
    point RV;

    if (gvc->rot == 0) {
	rv.x = PB.LL.x / gvc->scale + p.x + Offset.x;
	rv.y = PB.UR.y / gvc->scale - 1 - p.y - Offset.y;
    } else {
	rv.x = PB.UR.x / gvc->scale - 1 - p.y - Offset.x;
	rv.y = PB.UR.y / gvc->scale - 1 - p.x - Offset.y;
    }
    PF2P(rv, RV);
    return RV;
}

static void svgbzptarray(GVC_t *gvc, pointf * A, int n)
{
    int i;
    point p;
    char *c;

    c = "M";			/* first point */
    for (i = 0; i < n; i++) {
	p = svgpt(gvc, A[i]);
	svg_printf(gvc, "%s%d,%d", c, p.x, p.y);
	if (i == 0)
	    c = "C";		/* second point */
	else
	    c = " ";		/* remaining points */
    }
}

static char *svg_resolve_color(char *name)
{
/* color names from http://www.w3.org/TR/SVG/types.html */
    static char *known_colors[] = {
	"aliceblue", "antiquewhite", "aqua", "aquamarine", "azure",
	"beige", "bisque", "black", "blanchedalmond", "blue",
	"blueviolet", "brown", "burlywood",
	"cadetblue", "chartreuse", "chocolate", "coral",
	"cornflowerblue",
	"cornsilk", "crimson", "cyan",
	"darkblue", "darkcyan", "darkgoldenrod", "darkgray",
	"darkgreen",
	"darkgrey", "darkkhaki", "darkmagenta", "darkolivegreen",
	"darkorange", "darkorchid", "darkred", "darksalmon",
	"darkseagreen", "darkslateblue", "darkslategray",
	"darkslategrey", "darkturquoise", "darkviolet", "deeppink",
	"deepskyblue", "dimgray", "dimgrey", "dodgerblue",
	"firebrick", "floralwhite", "forestgreen", "fuchsia",
	"gainsboro", "ghostwhite", "gold", "goldenrod", "gray",
	"green",
	"greenyellow", "grey",
	"honeydew", "hotpink", "indianred",
	"indigo", "ivory", "khaki",
	"lavender", "lavenderblush", "lawngreen", "lemonchiffon",
	"lightblue", "lightcoral", "lightcyan",
	"lightgoldenrodyellow",
	"lightgray", "lightgreen", "lightgrey", "lightpink",
	"lightsalmon", "lightseagreen", "lightskyblue",
	"lightslategray", "lightslategrey", "lightsteelblue",
	"lightyellow", "lime", "limegreen", "linen",
	"magenta", "maroon", "mediumaquamarine", "mediumblue",
	"mediumorchid", "mediumpurple", "mediumseagreen",
	"mediumslateblue", "mediumspringgreen", "mediumturquoise",
	"mediumvioletred", "midnightblue", "mintcream",
	"mistyrose", "moccasin",
	"navajowhite", "navy", "oldlace",
	"olive", "olivedrab", "orange", "orangered", "orchid",
	"palegoldenrod", "palegreen", "paleturquoise",
	"palevioletred", "papayawhip", "peachpuff", "peru", "pink",
	"plum", "powderblue", "purple",
	"red", "rosybrown", "royalblue",
	"saddlebrown", "salmon", "sandybrown", "seagreen",
	"seashell",
	"sienna", "silver", "skyblue", "slateblue", "slategray",
	"slategrey", "snow", "springgreen", "steelblue",
	"tan", "teal", "thistle", "tomato", "turquoise",
	"violet",
	"wheat", "white", "whitesmoke",
	"yellow", "yellowgreen",
	0
    };
    static char buf[SMALLBUF];
    char *tok, **known;
    color_t color;

    tok = canontoken(name);
    for (known = known_colors; *known; known++)
	if (streq(tok, *known))
	    break;
    if (*known == 0) {
	if (streq(tok, "transparent")) {
	    tok = "none";
	} else {
	    colorxlate(name, &color, RGBA_BYTE);
	    sprintf(buf, "#%02x%02x%02x",
		    color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	    tok = buf;
	}
    }
    return tok;
}

static void svg_font(GVC_t *gvc, context_t * cp)
{
    char *color, buf[BUFSIZ];
    int needstyle = 0;

    strcpy(buf, " style=\"");
    if (strcasecmp(cp->fontfam, DEFAULT_FONTNAME)) {
	sprintf(buf + strlen(buf), "font-family:%s;", cp->fontfam);
	needstyle++;
    }
    if (cp->fontsz != DEFAULT_FONTSIZE) {
	sprintf(buf + strlen(buf), "font-size:%.2f;", (cp->fontsz));
	needstyle++;
    }
    color = svg_resolve_color(cp->pencolor);
    if ((strcasecmp(color, "black"))) {
	sprintf(buf + strlen(buf), "fill:%s;", color);
	needstyle++;
    }
    if (needstyle) {
	strcat(buf, "\"");
	svg_fputs(gvc, buf);
    }
}


static void svg_grstyle(GVC_t *gvc, context_t * cp, int filled)
{
    svg_fputs(gvc, " style=\"");
    if (filled)
	svg_printf(gvc, "fill:%s;", svg_resolve_color(cp->fillcolor));
    else
	svg_fputs(gvc, "fill:none;");
    svg_printf(gvc, "stroke:%s;", svg_resolve_color(cp->pencolor));
    if (cp->penwidth != WIDTH_NORMAL)
	svg_printf(gvc, "stroke-width:%d;", cp->penwidth);
    if (cp->pen == P_DASHED) {
	svg_printf(gvc, "stroke-dasharray:%s;", sdarray);
    } else if (cp->pen == P_DOTTED) {
	svg_printf(gvc, "stroke-dasharray:%s;", sdotarray);
    }
    svg_fputs(gvc, "\"");
}

static void svg_comment(GVC_t * gvc, void *obj, attrsym_t * sym)
{
    char *str = late_string(obj, sym, "");
    if (str[0]) {
	svg_fputs(gvc, "<!-- ");
	/* FIXME - should check for --> sequences in str */
	svg_fputs(gvc, str);
	svg_fputs(gvc, " -->\n");
    }
}

static void svg_begin_job(GVC_t * gvc)
{
    char *s;
#if HAVE_LIBZ
    int fd;
#endif
    graph_t *g = gvc->g;

    if (Output_lang == SVGZ) {
#if HAVE_LIBZ
	fd = dup(fileno(gvc->job->output_file));	/* open dup so can gzclose 
					   independent of FILE close */
#ifdef HAVE_SETMODE
#ifdef O_BINARY
	/*
	 * Windows will do \n -> \r\n  translations on
	 * stdout unless told otherwise.
	 */
	setmode(fd, O_BINARY);
#endif
#endif

	Zfile = gzdopen(fd, "wb");
	if (!Zfile) {
	    agerr(AGERR, "Error opening compressed output file\n");
	    exit(1);
	}
#else
	agerr(AGERR,
	      "No support for compressed output. Not compiled with zlib.\n");
	exit(1);
#endif
    }

/*	Pages = pages; */
    N_pages = gvc->pages.x * gvc->pages.y;
    svg_fputs
	(gvc, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
    if ((s = agget(g, "stylesheet")) && strlen(s)) {
	svg_fputs(gvc, "<?xml-stylesheet href=\"");
	svg_fputs(gvc, s);
	svg_fputs(gvc, "\" type=\"text/css\"?>\n");
    }
    svg_fputs(gvc, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n");
    svg_fputs
	(gvc, " \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\"");
#if 1
    /* This is to work around a bug in the SVG 1.0 DTD */
    if ((agfindattr(g, "URL")
	 || agfindattr(g->proto->n, "URL")
	 || agfindattr(g->proto->e, "URL"))) {
	svg_fputs(gvc, " [\n <!ATTLIST svg xmlns:xlink");
	svg_fputs(gvc, " CDATA #FIXED \"http://www.w3.org/1999/xlink\">\n]");
    }
#endif
    svg_fputs(gvc, ">\n<!-- Generated by ");
    svg_fputs(gvc, gvc->info[0]);
    svg_fputs(gvc, " version ");
    svg_fputs(gvc, gvc->info[1]);
    svg_fputs(gvc, " (");
    svg_fputs(gvc, gvc->info[2]);
    svg_fputs(gvc, ")\n     For user: ");
    svg_fputs(gvc, gvc->user);
    svg_fputs(gvc, "   Title: ");
    svg_fputs(gvc, g->name);
    svg_printf(gvc, "    Pages: %d -->\n", N_pages);
}

static void svg_begin_graph(GVC_t * gvc)
{
    char *str;
    double res;
    graph_t *g = gvc->g;

    PB.LL.x = PB.LL.y = 0;
    PB.UR.x = (gvc->bb.UR.x - gvc->bb.LL.x + 2 * GD_drawing(g)->margin.x) * SCALE;
    PB.UR.y = (gvc->bb.UR.y - gvc->bb.LL.y + 2 * GD_drawing(g)->margin.y) * SCALE;
    Offset.x = GD_drawing(g)->margin.x * SCALE;
    Offset.y = GD_drawing(g)->margin.y * SCALE;
    if (gvc->onetime) {
#if 0
	fprintf(stderr, "LL %d %d UR %d %d\n", PB.LL.x, PB.LL.y,
		PB.UR.x, PB.UR.y);
#endif
	init_svg();
	svg_comment(gvc, g, agfindattr(g, "comment"));
    }
    if ((str = agget(g, "resolution")) && str[0])
	res = atof(str);
    else
	res = 96;		/* terrific guess */
    if (res < 1.0)
	svg_printf(gvc, "<svg width=\"%dpt\" height=\"%dpt\"\n",
		   PB.UR.x - PB.LL.x + 2, PB.UR.y - PB.LL.y + 2);
    else
	svg_printf(gvc, "<svg width=\"%dpx\" height=\"%dpx\"\n",
		   ROUND((res / POINTS_PER_INCH) *
			 (PB.UR.x - PB.LL.x)) + 2,
		   ROUND((res / POINTS_PER_INCH) *
			 (PB.UR.y - PB.LL.y) + 2));
    /* establish absolute units in points */
    svg_printf(gvc, " viewBox = \"%d %d %d %d\"\n", PB.LL.x - 1,
	       PB.LL.y - 1, PB.UR.x + 1, PB.UR.y + 1);
    /* namespace of svg */
    svg_fputs(gvc, " xmlns=\"http://www.w3.org/2000/svg\"");
    /* namespace of xlink  if needed */
    if ((agfindattr(g, "URL")
	 || agfindattr(g->proto->n, "URL")
	 || agfindattr(g->proto->e, "URL"))) {
	svg_fputs(gvc, " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
    }
    svg_fputs(gvc, ">\n");
}

static void svg_end_graph(GVC_t * gvc)
{
    svg_fputs(gvc, "</svg>\n");
#ifdef HAVE_LIBZ
    if (Output_lang == SVGZ)
	gzclose(Zfile);
#endif
}

static void svg_output_anchor(GVC_t *gvc, char *url, char *label, char *tooltip)
{
    svg_fputs(gvc, "<a xlink:href=\"");
    svg_fputs(gvc, xml_string(url));
    svg_fputs(gvc, "\"");
    if (tooltip && tooltip[0]) {
	svg_fputs(gvc, " xlink:title=\"");
	svg_fputs(gvc, xml_string(tooltip));
	svg_fputs(gvc, "\"");
    }
    svg_fputs(gvc, ">\n");
}

static void svg_begin_page(GVC_t * gvc)
{
/*	int		page_number; */
    char *s;
    /* point        sz; */
    char *kind = "graph";
    graph_t *g = gvc->g;

/*	page_number =  page.x + page.y * Pages.x + 1; */
    /* sz = sub_points(PB.UR,PB.LL); */

    /* its really just a page of the graph, but its still a graph,
     * and it is the entire graph if we're not currently paging */
    svg_printf(gvc, "<g id=\"%s0\" class=\"%s\"", kind, kind);
    if (gvc->scale != 1.0)
	svg_printf(gvc, " transform = \"scale(%f)\"\n", gvc->scale);
    /* default style */
    svg_fputs(gvc, " style=\"font-family:");
    svg_fputs(gvc, cstk[0].fontfam);
    svg_printf(gvc, ";font-size:%.2f;\">\n", cstk[0].fontsz);
    svg_fputs(gvc, "<title>");
    svg_fputs(gvc, xml_string(g->name));
    svg_fputs(gvc, "</title>\n");
    if ((s = agget(g, "URL")) && strlen(s)) {
	GraphURL = 1;
	s = strdup_and_subst_graph(s, g);
	svg_fputs(gvc, "<a xlink:href=\"");
	svg_fputs(gvc, xml_string(s));
	svg_fputs(gvc, "\">\n");
	free(s);
    } else {
	GraphURL = 0;
    }
}

static void svg_end_page(GVC_t * gvc)
{
    if (GraphURL) {
	svg_fputs(gvc, "</a>");
	ClusterURL = 0;
    }
    svg_fputs(gvc, "</g>\n");
}

static void svg_begin_layer(GVC_t * gvc)
{
    /* svg_printf(gvc, "<g id=\"%s\" class=\"layer\">\n", xml_string(layerName)); */
    svg_fputs(gvc, "<g id=\"");
    svg_fputs(gvc, xml_string(gvc->layerName));
    svg_fputs(gvc, "\" class=\"layer\">\n");
    Obj = NONE;
}

static void svg_end_layer(GVC_t * gvc)
{
    svg_fputs(gvc, "</g>\n");
    Obj = NONE;
}

static void svg_begin_cluster(GVC_t * gvc)
{
    char *s;
    graph_t *sg = gvc->sg;
    char *kind = "cluster";

    svg_printf(gvc, "<g id=\"%s%ld\" class=\"%s\">", kind,
	       sg->meta_node->id, kind);
    svg_fputs(gvc, "<title>");
    svg_fputs(gvc, xml_string(sg->name));
    svg_fputs(gvc, "</title>\n");
    if ((s = agget(sg, "URL")) && strlen(s)) {
	ClusterURL = 1;
	s = strdup_and_subst_graph(s, sg);
	svg_fputs(gvc, "<a xlink:href=\"");
	svg_fputs(gvc, xml_string(s));
	svg_fputs(gvc, "\">\n");
	free(s);
    } else {
	ClusterURL = 0;
    }
}

static void svg_end_cluster(GVC_t * gvc)
{
    if (ClusterURL) {
	svg_fputs(gvc, "</a>");
	ClusterURL = 0;
    }
    svg_fputs(gvc, "</g>\n");
}

static void svg_begin_node(GVC_t * gvc)
{
    char *url, *label, *tooltip, *m_tooltip = NULL;
    node_t *n = gvc->n;
    char *kind = "node";

#if 0
    svg_printf(gvc, "<!-- %s -->\n", n->name);
    svg_comment(n, N_comment);
#endif
    svg_printf(gvc, "<g id=\"%s%ld\" class=\"%s\">", kind, n->id, kind);
    svg_fputs(gvc, "<title>");
    svg_fputs(gvc, xml_string(n->name));
    svg_fputs(gvc, "</title>\n");
    if ((url = agget(n, "URL")) && strlen(url)) {
	NodeURL = 1;
	url = strdup_and_subst_node(url, n);
	label = ND_label(n)->text;
	if ((tooltip = agget(n, "tooltip")) && strlen(tooltip)) {
	    m_tooltip = tooltip = strdup_and_subst_node(tooltip, n);
	} else {
	    tooltip = label;
	}
	svg_output_anchor(gvc, url, label, tooltip);
	if (m_tooltip)
	    free(tooltip);
	free(url);
    } else {
	NodeURL = 0;
    }
}

static void svg_end_node(GVC_t * gvc)
{
    if (NodeURL) {
	svg_fputs(gvc, "</a>");
	NodeURL = 0;
    }
    svg_fputs(gvc, "</g>\n");
}

static void svg_begin_edge(GVC_t * gvc)
{
    char *url, *label = NULL, *tooltip, *m_tooltip = NULL, *edgeop;
    textlabel_t *lab = NULL;
    edge_t *e = gvc->e;
    char *kind = "edge";

    svg_printf(gvc, "<g id=\"%s%ld\" class=\"edge\">", kind, e->id, kind);
    if (e->tail->graph->root->kind & AGFLAG_DIRECTED)
	edgeop = "-&gt;";
    else
	edgeop = "--";
    svg_fputs(gvc, "<title>");
    svg_fputs(gvc, xml_string(e->tail->name));
    svg_fputs(gvc, edgeop);
    /* can't do this in single svg_printf because
     * xml_string's buffer gets reused. */
    svg_fputs(gvc, xml_string(e->head->name));
    svg_fputs(gvc, "</title>\n");
    if ((url = agget(e, "URL")) && strlen(url)) {
	EdgeURL = 1;
	url = strdup_and_subst_edge(url, e);
	if ((lab = ED_label(e))) {
	    label = lab->text;
	}
	if ((tooltip = agget(e, "tooltip")) && strlen(tooltip)) {
	    m_tooltip = tooltip = strdup_and_subst_edge(tooltip, e);
	} else {
	    tooltip = label;
	}
	svg_output_anchor(gvc, url, label, tooltip);
	if (m_tooltip)
	    free(tooltip);
	free(url);
    } else {
	EdgeURL = 0;
    }
}

static void svg_end_edge(GVC_t * gvc)
{
    if (EdgeURL) {
	svg_fputs(gvc, "</a>");
	EdgeURL = 0;
    }
    svg_fputs(gvc, "</g>\n");
}

static void svg_begin_context(GVC_t * gvc)
{
    assert(SP + 1 < MAXNEST);
    cstk[SP + 1] = cstk[SP];
    SP++;
}

static void svg_end_context(GVC_t * gvc)
{
    int psp = SP - 1;
    assert(SP > 0);
    /*free(cstk[psp].fontfam); */
    SP = psp;
}

static void svg_set_font(GVC_t * gvc, char *name, double size)
{
    char *p;
#if 0
/* see below */
    char *q;
#endif
    context_t *cp;

    cp = &(cstk[SP]);
    cp->font_was_set = TRUE;
    cp->fontsz = size;
    p = strdup(name);
#if 0
/*
 * this doesn't work as originally intended 
 * fontnames can be things like: "Times-Roman"
 * where "Roman" is not an ITALIC or BOLD indicator.
 */
    if ((q = strchr(p, '-'))) {
	*q++ = 0;
	if (strcasecmp(q, "italic") == 0)
	    cp->fontopt = ITALIC;
	else if (strcasecmp(q, "bold") == 0)
	    cp->fontopt = BOLD;
    }
    cp->fontfam = p;
#else
    cp->fontfam = p;
#endif
}

static void svg_set_pencolor(GVC_t * gvc, char *name)
{
    cstk[SP].pencolor = name;
}

static void svg_set_fillcolor(GVC_t * gvc, char *name)
{
    cstk[SP].fillcolor = name;
}

static void svg_set_style(GVC_t * gvc, char **s)
{
    char *line, *p;
    context_t *cp;

    cp = &(cstk[SP]);
    while ((p = line = *s++)) {
	if (streq(line, "solid"))
	    cp->pen = P_SOLID;
	else if (streq(line, "dashed"))
	    cp->pen = P_DASHED;
	else if (streq(line, "dotted"))
	    cp->pen = P_DOTTED;
	else if (streq(line, "invis"))
	    cp->pen = P_NONE;
	else if (streq(line, "bold"))
	    cp->penwidth = WIDTH_BOLD;
	else if (streq(line, "setlinewidth")) {
	    while (*p)
		p++;
	    p++;
	    cp->penwidth = atol(p);
	} else if (streq(line, "filled"))
	    cp->fill = P_SOLID;
	else if (streq(line, "unfilled"))
	    cp->fill = P_NONE;
	else {
	    agerr(AGERR,
		  "svg_set_style: unsupported style %s - ignoring\n",
		  line);
	}
	cp->style_was_set = TRUE;
    }
    /* if (cp->style_was_set) svg_style(cp); */
}

static void svg_textline(GVC_t * gvc, pointf p, textline_t * line)
{
    char *anchor, *string;
    point mp;
    context_t *cp;

    string = xml_string(line->str);
    if (strlen(string) == 0) {
	/* its zero length, don't draw */
	return;
    }
    cp = &(cstk[SP]);
    if (cp->pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    switch (line->just) {
    case 'l':
	anchor = "start";
	break;
    case 'r':
	anchor = "end";
	break;
    default:
    case 'n':
	anchor = "middle";
	break;
    }

    mp = svgpt(gvc, p);
    svg_printf(gvc, "<text text-anchor=\"%s\" ", anchor);
    if (gvc->rot) {
	svg_printf(gvc, "transform=\"rotate(-90 %d %d)\" ", mp.x, mp.y);
    }
    svg_printf(gvc, "x=\"%d\" y=\"%d\"", mp.x, mp.y);
    svg_font(gvc, cp);
    svg_fputs(gvc, ">");
    svg_fputs(gvc, string);
    svg_fputs(gvc, "</text>\n");
}

static void svg_ellipse(GVC_t * gvc, pointf p, double rx, double ry, int filled)
{
    point mp;

    if (cstk[SP].pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    mp = svgpt(gvc, p);
    svg_printf(gvc, "<ellipse cx=\"%d\" cy=\"%d\"", mp.x, mp.y);
    if (gvc->rot) {
	int t = rx; t = rx; rx = ry; ry = t;
    }
    svg_printf(gvc, " rx=\"%d\" ry=\"%d\"", ROUND(rx), ROUND(ry));
    svg_grstyle(gvc, &cstk[SP], filled);
    svg_fputs(gvc, "/>\n");
}

static void svg_bezier(GVC_t * gvc, pointf * A, int n, int arrow_at_start,
		       int arrow_at_end)
{
    if (cstk[SP].pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    svg_fputs(gvc, "<path");
    svg_grstyle(gvc, &cstk[SP], 0);
    svg_fputs(gvc, " d=\"");
    svgbzptarray(gvc, A, n);
    svg_fputs(gvc, "\"/>\n");
}

static void svg_polygon(GVC_t * gvc, pointf * A, int n, int filled)
{
    int i;
    point p;

    if (cstk[SP].pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    svg_fputs(gvc, "<polygon");
    svg_grstyle(gvc, &cstk[SP], filled);
    svg_fputs(gvc, " points=\"");
    for (i = 0; i < n; i++) {
	p = svgpt(gvc, A[i]);
	svg_printf(gvc, "%d,%d ", p.x, p.y);
    }
    /* because Adobe SVG is broken */
    p = svgpt(gvc, A[0]);
    svg_printf(gvc, "%d,%d", p.x, p.y);
    svg_fputs(gvc, "\"/>\n");
}

static void svg_polyline(GVC_t * gvc, pointf * A, int n)
{
    int i;
    point p;

    if (cstk[SP].pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    svg_fputs(gvc, "<polyline");
    svg_grstyle(gvc, &cstk[SP], 0);
    svg_fputs(gvc, " points=\"");
    for (i = 0; i < n; i++) {
	p = svgpt(gvc, A[i]);
	svg_printf(gvc, "%d,%d ", p.x, p.y);
    }
    svg_fputs(gvc, "\"/>\n");
}

static void svg_user_shape(GVC_t * gvc, char *name, pointf * A, int np,
			   int filled)
{
    int i;
    point p;
    point sz;
    char *imagefile;
    int minx, miny;
    node_t *n = gvc->n;
    pointf PF;

    if (cstk[SP].pen == P_NONE) {
	/* its invisible, don't draw */
	return;
    }
    imagefile = agget(n, "shapefile");
    if (imagefile == 0) {
	svg_polygon(gvc, A, np, filled);
	return;
    }
/* FIXME - clean this up when NC_coord(n) is available in FP */
    p = ND_coord_i(n);
    p.x -= ND_lw_i(n);
    p.y += ND_ht_i(n) / 2;
    P2PF(p, PF);
    p = svgpt(gvc, PF);
    sz.x = ROUND((ND_lw_i(n) + ND_rw_i(n)));
    sz.y = ROUND(ND_ht_i(n));

    svg_fputs(gvc, "<clipPath id=\"mypath");
    svg_fputs(gvc, name);
    svg_fputs(gvc, n->name);
    svg_fputs(gvc, "\">\n<polygon points=\"");
    minx = svgpt(gvc, A[0]).x;
    miny = svgpt(gvc, A[0]).y;
    for (i = 0; i < np; i++) {
	p = svgpt(gvc, A[i]);

	if (p.x < minx)
	    minx = p.x;
	if (p.y < miny)
	    miny = p.y;

	svg_printf(gvc, "%d,%d ", p.x, p.y);
    }
    /* because Adobe SVG is broken (?) */
    p = svgpt(gvc, A[0]);
    svg_printf(gvc, "%d,%d ", p.x, p.y);
    svg_fputs(gvc, "\"/>\n</clipPath>\n<image xlink:href=\"");
    svg_fputs(gvc, imagefile);
    svg_printf(gvc, "\" width=\"%dpx\" height=\"%dpx\"", sz.x, sz.y);
    svg_fputs(gvc, " preserveAspectRatio=\"xMidYMid meet\"");
    svg_printf(gvc, " x=\"%d\" y=\"%d\"", minx, miny);
    svg_fputs(gvc, " clip-path=\"url(#mypath");
    svg_fputs(gvc, name);
    svg_fputs(gvc, n->name);
    svg_fputs(gvc, ")\"/>\n");
}

gvrender_engine_t gvre_SVG = {
    0,				/* svg_features */
    0,				/* svg_reset */
    svg_begin_job,
    0,				/* svg_end_job */
    svg_begin_graph,
    svg_end_graph,
    svg_begin_page,
    svg_end_page,
    svg_begin_layer,
    svg_end_layer,
    svg_begin_cluster,
    svg_end_cluster,
    0,				/* svg_begin_nodes */
    0,				/* svg_end_nodes */
    0,				/* svg_begin_edges */
    0,				/* svg_end_edges */
    svg_begin_node,
    svg_end_node,
    svg_begin_edge,
    svg_end_edge,
    svg_begin_context,
    svg_end_context,
    svg_set_font,
    svg_textline,
    svg_set_pencolor,
    svg_set_fillcolor,
    svg_set_style,
    svg_ellipse,
    svg_polygon,
    svg_bezier,
    svg_polyline,
    0,				/* svg_comment */
    0,				/* svg_textsize */
    svg_user_shape,
    0				/* svg_usershapesize */
};
