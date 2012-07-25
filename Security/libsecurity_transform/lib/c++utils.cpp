#include "c++utils.h"

using namespace std;

std::string StringFromCFString(CFStringRef theString)
{
	CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(theString), 0);
	
	if (maxLength <= 0) // roll over?  just plain bad?
	{
		return "";
	}
	
	// leave room for NULL termination
	maxLength += 1;
	
	char* buffer = new char[maxLength];

	if (buffer == NULL) // out of memory?  Naughty, naughty...
	{
		return "";
	}
	
	CFStringGetCString(theString, buffer, maxLength, 0);
	
	string result(buffer);
	delete buffer;
	return result;
}



CFStringRef CFStringFromString(std::string theString)
{
	return CFStringCreateWithCString(NULL, theString.c_str(), 0);
}



CFTypeRefHolder::~CFTypeRefHolder()
{
	if (mTypeRef != NULL)
	{
		CFRelease(mTypeRef);
	}
}



void CFTypeRefHolder::Set(CFTypeRef typeRef)
{
	if (mTypeRef != NULL)
	{
		CFRelease(mTypeRef);
	}
	
	mTypeRef = typeRef;
}

