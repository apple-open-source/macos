/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "dynadag/DynaDAG.h"

namespace DynaDAG {

//#define DOUBLE_MEDIANS
#ifdef DOUBLE_MEDIANS
#define med_type double
#define cvt_med 
#define med_eq(m1,m2) (fabs(m1-m2)<0.25)
#else
#define med_type int
#define cvt_med ROUND
#define med_eq(m1,m2) (m1==m2)
#endif

struct MedianOptimizer {
	Config &config;
	UpDown m_dir;
	int m_strength;
	MedianOptimizer(Config &config) : config(config),m_dir(DOWN),m_strength(0) {}
	int improve(DDModel::Node *n) {
		Rank *rank = config.ranking.GetRank(DDd(n).rank);
		if(MValExists(n,m_dir)) {
			med_type nm = cvt_med(MVal(n,m_dir));
			// find something on the left with a median greater than n's
			// if m_strength==1, it need only have equal median
			// if m_strength==2, find the last thing >=
			int i;
			DDModel::Node *l = 0;
			for(i = DDd(n).order-1;i!=-1;--i) {
				DDModel::Node *v = rank->order[i];
				if(MValExists(v,m_dir)) {
					// is it a good (or okay) idea to move n there?
					med_type lm = cvt_med(MVal(v,m_dir));
					if(nm<lm || (med_eq(nm,lm) && m_strength)) {
						l = v;
						if(m_strength>1)
							continue;
					}
					break;
				}
			}
			if(l) 
				return DDd(l).order;
			// same for right, with lesser medians
			DDModel::Node *r = 0;
			for(i = DDd(n).order+1;i!=rank->order.size(); ++i) {
				DDModel::Node *v = rank->order[i];
				if(MValExists(v,m_dir)) {
					med_type rm = cvt_med(MVal(v,m_dir));
					if(nm>rm || (med_eq(nm,rm) && m_strength)) {
						r = v;
						if(m_strength>1)
							continue;
					}
					break;
				}
			}
			if(r) 
				return DDd(r).order+1;
		}
		return -1;
	}
};
struct SiftingOptimizer {
	Config &config;
	SiftMatrix matrix;
	int m_strength;
	SiftingOptimizer(Config &config) : config(config),matrix(config),m_strength(0) {}
	int improve(DDModel::Node *n) {
		int r = DDd(n).rank;
		Rank *rank = config.ranking.GetRank(r);
		int numcross=0,min=0,what=-1;
		int o;
		// look to right
		numcross = min = 0;
		for(o = DDd(n).order+1; o!=rank->order.size(); ++o) {
			DDModel::Node *x = rank->order[o];
			numcross += matrix.allCrossings(x,n)-matrix.allCrossings(n,x);
			if(numcross<min || (m_strength==1 && numcross==0 && what==-1) || (m_strength==2 && numcross==min)) {
				min = numcross;
				what = o+1;
			}
		}
		// look to left
		numcross = 0;
		for(o = DDd(n).order-1; o>=0; --o) {
			DDModel::Node *x = rank->order[o];
			numcross += matrix.allCrossings(n,x)-matrix.allCrossings(x,n);
			if(numcross<min || (m_strength==1 && numcross==0 && what==-1) || (m_strength==2 && numcross==min)) {
				min = numcross;
				what = o;
			}
		}
		return what;
	}
	void move(DDModel::Node *n,int where) {
		Rank *rank = config.ranking.GetRank(DDd(n).rank);
		matrix.move(n,where==rank->order.size()?0:rank->order[where]);
	}
};
template<class Iter,class Optimizer,class OptState>
void optim(Config &config,Iter begin,Iter end, Optimizer *opt, OptState *state) {
	for(Iter ni = begin; ni!=end; ++ni) {
		int where = opt->improve(*ni);
		if(where!=-1) {
			state->move(*ni,where);
			int r = DDd(*ni).rank,o = DDd(*ni).order;
			config.RemoveNode(*ni);
			config.InstallAtOrder(*ni,r,where>o?where-1:where);
		}
	}
}
struct RankLess {
	bool operator()(DDModel::Node *u,DDModel::Node *v) {
		if(DDd(u).rank == DDd(v).rank)
			return DDd(u).order < DDd(v).order;
		return DDd(u).rank < DDd(v).rank;
	}
};
#define TIRE 5
void HybridOptimizer::Reorder(Layout &nodes,Layout &edges) {
	NodeV optimOrder;
	getCrossoptModelNodes(nodes,edges,optimOrder);
	if(optimOrder.empty())
		return;
	sort(optimOrder.begin(),optimOrder.end(),RankLess());
	loops.Field(r_crossopt,"model nodes for crossopt",optimOrder.size());
	loops.Field(r_crossopt,"total model nodes",optimOrder.front()->g->nodes().size());
	MedianOptimizer median(config);
	SiftingOptimizer sifting(config);
	Config::Ranks backup = config.ranking;
	// optimize once ignoring node crossings (they can scare the sifting alg) 
	// in a sec we'll sift using the node penalty to clean up
	sifting.matrix.m_light = true;
	sifting.matrix.recompute();
	int best = sifting.matrix.total();
	loops.Field(r_crossopt,"crossings before crossopt",best);
	int passes = 24,pass=0,score;
	while(pass<passes && best) {
		int tired = 0;
		while(pass<passes && tired<TIRE) {
			median.m_strength = pass%4<2;
			sifting.m_strength = !median.m_strength;
			if(pass%2) {
				median.m_dir = UP;
				optim(config,optimOrder.begin(),optimOrder.end(),&median,&sifting);
				int s2 = sifting.matrix.total();
				do {
					score = s2;
					optim(config,optimOrder.begin(),optimOrder.end(),&sifting,&sifting);
				}
				while((s2 = sifting.matrix.total())<score);
			}
			else {
				median.m_dir = DOWN;
				optim(config,optimOrder.rbegin(),optimOrder.rend(),&median,&sifting);
				int s2 = sifting.matrix.total();
				do {
					score = s2;
					optim(config,optimOrder.rbegin(),optimOrder.rend(),&sifting,&sifting);
				}
				while((s2 = sifting.matrix.total())<score);
			}
			score = sifting.matrix.total();
			if(reportEnabled(r_crossopt)) {
				char buf[10];
				sprintf(buf,"crossings pass %d",pass);
				loops.Field(r_crossopt,buf,score);
			}
			if(score<best) {
				backup = config.ranking;
				best = score;
				tired = 0;
			}
			else 
				tired++;
			pass++;
		}
		if(score>best || tired==TIRE) {
			config.Restore(backup);
			sifting.matrix.recompute();
		}
	}
	if(score>=best) 
		config.Restore(backup);
	loops.Field(r_crossopt,"model edge crossings",best);

	// sift out node crossings
	sifting.matrix.m_light = false;
	sifting.matrix.recompute();
	score = sifting.matrix.total();
	loops.Field(r_crossopt,"weighted crossings before heavy pass",score);
	if(score<NODECROSS_PENALTY) {
		loops.Field(r_crossopt,"weighted crossings after heavy pass",-1);
		return;
	}
	pass = 0;
	passes = 4;
	sifting.m_strength = 1;
	// sifting out upward or downward may be better - try both.
	bool improved;
	do {
		improved = false;
		backup = config.ranking;
		optim(config,optimOrder.begin(),optimOrder.end(),&sifting,&sifting);
		int down = sifting.matrix.total();
		Config::Ranks backup2 = config.ranking;
		config.Restore(backup);
		sifting.matrix.recompute();
		optim(config,optimOrder.rbegin(),optimOrder.rend(),&sifting,&sifting);
		int up = sifting.matrix.total();
		if(down<score && down<up) {
			config.Restore(backup2);
			sifting.matrix.recompute();
			score = down;
			improved = true;
		}
		else if(up<score) {
			score = up;
			improved = true;
		}
	}
	while(improved);
	loops.Field(r_crossopt,"weighted crossings after heavy pass",score);
	// absolutely must not leave here with nodes crossing nodes!!
	assert(score<NODECROSS_PENALTY*NODECROSS_PENALTY);
}
double HybridOptimizer::Reopt(DDModel::Node *n,UpDown dir) {
	return DDd(n).cur.x;
}

} // namespace DynaDAG
