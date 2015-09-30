#include <libkern/OSAtomic.h>
#include <CoreFoundation/CoreFoundation.h>

#include "CoreFoundationBasics.h"
#include "Block.h"
#include "TransformFactory.h"
#include "Utilities.h"
#include "syslog.h"
#include "misc.h"

using namespace std;


const CFStringRef gInternalCFObjectName = CFSTR("SecTransform Internal Object");
const CFStringRef gInternalProtectedCFObjectName = CFSTR("SecTransform Internal Object (protected)");

struct RegisteredClassInfo
{
	CFRuntimeClass mClass;
	CFTypeID mClassID;
	RegisteredClassInfo();
    void Register(CFStringRef name);

    static const RegisteredClassInfo *Find(CFStringRef name);
    static dispatch_queue_t readWriteLock;
    static dispatch_once_t initializationGuard;
    static CFMutableDictionaryRef registeredInfos;
};

dispatch_queue_t RegisteredClassInfo::readWriteLock;
dispatch_once_t RegisteredClassInfo::initializationGuard;
CFMutableDictionaryRef RegisteredClassInfo::registeredInfos;

/*
	THE FOLLOWING FUNCTION REGISTERS YOUR CLASS.  YOU MUST CALL YOUR CLASS INITIALIZER HERE!
*/

static CFErrorRef gNoMemory;

CFErrorRef GetNoMemoryError()
{
	return gNoMemory;
}



CFErrorRef GetNoMemoryErrorAndRetain()
{
	return (CFErrorRef) CFRetain(gNoMemory);
}



static void CoreFoundationObjectRegister()
{
	static dispatch_once_t gate = 0;

	dispatch_once(&gate,
	^{
		// Init NoMemory here, so other places we can safely return it (otherwise we might
		// not have enough memory to allocate the CFError)
		gNoMemory = CreateGenericErrorRef(kCFErrorDomainPOSIX, ENOMEM, "Out of memory.");
		
        // only one registration for internal objects, cuts down on how many objects
        // we register in the CF type table
        CoreFoundationObject::RegisterObject(gInternalCFObjectName, false);
        CoreFoundationObject::RegisterObject(gInternalProtectedCFObjectName, true);

        // register any objects which may be exposed as API here
        
        // call for externalizable transforms.
        TransformFactory::Setup();
	});
}



RegisteredClassInfo::RegisteredClassInfo()
{
	memset(&mClass, 0, sizeof(mClass));
    dispatch_once(&RegisteredClassInfo::initializationGuard, ^(void) {
        RegisteredClassInfo::readWriteLock = dispatch_queue_create("com.apple.security.transform.cfobject.RegisteredClassInfo", DISPATCH_QUEUE_CONCURRENT);
        RegisteredClassInfo::registeredInfos = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    });
}



CoreFoundationObject::CoreFoundationObject(CFStringRef name) : mHolder(NULL), mObjectType(name)
{
}



CoreFoundationObject::~CoreFoundationObject()
{
}



CFHashCode CoreFoundationObject::Hash()
{
	// Valid for address equality only, overload for something else.
	return (CFHashCode)mHolder;
}

void CoreFoundationObject::Finalize()
{
	delete this;
}


std::string CoreFoundationObject::FormattingDescription(CFDictionaryRef options)
{
	return "CoreFoundationObject";
}



std::string CoreFoundationObject::DebugDescription()
{
	return "CoreFoundationObject";
}



Boolean CoreFoundationObject::Equal(const CoreFoundationObject* obj)
{
	return mHolder == obj->mHolder;
}



static void FinalizeStub(CFTypeRef typeRef)
{
	CoreFoundationHolder::ObjectFromCFType(typeRef)->Finalize();
}



static Boolean IsEqualTo(CFTypeRef ref1, CFTypeRef ref2)
{
	if (CFGetTypeID(ref1) != CFGetTypeID(ref2)) {
        // If ref2 isn't a CoreFoundatonHolder treating
        // it like one is likely to crash.  (One could
        // argue that we should check to see if ref2
        // is *any* CoreFoundationHolder registered
        // type)
        return false;
    }
    
    CoreFoundationHolder* tr1 = (CoreFoundationHolder*) ref1;
	CoreFoundationHolder* tr2 = (CoreFoundationHolder*) ref2;
	return tr1->mObject->Equal(tr2->mObject);
}



