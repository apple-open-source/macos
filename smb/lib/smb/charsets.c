/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved.
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
/*      @(#)charsets.c      *
 *      (c) 2004   Apple Computer, Inc.  All Rights Reserved
 *
 *
 *      charsets.c -- Routines converting between UTF-8, 16-bit
 *			little-endian Unicode, and various Windows
 *			code pages.
 *
 *      MODIFICATION HISTORY:
 *       28-Nov-2004     Guy Harris	New today
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <asl.h>
#include <sys/mchain.h>

#include <netsmb/smb_lib.h>

#include "charsets.h"

UInt32 gcodePage = 437;

void setcharset(const char *cp)
{
	UInt32 codePage = 0;

	/*
	 * Make sure cp exist and has something in it.
	 */
	if ((cp == NULL) || (*cp == 0))
		return;
	/* We expected it to start with CP, but SAMBA doesn't care so we shouldn't care.*/
	if (((*cp == 'C') || (*cp == 'c')) && ((*(cp+1) == 'P')|| (*(cp+1) == 'p')))
		cp += 2;
	else
		smb_log_info("setcharset expected 'CP%s' got '%s'?", 0, ASL_LEVEL_DEBUG, cp, cp);

	codePage = strtol(cp, NULL, 0);
	
	if (codePage == 0) {
		smb_log_info("setcharset '%s' ignored", errno, ASL_LEVEL_ERR, cp);
		return;
	}
	gcodePage = codePage;
}

static unsigned
xtoi(unsigned u)
{
        if (isdigit(u))
                return (u - '0'); 
        else if (islower(u))
                return (10 + u - 'a'); 
        else if (isupper(u))
                return (10 + u - 'A'); 
        return (16);
}

/* Really should use CFStringUppercase */
char * str_upper(char *dst, const char *src)
{
	char *p = dst;
	
	while (*src)
	*dst++ = toupper(*src++);
	*dst = 0;
	return p;
}

/* Really should use CFStringLowercase */
char * str_lower(char *dst, const char *src)
{
	char *p = dst;
	
	while (*src)
	*dst++ = tolower(*src++);
	*dst = 0;
	return p;
}

/* Removes the "%" escape sequences from a URL component.
 * See IETF RFC 2396.
 */
char *
unpercent(char * component)
{
        unsigned char c, *s;
        unsigned hi, lo; 

        if (component)
                for (s = (unsigned char *)component; (c = (unsigned char)*s); s++) {
                        if (c != '%') 
                                continue;
                        if ((hi = xtoi(s[1])) > 15 || (lo = xtoi(s[2])) > 15)
                                continue; /* ignore invalid escapes */
                        s[0] = hi*16 + lo;
                        /*      
                         * This was strcpy(s + 1, s + 3); 
                         * But nowadays leftward overlapping copies are
                         * officially undefined in C.  Ours seems to
                         * work or not depending upon alignment.
                         */      
                        memmove(s+1, s+3, (strlen((char *)(s+3))) + 1);
                }       
        return (component);
}

/*
 * We read gcodePage from the configuration file. It holds the code page number that needs to be convert it to the 
 * CF encoding number.
 */
CFStringEncoding windows_encoding( void )
{
	static int first_time = TRUE;
	
	CFStringEncoding encoding;
	
	encoding = CFStringConvertWindowsCodepageToEncoding(gcodePage);
	if (encoding == kCFStringEncodingInvalidId)
		encoding = CFStringGetSystemEncoding();	/* Punt nothing else we can do here */
	if (first_time) {
		smb_log_info("%s: encoding = %d gcodePage = %d", 0, ASL_LEVEL_DEBUG, __FUNCTION__, 
					 (u_int32_t)encoding, (u_int32_t)gcodePage);	
		first_time = FALSE;
	}
	return encoding;
}

/*
 * %%% - Change all strings to CFStringRef, once we remove the UI code.
 */
