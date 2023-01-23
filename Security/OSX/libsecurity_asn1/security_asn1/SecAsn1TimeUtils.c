/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificatePriv.h>
#include <Security/X509Templates.h>

#include <utilities/SecCFRelease.h>

#include "plarenas.h"
#include "seccomon.h"
#include "secasn1.h"

#include "SecAsn1TimeUtils.h"

OSStatus SecAsn1DecodeTime(const SecAsn1Item* time, CFAbsoluteTime* date)
{
    CFErrorRef error = NULL;
    PLArenaPool* tmppoolp = NULL;
    OSStatus status = errSecSuccess;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL) {
        return errSecAllocate;
    }

    NSS_Time timeStr;
    if ((status = SEC_ASN1DecodeItem(tmppoolp, &timeStr, kSecAsn1TimeTemplate, time)) !=
        errSecSuccess) {
        goto errOut;
    }

    CFAbsoluteTime result = SecAbsoluteTimeFromDateContentWithError(timeStr.tag, timeStr.item.Data, timeStr.item.Length, &error);
    if (error) {
        status = (OSStatus)CFErrorGetCode(error);
        CFReleaseNull(error);
        goto errOut;
    }

    if (date) {
        *date = result;
    }

errOut:
    if (tmppoolp) {
        PORT_FreeArena(tmppoolp, PR_FALSE);
    }
    return status;
}

OSStatus SecAsn1EncodeTime(PLArenaPool *poolp, CFAbsoluteTime date, NSS_Time* asn1Time) {
    OSStatus result = errSecSuccess;
    CFLocaleRef locale = CFLocaleCreate(kCFAllocatorDefault, CFSTR("en_US"));
    CFDateFormatterRef dateFormatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterShortStyle, kCFDateFormatterShortStyle);
    CFTimeZoneRef timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
    CFDateFormatterSetProperty(dateFormatter, kCFDateFormatterTimeZone, timeZone);

    if (date < -1609459200.0 || //19500101000000Z
        date > 1546300799.0) {  //20491231235959Z
        CFDateFormatterSetFormat(dateFormatter, CFSTR("yyyyMMddHHmmss'Z'"));
        asn1Time->tag = SEC_ASN1_GENERALIZED_TIME;
    } else {
        CFDateFormatterSetFormat(dateFormatter, CFSTR("yyMMddHHmmss'Z'"));
        asn1Time->tag = SEC_ASN1_UTC_TIME;
    }

    CFStringRef dateString = CFDateFormatterCreateStringWithAbsoluteTime(NULL, dateFormatter, date);
    CFIndex stringLen = CFStringGetLength(dateString);
    if (stringLen < 0) {
        result = errSecAllocate;
        goto errOut;
    }
    asn1Time->item.Length = (size_t)stringLen;
    asn1Time->item.Data = PORT_ArenaAlloc(poolp, (size_t)stringLen);
    if (!asn1Time->item.Data) {
        result = errSecAllocate;
        goto errOut;
    }

    if (stringLen != CFStringGetBytes(dateString, CFRangeMake(0, stringLen), kCFStringEncodingUTF8, 0, false, asn1Time->item.Data, stringLen, NULL)) {
        result = errSecAllocate;
        goto errOut;
    }

errOut:
    CFReleaseNull(locale);
    CFReleaseNull(dateFormatter);
    CFReleaseNull(timeZone);
    CFReleaseNull(dateString);
    return result;
}
