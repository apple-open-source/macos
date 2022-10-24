//
//  XSDHParam.c
//  CoreDaemon
//
//  Created by Mike Abbott on 9/24/2013.
//  Copyright (c) 2013 Apple Inc.  All rights reserved.
//

#include "XSDHParam.h"
#include <Security/Security.h>
#include <Security/SecAsn1Coder.h>
#include <Security/../PrivateHeaders/keyTemplates.h>
#include <sys/stat.h>

static CFStringRef kXSDHBegin = CFSTR("-----BEGIN DH PARAMETERS-----");
static CFStringRef kXSDHEnd = CFSTR("-----END DH PARAMETERS-----");

static CFDataRef xsDHBase64Decode(CFStringRef str)
{
	CFDataRef decodedData = NULL;

	// could write yet another base64 decoder but this is less typo-prone (although slower)
	CFDataRef encodedData = CFStringCreateExternalRepresentation(NULL, str, kCFStringEncodingUTF8, 0);
	if (encodedData != NULL) {
		SecTransformRef decoder = SecDecodeTransformCreate(kSecBase64Encoding, NULL);
		if (decoder != NULL) {
			SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, encodedData, NULL);
			decodedData = SecTransformExecute(decoder, NULL);

			CFRelease(decoder);
		}

		CFRelease(encodedData);
	}

	return decodedData;
}

static CFArrayRef xsDHParamCreateFromString(CFStringRef str)
{
	CFMutableArrayRef result = NULL;

	CFArrayRef params = CFStringCreateArrayBySeparatingStrings(NULL, str, kXSDHBegin);
	if (params != NULL) {
		for (CFIndex i = 0; i < CFArrayGetCount(params); ++i) {
			CFMutableStringRef entry = CFStringCreateMutableCopy(NULL, 0, CFArrayGetValueAtIndex(params, i));
			if (entry != NULL) {
				CFStringTrimWhitespace(entry);
				CFRange end = CFStringFind(entry, kXSDHEnd, 0);
				if (end.location != kCFNotFound) {
					// remove end marker and anything after it
					end.length = CFStringGetLength(entry) - end.location;
					CFStringDelete(entry, end);

					CFDataRef data = xsDHBase64Decode(entry);
					if (data != NULL) {
						if (result == NULL)
							result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
						if (result != NULL)
							CFArrayAppendValue(result, data);

						CFRelease(data);
					}
				}

				CFRelease(entry);
			}
		}

		CFRelease(params);
	}

	return result;
}

CFArrayRef XSDHParamCreateFromFile(CFStringRef path)
{
	char pathbuf[PATH_MAX];
	if (!CFStringGetCString(path, pathbuf, sizeof pathbuf, kCFStringEncodingUTF8))
		return NULL;

	CFArrayRef result = NULL;

	int fd = open(pathbuf, O_RDONLY);
	if (fd >= 0) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (st.st_size == 0)
				result = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
			else if (st.st_size <= 10 * 1024 * 1024) {		// sanity check
				unsigned char *buf = malloc(st.st_size);
				if (buf != NULL) {
					if (read(fd, buf, st.st_size) == st.st_size) {
						CFStringRef string = CFStringCreateWithBytesNoCopy(NULL, buf, st.st_size, kCFStringEncodingUTF8, FALSE, kCFAllocatorNull);
						if (string != NULL) {
							result = xsDHParamCreateFromString(string);

							CFRelease(string);
						}
					}

					free(buf);
				}
			}
		}

		close(fd);
	}

	return result;
}

CFIndex XSDHParamGetSize(CFDataRef param)
{
	CFIndex result = 0;

	SecAsn1CoderRef coder = NULL;
	OSStatus status = SecAsn1CoderCreate(&coder);
	if (status == errSecSuccess) {
		NSS_DHParameter dhparam;
		memset(&dhparam, 0, sizeof dhparam);
		status = SecAsn1Decode(coder, CFDataGetBytePtr(param), CFDataGetLength(param), kSecAsn1DHParameterTemplate, &dhparam);
		if (status == errSecSuccess)
			result = dhparam.prime.Length * 8;

		SecAsn1CoderRelease(coder);
	}

	return result;
}
