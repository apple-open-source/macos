//
//  main.c
//  iCloudStats
//
//  Created by local on 7/20/13.
//
//

#include <CoreFoundation/CoreFoundation.h>

#import "SOSCloudCircle.h"
#import <Security/Security.h>

static const CFStringRef gMessageTracerPrefix = CFSTR("com.apple.message.");

static const CFStringRef gClientIsUsingiCloudKeychainSyncing = CFSTR("com.apple.cloudkeychain.deviceIsUsingICloudKeychain");
static const CFStringRef gClientIsNotUsingiCloudKeychainSyncing = CFSTR("com.apple.cloudkeychain.deviceIsNotUsingICloudKeychain");
static const CFStringRef gNumberOfPeers = CFSTR("com.apple.cloudkeychain.numberOfPeers");
static const CFStringRef gNumberOfItemsBeingSynced = CFSTR("com.apple.cloudkeychain.numberOfItemsBeingSynced");

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <asl.h>

static const char* gMessageTracerDomainField = "com.apple.message.domain";
static const const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.cloudkeychain";

static bool OSX_SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)
{
	bool result = false;
	
	if (NULL == key)
	{
		return result;
	}
	
	aslmsg mAsl = NULL;
	mAsl = asl_new(ASL_TYPE_MSG);
	if (NULL == mAsl)
	{
		return result;
	}
    
    CFIndex key_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key), kCFStringEncodingUTF8);
    key_length += 1; // For null
    char base_key_buffer[key_length];
    memset(base_key_buffer, 0,key_length);
    if (!CFStringGetCString(key, base_key_buffer, key_length, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
        return result;
    }
    
    
    CFStringRef key_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%@"), gMessageTracerPrefix, key);
    if (NULL == key_str)
    {
        asl_free(mAsl);
        return result;
    }
	
	CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%lld"), value);
    if (NULL == value_str)
    {
        asl_free(mAsl);
        CFRelease(key_str);
        return result;
    }
    
    CFIndex key_str_numBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key_str), kCFStringEncodingUTF8);
    key_str_numBytes += 1; // For null
    char key_buffer[key_str_numBytes];
    memset(key_buffer, 0, key_str_numBytes);
    if (!CFStringGetCString(key_str, key_buffer, key_str_numBytes, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
        CFRelease(key_str);
        CFRelease(value_str);
        return result;
    }
    CFRelease(key_str);
    
    CFIndex value_str_numBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value_str), kCFStringEncodingUTF8);
    value_str_numBytes += 1; // For null
    char value_buffer[value_str_numBytes];
    memset(value_buffer, 0, value_str_numBytes);
    if (!CFStringGetCString(value_str, value_buffer, value_str_numBytes, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
        CFRelease(value_str);
        return result;
    }
    CFRelease(value_str);
	
	asl_set(mAsl, gMessageTracerDomainField, base_key_buffer);
	
	asl_set(mAsl, key_buffer, value_buffer);
	asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "%s is %lld", key_buffer, value);
	asl_free(mAsl);
	return true;
}
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
#import <AggregateDictionary/ADClient.h>

static bool iOS_SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)
{
	if (NULL == key)
	{
		return false;
	}
	
	ADClientSetValueForScalarKey(key, value);
	return true;
}
#endif

static bool ClientIsInCircle()
{
    bool result = false;
    CFErrorRef error = NULL;
    SOSCCStatus status = kSOSCCError;
    
    status = SOSCCThisDeviceIsInCircle(&error);
    if (NULL != error)
    {
        CFRelease(error);
    }
    else
    {
        switch (status)
        {
            case kSOSCCInCircle:
            {
                result = true;
            }
				break;
				
				// kSOSCCRequestPending
				// While this device will be in a circle, it is not in
				// one yet. For now, this will be treated as if the device
				// was not in a circle and will wait for the device to
				// be in a circle and have that turn on this daemon with
				// launchctl
            case kSOSCCRequestPending:
            case kSOSCCCircleAbsent:
            case kSOSCCError:
            default:
                break;
        }
    }
    return result;
}


static bool sendTraceMessage(CFStringRef key, int64_t value)
{
	
#if (TARGET_IPHONE_SIMULATOR)
	return false;
#endif
    
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_SetCloudKeychainTraceValueForKey(key, value);
#endif
    
#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return iOS_SetCloudKeychainTraceValueForKey(key, value);
#endif
	
}

int64_t GetNumberOfPeers()
{
	int64_t result = 0;
	
	CFErrorRef error = NULL;
	CFArrayRef peers = NULL;
	
	peers = SOSCCCopyPeerPeerInfo(&error);
	if (NULL != error)
	{
		CFRelease(error);
		if (NULL != peers)
		{
			CFRelease(peers);
		}
		return result;
	}
	
	if (NULL != peers)
	{
		result = (int64_t)CFArrayGetCount(peers);
		CFRelease(peers);
	}
	
	return result;
}


int64_t GetNumberOfItemsBeingSynced()
{
	int64_t result = 0;
	
	
    CFTypeRef classTypes[] = {kSecClassInternetPassword, kSecClassGenericPassword};
    
    
    CFArrayRef classTypesArray = CFArrayCreate(kCFAllocatorDefault,
                (const void **)classTypes, (sizeof(classTypes)/sizeof(classTypes[0])),
                &kCFTypeArrayCallBacks);
    if (NULL == classTypesArray)
    {
        return result;
    }
    
    CFTypeRef keys[] = {kSecClass, kSecAttrSynchronizable, kSecReturnAttributes};
    CFTypeRef values[] = {classTypesArray, kCFBooleanTrue, kCFBooleanTrue};
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, (sizeof(keys)/sizeof(keys[0])), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(classTypesArray);
    if (NULL == query)
    {
        return result;
    }
    
	CFArrayRef query_result = NULL;
	OSStatus status =  SecItemCopyMatching(query, (CFTypeRef *)&query_result);
	if (noErr != status)
	{
        CFRelease(query);
		if (NULL != query_result)
		{
			CFRelease(query_result);
		}
		return result;
	}
    CFRelease(query);
	
	if (NULL != query_result)
    {
        result = 1; // There is at least one item being synced
        if (CFArrayGetTypeID() == CFGetTypeID(query_result))
        {
            result = (int64_t)CFArrayGetCount(query_result);
        }
        CFRelease(query_result);
    }
	
	return result;
}


int main(int argc, const char * argv[])
{
	int64_t value = 1;
    if (!ClientIsInCircle())
    {
		sendTraceMessage(gClientIsNotUsingiCloudKeychainSyncing, value);
        return 0;
    }
    
    sendTraceMessage(gClientIsUsingiCloudKeychainSyncing, value);
    
    value = GetNumberOfPeers();
    sendTraceMessage(gNumberOfPeers, value);
    
    value = GetNumberOfItemsBeingSynced();
    sendTraceMessage(gNumberOfItemsBeingSynced, value);

    
    return 0;
}

