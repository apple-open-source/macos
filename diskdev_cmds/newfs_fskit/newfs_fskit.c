/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "../disklib/fskit_support.h"


#define DEFAULT_EXIT_CODE 1 /* Standard Error exit code for newfs */

static void usage();

static void usage(void) {
    fprintf(stderr, "Usage: newfs_fskit [--progress] [-t fstype] <options> device\n");
    fprintf(stderr, "\t--progress if passed it will display the progress of newfs (optional)\n");
    fprintf(stderr, "\t-t file system type (required) e.g. msdos\n");
    exit(DEFAULT_EXIT_CODE);
}

int main(int argc, const char * argv[]) {

    int ret;
    int flags = 0;
    int index = 1;
    bool typeOptionFound = false;
    bool typeValueFound = false;

    if (argc < 4) {
        usage();
    }

    // We are only supporting `newfs_fskit [--progress] -t fstype <otheroptions> someDisk`, different filesystem use different options, so we are going
    // to leave the option parsing to FSKit.
    // --progress is optional, if passed it will display the progress of newfs

    if (strcmp(argv[index], "--progress") == 0) {
        flags = SHOW_PROGRESS_FLAG;
        index++;
    }
    if (strcmp(argv[index], "-t") == 0) {
        typeOptionFound = true;
        index++;
    }
    if (strncmp("-", argv[index], 1) != 0) {
        typeValueFound = true;
    }

    if(!typeOptionFound || !typeValueFound) {
        errx(1, "No file system type was provided");
    }

    // invoke_tool_from_fskit expects arguments to look like "fstype <other_options> disk", so we are ignoring `newfs_fskit` and `-t` arguments.
    argc -= index;
    argv += index;

    ret = invoke_tool_from_fskit(format_fs_op, flags, argc, argv);
    exit(ret);

}
