#ifndef __NULL_TRANSFORM__
#define __NULL_TRANSFORM__



#include "Transform.h"
#include "TransformFactory.h"



class NullTransform : public Transform
{
protected:
	std::string DebugDescription();
	NullTransform();

public:
	static CFTypeRef Make();
	static TransformFactory* MakeTransformFactory();
	
	virtual void AttributeChanged(CFStringRef name, CFTypeRef value);
};



#endif
