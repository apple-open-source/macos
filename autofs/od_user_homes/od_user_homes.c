/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <syslog.h>
#include <OpenDirectory/OpenDirectory.h>

/*
 * Get the C-string length of a CFString.
 */
static inline CFIndex
od_cfstrlen(CFStringRef cfstr)
{
	return (CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
	    kCFStringEncodingUTF8));
}

/*
 * Given a CFString and its C-string length, copy it to a buffer.
 */
static inline Boolean
od_cfstrlcpy(char *string, CFStringRef cfstr, size_t size)
{
	return (CFStringGetCString(cfstr, string, (CFIndex)size,
	    kCFStringEncodingUTF8));
}

/*
 * Get a C string from a CFStringRef.
 * The string is allocated with malloc(), and must be freed when it's
 * no longer needed.
 */
static char *
od_CFStringtoCString(CFStringRef cfstr)
{
	char *string;
	CFIndex length;

	length = od_cfstrlen(cfstr);
	string = malloc(length + 1);
	if (string == NULL)
		return (NULL);
	if (!od_cfstrlcpy(string, cfstr, length + 1)) {
		free(string);
		return (NULL);
	}
	return (string);
}

static char *
od_get_error_string(CFErrorRef err)
{
	CFStringRef errstringref;
	char *errstring;

	if (err != NULL) {
		errstringref = CFErrorCopyDescription(err);
		errstring = od_CFStringtoCString(errstringref);
		CFRelease(errstringref);
	} else
		errstring = strdup("Unknown error");
	return (errstring);
}

/*
 * Given a CFStringRef for the HomeDirectory attribute:
 *
 * if it has a <path> part and it's non-trivial, reject it, as that means
 * that what should be mounted is not the home directory but the share
 * above that, and that doesn't work with /home;
 *
 * otherwise, extract the <url> part.
 *
 * Return a CFMutableStringRef for the URL on success, NULL on failure.
 */
static CFMutableStringRef
get_home_dir_url(CFStringRef homedir)
{
	CFRange homedir_range, item_start, item_stop, item_actual;
	CFStringRef substring;
	CFMutableStringRef url;

	homedir_range = CFRangeMake(0, CFStringGetLength(homedir));

	/*
	 * Do we have a path and, if we do, is it non-trivial?
	 * If so, fail.
	 */
	if (CFStringFindWithOptions(homedir, CFSTR("<path>"), homedir_range,
				    kCFCompareCaseInsensitive, &item_start) &&
	    CFStringFindWithOptions(homedir, CFSTR("</path>"), homedir_range,
				    kCFCompareCaseInsensitive, &item_stop)) {
		item_actual = CFRangeMake(item_start.location + item_start.length,
					  item_stop.location - (item_start.location + item_start.length));
		substring = CFStringCreateWithSubstring(kCFAllocatorDefault,
		   homedir, item_actual);
		if (substring == NULL) {
			fprintf(stderr, "od_user_homes: CFStringCreateWithSubstring() failed for path\n");
			return (NULL);
		}
		if (CFStringCompare(substring, CFSTR(""), 0) != kCFCompareEqualTo &&
		    CFStringCompare(substring, CFSTR("/"), 0) != kCFCompareEqualTo) {
			/*
			 * We have a path, and it's neither empty nor /;
			 * that means this user record is set up under
			 * the assumption that what will be mounted is
			 * the share containing the home directory, not
			 * just the home directory, and that won't work
			 * with /home.
			 */
			CFRelease(substring);
			return (NULL);
		}
		CFRelease(substring);
	}

	/*
	 * Extract the URL.
	 */
	if (CFStringFindWithOptions(homedir, CFSTR("<url>"), homedir_range,
				    kCFCompareCaseInsensitive, &item_start) &&
	    CFStringFindWithOptions(homedir, CFSTR("</url>"), homedir_range,
				    kCFCompareCaseInsensitive, &item_stop)) {
		item_actual = CFRangeMake(item_start.location + item_start.length,
					  item_stop.location - (item_start.location + item_start.length));
		substring = CFStringCreateWithSubstring(kCFAllocatorDefault,
		    homedir, item_actual);
		if (substring == NULL) {
			fprintf(stderr, "od_user_homes: CFStringCreateWithSubstring() failed for URL\n");
			return (NULL);
		}
		url = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
		    substring);
		CFRelease(substring);
		return (url);
	} else {
		/*
		 * URL not found.
		 */
		return (NULL);
	}
}

