#ifndef __CPLUSPLUS_UTILS__
#define __CPLUSPLUS_UTILS__

#include <string>
#include <CoreFoundation/CoreFoundation.h>

std::string StringFromCFString(CFStringRef theString);
CFStringRef CFStringFromString(std::string theString);

// class to automatically manage the lifetime of a CFObject

class CFTypeRefHolder
{
private:
	CFTypeRef mTypeRef;

public:
	CFTypeRefHolder(CFTypeRef typeRef) : mTypeRef(typeRef) {}
	virtual ~CFTypeRefHolder();
	
	void Set(CFTypeRef typeRef); // replace the value in the holder with another -- releases the current value
	CFTypeRef Get() {return mTypeRef;}
};



#endif
