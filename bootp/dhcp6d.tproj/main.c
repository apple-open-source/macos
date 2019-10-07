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

/*
 * main.c
 * - stateless DHCPv6 server main
 */
/* 
 * Modification History
 *
 * September 7, 2018		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dispatch/dispatch.h>
#include "DHCPv6Server.h"

int
main(int argc, char * argv[])
{
    const char *	config_file = NULL;
    DHCPv6ServerRef	server;
    dispatch_block_t	signal_block;
    dispatch_source_t	signal_source;

    if (argc > 1) {
	config_file = argv[1];
    }
    server = DHCPv6ServerCreate(config_file);
    if (server == NULL) {
	exit(1);
    }
    signal_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL,
					   SIGHUP,
					   0,
					   dispatch_get_main_queue());
    signal_block = ^{
	DHCPv6ServerUpdateConfiguration(server);
    };
    dispatch_source_set_event_handler(signal_source, signal_block);
    dispatch_resume(signal_source);
    signal(SIGHUP, SIG_IGN);
    dispatch_main();
    return (0);
}

