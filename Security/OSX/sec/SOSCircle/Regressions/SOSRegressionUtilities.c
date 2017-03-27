/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
//  SOSRegressionUtilities.c
//

#include <AssertMacros.h>
#include <stdio.h>
#include <Security/SecItem.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>

#include <SOSCloudKeychainClient.h>
#include "SOSRegressionUtilities.h"
#include "SOSInternal.h"

#if TARGET_OS_IPHONE
#include <MobileGestalt.h>
#endif

static const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;

// MARK: ----- SOS General -----

const char *cloudKeychainProxyPath = "/System/Library/Frameworks/Security.framework/Resources/CloudKeychainProxy.bundle/CloudKeychainProxy";

static const char *basecfabsoluteTimeToString(CFAbsoluteTime abstime, CFTimeZoneRef tz)
{
    CFGregorianDate greg = CFAbsoluteTimeGetGregorianDate(abstime, NULL);
    char str[20];
    if (19 != snprintf(str, 20, "%4.4d-%2.2d-%2.2d_%2.2d:%2.2d:%2.2d",
        (int)greg.year, greg.month, greg.day, greg.hour, greg.minute, (int)greg.second))
        str[0]=0;
    char *data = (char *)malloc(20);
    strncpy(data, str, 20);
    return data;
}

const char *cfabsoluteTimeToString(CFAbsoluteTime abstime)
{
    return basecfabsoluteTimeToString(abstime, NULL);
}

const char *cfabsoluteTimeToStringLocal(CFAbsoluteTime abstime)
{
    // Caller must release using free
    CFDateFormatterRef formatter = NULL;
    CFTimeZoneRef tz = NULL;
	CFLocaleRef locale = NULL;
    CFDateRef date = NULL;
    CFStringRef cftime_string = NULL;
    char *time_string = NULL;
    char buffer[1024] = {0,};
    size_t sz;
    
    require(tz = CFTimeZoneCopySystem(), xit);
    require(locale = CFLocaleCreate(NULL, CFSTR("en_US")), xit);
    
    require(formatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterShortStyle, kCFDateFormatterShortStyle), xit);
    CFDateFormatterSetFormat(formatter, CFSTR("MM/dd/yy HH:mm:ss.SSS zzz"));
    require(date = CFDateCreate(kCFAllocatorDefault, abstime), xit);
    require(cftime_string = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, formatter, date), xit);

    CFStringGetCString(cftime_string, buffer, 1024, kCFStringEncodingUTF8);
    sz = strnlen(buffer, 1024);
    time_string = (char *)malloc(sz);
    strncpy(time_string, buffer, sz+1);
xit:
    CFReleaseSafe(tz);
    CFReleaseSafe(formatter);
    CFReleaseSafe(locale);
    CFReleaseSafe(date);
    CFReleaseSafe(cftime_string);
    return time_string;
}

#include <sys/stat.h>

static int file_exist (const char *filename)
{
    struct stat buffer;   
    return (stat (filename, &buffer) == 0);
}

bool XPCServiceInstalled(void)
{
    return file_exist(cloudKeychainProxyPath);
}

void registerForKVSNotifications(const void *observer, CFStringRef name, CFNotificationCallback callBack)
{
    // observer is basically a context; name may not be null
    CFNotificationCenterRef center = CFNotificationCenterGetDarwinNotifyCenter();
    CFNotificationSuspensionBehavior suspensionBehavior = CFNotificationSuspensionBehaviorDeliverImmediately;    //ignored?
    CFNotificationCenterAddObserver(center, observer, callBack, name, NULL, suspensionBehavior);
}

bool testPutObjectInCloudAndSync(CFStringRef key, CFTypeRef object, CFErrorRef *error, dispatch_group_t dgroup, dispatch_queue_t processQueue)
{
    bool result = testPutObjectInCloud(key, object, error, dgroup, processQueue);
    testSynchronize(processQueue, dgroup);

    return result;
}

