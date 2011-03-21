/*
 * Copyright (c) 2009-2010 Apple Inc. All rights reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotificationPriv.h>
#include "ui.h"

#define CS_NOT_INSTALLED_HEADER_KEY "CS_NOT_INSTALLED_HEADER_KEY"
#define CS_NOT_INSTALLED_MSG_KEY "CS_NOT_INSTALLED_MSG_KEY"
#define OK_KEY "OK"

SInt32
AlertCoreStorageNotInstalled(void)
{
	CFMutableDictionaryRef noteDict;
	CFUserNotificationRef noteRef = NULL;
	SInt32 error = -1;
	CFOptionFlags responseFlags = 0;
	CFURLRef bundleURL = NULL;

	noteDict = CFDictionaryCreateMutable(NULL,
	                   10,
	                   &kCFCopyStringDictionaryKeyCallBacks,
	                   &kCFTypeDictionaryValueCallBacks);
	if (!noteDict)
		goto out;

	bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
	                   CFSTR("/System/Library/Filesystems/nofs.fs"),
	                   kCFURLPOSIXPathStyle,
	                   true);
	if (!bundleURL)
		goto out;
	
	CFDictionarySetValue(noteDict,
	                   kCFUserNotificationLocalizationURLKey,
	                   bundleURL);

	CFDictionarySetValue(noteDict,
	                   kCFUserNotificationAlertHeaderKey,
	                   CFSTR(CS_NOT_INSTALLED_HEADER_KEY));
	CFDictionarySetValue(noteDict,
	                   kCFUserNotificationAlertMessageKey,
	                   CFSTR(CS_NOT_INSTALLED_MSG_KEY));
	CFDictionaryAddValue(noteDict,
	                   kCFUserNotificationDefaultButtonTitleKey,
	                   CFSTR(OK_KEY));

	noteRef = CFUserNotificationCreate(NULL, 60,
	                   kCFUserNotificationCautionAlertLevel,
	                   &error,
	                   noteDict);
	if (!noteRef)
		goto out;

	error = CFUserNotificationReceiveResponse(noteRef, 0, &responseFlags);

out:
	if (noteDict)
		CFRelease(noteDict);
	if (noteRef)
		CFRelease(noteRef);
	if (bundleURL)
		CFRelease(bundleURL);

	return error;
}
