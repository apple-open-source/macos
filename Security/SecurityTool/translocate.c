/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include <stdio.h>
#include <dirent.h>

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecTranslocate.h>

#include "translocate.h"

static CFURLRef CFURLfromPath(const char * path, Boolean isDir)
{
    return CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*)path, strlen(path), isDir);
}

static char * PathFromCFURL(CFURLRef url)
{
    char* path = malloc(PATH_MAX);

    if (!path)
    {
        goto done;
    }

    if (!CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, PATH_MAX))
    {
        free(path);
        path = NULL;
    }

done:
    return path;
}

static Boolean PathIsDir(const char * path)
{
    Boolean result = false;

    if(!path)
    {
        goto done;
    }

    DIR* d = opendir(path);

    if(d)
    {
        result = true;
        closedir(d);
    }

done:
    return result;
}

static void SafeCFRelease(CFTypeRef ref)
{
    if (ref)
    {
        CFRelease(ref);
    }
}

/* return 2 = bad args, anything else is ignored */

int translocate_create(int argc, char * const *argv)
{
    int result = -1;

    if (argc != 2)
    {
        return 2;
    }

    CFURLRef inUrl = CFURLfromPath(argv[1], PathIsDir(argv[1]));
    CFURLRef outUrl = NULL;
    CFErrorRef error = NULL;
    char* outPath = NULL;

    if(!inUrl)
    {
        printf("Error: failed to create url for: %s\n", argv[1]);
        goto done;
    }

    outUrl = SecTranslocateCreateSecureDirectoryForURL(inUrl, NULL, &error);

    if (!outUrl)
    {
        int err = (int)CFErrorGetCode(error);
        printf("Error: failed while trying to translocate %s (errno: %d, %s)\n", argv[1], err, strerror(err));
        goto done;
    }

    outPath = PathFromCFURL(outUrl);

    if( !outPath )
    {
        printf("Error: failed to convert out url to string for %s\n", argv[1]);
        goto done;
    }

    printf("Translocation point: (note if this is what you passed in then that path should not be translocated)\n\t%s\n",outPath);

    free(outPath);
    result = 0;

done:
    SafeCFRelease(inUrl);
    SafeCFRelease(outUrl);
    SafeCFRelease(error);

    return result;
}

int translocate_policy(int argc, char * const *argv)
{
    int result = -1;

    if (argc != 2)
    {
        return 2;
    }

    CFURLRef inUrl = CFURLfromPath(argv[1], PathIsDir(argv[1]));
    bool should = false;
    CFErrorRef error = NULL;

    if(!inUrl)
    {
        printf("Error: failed to create url for: %s\n", argv[1]);
        goto done;
    }

    if (!SecTranslocateURLShouldRunTranslocated(inUrl, &should, &error))
    {
        int err = (int)CFErrorGetCode(error);
        printf("Error: failed while trying to check policy for %s (errno: %d, %s)\n", argv[1], err, strerror(err));
        goto done;
    }

    printf("\t%s\n", should ? "Would translocate": "Would not translocate");

    result = 0;

done:
    SafeCFRelease(inUrl);
    SafeCFRelease(error);

    return result;
}

int translocate_check(int argc, char * const *argv)
{
    int result = -1;

    if (argc != 2)
    {
        return 2;
    }

    CFURLRef inUrl = CFURLfromPath(argv[1], PathIsDir(argv[1]));
    bool is = false;
    CFErrorRef error = NULL;

    if(!inUrl)
    {
        printf("Error: failed to create url for: %s\n", argv[1]);
        goto done;
    }

    if (!SecTranslocateIsTranslocatedURL(inUrl, &is, &error))
    {
        int err = (int)CFErrorGetCode(error);
        printf("Error: failed while trying to check status for %s (errno: %d, %s)\n", argv[1], err, strerror(err));
        goto done;
    }

    printf("\t%s\n", is ? "TRANSLOCATED": "NOT TRANSLOCATED");

    result = 0;

done:
    SafeCFRelease(inUrl);
    SafeCFRelease(error);

    return result;
}

int translocate_original_path(int argc, char * const * argv)
{
    int result = -1;

    if (argc != 2)
    {
        return 2;
    }

    CFURLRef inUrl = CFURLfromPath(argv[1], PathIsDir(argv[1]));
    CFURLRef outUrl = NULL;
    CFErrorRef error = NULL;
    char* outPath = NULL;

    if(!inUrl)
    {
        printf("Error: failed to create url for: %s\n", argv[1]);
        goto done;
    }

    outUrl = SecTranslocateCreateOriginalPathForURL(inUrl, &error);

    if (!outUrl)
    {
        int err = (int)CFErrorGetCode(error);
        printf("Error: failed while trying to find original path for %s (errno: %d, %s)\n", argv[1], err, strerror(err));
        goto done;
    }

    outPath = PathFromCFURL(outUrl);

    if( !outPath )
    {
        printf("Error: failed to convert out url to string for %s\n", argv[1]);
        goto done;
    }

    printf("Original Path: (note if this is what you passed in then that path is not translocated)\n\t%s\n",outPath);

    free(outPath);
    result = 0;

done:
    SafeCFRelease(inUrl);
    SafeCFRelease(outUrl);
    SafeCFRelease(error);

    return result;
}

