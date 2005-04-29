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

/*
    J$: added `pdfmark' URL embedding.  PostScript rendered from
        dot files with URL attributes will get active PDF links
        from Adobe's Distiller.
 */
#define	PDFMAX	3240	/*  Maximum size of Distiller's PDF canvas  */

#include	"render.h"
#include	"gvrender.h"
#include	"ps.h"
#include	"utils.h"

#ifndef MSWIN32
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <stdio.h>


static	int		N_pages,Cur_page;
/* static 	point	Pages; */
static	box		PB;
static int		onetime = TRUE;

static char	*Fill = "fill\n";
static char	*Stroke = "stroke\n";
static char	*Newpath_Moveto = "newpath %d %d moveto\n";
static char	**U_lib;

typedef struct grcontext_t {
	char	*pencolor,*fillcolor,*font;
	double	size;
} grcontext_t;

#define STACKSIZE 8
static grcontext_t S[STACKSIZE];
static int SP = 0;

static void
ps_reset(void)
{
	onetime = TRUE;
}

static void
ps_begin_job(FILE *ofp,graph_t *g, char **lib, char *user, char *info[],
point pages)
{
	/* Pages = pages; */
	U_lib = lib;
		/* wrong when drawing more than one than one graph - use (atend) */
	N_pages = pages.x * pages.y;
	Cur_page = 0;
	fprintf(Output_file,"%%!PS-Adobe-2.0\n");
	fprintf(Output_file,"%%%%Creator: %s version %s (%s)\n",
		info[0], info[1],info[2]);
	fprintf(Output_file,"%%%%For: %s\n",user);
	fprintf(Output_file,"%%%%Title: %s\n",g->name);
	fprintf(Output_file,"%%%%Pages: (atend)\n");

	/* remainder is emitted by first begin_graph */
}

static  void
ps_end_job(void)
{
	fprintf(Output_file,"%%%%Trailer\n");
	fprintf(Output_file,"%%%%Pages: %d\n",Cur_page);
	fprintf(Output_file,"end\nrestore\n");
	fprintf(Output_file,"%%%%EOF\n");
}

static void
ps_comment(void* obj, attrsym_t* sym)
{
	char	*str;
	str = late_string(obj,sym,"");
	if (str[0]) fprintf(Output_file,"%% %s\n",str);
}

#define N_EPSF 32
static int  N_EPSF_files;
static char *EPSF_contents[N_EPSF];

typedef struct epsf_s {
	int		macro_id;
	point	offset;
} epsf_t;

void epsf_init(GVC_t *gvc)
{
	char	*str,*contents;
	char	line[BUFSIZ];
	FILE	*fp;
	struct stat statbuf;
	int		i, saw_bb;
	int		lx,ly,ux,uy;
	epsf_t	*desc;
	node_t *n = gvc->n;

	if (N_EPSF_files >= N_EPSF) {
		agerr(AGERR, "Can't read another EPSF file. Maximum number (%d) exceeded.\n", N_EPSF);
		return;
	}

	if ((str=safefile(agget(n,"shapefile")))) {
		if ((fp = fopen(str,"r"))) {
			/* try to find size */
			saw_bb = FALSE;
			while (fgets(line, sizeof(line), fp)) {
			  if (sscanf(line,"%%%%BoundingBox: %d %d %d %d",&lx,&ly,&ux,&uy) == 4) {
				saw_bb = TRUE;
				break;
			  }
			}

			if (saw_bb) {
				ND_width(n) = PS2INCH(ux - lx);
				ND_height(n) = PS2INCH(uy - ly);
				fstat(fileno(fp),&statbuf);
				i = N_EPSF_files++;
				ND_shape_info(n) = desc = NEW(epsf_t);
				desc->macro_id = i;
				desc->offset.x = -lx - (ux - lx)/2;
				desc->offset.y = -ly - (uy - ly)/2;
				contents = EPSF_contents[i] = N_GNEW(statbuf.st_size+1,char);
				fseek(fp,0,SEEK_SET);
				fread(contents,statbuf.st_size,1,fp);
				contents[statbuf.st_size] = '\0';
				fclose(fp);
			}
			else agerr(AGWARN, "BoundingBox not found in epsf file %s\n",str);
		}
		else agerr(AGWARN, "couldn't open epsf file %s\n",str);
	}
	else agerr(AGWARN, "shapefile not set for epsf node %s\n",n->name);
}

void epsf_free(GVC_t *gvc)
{
/* FIXME  What about the allocated EPSF_contents[i] ? */

	if (ND_shape_info(gvc->n)) free(ND_shape_info(gvc->n));
}

