/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * July 17, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cfManager.h"


#ifdef	NOTNOW
/*
 * Opens a configuration file and returns a CFArrayRef consisting
 * of a CFStringRef for each line.
 */
__private_extern__
CFArrayRef
configRead(const char *path)
{
	int			fd;
	struct stat		statBuf;
	CFMutableDataRef	data;
	CFStringRef		str;
	CFArrayRef		config	= NULL;

	fd = open(path, O_RDONLY, 0644);
	if (fd == -1) {
		goto done;
	}
	if (fstat(fd, &statBuf) == -1) {
		goto done;
	}
	if ((statBuf.st_mode & S_IFMT) != S_IFREG) {
		goto done;
	}

	data = CFDataCreateMutable(NULL, statBuf.st_size);
	CFDataSetLength(data, statBuf.st_size);
	if(read(fd, (void *)CFDataGetMutableBytePtr(data), statBuf.st_size) != statBuf.st_size) {
		CFRelease(data);
		goto done;
	}

	str = CFStringCreateFromExternalRepresentation(NULL, data, kCFStringEncodingMacRoman);
	CFRelease(data);

	config = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR("\n"));
	CFRelease(str);

    done:

	if (fd != -1) {
		close(fd);
	}
	if (config == NULL) {
		CFMutableArrayRef	emptyConfig;

		/* pretend that we found an empty file */
		emptyConfig = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(emptyConfig, CFSTR(""));
		config = (CFArrayRef)emptyConfig;
	}
	return config;
}
#endif	/* NOTNOW */

/*
 * Writes a new configuration file with the contents of a CFArrayRef. Each
 * element of the array is a CFStringRef.
 */
__private_extern__
void
configWrite(const char *path, CFArrayRef config)
{
	CFStringRef	str;
	CFDataRef	data;
	int		fd = -1;
	int		len;

	str  = CFStringCreateByCombiningStrings(NULL, config, CFSTR("\n"));
	data = CFStringCreateExternalRepresentation(NULL, str, kCFStringEncodingMacRoman, (UInt8)'.');
	CFRelease(str);

	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		goto done;
	}

	len = CFDataGetLength(data);
	if (len) {
		(void)write(fd, CFDataGetBytePtr(data), len);
	}

    done:

	if (fd != -1)
		close(fd);
	CFRelease(data);
	return;
}


#ifdef	NOTNOW
__private_extern__
void
configSet(CFMutableArrayRef config, CFStringRef key, CFStringRef value)
{
	CFMutableStringRef	pref;
	CFIndex			configLen	= CFArrayGetCount(config);
	CFIndex			i;
	CFIndex			n;
	CFMutableStringRef	newValue;
	CFRange			range;
	CFStringRef		specialChars	= CFSTR(" \"'$!|\\<>`{}[]");
	boolean_t		mustQuote	= FALSE;

	/* create search prefix */
	pref = CFStringCreateMutableCopy(NULL, 0, key);
	CFStringAppend(pref, CFSTR("="));

	/*
	 * create new / replacement value
	 *
	 * Note: Since the configuration file may be processed by a
	 *       shell script we need to ensure that, if needed, the
	 *       string is properly quoted.
	 */
	newValue = CFStringCreateMutableCopy(NULL, 0, value);

	n = CFStringGetLength(specialChars);
	for (i = 0; i < n; i++) {
		CFStringRef	specialChar;

		specialChar = CFStringCreateWithSubstring(NULL, specialChars, CFRangeMake(i, 1));
		range = CFStringFind(newValue, specialChar, 0);
		CFRelease(specialChar);
		if (range.location != kCFNotFound) {
			/* this string has at least one special character */
			mustQuote = TRUE;
			break;
		}
	}

	if (mustQuote) {
		CFStringRef	charsToQuote	= CFSTR("\\\"$`");

		/*
		 * in addition to quoting the entire string we also need to escape the
		 * space " " and backslash "\" characters
		 */
		n = CFStringGetLength(charsToQuote);
		for (i = 0; i < n; i++) {
			CFStringRef	quoteChar;
			CFArrayRef	matches;

			quoteChar = CFStringCreateWithSubstring(NULL, charsToQuote, CFRangeMake(i, 1));
			range     = CFRangeMake(0, CFStringGetLength(newValue));
			matches   = CFStringCreateArrayWithFindResults(NULL,
								       newValue,
								       quoteChar,
								       range,
								       0);
			CFRelease(quoteChar);

			if (matches) {
				CFIndex	j = CFArrayGetCount(matches);

				while (--j >= 0) {
					const CFRange	*range;

					range = CFArrayGetValueAtIndex(matches, j);
					CFStringInsert(newValue, range->location, CFSTR("\\"));
				}
				CFRelease(matches);
			}
		}

		CFStringInsert(newValue, 0, CFSTR("\""));
		CFStringAppend(newValue, CFSTR("\""));
	}

	/* locate existing key */
	for (i = 0; i < configLen; i++) {
		if (CFStringHasPrefix(CFArrayGetValueAtIndex(config, i), pref)) {
			break;
		}
	}

	CFStringAppend(pref, newValue);
	CFRelease(newValue);
	if (i < configLen) {
		/* if replacing an existing entry */
		CFArraySetValueAtIndex(config, i, pref);
	} else {
		/*
		 * if new entry, insert it just prior to the last (emtpy)
		 * array element.
		 */
		CFArrayInsertValueAtIndex(config, configLen-1, pref);
	}
	CFRelease(pref);

	return;
}


__private_extern__
void
configRemove(CFMutableArrayRef config, CFStringRef key)
{
	CFMutableStringRef	pref;
	CFIndex			configLen = CFArrayGetCount(config);
	CFIndex			i;

	/* create search prefix */
	pref = CFStringCreateMutableCopy(NULL, 0, key);
	CFStringAppend(pref, CFSTR("="));

	/* locate existing key */
	for (i = 0; i < configLen; i++) {
		if (CFStringHasPrefix(CFArrayGetValueAtIndex(config, i), pref)) {
			/* entry found, remove it */
			CFArrayRemoveValueAtIndex(config, i);
			break;
		}
	}

	CFRelease(pref);
	return;
}
#endif	/* NOTNOW */
