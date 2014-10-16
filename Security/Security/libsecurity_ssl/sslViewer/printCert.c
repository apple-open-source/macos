/*
 * Copyright (c) 2003-2008,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * printCert.c - utility functions for printing certificate info
 */

#include "printCert.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>

void fprint_string(CFStringRef string, FILE *file) {
    UInt8 buf[256];
    CFRange range = { .location = 0 };
    range.length = CFStringGetLength(string);
    while (range.length > 0) {
        CFIndex bytesUsed = 0;
        CFIndex converted = CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, false, buf, sizeof(buf), &bytesUsed);
        fwrite(buf, 1, bytesUsed, file);
        range.length -= converted;
        range.location += converted;
    }
}

void print_line(CFStringRef line) {
    fprint_string(line, stdout);
    fputc('\n', stdout);
}

static void printPlist(CFArrayRef plist, CFIndex indent, CFIndex maxWidth) {
    CFIndex count = CFArrayGetCount(plist);
    CFIndex ix;
    for (ix = 0; ix < count ; ++ix) {
        CFDictionaryRef prop = (CFDictionaryRef)CFArrayGetValueAtIndex(plist,
            ix);
        CFStringRef pType = (CFStringRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyType);
        CFStringRef label = (CFStringRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyLabel);
        CFStringRef llabel = (CFStringRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyLocalizedLabel);
        CFTypeRef value = (CFTypeRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyValue);

        bool isSection = CFEqual(pType, kSecPropertyTypeSection);
        CFMutableStringRef line = CFStringCreateMutable(NULL, 0);
        CFIndex jx = 0;
        for (jx = 0; jx < indent; ++jx) {
            CFStringAppend(line, CFSTR("    "));
        }
        if (llabel) {
            CFStringAppend(line, llabel);
            if (!isSection) {
                for (jx = CFStringGetLength(llabel) + indent * 4;
                    jx < maxWidth; ++jx) {
                    CFStringAppend(line, CFSTR(" "));
                }
                CFStringAppend(line, CFSTR(" : "));
            }
        }
        if (CFEqual(pType, kSecPropertyTypeWarning)) {
            CFStringAppend(line, CFSTR("*WARNING* "));
            CFStringAppend(line, (CFStringRef)value);
        } else if (CFEqual(pType, kSecPropertyTypeError)) {
            CFStringAppend(line, CFSTR("*ERROR* "));
            CFStringAppend(line, (CFStringRef)value);
        } else if (CFEqual(pType, kSecPropertyTypeSuccess)) {
            CFStringAppend(line, CFSTR("*OK* "));
            CFStringAppend(line, (CFStringRef)value);
        } else if (CFEqual(pType, kSecPropertyTypeTitle)) {
            CFStringAppend(line, CFSTR("*"));
            CFStringAppend(line, (CFStringRef)value);
            CFStringAppend(line, CFSTR("*"));
        } else if (CFEqual(pType, kSecPropertyTypeSection)) {
        } else if (CFEqual(pType, kSecPropertyTypeData)) {
            CFDataRef data = (CFDataRef)value;
            CFIndex length = CFDataGetLength(data);
            if (length > 20)
                CFStringAppendFormat(line, NULL, CFSTR("[%d bytes] "), length);
            const UInt8 *bytes = CFDataGetBytePtr(data);
            for (jx = 0; jx < length; ++jx) {
                if (jx == 0)
                    CFStringAppendFormat(line, NULL, CFSTR("%02X"), bytes[jx]);
                else if (jx < 15 || length <= 20)
                    CFStringAppendFormat(line, NULL, CFSTR(" %02X"),
                        bytes[jx]);
                else {
                    CFStringAppend(line, CFSTR(" ..."));
                    break;
                }
            }
        } else if (CFEqual(pType, kSecPropertyTypeString)) {
            CFStringAppend(line, (CFStringRef)value);
        } else if (CFEqual(pType, kSecPropertyTypeDate)) {
            CFDateRef date = (CFDateRef)value;
            CFLocaleRef lc = CFLocaleCopyCurrent();
            CFDateFormatterRef df = CFDateFormatterCreate(NULL, lc, kCFDateFormatterMediumStyle, kCFDateFormatterLongStyle);
            CFStringRef ds;
            if (df) {
                CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0.0);
                CFDateFormatterSetProperty(df, kCFDateFormatterTimeZone, tz);
                CFRelease(tz);
                ds = CFDateFormatterCreateStringWithDate(NULL, df, date);
                CFRelease(df);
            } else {
                ds = CFStringCreateWithFormat(NULL, NULL, CFSTR("%g"), CFDateGetAbsoluteTime(date));
            }
            CFStringAppend(line, ds);
            CFRelease(ds);
            CFRelease(lc);
        } else if (CFEqual(pType, kSecPropertyTypeURL)) {
            CFURLRef url = (CFURLRef)value;
            CFStringAppend(line, CFSTR("<"));
            CFStringAppend(line, CFURLGetString(url));
            CFStringAppend(line, CFSTR(">"));
        } else {
            CFStringAppendFormat(line, NULL, CFSTR("*unknown type %@* = %@"),
            pType, value);
        }

		if (!isSection || label)
			print_line(line);
		CFRelease(line);
        if (isSection) {
            printPlist((CFArrayRef)value, indent + 1, maxWidth);
        }
    }
}

static CFIndex maxLabelWidth(CFArrayRef plist, CFIndex indent) {
    CFIndex count = CFArrayGetCount(plist);
    CFIndex ix;
    CFIndex maxWidth = 0;
    for (ix = 0; ix < count ; ++ix) {
        CFDictionaryRef prop = (CFDictionaryRef)CFArrayGetValueAtIndex(plist,
            ix);
        CFStringRef pType = (CFStringRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyType);
        CFStringRef llabel = (CFStringRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyLocalizedLabel);
        CFTypeRef value = (CFTypeRef)CFDictionaryGetValue(prop,
            kSecPropertyKeyValue);

        if (CFEqual(pType, kSecPropertyTypeSection)) {
            CFIndex width = maxLabelWidth((CFArrayRef)value, indent + 1);
            if (width > maxWidth)
                maxWidth = width;
        } else if (llabel) {
            CFIndex width = indent * 4 + CFStringGetLength(llabel);
            if (width > maxWidth)
                maxWidth = width;
        }
    }

    return maxWidth;
}

void print_plist(CFArrayRef plist) {
    if (plist)
        printPlist(plist, 0, maxLabelWidth(plist, 0));
    else
        printf("NULL plist\n");
}

void print_cert(SecCertificateRef cert, bool verbose) {
// TODO: merge these when all SecCertificate APIs are present on both iOS and OS X
#if TARGET_OS_IOS
    CFArrayRef plist;
    if (verbose)
        plist = SecCertificateCopyProperties(cert);
    else {
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        plist = SecCertificateCopySummaryProperties(cert, now);
    }

    CFStringRef subject = SecCertificateCopySubjectString(cert);
    if (subject) {
        print_line(subject);
        CFRelease(subject);
    } else {
        print_line(CFSTR("no subject"));
    }

    print_plist(plist);
    CFRelease(plist);
#else
    CFStringRef certName = NULL;
    OSStatus status = SecCertificateInferLabel(cert, &certName);
    if (certName) {
        print_line(certName);
        CFRelease(certName);
    }
    else {
        fprintf(stdout, "ERROR: unable to read certificate name\n");
    }
#endif
}