bool testPutObjectInCloud(CFStringRef key, CFTypeRef object, CFErrorRef *error, dispatch_group_t dgroup, dispatch_queue_t processQueue)
{
    secnotice("test", "testPutObjectInCloud: key: %@, %@", key, object);
    CFDictionaryRef objects = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, key, object, NULL);
    if (objects)
    {
        dispatch_group_enter(dgroup);
        SOSCloudKeychainPutObjectsInCloud(objects, processQueue, ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            secnotice("test", "testPutObjectInCloud returned: %@", returnedValues);
            if (error)
            {
                secnotice("test", "testPutObjectInCloud returned: %@", error);
                CFRelease(error);
            }
            dispatch_group_leave(dgroup);
        });
        CFRelease(objects);
    }

    return true;
}

CFTypeRef testGetObjectFromCloud(CFStringRef key, dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    // TODO: make sure we return NULL, not CFNull
    secnotice("test", "start");
    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(keysToGet, key);

    __block CFTypeRef object = NULL;

    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    dispatch_group_enter(dgroup);
    SOSCloudKeychainGetObjectsFromCloud(keysToGet, processQueue, ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secnotice("test", "SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        if (returnedValues)
        {
            object = (CFTypeRef)CFDictionaryGetValue(returnedValues, key);
            if (object)
                CFRetain(object);
        }
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
     //       CFRelease(*error);
        }
        dispatch_group_leave(dgroup);
        secnotice("test", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
        dispatch_semaphore_signal(waitSemaphore);
    });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
    }
    secnotice("test", "returned: %@", object);
    return object;
}

CFTypeRef testGetObjectsFromCloud(CFArrayRef keysToGet, dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    __block CFTypeRef object = NULL;

    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    dispatch_group_enter(dgroup);
    
    CloudKeychainReplyBlock replyBlock =
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        secnotice("test", "SOSCloudKeychainGetObjectsFromCloud returned: %@", returnedValues);
        object = returnedValues;
        if (object)
            CFRetain(object);
        if (error)
        {
            secerror("SOSCloudKeychainGetObjectsFromCloud returned error: %@", error);
        }
        dispatch_group_leave(dgroup);
        secnotice("test", "SOSCloudKeychainGetObjectsFromCloud block exit: %@", object);
        dispatch_semaphore_signal(waitSemaphore);
    };
    
    if (!keysToGet)
        SOSCloudKeychainGetAllObjectsFromCloud(processQueue, replyBlock);
    else
        SOSCloudKeychainGetObjectsFromCloud(keysToGet, processQueue, replyBlock);
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);
    if (object && (CFGetTypeID(object) == CFNullGetTypeID()))   // return a NULL instead of a CFNull
    {
        CFRelease(object);
        object = NULL;
    }
    secnotice("test", "returned: %@", object);
    return object;
}

bool testSynchronize(dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    __block bool result = false;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    dispatch_group_enter(dgroup);

    SOSCloudKeychainSynchronize(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef error)
        {
            result = true;
            dispatch_group_leave(dgroup);
            dispatch_semaphore_signal(waitSemaphore);
        });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);
    return result;
}

bool testClearAll(dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    __block bool result = false;
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    dispatch_group_enter(dgroup);

    SOSCloudKeychainClearAll(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef error)
        {
            result = true;
            secnotice("test", "SOSCloudKeychainClearAll returned: %@", error);
            dispatch_group_leave(dgroup);
            dispatch_semaphore_signal(waitSemaphore);
        });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
	dispatch_release(waitSemaphore);
    secnotice("test", "SOSCloudKeychainClearAll exit");
    return result;
}

void unregisterFromKVSNotifications(const void *observer)
{
    CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetDarwinNotifyCenter(), observer);
}

//
// MARK: SOSPeerInfo creation helpers
//

CFDictionaryRef SOSCreatePeerGestaltFromName(CFStringRef name)
{
    return CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                        kPIUserDefinedDeviceNameKey, name,
                                        NULL);
}


SOSPeerInfoRef SOSCreatePeerInfoFromName(CFStringRef name, SecKeyRef* outSigningKey, CFErrorRef *error)
{
    SOSPeerInfoRef result = NULL;
    SecKeyRef publicKey = NULL;
    CFDictionaryRef gestalt = NULL;

    require(outSigningKey, exit);

    require_quiet(SecError(GeneratePermanentECPair(256, &publicKey, outSigningKey), error, CFSTR("Failed To Create Key")), exit);

    gestalt = SOSCreatePeerGestaltFromName(name);
    require(gestalt, exit);

    result = SOSPeerInfoCreate(NULL, gestalt, NULL, *outSigningKey, error);

exit:
    CFReleaseNull(gestalt);
    CFReleaseNull(publicKey);

    return result;
}

