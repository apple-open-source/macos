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

#include	"render.h"
#include	"gvrender.h"
#include	"htmltable.h"

#define FILLED 	(1 << 0)
#define ROUNDED (1 << 1)
#define DIAGONALS (1 << 2)
#define AUXLABELS (1 << 3)
#define INVISIBLE (1 << 4)
#define RBCONST 12
#define RBCURVE .5

#ifndef HAVE_SINCOS
void sincos(x,s,c) double x,*s,*c; { *s = sin(x); *c = cos(x); }
#else
extern void sincos(double x, double *s, double *c);
#endif

static port	Center = { {0,0}, -1, 0, 0, 0, 0};

#define ATTR_SET(a,n) ((a) && (*(agxget(n,a->index)) != '\0'))
#define DEF_POINT 0.05
  /* extra null character needed to avoid style emitter from thinking
   * there are arguments.
   */
static char* point_style[3] = {"invis\0","filled\0",0}; 
static shape_desc* point_desc;

/* forward declarations of functions used in shapes tables */

/* static void  poly_init(GVC_t *gvc); */
/* static void  point_init(GVC_t *gvc); */
/* static void  record_init(GVC_t *gvc); */

static void  poly_free(GVC_t *gvc);
static port	poly_port(GVC_t *gvc, char *portname);
static boolean  poly_inside(inside_t *inside_context, pointf p);
static int   poly_path(GVC_t *gvc, int pt, box rv[], int* kptr);
static void  poly_gencode(GVC_t *gvc);

#ifdef NOT_READY_YET
static void  poly_shape_init(GVC_t *gvc);
static void  shape_free(GVC_t *gvc);
static port  shape_port(GVC_t *gvc, char *portname);
static boolean shape_inside(inside_t *inside_context, pointf p);
static void  shape_gencode(GVC_t *gvc);
#endif

static void  record_free(GVC_t *gvc);
static port  record_port(GVC_t *gvc, char *portname);
static boolean record_inside(inside_t *inside_context, pointf p);
static int   record_path(GVC_t *gvc, int pt, box rv[], int* kptr);
static void  record_gencode(GVC_t *gvc);

static boolean epsf_inside(inside_t *inside_context, pointf p);

/* polygon descriptions.  "polygon" with 0 sides takes all user control */

/*				      regul perip sides orien disto skew */
static polygon_t p_polygon	  = { FALSE,  1,    0,    0.,   0.,   0. };
/* builtin polygon descriptions */
static polygon_t p_ellipse	  = { FALSE,  1,    1,    0.,   0.,   0. };
static polygon_t p_circle	  = { TRUE,   1,    1,    0.,   0.,   0. };
static polygon_t p_egg		  = { FALSE,  1,    1,    0.,   -.3,  0. };
static polygon_t p_triangle	  = { FALSE,  1,    3,    0.,   0.,   0. };
static polygon_t p_box		  = { FALSE,  1,    4,    0.,   0.,   0. };
static polygon_t p_plaintext	  = { FALSE,  0,    4,    0.,   0.,   0. };
static polygon_t p_diamond	  = { FALSE,  1,    4,    45.,  0.,   0. };
static polygon_t p_trapezium	  = { FALSE,  1,    4,    0.,   -.4,  0. };
static polygon_t p_parallelogram  = { FALSE,  1,    4,    0.,   0.,   .6 };
static polygon_t p_house	  = { FALSE,  1,    5,    0.,   -.64, 0. };
static polygon_t p_pentagon	  = { FALSE,  1,    5,    0.,   0.,   0. };
static polygon_t p_hexagon	  = { FALSE,  1,    6,    0.,   0.,   0. };
static polygon_t p_septagon	  = { FALSE,  1,    7,    0.,   0.,   0. };
static polygon_t p_octagon	  = { FALSE,  1,    8,    0.,   0.,   0. };
/* redundant and undocumented builtin polygons */
static polygon_t p_doublecircle   = { TRUE,   2,    1,    0.,   0.,   0. };
static polygon_t p_invtriangle	  = { FALSE,  1,    3,    180., 0.,   0. };
static polygon_t p_invtrapezium   = { FALSE,  1,    4,    180., -.4,  0. };
static polygon_t p_invhouse	  = { FALSE,  1,    5,    180., -.64, 0. };
static polygon_t p_doubleoctagon  = { FALSE,  2,    8,    0.,   0.,   0. };
static polygon_t p_tripleoctagon  = { FALSE,  3,    8,    0.,   0.,   0. };
static polygon_t p_Mdiamond  = { FALSE,  1,    4,    45.,  0.,  0. ,DIAGONALS|AUXLABELS};
static polygon_t p_Msquare	  = { TRUE,  1,    4,    0.,  0.,  0. ,DIAGONALS};
static polygon_t p_Mcircle	  = { TRUE,   1,    1,    0.,   0.,   0.,DIAGONALS|AUXLABELS};

/*
 * every shape has these functions:
 *
 * void		SHAPE_init(GVC_t *gvc)
 *			initialize the shape (usually at least its size).
 * port		SHAPE_port(GVC_t *gvc, char *portname)
 *			return the aiming point and slope (if constrained)
 *			of a port.
 * int		SHAPE_inside(inside_t *inside_context, pointf p, edge_t *e);
 *			test if point is inside the node shape which is
 *			assumed convex.
 *			the point is relative to the node center.  the edge
 *			is passed in case the port affects spline clipping.
 * void		SHAPE_code(GVC_t *gvc)
 *			generate graphics code for a node.
 * int		SHAPE_path(GVC_t *gvc, edge_t *e, int pt, box path[], int *nbox)
 *			create a path for the port of e that touches n,
 *			return side
 *
 * some shapes, polygons in particular, use additional shape control data *
 *
 */

static shape_functions poly_fns = {
	poly_init,
	poly_free,
	poly_port,
	poly_inside,
	poly_path,
	poly_gencode
};
static shape_functions point_fns = {
	point_init,
	poly_free,
	poly_port,
	poly_inside,
	NULL,
	poly_gencode
};
static shape_functions record_fns = {
	record_init,
	record_free,
	record_port,
	record_inside,
	record_path,
	record_gencode
};
static shape_functions epsf_fns = {
	epsf_init,
	epsf_free,
	poly_port,
	epsf_inside,
	NULL,
	epsf_gencode
};

static shape_desc Shapes[]  = {	/* first entry is default for no such shape */
{"box"		,&poly_fns	,&p_box			},
{"polygon"	,&poly_fns	,&p_polygon		},
{"ellipse"	,&poly_fns	,&p_ellipse		},
{"circle"	,&poly_fns	,&p_circle		},
{"point"	,&point_fns	,&p_circle		},
{"egg"		,&poly_fns	,&p_egg			},
{"triangle"	,&poly_fns	,&p_triangle		},
{"plaintext"	,&poly_fns	,&p_plaintext		},
{"diamond"	,&poly_fns	,&p_diamond		},
{"trapezium"	,&poly_fns	,&p_trapezium		},
{"parallelogram",&poly_fns	,&p_parallelogram	},
{"house"	,&poly_fns	,&p_house		},
{"pentagon"	,&poly_fns	,&p_pentagon		},
{"hexagon"	,&poly_fns	,&p_hexagon		},
{"septagon"	,&poly_fns	,&p_septagon		},
{"octagon"	,&poly_fns	,&p_octagon		},
/* redundant and undocumented builtin polygons */
{"rect"		,&poly_fns	,&p_box			},
{"rectangle"	,&poly_fns	,&p_box			},
{"doublecircle"	,&poly_fns	,&p_doublecircle	},
{"doubleoctagon",&poly_fns	,&p_doubleoctagon	},
{"tripleoctagon",&poly_fns	,&p_tripleoctagon	},
{"invtriangle"	,&poly_fns	,&p_invtriangle		},
{"invtrapezium"	,&poly_fns	,&p_invtrapezium	},
{"invhouse"	,&poly_fns	,&p_invhouse		},
{"Mdiamond"	,&poly_fns	,&p_Mdiamond		},
{"Msquare"	,&poly_fns	,&p_Msquare		},
{"Mcircle"	,&poly_fns	,&p_Mcircle		},
/*  *** shapes other than polygons  *** */
{"record"	,&record_fns	,NULL			},
{"Mrecord"	,&record_fns	,NULL			},
{"epsf"		,&epsf_fns	,NULL			},
{NULL		,NULL		,NULL			}
};

