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
 * DotMacTpMutils.h - ObjC utils, callable from any language
 */
 
#ifndef	_DOT_MAC_TP_MUTILS_H_
#define _DOT_MAC_TP_MUTILS_H_

#include <CoreFoundation/CFString.h>
#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	NDEBUG
#define XML_DEBUG   0
#else
#define XML_DEBUG   1
#endif

#if		XML_DEBUG   
void logCFstr(
	const char *cstr,
	CFStringRef cfstr);
#else
#define logCFStr(cs, cf)
#endif

#if		XML_DEBUG
#define DICTIONARY_DEBUG	1
#else
#define DICTIONARY_DEBUG	0
#endif

#if		DICTIONARY_DEBUG

#include <CoreFoundation/CFDictionary.h>

void dumpDictionary(
	const char *title,
	CFDictionaryRef dict);
#endif

CSSM_RETURN dotMacHttpStatToOs(
	unsigned httpStat);


#ifdef __cplusplus
}
#endif

#endif	/* _DOT_MAC_TP_MUTILS_H_ */

