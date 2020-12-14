/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import "FakeXCTest.h"
#import <getopt.h>
#import "json_support.h"


extern char g_test_url1[1024];
extern char g_test_url2[1024];
extern char g_test_url3[1024];

CFMutableDictionaryRef json_dict = NULL;
int json = 0;

static void
usage(void)
{
    fprintf (stderr, "usage: smbclient_test [-f JSON] [-l testName] [-o output_file] -1 URL1 -2 URL2 -3 SMB1-URL [-h] \n\
             -f%s Print info in the provided format. Supported formats: JSON \n\
             -l%s Limit run to only testName \n\
             -o%s Filename to write JSON output to \n\
             -1%s Used for single user testing. Format of smb://domain;user:password@server/share \n\
             -2%s Has to be a different user to the same server/share used in URL1. Format of smb://domain;user2:password@server/share \n\
             -3%s Has to be a CIFS URL to a server that supports SMB v1. Format of cifs://domain;user:password@server/share   \n\
             \n",
             ",--format  ",
             ",--limit   ",
             ",--outfile ",
             ",--url1    ",
             ",--url2    ",
             ",--url3    "
             );
    exit(EINVAL);
}

int
main(int argc, char **argv) {
    int result = 1;
    int ch;
    char *output_file_path = NULL;
    FILE *fd = NULL;

    static struct option longopts[] = {
        { "format",     required_argument,      NULL,           'f' },
        { "limit",      required_argument,      NULL,           'l' },
        { "outfile",    required_argument,      NULL,           'o' },
        { "url1",       required_argument,      NULL,           '1' },
        { "url2",       required_argument,      NULL,           '2' },
        { "url3",       required_argument,      NULL,           '3' },
        { "help",       no_argument,            NULL,           'h' },
        { NULL,         0,                      NULL,           0   }
    };

    optind = 0;
    while ((ch = getopt_long(argc, argv, "f:l:o:1:2:3:h", longopts, NULL)) != -1) {
        switch (ch) {
            case 'f':
                if (strcasecmp(optarg, "json") == 0) {
                    json = 1;
                }
                else {
                    usage();
                }
                break;
            case 'l':
                [XCTest setLimit:optarg];
                break;
            case 'o':
                output_file_path = optarg;
                break;
            case '1':
                strlcpy(g_test_url1, optarg, sizeof(g_test_url1));
                break;
            case '2':
                strlcpy(g_test_url2, optarg, sizeof(g_test_url2));
                break;
            case '3':
                strlcpy(g_test_url3, optarg, sizeof(g_test_url2));
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }

    if (strnlen(g_test_url1, sizeof(g_test_url1)) == 0) {
        fprintf(stderr, "URL1 is null \n");
        usage();
    }

    if (strnlen(g_test_url2, sizeof(g_test_url2)) == 0) {
        fprintf(stderr, "URL2 is null \n");
        usage();
    }

    if (strnlen(g_test_url3, sizeof(g_test_url3)) == 0) {
        fprintf(stderr, "URL3 is null \n");
        usage();
    }

    json_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    if (json_dict == NULL) {
        fprintf(stderr, "*** %s: CFDictionaryCreateMutable failed\n",
                __FUNCTION__);
        return( ENOMEM );
    }

    if (json == 0) {
        /* Not using JSON */
        printf("URL1: <%s> \n", g_test_url1);
        printf("URL2: <%s> \n", g_test_url2);
        printf("SMB1 URL3: <%s> \n", g_test_url3);
    }

    @autoreleasepool {
        result = [XCTest runTests];
    }

    // If redirected stdout, then close it here before writing JSON
    if ( fd != NULL) {
        fclose(fd);
    }

    if (json == 1) {
        printf("\nWriting JSON data to <%s> \n",
               output_file_path == NULL ? "stdout" : output_file_path);
        // Dont CFRelease the dictionary after JSON printing
        json_print_cf_object(json_dict, output_file_path);
        printf("\n");
    }

    return result;
}

