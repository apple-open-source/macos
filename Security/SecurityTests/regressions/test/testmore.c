/*
 * Copyright (c) 2005 Apple Computer, Inc. All Rights Reserved.
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
#include <unistd.h>
#include <AvailabilityMacros.h>
#include <Security/cssmapple.h>

#include "testmore.h"

static int test_num = 0;
static int test_fails = 0;
static int test_cases = 0;

const char *test_directive = NULL;
const char *test_reason = NULL;

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
    if (test_num > test_cases)
    {
	test_skip(reason, test_cases - test_num, 0);
	exit(test_fails > 255 ? 255 : test_fails);
    }
}

static void test_plan_exit(void)
{
    int status = 0;
    fflush(stdout);

    if (!test_num)
    {
        if (test_cases)
        {
            fprintf(stderr, "# No tests run!\n");
            status = 255;
        }
        else
        {
            fprintf(stderr, "# Looks like your test died before it could "
                    "output anything.\n");
            status = 255;
        }
    }
    else if (test_num < test_cases)
    {
        fprintf(stderr, "# Looks like you planned %d tests but only ran %d.\n",
               test_cases, test_num);
        status = test_fails + test_cases - test_num;
    }
    else if (test_num > test_cases)
    {
        fprintf(stderr, "# Looks like you planned %d tests but ran %d extra.\n",
               test_cases, test_num - test_cases);
        status = test_fails;
    }
    else if (test_fails)
    {
        fprintf(stderr, "# Looks like you failed %d tests of %d.\n",
               test_fails, test_cases);
        status = test_fails;
    }

    fflush(stderr);
    if (status)
        _exit(status > 255 ? 255 : status);
}

void test_plan_tests(int count, const char *file, unsigned line)
{
    if (atexit(test_plan_exit) < 0)
    {
        fprintf(stderr, "failed to setup atexit handler: %s\n",
                strerror(errno));
        fflush(stderr);
        exit(255);
    }

	if (test_cases)
    {
        fprintf(stderr,
                "You tried to plan twice!  Second plan at %s line %u\n",
                file, line);
        fflush(stderr);
        exit(255);
    }
    else
	{
        if (!count)
        {
            fprintf(stderr, "You said to run 0 tests!  You've got to run "
                    "something.\n");
            fflush(stderr);
            exit(255);
        }

        test_cases = count;
		printf("1..%d\n", test_cases);
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
test_ok(int passed, const char *description, const char *directive,
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
			fprintf(stderr, "# SETUP not ok%s%s%s%s\n", 
				   description ? " - " : "",
				   description ? description : "",
				   reason ? " - " : "",
				   reason ? reason : "");
		}
	}
	else
	{
		if (!test_cases)
		{
			atexit(test_plan_exit);
			fprintf(stderr, "You tried to run a test without a plan!  "
					"Gotta have a plan. at %s line %u\n", file, line);
			fflush(stderr);
			exit(255);
		}

		++test_num;
		if (test_num > test_cases || (!passed && !is_todo))
			++test_fails;

		printf("%sok %d%s%s%s%s%s%s\n", passed ? "" : "not ", test_num,
			   description ? " - " : "",
			   description ? description : "",
			   directive ? " # " : "",
			   directive ? directive : "",
			   reason ? " " : "",
			   reason ? reason : "");
	}

    if (passed)
		fflush(stdout);
	else
    {
		va_list args;

		va_start(args, fmt);

		if (is_todo)
		{
			printf("#     Failed (TODO) test (%s at line %u)\n", file, line);
			if (fmt)
				vprintf(fmt, args);
			fflush(stdout);
		}
        else
		{
			fflush(stdout);
			fprintf(stderr, "#     Failed test (%s at line %u)\n", file, line);
			if (fmt)
				vfprintf(stderr, fmt, args);
			fflush(stderr);
		}

		va_end(args);
    }

    return passed;
}


const char *
sec_errstr(int err)
{
    if (err >= errSecErrnoBase && err <= errSecErrnoLimit)
        return strerror(err - 100000);

#ifdef MAC_OS_X_VERSION_10_4
    /* AvailabilityMacros.h would only define this if we are on a
       Tiger or later machine. */
    extern const char *cssmErrorString(long);
    return cssmErrorString(err);
#else
#if 0
    extern const char *_ZN8Security15cssmErrorStringEl(long);
    return _ZN8Security15cssmErrorStringEl(err);
#else
    return "";
#endif
#endif
}
