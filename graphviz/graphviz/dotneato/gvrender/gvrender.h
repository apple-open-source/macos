/* $Id: gvrender.h,v 1.1 2004/05/04 21:50:47 yakimoto Exp $ $Revision: 1.1 $ */
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

/* This is the public header for the callers of libgvrender */

#ifndef GVRENDER_H
#define GVRENDER_H

#include "gvrenderint.h"

extern GVC_t *gvNEWcontext(char **info, char *user);
extern void gvBindContext(GVC_t * gvc, graph_t *g);
extern void gvFREEcontext(GVC_t * gvc);

/* */

extern void gvrender_output_filename_job(GVC_t * gvc, char *name);
extern void gvrender_output_langname_job(GVC_t * gvc, char *name);
extern gvrender_job_t *gvrender_first_job(GVC_t * gvc);
extern gvrender_job_t *gvrender_next_job(GVC_t * gvc);
extern void gvrender_delete_jobs(GVC_t * gvc);

extern int gvrender_features(GVC_t * gvc);

/* */

extern void gvrender_reset(GVC_t * gvc);
extern void gvrender_begin_job(GVC_t * gvc, char **lib, point pages);
extern void gvrender_end_job(GVC_t * gvc);
extern void gvrender_begin_graph(GVC_t * gvc, graph_t * g, box bb,
				 point pb);
extern void gvrender_end_graph(GVC_t * gvc);
extern void gvrender_begin_page(GVC_t * gvc, point page, double scale,
				int rot, point offset);
extern void gvrender_end_page(GVC_t * gvc);
extern void gvrender_begin_layer(GVC_t * gvc, char *layerName, int n,
				 int nLayers);
extern void gvrender_end_layer(GVC_t * gvc);
extern void gvrender_begin_cluster(GVC_t * gvc, graph_t * sg);
extern void gvrender_end_cluster(GVC_t * gvc);
extern void gvrender_begin_nodes(GVC_t * gvc);
extern void gvrender_end_nodes(GVC_t * gvc);
extern void gvrender_begin_edges(GVC_t * gvc);
extern void gvrender_end_edges(GVC_t * gvc);
extern void gvrender_begin_node(GVC_t * gvc, node_t * n);
extern void gvrender_end_node(GVC_t * gvc);
extern void gvrender_begin_edge(GVC_t * gvc, edge_t * e);
extern void gvrender_end_edge(GVC_t * gvc);
extern void gvrender_begin_context(GVC_t * gvc);
extern void gvrender_end_context(GVC_t * gvc);
extern void gvrender_set_font(GVC_t * gvc, char *fontname,
			      double fontsize);
extern void gvrender_textline(GVC_t * gvc, point p, textline_t * str);
extern void gvrender_set_pencolor(GVC_t * gvc, char *name);
extern void gvrender_set_fillcolor(GVC_t * gvc, char *name);
extern void gvrender_set_style(GVC_t * gvc, char **s);
extern void gvrender_ellipse(GVC_t * gvc, point p, int rx, int ry,
			     int filled);
extern void gvrender_polygon(GVC_t * gvc, point * A, int n, int filled);
extern void gvrender_beziercurve(GVC_t * gvc, point * A, int n,
				 int arrow_at_start, int arrow_at_end);
extern void gvrender_polyline(GVC_t * gvc, point * A, int n);
extern void gvrender_comment(GVC_t * gvc, void *obj, attrsym_t * sym);
extern point gvrender_textsize(GVC_t * gvc, char *str, char *fontname,
			       double fontsz);
extern void gvrender_user_shape(GVC_t * gvc, char *name, point * A,
				int sides, int filled);
extern point gvrender_usershapesize(GVC_t * gvc, node_t * n, char *name);
#endif
