#include "SecNullTransform.h"
#include "NullTransform.h"

const CFStringRef kSecNullTransformName = CFSTR("Null Transform");

SecNullTransformRef SecNullTransformCreate()
{
	return (SecNullTransformRef) NullTransform::Make();
}
