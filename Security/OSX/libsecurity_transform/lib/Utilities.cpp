/*
 * Copyright (c) 2011,2014 Apple Inc. All Rights Reserved.
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

#include "Utilities.h"
#include "SecTransform.h"
#include <sys/sysctl.h>
#include <syslog.h>
#include <dispatch/dispatch.h>

void MyDispatchAsync(dispatch_queue_t queue, void(^block)(void))
{
	fprintf(stderr, "Running job on queue %p\n", queue);
	dispatch_async(queue, block);
}



dispatch_queue_t MyDispatchQueueCreate(const char* name, dispatch_queue_attr_t attr)
{
	dispatch_queue_t result = dispatch_queue_create(name, attr);
	// fprintf(stderr, "Created queue %s as %p\n", name, result);
	return result;
}



static CFErrorRef CreateErrorRefCore(CFStringRef domain, int errorCode, const char* format, va_list ap)
{
	CFStringRef fmt = CFStringCreateWithCString(NULL, format, kCFStringEncodingUTF8);
	CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, ap);
	va_end(ap);
	CFRelease(fmt);
	
	CFStringRef keys[] = {kCFErrorDescriptionKey};
	CFStringRef values[] = {str};
	
	CFErrorRef result = CFErrorCreateWithUserInfoKeysAndValues(NULL, domain, errorCode, (const void**) keys, (const void**) values, 1);
	CFRelease(str);
	
	return result;
}



CFErrorRef CreateGenericErrorRef(CFStringRef domain, int errorCode, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	return CreateErrorRefCore(domain, errorCode, format, ap);
}



CFErrorRef CreateSecTransformErrorRef(int errorCode, const char* format, ...)
{
	// create a CFError in the SecTransform error domain.  You can add an explanation, which is cool.
	va_list ap;
	va_start(ap, format);
	
	return CreateErrorRefCore(kSecTransformErrorDomain, errorCode, format, ap);
}



CFErrorRef CreateSecTransformErrorRefWithCFType(int errorCode, CFTypeRef message)
{
	CFStringRef keys[] = {kCFErrorLocalizedDescriptionKey};
	CFTypeRef values[] = {message};
	return CFErrorCreateWithUserInfoKeysAndValues(NULL, kSecTransformErrorDomain, errorCode, (const void**) keys, (const void**) values, 1);
}



CFTypeRef gAnnotatedRef = NULL;

CFTypeRef DebugRetain(const void* owner, CFTypeRef type)
{
	CFTypeRef result = CFRetain(type);
	if (type == gAnnotatedRef)
	{
		fprintf(stderr, "Object %p was retained by object %p, count = %ld\n", type, owner, CFGetRetainCount(type));
	}
	
	return result;
}



void DebugRelease(const void* owner, CFTypeRef type)
{
	if (type == gAnnotatedRef)
	{
		fprintf(stderr, "Object %p was released by object %p, count = %ld\n", type, owner, CFGetRetainCount(type) - 1);
	}
	
	CFRelease(type);
}

// Cribbed from _dispatch_bug and altered a bit
void transforms_bug(size_t line, long val)
{
    static dispatch_once_t pred;
    static char os_build[16];
    static void *last_seen;
    void *ra = __builtin_return_address(0);
    dispatch_once(&pred, ^{
#ifdef __APPLE__
        int mib[] = { CTL_KERN, KERN_OSVERSION };
        size_t bufsz = sizeof(os_build);
        sysctl(mib, 2, os_build, &bufsz, NULL, 0);
#else
        os_build[0] = '\0';
#endif
    });
    if (last_seen != ra) {
        last_seen = ra;
        syslog(LOG_NOTICE, "BUG in SecTransforms: %s - %p - %lu - %lu", os_build, last_seen, (unsigned long)line, val);
    }
}
