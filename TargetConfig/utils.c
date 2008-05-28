/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
// Kevin Van Vechten <kvv@apple.com>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils.h"

char*
cfstrdup(CFStringRef str) {
        char* result = NULL;
        if (str != NULL) {
                CFIndex length = CFStringGetLength(str);
                CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
                result = malloc(size+1);
                if (result != NULL) {
                        length = CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingUTF8, '?', 0, (UInt8*)result, size, NULL);
                        result[length] = 0;
                }
        }
        return result;
}

CFStringRef
cfstr(const char* str) {
        CFStringRef result = NULL;
        if (str) result = CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
        return result;
}

void
cfperror(CFStringRef str) {
        char* cstr = cfstrdup(str);
        if (cstr) {
                fprintf(stderr, "%s", cstr);
                free(cstr);
        }
}

CFPropertyListRef
read_plist(const char* path) {
        CFPropertyListRef result = NULL;
        int fd = open(path, O_RDONLY, (mode_t)0);
        if (fd != -1) {
                struct stat sb;
                if (stat(path, &sb) != -1) {
                        off_t size = sb.st_size;
                        void* buffer = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t)0);
                        if (buffer != (void*)-1) {
                                CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, buffer, size, kCFAllocatorNull);
                                if (data) {
                                        CFStringRef str = NULL;
                                        result = CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListMutableContainers, &str);
                                        CFRelease(data);
                                        if (result == NULL) {
                                                cfperror(str);
                                        }
                                }
                                munmap(buffer, size);
                        } else {
                                perror(path);
                        }
                }
                close(fd);
        } else {
                perror(path);
        }
        return result;
}


int
cfprintf(FILE* file, const char* format, ...) {
		char* cstr;
		int result;
        va_list args;
        va_start(args, format);
        CFStringRef formatStr = CFStringCreateWithCStringNoCopy(NULL, format, kCFStringEncodingUTF8, kCFAllocatorNull);
        CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, formatStr, args);
        va_end(args);
        cstr = cfstrdup(str);
        result = fprintf(file, "%s", cstr);
        free(cstr);
        CFRelease(str);
        CFRelease(formatStr);
		return result;
}

CFArrayRef
dictionaryGetSortedKeys(CFDictionaryRef dictionary) {
        CFIndex count = CFDictionaryGetCount(dictionary);

        const void** keys = malloc(sizeof(CFStringRef) * count);
        CFDictionaryGetKeysAndValues(dictionary, keys, NULL);
        CFArrayRef keysArray = CFArrayCreate(NULL, keys, count, &kCFTypeArrayCallBacks);
        CFMutableArrayRef sortedKeys = CFArrayCreateMutableCopy(NULL, count, keysArray);
        CFRelease(keysArray);
        free(keys);

        CFArraySortValues(sortedKeys, CFRangeMake(0, count), (CFComparatorFunction)CFStringCompare, 0);
        return sortedKeys;
}

void
dictionaryApplyFunctionSorted(CFDictionaryRef dict,
	CFDictionaryApplierFunction applier,
	void* context) {
	CFArrayRef keys = dictionaryGetSortedKeys(dict);
	if (keys) {
		CFIndex i, count = CFArrayGetCount(keys);
		for (i = 0; i < count; ++i) {
			CFTypeRef key = CFArrayGetValueAtIndex(keys, i);
			CFTypeRef val = CFDictionaryGetValue(dict, key);
			applier(key, val, context);
		}
		CFRelease(keys);
	}
}

