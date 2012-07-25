#ifndef __SOURCE__
#define __SOURCE__



#include "SecTransform.h"
#include "CoreFoundationBasics.h"
#include "Transform.h"

class Source : public CoreFoundationObject
{
protected:
	Transform* mDestination;
	CFStringRef mDestinationName;
	CFTypeRef mLastValue;
	dispatch_queue_t mDispatchQueue;

	void SetValue(CFTypeRef value);

	Source(CFStringRef sourceObjectName, Transform* destination, CFStringRef destinationName);

public:
	virtual ~Source();
	
	void Activate();
	virtual void DoActivate() = 0;
	
	Boolean Equal(const CoreFoundationObject* obj);
	CFTypeRef GetValue() const {return mLastValue;}
	std::string DebugDescription();
};

#endif