double
textwidth (GVC_t *gvc, char *line, char *fontname, double fontsize)
{
	int	iwidth = 0;
	iwidth = (gvrender_textsize(gvc, line, fontname, fontsize).x);
	if (!iwidth)
		iwidth = estimate_textsize(line, fontname,fontsize).x;

	return (double)iwidth;
}

static void
storeline(GVC_t *gvc, textlabel_t *lp, char *line, char terminator,graph_t *g)
{
	double	width = 0.0;

	lp->u.txt.line = ALLOC(lp->u.txt.nlines+2,lp->u.txt.line,textline_t);
	lp->u.txt.line[lp->u.txt.nlines].str = line;
	if (line) {
		width = textwidth (gvc, line, lp->fontname, lp->fontsize);
	}
	lp->u.txt.line[lp->u.txt.nlines].width = width;
	lp->u.txt.line[lp->u.txt.nlines].just = terminator;
	lp->u.txt.nlines++;
	width = PS2INCH(width);
	/* total width = max line width */
	if (lp->dimen.x < width)
		lp->dimen.x = width;
	/* recalculate total height */
	lp->dimen.y = PS2INCH(lp->u.txt.nlines*(int)(lp->fontsize*LINESPACING));
}

/* compiles <str> into a label <lp> and returns its bounding box size.  */
pointf label_size(GVC_t *gvc, char *str, textlabel_t *lp, graph_t *g)
{
	char		c,*p,*line,*lineptr;
	unsigned char byte = 0x00;

	if (*str == '\0') return lp->dimen;

	line = lineptr = NULL;
	p = str;
	line = lineptr = N_GNEW(strlen(p) + 1,char);
	*line = 0;
	while ((c = *p++)) {
		byte = (unsigned int )c;
		if (c & ~0x7f) GD_has_Latin1char(g) = TRUE;
		/* Fix some Double Character error for Big5 (Start) */
		if (0xA1 <= byte && byte <= 0xFE) {		
			*lineptr++ = c;
			c = *p++;
			*lineptr++ = c;
		/* Fix some Double Character error for Big5 (End) */
		}else{
			if (c == '\\') {
				switch (*p) {
					case 'n': case 'l': case 'r':
						*lineptr++ = '\0';
						storeline(gvc, lp,line,*p,g);
						line = lineptr;
						break;
					default:
						*lineptr++ = *p;
				}
				if (*p) p++;
			/* tcldot can enter real linend characters */
			} else if (c == '\n') {
				*lineptr++ = '\0';
				storeline(gvc, lp,line,'n',g);
				line = lineptr;
			}else{
				*lineptr++ = c;
			}
		}
	}

	if (line != lineptr) {
		*lineptr++ = '\0';
		storeline(gvc, lp,line,'n',g);
	}

	return lp->dimen;
}

static void
unrecognized(node_t* n, char* p)
{
	agerr(AGWARN, "node %s, port %s unrecognized\n",n->name,p);
}

static double quant(double val, double q)
{
	int	i;
	i = val / q;
	if (i * q + .00001 < val) i++;
	return i * q;
}

static int same_side(pointf p0, pointf p1, pointf L0, pointf L1)
{
	int s0,s1;
	double a,b,c;

	/* a x + b y = c */
	a = -(L1.y - L0.y);
	b = (L1.x - L0.x);
	c = a * L0.x + b * L0.y;

	s0 = (a*p0.x + b*p0.y - c >= 0);
	s1 = (a*p1.x + b*p1.y - c >= 0);
	return (s0 == s1);
}

static
void pencolor(GVC_t *gvc)
{
	char *color;

	color = late_nnstring(gvc->n,N_color,"");
	if (color[0]) 
		gvrender_set_pencolor(gvc, color);
}

static
char* findFill(GVC_t *gvc)
{
	char *color;
	node_t *n = gvc->n;
	
	color = late_nnstring(n,N_fillcolor,"");
	if (! color[0]) {
		/* for backward compatibilty, default fill is same as pen */
		color = late_nnstring(n,N_color,"");
		if (! color[0]) {
			if (ND_shape(n) == point_desc) {
				color = "black";
			}
			else {
				color = (Output_lang == MIF ? "black" : DEFAULT_FILL);
			}
		}
	}
	return color;
}

static
void fillcolor(GVC_t *gvc)
{
	gvrender_set_fillcolor(gvc, findFill(gvc));
}

static char** 
checkStyle (node_t* n, int* flagp)
{
	char*		style;
    char**		pstyle = 0;
	int			istyle = 0;
	polygon_t*	poly;

	style = late_nnstring(n,N_style,"");
	if (style[0]) {
		int  i;
		pstyle = parse_style(style);
		for (i = 0; pstyle[i]; i++) {
			if (strcmp(pstyle[i],"filled") == 0) {istyle |= FILLED;}
			else if (strcmp(pstyle[i],"rounded") == 0) {istyle |= ROUNDED;}
			else if (strcmp(pstyle[i],"diagonals") == 0) {istyle |= DIAGONALS;}
			else if (strcmp(pstyle[i],"invis") == 0) {istyle |= INVISIBLE;}
		}
	}
	if ((poly = ND_shape(n)->polygon)) istyle |= poly->option;

	*flagp = istyle;
	return pstyle;
}

static
int stylenode(GVC_t *gvc)
{
	char**	pstyle;
	int		istyle;

	if ((pstyle = checkStyle (gvc->n, &istyle)))
		gvrender_set_style(gvc, pstyle);
	return istyle;
}

static void hack1(GVC_t *gvc, char* str, int k)
{
	point		p;
	double		fontsize;
	textline_t	fake;
	node_t *n = gvc->n;

	fontsize = ND_label(n)->fontsize*.8;	/* magic? */

	p.x = ND_coord_i(n).x - (strlen(str) * ND_label(n)->fontsize) / 2;
	p.y = ND_coord_i(n).y + (k * (ND_ht_i(n) - ND_label(n)->fontsize - 2)) / 2;
	gvrender_begin_context(gvc);
	gvrender_set_font(gvc, ND_label(n)->fontname,fontsize);
	fake.str = str;
	fake.width = strlen(str) * fontsize; /* ugh */
	fake.just = 0; /* does this field do anything? SCN */
	gvrender_textline(gvc, p, &fake);
	gvrender_end_context(gvc);
}

static void Mlabel_hack(GVC_t *gvc)
{
	char		*str;
	if ((str = agget(gvc->n,"toplabel"))) hack1(gvc, str, 1);
	if ((str = agget(gvc->n,"bottomlabel"))) hack1(gvc, str, -1);
}

static void Mcircle_hack(GVC_t *gvc)
{
	double	x,y;
	point	A[2],p;
	node_t *n = gvc->n;

	y = .7500;
	x = .6614;	/* x^2 + y^2 = 1.0 */
	p.y = y * ND_ht_i(n) / 2.0;
	p.x = ND_rw_i(n) * x;	/* assume node is symmetric */

	A[0] = add_points(p,ND_coord_i(n));
	A[1].y = A[0].y; A[1].x = A[0].x - 2*p.x;
	gvrender_polyline(gvc, A, 2);
	A[0].y -= 2*p.y; A[1].y = A[0].y;
	gvrender_polyline(gvc, A, 2);
}

static point interpolate(double t, point p0, point p1)
{
	point	rv;
	rv.x = p0.x + t * (p1.x - p0.x);
	rv.y = p0.y + t * (p1.y - p0.y);
	return rv;
}

