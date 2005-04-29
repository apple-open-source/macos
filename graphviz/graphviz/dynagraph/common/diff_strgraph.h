/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

StrAttrs *diff_attr(StrAttrs &a1,StrAttrs &a2) {
	StrAttrs *ret = 0;
	for(StrAttrs::iterator i1 = a1.begin(),i2 = a2.begin(); i1!=a1.end() || i2!=a2.end();)
		if(i1->first==i2->first) { // attr in both
			if(i1->second!=i2->second) { 
				if(!ret)
					ret = new StrAttrs;
				(*ret)[i2->first] = i2->second;
			}
			i1++;
			i2++;
		}
		else if(i1->first<i2->first) {// attr only in first
			if(!ret)
				ret = new StrAttrs;
			(*ret)[i1->first] = 0;
			i1++;
		}
		else { // attr only in second
			if(!ret)
				ret = new StrAttrs;
			(*ret)[i2->first] = i2->second;
			i2++;
		}
	return ret;
}
template<typename React>
void diff_strgraph(StrGraph *sg1,StrGraph *sg2,React &react) {
	{ // find deletions
		for(StrGraph::node_iter ni1 = sg1->nodes().begin(); ni1!=sg1->nodes().end(); ++ni1)
			if(sg2->dict.find(gd<Name>(*ni1))==sg2->dict.end()) {
				for(StrGraph::nodeedge_iter ei1 = (*ni1)->alledges().begin(); ei1!=(*ni1)->alledges().end(); ++ei1)
					// if both end nodes gone, delete edge only once
					if((*ei1)->tail==*ni1 || sg2->dict.find(gd<Name>((*ei1)->other(*ni1)))!=sg2->dict.end())
						react.del(*ei1);
				react.del(*ni1);
			}
		for(StrGraph::graphedge_iter ei1 = sg1->edges().begin(); ei1!=sg1->edges().end(); ++ei1) {
			// find edges by head and tail because prob has no name.
			StrGraph::Node *t2=0,*h2=0;
			StrGraph::Dict::iterator di2 = sg2->dict.find(gd<Name>((*ei1)->tail));
			if(di2!=sg2->dict.end()) {
				t2 = di2->second;
				di2 = sg2->dict.find(gd<Name>((*ei1)->head));
				if(di2!=sg2->dict.end()) {
					h2 = di2->second;
					if(!sg2->find_edge(t2,h2)) 
						react.del(*ei1);
				}
			}
			// if tail or head missing we've already called DelNode thus don't need to call DelEdge
		}
	}
	{ // find insertions & modifications
		for(StrGraph::node_iter ni2 = sg2->nodes().begin(); ni2!=sg2->nodes().end(); ++ni2) {
			StrGraph::Dict::iterator di1 = sg1->dict.find(gd<Name>(*ni2));
			if(di1!=sg1->dict.end()) {
				if(StrAttrs *diff = diff_attr(gd<StrAttrs>(di1->second),gd<StrAttrs>(*ni2))) {
					react.mod(*ni2,diff);
					delete diff;
				}
			}
			else
				react.ins(*ni2);
		}
		for(StrGraph::graphedge_iter ei2 = sg2->edges().begin(); ei2!=sg2->edges().end(); ++ei2) {
			DString id = gd<Name>(*ei2),
				t = gd<Name>((*ei2)->tail),
				h = gd<Name>((*ei2)->head);
			StrGraph::Dict::iterator di1 = sg1->dict.find(t);
			if(di1==sg1->dict.end()) {
				react.ins(*ei2);
				continue;
			}
			StrGraph::Node *t1 = di1->second;
			di1 = sg1->dict.find(h);
			if(di1==sg1->dict.end()) {
				react.ins(*ei2);
				continue;
			}
			StrGraph::Node *h1 = di1->second;
			if(StrGraph::Edge *e = sg1->find_edge(t1,h1)) {
				if(StrAttrs *diff = diff_attr(gd<StrAttrs>(e),gd<StrAttrs>(*ei2))) {
					react.mod(*ei2,diff);
					delete diff;
				}
			}
			else react.ins(*ei2);
		}
	}
}