char *
convert_wincs_to_utf8(const char *windows_string)
{
	CFStringRef s;
	CFIndex maxlen;
	char *result;

	s = CFStringCreateWithCString(NULL, windows_string, 
		windows_encoding());
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCString for Windows code page failed on \"%s\" ",
						-1, ASL_LEVEL_DEBUG,  windows_string);

		/* kCFStringEncodingMacRoman should always succeed */
		s = CFStringCreateWithCString(NULL, windows_string, 
		    kCFStringEncodingMacRoman);
		if (s == NULL) {
			smb_log_info("CFStringCreateWithCString for Windows code page failed on \"%s\" with kCFStringEncodingMacRoman - skipping",
						-1, ASL_LEVEL_DEBUG, windows_string);
			return NULL;
		}
	}

	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	result = malloc(maxlen);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for UTF-8 string for \"%s\" - skipping", 
					-1, ASL_LEVEL_DEBUG, windows_string);
		CFRelease(s);
		return NULL;
	}
	if (!CFStringGetCString(s, result, maxlen, kCFStringEncodingUTF8)) {
		smb_log_info("CFStringGetCString for UTF-8 failed on \"%s\" - skipping",
					-1, ASL_LEVEL_DEBUG, windows_string);
		CFRelease(s);
		return NULL;
	}
	CFRelease(s);
	return result;
}

/*
 * %%% - Change all strings to CFStringRef, once we remove the UI code.
 */
char *
convert_utf8_to_wincs(const char *utf8_string)
{
	CFStringRef s;
	CFIndex maxlen;
	char *result;

	s = CFStringCreateWithCString(NULL, utf8_string, kCFStringEncodingUTF8);
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCString for UTF-8 failed on \"%s\"", 
					-1, ASL_LEVEL_DEBUG, utf8_string);
		return NULL;
	}

	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    windows_encoding()) + 1;
	result = malloc(maxlen);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Windows code page string for \"%s\" - skipping", 
					-1, ASL_LEVEL_DEBUG, utf8_string);
		CFRelease(s);
		return NULL;
	}
	if (!CFStringGetCString(s, result, maxlen,
	    windows_encoding())) {
		smb_log_info("CFStringGetCString for Windows code page failed on \"%s\" - skipping",
					-1, ASL_LEVEL_DEBUG, utf8_string);
		CFRelease(s);
		return NULL;
	}
	CFRelease(s);
	return result;
}

/*
 * Convert little-endian Unicode string to UTF-8.
 * Converts the Unicode string to host byte order in place.
 */
char *
convert_leunicode_to_utf8(unsigned short *unicode_string)
{
	unsigned short *unicode_charp, unicode_char;

	for (unicode_charp = unicode_string;
	    (unicode_char = *unicode_charp) != 0;
	    unicode_charp++)
		*unicode_charp = CFSwapInt16LittleToHost(unicode_char);
	return convert_unicode_to_utf8(unicode_string);
}

/*
 * Convert Unicode string to UTF-8.
 */
char *
convert_unicode_to_utf8(unsigned short *unicode_string)
{
	size_t uslen;
	CFStringRef s;
	CFIndex maxlen;
	char *result;

	for (uslen = 0; unicode_string[uslen] != 0; uslen++)
		;
	s = CFStringCreateWithCharacters(NULL, unicode_string, uslen);
	if (s == NULL) {
		smb_log_info("CFStringCreateWithCharacters failed", -1, ASL_LEVEL_DEBUG);
		return NULL;
	}

	maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	result = malloc(maxlen);
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Unicode string - skipping", -1, ASL_LEVEL_DEBUG);
		CFRelease(s);
		return NULL;
	}
	if (!CFStringGetCString(s, result, maxlen, kCFStringEncodingUTF8)) {
		smb_log_info("CFStringGetCString failed on Unicode string - skipping", -1, ASL_LEVEL_DEBUG);
		CFRelease(s);
		free(result);
		return NULL;
	}
	CFRelease(s);
	return result;
}

/*
 * Convert UTF-8 string to little-endian Unicode.
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
		smb_log_info("CFStringCreateWithCString for UTF-8 failed on \"%s\"", -1, ASL_LEVEL_DEBUG, utf8_string);
		return NULL;
	}

	maxlen = CFStringGetLength(s);
	result = malloc(2*(maxlen + 1));
	if (result == NULL) {
		smb_log_info("Couldn't allocate buffer for Unicode string for \"%s\" - skipping", 
				-1, ASL_LEVEL_DEBUG, utf8_string);
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
