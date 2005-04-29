/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef STRINGDICT_H
#define STRINGDICT_H

#include "cdt.h"
#include <string>
#include <algorithm>
#include <iostream>

// adaptation of agraph's refstr
struct StringDict {
	StringDict();
	void init(); 
	const char *enter(const char *val);
	void release(const char *val);
	void ref(const char *val); // MUST have come from here!
private:
	Dict_t *dict;
};
extern StringDict g_stringDict;

struct DString { // imitation of std::string
	typedef const char *iterator;
	typedef const char *const_iterator;
	typedef size_t size_type;
	static const size_type npos;

	DString() : val(0) {}
	DString(const char *v) : val(g_stringDict.enter(v)) {}
	DString(const DString &ds) : val(ds.val) {
		g_stringDict.ref(val);
	}
	DString(const std::string &s) : val(g_stringDict.enter(s.c_str())) {}
	~DString() {
		g_stringDict.release(val);
	}
	DString &operator =(const DString &ds) {
		const char *old = val; // do ref first for unlikely s = s
		g_stringDict.ref(val = ds.val);
		g_stringDict.release(old);
		return *this;
	}
    operator std::string() const {
        return val?std::string(val):std::string();
	}
	// these are what make this super-cool: single-word compare!
	bool operator <(const DString &ds) const {
		return val<ds.val;
	}
	bool operator ==(const DString &ds) const {
		return val==ds.val;
	}
	bool operator !=(const DString &ds) const {
		return val!=ds.val;
	}
	const char *c_str() const {
		return val;
	}
	size_type size() const {
		if(!val)
			return 0;
		return strlen(val);
	}
	size_type length() const {
		return size();
	}
	bool empty() const {
		return length()==0;
	}
	void clear() {
		*this = 0;
	}
	char operator[](size_t pos) const {
		return val[pos];
	}
	const char &operator[](size_t pos) {
		return val[pos];
	}
	iterator begin() {
		return val;
	}
	iterator end() {
		return val+size();
	}
	const_iterator begin() const {
		return val;
	}
	const_iterator end() const {
		return val+size();
	}
	size_type find(char c,size_type pos) const {
		if(pos>=size())
			return npos;
		const_iterator i = std::find(begin()+pos,end(),c);
		if(i==end())
			return npos;
		else
			return i-begin();
	}
	DString substr(size_type pos,size_type len) const {
		DString ret;
		if(pos>=size())
			return ret;
		ret.assign(begin()+pos,len);
		return ret;
	}
	DString &assign(const char *v,size_type len) {
	  if(!v) {
	    return *this = 0;
	  }
		if(len>=strlen(v))
			return *this = v;
		// this does not work because if v is a DString, this changes the dictionary entry itself.
		/*
		char *sneaky = const_cast<char*>(v),
			c = sneaky[len];
		sneaky[len] = 0;
		*this = sneaky;
		sneaky[len] = c;
		*/
		char *copy = new char[len+1];
		strncpy(copy,v,len);
		copy[len] = 0;
		*this = copy;
		delete [] copy;
		return *this;
	}
private:
	const char *val;
};
inline std::ostream &operator <<(std::ostream &os,const DString &s) {
  if(s.length())
    os << s.c_str();
  return os;
}
#include "common/dgxep.h"
struct DictStringLost : DGException {
	const char *s;
	DictStringLost(const char *s) : 
	  DGException("StringDict internal exception: string lost"),
	  s(s)
  {}
};

#endif // STRINGDICT_H