int
writePlist(FILE* f, CFPropertyListRef p, int tabs) {
		int result = 0;
        CFTypeID type = CFGetTypeID(p);

        if (tabs == 0) {
                result += fprintf(f, "// !$*UTF8*$!\n");
        }

        char* t = malloc(tabs+1);
        int i;
        for (i = 0; i < tabs; ++i) {
                t[i] = '\t';
        }
        t[tabs] = 0;

        if (type == CFStringGetTypeID()) {
                char* utf8 = cfstrdup(p);
                int quote = 0;
                for (i = 0 ;; ++i) {
                        int c = utf8[i];
                        if (c == 0) break;
                        if (!((c >= 'A' && c <= 'Z') ||
                                  (c >= 'a' && c <= 'z') ||
                                  (c >= '0' && c <= '9') ||
                                  c == '/' ||
                                  c == '.' ||
                                  c == '_' )) {
                                quote = 1;
                                break;
                        }
                }
                if (utf8[0] == 0) quote = 1;

                if (quote) result += fprintf(f, "\"");
                for (i = 0 ;; ++i) {
                        int c = utf8[i];
                        if (c == 0) break;
                        if (c == '\"' || c == '\\') fprintf(f, "\\");
                        //if (c == '\n') c = 'n';
                        result += fprintf(f, "%c", c);
                }
                if (quote) result += fprintf(f, "\"");
                free(utf8);
        } else if (type == CFArrayGetTypeID()) {
                result += fprintf(f, "(\n");
                int count = CFArrayGetCount(p);
                for (i = 0; i < count; ++i) {
                        CFTypeRef pp = CFArrayGetValueAtIndex(p, i);
                        result += fprintf(f, "%s\t", t);
                        result += writePlist(f, pp, tabs+1);
                        result += fprintf(f, ",\n");
                }
                result += fprintf(f, "%s)", t);
        } else if (type == CFDictionaryGetTypeID()) {
                result += fprintf(f, "{\n");
                CFArrayRef keys = dictionaryGetSortedKeys(p);
                int count = CFArrayGetCount(keys);
                for (i = 0; i < count; ++i) {
                        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
						result += fprintf(f, "\t%s", t);
						result += writePlist(f, key, tabs+1);
                        result += fprintf(f, " = ");
                        result += writePlist(f, CFDictionaryGetValue(p,key), tabs+1);
                        result += fprintf(f, ";\n");
                }
                CFRelease(keys);
                result += fprintf(f, "%s}", t);
        }
        if (tabs == 0) result += fprintf(f, "\n");
        free(t);
		return result;
}

CFArrayRef
tokenizeString(CFStringRef str) {
	if (!str) return NULL;
	
	CFMutableArrayRef result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	CFCharacterSetRef whitespace = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR(" \t\n\r"));

	CFStringInlineBuffer buf;
	CFIndex i, length = CFStringGetLength(str);
	CFIndex start = 0;
	CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, length));
	for (i = 0; i < length; ++i) {
		UniChar c = CFStringGetCharacterFromInlineBuffer(&buf, i);
		if (CFCharacterSetIsCharacterMember(whitespace, c)) {
			if (start != kCFNotFound) {
				CFStringRef sub = CFStringCreateWithSubstring(NULL, str, CFRangeMake(start, i - start));
				start = kCFNotFound;
				CFArrayAppendValue(result, sub);
				CFRelease(sub);
			}
		} else if (start == kCFNotFound) {
			start = i;
		}
	}
	if (start != kCFNotFound) {
		CFStringRef sub = CFStringCreateWithSubstring(NULL, str, CFRangeMake(start, i - start));
		CFArrayAppendValue(result, sub);
		CFRelease(sub);
	}

	CFRelease(whitespace);

	return result;
}

// appends each element that does not already exist in the array
void
arrayAppendArrayDistinct(CFMutableArrayRef array, CFArrayRef other) {
	CFIndex i, count = CFArrayGetCount(other);
	CFRange range = CFRangeMake(0, CFArrayGetCount(array));
	for (i = 0; i < count; ++i) {
		CFTypeRef o = CFArrayGetValueAtIndex(other, i);
		if (!CFArrayContainsValue(array, range, o)) {
			CFArrayAppendValue(array, o);
		}
	}
}

int
is_file(const char* path) {
	struct stat sb;
	return (stat(path, &sb) == 0 && S_ISREG(sb.st_mode));
}
