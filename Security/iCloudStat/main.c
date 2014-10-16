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
static const CFStringRef gClientIsUsingiCloudKeychainSyncing = CFSTR("com.apple.icloudkeychain.deviceIsUsingICloudKeychain");
static const CFStringRef gNumberOfPeers = CFSTR("com.apple.icloudkeychain.numberOfPeers");
static const CFStringRef gNumberOfItemsBeingSynced = CFSTR("com.apple.icloudkeychain.numberOfItemsBeingSynced");
    
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
static const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.icloudkeychain";
#endif
    
#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
static const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.icloudkeychain";
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <asl.h>


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
	
	asl_set(mAsl, key_buffer, value_buffer);
	asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "%s", gTopLevelKeyForiCloudKeychainTracing);
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
	
    if (0LL == value)
    {
        ADClientClearScalarKey(key);
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

static int64_t GetNumberOfPeers()
{
	int64_t result = 0LL;
	
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

static int64_t GetNumberOfItemsBeingSyncedForType(CFTypeRef class)
{
    int64_t result = 0;
    
    CFTypeRef keys[] = {kSecClass, kSecAttrSynchronizable, kSecMatchLimit, kSecReturnAttributes};
    CFTypeRef values[] = {class, kCFBooleanTrue, kSecMatchLimitAll, kCFBooleanTrue};
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, (const void **)values, (sizeof(keys)/sizeof(keys[0])), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if (NULL == query)
    {
        return result;
    }
    
    CFArrayRef query_result = NULL;
	OSStatus status =  SecItemCopyMatching(query, (CFTypeRef *)&query_result);
    CFRelease(query);
    if (noErr != status || NULL == query_result)
	{
		if (NULL != query_result)
		{
			CFRelease(query_result);
		}
		return result;
	}
    
    result = (int64_t)CFArrayGetCount(query_result);
    CFRelease(query_result);
    return result;
}

static int64_t GetNumberOfItemsBeingSynced()
{
	int64_t result = 0;
	
    result = GetNumberOfItemsBeingSyncedForType(kSecClassInternetPassword);
    result += GetNumberOfItemsBeingSyncedForType(kSecClassGenericPassword);
	
	return result;
}


int main(int argc, const char * argv[])
{
    int64_t value =  0;
    if (!ClientIsInCircle())
    {
        // This will clear the value in the backend database if it had been previously set
        sendTraceMessage(gClientIsUsingiCloudKeychainSyncing, value);
        return 0;
    }
    
    value = 1;
    sendTraceMessage(gClientIsUsingiCloudKeychainSyncing, value);
    
    value = GetNumberOfPeers();
    sendTraceMessage(gNumberOfPeers, value);
    
    value = GetNumberOfItemsBeingSynced();
    sendTraceMessage(gNumberOfItemsBeingSynced, value);
    
    return 0;
}