SOSFullPeerInfoRef SOSCreateFullPeerInfoFromName(CFStringRef name, SecKeyRef* outSigningKey, CFErrorRef *error)
{
    SOSFullPeerInfoRef result = NULL;
    SecKeyRef publicKey = NULL;
    CFDictionaryRef gestalt = NULL;

    require(outSigningKey, exit);

    //GeneratePermanentECPair(256, &publicKey, outSigningKey);
    *outSigningKey = GeneratePermanentFullECKey(256, name, error);
    require(*outSigningKey, exit);

    gestalt = SOSCreatePeerGestaltFromName(name);
    require(gestalt, exit);

    result = SOSFullPeerInfoCreate(NULL, gestalt, NULL, *outSigningKey, error);

exit:
    CFReleaseNull(gestalt);
    CFReleaseNull(publicKey);

    return result;
}

// MARK: ----- MAC Address -----

/*
 *	Name:			GetHardwareAdress
 *
 *	Parameters:		None.
 *
 *	Returns:		Nothing
 *
 *	Description:	Retrieve the hardare address for a specified network interface
 *
 */

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>

static int getHardwareAddress(const char *interfaceName, size_t maxLenAllowed, size_t *outActualLength, char *outHardwareAddress)
{
	char				*end;
	struct if_msghdr	*ifm;
	struct sockaddr_dl	*sdl;
	char				*buf;
	int				result = -1;
	size_t				buffSize;
	int					mib[6] = {CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_IFLIST, 0 };
	
	buf = 0;
	*outActualLength = 0;
	//	see how much space is needed
	require_noerr(result = sysctl(mib, 6, NULL, &buffSize, NULL, 0), xit);

	//	allocate the buffer
	require(buf = malloc(buffSize), xit);
		
	//	get the interface info	
	require_noerr(result = sysctl(mib, 6, buf, &buffSize, NULL, 0), xit);
    
	ifm = (struct if_msghdr *) buf;
	end = buf + buffSize;
	do
	{
		if (ifm->ifm_type == RTM_IFINFO) 		//	should always be true
		{
			sdl = (struct sockaddr_dl *) (ifm + 1);
			if ( sdl->sdl_nlen == strlen( interfaceName ) && ( bcmp( sdl->sdl_data, interfaceName, sdl->sdl_nlen ) == 0 ) )
			{
				if (  sdl->sdl_alen > 0 )
				{
					size_t hardwareLen;
					
					result = 0;						//	indicate found the interface
					hardwareLen = sdl->sdl_alen;
					if ( hardwareLen > maxLenAllowed )
					{
						hardwareLen = maxLenAllowed;
						result = -2;				//	indicate truncation of the address
					} 
					memcpy( outHardwareAddress, sdl->sdl_data + sdl->sdl_nlen, hardwareLen );
					*outActualLength = hardwareLen;
					break;
					
				}	
			}	
		}
		ifm = (struct if_msghdr *)  ((char*)ifm + ifm->ifm_msglen);
	} while ( (char*)ifm < end );
	
xit:
	if (buf)
		free(buf);

	return result;	
}

// MARK: ----- cloudTransportTests -----

CFStringRef myMacAddress(void)
{
    // 6 bytes, no ":"s
    CFStringRef result = NULL;
    const char *interfaceName = "en0";
    size_t maxLenAllowed = 1024;
    size_t outActualLength = 0;
    char outHardwareAddress[1024];
    
    require_noerr(getHardwareAddress(interfaceName, maxLenAllowed, &outActualLength, outHardwareAddress), xit);
    require(outActualLength==6, xit);
    unsigned char buf[32]={0,};
    
    unsigned char *ps = (unsigned char *)buf;
    unsigned char *pa = (unsigned char *)outHardwareAddress;
    for (int ix = 0; ix < 6; ix++, pa++)
        ps += sprintf((char *)ps, "%02x", *pa);

    result = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)buf, kCFStringEncodingUTF8);
    
xit:
    return result;    
}
