/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
/*
 *  BLGetOSVersion.c
 *  bless
 *
 */

#include <sys/param.h>
#include "bless.h"
#include "bless_private.h"


int BLGetOSVersion(BLContextPtr context, const char *mount, BLVersionRec *version)
{
	int				err = 0;
	char			fullpath[MAXPATHLEN];
	CFURLRef		plistURL = NULL;
	CFDataRef		versData = NULL;
	CFDictionaryRef	versDict = NULL;
	CFStringRef		versString;
	CFArrayRef		versArray = NULL;
	
	snprintf(fullpath, sizeof fullpath, "%s/%s", mount, kBL_PATH_SYSTEM_VERSION_PLIST);
	plistURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)fullpath, strlen(fullpath), 0);
	if (!plistURL) {
		contextprintf(context, kBLLogLevelError, "Can't get URL for \"%s\"\n", fullpath);
		err = 1;
		goto exit;
	}
	if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, plistURL, &versData, NULL, NULL, NULL)) {
		contextprintf(context, kBLLogLevelError, "Can't load \"%s\"\n", fullpath);
		err = 2;
		goto exit;
	}
	versDict = CFPropertyListCreateWithData(kCFAllocatorDefault, versData, 0, NULL, NULL);
	if (!versDict) {
		contextprintf(context, kBLLogLevelError, "Could not recognize contents of \"%s\" as a property list\n", fullpath);
		err = 3;
		goto exit;
	}
	versString = CFDictionaryGetValue(versDict, CFSTR("ProductVersion"));
	if (!versString) {
		contextprintf(context, kBLLogLevelError, "Version plist \"%s\" missing ProductVersion item\n", fullpath);
		err = 4;
		goto exit;
	}
	versArray = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, versString, CFSTR("."));
	if (!versArray || CFArrayGetCount(versArray) < 2) {
		contextprintf(context, kBLLogLevelError, "Badly formed version string in plist \"%s\"\n", fullpath);
		err = 5;
		goto exit;
	}
	if (version) {
		version->major = CFStringGetIntValue(CFArrayGetValueAtIndex(versArray, 0));
		version->minor = CFStringGetIntValue(CFArrayGetValueAtIndex(versArray, 1));
		version->patch = (CFArrayGetCount(versArray) >= 3) ? CFStringGetIntValue(CFArrayGetValueAtIndex(versArray, 2)) : 0;
	}
	
exit:
	if (plistURL) CFRelease(plistURL);
	if (versData) CFRelease(versData);
	if (versDict) CFRelease(versDict);
	if (versArray) CFRelease(versArray);
	return err;
}
