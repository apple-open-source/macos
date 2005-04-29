/* $Id: gvrender.c,v 1.1 2004/05/04 21:50:47 yakimoto Exp $ $Revision: 1.1 $ */
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

/*
 *  graphics code generator wrapper
 *
 *  This library will eventually form the socket for run-time loadable
 *  renderer plugins.   Initially it just provides wrapper functions
 *  to the old codegens so that the changes can be locallized away from all
 *  the various codegen callers.
 *
 */

#include	<stdio.h>
#include	<stdlib.h>

#include	"utils.h"
#include	"render.h"
#include	"gvre.h"
#include	"gvrender.h"

/* from common/utils.c */
extern void *zmalloc(size_t);

GVC_t *gvNEWcontext(char **info, char *user)
{
    GVC_t *gvc = zmalloc(sizeof(GVC_t));

    if (gvc) {
	gvc->info = info;
	gvc->user = user;
	gvc->onetime = TRUE;	/* force initial reset */
    }
    return gvc;
}

void gvBindContext(GVC_t * gvc, graph_t *g)
{
	gvc->g = g;
	GD_gvc(g) = gvc;
}

void gvFREEcontext(GVC_t * gvc)
{
    gvrender_delete_jobs(gvc);
    free(gvc);
}

/* =================== */

static gvrender_job_t *output_filename_job;
static gvrender_job_t *output_langname_job;

/*
 * -T and -o can be specified in any order relative to the other, e.g.
 *            -T -T -o -o
 *            -T -o -o -T
 * The first -T is paired with the first -o, the second with the second, and so on.
 *
 * If there are more -T than -o, then the last -o is repeated for the remaining -T
 * and vice-versa
 *
 * If there are no -T or -o then a single job is instantiated.
 *
 * If there is no -T on the first job, then "dot" is used.
 */
void gvrender_output_filename_job(GVC_t * gvc, char *name)
{
    if (!gvc->jobs) {
	output_filename_job = gvc->job = gvc->jobs =
		zmalloc(sizeof(gvrender_job_t));
    } else {
	if (!output_filename_job) {
	    output_filename_job = gvc->jobs;
	} else {
	    if (!output_filename_job->next) {
		output_filename_job->next = zmalloc(sizeof(gvrender_job_t));
	    }
	    output_filename_job = output_filename_job->next;
	}
    }
    output_filename_job->output_filename = name;
}

void gvrender_output_langname_job(GVC_t * gvc, char *name)
{
    if (!gvc->jobs) {
	output_langname_job = gvc->job = gvc->jobs = 
		zmalloc(sizeof(gvrender_job_t));
    } else {
	if (!output_langname_job) {
	    output_langname_job = gvc->jobs;
	} else {
	    if (!output_langname_job->next) {
		output_langname_job->next =
		    zmalloc(sizeof(gvrender_job_t));
	    }
	    output_langname_job = output_langname_job->next;
	}
    }
    output_langname_job->output_langname = name;
}

gvrender_job_t *gvrender_first_job(GVC_t * gvc)
{
    return (gvc->job = gvc->jobs);
}

gvrender_job_t *gvrender_next_job(GVC_t * gvc)
{
    gvrender_job_t *job = gvc->job->next;

    if (job) {
	/* if langname not specified, then repeat previous value */
	if (!job->output_langname)
	    job->output_langname = gvc->job->output_langname;
	/* if filename not specified, then leave NULL to indicate stdout */
    }
    return (gvc->job = job);
}

void gvrender_delete_jobs(GVC_t * gvc)
{
    gvrender_job_t *job, *j;

    job = gvc->jobs;
    while ((j = job)) {
	job = job->next;
	free(j);
    }
    gvc->jobs = gvc->job = output_filename_job = output_langname_job = NULL;
}

/* =================== */

static point p0 = { 0, 0 };
static box b0 = { {0, 0}, {0, 0} };

int gvrender_features(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

#if ENABLE_CODEGENS
    if (gvre)
#endif
	return gvre->features;
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;
	int features = 0;

	if (cg && cg->bezier_has_arrows)
	    features |= GVRENDER_DOES_ARROWS;
	if (cg && cg->begin_layer)
	    features |= GVRENDER_DOES_LAYERS;
	/* WARNING - nasty hack to avoid modifying old codegens */
	if (cg == &PS_CodeGen)
	    features |= GVRENDER_DOES_MULTIGRAPH_OUTPUT_FILES;

	return features;
    }
#endif
}

