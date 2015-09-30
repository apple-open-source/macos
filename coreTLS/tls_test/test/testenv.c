/*
 * Copyright (c) 2005-2007,2009-2011 Apple Inc. All Rights Reserved.
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
 * testenv.c
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "testmore.h"
#include "testenv.h"

int test_verbose = 0;
int test_leaks = 1;

static int
tests_init(void) {
    return 0;
}

static int
tests_end(void)
{
    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "usage: %s [-k][-w][testname [testargs] ...]\n", progname);
    exit(1);
}

static int tests_run_index(int i, int argc, char * const *argv)
{
    int ch;

    while ((ch = getopt(argc, argv, "v")) != -1)
    {
        switch  (ch)
        {
            case 'v':
                test_verbose++;
                break;
            default:
                usage(argv[0]);
        }
    }

    fprintf(stderr, "[BEGIN]: Test Case '%s' started.\n", testlist[i].name);

    run_one_test(&testlist[i], argc, argv);
    if(testlist[i].failed_tests) {
        fprintf(stderr, "[FAIL]: Test Case '%s' failed.\n", testlist[i].name);
    } else {
        fprintf(stderr, "[PASS]: Test Case '%s' passed. (%lu ms)\n", testlist[i].name, testlist[i].duration);
    }
    return testlist[i].failed_tests;
}

static int strcmp_under_is_dash(const char *s, const char *t) {
    for (;;) {
        char a = *s++, b = *t++;
        if (a != b) {
            if (a != '_' || b != '-')
                return a - b;
        } else if (a == 0) {
            return 0;
        }
    }
}

static int tests_named_index(const char *testcase)
{
    int i;

    for (i = 0; testlist[i].name; ++i) {
        if (strcmp_under_is_dash(testlist[i].name, testcase) == 0) {
            return i;
        }
    }

    return -1;
}

static int tests_run_all(int argc, char * const *argv)
{
    int curroptind = optind;
    int i;
    int failcount=0;

    for (i = 0; testlist[i].name; ++i) {
        if(!testlist[i].off) {
            failcount+=tests_run_index(i, argc, argv);
            optind = curroptind;
        }
    }

    return failcount;
}

int
tests_begin(int argc, char * const *argv)
{
    const char *testcase = NULL;
    bool initialized = false;
    int testix = -1;
    int failcount = 0;
	int ch;
    int loop = 0;

    for (;;) {
        while (!testcase && (ch = getopt(argc, argv, "klLvws")) != -1)
        {
            switch  (ch)
            {
#ifdef NO_SERVER
            case 'k':
                keep_scratch_dir = true;
                break;
#endif
            case 'v':
                test_verbose++;
                break;
            case 'w':
                sleep(100);
                break;
            case 'l':
                loop = 1;
                break;
            case 'L': // don't run leaks.
                test_leaks=false;
                break;
            case '?':
            default:
                printf("invalid option %c\n",ch);
                usage(argv[0]);
            }
        }

        if (optind < argc) {
            testix = tests_named_index(argv[optind]);
            if(testix<0) {
                printf("invalid test %s\n",argv[optind]);
                usage(argv[0]);
            }
        }

        if (testix < 0) {
            if (!initialized) {
                tests_init();
                failcount+=tests_run_all(argc, argv);
            }
            break;
        } else {
            if (!initialized) {
                tests_init();
                initialized = true;
            }
            optind++;
            failcount+=tests_run_index(testix, argc, argv);
            testix = -1;
        }
    }

    fprintf(stderr, "[SUMMARY]\n");


    printf("Total failcount = %d\n", failcount);

    /* Cleanups */
    tests_end();

    if(loop) {
        printf("Looping until key press 'q'. You can run leaks now.\n");
        while(getchar()!='q');
    }

    return failcount;
}
