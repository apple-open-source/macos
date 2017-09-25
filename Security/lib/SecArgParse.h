/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#ifndef SecArgParse_h
#define SecArgParse_h

#include <getopt.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* This is a poor simulacrum of python's argparse library. To use, create an arguments struct, including an array of
 * argument elements, and pass the struct (along with argv+argc) to options_parse(). This is one-shot argument parsing:
 * you must set pointers for *argument and *flag in each option to receive the results of the argument parsing.
 *
 * Currently does not support:
 *  non-string arguments
 *  default values
 *  relationships between arguments
 *  detecting meaningless option configurations
 *
 * Example arguments:
 *     static struct argument options[] = {
 *       { .shortname='p', .longname="perfcounters", .flag=&perfCounters, .flagval=true, .description="Print performance counters"},
 *       { .longname="test", .flag=&test, .flagval=true, .description="test long option"},
 *       { .command="resync", .flag=&resync, .flagval=true, .description="Initiate a resync"},
 *       { .positional_name="positional", .positional_optional=false, .argument=&position, .description = "Positional argument" },
 *       { .shortname='a', .longname="asdf", .argname="number", .argument=&asdf, .description = "Long arg with argument" },
 *     };
 *
 *     static struct arguments args = {
 *       .programname="testctl",
 *       .description="Control and report",
 *       .arguments = options,
 *     };
 *
 *  Note: this library automatically adds a '-h' and a '--help' operation. Don't try to override this.
 */

struct argument {
    char   shortname;
    char*  longname;
    char*  command;
    char*  positional_name;
    bool   positional_optional;
    char*  argname;

    char** argument;
    int*   flag;
    int    flagval;
    char*  description;
};

struct arguments {
    char* programname;
    char* description;

    struct argument* arguments;
};

bool options_parse(int argc, char * const *argv, struct arguments* args);
void print_usage(struct arguments* args);

#endif /* SecArgParse_h */
