#include "common/Dynagraph.h"
#include "View.h"

using namespace std;

View::View(Name name) : 
	current(&layout),
    old(&layout),
	Q(&layout,&current),
	dgserver(0),
	locks(0),
	allowAnOpen(false) {
	if(name.empty())
		name = randomName('v');
	gd<Name>(&layout) = name;
	g_views[name] = this;
}
View::~View() {
	if(dgserver)
		delete dgserver;
}
void View::createServer() {
    if(dgserver) {
        // replace an engine
        delete dgserver;
        // feed the current graph into insertion queues & remove dead links to old engine
        for(Layout::node_iter ni = current.nodes().begin(); ni!=current.nodes().end(); ++ni) {
            gd<ModelPointer>(*ni).model = 0;
            Q.InsNode(*ni);
        }
        for(Layout::graphedge_iter ei = current.edges().begin(); ei!=current.edges().end(); ++ei) {
            gd<ModelPointer>(*ei).model = 0;
            Q.InsEdge(*ei);
        }
        old = current; // remember what's not actually new for completeReplacement
        current.clear();
    }
	dgserver = createLayoutServer(&layout,&current);
}
void View::completeReplacement() {
    // this is kinda weird: mark all that were re-inserted into new graph "moved" 
    // (although there may be other changes)
    for(Layout::node_iter ni = old.nodes().begin(); ni!=old.nodes().end(); ++ni) {
        Q.insN.erase(*ni);
        Q.ModNode(*ni,DG_UPD_MOVE);
    }
    for(Layout::graphedge_iter ei = old.edges().begin(); ei!=old.edges().end(); ++ei) {
        Q.insE.erase(*ei);
        Q.ModEdge(*ei,DG_UPD_MOVE);
    }
    old.clear();
}
// tricky problem: if client doesn't specify a name, we need to generate one
// can't use e.g. e17 because the client might already have something named that!
// so generate a random name
DString View::randomName(char prefix) {
	char buf[10];
	unsigned char ch, i=2;
	buf[0] = prefix;
	buf[1] = '_';
	while(i<8) 
		if(isalnum(ch = rand()%256))
			buf[i++] = ch;
	buf[8] = 0;
	return buf;
}
// to detect those bad names that agraph is generating
bool View::poorEdgeName(const char *name) {
	if(!name[0])
		return true;
	if(name[0]!='e')
		return false;
	if(!name[1])
		return false; // "e" okay i guess
	for(int i=1;name[i]; ++i)
		if(!isdigit(name[i]))
			return false;
	return true;
}
pair<Layout::Node*,bool> View::getNode(DString id,bool create) {
	if(id.empty())
		id = randomName('n');
	Layout::Node *n = nodes[id];
	if(n)
		return make_pair(n,false);
	if(!create) 
		return make_pair<Layout::Node*>(0,false);
    NodeAttrs *NA = new NodeAttrs();
	n = layout.create_node(*NA);
    delete NA;
	gd<Name>(n) = id;
	nodes[id] = n;
	return make_pair(n,true);
}
Layout::Node *View::getNode(DString id) {
	return getNode(id,false).first;
}
pair<Layout::Edge*,bool> View::getEdge(DString id,Layout::Node *t,Layout::Node *h,bool create) {
	Layout::Edge *e=0;
	if(id.empty())
		id = randomName('e');
	else if((e = edges[id]))
		return make_pair(e,false);
	if(!(t && h))
		return make_pair<Layout::Edge*>(0,false);
	e = layout.find_edge(t,h);
	if(e)
		return make_pair(e,false);
	if(!create)
		return make_pair<Layout::Edge*>(0,false);
	e = layout.create_edge(t,h).first;
	gd<Name>(e) = id;
	edges[id] = e;
	return make_pair(e,true);
}
pair<Layout::Edge*,bool> View::getEdge(DString id,DString tail,DString head,bool create) {
	assert(tail.size()&&head.size());
	Layout::Node *t = getNode(tail,false).first,
		*h = getNode(head,false).first;
	return getEdge(id,t,h,create);
}
Layout::Edge *View::getEdge(DString id,DString tail,DString head) {
	return getEdge(id,tail,head,false).first;
}
Layout::Edge *View::getEdge(DString id) {
	return getEdge(id,(Layout::Node*)0,(Layout::Node*)0,false).first;
}
void View::rename(Layout *l,DString newName) {
	assert(l==&layout);
	g_views.erase(gd<Name>(&layout));
	g_views[gd<Name>(&layout) = newName] = this;
}
void View::rename(Layout::Node *n,DString newName) {
	forget(n);
	nodes[gd<Name>(n) = newName] = n;
}
void View::rename(Layout::Edge *e,DString newName) {
	forget(e);
	edges[gd<Name>(e) = newName] = e;
}
void View::forget(Layout::Node *n) {
	nodes.erase(gd<Name>(n));
}
void View::forget(Layout::Edge *e) {
	edges.erase(gd<Name>(e));
}
void View::destroy(Layout::Node *n) {
	forget(n);
	layout.erase(n);
}
void View::destroy(Layout::Edge *e) {
	forget(e);
	layout.erase(e);
}
