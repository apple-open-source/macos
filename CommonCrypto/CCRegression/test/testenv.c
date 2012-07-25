/*
 * Copyright (c) 2005-2007,2009-2010 Apple Inc. All Rights Reserved.
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

#include "testmore.h"
#include "testenv.h"

#if NO_SERVER
#include <securityd/spi.h>

static int current_dir = -1;
static char scratch_dir[50];
static char *home_var;
static bool keep_scratch_dir = false;

static int
rmdir_recursive(const char *path)
{
	char command_buf[256];
	if (strlen(path) + 10 > sizeof(command_buf) || strchr(path, '\''))
	{
		fprintf(stderr, "# rmdir_recursive: invalid path: %s", path);
		return -1;
	}

	sprintf(command_buf, "rm -rf '%s'", path);
	return system(command_buf);
}
#endif

static int tests_init(void) {
    printf("[TEST] CommonCrypto\n");
#if NO_SERVER
	char preferences_dir[80];
	char library_dir[70];

    securityd_init();

	setup("tests_init");
    
    /* Create scratch dir for tests to run in. */
    sprintf(scratch_dir, "/tmp/tst-%d", getpid());
    if (keep_scratch_dir) {
        printf("running tests with HOME=%s\n", scratch_dir);
    }
    sprintf(library_dir, "%s/Library", scratch_dir);
    sprintf(preferences_dir, "%s/Preferences", library_dir);
    return (ok_unix(mkdir(scratch_dir, 0755), "mkdir") &&
            ok_unix(current_dir = open(".", O_RDONLY), "open") &&
            ok_unix(chdir(scratch_dir), "chdir") &&
            ok_unix(setenv("HOME", scratch_dir, 1), "setenv") &&
            /* @@@ Work around a bug that the prefs code in
             libsecurity_keychain never creates the Library/Preferences
             dir. */
            ok_unix(mkdir(library_dir, 0755), "mkdir") &&
            ok_unix(mkdir(preferences_dir, 0755), "mkdir") &&
            ok(home_var = getenv("HOME"), "getenv"));
    
#else
    return 0;
#endif 
}

static int
tests_end(void)
{
    printf("[SUMMARY]\n"); 
#if NO_SERVER
	setup("tests_end");
	/* Restore previous cwd and remove scratch dir. */
	int ok = ok_unix(fchdir(current_dir), "fchdir");
	if (ok)
		ok = ok_unix(close(current_dir), "close");
	if (ok) {
		if (!keep_scratch_dir) {
			ok = ok_unix(rmdir_recursive(scratch_dir), "rmdir_recursive");
		}
	}
    
	return ok;
#else
    return 0;
#endif
}

static void usage(const char *progname)
{
    fprintf(stderr, "usage: %s [-k][-w][testname [testargs] ...]\n", progname);
    exit(1);
}

static int tests_run_index(int i, int argc, char * const *argv)
{
    int verbose = 0;
    int ch;

    while ((ch = getopt(argc, argv, "v")) != -1)
    {
        switch  (ch)
        {
            case 'v':
                verbose++;
                break;
            default:
                usage(argv[0]);
        }
    }

    fprintf(stderr, "[BEGIN] %s\n", testlist[i].name);
    
    run_one_test(&testlist[i], argc, argv);
    if(testlist[i].failed_tests) {
        fprintf(stderr, "[FAIL] %s\n", testlist[i].name);
    } else {
        fprintf(stderr, "duration: %u ms\n", testlist[i].duration);
        fprintf(stderr, "[PASS] %s\n", testlist[i].name);
    }
    
    return 0;
}

static int tests_named_index(const char *testcase)
{
    int i;

    for (i = 0; testlist[i].name; ++i) {
        if (strcmp(testlist[i].name, testcase) == 0) {
            return i;
        }
    }

    return -1;
}

static int tests_run_all(int argc, char * const *argv)
{
    int curroptind = optind;
    int i;

    for (i = 0; testlist[i].name; ++i) {
        tests_run_index(i, argc, argv);
        optind = curroptind;
    }
    
    return 0;
}

int
tests_begin(int argc, char * const *argv)
{
    const char *testcase = NULL;
    bool initialized = false;
    int testix = -1;
	int ch;

    for (;;) {
        while (!testcase && (ch = getopt(argc, argv, "kw")) != -1)
        {
            switch  (ch)
            {
#ifdef NO_SERVER
            case 'k':
                keep_scratch_dir = true;
                break;
#endif
            case 'w':
                sleep(100);
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
                initialized = true;
                tests_init();
                tests_run_all(argc, argv);
            }
            break;
        } else {
            if (!initialized) {
                tests_init();
                initialized = true;
            }
            optind++;
            tests_run_index(testix, argc, argv);
            testix = -1;
        }
    }
        
    /* Cleanups */
    tests_end();
    
    return 0;
}

