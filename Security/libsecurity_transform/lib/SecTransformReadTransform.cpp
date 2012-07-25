#include "SecTransformReadTransform.h"
#include "SecCustomTransform.h"
#include "Utilities.h"

static CFStringRef kStreamTransformName = CFSTR("SecReadStreamTransform");
static CFStringRef kStreamMaxSize = CFSTR("MAX_READSIZE");

static SecTransformInstanceBlock StreamTransformImplementation(CFStringRef name,
															   SecTransformRef newTransform,
															   SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock =
	^{
		CFErrorRef result = NULL;
		
		if (NULL == name || NULL == newTransform)
		{
		}
		
		// define the storage for our block
		__block CFIndex blockSize = 4096;  // make a default block size
		
		// it's not necessary to set the input stream size
		SecTransformCustomSetAttribute(ref, kStreamMaxSize, kSecTransformMetaAttributeRequired, kCFBooleanFalse);
		
		// define the action if we change the max read size
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kStreamMaxSize,
		^(SecTransformAttributeRef attribute, CFTypeRef value)
		{
			CFNumberGetValue((CFNumberRef) value, kCFNumberCFIndexType, &blockSize);
			return value;
		});
		
		// define for our input action
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName,
		^(SecTransformAttributeRef attribute, CFTypeRef value)
		{
			if (value == NULL)
			{
				return (CFTypeRef) NULL;
			}
			
			CFArrayRef array = (CFArrayRef) value;
			CFReadStreamRef input = (CFReadStreamRef) CFArrayGetValueAtIndex(array, 0);
			
			// open the stream
			if (!CFReadStreamOpen(input))
			{
				// We didn't open properly.  Error out
				return (CFTypeRef) CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "An error occurred while opening the stream.");
			}
			
			// allocate the read buffer on the heap
			u_int8_t* buffer = (u_int8_t*) malloc(blockSize);
			
			CFIndex bytesRead;
			
			bytesRead = CFReadStreamRead(input, buffer, blockSize);
			while (bytesRead > 0)
			{
				// make data from what was read
				CFDataRef value = CFDataCreate(NULL, buffer, bytesRead);
				
				// send it down the chain
				SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, value);
				
				// cleanup
				CFRelease(value);
				
				bytesRead = CFReadStreamRead(input, buffer, blockSize);
			}
			
			free(buffer);
			
			SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, (CFTypeRef) NULL);
			
			return (CFTypeRef) NULL;
		});
		
		return result;
	};
	
	return Block_copy(instanceBlock);
}



SecTransformRef SecTransformCreateReadTransformWithReadStream(CFReadStreamRef inputStream)
{
	static dispatch_once_t once = 0;
	
	__block bool ok = true;
	__block CFErrorRef result = NULL;
	
	dispatch_once(&once,
	^{
		ok = SecTransformRegister(kStreamTransformName, &StreamTransformImplementation, &result);
	});
	
	if (!ok)
	{
		return result;
	}
	else
	{
	
		SecTransformRef transform = SecTransformCreate(kStreamTransformName, &result);
		if (NULL != transform)
		{
			// if we add the read stream directly to the transform the internal source stream
			// will take over. This is bad.  Instead, we wrap this in a CFArray so that we can
			// pass through undetected
			CFTypeRef arrayData[] = {inputStream};
			CFArrayRef arrayRef = CFArrayCreate(NULL, arrayData, 1, &kCFTypeArrayCallBacks);

			// add the input to the transform
			SecTransformSetAttribute(transform, kSecTransformInputAttributeName, arrayRef, &result);
			
			CFRelease(arrayRef);
		}
		
		return transform;
	}
}