void gvrender_reset(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->onetime = TRUE;
    if (gvre && gvre->reset)
	gvre->reset(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->reset)
	    cg->reset();
    }
#endif
}

void gvrender_begin_job(GVC_t * gvc, char **lib, point pages)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->lib = lib;
    gvc->pages = pages;
    if (gvre && gvre->begin_job)
	gvre->begin_job(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_job)
	    cg->begin_job(gvc->job->output_file, gvc->g, lib, gvc->user, gvc->info, pages);
    }
#endif
}

void gvrender_end_job(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_job)
	gvre->end_job(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_job)
	    cg->end_job();
    }
#endif
    gvc->lib = NULL;
    gvc->pages = p0;
}

void gvrender_begin_graph(GVC_t * gvc, graph_t * g, box bb, point pb)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->g = g;
    gvc->bb = bb;
    gvc->pb = pb;
    if (gvre && gvre->begin_graph)
	gvre->begin_graph(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_graph)
	    cg->begin_graph(g, bb, pb);
    }
#endif
}

void gvrender_end_graph(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_graph)
	gvre->end_graph(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_graph)
	    cg->end_graph();
    }
#endif
    gvc->bb = b0;
    gvc->pb = p0;
/* FIXME    gvc->g = NULL; */
    gvc->onetime = FALSE;
}

void gvrender_begin_page(GVC_t * gvc, point page, double scale, int rot,
			 point offset)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->page = page;
    gvc->scale = scale;
    gvc->rot = rot;
    gvc->offset = offset;
    if (gvre && gvre->begin_page)
	gvre->begin_page(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_page)
	    cg->begin_page(gvc->g, page, scale, rot, offset);
    }
#endif
}

void gvrender_end_page(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_page)
	gvre->end_page(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_page)
	    cg->end_page();
    }
#endif
    gvc->page = p0;
    gvc->scale = 0.0;
    gvc->rot = 0;
    gvc->offset = p0;

}

void gvrender_begin_layer(GVC_t * gvc, char *layerName, int layer,
			  int nLayers)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->layerName = layerName;
    gvc->layer = layer;
    gvc->nLayers = nLayers;
    if (gvre && gvre->begin_layer)
	gvre->begin_layer(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_layer)
	    cg->begin_layer(layerName, layer, nLayers);
    }
#endif
}

void gvrender_end_layer(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_layer)
	gvre->end_layer(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_layer)
	    cg->end_layer();
    }
#endif
    gvc->layerName = NULL;
    gvc->layer = 0;
    gvc->nLayers = 0;
}

void gvrender_begin_cluster(GVC_t * gvc, graph_t * sg)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->sg = sg;
    if (gvre && gvre->begin_cluster)
	gvre->begin_cluster(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_cluster)
	    cg->begin_cluster(sg);
    }
#endif
}

void gvrender_end_cluster(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_cluster)
	gvre->end_cluster(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_cluster)
	    cg->end_cluster();
    }
#endif
    gvc->sg = NULL;
}

void gvrender_begin_nodes(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_nodes)
	gvre->begin_nodes(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_nodes)
	    cg->begin_nodes();
    }
#endif
}

void gvrender_end_nodes(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_nodes)
	gvre->end_nodes(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_nodes)
	    cg->end_nodes();
    }
#endif
}

void gvrender_begin_edges(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_edges)
	gvre->begin_edges(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_edges)
	    cg->begin_edges();
    }
#endif
}

void gvrender_end_edges(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_edges)
	gvre->end_edges(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_edges)
	    cg->end_edges();
    }
#endif
}

void gvrender_begin_node(GVC_t * gvc, node_t * n)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->n = n;
    if (gvre && gvre->begin_node)
	gvre->begin_node(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_node)
	    cg->begin_node(n);
    }
#endif
}

void gvrender_end_node(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_node)
	gvre->end_node(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_node)
	    cg->end_node();
    }
#endif
    gvc->n = NULL;
}

void gvrender_begin_edge(GVC_t * gvc, edge_t * e)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->e = e;
    if (gvre && gvre->begin_edge)
	gvre->begin_edge(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_edge)
	    cg->begin_edge(e);
    }
#endif
}

void gvrender_end_edge(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_edge)
	gvre->end_edge(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_edge)
	    cg->end_edge();
    }
#endif
    gvc->e = NULL;
}

void gvrender_begin_context(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_context)
	gvre->begin_context(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_context)
	    cg->begin_context();
    }
#endif
}

void gvrender_end_context(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_context)
	gvre->end_context(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_context)
	    cg->end_context();
    }
#endif
}

