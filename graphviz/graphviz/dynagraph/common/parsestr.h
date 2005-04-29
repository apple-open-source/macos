/*   Copyright (c) AT&T Corp.  All rights reserved.
   
This software may only be used by you under license from 
AT&T Corp. ("AT&T").  A copy of AT&T's Source Code Agreement 
is available at AT&T's Internet website having the URL 

http://www.research.att.com/sw/tools/graphviz/license/

If you received this software without first entering into a license 
with AT&T, you have an infringing copy of this software and cannot 
use it without violating AT&T's intellectual property rights. */

#ifndef PARSESTR_H
#define PARSESTR_H

// overload >> DString for better-delimited tokens
// returns one token or one delimeter
inline std::istream &operator>>(std::istream &in,DString &s) {
	static const std::string delim = "=,[]; \t\n\r";
	std::string temp;
	in >> std::ws;
	while(!in.eof()) {
		int c = in.get();
		if(c<0)
			break; // why does eof() delay?
		unsigned i;
		for(i=0; i<delim.length(); ++i)
			if(delim[i]==c) {
				if(temp.empty())
					temp += c;
				else
					in.putback(c);
				break;
			}
		if(i<delim.length())
			break;
		temp += c;
	}
	s = temp.c_str();
	return in;
}
struct ParseEOF : DGException {
    ParseEOF() : DGException("the parser encountered an end-of-file") {}
};
struct ParseExpected : DGException { 
  DString expect,got; 
  ParseExpected(DString expect,DString got) : 
    DGException("the std::string parser didn't get what it expected"),
       expect(expect),got(got) {} 
  ParseExpected(char e,char g) : DGException("the std::string parser didn't get char it expected") {
      char buf[2];
      buf[1] = 0;
      buf[0] = e;
      expect = buf;
      buf[0] = g;
      got = buf;
  }
};
struct must {
	DString s;
	must(DString s) : s(s) {}
};
inline std::istream &operator>>(std::istream &in,const must &m) {
	DString s;
	in >> s;
	if(s!=m.s)
		throw ParseExpected(m.s,s);
	return in;
}
struct quote {
	const std::string &s;
	quote(const std::string &s) : s(s) {}
};
inline std::ostream &operator<<(std::ostream &out,quote &o) {
  out << '"';
  if(!o.s.empty()) 
    for(unsigned i = 0; i<o.s.length(); ++i)
      if(o.s[i]=='"') // " => \"
	out << "\\\""; 
      else if(o.s[i]=='\\') // \ => \\       .
	out << "\\\\";
      else
	out << o.s[i];
  out << '"';
  return out;
}
struct unquote {
	std::string &s;
	unquote(std::string &s) : s(s) {}
};
inline std::istream &operator>>(std::istream &in,unquote &o) {
	char c;
	in >> std::ws;
	in.get(c);
	if(c!='"') 
		throw ParseExpected('"',c);
	o.s.erase();
	while(1) {
		in.get(c);
		if(c=='\\')
			in.get(c);
		else if(c=='"')
			break;
		o.s += c;
	}
	return in;
}
/*
from the agraph & incrface lexers
LETTER [A-Za-z_\200-\377]
DIGIT	[0-9]
NAME	{LETTER}({LETTER}|{DIGIT})*
NUMBER	[-]?(({DIGIT}+(\.{DIGIT}*)?)|(\.{DIGIT}+))
ID		({NAME}|{NUMBER})

absurd solution follows.  really should modernize the 
lexer so it'd be safe to use it here.
*/
inline bool isletter(int C) {
    return ('A'<=C && C<='Z')||('a'<=C && C<='z')||(0x80<=C && C<=0xff)||C=='_';
}
// signed chars become bad int characters
inline bool isletter(char c) {
	return isletter(int(static_cast<unsigned char>(c)));
}
template<typename C>
inline bool needsQuotes(const C *s) {
    int state = 0;
    for(const C *eye = s;*eye;++eye) {
        switch(state) {
            case 0:
                if(isletter(*eye))
                    state = 1;
                else if(*eye=='-')
                    state = 2;
                else if(isdigit(*eye))
                    state = 3;
                else if(*eye=='.')
                    state = 4;
                else return true;
                break;
            case 1:
                if(isletter(*eye)||isdigit(*eye))
                    state = 1;
                else return true;
                break;
            case 2:
                if(isdigit(*eye))
                    state = 3;
                else if(*eye=='.')
                    state = 4;
                else
                    return true;
                break;
            case 3:
                if(isdigit(*eye))
                    state = 3;
                else if(*eye=='.')
                    state = 5;
                else
                    return true;
                break;
            case 4:
                if(isdigit(*eye))
                    state = 5;
                else
                    return true;
                break;
            case 5:
                if(isdigit(*eye))
                    state = 5;
                else
                    return true;
        }
    }
    return (state%2)==0; // odd states are accepting
}
/* this wasn't good enough
template<typename C>
inline bool needsQuotes(const C *s) {
	static const char g_badChars[] = "\" ,[]-+";
	static const int g_nBads = sizeof(g_badChars);
	if(isdigit(s[0]))
		return true;
	// didn't work in gcc: find_first_of(s.begin(),s.end(),g_badChars,g_badChars+g_nBads)!=s.end()
	for(const C *i=s; *i; ++i)
		for(int i2 = 0; i2<g_nBads; ++i2)
			if(*i==g_badChars[i2]) 
				return true;
	return false;
}
*/
bool needsQuotes(const DString &s);
struct mquote { // quote if needed
	const DString &s;
	mquote(const DString &s) : s(s) {}
};
std::ostream &operator<<(std::ostream &,const mquote &);
// match e.g. braces.  throws outers away (this isn't general yet)
struct match {
    char open,close;
    std::string &s;
    match(char open,char close,std::string &s) : open(open),close(close),s(s) {}
};
inline std::istream &operator>>(std::istream &in,match &m) {
    in >> std::ws;
    char c;
    in.get(c);
    if(c!=m.open)
        throw ParseExpected(m.open,c);
    int count = 1;
    while(!in.eof()) {
        in.get(c);
        if(c==m.open)
            ++count;
        else if(c==m.close)
            if(!--count)
                return in;
        m.s += c;
    }
    throw ParseEOF();
    return in;
}

#endif // PARSESTR_H
