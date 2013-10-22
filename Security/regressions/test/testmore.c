/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
 * testmore.c
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <AvailabilityMacros.h>

#include "testmore.h"
#include "testenv.h"

static int test_num = 0;
static int test_fails = 0;
static int test_cases = 0;
static const char *test_plan_file;
static int test_plan_line=0;

const char *test_directive = NULL;
const char *test_reason = NULL;

static void fprint_string(FILE *file, CFStringRef string) {
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

static void cffprint(FILE *file, CFStringRef fmt, ...) CF_FORMAT_FUNCTION(2,0);

static void cffprint(FILE *file, CFStringRef fmt, ...) {
    va_list args;
    va_start(args, fmt);
    CFStringRef line = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, args);
    va_end(args);
    fprint_string(file, line);
    CFRelease(line);
}

void test_skip(const char *reason, int how_many, int unless)
{
    if (unless)
        return;

    int done;
    for (done = 0; done < how_many; ++done)
        test_ok(1, NULL, "skip", reason, __FILE__, __LINE__, NULL);
}

void test_bail_out(const char *reason, const char *file, unsigned line)
{
    printf("BAIL OUT! (%s at line %u) %s\n", file, line, reason);
    fflush(stdout);
    exit(255);
}

void test_plan_skip_all(const char *reason)
{
    if (test_num < test_cases)
    {
        test_skip(reason, test_cases - test_num, 0);
    }
}

static int test_plan_exit(void)
{
    int status = 0;
    fflush(stdout);

    if (!test_num)
    {
        if (test_cases)
        {
            fprintf(stderr, "%s:%u: warning: No tests run!\n", test_plan_file, test_plan_line);
            status = 255;
        }
        else
        {
            fprintf(stderr, "%s:%u: error: Looks like your test died before it could "
                    "output anything.\n", test_plan_file, test_plan_line);
            status = 255;
        }
    }
    else if (test_num < test_cases)
    {
        fprintf(stderr, "%s:%u: warning: Looks like you planned %d tests but only ran %d.\n",
               test_plan_file, test_plan_line, test_cases, test_num);
        status = test_fails + test_cases - test_num;
    }
    else if (test_num > test_cases)
    {
        fprintf(stderr, "%s:%u: warning: Looks like you planned %d tests but ran %d extra.\n",
               test_plan_file, test_plan_line, test_cases, test_num - test_cases);
        status = test_fails;
    }
    else if (test_fails)
    {
        fprintf(stderr, "%s:%u: error: Looks like you failed %d tests of %d.\n",
               test_plan_file, test_plan_line, test_fails, test_cases);
        status = test_fails;
    }

    fflush(stderr);
    
    /* reset the test plan */
    test_num = 0;
    test_fails = 0;
    test_cases = 0;

    return status;
}

void test_plan_tests(int count, const char *file, unsigned line)
{
#if 0
    if (atexit(test_plan_exit) < 0)
    {
        fprintf(stderr, "failed to setup atexit handler: %s\n",
                strerror(errno));
        fflush(stderr);
        exit(255);
    }
#endif

	if (test_cases)
    {
        fprintf(stderr,
                "%s:%u: error: You tried to plan twice!\n",
                file, line);
        
        fflush(stderr);
        exit(255);
    }
    else
	{
        if (!count)
        {
            fprintf(stderr, "%s:%u: warning: You said to run 0 tests!  You've got to run "
                    "something.\n", file, line);
            fflush(stderr);
            exit(255);
        }

        test_plan_file=file;
        test_plan_line=line;
        
        test_cases = count;
		fprintf(stderr, "%s:%u: note: 1..%d\n", file, line, test_cases);
		fflush(stdout);
	}
}

int
test_diag(const char *directive, const char *reason,
	const char *file, unsigned line, const char *fmt, ...)
{
	int is_todo = directive && !strcmp(directive, "TODO");
	va_list args;

	va_start(args, fmt);

	if (is_todo)
	{
		fputs("# ", stdout);
		if (fmt)
			vprintf(fmt, args);
		fputs("\n", stdout);
		fflush(stdout);
	}
	else
	{
		fflush(stdout);
		fputs("# ", stderr);
		if (fmt)
			vfprintf(stderr, fmt, args);
		fputs("\n", stderr);
		fflush(stderr);
	}

	va_end(args);

	return 1;
}

