//
//  Exception.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/7/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_Exception_hpp
#define CPPUtil_Exception_hpp

class Exception : public std::exception {
    protected:
	std::string _what;

    public:
	Exception(std::string& what) : _what(what) {} ;
	virtual ~Exception() throw () {};
	
	virtual char const* what() const throw() { return _what.c_str(); }
};

#define THROW(e)			\
{					\
	std::ostringstream s;		\
	s << e;				\
	std::string str = s.str();	\
	Exception exp(str);		\
	throw exp;			\
}

#define UNIMPLEMENTED() THROW("Unimplemented: " << Path((char*)__FILE__).basename() << ":" << __LINE__ )

#endif
