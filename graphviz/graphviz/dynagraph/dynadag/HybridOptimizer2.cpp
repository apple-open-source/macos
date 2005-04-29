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

struct OrderConstraintSwitchable {
	bool canSwitch(DDModel::Node *l,DDModel::Node *r) {
		return DDd(l).orderConstraint<0 || DDd(r).orderConstraint<0 ||
			DDd(l).orderConstraint > DDd(r).orderConstraint;
	}
};
struct MedianCompare {
	UpDown m_dir;
	bool m_allowEqual;
	MedianCompare(UpDown dir,bool allowEqual) : m_dir(dir),m_allowEqual(allowEqual) {}
	bool comparable(DDModel::Node *n) {
		return MValExists(n,m_dir);
	}
	bool shouldSwitch(DDModel::Node *l,DDModel::Node *r) {
		med_type lm = cvt_med(MVal(l,m_dir)),
			rm = cvt_med(MVal(r,m_dir));
		return lm > rm || lm==rm && m_allowEqual;
	}
};
struct CrossingCompare {
	Config &config;
	SiftMatrix &matrix;
	bool m_allowEqual;
	CrossingCompare(Config &config,SiftMatrix &matrix,bool allowEqual) : config(config),matrix(matrix),m_allowEqual(allowEqual) {}
	bool comparable(DDModel::Node *n) {
		return true;
	}
	bool shouldSwitch(DDModel::Node *l,DDModel::Node *r) {
		assert(DDd(l).rank==DDd(r).rank);
		assert(DDd(l).order<DDd(r).order);
		Rank *rank = config.ranking.GetRank(DDd(l).rank);
		int numcross=0;
		for(int o = DDd(l).order; o<=DDd(r).order; ++o)
			numcross += matrix.allCrossings(rank->order[o],l) - matrix.allCrossings(l,rank->order[o]);
		return numcross<0 || numcross==0&&m_allowEqual;
	}
};
void moveBefore(Config &config,SiftMatrix &matrix,DDModel::Node *n,DDModel::Node *before) {
	matrix.move(n,before);
	int rank = DDd(n).rank;
	config.RemoveNode(n);
	if(before)
		config.InstallAtOrder(n,rank,DDd(before).order);
	else
		config.InstallAtRight(n,rank);
}
template<class Switchable, class Compare>
void bubblePassR(Config &config,SiftMatrix &matrix,Rank *r,Switchable &switchable,Compare &compare) {
	NodeV::iterator li;
	for(li = r->order.begin(); li<r->order.end()-1; ++li) {
		if(!compare.comparable(*li))
			continue;
		// search to right: if you find something you can't switch, go on to next li
		// if you find something to switch, jump to place it's been put
		for(NodeV::iterator ri = li+1;ri!=r->order.end(); ++ri)
			if(!switchable.canSwitch(*li,*ri))
				break;
			else if(compare.comparable(*ri)) {
				if(compare.shouldSwitch(*li,*ri)) {
					moveBefore(config,matrix,*li,(ri==r->order.end()-1)?0:*(ri+1));
					li = ri;
				}
				break;
			}
	}
}
template<class Switchable, class Compare>
void bubblePassL(Config &config,SiftMatrix &matrix,Rank *r,Switchable &switchable,Compare &compare) {
	NodeV::reverse_iterator ri;
	for(ri = r->order.rbegin(); ri<r->order.rend()-1; ++ri) {
		if(!compare.comparable(*ri))
			continue;
		for(NodeV::reverse_iterator li = ri+1;li!=r->order.rend(); ++li)
			if(!switchable.canSwitch(*li,*ri))
				break;
			else if(compare.comparable(*li)) {
				if(compare.shouldSwitch(*li,*ri)) {
					moveBefore(config,matrix,*ri,*li);
					ri = li-1;
				}
				break;
			}
	}
}
template<class Switchable, class Compare>
void bubblePass(Config &config,SiftMatrix &matrix,const vector<int> &ranks,UpDown dir,LeftRight way,Switchable &switchable,Compare &compare) {
	if(dir==DOWN)
		for(vector<int>::const_iterator ri = ranks.begin(); ri!=ranks.end(); ++ri) {
			Rank *r = config.ranking.GetRank(*ri);
			if(way==RIGHT)
				bubblePassR(config,matrix,r,switchable,compare);
			else
				bubblePassL(config,matrix,r,switchable,compare);
		}
	else
		for(vector<int>::const_reverse_iterator ri = ranks.rbegin(); ri!=ranks.rend(); ++ri) {
			Rank *r = config.ranking.GetRank(*ri);
			if(way==RIGHT)
				bubblePassR(config,matrix,r,switchable,compare);
			else
				bubblePassL(config,matrix,r,switchable,compare);
		}
	//matrix.check();
}
struct RankLess {
	bool operator()(DDModel::Node *u,DDModel::Node *v) {
		if(DDd(u).rank == DDd(v).rank)
			return DDd(u).order < DDd(v).order;
		return DDd(u).rank < DDd(v).rank;
	}
};
#define TIRE 6
void HybridOptimizer2::Reorder(Layout &nodes,Layout &edges) {
	vector<int> affectedRanks;
	{
		NodeV optimVec;
		getCrossoptModelNodes(nodes,edges,optimVec);
		if(optimVec.empty())
			return;
		sort(optimVec.begin(),optimVec.end(),RankLess());
		NodeV::iterator wot = optimVec.begin();
		for(Config::Ranks::iterator ri = config.ranking.begin(); ri!=config.ranking.end(); ++ri)
			for(NodeV::iterator ni = (*ri)->order.begin(); ni!=(*ri)->order.end(); ++ni)
				if(wot!=optimVec.end() && *ni==*wot) {
					DDd(*ni).orderConstraint = -1;
					++wot;
				}
				else
					DDd(*ni).orderConstraint = DDd(*ni).order;
		assert(wot==optimVec.end());
		loops.Field(r_crossopt,"model nodes for crossopt",optimVec.size());
		loops.Field(r_crossopt,"total model nodes",optimVec.front()->g->nodes().size());

		for(NodeV::iterator ni = optimVec.begin(); ni!=optimVec.end(); ++ni)
			if(affectedRanks.empty() || affectedRanks.back()!=DDd(*ni).rank)
				affectedRanks.push_back(DDd(*ni).rank);
		loops.Field(r_crossopt,"ranks for crossopt",affectedRanks.size());
		loops.Field(r_crossopt,"total ranks",config.ranking.size());
	}
	SiftMatrix matrix(config),backupM(config);
	MedianCompare median(DOWN,false);
	CrossingCompare crossing(config,matrix,false);
	OrderConstraintSwitchable switchable;

	Config::Ranks backup = config.ranking;
	// optimize once ignoring node crossings (they can scare the sifting alg) 
	// in a sec we'll sift using the node penalty to clean up
	matrix.m_light = true;
	matrix.recompute();
	backupM = matrix;
	int best = matrix.total(),bestPass=0;
	loops.Field(r_crossopt,"crossings before crossopt",best);
	int passes = 32,pass=0,score;
	while(pass<passes && best) {
		int tired = 0;
		while(pass<passes && tired<TIRE) {
				assert(matrix.m_light);
			median.m_allowEqual = pass%8>4;
			crossing.m_allowEqual = !median.m_allowEqual;
			LeftRight way = (pass%2) ? RIGHT : LEFT;
			UpDown dir;
				assert(matrix.m_light);
			if(pass%4<2) {
				median.m_dir = UP;
				dir = DOWN;
			}
			else {
				median.m_dir = DOWN;
				dir = UP;
			}
			bubblePass(config,matrix,affectedRanks,dir,way,switchable,median);
				assert(matrix.m_light);

			int s2 = matrix.total();
			do {
				score = s2;
				assert(matrix.m_light);
				bubblePass(config,matrix,affectedRanks,dir,way,switchable,crossing);
				s2 = matrix.total();
				assert(s2<=score);
			}
			while(s2<score);
			score = s2;
			if(reportEnabled(r_crossopt)) {
				char buf[10];
				sprintf(buf,"crossings pass %d",pass);
				loops.Field(r_crossopt,buf,score);
			}

			if(score<best) {
				backup = config.ranking;
				backupM = matrix;
				best = score;
				bestPass = pass;
				tired = 0;
			}
			else 
				tired++;
			pass++;
		}
		if(score>best || tired==TIRE) {
			config.Restore(backup);
			//matrix.recompute();
			matrix = backupM;
			tired = 0;
		}
	}
	if(reportEnabled(r_crossopt))
		for(pass;pass<passes;++pass) {
			char buf[10];
			sprintf(buf,"crossings pass %d",pass);
			loops.Field(r_crossopt,buf,0);
		}

	if(score>=best) {
		config.Restore(backup);
	}
	loops.Field(r_crossopt,"model edge crossings",best);

	// sift out node crossings
	matrix.m_light = false;
	matrix.recompute();
	score = matrix.total();
	loops.Field(r_crossopt,"weighted crossings before heavy pass",score);
	/*
	report(r_crossopt,"%d node-node crossings; %d node-edge crossings; %d edge-edge crossings\n",score/(NODECROSS_PENALTY*NODECROSS_PENALTY),
		(score/NODECROSS_PENALTY)%NODECROSS_PENALTY,score%NODECROSS_PENALTY);
	*/
	if(score<NODECROSS_PENALTY) {
		loops.Field(r_crossopt,"weighted crossings after heavy pass",-1);
		return;
	}
	pass = 0;
	passes = 4;
	crossing.m_allowEqual = true;
	// sifting out upward or downward may be better - try both.
	bool improved;
	do {
		improved = false;
		backup = config.ranking;
		bubblePass(config,matrix,affectedRanks,DOWN,RIGHT,switchable,crossing);
		int down = matrix.total();
		Config::Ranks backup2 = config.ranking;
		config.Restore(backup);
		matrix.recompute();
		bubblePass(config,matrix,affectedRanks,UP,RIGHT,switchable,crossing);
		int up = matrix.total();
		if(down<score && down<up) {
			config.Restore(backup2);
			matrix.recompute();
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
double HybridOptimizer2::Reopt(DDModel::Node *n,UpDown dir) {
	return DDd(n).cur.x;
}

} // namespace DynaDAG
