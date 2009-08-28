/*
 * Copyright (c) 2007,2008 Apple Inc. All rights reserved.
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

#include "macros.hpp"
#include "common.hpp"

#include <cstdio>
#include <getopt.h>
#include <sys/stat.h>

static const struct option prefs_longopts[] =
{
    { "help", no_argument, NULL, 'h' },
    { "verbose", no_argument, NULL, 'v' },
    { "linger", no_argument, NULL, 'l' },
    { "force-sync", no_argument, NULL, 'y' },
    { "suspend-services", no_argument, NULL, 's' },
    { "restart-services", no_argument, NULL, 'r' },
    { "changes-pending", no_argument, NULL, 'c' },
    { "list-pending", no_argument, NULL, 'p' },
    { "list-defaults", no_argument, NULL, 'd' },

    { NULL, 0, NULL, 0 }
};

static const struct option shares_longopts[] =
{
    { "help", no_argument, NULL, 'h' },
    { "verbose", no_argument, NULL, 'v' },
    { "list-pending", no_argument, NULL, 'p' },
    { "enable-guest", no_argument, NULL, 'g' },

    { NULL, 0, NULL, 0 }
};

bool Options::Verbose = false;
bool Options::Debug = false;
bool Options::Linger = false;
bool Options::ForceSync = false;
bool Options::ForceSuspend = false;
bool Options::ForceRestart = false;
bool Options::DefaultGuest = false;

Options::command_type Options::Command = Options::SYNC;

static void prefs_help(void)
{
    static const char help[] =
"Usage: synchronize-preferences [options]\n"
"        --verbose                    print extra debugging messages\n"
"        --linger                     check in with launchd and  around and\n"
"                                     sync updates until idle\n"
"        --force-sync                 force synchronization even if it is\n"
"                                     unnecessary\n"
"        --restart-services           restart any services that are\n"
"                                     already running\n"
"        --suspend-services           leave all services disabled\n"
"        --changes-pending            exit with 0 status if there are\n"
"                                     unsynchronized changes\n"
"        --list-pending               print the pending configuration, but\n"
"                                     do not synchronize\n"
"        --list-defaults              print the default configuration and exit\n"
;

    std::fprintf(stdout, "%s", help);
}

static void shares_help(void)
{
    static const char help[] =
"Usage: synchronize-shares [options]\n"
"        --verbose                    print extra debugging messages\n"
"        --enable-guest               enable guest access by default\n"
"        --list-pending               print the pending configuration, but\n"
"                                     do not synchronize\n"
;

    std::fprintf(stdout, "%s", help);
}

void Options::parse_prefs(int argc, char * const * argv)
{
    int c;

    while ((c = ::getopt_long(argc, argv, "", prefs_longopts, NULL)) != -1) {
	switch (c) {
	case '?': exit(1); break; /* exit non-zero for unregocnised option */
	case 'h': prefs_help(); exit(0); break;

	case 'v': Options::Verbose = true; break;
	case 'l': Options::Linger = true; break;
	case 'y': Options::ForceSync = true; break;
	case 's': Options::ForceSuspend = true; break;
	case 'r': Options::ForceRestart = true; break;

	case 'c': Options::Command = Options::CHANGES_PENDING; break;
	case 'p': Options::Command = Options::LIST_PENDING; break;
	case 'd': Options::Command = Options::LIST_DEFAULTS; break;
	}
    }
}

void Options::parse_shares(int argc, char * const * argv)
{
    int c;

    while ((c = ::getopt_long(argc, argv, "", shares_longopts, NULL)) != -1) {
	switch (c) {
	case '?': exit(1); break; /* exit non-zero for unregocnised option */
	case 'h': shares_help(); exit(0); break;

	case 'v': Options::Verbose = true; break;
	case 'g': Options::DefaultGuest = true; break;

	case 'p': Options::Command = Options::LIST_PENDING; break;
	}
    }
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
