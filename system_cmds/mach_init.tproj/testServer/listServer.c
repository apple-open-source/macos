/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import "bootstrap.h"

#import <mach.h>
#import <stdarg.h>
#import <stdio.h>
#import <sys/boolean.h>

#define	NELEM(x)	(sizeof(x)/sizeof(x[0]))
#define	LAST_ELEMENT(x)	((x)[NELEM(x)-1])

print(const char *format, ...)
{
	va_list ap;
	
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

error(const char *format, ...)
{
	va_list ap;
	
	va_start(ap, format);
	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}


main()
{
	kern_return_t result;
	port_t bootstrap_port;
	name_array_t service_names;
	unsigned service_cnt, server_cnt, service_active_cnt;
	name_array_t server_names;
	boolean_t *service_actives;
	int i;
	
	result = task_get_bootstrap_port(task_self(), &bootstrap_port);
	if (result != KERN_SUCCESS) {
		error("Couldn't get bootstrap port: %d", result);
		exit(1);
	}
	if (bootstrap_port == PORT_NULL) {
		error("Invalid bootstrap port");
		exit(1);
	}

	result = bootstrap_info(bootstrap_port, &service_names, &service_cnt,
	  &server_names, &server_cnt, &service_actives, &service_active_cnt);
	if (result != BOOTSTRAP_SUCCESS)
		error("info failed: %d", result);
	else {
		for (i = 0; i < service_cnt; i++)
			print("Name: %-15s   Server: %-15s    Active: %-4s",
			 service_names[i],
			 server_names[i][0] == '\0' ? "Unknown" : server_names[i],
			 service_actives[i] ? "Yes" : "No");
	}
		
	exit(0);
}
	
	


