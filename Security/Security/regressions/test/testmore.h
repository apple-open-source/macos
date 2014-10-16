/*
 * Copyright (c) 2005-2007,2012-2014 Apple Inc. All Rights Reserved.
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
 * testmore.h
 */

#ifndef _TESTMORE_H_
#define _TESTMORE_H_  1

#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

/* Macros to be used in testlist.h style headers. */
#define ONE_TEST(x) int x(int argc, char *const *argv);
#define DISABLED_ONE_TEST(x) ONE_TEST(x)
#define OFF_ONE_TEST(x) ONE_TEST(x)

typedef int (*one_test_entry)(int argc, char *const *argv);
    
#define ONE_TEST_ENTRY(x) int x(int argc, char *const *argv)
    
struct one_test_s {
    char *name;            /* test name */
    one_test_entry entry;  /* entry point */
    int off;               /* off by default */
    int sub_tests;         /* number of subtests */
    int failed_tests;      /* number of failed tests*/
    unsigned long duration; /* test duration in msecs */
    /* add more later: timing, etc... */
};

extern struct one_test_s testlist[];
    
int run_one_test(struct one_test_s *test, int argc, char * const *argv);

/* this test harnes rely on shadowing for TODO, SKIP and SETUP blocks */
#pragma GCC diagnostic ignored "-Wshadow"

#define test_create_description(TESTNAME, ...) \
    CFStringCreateWithFormat(NULL, NULL, CFSTR(TESTNAME), ## __VA_ARGS__)

#define ok(THIS, ...) \
({ \
    bool is_ok = !!(THIS); \
    test_ok(is_ok, test_create_description(__VA_ARGS__), test_directive, \
        test_reason, __FILE__, __LINE__, NULL); \
})
#define is(THIS, THAT, ...) \
({ \
    __typeof__(THIS) _this = (THIS); \
    __typeof__(THAT) _that = (THAT); \
    test_ok((_this == _that), test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
		"#          got: '%d'\n" \
		"#     expected: '%d'\n", \
		_this, _that); \
})
#define isnt(THIS, THAT, ...) \
	cmp_ok((THIS), !=, (THAT), __VA_ARGS__)
#define diag(MSG, ARGS...) \
	test_diag(test_directive, test_reason, __FILE__, __LINE__, MSG, ## ARGS)
#define cmp_ok(THIS, OP, THAT, ...) \
({ \
	__typeof__(THIS) _this = (THIS); \
	__typeof__(THAT) _that = (THAT); \
	test_ok((_this OP _that), test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
	   "#     '%d'\n" \
	   "#         " #OP "\n" \
	   "#     '%d'\n", \
	   _this, _that); \
})
#define eq_string(THIS, THAT, ...) \
({ \
	const char *_this = (THIS); \
	const char *_that = (THAT); \
	test_ok(!strcmp(_this, _that), test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
	   "#     '%s'\n" \
	   "#         eq\n" \
	   "#     '%s'\n", \
	   _this, _that); \
})
#define eq_stringn(THIS, THISLEN, THAT, THATLEN, ...) \
({ \
	__typeof__(THISLEN) _thislen = (THISLEN); \
	__typeof__(THATLEN) _thatlen = (THATLEN); \
	const char *_this = (THIS); \
	const char *_that = (THAT); \
	test_ok(_thislen == _thatlen && !strncmp(_this, _that, _thislen), \
		test_create_description(__VA_ARGS__), test_directive, test_reason, \
		__FILE__, __LINE__, \
	   "#     '%.*s'\n" \
	   "#         eq\n" \
	   "#     '%.*s'\n", \
	   (int)_thislen, _this, (int)_thatlen, _that); \
})
#define eq_cf(THIS, THAT, ...) \
({ \
    CFTypeRef _this = (THIS); \
    CFTypeRef _that = (THAT); \
    test_ok(CFEqualSafe(_this, _that), test_create_description(__VA_ARGS__), test_directive, test_reason, \
    __FILE__, __LINE__, \
    "#     '%@'\n" \
    "#      eq\n" \
    "#     '%@'\n", \
    _this, _that); \
})

#define like(THIS, REGEXP, ...) like_not_yet_implemented()
#define unlike(THIS, REGEXP, ...) unlike_not_yet_implemented()
#define is_deeply(STRUCT1, STRUCT2, ...) is_deeply_not_yet_implemented()
#define TODO switch(0) default
#define SKIP switch(0) default
#define SETUP switch(0) default
#define todo(REASON) const char *test_directive __attribute__((unused)) = "TODO", \
	*test_reason __attribute__((unused)) = (REASON)
#define skip(WHY, HOW_MANY, UNLESS) if (!(UNLESS)) \
    { test_skip((WHY), (HOW_MANY), 0); break; }
#define setup(REASON) const char *test_directive = "SETUP", \
	*test_reason = (REASON)
#define pass(...) ok(1, __VA_ARGS__)
#define fail(...) ok(0, __VA_ARGS__)
#define BAIL_OUT(WHY) test_bail_out(WHY, __FILE__, __LINE__)
#define plan_skip_all(REASON) test_plan_skip_all(REASON)
#define plan_tests(COUNT) test_plan_tests(COUNT, __FILE__, __LINE__)

#define ok_status(THIS, ...) \
({ \
	OSStatus _this = (THIS); \
	test_ok(!_this, test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
	   "#     status: %s(%" PRId32 ")\n", \
	   sec_errstr(_this), _this); \
})
#define is_status(THIS, THAT, ...) \
({ \
    OSStatus _this = (THIS); \
    OSStatus _that = (THAT); \
    test_ok(_this == _that, test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
	   "#          got: %s(%" PRId32 ")\n" \
	   "#     expected: %s(%" PRId32 ")\n", \
	   sec_errstr(_this), _this, sec_errstr(_that), _that); \
})
#define ok_unix(THIS, ...) \
({ \
    int _this = (THIS) < 0 ? errno : 0; \
    test_ok(!_this, test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
	   "#          got: %s(%d)\n", \
	   strerror(_this), _this); \
})
#define is_unix(THIS, THAT, ...) \
({ \
    int _result = (THIS); \
    int _this = _result < 0 ? errno : 0; \
    int _that = (THAT); \
    _that && _result < 0 \
	? test_ok(_this == _that, test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
		"#          got: %s(%d)\n" \
		"#     expected: %s(%d)\n", \
		strerror(_this), _this, strerror(_that), _that) \
	: test_ok(_this == _that, test_create_description(__VA_ARGS__), \
        test_directive, test_reason, __FILE__, __LINE__, \
		"#            got: %d\n" \
		"# expected errno: %s(%d)\n", \
		_result, strerror(_that), _that); \
})


extern const char *test_directive;
extern const char *test_reason;

void test_bail_out(const char *reason, const char *file, unsigned line);
int test_diag(const char *directive, const char *reason,
	const char *file, unsigned line, const char *fmt, ...);
int test_ok(int passed, __attribute((cf_consumed)) CFStringRef description, const char *directive,
	const char *reason, const char *file, unsigned line, const char *fmt, ...);
void test_plan_skip_all(const char *reason);
void test_plan_tests(int count, const char *file, unsigned line);
void test_skip(const char *reason, int how_many, int unless);

const char *sec_errstr(int err);

__END_DECLS

#endif /* !_TESTMORE_H_ */