void gvrender_set_font(GVC_t * gvc, char *fontname, double fontsize)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->set_font)
	gvre->set_font(gvc, fontname, fontsize);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_font)
	    cg->set_font(fontname, fontsize);
    }
#endif
}

void gvrender_textline(GVC_t * gvc, point p, textline_t * str)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    pointf PF;

    P2PF(p, PF);
/* end hack */

    if (gvre && gvre->textline)
	gvre->textline(gvc, PF, str);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->textline)
	    cg->textline(p, str);
    }
#endif
}

void gvrender_set_pencolor(GVC_t * gvc, char *color)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->set_pencolor)
	gvre->set_pencolor(gvc, color);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_pencolor)
	    cg->set_pencolor(color);
    }
#endif
}

void gvrender_set_fillcolor(GVC_t * gvc, char *color)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->set_fillcolor)
	gvre->set_fillcolor(gvc, color);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_fillcolor)
	    cg->set_fillcolor(color);
    }
#endif
}

void gvrender_set_style(GVC_t * gvc, char **s)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->set_style)
	gvre->set_style(gvc, s);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_style)
	    cg->set_style(s);
    }
#endif
}

void gvrender_ellipse(GVC_t * gvc, point p, int rx, int ry, int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    pointf PF;
    double RXF = (double) rx;
    double RYF = (double) ry;

    P2PF(p, PF);
/* end hack */

    if (gvre && gvre->ellipse)
	gvre->ellipse(gvc, PF, RXF, RYF, filled);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->ellipse)
	    cg->ellipse(p, rx, ry, filled);
    }
#endif
}

void gvrender_polygon(GVC_t * gvc, point * A, int n, int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    static pointf *AF;
    static int sizeAF;
    int i;

    if (sizeAF < n)
	AF = realloc(AF, n * sizeof(pointf));
    for (i = 0; i < n; i++)
	P2PF(A[i], AF[i]);
/* end hack */

    if (gvre && gvre->polygon)
	gvre->polygon(gvc, AF, n, filled);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->polygon)
	    cg->polygon(A, n, filled);
    }
#endif
}

void gvrender_beziercurve(GVC_t * gvc, point * A, int n,
			  int arrow_at_start, int arrow_at_end)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    static pointf *AF;
    static int sizeAF;
    int i;

    if (sizeAF < n)
	AF = realloc(AF, n * sizeof(pointf));
    for (i = 0; i < n; i++)
	P2PF(A[i], AF[i]);
/* end hack */

    if (gvre && gvre->beziercurve)
	gvre->beziercurve(gvc, AF, n, arrow_at_start, arrow_at_end);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->beziercurve)
	    cg->beziercurve(A, n, arrow_at_start, arrow_at_end);
    }
#endif
}

void gvrender_polyline(GVC_t * gvc, point * A, int n)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    static pointf *AF;
    static int sizeAF;
    int i;

    if (sizeAF < n)
	AF = realloc(AF, n * sizeof(pointf));
    for (i = 0; i < n; i++)
	P2PF(A[i], AF[i]);
/* end hack */

    if (gvre && gvre->polyline)
	gvre->polyline(gvc, AF, n);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->polyline)
	    cg->polyline(A, n);
    }
#endif
}

void gvrender_comment(GVC_t * gvc, void *obj, attrsym_t * sym)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->comment)
	gvre->comment(gvc, obj, sym);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->comment)
	    cg->comment(obj, sym);
    }
#endif
}

point gvrender_textsize(GVC_t * gvc, char *str, char *fontname,
			double fontsz)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->textsize)
	gvre->textsize(gvc, str, fontname, fontsz);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->textsize)
	    return cg->textsize(str, fontname, fontsz);
    }
#endif
    return p0;
}

void gvrender_user_shape(GVC_t * gvc, char *name, point * A, int n,
			 int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    static pointf *AF;
    static int sizeAF;
    int i;

    if (sizeAF < n)
	AF = realloc(AF, n * sizeof(pointf));
    for (i = 0; i < n; i++)
	P2PF(A[i], AF[i]);
/* end hack */

    if (gvre && gvre->user_shape)
	gvre->user_shape(gvc, name, AF, n, filled);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->user_shape)
	    cg->user_shape(name, A, n, filled);
    }
#endif
}

point gvrender_usershapesize(GVC_t * gvc, node_t * n, char *name)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->usershapesize)
	return gvre->usershapesize(gvc, n, name);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->usershapesize)
	    return cg->usershapesize(n, name);
    }
#endif
    return p0;
}
