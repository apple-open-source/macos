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
/*
 * Bootstrap subset exercise.
 *
 *	do {
 *		create a subset port with requestor = req_port;
 * 		register "foo" on subset_port;
 *		deallocate req_port; 
 *	} until error;
 */
 
#import <sys/types.h>
#import <mach.h>
#import <servers/bootstrap.h>
#import <libc.h>
#import <mach_error.h>

void log_boot_servers(port_t boot_port);

int main(int argc, char **argv)
{
	kern_return_t krtn;
	port_t subset_port;
	port_t requestor_port;
	port_t foo_port;
	int loop = 0;
	int deallocate_subset = 0;
	
	if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'r') {
		port_t	newboot;

		krtn = bootstrap_look_up(bootstrap_port, &argv[1][2], &newboot);
		if (krtn) {
			mach_error("bootstrap lookup", krtn);
			exit(1);
		}
		bootstrap_port = newboot;
		--argc; ++argv;
	}
	if(argc >= 2) {
		if(argv[1][0] == '-' && argv[1][0] == 'd')
			deallocate_subset = 1;
	}
	
	/*
	 * Allocate some resources.
	 */
	krtn = port_allocate(task_self(), &foo_port);
	if(krtn) {
		mach_error("port_allocate", krtn);
		exit(1);
	}
	
	do {
		krtn = port_allocate(task_self(), &requestor_port);
		if(krtn) {
			mach_error("port_allocate", krtn);
			exit(1);
		}
		krtn = bootstrap_subset(bootstrap_port,
			requestor_port,			/* requestor */
			&subset_port);
		if(krtn) {
			mach_error("bootstrap_subset", krtn);
			break;
		}
		printf("Loop %d, prior to bootstrap_register:\n", loop);
		log_boot_servers(subset_port);	
		
		krtn = bootstrap_register(subset_port,
			"foo",
			foo_port);
		if(krtn) {
			mach_error("bootstrap_register (subset)", krtn);
			exit(1);
		}
		printf("Loop %d, after bootstrap_register:\n", loop);
		log_boot_servers(subset_port);	
		
		/*
		 * Delete requestor_port, subset should go away.
		 */
		krtn = port_deallocate(task_self(), requestor_port);
		if(krtn) {
			mach_error("port_deallocate", krtn);
			exit(1);
		}
		
		if(deallocate_subset) {
			krtn = port_deallocate(task_self(), subset_port);
			if(krtn) {
				mach_error("port_deallocate(subset)", krtn);
				exit(1);
			}
		}
		loop++;
	} while(krtn == KERN_SUCCESS);

	printf("...done\n");
	exit(0);
}

void log_boot_servers(port_t boot_port)
{
	int i;
	name_array_t service_names;
	unsigned int service_cnt;
	name_array_t server_names;
	unsigned int server_cnt;
	bool_array_t service_active;
	unsigned int service_active_cnt;
	kern_return_t krtn;
	
	krtn = bootstrap_info(boot_port, 
		&service_names, 
		&service_cnt,
		&server_names, 
		&server_cnt, 
		&service_active, 
		&service_active_cnt);
	if (krtn != BOOTSTRAP_SUCCESS)
		printf("ERROR:  info failed: %d", krtn);
	else {
		printf("log_boot_server: service_cnt = %d\n", service_cnt);
		for (i = 0; i < service_cnt; i++)
		   	printf("Name: %-15s   Server: %-15s    "
			    "Active: %-4s",
			service_names[i],
			server_names[i][0] == '\0' ? 
			    "Unknown" : server_names[i],
			service_active[i] ? "Yes\n" : "No\n");
	}
}

   