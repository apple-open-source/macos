/**
 * main.c - System Starter main
 * Wilfredo Sanchez | wsanchez@apple.com
 * $Apple$
 **
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 **/

#include <unistd.h>
#include <crt_externs.h>
#import  <CoreFoundation/CoreFoundation.h>
#include "Log.h"
#include "SystemStarter.h"

/* Command line options */
int gDebugFlag   = 0;
int gVerboseFlag = 0;
int gNoRunFlag   = 0;

static void usage() __attribute__((__noreturn__));
static void usage()
{
    char* aProgram = **_NSGetArgv();
    error(CFSTR("usage: %s [-vdDqn?]\n"), aProgram);
    exit(1);
}

int main (int argc, char *argv[])
{
    /* Open log facility */
    initLog();

    /**
     * Handle command line.
     **/
    {
        char c;
        while ((c = getopt(argc, argv, "vidDqn?")) != -1) {
            switch (c) {
	        /* Display Options */
                case 'v':
                    gVerboseFlag = 1;
                    break;

		/* Debugging Options */
                case 'd':
                    gDebugFlag   = 1;
                    break;
                case 'D':
                    gDebugFlag   = 2;
                    break;
                case 'q':
                    gDebugFlag   = 0;
                    break;
                case 'n':
                    gNoRunFlag   = 1;
                    break;

		/* Usage */
                case '?':
                    usage();
                    break;
                default:
                    warning(CFSTR("ignoring unknown option '-%c'\n"), c);
                    break;
            }
        }
        if (optind != argc)
            warning(CFSTR("ignoring excess arguments\n"));
    }

#if 0
    if (!gNoRunFlag && (getuid() != 0))
      {
        error(CFSTR("you must be root to run %s\n"), argv[0]);
        exit(1);
      }
#endif

    exit(system_starter());
}
