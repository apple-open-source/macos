/*
 * Copyright (c) 2010-2012,2014 Apple Inc. All Rights Reserved.
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


#include "misc.h"


// NOTE: the return may or allocate a fair bit more space then it needs.
// Use it for short lived conversions (or strdup the result).
char *utf8(CFStringRef s) {
	CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
	CFIndex used = 0;
	UInt8 *buf = (UInt8 *)malloc(sz);
	if (!buf) {
		return NULL;
	}
	CFStringGetBytes(s, CFRangeMake(0, CFStringGetLength(s)), kCFStringEncodingUTF8, '?', FALSE, buf, sz, &used);
	buf[used] = 0;
	
	return (char*)buf;
}

void CFfprintf(FILE *f, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	
	CFStringRef fmt = CFStringCreateWithCString(NULL, format, kCFStringEncodingUTF8);
	CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, ap);
	va_end(ap);
	CFRelease(fmt);
	
	CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
	sz += 1;
	CFIndex used = 0;
	unsigned char *buf;
	bool needs_free = false;
	if (sz < 1024) {
		buf = alloca(sz);
	} else {
		buf = malloc(sz);
		needs_free = true;
	}
	if (buf) {
		CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, '?', FALSE, buf, sz, &used);
	} else {
		buf = (unsigned char *)"malloc failue during CFfprintf\n";
	}
	
	fwrite(buf, 1, used, f);
	if (needs_free) {
		free(buf);
	}
	
	CFRelease(str);
}

CFErrorRef fancy_error(CFStringRef domain, CFIndex code, CFStringRef description) {
	const void *v_ekey = kCFErrorDescriptionKey;
	const void *v_description = description;
	CFErrorRef err = CFErrorCreateWithUserInfoKeysAndValues(NULL, domain, code, &v_ekey, &v_description, 1);
	
	return err;
}

static
void add_t2ca(CFMutableDictionaryRef t2ca, CFStringRef t, CFStringRef a) {
	CFMutableSetRef ca = (CFMutableSetRef)CFDictionaryGetValue(t2ca, t);
	if (!ca) {
		ca = CFSetCreateMutable(NULL, 0, &kCFCopyStringSetCallBacks);
		CFDictionarySetValue(t2ca, t, ca);
	}
	CFSetAddValue(ca, a);
}

void CFSafeRelease(CFTypeRef object) {
	if (object) {
		CFRelease(object);
	}
}

void graphviz(FILE *f, SecTransformRef tr) {
	CFDictionaryRef d = SecTransformCopyExternalRepresentation(tr);
	
	CFfprintf(f, "digraph TR {\n\tnode [shape=plaintext];\n\n");
	CFIndex i, j;
	CFArrayRef transforms = CFDictionaryGetValue(d, CFSTR("TRANSFORMS"));
	CFArrayRef connections = CFDictionaryGetValue(d, CFSTR("ARRAY"));
	
	
	// map transforms to connected attributes
	CFMutableDictionaryRef t2ca = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	for(i = 0; i < CFArrayGetCount(connections); i++) {
		CFDictionaryRef c = CFArrayGetValueAtIndex(connections, i);
		
		CFStringRef t_from = CFDictionaryGetValue(c, CFSTR("FROM_NAME"));
		CFStringRef t_to = CFDictionaryGetValue(c, CFSTR("TO_NAME"));
		CFStringRef a_from = CFDictionaryGetValue(c, CFSTR("FROM_ATTRIBUTE"));
		CFStringRef a_to = CFDictionaryGetValue(c, CFSTR("TO_ATTRIBUTE"));
		
		add_t2ca(t2ca, t_from, a_from);
		add_t2ca(t2ca, t_to, a_to);
	}
	
	
	for(i = 0; i < CFArrayGetCount(transforms); i++) {
		CFDictionaryRef t = CFArrayGetValueAtIndex(transforms, i);
		// NAME STATE(dict) TYPE
		CFStringRef name = CFDictionaryGetValue(t, CFSTR("NAME"));		
		
		CFfprintf(f, "\tsubgraph \"cluster_%@\"{\n", name);
		
		CFMutableSetRef ca = (CFMutableSetRef)CFDictionaryGetValue(t2ca, name);
		if (ca) {
			CFIndex cnt = CFSetGetCount(ca);
			CFStringRef *attrs = malloc(cnt * sizeof(CFStringRef));
			CFSetGetValues(ca, (const void **)attrs);
			
			for(j = 0; j < cnt; j++) {
				CFfprintf(f, "\t\t\"%@#%@\" [label=\"%@\"];\n", name, attrs[j], attrs[j]);
			}
			CFfprintf(f, "\t\t\"%@\" [fontcolor=blue, fontsize=20];\n\t}\n");
		}
	}
	
	CFfprintf(f, "\n");
	
	for(i = 0; i < CFArrayGetCount(connections); i++) {
		CFDictionaryRef c = CFArrayGetValueAtIndex(connections, i);
		
		CFStringRef t_from = CFDictionaryGetValue(c, CFSTR("FROM_NAME"));
		CFStringRef t_to = CFDictionaryGetValue(c, CFSTR("TO_NAME"));
		CFStringRef a_from = CFDictionaryGetValue(c, CFSTR("FROM_ATTRIBUTE"));
		CFStringRef a_to = CFDictionaryGetValue(c, CFSTR("TO_ATTRIBUTE"));
		
		CFfprintf(f, "\t\"%@#%@\" -> \"%@#%@\";\n", t_from, a_from, t_to, a_to);
	}
	
	CFfprintf(f, "}\n");
	
	CFfprintf(f, "\n/*\n%@\n/*\n", d);
}
