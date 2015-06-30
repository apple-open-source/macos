//
//  UtilString.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilString_hpp
#define CPPUtil_UtilString_hpp

struct ConstCharHash {
	//
	// Okay, by design std::hash<char*> hashes on the pointer,
	// not the contents of that pointer.
	//
	// The C++11 std::hash<std::string> hash works, but must
	// construct a copy of the passed in string to hash.
	//
	// That's 3x slower than this, minimum.
	//
	// This is just the __gnu_cxx hash code inlined.
	//
	std::size_t operator()(const char* __s) const {
		unsigned long __h = 0;
		for ( ; *__s; ++__s)
			__h = 5 * __h + *__s;
		return size_t(__h);
	};

};

struct ConstCharEqualTo {
	bool operator() (const char* s1, const char* s2) const {
		return strcmp(s1, s2) == 0;
	}
};

bool ends_with(std::string& str, std::string postfix);

#endif
