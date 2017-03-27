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
 * testmore.c
 */

#include <fcntl.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <AvailabilityMacros.h>

#include "testmore.h"
#include "testenv.h"

pthread_mutex_t test_mutex; // protects the test number variables
static int test_fails = 0;
static int test_todo_pass = 0;
static int test_todo = 0;
static int test_num = 0;
static int test_cases = 0;
static int test_plan_line = 0;
static const char *test_plan_file = NULL;

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

static void cffprint_v(FILE *file, CFStringRef fmt, va_list args);
static void cffprint_c_v(FILE *file, const char *fmt, va_list args);

static void cffprint_v(FILE *file, CFStringRef fmt, va_list args) {
    CFStringRef line = CFStringCreateWithFormatAndArguments(NULL, NULL, fmt, args);
    fprint_string(file, line);
    CFRelease(line);
}

static void cffprint_c_v(FILE *file, const char *fmt, va_list args) {
    CFStringRef cffmt = CFStringCreateWithCString(kCFAllocatorDefault, fmt, kCFStringEncodingUTF8);
    cffprint_v(file, cffmt, args);
    CFRelease(cffmt);
}

static void cffprint(FILE *file, CFStringRef fmt, ...) {
    va_list args;
    va_start(args, fmt);
    cffprint_v(file, fmt, args);
    va_end(args);
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
    fprintf(stdout, "[FAIL] BAIL OUT! (%s at line %u) %s\n", file, line, reason);
    fflush(stdout);
    exit(255);
}

void test_plan_skip_all(const char *reason)
{
    // Not super thread-safe. Don't test_plan_skip_all from multiple threads simultaneously.
    pthread_mutex_lock(&test_mutex);
    int skipN = test_cases - test_num;
    pthread_mutex_unlock(&test_mutex);

    if (skipN > 0)
    {
        test_skip(reason, skipN, 0);
    }
}

static const char *test_plan_name(void) {
    const char *plan_name = strrchr(test_plan_file, '/');
    plan_name = plan_name ? plan_name + 1 : test_plan_file;
    return plan_name;
}

static int test_plan_pass(void) {
    if (test_verbose) {
        const char *name = test_plan_name();
        fprintf(stdout, "[BEGIN] %s plan\n[PASS] %s plan\n", name, name);
        // Update counts for summary
        //test_num++;
        //test_cases++;
    }
    return 0;
}

static int test_plan_fail(CFStringRef reason, ...) {
    const char *name = test_plan_name();
    va_list ap;
    va_start(ap, reason);
    CFStringRef desc = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, reason, ap);
    cffprint(stdout, CFSTR("[BEGIN] %s plan\n%@[WARN] %s plan\n"), name, desc, name);
    CFRelease(desc);
    // Update counts for summary.  We consider test_plan_ok itself an unscheduled testcase for counts.
    //test_num++;
    //test_fails++;
    //test_cases++;
    return 1;
}

int test_plan_ok(void) {
    int status = 0;
    fflush(stderr);
    const char *name = test_plan_name();

    pthread_mutex_lock(&test_mutex);
    if (!test_num)
    {
        if (test_cases)
        {
            status = test_plan_fail(CFSTR("No tests run!\n"));
        }
        else
        {
            status = test_plan_fail(CFSTR("Looks like your test died before it could output anything.\n"));
        }
    }
    else if (test_num < test_cases)
    {
        status = test_plan_fail(CFSTR("Looks like you planned %d tests but only ran %d.\n"), test_cases, test_num);
    }
    else if (test_num > test_cases)
    {
        status = test_plan_fail(CFSTR("Looks like you planned %d tests but ran %d.\n"), test_cases, test_num);
    } else if (!test_fails) {
        status = test_plan_pass();
    }
    if (test_fails)
    {
        fprintf(stdout, "%s failed %d tests of %d.\n", name, test_fails, test_num);
        status = 1;
    }
    pthread_mutex_unlock(&test_mutex);
    fflush(stdout);

    return status;
}

// You should hold the test_mutex when you call this.
static void test_plan_reset(void) {
    test_fails = 0;
    test_todo_pass = 0;
    test_todo = 0;
    test_num = 0;
    test_cases = 0;
    test_plan_file = NULL;
    test_plan_line = 0;
}

void test_plan_final(int *failed, int *todo_pass, int *todo, int *actual, int *planned, const char **file, int *line) {
    pthread_mutex_lock(&test_mutex);
    if (failed)
        *failed = test_fails;
    if (todo_pass)
        *todo_pass = test_todo_pass;
    if (todo)
        *todo = test_todo;
    if (actual)
        *actual = test_num;
    if (planned)
        *planned = test_cases;
    if (file)
        *file = test_plan_file;
    if (line)
        *line = test_plan_line;

    test_plan_reset();
    pthread_mutex_unlock(&test_mutex);
}

