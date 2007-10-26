/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "macros.hpp"
#include "common.hpp"
#include "smb_server_prefs.h"
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/time.h>

bool is_server_system(void)
{
    struct stat sbuf;

    if (stat("/System/Library/CoreServices/ServerVersion.plist", &sbuf) == 0) {
	return true;
    }

    return false;
}

CFStringRef cfstring_wrap(const char * str)
{
    CFStringRef ret = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, str,
					    kCFStringEncodingUTF8,
					    kCFAllocatorNull);

    if (ret == NULL) {
	/* Yay! There's no way to figure out what went wrong, but we kno that
	 * if something did go wrong we are toast, so better to throw.
	 */
	throw std::runtime_error("CFString wrapping failed");
    }

    return ret;
}

std::string cfstring_convert(CFStringRef cfstr)
{
    const char * ptr;
    CFIndex bufsz;
    char * buf;

    if ((ptr = CFStringGetCStringPtr(cfstr, kCFStringEncodingUTF8))) {
	return std::string(ptr);
    }

    /* Conversion will fail for an empty string, to bail early. */
    if (CFStringGetLength(cfstr) == 0) {
	return std::string();
    }

    bufsz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
					    kCFStringEncodingUTF8);

    if ((buf = new char[bufsz + 1]) == NULL) {
	throw std::runtime_error("out of memory");
    }

    if (!CFStringGetCString(cfstr, buf, bufsz, kCFStringEncodingUTF8)) {
	free(buf);
	throw std::runtime_error("CFString conversion failed");
    }

    std::string result = std::string(buf);
    free(buf);

    return result;
}

std::string cftype_string(CFTypeID obj_type)
{
    CFStringRef str = CFCopyTypeIDDescription(obj_type);
    if (str == NULL) {
	return std::string("<invalid_type>");
    }

    std::string ret(cfstring_convert(str));
    safe_release(str);
    return ret;
}

SyncMutex::SyncMutex()
{
    this->m_fd = ::open(kSMBPreferencesSyncTool, O_RDONLY | O_EXLOCK);
    if (this->m_fd == -1) {
	throw std::runtime_error("unable to acquire preferences mutex");
    }

}

SyncMutex::~SyncMutex()
{
    ::close(this->m_fd);
}

unsigned long long time_now_usec(void)
{
    struct timeval tv;
    unsigned long long now;

    if (::gettimeofday(&tv, NULL) == -1) {
	throw std::runtime_error("gettimeofday failed");
    }

    now = SEC_TO_USEC(tv.tv_sec) + tv.tv_usec;
    return now;
}

#define SERVICE_CHANGED "com.apple.ServiceConfigurationChangedNotification"

void post_service_notification(const char * service_name,
				const char * service_state)
{
    CFNotificationCenterRef notify;

    notify = CFNotificationCenterGetDistributedCenter();
    if (notify == NULL) {
	return;
    }

    VERBOSE("posting %s with %s=>%s\n",
	    SERVICE_CHANGED, service_name, service_state);

    cf_typeref<CFMutableDictionaryRef>
	info(CFDictionaryCreateMutable(kCFAllocatorDefault,
		0 /* capacity */,
		&kCFTypeDictionaryKeyCallBacks /* CFType callbacks */,
		&kCFTypeDictionaryValueCallBacks /* CFType callbacks */));
    if (info == (CFMutableDictionaryRef)NULL) {
	return;
    }

    try {
	cf_typeref<CFStringRef> name(cfstring_wrap(service_name));
	cf_typeref<CFStringRef> state(cfstring_wrap(service_state));

	/* cfstring_wrap() can throw. */

	CFDictionaryAddValue(info, CFSTR("ServiceName"), name);
	CFDictionaryAddValue(info, CFSTR("State"), state);

	CFNotificationCenterPostNotificationWithOptions(notify,
		CFSTR(SERVICE_CHANGED),
		NULL /* sender */, info,
		kCFNotificationPostToAllSessions);

    } catch (...) {
	return;
    }

}

/* vim: set cindent ts=8 sts=4 tw=79 : */