void epsf_gencode(GVC_t *gvc)
{
	epsf_t	*desc;
	node_t *n = gvc->n;

	desc = (epsf_t*)(ND_shape_info(n));
	if (!desc) return;
	gvrender_begin_context(gvc);
	if (desc) fprintf(Output_file,"%d %d translate newpath user_shape_%d\n",
		ND_coord_i(n).x+desc->offset.x,ND_coord_i(n).y+desc->offset.y,
		desc->macro_id);
	ND_label(n)->p = ND_coord_i(n);
	gvrender_end_context(gvc);
	emit_label(gvc, ND_label(n));
}

static void
epsf_define(void)
{
	int	i;
#define FILTER_EPSF 1	
#if FILTER_EPSF
	char	*p;
#endif

	for (i = 0; i < N_EPSF_files; i++) {
		fprintf(Output_file,"/user_shape_%d {\n",i);

		if (fputs("%%BeginDocument:\n", Output_file) == EOF) {
			perror("epsf_define()->fputs");
			exit(EXIT_FAILURE);
		}

#if FILTER_EPSF
		/* this removes EPSF DSC comments that, when nested in another
		 * document, cause errors in Ghostview and other Postscript
		 * processors (although legal according to the Adobe EPSF spec).
		 */
		p = EPSF_contents[i];
		while (*p) {	/* skip %%EOF lines */
			if ((p[0] == '%') && (p[1] == '%') && (
					!strncasecmp(&p[2],"EOF",3) ||
					!strncasecmp(&p[2],"BEGIN",5) ||
					!strncasecmp(&p[2],"END",3) ||
					!strncasecmp(&p[2],"TRAILER",7)
						)) {
				while (*p++ != '\n');
				continue;
			}
			do {fputc(*p,Output_file);} while (*p++ != '\n');
		}
#else
		if (fputs(EPSF_contents[i], Output_file) == EOF) {
			perror("epsf_define()->fputs");
			exit(EXIT_FAILURE);
		}
#endif

		if (fputs("%%EndDocument\n", Output_file) == EOF) {
			perror("epsf_define()->fputs");
			exit(EXIT_FAILURE);
		}

		if (fputs("} bind def\n",Output_file) == EOF) {
			perror("epsf_define()->fputs");
			exit(EXIT_FAILURE);
		}

		free(EPSF_contents[i]);
#if 0
		fprintf(Output_file,"} bind def\n");
#endif
	}
	N_EPSF_files = 0;
}

static void
ps_begin_graph(graph_t* g, box bb, point pb)
{
	char *s;
	static char setupLatin1 = FALSE;

	PB = bb;
	if (onetime) {
		fprintf(Output_file,"%%%%BoundingBox: %d %d %d %d\n",
			bb.LL.x-1,bb.LL.y-1,bb.UR.x+1,bb.UR.y+1);
		ps_comment(g,agfindattr(g,"comment"));
		fprintf(Output_file,"%%%%EndComments\nsave\n");
		cat_libfile(Output_file,U_lib,ps_txt);
		epsf_define();

 		/*  Set base URL for relative links (for Distiller >= 3.0)  */
 		if (((s = agget(g, "href")) && s[0]) 
 		  || ((s = agget(g, "URL")) && s[0])) {
 			fprintf(Output_file,
				"[ {Catalog} << /URI << /Base (%s) >> >>\n"
 				"/PUT pdfmark\n", s);
		}
	}
	if (GD_has_Latin1char(g) && !setupLatin1) {
		fprintf(Output_file,"setupLatin1\n");	/* as defined in ps header */
		setupLatin1 = TRUE;
	}
}

static void
ps_end_graph(void)
{
	onetime = FALSE;
}

