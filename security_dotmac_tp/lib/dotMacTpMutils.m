/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * DotMacTpMutils.m - ObjC utils, callable from any language
 */

#include "dotMacTpMutils.h"
#include <Security/cssmapple.h>

#if		XML_DEBUG   
#include <Cocoa/Cocoa.h>
void logCFstr(
	const char *cstr,
	CFStringRef cfstr)
{
	NSLog(@"%s %@\n", cstr, cfstr);
}
#endif

#if		DICTIONARY_DEBUG

#include <Foundation/NSObjCRuntime.h>

void dumpDictionary(
	const char *title,
	CFDictionaryRef dict)
{
	printf("%s:\n", title);
	#if 0
	NSLog(@"%@\n", CFCopyDescription(dict));
	#else
	CFIndex items = CFDictionaryGetCount(dict);
	if(items <= 0) {
		printf("Error on CFDictionaryGetCount\n");
		return;
	}
	const void **keys = (const void **)malloc(items * sizeof(void *));
	const void **values = (const void **)malloc(items * sizeof(void *));
	CFDictionaryGetKeysAndValues(dict, keys, values);
	CFIndex dex;
	for(dex=0; dex<items; dex++) {
		CFStringRef key = (CFStringRef)keys[dex];
		CFTypeID keyType = CFGetTypeID(key);
		if(CFStringGetTypeID() == keyType) {
			NSLog(@"key %d : %@ \n", (int)dex, key);
		}
		else {
			fprintf(stderr, "<key %d is not a string>\n", (int)dex);
		}
		CFTypeID valType = CFGetTypeID(values[dex]);
		if(valType == CFStringGetTypeID()) {
			NSLog(@"  val type = CFString : %@\n", values[dex]);
		}
		else if(valType == CFArrayGetTypeID()) {
			NSLog(@"  val type = CFArray\n");
		}
		else if(valType == CFDictionaryGetTypeID()) {
			NSLog(@"  val type = CFDictionary\n");
			fprintf(stderr, "======== recursively dumping dictionary value ========\n");
			dumpDictionary("Dictionary contents", (CFDictionaryRef)values[dex]);
			fprintf(stderr, "======== end of dictionary value ========\n");
		}
		else if(valType == CFNumberGetTypeID()) {
			NSLog(@"  val type = CFNumber: %@\n", CFCopyDescription(values[dex]));
		}
		else {
			NSLog(@"  val type = unknown: %@\n", CFCopyDescription(values[dex]));
		}
	}
	#endif
}
#endif

/* 
 * Map an HTTP status to a CSSM status. Good luck!
 */
CSSM_RETURN dotMacHttpStatToOs(
	unsigned httpStat) 
{
	CSSM_RETURN crtn = CSSM_OK;
	switch(httpStat) {
		case 200:
			crtn = CSSM_OK;
			break;
		case 401: 
			crtn = CSSMERR_TP_AUTHENTICATION_FAILED;
			break;
		case 403:
			crtn = CSSMERR_TP_REQUEST_REJECTED;
			break;
		case 500:
			crtn = CSSMERR_APPLE_DOTMAC_REQ_SERVER_NOT_AVAIL;
			break;
		default:
			/* FIXME - anything else? */
			crtn = CSSMERR_APPLETP_NETWORK_FAILURE; 
			break;
	}
	return crtn;
}
