
#include "SingleShotSource.h"
#include <string>

using namespace std;

CFStringRef gSingleShotSourceName = CFSTR("Single Shot Source");

SingleShotSource::SingleShotSource(CFTypeRef value, Transform* t, CFStringRef name) :
	Source(gSingleShotSourceName, t, name)
{
	SetValue(value);
}

void SingleShotSource::DoActivate()
{
	// Make sure our destination doesn't vanish while we are sending it data (or the final NULL)
	CFRetain(mDestination->GetCFObject());
	
	// take our value and send it on its way
	mDestination->SetAttribute(mDestinationName, GetValue());
	
	// send an end of stream
	mDestination->SetAttribute(mDestinationName, NULL);

	CFRelease(mDestination->GetCFObject());
}



Boolean SingleShotSource::Equal(const CoreFoundationObject* obj)
{
	if (Source::Equal(obj))
	{
		const SingleShotSource* sss = (const SingleShotSource*) obj;
		return CFEqual(GetValue(), sss->GetValue());
	}
	
	return false;
}



CFTypeRef SingleShotSource::Make(CFTypeRef value, Transform* t, CFStringRef name)
{
	return CoreFoundationHolder::MakeHolder(gInternalCFObjectName, new SingleShotSource(value, t, name));
}



std::string SingleShotSource::DebugDescription()
{
	string result = Source::DebugDescription() + ": SingleShotSource ";
	
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "(value = %p)", GetValue());
	
	result += buffer;
	
	return result;
}
