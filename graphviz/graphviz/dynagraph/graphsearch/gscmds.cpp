/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "common/Dynagraph.h"
#include "common/ag2str.h"
#include "common/emitGraph.h"
#include "common/bufferGraphStream.h"
#include "common/diff_strgraph.h"

#include "graphsearch/gscmds.h"
#include "graphsearch/Search.h"

using namespace GSearch;
using namespace std;

static StrAttrs g_currAttrs;
extern StrGraph *g_sourceGraph;

Patterns g_patterns;

typedef map<DString,Search*> Searches;
Searches g_searches;

Inputs g_inputs;

struct Session {
	DString searchname;
	Search *search;
	int locks; 
    bool changed;
	Session() : search(0),locks(0),changed(false) {}
};
typedef map<DString,Session> Sessions;
Sessions g_sessions;

bool assign_search(Session &s) {
	StrAttrs::iterator ai;
	ai = g_currAttrs.find("searchname");
	if(ai==g_currAttrs.end())
		return false;
	Searches::iterator si = g_searches.find(ai->second);
	if(si==g_searches.end())
		throw GSError();
	if(s.search)
		delete s.search;
	s.searchname = si->first;
	// copy the search because it has state
	s.search = new Search(*si->second);
	return true;
}

StrGraph &result(Session &s) {
	Search::Node *finish = s.search->dict["finish"];
	if(!finish)
		throw GSError();
	return gd<SearchStage>(finish).result;
}
struct emitReact {
	const char *view;
	void ins(StrGraph::Node *n) {
		cout << "insert " << mquote(view) << " node " << mquote(gd<Name>(n).c_str()) << " ";
		emitAttrs(cout,gd<StrAttrs>(n));
	}
	void mod(StrGraph::Node *n,StrAttrs *attrs) {
		cout << "modify " << mquote(view) << " node " << mquote(gd<Name>(n).c_str()) << " ";
		emitAttrs(cout,*attrs);
	}
	void del(StrGraph::Node *n) {
		cout << "delete " << mquote(view) << " node " << mquote(gd<Name>(n).c_str()) << endl;
	}
	void ins(StrGraph::Edge *e) {
		cout << "insert " << mquote(view) << " edge " << mquote(gd<Name>(e).c_str()) << 
            " " << mquote(gd<Name>(e->tail)) << " " << mquote(gd<Name>(e->head)) << " ";
		emitAttrs(cout,gd<StrAttrs>(e));
	}
	void del(StrGraph::Edge *e) {
		cout << "delete " << mquote(view) << " edge " << mquote(gd<Name>(e).c_str()) << "\n";
	}
	void mod(StrGraph::Edge *e,StrAttrs *attrs) {
		cout << "modify " << mquote(view) << " edge " << mquote(gd<Name>(e).c_str()) << " ";
		emitAttrs(cout,*attrs);
	}
};
void output_result(const char *view,StrGraph &start,Session &s) {
	StrGraph &finish = result(s);
	start.oopsRefreshDictionary(); // ecch.
	finish.oopsRefreshDictionary();
	emitReact r;
	r.view = view;
	cout << "lock view " << mquote(view) << endl;
	diff_strgraph(&start,&finish,r);
	cout << "unlock view " << mquote(view) << endl;
	/*
	cout << "segue view " << mquote(view) << endl;
	cout.flush();
	emitGraph(cout,&gd<SearchStage>(finish).result);
	*/
}
void run(Session &s,const char *view) {
    if(!s.locks) {
	    StrGraph before = result(s);
	    s.search->Run(g_inputs);
	    output_result(view,before,s);
    }
    else
        s.changed = true;
}
void gs_open_view(char *view)
{
	Session &s = g_sessions[view];
	if(s.search) {
		gs_error(IF_ERR_ALREADY_OPEN,view);
		gs_close_view(view);
	}
	bool hasSearch = assign_search(s);
	cout << "open view " << mquote(view) << ' ';
	emitAttrs(cout,g_currAttrs);
	if(hasSearch) 
        run(s,view);
}

void gs_close_view(char *view)
{
	Session &s = g_sessions[view];
	if(s.search) {
		delete s.search;
		g_sessions.erase(view);
	}
	else gs_error(IF_ERR_NOT_OPEN,view);
	cout << "close view " << mquote(view) << endl;
}

