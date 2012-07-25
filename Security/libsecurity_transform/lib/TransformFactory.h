#ifndef __TRANSFORM_FACTORY__
#define __TRANSFORM_FACTORY__

#include "Transform.h"
#include "LinkedList.h"

class TransformFactory
{
protected:
	static void Register(TransformFactory* tf);
    static dispatch_once_t gSetup;
    static dispatch_queue_t gRegisteredQueue;
    static CFMutableDictionaryRef gRegistered;
    
	CFStringRef mCFType;

	static TransformFactory* FindTransformFactoryByType(CFStringRef type);
	static void RegisterTransforms();
	static void RegisterTransform(TransformFactory* tf, CFStringRef cfname = NULL);
    static void Setup(void *);

private:
    static bool RegisterTransform_prelocked(TransformFactory* tf, CFStringRef name);

public:
	static SecTransformRef MakeTransformWithType(CFStringRef type, CFErrorRef* baseError);

	TransformFactory(CFStringRef type, bool registerGlobally = false, CFStringRef cftype = NULL);
	static void Setup();
	virtual CFTypeRef Make() = 0;
    CFStringRef GetTypename() { return mCFType; };
};



#endif
