#include "NullTransform.h"

NullTransform::NullTransform() : Transform(CFSTR("NullTransform"))
{
}



CFTypeRef NullTransform::Make()
{
	return CoreFoundationHolder::MakeHolder(gInternalCFObjectName, new NullTransform());
}



void NullTransform::AttributeChanged(CFStringRef name, CFTypeRef value)
{
	// move input to output, otherwise do nothing
	if (CFStringCompare(name, kSecTransformInputAttributeName, 0) == kCFCompareEqualTo)
	{
		SetAttributeNoCallback(kSecTransformOutputAttributeName, value);
	}
}



std::string NullTransform::DebugDescription()
{
	return Transform::DebugDescription() + ": NullTransform";
}



class NullTransformFactory : public TransformFactory
{
public:
	NullTransformFactory();
	
	virtual CFTypeRef Make();
};



TransformFactory* NullTransform::MakeTransformFactory()
{
	return new NullTransformFactory();
}



NullTransformFactory::NullTransformFactory() : TransformFactory(CFSTR("Null Transform"))
{
}



CFTypeRef NullTransformFactory::Make()
{
	return NullTransform::Make();
}