static void round_corners(GVC_t *gvc, point *A, int n, int style)
{
	point	*B,C[2],p0,p1;
	double	d,dx,dy,t;
	int		i,seg,mode;

	if (style & DIAGONALS) mode = DIAGONALS;
	else mode = ROUNDED;
	B = N_NEW(4*n+4,point);
	i = 0;
	for (seg = 0; seg < n; seg++) {
		p0 = A[seg];
		if (seg < n - 1) p1 = A[seg+1];
		else p1 = A[0];
		dx = p1.x - p0.x;
		dy = p1.y - p0.y;
		d = sqrt(dx*dx + dy*dy);
		/*t = ((mode == ROUNDED)? RBCONST / d : .5);*/
		t = RBCONST / d;
		if (mode != ROUNDED) B[i++] = p0;
		if (mode == ROUNDED) B[i++] = interpolate(RBCURVE*t,p0,p1);
		B[i++] = interpolate(t,p0,p1);
		B[i++] = interpolate(1.0 - t,p0,p1);
		if (mode == ROUNDED) B[i++] = interpolate(1.0 - RBCURVE*t,p0,p1);
	}
	B[i++] = B[0];
	B[i++] = B[1];
	B[i++] = B[2];

	if (mode == ROUNDED) {
		for (seg = 0; seg < n; seg++) {
			gvrender_polyline(gvc, B + 4*seg+1, 2);
			gvrender_beziercurve(gvc, B + 4*seg+2, 4, FALSE, FALSE);
		}
	}
	else {	/* diagonals are weird.  rewrite someday. */
		pencolor(gvc);
		if (style & FILLED) fillcolor(gvc); /* emit fill color */
		gvrender_polygon(gvc, A, n, style&FILLED);
		for (seg = 0; seg < n; seg++) {
#ifdef NOTDEF
			C[0] = B[3 * seg]; C[1] = B[3 * seg + 3];
			gvrender_polyline(gvc, C, 2);
#endif
			C[0] = B[3 * seg + 2]; C[1] = B[3 * seg + 4];
			gvrender_polyline(gvc, C, 2);
		}
	}
	free(B);
}

/*=============================poly start=========================*/

void poly_init(GVC_t *gvc)
{
	pointf	dimen;
	pointf	P,Q,R;
	pointf	*vertices;
	double  temp,alpha,beta,gamma,delta,xb,yb;
	double	orientation,distortion,skew;
	double	sectorangle, sidelength, skewdist, gdistortion, gskew;
	double	angle, sinx, cosx, xmax, ymax, scalex, scaley;
	int     regular,peripheries,sides;
	int	i,j,outp;
	polygon_t *poly=NEW(polygon_t);
	node_t *n = gvc->n;

	regular = ND_shape(n)->polygon->regular;
	peripheries = ND_shape(n)->polygon->peripheries;
	sides = ND_shape(n)->polygon->sides;
	orientation = ND_shape(n)->polygon->orientation;
	skew = ND_shape(n)->polygon->skew;
	distortion = ND_shape(n)->polygon->distortion;

	regular |= mapbool(agget(n,"regular"));
	peripheries = late_int(n,N_peripheries,peripheries,0);
	orientation += late_double(n,N_orientation,0.0,-360.0);
	if (sides==0) { /* not for builtins */
		skew = late_double(n,N_skew,0.0,-100.0);
		sides = late_int(n,N_sides,4,0);
		distortion = late_double(n,N_distortion,0.0,-100.0);
	}

	/* get label dimensions */
	dimen = ND_label(n)->dimen;

	/* minimal whitespace around label */
	dimen.x += 4*GAP;
	dimen.y += 2*GAP;

	if (mapbool(late_string(n,N_fixed,"false"))) {
		if ((ND_width(n) < dimen.x) || (ND_height(n) < dimen.y))
			agerr(AGWARN, "node '%s' size too small for label\n",
				n->name);
		dimen.x = dimen.y = 0;
	}
	else {	
		if (ND_shape(n)->usershape) {
			point	imagesize;
			char*   sfile = agget(n,"shapefile");

			imagesize = gvrender_usershapesize(gvc, n,sfile);
            if ((imagesize.x == 0) && (imagesize.y == 0)) {
				agerr(AGERR, "No or improper shapefile=\"%s\" for user-defined shape=\"%s\" on node \"%s\"\n", 
					(sfile ? sfile : "<nil>"), agget(n,"shape"), n->name);
 			}
			dimen.x = MAX(dimen.x,PS2INCH(imagesize.x));
			dimen.y = MAX(dimen.y,PS2INCH(imagesize.y));
		}
	}

	/* quantization */
	if ((temp = GD_drawing(n->graph)->quantum) > 0.0) {
		dimen.x = quant(dimen.x,temp);
		dimen.y = quant(dimen.y,temp);
	}

	/* make square if necessary */
	if (regular) {
		/* make x and y dimensions equal */
		ND_width(n) = ND_height(n) = MIN(ND_width(n),ND_height(n));
		xb = yb = MAX(dimen.x,dimen.y);
	} else {
		xb = dimen.x; yb = dimen.y;
	}


	/* I don't know how to distort or skew ellipses in postscript */
	/* Convert request to a polygon with a large number of sides */
	if ((sides<=2) && ((distortion!=0.) || (skew!=0.))) {
		sides = 120;
	}

	/* adjust bounding box so that label fits in inner ellipse */
	/* this will change the symmetry of the bounding box */
	/* adjust for inner to outer diameter of polygon */
	if (sides>2) { /* except ellipses */
		temp = cos(PI/sides);
		xb /= temp; yb /= temp;
	}

	if (  (sides!=4)
	   || ((ROUND(orientation)%90)!=0)
	   || (distortion!=0.)
	   || (skew!=0.) ) 
	{
		if (yb>xb) temp = xb * (sqrt(2.) - 1.);
		else temp = yb * (sqrt(2.) - 1.);
		xb += temp; yb += temp;
	}
	xb=MAX(ND_width(n),xb); yb=MAX(ND_height(n),yb);
	outp=peripheries;
	if (peripheries<1) outp=1;
	if (sides<3) { /* ellipses */
		sides=1;
		vertices=N_NEW(outp,pointf);
		P.x=xb/2.; P.y=yb/2.;
		vertices[0] = P;
		if (peripheries>1) {
			for (j=1; j<peripheries; j++) {
				P.x += GAP; P.y += GAP;
				vertices[j] = P;
			}
			xb=2.*P.x; yb=2.*P.y;
		}
	} else {
		vertices=N_NEW(outp*sides,pointf);
		sectorangle = 2.*PI/sides;
		sidelength = sin(sectorangle/2.);
		skewdist = hypot(fabs(distortion)+fabs(skew),1.);
		gdistortion = distortion*sqrt(2.)/cos(sectorangle/2.);
		gskew = skew/2.;
		angle = (sectorangle-PI)/2.;
		sincos(angle,&sinx,&cosx);
		R.x = .5*cosx; R.y = .5*sinx;
		xmax=ymax=0.;
		angle += (PI-sectorangle)/2.;
		for (i=0; i<sides; i++) {
	
			/*next regular vertex*/
			angle += sectorangle;
			sincos(angle,&sinx,&cosx);
			R.x += sidelength*cosx; R.y += sidelength*sinx;
	
			/*distort and skew*/
			P.x = R.x*(skewdist+R.y*gdistortion)+R.y*gskew;
			P.y = R.y;
	
			/*orient P.x,P.y*/
			alpha = RADIANS(orientation)+atan2(P.y,P.x);
			sincos(alpha,&sinx,&cosx);
			P.x = P.y = hypot(P.x,P.y);
			P.x *= cosx; P.y *= sinx;

			/*scale for label*/
			P.x *= xb; P.y *= yb;
	
			/*find max for bounding box*/
			xmax = MAX(fabs(P.x),xmax); ymax = MAX(fabs(P.y),ymax);
	
			/* store result in array of points */
			vertices[i] = P;
		}

		/* apply minimum dimensions */
		xmax *=2.; ymax *=2.;
		xb=MAX(ND_width(n),xmax); yb=MAX(ND_height(n),ymax);
		scalex=xb/xmax; scaley=yb/ymax;
	
		for (i=0; i<sides; i++) {
			P = vertices[i];
			P.x *= scalex; P.y *= scaley;
			vertices[i] = P;
		}
	
		if (peripheries>1) {
			Q = vertices[(sides-1)];
			R = vertices[0];
			beta = atan2(R.y-Q.y,R.x-Q.x);
			for (i=0; i<sides; i++) {

				/*for each vertex find the bisector*/
				P = Q; Q = R; R = vertices[(i+1)%sides];
				alpha = beta; beta = atan2(R.y-Q.y,R.x-Q.x);
				gamma = (alpha+PI-beta)/2.;

				/*find distance along bisector to*/
				/*intersection of next periphery*/
				temp = GAP/sin(gamma);

				/*convert this distance to x and y*/
				delta = alpha-gamma;
				sincos(delta,&sinx,&cosx);
				sinx *= temp; cosx *= temp;

				/*save the vertices of all the*/
				/*peripheries at this base vertex*/
				for (j=1; j<peripheries; j++) {
					Q.x += cosx; Q.y += sinx;
					vertices[i+j*sides] = Q;
				}
			}
			for (i=0; i<sides; i++) {
				P = vertices[i+(peripheries-1)*sides];
				xb = MAX(2.*fabs(P.x),xb);
				yb = MAX(2.*fabs(P.y),yb);
			}
		}
	}
	poly->regular = regular;
	poly->peripheries = peripheries;
	poly->sides = sides;
	poly->orientation = orientation;
	poly->skew = skew;
	poly->distortion = distortion;
	poly->vertices = vertices;

	ND_width(n) = xb;
	ND_height(n) = yb;
	ND_shape_info(n) = (void*) poly;
}

