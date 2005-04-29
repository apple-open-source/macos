/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "voronoi/voronoi.h"
#include "fdp/fdp.h"
#include "shortspline/shortspline.h"
#include "dynadag/DynaDAG.h"
#include "common/ColorByAge.h"
#include "incrface/createLayoutServer.h"
#include "common/breakList.h"

using namespace std;

// is it possible to do this w/out the struct?
typedef Server *(*creatorf)(Layout *cli,Layout *curr);
template<typename ST>
struct tcreator {
	static Server *create(Layout *client,Layout *current) {
		return new ST(client,current);
	}
};
struct creators : map<DString,creatorf> {
	creators() {
		creators &me = *this;

		me["dynadag"] = tcreator<DynaDAG::DynaDAGServer>::create;
		me["fdp"] = tcreator<FDP::FDPServer>::create;
		me["voronoi"] = tcreator<Voronoi::VoronoiServer>::create;
		me["visspline"] = tcreator<ShortSpliner>::create;
		me["labels"] = tcreator<LabelPlacer>::create;
		me["shapegen"] = tcreator<ShapeGenerator>::create;
        me["colorbyage"] = tcreator<ColorByAge>::create;
	}
} g_creators;
Server *createLayoutServer(Layout *client,Layout *current) {
	UpdateCurrent *uc = new UpdateCurrent(client,current);
	CompoundServer *eng = new CompoundServer(client,current);
	eng->actors.push_back(uc); // obligatory
	DString &serverlist = gd<StrAttrs>(client)["engines"];
	if(serverlist.empty())
		serverlist = "shapegen,dynadag,labels,colorbyage";
    //gd<StrAttrs>(client)["agecolors"] = "green,blue,black";
    vector<DString> engs;
    breakList(serverlist,engs);
    for(vector<DString>::iterator ei = engs.begin(); ei!=engs.end(); ++ei) {
		creatorf crea = g_creators[*ei];
		if(!crea) {
			delete eng;
			ServerUnknown su;
			su.serverName = *ei;
			throw su;
		}
		Server *server = crea(client,current);
		eng->actors.push_back(server);
	}

	return eng;
}