static void
ps_begin_page(graph_t *g, point page, double scale, int rot, point offset)
{
	point	sz;

	Cur_page++;
	sz = sub_points(PB.UR,PB.LL);
    fprintf(Output_file,"%%%%Page: %d %d\n",Cur_page,Cur_page);
    fprintf(Output_file,"%%%%PageBoundingBox: %d %d %d %d\n",
		PB.LL.x,PB.LL.y,PB.UR.x+1,PB.UR.y+1);
	fprintf(Output_file,"%%%%PageOrientation: %s\n",(rot?"Landscape":"Portrait"));
    fprintf(Output_file,"gsave\n%d %d %d %d boxprim clip newpath\n",
		PB.LL.x-1, PB.LL.y-1, sz.x + 2, sz.y + 2);
	fprintf(Output_file,"%d %d translate\n",PB.LL.x,PB.LL.y);
	if (rot) fprintf(Output_file,"gsave %d %d translate %d rotate\n",
		PB.UR.x-PB.LL.x,0,rot);
	fprintf(Output_file,"%d %d %d beginpage\n",page.x,page.y,N_pages);
	if (rot) fprintf(Output_file,"grestore\n");
	if (scale != 1.0) fprintf(Output_file,"%.4f set_scale\n",scale);
	fprintf(Output_file,"%d %d translate %d rotate\n",offset.x,offset.y,rot);
	assert(SP == 0);
	S[SP].font =  S[SP].pencolor = S[SP].fillcolor = "";
       	S[SP].size = 0.0;

 	/*  Define the size of the PS canvas  */
	if (Output_lang == PDF) {
 		if (PB.UR.x >= PDFMAX || PB.UR.y >= PDFMAX)
			agerr(AGWARN,
			 "canvas size (%d,%d) exceeds PDF limit (%d)\n"
 				"\t(suggest setting a bounding box size, see dot(1))\n",
 				PB.UR.x, PB.UR.y, PDFMAX);
 		fprintf(Output_file,"[ /CropBox [%d %d %d %d] /PAGES pdfmark\n",
 			PB.LL.x, PB.LL.y, PB.UR.x+1, PB.UR.y+1);
	}
}

static void
ps_end_page(void)
{
	/* the showpage is really a no-op, but at least one PS processor
	 * out there needs to see this literal token.  endpage does the real work.
	 */
	fprintf(Output_file,"endpage\nshowpage\ngrestore\n");
	fprintf(Output_file,"%%%%PageTrailer\n");
	fprintf(Output_file,"%%%%EndPage: %d\n",Cur_page);
	assert(SP == 0);
}

static void ps_begin_layer(char *s, int n, int Nlayers) 
{ 
	fprintf(Output_file,"%d %d setlayer\n", n, Nlayers);
}
    
static void
ps_begin_cluster(graph_t* g)
{
	fprintf(Output_file,"%% %s\n",g->name);

 	/*  Embed information for Distiller to generate hyperlinked PDF  */
	map_begin_cluster(g);
}

static void
ps_begin_node(node_t* n)
{
	fprintf(Output_file,"\n%%\t%s\n",n->name);
	ps_comment(n,N_comment);

 	/*  Embed information for Distiller to generate hyperlinked PDF  */
	map_begin_node(n);
}

static void
ps_begin_edge (edge_t* e)
{
    fprintf(Output_file,"\n%%\t%s -> %s\n",e->tail->name,e->head->name);
    ps_comment(e,E_comment);

    /*  Embed information for Distiller, so it can generate hyperactive PDF  */
    map_begin_edge(e);
}


static void
ps_begin_context(void)
{
	fprintf(Output_file,"gsave 10 dict begin\n");
	if (SP == STACKSIZE - 1) agerr(AGWARN, "psgen stk ovfl\n");
	else {SP++; S[SP] = S[SP-1];}
}

static void
ps_end_context(void)
{
	if (SP == 0) agerr(AGWARN, "psgen stk undfl\n");
	else SP--;
	fprintf(Output_file,"end grestore\n");
}

static void
ps_set_font(char* name, double size)
{
	if (strcmp(S[SP].font,name) || (size != S[SP].size)) {
		fprintf(Output_file,"%.2f /%s set_font\n",size,name);
		S[SP].font = name;
		S[SP].size = size;
	}
}

static void
ps_set_color(char* name)
{
	static char *op[] = {"graph","node","edge","sethsb"};
	color_t	color;

	colorxlate(name,&color,HSV_DOUBLE);
	fprintf(Output_file,"%.3f %.3f %.3f %scolor\n",
		color.u.HSV[0],color.u.HSV[1],color.u.HSV[2],op[Obj]);
}

static void
ps_set_pencolor(char* name)
{
	if (strcmp(name,S[SP].pencolor)) {
		ps_set_color(name);   /* change pen color immediately */
		S[SP].pencolor = name;
	}
}

static void
ps_set_fillcolor(char* name)
{
	S[SP].fillcolor = name;      /* defer changes to fill color to shape */
}

static void
ps_set_style(char** s)
{
	char	*line,*p;

	while ((p = line = *s++)) {
		while (*p) p++; p++;
		while (*p) {
			fprintf(Output_file,"%s ",p);
			while (*p) p++; p++;
		}
		fprintf(Output_file,"%s\n",line);
	}
}

