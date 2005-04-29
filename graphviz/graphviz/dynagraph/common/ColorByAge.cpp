/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include "Dynagraph.h"
#include <sstream>
#include "breakList.h"
#include "ColorByAge.h"

// an apology: this uses string attributes, which are supposed to be confined to 
// stringsIn.cpp and stringsOut.cpp

using namespace std;
void rotateColor(vector<DString> colors, StrAttrs2 &attrs2) {
  DString color = attrs2["color"];
  if(color.empty())
    attrs2.put("color",colors[0]);
  else for(size_t i=0; i<colors.size()-1; ++i)
    if(color==colors[i]) {
      attrs2.put("color",colors[i+1]);
      break;
    }
}
void ColorByAge::Process(ChangeQueue &Q) {
    StrAttrs::iterator ai = gd<StrAttrs>(Q.current).find("agecolors");
    if(ai==gd<StrAttrs>(Q.current).end())
        return;
    vector<DString> colors;
    breakList(ai->second,colors);
    if(colors.size()==0)
        return;
    for(Layout::node_iter ni = Q.current->nodes().begin(); ni!=Q.current->nodes().end(); ++ni) 
        rotateColor(colors,gd<StrAttrs2>(*ni));
    for(Layout::graphedge_iter ei = Q.current->edges().begin(); ei!=Q.current->edges().end(); ++ei) 
        rotateColor(colors,gd<StrAttrs2>(*ei));
}