static void poly_free(GVC_t *gvc)
{
	polygon_t* p = ND_shape_info(gvc->n);

	if (p) {
		free(p->vertices);
		free(p);
	}
}

#define GET_PORT_BOX(n,e) ((n) == (e)->head ? ED_head_port(e).bp : ED_tail_port(e).bp)

static boolean poly_inside(inside_t *inside_context, pointf p)
{
	static node_t*	lastn;   /* last node argument */
	static edge_t*	laste;   /* last edge argument */
	static node_t*	datan;   /* node used for cached data */
	static polygon_t *poly;
	static int	last,outp,sides;
	static pointf	O;
	static pointf	*vertex;
	static double	xsize,ysize,scalex,scaley,box_URx,box_URy;
	static box*		bp;

	int			i,i1,j,s;
	pointf		P,Q,R;
	edge_t*		f;
	edge_t*	e = inside_context->e;
	node_t* n = inside_context->n;

	P = (GD_left_to_right(n->graph)? flip_ptf(p) : p);
	for (f = e; ED_edge_type(f) != NORMAL; f = ED_to_orig(f));
	e = f;

	if ((n != lastn) || (e != laste)) {
		bp = GET_PORT_BOX (n, e);

		if ((bp == NULL) && (n != datan)) {
			datan = n;
			poly = (polygon_t*) ND_shape_info(n);
			vertex = poly->vertices;
			sides = poly->sides;
	
			/* get point and node size adjusted for rankdir=LR */
			if (GD_left_to_right(n->graph)) {
				ysize = ND_lw_i(n) + ND_rw_i(n); xsize = ND_ht_i(n);
			}
			else {
				xsize = ND_lw_i(n) + ND_rw_i(n); ysize = ND_ht_i(n);
			}
	
	        	/* scale */
			if (xsize == 0.0) xsize = 1.0;
			if (ysize == 0.0) ysize = 1.0;
			scalex = ND_width(n)/xsize; scaley = ND_height(n)/ysize;
			box_URx = ND_width(n)/2.0; box_URy = ND_height(n)/2.0;
	
			/* index to outer-periphery */
			outp=(poly->peripheries-1)*sides;
			if (outp<0) outp=0;
		}
		lastn = n;
		laste = e;
	}

		/* Quick test if port rectangle is target */
	if (bp) {
		box bbox = *bp;
		return INSIDE(P,bbox);
	}

        /* scale */
	P.x *= scalex; P.y *= scaley;

	/* inside bounding box? */
	if ((fabs(P.x)>box_URx) || (fabs(P.y)>box_URy)) return FALSE;

        /* ellipses */
	if (sides<=2) return (hypot(P.x/box_URx,P.y/box_URy)<1.);

	/* use fast test in case we are converging on a segment */
	i = last % sides; /*in case last left over from larger polygon*/
	i1 = (i + 1) % sides;
	Q = vertex[i+outp]; R = vertex[i1+outp];
	if ( !(same_side(P,O,Q,R))) return FALSE;
	if (  (s=same_side(P,Q,R,O)) && (same_side(P,R,O,Q))) return TRUE;
	for (j = 1; j < sides; j++) {
		if (s) {
			i = i1; i1 = (i + 1) % sides;
		} else {
			i1 = i; i = (i + sides - 1) % sides;
		} 
		if ( !(same_side(P,O,vertex[i+outp],vertex[i1+outp]))) {
			last = i;
			return FALSE;
		}
	}
	last = i;  /* in case next edge is to same side */
	return TRUE;
}

static int   poly_path(GVC_t *gvc, int pt, box rv[], int* kptr)
{
	int  side = 0;
	edge_t *f;
	node_t *n = gvc->n;
	edge_t *e = gvc->e;

	if (ND_label(n)->html && ND_has_port(n)) {
		for (f = e; ED_edge_type(f) != NORMAL; f = ED_to_orig(f));
		e = f;
		if (GET_PORT_BOX(n,e)) side = html_path (n, e, pt, rv, kptr);
	}
	return side;
}

