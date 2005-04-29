/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include "common/Dynagraph.h"
#include "incrface/View.h"

void incr_message(const char *msg);
void incr_begin_session(const char *id);
void incr_end_session(const char *id);
void incr_open_view(const char *id);
void incr_close_view(const char *id);
void incr_lock(const char *id);
void incr_unlock(const char *id);
void incr_segue(const char *id);
void incr_load_strgraph(View *v,StrGraph *sg,bool merge,bool doDeletes);
void incr_mod_view(const char *id);
void incr_ins_node(const char *view,const char *id);
void incr_mod_node(const char *view,const char *id);
void incr_del_node(const char *view,const char *id);
void incr_ins_edge(const char *view,const char *id, const char *tail, const char *head);
void incr_mod_edge(const char *view,const char *id);
void incr_del_edge(const char *view,const char *id);

void incr_view_obj(const char *vid, const char *objid);
void incr_unview_obj(const char *vid, const char *objid);

void incr_abort(int code);
void incr_reset_attrs();
void incr_append_attr(const char *name, const char *value);
const char *incr_get_attr(const char *name);
void incr_error(int code, const char *msg);
void incr_lexeof();

// in incrscan
void incr_yyerror(const char *str);
int incr_yyparse(void);
int incr_yylex(void);

#define IF_ERR_UNKNOWN			0
#define IF_ERR_ALREADY_OPEN		1
#define IF_ERR_NOT_OPEN			2
#define IF_ERR_NAME_MISMATCH	3
#define IF_ERR_SYNTAX			4
#define IF_ERR_DUPLICATE_ID		5
#define IF_ERR_NOT_IMPLEMENTED	6
#define IF_ERR_OBJECT_DOESNT_EXIST 7
#define IF_MAX_ERR				8

#define IF_MAXATTR				128

#ifdef offsetof
#undef offsetof
#endif
#define offsetof(typ,fld)  ((int)(&(((typ*)0)->fld)))
#ifndef streq
#define streq(s,t) (!strcmp(s,t))
#endif
