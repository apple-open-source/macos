#include "Source.h"
#include "Utilities.h"
#include "c++utils.h"

#include <string>

using namespace std;

Source::Source(CFStringRef sourceObjectName, Transform* destination, CFStringRef destinationName) :
	CoreFoundationObject(sourceObjectName),
	mDestination(destination),
	mDestinationName(destinationName)
{
    CFStringRef queueName = CFStringCreateWithFormat(NULL, NULL, CFSTR("source:%@"), sourceObjectName);
    char *queueName_cstr = utf8(queueName);
	
	mLastValue = NULL;
	mDispatchQueue = MyDispatchQueueCreate(queueName_cstr, NULL);
    free((void*)queueName_cstr);
    CFReleaseNull(queueName);
}



Source::~Source()
{
	if (mLastValue != NULL)
	{
		CFReleaseNull(mLastValue);
	}
	
	dispatch_release(mDispatchQueue);
}



void Source::Activate()
{
	dispatch_async(mDispatchQueue, ^{DoActivate();});
}



void Source::SetValue(CFTypeRef value)
{
	if (value == mLastValue)
	{
		return;
	}
	
	// is there an existing value?  If so, release it
    CFReleaseNull(mLastValue);

    mLastValue = CFRetainSafe(value);
}



Boolean Source::Equal(const CoreFoundationObject* obj)
{
	if (CoreFoundationObject::Equal(obj))
	{
		const Source* objSource = (const Source*) obj;
		if (objSource->mDestination == mDestination &&
			CFStringCompare(objSource->mDestinationName, mDestinationName, 0) == kCFCompareEqualTo)
		{
			return true;
		}
	}
	
	return false;
}



std::string Source::DebugDescription()
{
	string result = CoreFoundationObject::DebugDescription() + ": Source ";
	
	char buffer[256];
	sprintf(buffer, "(Destination = %p, name = %s)", mDestination, StringFromCFString(mDestinationName).c_str());
	
	result += buffer;
	
	return result;
}