void gs_mod_view(char *view) {
	Session &s = g_sessions[view];
    if(!s.locks) {
	    cout.flush();
	    cout << "modify view " << mquote(view) << ' ';
	    emitAttrs(cout,g_currAttrs);
    }
	if(assign_search(s)) 
        run(s,view);
}


void gs_lock(char *view) {
	Session &s = g_sessions[view];
	s.locks++;
}
void gs_unlock(char *view) {
	Session &s = g_sessions[view];
	if(!--s.locks && s.changed)
        run(s,view);
}

void gs_segue(char *view) {
	throw NYI();
}
void search_changed(DString sname,bool recopy) {
	Search &search = *g_searches[sname];
	for(Sessions::iterator si = g_sessions.begin(); si!=g_sessions.end(); ++si) {
		Session &session = si->second;
		if(session.searchname==sname) {
			if(recopy) {
				delete session.search;
				session.search = new Search(search);
			}
			else session.search->reset();
            run(session,si->first.c_str());
		}
	}
}
extern FILE *gs_yyin;
void gs_define_pattern() {
	bufferGraphStream fix(gs_yyin);
	StrGraph *sg = readStrGraph(fix.fin);
	if(!sg) 
		throw ParseError();
	Pattern &pattern = g_patterns[gd<Name>(sg)];
	pattern.readStrGraph(*sg);
	for(Searches::iterator si = g_searches.begin(); si!=g_searches.end(); ++si)
        for(Search::node_iter sti = si->second->nodes().begin(); sti!=si->second->nodes().end(); ++sti) {
	  //Search::Node *stage = *sti;
			if(gd<SearchStage>(*sti).pattern==&pattern)
				search_changed(si->first,false);
        }
	delete sg;
}
void gs_define_search() {
	bufferGraphStream fix(gs_yyin);
	StrGraph *sg = readStrGraph(fix.fin);
	if(!sg)
		throw ParseError();
	Name name = gd<Name>(sg);
	Search *&search = g_searches[name];
	if(search==0)
		search = new Search(*g_sourceGraph);
	try {
		search->readStrGraph(g_patterns,*sg);
		search_changed(name,true);
	}
	catch(UndefinedPattern up) {
		fprintf(stderr,"undefined pattern %s\n",up.name.c_str());
	}
	delete sg;
}
void gs_define_input() {
	bufferGraphStream fix(gs_yyin);
	StrGraph *sg = readStrGraph(fix.fin);
	if(!sg)
		throw ParseError();
	StrGraph *input = new StrGraph(g_sourceGraph);
	input->readSubgraph(sg);
    g_inputs[gd<Name>(sg)] = input;
    for(Sessions::iterator si = g_sessions.begin(); si!=g_sessions.end(); ++si) {
        Session &s = si->second;
        if(!s.search)
            continue;
		StrGraph before = result(s);
		s.search->reset();
		s.search->Run(g_inputs);
		output_result(si->first.c_str(),before,s);
	}
	delete sg;
}
void gs_ins_node(char *view,char *id)
{
	throw NYI();
}

void gs_mod_node(char *view,char *id)
{
	throw NYI();
}

void gs_del_node(char *view,char *id)
{
	throw NYI();
}

void gs_ins_edge(char *view,char *id, char *tail, char *head)
{
	throw NYI();
}

void gs_mod_edge(char *view,char *id)
{
	throw NYI();
}

void gs_del_edge(char *view,char *id)
{
	throw NYI();
}

/* attribute scanning */

void gs_reset_attrs()
{
	g_currAttrs.clear();
}

void gs_append_attr(char *name, char *value)
{
	g_currAttrs[name] = value;
}


/* error handler */

static char *ErrMsg[] = {
	"unknown error",
	"graph/view already open",
	"graph/view not open",
	"name mismatch",
	"syntax error",
	"duplicated ID (node/edge)",
	"not implemented",
	"object doesn't exist",
	(char*)0
} ;
void gs_error(int code, char *str)
{
	if (!str) str = "";
	fprintf(stderr,"graphsearch interface: %s %s\n",ErrMsg[code],str);
	throw GSError();
}

void gs_abort(int code)
{
	g_sessions.clear();
	gs_error(code,0);
}

