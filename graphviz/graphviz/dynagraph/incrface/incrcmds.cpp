/*   Copyright (c) AT&T Corp.  All rights reserved.

This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "incrface/incrcmds.h"
#include "common/stringsIn.h"
#include "common/stringsOut.h"
#include "common/ag2str.h"
//#include "common/bufferGraphStream.h"
#include "incrface/incrxep.h"

using namespace std;


#define YORIGIN 0

Transform *g_transform = 0;
bool g_dotCoords = false;
static StrAttrs g_currAttrs;
static Coord Origin;
Views g_views;

// must implement this
// e.g. in dynagraph/main.cpp and comdg/Engine.cpp
extern View *createView(Name name);

//extern "C" {

static void maybe_go(View *v,const char *view) 
{
    if(v->locks<=0) {
        while(!v->Q.Empty()) {
            v->dgserver->Process(v->Q);
            stringsOut(g_transform,v->Q);
            v->IncrHappened(); // must clear Q but the events might cause more changes
        }
    }
}

void incr_message(const char *msg) 
{
    // pass through
    cout << "message \"" << msg <<"\"\n";
}
void incr_open_view(const char *view)
{
    View *v = g_views[view];
    if(v) {
        if(v->allowAnOpen)
            v->allowAnOpen = false;
        else {
            incr_error(IF_ERR_ALREADY_OPEN,view);
            incr_close_view(view);
        }
    }
    v = createView(view);
    if(!v) {
        fprintf(stderr,"dg: view %s rejected.\n",view);
        return;
    }
    stringsIn(g_transform,&v->current,g_currAttrs,false);
    v->createServer();
    if(!v->dgserver) {
        fprintf(stderr,"error: unknown engine\n");
        delete v;
        return;
    }

    g_views[view] = v;
    fprintf(stdout,"open view %s\n",view);
}

void incr_close_view(const char *view)
{
    View *v = g_views[view];
    if(v) {
        g_views.erase(view);
        delete v;
    }
    else incr_error(IF_ERR_NOT_OPEN,view);
    fprintf(stdout,"close view %s\n",view);
}

void incr_mod_view(const char *view) {
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    // special case: if "engines" was changed, re-create server
    StrAttrs::iterator ai = g_currAttrs.find("engines");
    if(ai!=g_currAttrs.end())
        if(ai->second!=gd<StrAttrs>(&v->current)["engines"])
            v->createServer();
    v->Q.GraphUpdateFlags() |= stringsIn(g_transform,&v->layout,g_currAttrs,false).flags;
    maybe_go(v,view);
}

void incr_lock(const char *view) {
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    v->locks++;
}

void incr_unlock(const char *view) {
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    --v->locks;
    maybe_go(v,view);
}

void incr_load_strgraph(View *v,StrGraph *sg,bool merge, bool del) {
    typedef map<DString,StrGraph::Node*> strnode_dict;
    if(merge && del) { // find deletions
        strnode_dict nd;
        for(StrGraph::node_iter sni = sg->nodes().begin(); sni!=sg->nodes().end(); ++sni)
            nd[gd<Name>(*sni)] = *sni;
        for(Layout::node_iter ni = v->current.nodes().begin(); ni!=v->current.nodes().end(); ++ni)
            if(nd.find(gd<Name>(*ni))==nd.end()) {
                v->Q.DelNode(*ni);
                v->forget(*ni);
                for(Layout::nodeedge_iter ei = (*ni)->alledges().begin(); ei!=(*ni)->alledges().end(); ++ei)
                    v->forget(*ei);
            }
            for(Layout::graphedge_iter ei = v->current.edges().begin(); ei!=v->current.edges().end(); ++ei) {
                // find edges by head and tail because prob has no name.
                StrGraph::Node *t=0,*h=0;
                strnode_dict::iterator di = nd.find(gd<Name>((*ei)->tail));
                if(di!=nd.end()) {
                    t = di->second;
                    di = nd.find(gd<Name>((*ei)->head));
                    if(di!=nd.end()) {
                        h = di->second;
                        if(!sg->find_edge(t,h)) {
                            v->Q.DelEdge(*ei);
                            v->forget(*ei);
                        }
                    }
                }
                // if tail or head missing we've already called DelNode thus don't need to call DelEdge
            }
    }
    { // find insertions & modifications
        map<DString,Layout::Node*> nd; // an override dictionary, only if !merge
        for(StrGraph::node_iter ni = sg->nodes().begin(); ni!=sg->nodes().end(); ++ni) {
            DString id = gd<Name>(*ni);
            pair<Layout::Node *,bool> nb = v->getNode(id,true);
            if(!nb.second&&!merge) { 
                // name already was used, so make anónimo and label with the name
                nb.second = true;
                nb.first = v->getNode(0,true).first;
                gd<StrAttrs>(nb.first)["label"] = id;
                nd[id] = nb.first;
            }
            Update upd = stringsIn(g_transform,nb.first,gd<StrAttrs>(*ni),true);
            if(nb.second) {
                v->Q.InsNode(nb.first);
                v->IncrNewNode(nb.first);
            }
            else
                v->Q.ModNode(nb.first,upd);
        }
        for(StrGraph::graphedge_iter ei = sg->edges().begin(); ei!=sg->edges().end(); ++ei) {
            DString id = gd<Name>(*ei),
                t = gd<Name>((*ei)->tail),
                h = gd<Name>((*ei)->head);
            if(View::poorEdgeName(id.c_str()))
                id = 0;
            Layout::Node *tn=0,*hn=0;
            if(!merge) {
                tn = nd[t];
                hn = nd[h];
            }
            if(!tn)
                tn = v->nodes[t];
            if(!hn)
                hn = v->nodes[h];
            assert(tn&&hn);
            if(v->layout.find_edge(hn,tn))
                continue; // can't yet deal with 2-cycles
            pair<Layout::Edge*,bool> eb = v->getEdge(id,tn,hn,true);
            Update upd = stringsIn(g_transform,eb.first,gd<StrAttrs>(*ei),true);
            if(eb.second) {
                v->Q.InsEdge(eb.first);
                v->IncrNewEdge(eb.first);
            }
            else
                v->Q.ModEdge(eb.first,upd);
        }
    }
}
extern FILE *incr_yyin;
extern "C" void ag_yyrestart(FILE *);
void incr_segue(const char *view) {
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
    }
    //bufferGraphStream fix(incr_yyin);
    //assert(!feof(fix.fin));
    StrGraph *sg = readStrGraph(incr_yyin);
    /*
    assert(!feof(fix.fin));
    char buf[200];
    fgets(buf,200,fix.fin);
    */
    if(!sg) 
        fprintf(stderr,"graph read error\n");
    else {	
        v->locks++;
        incr_load_strgraph(v,sg,true,true);
        v->locks--;
        maybe_go(v,view);
    }
}

