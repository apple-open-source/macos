#include "StringDict.h"
#include "parsestr.h"

using namespace std;

inline bool needsQuotes(const DString &s) {
	return needsQuotes(s.c_str());
}
ostream &operator<<(ostream &os,const mquote &mq) {
	if(mq.s.empty())
		os << "\"\"";
	else if(needsQuotes(mq.s))
		os << '"' << mq.s << '"';
	else
		os << mq.s;
	return os;
}
