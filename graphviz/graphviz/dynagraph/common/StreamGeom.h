/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#include <iostream>
struct GeomParseError : DGException {
	std::string val;
	GeomParseError(std::string val = std::string()) : 
	  DGException("coord reader expected comma"),val(val) {}
};
struct BadSplineValue : DGException {
	int val;
	BadSplineValue(int val) : 
	  DGException("bezier degree must be 1 or 3"),val(val) {}
};
inline std::istream & operator>>(std::istream &is, Coord &read) {
	is >> read.x;
	char c;
	is >> c;
	if(c!=',') {
		fprintf(stderr,">>Coord: %c %d\n",c,c);
		GeomParseError xep;
		xep.val.assign(1,c);
		throw xep;
	}
	is >> read.y;
	return is;
}
inline std::istream & operator>>(std::istream &is,Line &read) {
	read.clear();
	char ch;
	if(!is.eof()) {
		is.get(ch);
		if(ch=='b') {
			int level;
			is >> level;
			switch(level) {
			case 1:
			case 3:
				read.degree = level;
				break;
			default:
				throw BadSplineValue(level);
			}
		}
		else {
			read.degree = 1;
			is.unget();
		}
	}
	while(!is.eof()) {
		Coord c;
		is >> c;
		read.push_back(c);
	}
	return is;
}
inline std::istream & operator>>(std::istream &is,Lines &read) {
	read.clear();
	while(!is.eof()) {
		read.push_back(Line());
		is >> read.back();
	}
	return is;
}
inline std::ostream & operator <<(std::ostream &os,const Coord &write) {
	os.flags(os.flags()|std::ios::fixed);
	os << write.x << ',' << write.y;
	return os;
}
inline std::ostream & operator <<(std::ostream &os,const Line &write) {
	os << 'b' << write.degree;
	for(Line::const_iterator ci = write.begin(); ci!=write.end(); ++ci)
		os << ' ' << *ci;
	return os;
}
inline std::ostream & operator <<(std::ostream &os,const Lines &write) {
	for(Lines::const_iterator li = write.begin(); li!=write.end(); ++li)
		os << *li << ';';
	return os;
}
