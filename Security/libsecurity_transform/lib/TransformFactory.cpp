#include <libkern/OSAtomic.h>

#include "TransformFactory.h"
#include "NullTransform.h"
#include "Digest.h"
#include "EncryptTransform.h"
#include "GroupTransform.h"
#include "Utilities.h"


void TransformFactory::RegisterTransforms()
{
	RegisterTransform_prelocked(NullTransform::MakeTransformFactory(), NULL);
	RegisterTransform_prelocked(DigestTransform::MakeTransformFactory(), NULL);
	RegisterTransform_prelocked(EncryptTransform::MakeTransformFactory(), NULL);
	RegisterTransform_prelocked(DecryptTransform::MakeTransformFactory(), NULL);
	RegisterTransform_prelocked(GroupTransform::MakeTransformFactory(), NULL);
}

CFMutableDictionaryRef TransformFactory::gRegistered;
dispatch_once_t TransformFactory::gSetup;
dispatch_queue_t TransformFactory::gRegisteredQueue;

bool TransformFactory::RegisterTransform_prelocked(TransformFactory* tf, CFStringRef cfname)
{
    if (!CFDictionaryContainsKey(gRegistered, tf->mCFType)) {
        CFDictionaryAddValue(gRegistered, tf->mCFType, tf);
        if (!cfname) {
            CoreFoundationObject::RegisterObject(tf->mCFType, false);
        } else {
            if (!CoreFoundationObject::FindObjectType(cfname)) {
                CoreFoundationObject::RegisterObject(cfname, false);
            }
        }
    }
    
    return true;
}


void TransformFactory::RegisterTransform(TransformFactory* tf, CFStringRef cfname)
{
    dispatch_once_f(&gSetup, NULL, Setup);
    dispatch_barrier_sync(gRegisteredQueue, ^{
        RegisterTransform_prelocked(tf, cfname);
    });
}

void TransformFactory::Setup(void *)
{
    gRegisteredQueue = dispatch_queue_create("com.apple.security.TransformFactory.Registered", DISPATCH_QUEUE_CONCURRENT);
    gRegistered = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL);
    RegisterTransforms();
}

void TransformFactory::Setup()
{
    dispatch_once_f(&gSetup, NULL, Setup);
}

TransformFactory* TransformFactory::FindTransformFactoryByType(CFStringRef name)
{
    dispatch_once_f(&gSetup, NULL, Setup);
    __block TransformFactory *ret;
    dispatch_barrier_sync(gRegisteredQueue, ^{
        ret = (TransformFactory*)CFDictionaryGetValue(gRegistered, name);
    });
    return ret;
}



SecTransformRef TransformFactory::MakeTransformWithType(CFStringRef type, CFErrorRef* baseError)
{
	TransformFactory* tf = FindTransformFactoryByType(type);
	if (!tf)
	{
		if (baseError != NULL)
		{
#if 0
            // This version lists out all regestered transform types.
            // It is useful more for debugging then for anything else,
            // so it is great to keep around, but normally not so good
            // to run.
			dispatch_barrier_sync(gRegisteredQueue, ^(void) {
                CFMutableStringRef transformNames = CFStringCreateMutable(NULL, 0);
                CFIndex numberRegistered = CFDictionaryGetCount(gRegistered);
                CFStringRef names[numberRegistered];
                CFDictionaryGetKeysAndValues(gRegistered, (const void**)names, NULL);
                for(int i = 0; i < numberRegistered; i++) {
                    if (i != 0) {
                        CFStringAppend(transformNames, CFSTR(", "));
                    }
                    CFStringAppend(transformNames, names[i]);
                }
                
                *baseError = CreateSecTransformErrorRef(kSecTransformTransformIsNotRegistered, 
                                                        "The %s transform is not registered, choose from: %@", type,transformNames);

            });
#else
            *baseError = CreateSecTransformErrorRef(kSecTransformTransformIsNotRegistered, 
                                                    "The %s transform is not registered", type);
#endif
		}
		
		return NULL;
	}
	else
	{
		return tf->Make();
	}
}



TransformFactory::TransformFactory(CFStringRef type, bool registerGlobally, CFStringRef cftype) : mCFType(type)
{
	if (registerGlobally)
	{
		CoreFoundationObject::RegisterObject(cftype ? cftype : type, false);
	}
}
