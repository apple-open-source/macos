/*
 *  utils.c
 *  KerberosHelper
 */

/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include "utils.h"

#include <Carbon/Carbon.h>

OSStatus __KRBCreateUTF8StringFromCFString (CFStringRef inString, char **outString)
{
	OSStatus	err = noErr;
	char		*string = NULL;
	CFIndex		length;
	
	if (inString  == NULL) { err = paramErr; goto Done; }
    if (outString == NULL) { err = paramErr; goto Done; }
	
	/* This is the fastest way to get the C string, but it does depend on how
	 * it was encoded
	 */
	string = (char *) CFStringGetCStringPtr (inString, kCFStringEncodingUTF8);

	if (NULL != string) {
		string = strdup (string);
	} else {
		length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (inString), kCFStringEncodingUTF8) + 1;
		string = malloc (length);
		
		if (NULL != string && !CFStringGetCString (inString, string, length, kCFStringEncodingUTF8)) {
			free (string);
			string = NULL;
			err = -1; /* XXX Pick a good one! */
		}
	}		
		
	*outString = string;
Done:
	return err;
}

OSStatus __KRBReleaseUTF8String (char *inString) 
{
	OSStatus	err = noErr;

	if (NULL == inString) { err = paramErr; goto Done; }
    free (inString);

Done:	
	return err;
}