static port poly_port(GVC_t *gvc, char* portname)
{
	static char *points_of_compass[] =
		{"n","ne","e","se","s","sw","w","nw",NULL};
static struct {signed char x,y;} a[] = {{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};

	int		i,ht,wd;
	port	rv;
	char	*p;
	node_t *n = gvc->n;

	if (portname[0] != ':') return Center;		/*could be '\000' */
	portname++; /* skip over delim */

	if (ND_label(n)->html) {
		if (html_port (n, portname, &rv)) return rv;
	}

	for (i = 0; (p = points_of_compass[i]); i++)
		if (streq(p,portname)) break;

	if (p == NULL) {
		unrecognized(n,portname);
		rv = Center;
	}
	else {
		ht = ND_ht_i(n) / 2; 
		wd = ND_lw_i(n);
		rv.p.x = a[i].x * wd;
		rv.p.y = a[i].y * ht;
		rv.bp = 0;
		rv.order = (MC_SCALE * (ND_lw_i(n) + rv.p.x)) / (ND_lw_i(n) + ND_rw_i(n));
		rv.constrained = FALSE;
		rv.defined = TRUE;
	}
	return rv;
}

/* generic polygon gencode routine */
static void poly_gencode(GVC_t *gvc)
{
	polygon_t		*poly;
	double			xsize, ysize;
	int			i,j,peripheries,sides,style;
	pointf			P,*vertices;
	static point	*A;
	static int		A_size;
	int			filled;
	node_t *n = gvc->n;
	
	poly = (polygon_t*) ND_shape_info(n);
	vertices = poly->vertices;
	sides = poly->sides;
	peripheries = poly->peripheries;
	if (A_size < sides) {A_size = sides + 5; A = ALLOC(A_size,A,point);}

	ND_label(n)->p = ND_coord_i(n);
/* prescale by 16.0 to help rounding trick below */
	xsize = ((ND_lw_i(n) + ND_rw_i(n)) / ND_width(n)) * 16.0;
	ysize = ((ND_ht_i(n)) / ND_height(n)) * 16.0;

	/* this is bad, but it's because of how the VRML driver works */
#ifdef HAVE_LIBPNG
	if ((gvc->codegen == &VRML_CodeGen) && (peripheries == 0)) {
		peripheries = 1;
	}
#endif

	if (ND_shape(n) == point_desc) {
		checkStyle (n, &style);
		if (style & INVISIBLE) gvrender_set_style(gvc, point_style);
		else gvrender_set_style(gvc, &point_style[1]);
		style = FILLED;
	}
	else {
		style = stylenode(gvc); 
	}
	if (style & FILLED) {
		fillcolor(gvc); /* emit fill color */
		filled = 1;
	}
	else {
		filled = 0;
	}
		/* if no boundary but filled, set boundary color to fill color */
	if ((peripheries == 0) && filled) {
		char*   color;
		peripheries = 1;
		color = findFill(gvc);
    		if (color[0])
			gvrender_set_pencolor(gvc, color);
	}
	else pencolor(gvc); /* emit pen color */

	if (ND_shape(n)->usershape) {
		for (i = 0; i < sides; i++) {
			P = vertices[i];
/* simple rounding produces random results around .5 
 * this trick should clip off the random part. 
 * (note xsize/ysize prescaled by 16.0 above) */
			A[i].x = ROUND(P.x * xsize) / 16;
			A[i].y = ROUND(P.y * ysize) / 16;
			if (sides > 2) {
				A[i].x += ND_coord_i(n).x;
				A[i].y += ND_coord_i(n).y;
			}
		}
		gvrender_user_shape(gvc,ND_shape(n)->name,A,sides,filled);
		filled = 0;
	}
	for (j = 0; j < peripheries; j++) {
		for (i = 0; i < sides; i++) {
			P = vertices[i+j*sides];
/* simple rounding produces random results around .5 
 * this trick should clip off the random part. 
 * (note xsize/ysize prescaled by 16.0 above) */
			A[i].x = ROUND(P.x * xsize) / 16;
			A[i].y = ROUND(P.y * ysize) / 16;
			if (sides > 2) {
				A[i].x += ND_coord_i(n).x;
				A[i].y += ND_coord_i(n).y;
			}
		}
		if (sides <= 2) {
			gvrender_ellipse(gvc, ND_coord_i(n), A[0].x, A[0].y, filled);
			if (style & DIAGONALS) {
				Mcircle_hack(gvc);
			}
		}
		else if (style & (ROUNDED | DIAGONALS)) {
			round_corners(gvc, A, sides, style);
		}
		else {
			gvrender_polygon(gvc, A, sides, filled);
		}
		/* fill innermost periphery only */
		filled = 0;
	}

	if (style & AUXLABELS) Mlabel_hack(gvc);
	emit_label(gvc, ND_label(n));
}

/*=======================end poly======================================*/

/*=======================start shape======================================*/

#ifdef NOT_READY_YET
static void poly_shape_init(GVC_t *gvc)
{
	pointf	dimen;
	pointf	P,Q,R;
	pointf	*vertices;
	double  temp,alpha,beta,gamma,delta,xb,yb;
	double	orientation,distortion,skew;
	double	sectorangle, sidelength, skewdist, gdistortion, gskew;
	double	angle, sinx, cosx, xmax, ymax, scalex, scaley;
	int     regular,peripheries,sides;
	int	i,j,outp;
	polygon_t *poly=NEW(polygon_t);
	node_t *n = gvc->n;

	regular = ND_shape(n)->polygon->regular;
	peripheries = ND_shape(n)->polygon->peripheries;
	sides = ND_shape(n)->polygon->sides;
	orientation = ND_shape(n)->polygon->orientation;
	skew = ND_shape(n)->polygon->skew;
	distortion = ND_shape(n)->polygon->distortion;

	regular |= mapbool(agget(n,"regular"));
	peripheries = late_int(n,N_peripheries,peripheries,0);
	orientation += late_double(n,N_orientation,0.0,-360.0);
	if (sides==0) { /* not for builtins */
		skew = late_double(n,N_skew,0.0,-100.0);
		sides = late_int(n,N_sides,4,0);
		distortion = late_double(n,N_distortion,0.0,-100.0);
	}

	/* get label dimensions */
	dimen = ND_label(n)->dimen;

	if (mapbool(late_string(n,N_fixed,"false"))) {
		if ((ND_width(n) < dimen.x) || (ND_height(n) < dimen.y))
			agerr(AGWARN, "node '%s' size too small for label\n",
				n->name);
		dimen.x = dimen.y = 0;
	}
	else {	
		if (ND_shape(n)->usershape) {
			point	imagesize;
			char*   sfile = agget(n,"shapefile");

			imagesize = gvrender_usershapesize(n,sfile);
            if ((imagesize.x == 0) && (imagesize.y == 0)) {
				agerr(AGERR, "No or improper shapefile=\"%s\" for user-defined shape=\"%s\" on node \"%s\"\n", 
					(sfile ? sfile : "<nil>"), agget(n,"shape"), n->name);
 			}
			dimen.x = MAX(dimen.x,PS2INCH(imagesize.x));
			dimen.y = MAX(dimen.y,PS2INCH(imagesize.y));
		}
	}

	/* quantization */
	if ((temp = GD_drawing(n->graph)->quantum) > 0.0) {
		dimen.x = quant(dimen.x,temp);
		dimen.y = quant(dimen.y,temp);
	}

	/* make square if necessary */
	if (regular) {
		/* make x and y dimensions equal */
		ND_width(n) = ND_height(n) = MIN(ND_width(n),ND_height(n));
		xb = yb = MAX(dimen.x,dimen.y);
	} else {
		xb = dimen.x; yb = dimen.y;
	}


	/* I don't know how to distort or skew ellipses in postscript */
	/* Convert request to a polygon with a large number of sides */
	if ((sides<=2) && ((distortion!=0.) || (skew!=0.))) {
		sides = 120;
	}

	/* adjust bounding box so that label fits in inner ellipse */
	/* this will change the symmetry of the bounding box */
	/* adjust for inner to outer diameter of polygon */
	if (sides>2) { /* except ellipses */
		temp = cos(PI/sides);
		xb /= temp; yb /= temp;
	}

	if (  (sides!=4)
	   || ((ROUND(orientation)%90)!=0)
	   || (distortion!=0.)
	   || (skew!=0.) ) {
		if (yb>xb) temp = xb * (sqrt(2.) - 1.);
		else temp = yb * (sqrt(2.) - 1.);
		xb += temp; yb += temp;
        }
	xb=MAX(ND_width(n),xb); yb=MAX(ND_height(n),yb);
	outp=peripheries;
	if (peripheries<1) outp=1;
	if (sides<3) { /* ellipses */
		sides=1;
		vertices=N_NEW(outp,pointf);
		P.x=xb/2.; P.y=yb/2.;
		vertices[0] = P;
		if (peripheries>1) {
			for (j=1; j<peripheries; j++) {
				P.x += GAP; P.y += GAP;
				vertices[j] = P;
			}
			xb=2.*P.x; yb=2.*P.y;
		}
	} else {
		vertices=N_NEW(outp*sides,pointf);
		sectorangle = 2.*PI/sides;
		sidelength = sin(sectorangle/2.);
		skewdist = hypot(fabs(distortion)+fabs(skew),1.);
		gdistortion = distortion*sqrt(2.)/cos(sectorangle/2.);
		gskew = skew/2.;
		angle = (sectorangle-PI)/2.;
		sincos(angle,&sinx,&cosx);
		R.x = .5*cosx; R.y = .5*sinx;
		xmax=ymax=0.;
		angle += (PI-sectorangle)/2.;
		for (i=0; i<sides; i++) {
	
			/*next regular vertex*/
			angle += sectorangle;
			sincos(angle,&sinx,&cosx);
			R.x += sidelength*cosx; R.y += sidelength*sinx;
	
			/*distort and skew*/
			P.x = R.x*(skewdist+R.y*gdistortion)+R.y*gskew;
			P.y = R.y;
	
			/*orient P.x,P.y*/
			alpha = RADIANS(orientation)+atan2(P.y,P.x);
			sincos(alpha,&sinx,&cosx);
			P.x = P.y = hypot(P.x,P.y);
			P.x *= cosx; P.y *= sinx;

			/*scale for label*/
			P.x *= xb; P.y *= yb;
	
			/*find max for bounding box*/
			xmax = MAX(fabs(P.x),xmax); ymax = MAX(fabs(P.y),ymax);
	
			/* store result in array of points */
			vertices[i] = P;
		}

		/* apply minimum dimensions */
		xmax *=2.; ymax *=2.;
		xb=MAX(ND_width(n),xmax); yb=MAX(ND_height(n),ymax);
		scalex=xb/xmax; scaley=yb/ymax;
	
		for (i=0; i<sides; i++) {
			P = vertices[i];
			P.x *= scalex; P.y *= scaley;
			vertices[i] = P;
		}
	
		if (peripheries>1) {
			Q = vertices[(sides-1)];
			R = vertices[0];
			beta = atan2(R.y-Q.y,R.x-Q.x);
			for (i=0; i<sides; i++) {

				/*for each vertex find the bisector*/
				P = Q; Q = R; R = vertices[(i+1)%sides];
				alpha = beta; beta = atan2(R.y-Q.y,R.x-Q.x);
				gamma = (alpha+PI-beta)/2.;

				/*find distance along bisector to*/
				/*intersection of next periphery*/
				temp = GAP/sin(gamma);

				/*convert this distance to x and y*/
				delta = alpha-gamma;
				sincos(delta,&sinx,&cosx);
				sinx *= temp; cosx *= temp;

				/*save the vertices of all the*/
				/*peripheries at this base vertex*/
				for (j=1; j<peripheries; j++) {
					Q.x += cosx; Q.y += sinx;
					vertices[i+j*sides] = Q;
				}
			}
			for (i=0; i<sides; i++) {
				P = vertices[i+(peripheries-1)*sides];
				xb = MAX(2.*fabs(P.x),xb);
				yb = MAX(2.*fabs(P.y),yb);
			}
		}
	}
	poly->regular = regular;
	poly->peripheries = peripheries;
	poly->sides = sides;
	poly->orientation = orientation;
	poly->skew = skew;
	poly->distortion = distortion;
	poly->vertices = vertices;

	ND_width(n) = xb;
	ND_height(n) = yb;
	ND_shape_info(n) = (void*) poly;
}

static void shape_free(GVC_t *gvc)
{
	polygon_t* p = ND_shape_info(gvc->n);

	if (p) {
		free(p->vertices);
		free(p);
	}
}

static boolean shape_inside(inside_t *inside_context, pointf p);
{
	static polygon_t *poly;
	static int	last,outp,sides;
	static node_t	*lastn;
	static pointf	O;
	static pointf	*vertex;
	static double	xsize,ysize,scalex,scaley,box_URx,box_URy;

	int		i,i1,j,s;
	pointf		P,Q,R;
	edge_t	*e = inside_context->e;
	node_t	*n = inside_context->n;

	P = (GD_left_to_right(n->graph)? flip_ptf(p) : p);
	if (n != lastn) {
		poly = (polygon_t*) ND_shape_info(n);
		vertex = poly->vertices;
		sides = poly->sides;
		lastn = n;

		/* get point and node size adjusted for rankdir=LR */
		if (GD_left_to_right(n->graph)) {
			ysize = ND_lw_i(n) + ND_rw_i(n); xsize = ND_ht_i(n);
		}
		else {
			xsize = ND_lw_i(n) + ND_rw_i(n); ysize = ND_ht_i(n);
		}

        	/* scale */
		if (xsize == 0.0) xsize = 1.0;
		if (ysize == 0.0) ysize = 1.0;
		scalex = ND_width(n)/xsize; scaley = ND_height(n)/ysize;
		box_URx = ND_width(n)/2.0; box_URy = ND_height(n)/2.0;

		/* index to outer-periphery */
		outp=(poly->peripheries-1)*sides;
		if (outp<0) outp=0;
	}

        /* scale */
	P.x *= scalex; P.y *= scaley;

	/* inside bounding box? */
	if ((fabs(P.x)>box_URx) || (fabs(P.y)>box_URy)) return FALSE;

        /* ellipses */
	if (sides<=2) return (hypot(P.x/box_URx,P.y/box_URy)<1.);

	/* use fast test in case we are converging on a segment */
	i = last % sides; /*in case last left over from larger polygon*/
	i1 = (i + 1) % sides;
	Q = vertex[i+outp]; R = vertex[i1+outp];
	if ( !(same_side(P,O,Q,R))) return FALSE;
	if (  (s=same_side(P,Q,R,O)) && (same_side(P,R,O,Q))) return TRUE;
	for (j = 1; j < sides; j++) {
		if (s) {
			i = i1; i1 = (i + 1) % sides;
		} else {
			i1 = i; i = (i + sides - 1) % sides;
		} 
		if ( !(same_side(P,O,vertex[i+outp],vertex[i1+outp]))) {
			last = i;
			return FALSE;
		}
	}
	last = i;  /* in case next edge is to same side */
	return TRUE;
}

static port shape_port(GVC_t *gvc, char *portname)
{
	static char *points_of_compass[] =
		{"n","ne","e","se","s","sw","w","nw",NULL};
static struct {signed char x,y;} a[] = {{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};

	int		i,ht,wd;
	port	rv;
	char	*p;
	node_t *n = gvc->n;


	if (*portname) portname++; /* skip over delim */
	for (i = 0; (p = points_of_compass[i]); i++)
		if (streq(p,portname)) break;

	if (p == NULL) {
		if (portname[0]) unrecognized(n,portname);
		rv = Center;
	}
	else {
		ht = ND_ht_i(n) / 2; 
		wd = ND_lw_i(n);
		rv.p.x = a[i].x * wd;
		rv.p.y = a[i].y * ht;
		rv.order = (MC_SCALE * (ND_lw_i(n) + rv.p.x)) / (ND_lw_i(n) + ND_rw_i(n));
		rv.constrained = FALSE;
		rv.defined = TRUE;
	}
	return rv;
}

/* generic shape gencode routine */
static void shape_gencode(GVC_t *gvc)
{
	polygon_t		*poly;
	double			xsize, ysize;
	int			i,j,peripheries,sides,style;
	pointf			P,*vertices;
	static point	*A;
	static int		A_size;
	int			filled;
	node_t *n = gvc->n;
	
	poly = (polygon_t*) ND_shape_info(n);
	vertices = poly->vertices;
	sides = poly->sides;
	peripheries = poly->peripheries;
	if (A_size < sides) {A_size = sides + 5; A = ALLOC(A_size,A,point);}

/* prescale by 16.0 to help rounding trick below */
	xsize = ((ND_lw_i(n) + ND_rw_i(n)) / ND_width(n)) * 16.0;
	ysize = ((ND_ht_i(n)) / ND_height(n)) * 16.0;

	/* this is bad, but it's because of how the VRML driver works */
#ifdef HAVE_LIBPNG
	if ((gvc->codegen == &VRML_CodeGen) && (peripheries == 0)) {
		peripheries = 1;
	}
#endif

	if (ND_shape(n) == point_desc) {
		checkStyle (n, &style);
		if (style & INVISIBLE) gvrender_set_style(point_style);
		else gvrender_set_style(&point_style[1]);
		style = FILLED;
    }
	else style = stylenode(gvc); 
	pencolor(gvc); /* emit pen color */
	if (style & FILLED) fillcolor(gvc); /* emit fill color */
	for (j = 0; j < peripheries; j++) {
		for (i = 0; i < sides; i++) {
			P = vertices[i+j*sides];
/* simple rounding produces random results around .5 
 * this trick should clip off the random part. 
 * (note xsize/ysize prescaled by 16.0 above) */
			A[i].x = ROUND(P.x * xsize) / 16;
			A[i].y = ROUND(P.y * ysize) / 16;
			if (sides > 2) {A[i].x += ND_coord_i(n).x; A[i].y += ND_coord_i(n).y;}
		}
		if (!j && (style & FILLED)) {
			/* fill innermost periphery only */
			filled = 1;
		}
		else {
			filled = 0;
		}
		if (ND_shape(n)->usershape) {
			gvrender_user_shape(ND_shape(n)->name,A,sides,filled);
		}
		else if (sides <= 2) {
			gvrender_ellipse(ND_coord_i(n),A[0].x,A[0].y,filled);
			if (style & DIAGONALS) {
				Mcircle_hack(n);
			}
		}
		else if (style & (ROUNDED | DIAGONALS)) {
			round_corners(gvc, n, A, sides, style);
		}
		else {
			gvrender_polygon(A,sides,filled);
		}
	}

	if (style & AUXLABELS) Mlabel_hack(n);
    	ND_label(n)->p = ND_coord_i(n);
	emit_label(gvc,ND_label(n),n->graph);
}
#endif

/*=======================end shape======================================*/

/*===============================point start========================*/

static textlabel_t ptlabel;

/* point_init:
 * shorthand for shape=circle, style=filled, width=0.05, label=""
 */
void point_init(GVC_t *gvc)
{
	textlabel_t* p;
	node_t *n = gvc->n;

	if (!point_desc) {
		shape_desc	*ptr;
		for (ptr = Shapes; ptr->name; ptr++)
			if (!strcmp(ptr->name,"point")) {point_desc = ptr; break;}
		assert(point_desc);
	}

		/* adjust label to "" */
	p = ND_label(n);
	free_label(p);
	ND_label(n) = &ptlabel;
      
		/* set width and height, and make them equal
		 * if user has set weight or height, use it.
		 * if both are set, use smallest.
		 * if neither, use default
		 */
    if (ATTR_SET(N_width,n)) {
    	if (ATTR_SET(N_height,n)) {
			ND_width(n) = ND_height(n) = MIN(ND_width(n),ND_height(n));
    	}
		else ND_height(n) = ND_width(n);
    }
    else if (ATTR_SET(N_height,n)) {
		ND_width(n) = ND_height(n);
    }
	else ND_width(n) = ND_height(n) = DEF_POINT;

	poly_init (gvc);
}


/* the "record" shape is a rudimentary table formatter */

#define HASTEXT 1
#define HASPORT 2
#define HASTABLE 4
#define INTEXT 8
#define INPORT 16

#define ISCTRL(c) ((c) == '{' || (c) == '}' || (c) == '|' || (c) == '<' || (c) == '>')

static char *reclblp;

static field_t	*
parse_reclbl(GVC_t *gvc, node_t *n, int LR, int flag, char* text)
{
	field_t	*fp, *rv = NEW(field_t);
	char *tsp, *psp, *hstsp, *hspsp, *sp;
	char port[SMALLBUF];
	int maxf, cnt, mode, wflag, ishardspace, fi;

	fp = NULL;
	for (maxf = 1, cnt = 0, sp = reclblp; *sp; sp++) {
		if (*sp == '\\') {
			sp++;
			if (*sp && (*sp == '{' || *sp == '}' || *sp == '|'))
				continue;
		}
		if (*sp == '{')
			cnt++;
		else if (*sp == '}')
			cnt--;
		else if (*sp == '|' && cnt == 0)
			maxf++;
		if (cnt < 0)
			break;
	}
	/*maxf = strccnt(reclblp, '|') + 1;*/
	rv->fld = N_NEW (maxf, field_t*);
	rv->LR = LR;
	mode = 0;
	fi = 0;
	hstsp = tsp = text, hspsp = psp = &port[0];
	wflag = TRUE;
	ishardspace = FALSE;
	while (wflag) {
		switch (*reclblp) {
		case '<':
			if (mode & (HASTABLE | HASPORT))
				return NULL;
			mode |= (HASPORT | INPORT);
			reclblp++;
			break;
		case '>':
			if (!(mode & INPORT))
				return NULL;
			mode &= ~INPORT;
			reclblp++;
			break;
		case '{':
			reclblp++;
			if (mode != 0 || !*reclblp)
				return NULL;
			mode = HASTABLE;
			if (!(rv->fld[fi++] = parse_reclbl (gvc, n, NOT (LR) , FALSE, text)))
				return NULL;
			break;
		case '}':
		case '|':
		case '\000':
			if ((!*reclblp && !flag) || (mode & INPORT))
				return NULL;
			if (!(mode & HASTABLE))
				fp = rv->fld[fi++] = NEW (field_t);
			if (mode & HASPORT) {
				if (psp > &port[0] + 1 &&
						psp - 1 != hspsp &&
						*(psp - 1) == ' ')
					psp--;
				*psp = '\000';
				fp->id = strdup (&port[0]);
				hspsp = psp = &port[0];
			}
			if (!(mode & (HASTEXT | HASTABLE)))
				mode |= HASTEXT, *tsp++ = ' ';
			if (mode & HASTEXT) {
				if (tsp > text + 1 &&
						tsp - 1 != hstsp &&
						*(tsp - 1) == ' ')
					tsp--;
				*tsp = '\000';
				fp->lp = make_label (gvc, 0,strdup (text), ND_label(n)->fontsize, ND_label(n)->fontname, ND_label(n)->fontcolor,n->graph);
				fp->LR = TRUE;
				hstsp = tsp = text;
			}
			if (*reclblp) {
				if (*reclblp == '}') {
					reclblp++;
					rv->n_flds = fi;
					return rv;
				}
				mode = 0;
				reclblp++;
			} else
				wflag = FALSE;
			break;
		case '\\':
			if (*(reclblp + 1)) {
				if (ISCTRL (*(reclblp + 1)))
					reclblp++;
				else if (*(reclblp + 1) == ' ')
					ishardspace = TRUE, reclblp++;
			}
			/* falling through ... */
		default:
			if ((mode & HASTABLE) && *reclblp != ' ')
				return NULL;
			if (!(mode & (INTEXT | INPORT)) && *reclblp != ' ')
				mode |= (INTEXT | HASTEXT);
			if (mode & INTEXT) {
				if (!(*reclblp == ' ' && !ishardspace &&
						*(tsp - 1) == ' '))
					*tsp++ = *reclblp;
				if (ishardspace)
					hstsp = tsp - 1;
			} else if (mode & INPORT) {
				if (!(*reclblp == ' ' && !ishardspace &&
						(psp == &port[0] ||
						*(psp - 1) == ' ')))
					*psp++ = *reclblp;
				if (ishardspace)
					hspsp = psp - 1;
			}
			reclblp++;
			while (*reclblp & 128)
				reclblp++;
			break;
		}
	}
	rv->n_flds = fi;
	return rv;
}

static point size_reclbl(node_t* n, field_t* f)
{
	int		i;
	point	d,d0;
	pointf	dimen;

	if (f->lp) {
		dimen = f->lp->dimen;

		/* minimal whitespace around label */
		dimen.x += 2*GAP;
		dimen.y += 2*GAP;

		d = cvt2pt(dimen);
	}
	else {
		d.x = d.y = 0;
		for (i = 0; i < f->n_flds; i++) {
			d0 = size_reclbl(n,f->fld[i]);
			if (f->LR) { d.x += d0.x; d.y = MAX(d.y,d0.y); }
			else { d.y += d0.y; d.x = MAX(d.x,d0.x); }
		}
	}
	f->size = d;
	return d;
}

static void resize_reclbl(field_t* f, point sz)
{
	int			i,amt;
	double		inc;
	point		d,newsz;
	field_t		*sf;

	/* adjust field */
	d.x = sz.x - f->size.x; d.y = sz.y - f->size.y;
	f->size = sz;

	/* adjust children */
	if (f->n_flds) {
		if (f->LR) inc = (double)d.x/f->n_flds;
		else inc = (double)d.y/f->n_flds;
		for (i = 0; i < f->n_flds; i++) {
			sf = f->fld[i];
			amt = ((int)((i+1)*inc)) - ((int)(i*inc));
			if (f->LR) newsz = pointof(sf->size.x+amt,sz.y);
			else newsz = pointof(sz.x,sf->size.y+amt);
			resize_reclbl(sf,newsz);
		}
	}
}

static void pos_reclbl(field_t* f, point ul)
{
	int		i;

	f->b.LL = pointof(ul.x,ul.y-f->size.y);
	f->b.UR = pointof(ul.x+f->size.x,ul.y);
	for (i = 0; i < f->n_flds; i++) {
		pos_reclbl(f->fld[i],ul);
		if (f->LR) ul.x = ul.x + f->fld[i]->size.x;
		else ul.y = ul.y - f->fld[i]->size.y;
	}
}

#ifdef DEBUG
static void
indent (int l)
{
  int i;
  for (i = 0; i < l; i++) fputs ("  ", stderr);
}

static void
prbox (box b)
{
  fprintf (stderr, "((%d,%d),(%d,%d))\n", b.LL.x, b.LL.y, b.UR.x, b.UR.y);
}

static void
dumpL (field_t *info, int level)
{
  int i;

  indent (level);
  if (info->n_flds == 0) {
    fprintf (stderr, "Label \"%s\" ", info->lp->text);
    prbox (info->b);
  }
  else {
    fprintf (stderr, "Tbl ");
    prbox (info->b);
    for (i = 0; i < info->n_flds; i++) {
      dumpL (info->fld[i], level+1);  
    }
  }
}
#endif

/* syntax of labels: foo|bar|baz or foo|(recursive|label)|baz */
void record_init(GVC_t *gvc)
{
	field_t	*info;
	point	ul,sz;
    int     len;
    char*   textbuf;   /* temp buffer for storing labels */
	node_t *n = gvc->n;

	reclblp = ND_label(n)->text;
    len = strlen (reclblp);
    textbuf = N_NEW(len+1,char);
	if (!(info = parse_reclbl(gvc, n,NOT(GD_left_to_right(n->graph)), TRUE, textbuf))) {
		agerr(AGERR, "bad label format %s\n", ND_label(n)->text);
		reclblp = "\\N";
		info = parse_reclbl(gvc, n,NOT(GD_left_to_right(n->graph)), TRUE, textbuf);
	}
    free (textbuf);

	size_reclbl(n,info);
	sz.x = POINTS(ND_width(n));  sz.y = POINTS(ND_height(n));
    if (mapbool(late_string(n,N_fixed,"false"))) {
		if ((sz.x < info->size.x) || (sz.y < info->size.y)) {
/* should check that the record really won't fit, e.g., there may be no text.
			agerr(AGWARN, "node '%s' size may be too small\n",
				n->name);
 */
		}
    }
    else {
		sz.x = MAX(info->size.x,sz.x); sz.y = MAX(info->size.y,sz.y);
    }
	resize_reclbl(info,sz);
	ul = pointof(-sz.x/2,sz.y/2);
	pos_reclbl(info,ul);
	ND_width(n) = PS2INCH(info->size.x);
	ND_height(n) = PS2INCH(info->size.y);
	ND_shape_info(n) = (void*) info;
}

static void record_free(GVC_t *gvc)
{
	field_t* p = ND_shape_info(gvc->n);

	free(p);
}

static field_t	* map_rec_port(field_t* f, char* str)
{
	field_t		*rv;
	int		sub;

	if (f->id && (strcmp(f->id,str) == 0)) rv = f;
	else {
		rv = NULL;
		for (sub = 0; sub < f->n_flds; sub++)
			if ((rv = map_rec_port(f->fld[sub],str))) break;
	}
	return rv;
}

static port record_port(GVC_t *gvc, char* portname)
{
	field_t	*f;
	box		b;
	port	rv;
	node_t *n = gvc->n;

	if (portname[0] != ':') return Center;		/*could be '\000' */
	portname++;   /* skip delimiter */
	if ((f = map_rec_port((field_t*) ND_shape_info(n),portname)) == NULL) {
		unrecognized(n,portname);
		return Center;
	}

	b = f->b;
	rv.p = pointof((b.LL.x+b.UR.x)/2,(b.LL.y+b.UR.y)/2);
	if (GD_left_to_right(n->graph)) rv.p = invflip_pt(rv.p);
	rv.bp = &f->b;
	rv.order = (MC_SCALE * (ND_lw_i(n) + rv.p.x)) / (ND_lw_i(n) + ND_rw_i(n));
	rv.constrained = FALSE;
	rv.defined = TRUE;
	return rv;
}

static boolean record_inside(inside_t *inside_context, pointf p)
{

	edge_t			*f;
	field_t			*fld0;
	static edge_t	*last_e;
	static node_t	*last_n;
	static box		bbox;
        edge_t  *e = inside_context->e;
        node_t  *n = inside_context->n;

		/* convert point to node coordinate system */
	if (GD_left_to_right(n->graph)) p = flip_ptf(p);
		/* find real edge */
	for (f = e; ED_edge_type(f) != NORMAL; f = ED_to_orig(f));
	e = f;

	if ((e != last_e) || (n != last_n)) {
		box*  bp;
		last_e = e; last_n = n;
		bp = GET_PORT_BOX(n,e);
		if (bp) bbox = *bp;
		else {
			fld0 = (field_t*) ND_shape_info(n);
			bbox = fld0->b;
		}
	}

	return INSIDE(p,bbox);
}

static int record_path(GVC_t *gvc, int pt, box rv[], int* kptr)
{
	int			i,side,ls,rs;
	point		p;
	field_t		*info;
	node_t *n = gvc->n;
	edge_t *e = gvc->e;

	if (pt == 1) p = ED_tail_port(e).p;
	else p = ED_head_port(e).p;
	info = (field_t*) ND_shape_info(n);

	for (i = 0; i < info->n_flds; i++) {
		if (GD_left_to_right(n->graph) == FALSE)
			{ ls = info->fld[i]->b.LL.x; rs = info->fld[i]->b.UR.x; }
		else
			{ ls = info->fld[i]->b.LL.y; rs = info->fld[i]->b.UR.y; }
		if (BETWEEN(ls,p.x,rs)) {
			/* FIXME: I don't understand this code */
			if (GD_left_to_right(n->graph)) {
				rv[0] = flip_rec_box(info->fld[i]->b,ND_coord_i(n));
			}
			else {
				rv[0].LL.x = ND_coord_i(n).x + ls;
				rv[0].LL.y = ND_coord_i(n).y - ND_ht_i(n)/2;
				rv[0].UR.x = ND_coord_i(n).x + rs;
			}
#if 0
			s0 = (rv[0].UR.x - rv[0].LL.x)/6;
			s0 = MIN(s0,n->GD_nodesep(graph));
			s1 = MIN(p.x - rv[0].LL.x,rv[0].UR.x - p.x)/2;
			sep = MIN(s0,s1);
			rv[0].LL.x += sep;
			rv[0].UR.x -= sep;
#endif
			rv[0].UR.y = ND_coord_i(n).y + ND_ht_i(n)/2;
			*kptr = 1;
			break;
		}
	}
	if (pt == 1) side = BOTTOM; else side = TOP;
	return side;
}

static void gen_fields(GVC_t *gvc, field_t* f)
{
	int			i;
	double		cx,cy;
	point		A[2];
	node_t *n = gvc->n;

	if (f->lp) {
		cx = (f->b.LL.x + f->b.UR.x)/2.0 + ND_coord_i(n).x;
		cy = (f->b.LL.y + f->b.UR.y)/2.0 + ND_coord_i(n).y;
		f->lp->p =  pointof((int)cx,(int)cy);
		emit_label(gvc, f->lp);
	}

	/* yes it is ridiculous that black is hardwired here, the same way
	 * it is wired into psgen.c ... outline color should be adjustable */
    /* for reasons not presently remembered, we used to say
                gvrender_set_color("black");
        right here */

	for (i = 0; i < f->n_flds; i++) {
		if (i > 0) {
			if (f->LR) {
				A[0] = f->fld[i]->b.LL;
				A[1].x = A[0].x;
				A[1].y = f->fld[i]->b.UR.y;
			}
			else {
				A[1] = f->fld[i]->b.UR;
				A[0].x = f->fld[i]->b.LL.x;
				A[0].y = A[1].y;
			}
			A[0] = add_points(A[0],ND_coord_i(n));
			A[1] = add_points(A[1],ND_coord_i(n));
			gvrender_polyline(gvc, A, 2);
		}
		gen_fields(gvc, f->fld[i]);
	}
}

static void record_gencode(GVC_t *gvc)
{
	point	A[4];
	int		i,style;
	field_t	*f;
	node_t *n = gvc->n;

	f = (field_t*) ND_shape_info(n);
	A[0] = f->b.LL;
	A[2] = f->b.UR;
	A[1].x = A[2].x; A[1].y = A[0].y;
	A[3].x = A[0].x; A[3].y = A[2].y;
	for (i = 0; i < 4; i++) A[i] = add_points(A[i],ND_coord_i(n));
	style = stylenode(gvc);
	pencolor(gvc);
	if (style & FILLED) fillcolor(gvc); /* emit fill color */
	if (streq(ND_shape(n)->name,"Mrecord")) style |= ROUNDED;
	if (style & (ROUNDED | DIAGONALS)) round_corners(gvc, A, 4, ROUNDED);
	else gvrender_polygon(gvc, A, 4, style&FILLED);
	gen_fields(gvc, f);
}

static shape_desc **UserShape;
static int	N_UserShape;

shape_desc *find_user_shape(char* name)
{
	int		i;
	if (UserShape) {
		for (i = 0; i < N_UserShape; i++) {
			if (streq(UserShape[i]->name,name)) return UserShape[i];
		}
	}
	return NULL;
}

static shape_desc *user_shape(char* name)
{
	int			i;
	shape_desc	*p;

	if ((p = find_user_shape(name))) return p;
	i = N_UserShape++;
	UserShape = ALLOC(N_UserShape,UserShape,shape_desc*);
	p = UserShape[i] = NEW(shape_desc);
	*p = Shapes[0];
	p->name = name;
	p->usershape = TRUE;
	if (Lib == NULL)
		agerr(AGWARN, "using %s for unknown shape %s\n", 
        Shapes[0].name,p->name);
	return p;
}

shape_desc * bind_shape(char* name)
{
	shape_desc	*ptr,*rv= NULL;

	for (ptr = Shapes; ptr->name; ptr++)
		if (!strcmp(ptr->name,name)) {rv = ptr; break;}
	if (rv == NULL) rv = user_shape(name);
	return rv;
}

static boolean epsf_inside(inside_t *inside_context, pointf p)
{
	pointf	P;
	double	x2;
        node_t  *n = inside_context->n;

	P = (GD_left_to_right(n->graph)? flip_ptf(p) : p);
	x2 = ND_ht_i(n) / 2;
	return ((P.y >= -x2) && (P.y <= x2) && (P.x >= -ND_lw_i(n)) && (P.x <= ND_rw_i(n)));
}