int
main(int argc, char **argv)
{
	CFStringRef username;
	CFErrorRef error;
	char *errstring;
	ODNodeRef noderef;
	CFTypeRef attrs[] = { kODAttributeTypeMetaNodeLocation,
			      kODAttributeTypeHomeDirectory,
			      kODAttributeTypeNFSHomeDirectory };
	CFArrayRef attributes;
	ODQueryRef query;
	CFArrayRef results;
	CFIndex count;
	int status;
	CFIndex i;
	ODRecordRef record;
	CFArrayRef values;

	if (argc == 1) {
		/*
		 * We don't support listing entries.
		 */
		return 0;
	}
	if (argc != 2) {
		fprintf(stderr, "Usage: od_user_homes <username>\n");
		return 1;
	}

	username = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, argv[1],
	    kCFStringEncodingUTF8, kCFAllocatorNull);
	if (username == NULL) {
		fprintf(stderr, "od_user_home: Can't create CFString for user name\n");
		return 2;
	}
	error = NULL;
	noderef = ODNodeCreateWithNodeType(kCFAllocatorDefault,
	    kODSessionDefault, kODNodeTypeAuthentication, &error);
	if (noderef == NULL) {
		errstring = od_get_error_string(error);
		fprintf(stderr, "od_user_home: Can't create OD node: %s\n",
		    errstring);
		free(errstring);
		return 2;
	}
	attributes = CFArrayCreate(kCFAllocatorDefault,
	    attrs, sizeof(attrs)/sizeof(*attrs), &kCFTypeArrayCallBacks);
	query = ODQueryCreateWithNodeType(kCFAllocatorDefault,
	    kODNodeTypeAuthentication, kODRecordTypeUsers,
	    kODAttributeTypeRecordName, kODMatchEqualTo, username, attributes,
	    INT_MAX, &error);
	if (query == NULL) {
		errstring = od_get_error_string(error);
		fprintf(stderr, "od_user_home: Can't create OD query: %s\n",
		    errstring);
		free(errstring);
		return 2;
	}
	results = ODQueryCopyResults(query, FALSE, &error);
	if (results == NULL) {
		if (error != NULL) {
			errstring = od_get_error_string(error);
			fprintf(stderr, "od_user_home: Can't copy OD query results: %s\n",
			    errstring);
			free(errstring);
			return 2;
		}

		/*
		 * No results - no such user.  Return an error.
		 */
		return 2;
	}
	count = CFArrayGetCount(results);

	/*
	 * Scan the results until we find a URL or run out of results.
	 */
	status = 2;	/* assume failure */
	for (i = 0; i < count && status != 0; i++) {
		record = (ODRecordRef) CFArrayGetValueAtIndex(results, i);
		values = ODRecordCopyValues(record,
		    kODAttributeTypeHomeDirectory, &error);
		if (values == NULL) {
			if (error != NULL) {
				errstring = od_get_error_string(error);
				fprintf(stderr, "od_user_homes: Can't get kODAttributeTypeMetaNodeLocation from record: %s\n",
				    errstring);
				free(errstring);
				return 2;
			}

			/*
			 * This record doesn't happen to have a
			 * kODAttributeTypeHomeDirectory attribute;
			 * skip it.
			 */
			continue;
		} else {
			if (CFArrayGetCount(values) != 0) {
				CFStringRef homedir = CFArrayGetValueAtIndex(values, 0);
				if (CFGetTypeID(homedir) == CFStringGetTypeID()) {
					CFMutableStringRef url;
					char *urlstr;

					url = get_home_dir_url(homedir);
					if (url != NULL) {
						// if it's AFP, ensure we have no-user auth
						CFStringFindAndReplace(url,
						    CFSTR("afp://"),
						    CFSTR("afp://;AUTH=NO%20USER%20AUTHENT@"),
						    CFRangeMake(0, CFStringGetLength(url)),
						    0);
						urlstr = od_CFStringtoCString(url);
						if (urlstr == NULL) {
							fprintf(stderr, "od_user_homes: Can't convert path string to C string\n");
							return 2;
						}
						printf("%s\n", urlstr);
						free(urlstr);
						CFRelease(url);
						status = 0;	/* success */
					}
				}
			}
			CFRelease(values);
		}
	}
	CFRelease(results);
	CFRelease(query);
	CFRelease(attributes);
	CFRelease(noderef);
	CFRelease(username);
	
	return status;
}