int
test_ok(int passed, __attribute((cf_consumed)) CFStringRef description, const char *directive,
	const char *reason, const char *file, unsigned line,
	const char *fmt, ...)
{
	int is_todo = !passed && directive && !strcmp(directive, "TODO");
	int is_setup = directive && !is_todo && !strcmp(directive, "SETUP");

	if (is_setup)
	{
		if (!passed)
		{
			fflush(stdout);
            cffprint(stderr, CFSTR("# SETUP not ok%s%@%s%s\n"),
				   description ? " - " : "",
				   description ? description : CFSTR(""),
				   reason ? " - " : "",
				   reason ? reason : "");
		}
	}
	else
	{
		if (!test_cases)
		{
			atexit((void(*)(void))test_plan_exit);
			fprintf(stderr, "You tried to run a test without a plan!  "
					"Gotta have a plan. at %s line %u\n", file, line);
			fflush(stderr);
			exit(255);
		}

		++test_num;
		if (test_num > test_cases || (!passed && !is_todo))
			++test_fails;

        /* We only print this when a test fail, unless verbose is enabled */
        if ((!passed) || (test_verbose > 0)) {
            cffprint(stderr, CFSTR("%s:%u: note: %sok %d%s%@%s%s%s%s\n"),
                      file, line, passed ? "" : "not ", test_num,
                      description ? " - " : "",
                      description ? description : CFSTR(""),
                      directive ? " # " : "",
                      directive ? directive : "",
                      reason ? " " : "",
                      reason ? reason : "");
        }
    }

    if (passed)
		fflush(stdout);
	else
    {
		va_list args;

		va_start(args, fmt);

		if (is_todo)
		{
/* Enable this to output TODO as warning */
#if 0             
			printf("%s:%d: warning: Failed (TODO) test\n", file, line);
			if (fmt)
				vprintf(fmt, args);
#endif
			fflush(stdout);
		}
        else
		{
			fflush(stdout);
			fprintf(stderr, "%s:%d: error: Failed test\n", file, line);
			if (fmt)
				vfprintf(stderr, fmt, args);
			fflush(stderr);
		}

		va_end(args);
    }

    if (description)
        CFRelease(description);

    return passed;
}


const char *
sec_errstr(int err)
{
#if 1
	static int bufnum = 0;
    static char buf[2][20];
	bufnum = bufnum ? 0 : 1;
    sprintf(buf[bufnum], "0x%X", err);
    return buf[bufnum];
#else /* !1 */
    if (err >= errSecErrnoBase && err <= errSecErrnoLimit)
        return strerror(err - 100000);

#ifdef MAC_OS_X_VERSION_10_4
    /* AvailabilityMacros.h would only define this if we are on a
       Tiger or later machine. */
    extern const char *cssmErrorString(long);
    return cssmErrorString(err);
#else /* !defined(MAC_OS_X_VERSION_10_4) */
    extern const char *_ZN8Security15cssmErrorStringEl(long);
    return _ZN8Security15cssmErrorStringEl(err);
#endif /* MAC_OS_X_VERSION_10_4 */
#endif /* !1 */
}

/* run one test, described by test, return info in test struct */
int run_one_test(struct one_test_s *test, int argc, char * const *argv)
{
    struct timeval start, stop;
    
    if(test->entry==NULL) {
        printf("%s:%d: error: wtf?\n", __FILE__, __LINE__);
        return -1;
    }
    
    gettimeofday(&start, NULL);
    test->entry(argc, argv);
    gettimeofday(&stop, NULL);
    
    
    /* this may overflow... */
    test->duration=(stop.tv_sec-start.tv_sec)*1000+(stop.tv_usec/1000)-(start.tv_usec/1000);
    test->failed_tests=test_fails;

    return test_plan_exit();
 };
