//
//  si-14-dateparse.c
//  regressions
//
//  Created by Michael Brouwer on 5/5/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecInternal.h>
#include <libDER/asn1Types.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }

#define dateparse(TTYPE, DATE)  SecAbsoluteTimeFromDateContent(ASN1_ ## TTYPE ## _TIME, (const uint8_t *)DATE, sizeof(DATE) - 1); \

#define dateequals(TTYPE, DATE, EXPECTED)  do { \
    UInt8 string[64]; \
    CFIndex string_len = 63; \
    CFAbsoluteTime at = dateparse(TTYPE, DATE); \
    if (at == NULL_TIME) { \
        eq_string("NULL_TIME", EXPECTED, "input: " DATE); \
        break; \
    } \
    ds = CFDateFormatterCreateStringWithAbsoluteTime(NULL, df, at); \
    CFStringGetBytes(ds, CFRangeMake(0, CFStringGetLength(ds)), kCFStringEncodingASCII, ' ', false, string, 63, &string_len); \
    string[string_len] = 0; \
    eq_string((const char *)string, EXPECTED, "input: " DATE); \
    CFReleaseSafe(ds); \
} while(0)

/* Test SecAbsoluteTimeFromDateContent. */
static void tests(void)
{
    CFLocaleRef ls = CFLocaleGetSystem();
    CFDateFormatterRef df = CFDateFormatterCreate(NULL, ls, kCFDateFormatterMediumStyle, kCFDateFormatterLongStyle);
    CFStringRef ds = NULL;
    CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0.0);
    if (tz) {
        CFDateFormatterSetProperty(df, kCFDateFormatterTimeZone, tz);
        CFRelease(tz);
    }
    CFDateFormatterSetFormat(df, CFSTR("yyyyMMddHHmmss"));

    dateequals(UTC, "1101010000Z", "20110101000000");
    dateequals(UTC, "010101000001Z", "20010101000001");
    dateequals(GENERALIZED, "201101010000Z", "NULL_TIME"); // Invalid date
    dateequals(GENERALIZED, "20010101000001Z", "20010101000001");

    dateequals(GENERALIZED, "20000101000000Z", "20000101000000");
    dateequals(UTC, "990101000000Z", "19990101000000");
    dateequals(UTC, "710101000000Z", "19710101000000");

    dateequals(UTC, "020101000000+0000", "20020101000000");
    dateequals(UTC, "020101000000-0800", "20020101080000");
    dateequals(UTC, "020101000000+0800", "20011231160000");
    dateequals(UTC, "020101000000-0420", "20020101042000");
    dateequals(UTC, "020101000013+0430", "20011231193013");

    dateequals(UTC, "0201010000+0000", "NULL_TIME");

    dateequals(GENERALIZED, "20020101000000+0000", "20020101000000");
    dateequals(GENERALIZED, "20020101000000-0800", "20020101080000");
    dateequals(GENERALIZED, "20020101000000+0800", "20011231160000");
    dateequals(GENERALIZED, "20020101000000-0420", "20020101042000");
    dateequals(GENERALIZED, "20020101000013+0430", "20011231193013");

    dateequals(GENERALIZED, "20060101000013+0900", "20051231150013");
    dateequals(GENERALIZED, "20090101000013+0900", "20081231150013");
    dateequals(GENERALIZED, "20110101000013+0900", "20101231150013");

    /* I'd expect these to be off by one second but since they aren't it
       seems we don't support leap seconds. */
    dateequals(GENERALIZED, "20051231200013-0900", "20060101050013");
    dateequals(GENERALIZED, "20081231200013-0900", "20090101050013");
    dateequals(GENERALIZED, "20101231200013-0900", "20110101050013");

    dateequals(GENERALIZED, "20051231200013+0900", "20051231110013");
    dateequals(GENERALIZED, "20081231200013+0900", "20081231110013");
    dateequals(GENERALIZED, "20101231200013+0900", "20101231110013");

    dateequals(GENERALIZED, "19001231200013Z", "19001231200013");
    dateequals(GENERALIZED, "19811231200013Z", "19811231200013");
    dateequals(GENERALIZED, "19840229000001Z", "19840229000001");
    dateequals(GENERALIZED, "19810229000002Z", "NULL_TIME"); // Feb 29 in a non leap-year
    dateequals(GENERALIZED, "20000001000000Z", "NULL_TIME"); // Month 0
    dateequals(GENERALIZED, "20000100000000Z", "NULL_TIME"); // Day 0
    dateequals(GENERALIZED, "20000131000000Z", "20000131000000"); // Day 1/31
    dateequals(GENERALIZED, "20000132000000Z", "NULL_TIME"); // Day 1/31
    dateequals(GENERALIZED, "20000229000000Z", "20000229000000"); // Day 2/29
    dateequals(GENERALIZED, "20000230000000Z", "NULL_TIME"); // Day 2/30
    dateequals(GENERALIZED, "20010331000000Z", "20010331000000");
    dateequals(GENERALIZED, "20010332000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010430000000Z", "20010430000000");
    dateequals(GENERALIZED, "20010431000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010531000000Z", "20010531000000");
    dateequals(GENERALIZED, "20010532000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010630000000Z", "20010630000000");
    dateequals(GENERALIZED, "20010631000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010731000000Z", "20010731000000");
    dateequals(GENERALIZED, "20010732000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010831000000Z", "20010831000000");
    dateequals(GENERALIZED, "20010832000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20010930000000Z", "20010930000000");
    dateequals(GENERALIZED, "20010931000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20011031000000Z", "20011031000000");
    dateequals(GENERALIZED, "20011032000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20011130000000Z", "20011130000000");
    dateequals(GENERALIZED, "20011131000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20011231000000Z", "20011231000000");
    dateequals(GENERALIZED, "20011232000000Z", "NULL_TIME");
    dateequals(GENERALIZED, "20011301000000Z", "NULL_TIME");

    dateequals(GENERALIZED, "21000301120000Z", "21000301120000");
    dateequals(GENERALIZED, "23000301120000Z", "23000301120000");
    dateequals(GENERALIZED, "24000301120000Z", "24000301120000");
    dateequals(GENERALIZED, "30000301120000Z", "30000301120000");
    dateequals(GENERALIZED, "60000228120000Z", "60000228120000");
    dateequals(GENERALIZED, "60800301120000Z", "60800301120000");

    CFReleaseSafe(df);
}

int si_14_dateparse(int argc, char *const *argv)
{
	plan_tests(64);

	tests();

	return 0;
}