void test_plan_tests(int count, const char *file, unsigned line) {
	if (test_cases)
    {
        fprintf(stdout,
                "[FAIL] You tried to plan twice!\n");
        
        fflush(stdout);
        exit(255);
    }
    else
	{
        if (!count)
        {
            fprintf(stdout, "[BEGIN] plan_tests\nYou said to run 0 tests!  "
                "You've got to run something.\n[WARN] plan_tests\n");
            fflush(stdout);
        }

        test_plan_file=file;
        test_plan_line=line;

        test_cases = count;

        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            if(pthread_mutex_init(&test_mutex, NULL) != 0) {
                fprintf(stdout, "Failed to initialize mutex: %d\n", errno);
            }
        });
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
		fputs("# ", stdout);
		if (fmt)
			vfprintf(stdout, fmt, args);
		fputs("\n", stdout);
		fflush(stdout);
	}

	va_end(args);

	return 1;
}

int
test_ok(int passed, __attribute((cf_consumed)) CFStringRef CF_CONSUMED description, const char *directive,
	const char *reason, const char *file, unsigned line,
	const char *fmt, ...)
{
	int is_todo = directive && !strcmp(directive, "TODO");
	int is_setup = directive && !is_todo && !strcmp(directive, "SETUP");

	if (is_setup)
	{
		if (!passed)
		{
			fflush(stderr);
            fprintf(stdout, "[BEGIN] SETUP\n");
            if (fmt) {
                va_list args;
                va_start(args, fmt);
                cffprint_c_v(stdout, fmt, args);
                va_end(args);
            }
            cffprint(stdout, CFSTR("[WARN] SETUP%s%@%s%s\n"),
				   description ? " - " : "",
				   description ? description : CFSTR(""),
				   reason ? " - " : "",
				   reason ? reason : "");
            fflush(stdout);
		}
	}
	else
	{
		if (!test_cases)
		{
            // Make having a plan optional? Commenting out the next 3 lines does - mb
			//fprintf(stdout, "[FAIL] You tried to run a test without a plan!  "
			//		"Gotta have a plan. at %s line %u\n", file, line);
			//fflush(stdout);
		}

        pthread_mutex_lock(&test_mutex);
		++test_num;
        if (passed) {
            if (is_todo) {
                test_todo_pass++;
            }
        } else if (is_todo) {
            test_todo++;
        } else {
            ++test_fails;
        }

        /* We only print this when a test fails, unless verbose is enabled */
        if ((!passed && !is_todo) || test_verbose > 0) {
            fflush(stderr);
            if (test_strict_bats) {
                cffprint(stdout, CFSTR("[BEGIN] %d%s%@\n"),
                         test_num,
                         description ? " - " : "",
                         description ? description : CFSTR(""));
            }
            if (is_todo && passed) {
                fprintf(stdout, "%s:%d: warning: Unexpectedly passed (TODO) test\n", file, line);
            } else if (is_todo && !passed) {
                /* Enable this to output TODO as warning */
                fprintf(stdout, "%s:%d: ok: Failed (TODO) test\n", file, line);
            } else if (!passed) {
                fprintf(stdout, "%s:%d: error: Failed test\n", file, line);
            }
            if (fmt) {
                va_list args;
                va_start(args, fmt);
                cffprint_c_v(stdout, fmt, args);
                va_end(args);
            }
            cffprint(stdout, CFSTR("[%s] %d%s%@%s%s%s%s\n"), passed ? (is_todo ? "PASS" : "PASS") : (is_todo ? "PASS" : "FAIL"),
                     test_num,
                     description ? " - " : "",
                     description ? description : CFSTR(""),
                     directive ? " # " : "",
                     directive ? directive : "",
                     reason ? " " : "",
                     reason ? reason : "");
            fflush(stdout);
        }
        pthread_mutex_unlock(&test_mutex);
    }

    if (description)
        CFRelease(description);

    return passed;
}


// TODO: Move this to testsec.h so that testmore and testenv can be shared
static void buf_kill(void* p) {
    free(p);
}

const char *
sec_errstr(int err)
{
    static pthread_key_t buffer0key;
    static pthread_key_t buffer1key;
    static pthread_key_t switchkey;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pthread_key_create(&buffer0key, buf_kill);
        pthread_key_create(&buffer1key, buf_kill);
        pthread_key_create(&switchkey, buf_kill);
    });

    uint32_t * switchp = (uint32_t*) pthread_getspecific(switchkey);
    if(switchp == NULL) {
        switchp = (uint32_t*) malloc(sizeof(uint32_t));
        *switchp = 0;
        pthread_setspecific(switchkey, switchp);
    }

    char* buf = NULL;

    pthread_key_t current = (*switchp) ? buffer0key : buffer1key;
    *switchp = !(*switchp);

    buf = pthread_getspecific(current);
    if(buf == NULL) {
        buf = (char*) malloc(20);
        pthread_setspecific(current, buf);
    }

    snprintf(buf, 20, "0x%X", err);
    return buf;
}