void incr_ins_node(const char *view,const char *id)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    pair<Layout::Node*,bool> nb = v->getNode(id,true);
    Update upd = stringsIn(g_transform,nb.first,g_currAttrs,true);
    if(nb.second) {
        v->IncrNewNode(nb.first);
        v->Q.InsNode(nb.first);
    }
    else // treat re-insert as modify
        v->Q.ModNode(nb.first,upd);
    maybe_go(v,view);
}

void incr_ins_edge(const char *view,const char *id, const char *tail, const char *head)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    pair<Layout::Edge*,bool> eb = v->getEdge(id,tail,head,true);
    if(!eb.second && gd<Name>(eb.first)!=id) {
        incr_error(IF_ERR_NAME_MISMATCH,view);
        return;
    }
    Update upd = stringsIn(g_transform,eb.first,g_currAttrs,true);
    if(eb.second) {
        v->IncrNewEdge(eb.first);
        v->Q.InsEdge(eb.first);
    }
    else 
        v->Q.ModEdge(eb.first,upd);
    maybe_go(v,view);
}

void incr_mod_node(const char *view,const char *id)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    Layout::Node *n = v->getNode(id);
    if(!n) {
        incr_error(IF_ERR_OBJECT_DOESNT_EXIST,id);
        return;
    }
    v->Q.ModNode(n,stringsIn(g_transform,n,g_currAttrs,false));
    maybe_go(v,view);
}

void incr_mod_edge(const char *view,const char *id)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    Layout::Edge *e = v->getEdge(id);
    if(!e) {
        incr_error(IF_ERR_OBJECT_DOESNT_EXIST,id);
        return;
    }
    v->Q.ModEdge(e,stringsIn(g_transform,e,g_currAttrs,false));
    maybe_go(v,view);
}

void incr_del_node(const char *view,const char *id)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    Layout::Node *n = v->getNode(id);
    if(!n) {
        incr_error(IF_ERR_OBJECT_DOESNT_EXIST,id);
        return;
    }
    v->Q.DelNode(n);
    v->forget(n);
    maybe_go(v,view);
}

void incr_del_edge(const char *view,const char *id)
{
    View *v = g_views[view];
    if(!v) {
        incr_error(IF_ERR_NOT_OPEN,view);
        return;
    }
    Layout::Edge *e = v->getEdge(id);
    if(!e) {
        incr_error(IF_ERR_OBJECT_DOESNT_EXIST,id);
        return;
    }
    v->Q.DelEdge(e);
    v->forget(e);
    maybe_go(v,view);
}

/* attribute scanning */

void incr_reset_attrs()
{
    g_currAttrs.clear();
}

void incr_append_attr(const char *name, const char *value)
{
    g_currAttrs[name] = value;
}


/* error handler */

static const char *ErrMsg[] = {
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

void incr_error(int code, const char *str)
{
    if (!str) str = "";
    char buf[300];
    sprintf(buf,"incr interface: %s %s\n",ErrMsg[code],str);
    throw IncrError(buf);
}

void incr_abort(int code)
{
    for(Views::iterator vi = g_views.begin(); vi!=g_views.end(); ++vi)
        if(vi->second)
            delete vi->second;
    g_views.clear();
    incr_error(code,0);
}

//}
