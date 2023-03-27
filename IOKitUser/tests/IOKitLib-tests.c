#include <darwintest.h>

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

T_DECL(IOMasterPort,
       "check if one can retrieve mach port for communicating with IOKit",
       T_META_NAMESPACE("IOKitUser.IOKitLib")
       )
{
    mach_port_t masterPort = MACH_PORT_NULL;

    T_EXPECT_MACH_SUCCESS(IOMasterPort(MACH_PORT_NULL, &masterPort), NULL);
    T_EXPECT_NE(MACH_PORT_NULL, masterPort, NULL);
}

T_DECL(OSNumberFloats,
       "check roundtrip serialization of float/double CFNumber serialization",
       T_META_NAMESPACE("IOKitUser.IOKitLib")
       )
{
	CFMutableDictionaryRef dict, props;
	CFNumberRef num;

	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
	props = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(props, CFSTR("OSNumberFloatTest"), dict);

	float floatValue = 1234.5678;
    num = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &floatValue );
	CFDictionarySetValue(dict, CFSTR("floatValue"), num);
	CFRelease(num);

	double doubleValue = 5678.1234;
    num = CFNumberCreate( kCFAllocatorDefault, kCFNumberDoubleType, &doubleValue );
	CFDictionarySetValue(dict, CFSTR("doubleValue"), num);
	CFRelease(num);

	SInt64 intValue = 12345678;
    num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt64Type, &intValue );
	CFDictionarySetValue(dict, CFSTR("intValue"), num);
	CFRelease(num);

	kern_return_t kr;
	io_service_t
	service = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/IOResources");
    T_EXPECT_NE(MACH_PORT_NULL, service, NULL);

	kr = IORegistryEntrySetCFProperties(service, props);
    T_EXPECT_MACH_SUCCESS(kr, NULL);

	CFTypeRef obj = IORegistryEntryCreateCFProperty(service, CFSTR("OSNumberFloatTest"), kCFAllocatorDefault, 0);
    T_EXPECT_NE(NULL, obj, NULL);
    T_EXPECT_TRUE(CFEqual(obj, dict), NULL);

	CFRelease(obj);
	CFRelease(dict);
	CFRelease(props);
}
