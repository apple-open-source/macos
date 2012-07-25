#ifndef __STREAM_SOURCE__
#define __STREAM_SOURCE__


#include "Source.h"



extern CFStringRef gStreamSourceName;



class StreamSource : public Source
{
protected:
	StreamSource(CFReadStreamRef input, Transform* transform, CFStringRef name);

	virtual void Finalize();
	CFReadStreamRef mReadStream;
	dispatch_group_t mReading;

	void BackgroundActivate();
	
public:
	
	void DoActivate();
	virtual ~StreamSource();
	
	static CFTypeRef Make(CFReadStreamRef input, Transform* transform, CFStringRef name);
	Boolean Equal(const CoreFoundationObject* object);
	std::string DebugDescription();
};



#endif
