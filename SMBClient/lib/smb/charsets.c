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
		os_log_debug(OS_LOG_DEFAULT, "CFStringCreateWithCString for Windows code page failed on \"%s\", syserr = %s",
						 windows_string, strerror(errno));

		/* kCFStringEncodingMacRoman should always succeed */
		s = CFStringCreateWithCString(NULL, windows_string, 
		    kCFStringEncodingMacRoman);
		if (s == NULL) {
			os_log_debug(OS_LOG_DEFAULT, "CFStringCreateWithCString for Windows code page failed on \"%s\" with kCFStringEncodingMacRoman - skipping, syserr = %s",
						windows_string, strerror(errno));
			return NULL;
		}
	}

	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
    
	result = malloc(maxlen);
	if (result == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "Couldn't allocate buffer for UTF-8 string for \"%s\" - skipping, syserr = %s",
					windows_string, strerror(errno));
		CFRelease(s);
		return NULL;
	}
    
	if (!CFStringGetCString(s, result, maxlen, kCFStringEncodingUTF8)) {
		os_log_debug(OS_LOG_DEFAULT, "CFStringGetCString for UTF-8 failed on \"%s\" - skipping, syserr = %s",
					windows_string, strerror(errno));
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
		os_log_debug(OS_LOG_DEFAULT, "CFStringCreateWithCString for UTF-8 failed on \"%s\", syserr = %s",
					utf8_string, strerror(errno));
		goto done;
	}

	if (uppercase) {
		CFStringUppercase(utfMutableStr, CFLocaleGetSystem());
	}
	
	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(utfMutableStr), codePage) + 1;
	result = malloc(maxlen);
	if (result == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "Couldn't allocate buffer for Windows code page string for \"%s\" - skipping, syserr = %s",
					utf8_string, strerror(errno));
		goto done;
	}
	if (!CFStringGetCString(utfMutableStr, result, maxlen, codePage)) {
		os_log_debug(OS_LOG_DEFAULT, "CFStringGetCString for Windows code page failed on \"%s\" - skipping, syserr = %s",
					utf8_string, strerror(errno));
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
	return convert_unicode_to_utf8(unicode_string, maxLen, 0);
}

/*
 * Convert Unicode string to UTF-8.
 */
char *
convert_unicode_to_utf8(const uint16_t *unicode_string, size_t maxLen, uint32_t decompose)
{
	size_t uslen;
	CFStringRef string_ref = NULL;
	char *return_stringp = NULL;
	
	 /* Number of characters not bytes */
	maxLen = maxLen / 2;
	for (uslen = 0; (unicode_string[uslen] != 0) && (uslen < maxLen); uslen++);
	
	/* Convert to CFString */
	string_ref = CFStringCreateWithCharacters(kCFAllocatorDefault,
											  unicode_string, uslen);
	if (string_ref == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "CFStringCreateWithCharacters failed, syserr = %s",
					 strerror(errno));
		goto bad;
	}

	if (decompose == 1) {
		/*
		 * Note that CFStringGetMaximumSizeOfFileSystemRepresentation returns a
		 * size that includes space for the zero termination
		 */
		maxLen = CFStringGetMaximumSizeOfFileSystemRepresentation(string_ref);
	}
	else {
		maxLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string_ref),
												   kCFStringEncodingUTF8) + 1;
	}
	
	return_stringp = calloc(maxLen, 1);
	if (return_stringp == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "Couldn't allocate buffer for Unicode string - skipping, syserr = %s",
					 strerror(errno));
		goto bad;
	}
	
	if (decompose == 1) {
		/*
		 * OS X uses decomposed unicode while Windows OS tends to use precomposed.
		 * Convert from possible precomposed to decomposed here.
		 */
		if (!CFStringGetFileSystemRepresentation(string_ref, return_stringp, maxLen)) {
			os_log_debug(OS_LOG_DEFAULT, "CFStringGetFileSystemRepresentation failed on Unicode string - skipping, syserr = %s",
						 strerror(errno));
			free(return_stringp);
			return_stringp = NULL;
			goto bad;
		}
	}
	else {
		if (!CFStringGetCString(string_ref, return_stringp, maxLen, kCFStringEncodingUTF8)) {
			os_log_debug(OS_LOG_DEFAULT, "CFStringGetCString failed on Unicode string - skipping, syserr = %s",
						 strerror(errno));
			free(return_stringp);
			return_stringp = NULL;
			goto bad;
		}
	}
	
bad:
	if (string_ref != NULL) {
		CFRelease(string_ref);
	}
	
	return (return_stringp);
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
		os_log_debug(OS_LOG_DEFAULT, "CFStringCreateWithCString for UTF-8 failed on \"%s\", syserr = %s",
					 utf8_string, strerror(errno));
		return NULL;
	}

	maxlen = CFStringGetLength(s);
	result = malloc(2*(maxlen + 1));
	if (result == NULL) {
		os_log_debug(OS_LOG_DEFAULT, "Couldn't allocate buffer for Unicode string for \"%s\" - skipping, syserr = %s",
				utf8_string, strerror(errno));
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
