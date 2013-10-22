/*
 * Copyright (c) 2001 - 2010 Apple Inc. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <netsmb/smb_lib.h>
#include "charsets.h"

/* 
 * We now use CFStringUppercase
 */
void str_upper(char *dst, size_t maxDstLen, CFStringRef srcRef)
{
	CFMutableStringRef upperRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, srcRef);
	if (upperRef == NULL) {
		/* Nothing else we can do here */
		CFStringGetCString(srcRef, dst, maxDstLen, kCFStringEncodingUTF8);
		return;
	}
	CFStringUppercase(upperRef, NULL);
	CFStringGetCString(upperRef, dst, maxDstLen, kCFStringEncodingUTF8);
	CFRelease(upperRef);
}

/*
 * %%% - Change all strings to CFStringRef, once we remove the UI code.
 */
char *
convert_wincs_to_utf8(const char *windows_string, CFStringEncoding codePage)
{
	CFStringRef s;
	CFIndex maxlen;
	char *result;

	s = CFStringCreateWithCString(NULL, windows_string, codePage);
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCString for Windows code page failed on \"%s\", syserr = %s",
						ASL_LEVEL_DEBUG,  windows_string, strerror(errno));

		/* kCFStringEncodingMacRoman should always succeed */
		s = CFStringCreateWithCString(NULL, windows_string, 
		    kCFStringEncodingMacRoman);
		if (s == NULL) {
			smb_log_info("CFStringCreateWithCString for Windows code page failed on \"%s\" with kCFStringEncodingMacRoman - skipping, syserr = %s",
						ASL_LEVEL_DEBUG, windows_string, strerror(errno));
			return NULL;
		}
	}

	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
    
	result = malloc(maxlen);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for UTF-8 string for \"%s\" - skipping, syserr = %s", 
					ASL_LEVEL_DEBUG, windows_string, strerror(errno));
		CFRelease(s);
		return NULL;
	}
    
	if (!CFStringGetCString(s, result, maxlen, kCFStringEncodingUTF8)) {
		smb_log_info("CFStringGetCString for UTF-8 failed on \"%s\" - skipping, syserr = %s",
					ASL_LEVEL_DEBUG, windows_string, strerror(errno));
		CFRelease(s);
		free(result);
		return NULL;
	}
    
	CFRelease(s);
	return result;
}

/*
 * This routine assumes the inbound c-style string is really a UTF8 string.
 * We create a CFString, uppercase if the flag is set, convert it to the code
 * page and then return a c-style string containing the new converted string.
 */
char *convert_utf8_to_wincs(const char *utf8_string, CFStringEncoding codePage, int uppercase)
{
	CFStringRef utfStr;
	CFMutableStringRef utfMutableStr = NULL;
	CFIndex maxlen;
	char *result = NULL;

	utfStr = CFStringCreateWithCString(NULL, utf8_string, kCFStringEncodingUTF8);
	if (utfStr) {
		utfMutableStr = CFStringCreateMutableCopy(NULL, 0, utfStr);
		CFRelease(utfStr);
	}
	
	if (utfMutableStr == NULL) {
		smb_log_info("CFStringCreateWithCString for UTF-8 failed on \"%s\", syserr = %s", 
					ASL_LEVEL_DEBUG, utf8_string, strerror(errno));
		goto done;
	}

	if (uppercase) {
		CFStringUppercase(utfMutableStr, CFLocaleGetSystem());
	}
	
	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(utfMutableStr), codePage) + 1;
	result = malloc(maxlen);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Windows code page string for \"%s\" - skipping, syserr = %s", 
					ASL_LEVEL_DEBUG, utf8_string, strerror(errno));
		goto done;
	}
	if (!CFStringGetCString(utfMutableStr, result, maxlen, codePage)) {
		smb_log_info("CFStringGetCString for Windows code page failed on \"%s\" - skipping, syserr = %s",
					ASL_LEVEL_DEBUG, utf8_string, strerror(errno));
		free(result);
		result =  NULL;
		goto done;
	}
	
done:
	if (utfMutableStr)
		CFRelease(utfMutableStr);
	return result;
}

/*
 * Convert little-endian Unicode string to UTF-8.
 * Converts the Unicode string to host byte order in place.
 * XXX - <rdar://problem/7518600>  will clean this up
 */
char *
convert_leunicode_to_utf8(unsigned short *unicode_string, size_t maxLen)
{
	unsigned short *unicode_charp, unicode_char;

	for (unicode_charp = unicode_string;
	    (unicode_char = *unicode_charp) != 0;
	    unicode_charp++)
		*unicode_charp = CFSwapInt16LittleToHost(unicode_char);
	return convert_unicode_to_utf8(unicode_string, maxLen);
}

/*
 * Convert Unicode string to UTF-8.
 * XXX - <rdar://problem/7518600>  will clean this up
 */
char *
convert_unicode_to_utf8(const uint16_t *unicode_string, size_t maxLen)
{
	size_t uslen;
	CFStringRef s;
	char *result;
	
	 /* Number of characters not bytes */
	maxLen = maxLen / 2;
	for (uslen = 0; (unicode_string[uslen] != 0) && (uslen < maxLen); uslen++)
		;
	s = CFStringCreateWithCharacters(kCFAllocatorDefault, unicode_string, uslen);
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCharacters failed, syserr = %s", 
					 ASL_LEVEL_DEBUG, strerror(errno));
		return NULL;
	}
	maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	result = calloc(maxLen, 1);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Unicode string - skipping, syserr = %s", 
					 ASL_LEVEL_DEBUG, strerror(errno));
		CFRelease(s);
		return NULL;
	}
	if (!CFStringGetCString(s, result, maxLen, kCFStringEncodingUTF8)) {
		smb_log_info("CFStringGetCString failed on Unicode string - skipping, syserr = %s", 
					 ASL_LEVEL_DEBUG, strerror(errno));
		CFRelease(s);
		free(result);
		return NULL;
	}
	CFRelease(s);
	return result;
}

/*
 * Convert UTF-8 string to little-endian Unicode.
 * XXX - <rdar://problem/7518600>  will clean this up
*/
unsigned short *
convert_utf8_to_leunicode(const char *utf8_string)
{
	CFStringRef s;
	CFIndex maxlen;
	unsigned short *result;
	CFRange range;
	int i;

	s = CFStringCreateWithCString(NULL, utf8_string,
	     kCFStringEncodingUTF8);
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCString for UTF-8 failed on \"%s\", syserr = %s",
					 ASL_LEVEL_DEBUG, utf8_string, strerror(errno));
		return NULL;
	}

	maxlen = CFStringGetLength(s);
	result = malloc(2*(maxlen + 1));
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Unicode string for \"%s\" - skipping, syserr = %s", 
				ASL_LEVEL_DEBUG, utf8_string, strerror(errno));
		CFRelease(s);
		return NULL;
	}
	range.location = 0;
	range.length = maxlen;
	CFStringGetCharacters(s, range, result);
	for (i = 0; i < maxlen; i++)
		result[i] = CFSwapInt16HostToLittle(result[i]);
	result[maxlen] = 0;
	CFRelease(s);
	return result;
}