static CFHashCode MakeHash(CFTypeRef typeRef)
{
	CoreFoundationHolder* tr = (CoreFoundationHolder*) typeRef;
	return tr->mObject->Hash();
}



static CFStringRef MakeFormattingDescription(CFTypeRef typeRef, CFDictionaryRef formatOptions)
{
	// get the string
	CoreFoundationHolder* tr = (CoreFoundationHolder*) typeRef;
	string desc = tr->mObject->FormattingDescription(formatOptions);
	
	if (desc.length() == 0)
	{
		return NULL;
	}
	
	// convert it to a CFString
	return CFStringCreateWithCString(NULL, desc.c_str(), kCFStringEncodingMacRoman);
}


static CFStringRef MakeDebugDescription(CFTypeRef typeRef)
{
	// get the string
	CoreFoundationHolder* tr = (CoreFoundationHolder*) typeRef;
	string desc = tr->mObject->DebugDescription();
	
	if (desc.length() == 0)
	{
		return NULL;
	}
	
	// convert it to a CFString
	return CFStringCreateWithCString(NULL, desc.c_str(), kCFStringEncodingMacRoman);
}




void CoreFoundationObject::RegisterObject(CFStringRef name, bool protectDelete)
{
	RegisteredClassInfo *classRecord = new RegisteredClassInfo;
	
    classRecord->mClass.version = 0;
    // XXX: this is kind of lame, "name" is almost always a CFSTR, so it would
    // be best to use the CFStringGetPtr result AND hold a reference to the
    // name string, but fall back to utf8...   (note there is no unRegisterObject)
    char *utf8_name = utf8(name);
    classRecord->mClass.className = strdup(utf8_name);
    free(utf8_name);
    classRecord->mClass.init = NULL;
    classRecord->mClass.copy = NULL;
    classRecord->mClass.finalize = protectDelete ? NULL : FinalizeStub;
    classRecord->mClass.equal = IsEqualTo;
    classRecord->mClass.hash = MakeHash;
    classRecord->mClass.copyFormattingDesc = MakeFormattingDescription;
    classRecord->mClass.copyDebugDesc = MakeDebugDescription;
    classRecord->mClassID = _CFRuntimeRegisterClass((const CFRuntimeClass * const)&classRecord->mClass);
    classRecord->Register(name);
}



CFTypeID CoreFoundationObject::FindObjectType(CFStringRef name)
{
    const RegisteredClassInfo *classInfo = RegisteredClassInfo::Find(name);
    if (classInfo) {
        return classInfo->mClassID;
    } else {
        return _kCFRuntimeNotATypeID;
    }
}

const RegisteredClassInfo *RegisteredClassInfo::Find(CFStringRef name)
{
    __block const RegisteredClassInfo *ret = NULL;
    dispatch_sync(RegisteredClassInfo::readWriteLock, ^(void) {
        ret = (const RegisteredClassInfo *)CFDictionaryGetValue(RegisteredClassInfo::registeredInfos, name);
    });
    
    return ret;
}

void RegisteredClassInfo::Register(CFStringRef name)
{
    dispatch_barrier_sync(RegisteredClassInfo::readWriteLock, ^(void) {
        CFDictionarySetValue(RegisteredClassInfo::registeredInfos, name, this);
    });
}

CFStringRef CoreFoundationObject::GetTypeAsCFString()
{
	return mObjectType;
}



CoreFoundationHolder* CoreFoundationHolder::MakeHolder(CFStringRef name, CoreFoundationObject* object)
{
	// setup the CoreFoundation registry, just in case we need it.

	CoreFoundationObjectRegister();
	
	CoreFoundationHolder* data = (CoreFoundationHolder*) _CFRuntimeCreateInstance(kCFAllocatorDefault,
															CoreFoundationObject::FindObjectType(name),
															sizeof(CoreFoundationHolder) - sizeof(CFRuntimeBase),
															NULL);
	data->mObject = object;
	object->SetHolder(data);

	return data;
}



CoreFoundationObject* CoreFoundationHolder::ObjectFromCFType(CFTypeRef type)
{
	return ((CoreFoundationHolder*) type)->mObject;
}



