#ifndef __SINGLE_SHOT_SOURCE__
#define __SINGLE_SHOT_SOURCE__

#include "Source.h"

extern CFStringRef gSingleShotSourceName;

/*
	We need this source because we need to send the data followed by
	a null value, so that all input sources have the same behavior.
*/

class SingleShotSource : public Source
{
protected:
	SingleShotSource(CFTypeRef value, Transform* t, CFStringRef name);

public:
	void DoActivate();
	Boolean Equal(const CoreFoundationObject* obj);
	static CFTypeRef Make(CFTypeRef value, Transform* t, CFStringRef name);
	std::string DebugDescription();
};

#endif