char *
ps_string(char *s)
{
	static char	*buf = NULL;
	static int	bufsize = 0;
	int		pos = 0;
	char		*p;

        if (!buf) {
                bufsize = 64;
                buf = N_GNEW(bufsize,char);
        }

	p = buf;
	*p++ = LPAREN;
	pos++;
	while (*s)  {
		if (pos > (bufsize-8)) {
			bufsize *= 2;
			buf = grealloc(buf,bufsize);
			p = buf + pos;
		}
		if ((*s == LPAREN) || (*s == RPAREN) || (*s == '\\')) {
			*p++ = '\\';
			pos++;
		}
		*p++ = *s++;
		pos++;
	}
	*p++ = RPAREN;
	*p = '\0';
	return buf;
}

static void
ps_textline(point p, textline_t *line)
{
	double adj;

	switch(line->just) {
		case 'l':
			adj = 0.0;
			break;
		case 'r':
			adj = -1.0;
			break;
		default:
		case 'n':
			adj = -0.5;
			break;
	}
	fprintf(Output_file,"%d %d moveto %d %.1f %s alignedtext\n",
		p.x,p.y,line->width,adj,ps_string(line->str));
}

static void
ps_bezier(point *A, int n, int arrow_at_start, int arrow_at_end)
{
	int		j;
	if (arrow_at_start || arrow_at_end)
		agerr(AGERR, "ps_bezier illegal arrow args\n");
	fprintf(Output_file,Newpath_Moveto,A[0].x,A[0].y);
	for (j = 1; j < n; j += 3)
		fprintf(Output_file,"%d %d %d %d %d %d curveto\n",
			A[j].x,A[j].y,A[j+1].x,A[j+1].y,A[j+2].x,A[j+2].y);
	fprintf(Output_file,Stroke);
}

static void
ps_polygon(point *A, int n, int filled)
{
	int		j;

	if (filled) {
		ps_set_color(S[SP].fillcolor);
		fprintf(Output_file,Newpath_Moveto,A[0].x,A[0].y);
		for (j = 1; j < n; j++)
		       	fprintf(Output_file,"%d %d lineto\n",A[j].x,A[j].y);
		fprintf(Output_file,"closepath\n");
		fprintf(Output_file, Fill);
		ps_set_color(S[SP].pencolor);
	}
	fprintf(Output_file,Newpath_Moveto,A[0].x,A[0].y);
	for (j = 1; j < n; j++)
	       	fprintf(Output_file,"%d %d lineto\n",A[j].x,A[j].y);
	fprintf(Output_file,"closepath\n");
	fprintf(Output_file, Stroke);
}

static void
ps_ellipse(point p, int rx, int ry, int filled)
{
	if (filled) {
		ps_set_color(S[SP].fillcolor);
		fprintf(Output_file,"%d %d %d %d ellipse_path\n",p.x,p.y,rx,ry);
		fprintf(Output_file, Fill);
		ps_set_color(S[SP].pencolor);
	}
	if (!filled || (filled && strcmp(S[SP].fillcolor,S[SP].pencolor))) {
		fprintf(Output_file,"%d %d %d %d ellipse_path\n",p.x,p.y,rx,ry);
		fprintf(Output_file, Stroke);
	}
}

static void
ps_polyline(point* A, int n)
{
	int		j;

	fprintf(Output_file,Newpath_Moveto,A[0].x,A[0].y);
	for (j = 1; j < n; j ++) fprintf(Output_file,"%d %d lineto\n",A[j].x,A[j].y);
	fprintf(Output_file,Stroke);
}

static void
ps_user_shape(char *name, point *A, int sides, int filled)
{
	int		j;
	fprintf(Output_file,"[ ");
	for (j = 0; j < sides; j++) fprintf(Output_file,"%d %d ",A[j].x,A[j].y);
	fprintf(Output_file,"%d %d ",A[0].x,A[0].y);
	fprintf(Output_file,"]  %d %s %s\n",sides,(filled?"true":"false"),name);
}

codegen_t	PS_CodeGen = {
	ps_reset,
	ps_begin_job, ps_end_job,
	ps_begin_graph, ps_end_graph,
	ps_begin_page, ps_end_page,
	ps_begin_layer, 0, /* ps_end_layer */
	ps_begin_cluster, 0, /* ps_end_cluster */
	0, /* ps_begin_nodes */ 0, /* ps_end_nodes */
	0, /* ps_begin_edges */ 0, /* ps_end_edges */
	ps_begin_node, 0, /* ps_end_node */
	ps_begin_edge, 0, /* ps_end_edge */
	ps_begin_context, ps_end_context,
	ps_set_font, ps_textline,
	ps_set_pencolor, ps_set_fillcolor, ps_set_style,
	ps_ellipse, ps_polygon,
	ps_bezier, ps_polyline,
	0, /* bezier_has_arrows */
	ps_comment,
	0, /* textsize */
	ps_user_shape,
	0 /* usershapesize */
};
