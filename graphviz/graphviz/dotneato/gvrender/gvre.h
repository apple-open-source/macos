/* $Id: gvre.h,v 1.1 2004/05/04 21:50:47 yakimoto Exp $ $Revision: 1.1 $ */
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

/* This is the public header for the libgvre_* plugins */

#ifndef GVRE_H
#define GVRE_H

#include "gvrenderint.h"

struct gvrender_engine_s {
    int features;
    void (*reset) (GVC_t *);
    void (*begin_job) (GVC_t *);
    void (*end_job) (GVC_t *);
    void (*begin_graph) (GVC_t *);
    void (*end_graph) (GVC_t *);
    void (*begin_page) (GVC_t *);
    void (*end_page) (GVC_t *);
    void (*begin_layer) (GVC_t *);
    void (*end_layer) (GVC_t *);
    void (*begin_cluster) (GVC_t *);
    void (*end_cluster) (GVC_t *);
    void (*begin_nodes) (GVC_t *);
    void (*end_nodes) (GVC_t *);
    void (*begin_edges) (GVC_t *);
    void (*end_edges) (GVC_t *);
    void (*begin_node) (GVC_t *);
    void (*end_node) (GVC_t *);
    void (*begin_edge) (GVC_t *);
    void (*end_edge) (GVC_t *);
    void (*begin_context) (GVC_t *);
    void (*end_context) (GVC_t *);
    void (*set_font) (GVC_t *, char *fontname, double fontsize);
    void (*textline) (GVC_t *, pointf p, textline_t * str);
    void (*set_pencolor) (GVC_t *, char *color);
    void (*set_fillcolor) (GVC_t *, char *color);
    void (*set_style) (GVC_t *, char **s);
    void (*ellipse) (GVC_t *, pointf p, double rx, double ry, int filled);
    void (*polygon) (GVC_t *, pointf * A, int n, int filled);
    void (*beziercurve) (GVC_t *, pointf * A, int n, int arrow_at_start,
			 int arrow_at_end);
    void (*polyline) (GVC_t *, pointf * A, int n);
    void (*comment) (GVC_t *, void *obj, attrsym_t * sym);
     point(*textsize) (GVC_t *, char *str, char *fontname, double fontsz);
    void (*user_shape) (GVC_t *, char *name, pointf * A, int sides,
			int filled);
     point(*usershapesize) (GVC_t *, node_t * n, char *name);
};

#endif
