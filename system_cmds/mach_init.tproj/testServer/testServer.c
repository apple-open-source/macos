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
	port_t bootstrap_port, port, myport;
	port_type_t ptype;
	port_t *mach_ports;
	port_t *ports;
	unsigned port_cnt;
	unsigned mach_ports_cnt;
	name_t name_array[4];
	boolean_t all_known;
	port_t unpriv_port;
	port_t subset_port;
	port_t sub_reg_port;
	boolean_t active;
	name_array_t service_names;
	unsigned service_cnt, server_cnt, service_active_cnt;
	name_array_t server_names;
	boolean_t *service_actives;
	int i;
	
	print("test server running");
	result = task_get_bootstrap_port(task_self(), &bootstrap_port);
	if (result != KERN_SUCCESS) {
		error("Couldn't get bootstrap port: %d", result);
		exit(1);
	} else
		print("Bootstrap port is %d", bootstrap_port);
	if (bootstrap_port == PORT_NULL) {
		error("Invalid bootstrap port");
		exit(1);
	}
	
	/*
	 * Try a checkin
	 */
	print("Checkin test 1");
	result = bootstrap_check_in(bootstrap_port, "FreeService1", &port);
	if (result != BOOTSTRAP_SUCCESS)
		error("Checkin failed: %d", result);
	else {
		result = port_type(task_self(), port, &ptype);
		if (result != KERN_SUCCESS)
			error("port type failed: %d", result);
		else
			print("Checkin returned port type 0x%x", ptype);
		/*
		 * Try a status request
		 */
		result = bootstrap_status(bootstrap_port, "FreeService1", &active);
		if (result != BOOTSTRAP_SUCCESS)
			error("Status failed: %d", result);
		else if (active != TRUE)
			error("Service shown inactive");
	}
	
	/*
	 * Try a lookup
	 */
	print("lookup test");
	result = bootstrap_look_up(bootstrap_port, "FreeService2", &port);
	if (result != BOOTSTRAP_SUCCESS)
		error("lookup failed: %d", result);
	else {
		result = port_type(task_self(), port, &ptype);
		if (result != KERN_SUCCESS)
			error("port type failed: %d", result);
		else
			print("Lookup returned port type 0x%x", ptype);
		/*
		 * Try a status request
		 */
		result = bootstrap_status(bootstrap_port, "FreeService2", &active);
		if (result != BOOTSTRAP_SUCCESS)
			error("Status failed: %d", result);
		else if (active != FALSE)
			error("Service shown active");
	}
	
	/*
	 * Test that mach ports are initialized
	 */
	print("mach ports test");
	result = mach_ports_lookup(task_self(), &mach_ports, &mach_ports_cnt);
	if (result != KERN_SUCCESS)
		error("mach_ports_lookup failed: %d", result);
	else {
		result = bootstrap_look_up(bootstrap_port, "NetMsgService", &port);
		if (result != BOOTSTRAP_SUCCESS)
			error("Lookup of NetMsgService failed: %d", result);
		else if (port != mach_ports[0])
			error("mach ports not setup correctly for NetMsgService");
		
		result = bootstrap_look_up(bootstrap_port, "EnvironService", &port);
		if (result != BOOTSTRAP_SUCCESS)
			error("Lookup of EnvironService failed: %d", result);
		else if (port != mach_ports[1])
			error("mach ports not setup correctly for EnvironService");

		result = bootstrap_look_up(bootstrap_port, "Service", &port);
		if (result != BOOTSTRAP_SUCCESS)
			error("Lookup of Service failed: %d", result);
		else if (port != mach_ports[2])
			error("mach ports not setup correctly for Service");
			
		result = bootstrap_look_up(bootstrap_port, "WindowService", &port);
		if (result != BOOTSTRAP_SUCCESS)
			error("Lookup of WindowService failed: %d", result);
		else if (port != mach_ports[3])
			error("mach ports not setup correctly for WindowService");
	}
	
	/*
	 * Try doing a checkin with the old service interface
	 */
	result = service_checkin(mach_ports[2], mach_ports[1], &myport);
	if (result != KERN_SUCCESS)
		error("service checkin failed: %d", result);
	else {
		result = port_type(task_self(), myport, &ptype);
		if (result != KERN_SUCCESS)
			error("port type failed: %d", result);
		else
			print("Checkin returned port type 0x%x", ptype);
	}
	
	/*
	 * Try a register
	 */
	print("register test");
	print("...Dynamic creation");
	result = port_allocate(task_self(), &myport);
	if (result != KERN_SUCCESS)
		error("couldn't allocate port: %d", result);
	else {
		result = bootstrap_register(bootstrap_port, "NewService", myport);
		if (result != BOOTSTRAP_SUCCESS)
			error("Couldn't register port: %d", result);
		else {
	
			/*
			 * Try a lookup on just registered port
			 */
			result = bootstrap_look_up(bootstrap_port, "NewService", &port);
			if (result != BOOTSTRAP_SUCCESS)
				error("lookup failed: %d", result);
			else {
				result = port_type(task_self(), port, &ptype);
				if (result != KERN_SUCCESS)
					error("port type failed: %d", result);
				else {
					print("Lookup returned port type 0x%x", ptype);
					if (port != myport)
						error("lookup didn't match register");
				}
			}
			
			/*
			 * Try re-registering service name
			 */
			result = bootstrap_register(bootstrap_port, "NewService", myport);
			if (result != BOOTSTRAP_SERVICE_ACTIVE)
				error("Unexpected register response: %d", result);

			/*
			 * Delete the port.  This should cause the service to go away
			 * in the server.
			 */
			port_deallocate(task_self(), myport);
			
			result = bootstrap_look_up(bootstrap_port, "NewService", &port);
			if (result != BOOTSTRAP_UNKNOWN_SERVICE)
				error("service active after port deleted");
		}
	}

	print("...Declared service");
	result = port_allocate(task_self(), &myport);
	if (result != KERN_SUCCESS)
		error("couldn't allocate port: %d", result);
	else {
		result = bootstrap_register(bootstrap_port, "FreeService2", myport);
		if (result != BOOTSTRAP_SUCCESS)
			error("Couldn't register port: %d", result);
		else {
	
			/*
			 * Try a lookup on just registered port
			 */
			result = bootstrap_look_up(bootstrap_port, "FreeService2", &port);
			if (result != BOOTSTRAP_SUCCESS)
				error("lookup failed: %d", result);
			else {
				result = port_type(task_self(), port, &ptype);
				if (result != KERN_SUCCESS)
					error("port type failed: %d", result);
				else {
					print("Lookup returned port type 0x%x", ptype);
					if (port != myport)
						error("lookup didn't match register");
				}
			}
			
			/*
			 * Delete the port.  This should cause service to revert.
			 */
			port_deallocate(task_self(), myport);
			
			result = bootstrap_status(bootstrap_port, "FreeService2", &active);
			if (result != BOOTSTRAP_SUCCESS)
				error("Status failed: %d", result);
			else if (active != FALSE)
				error("Service shown active");
		}
	}

	/*
	 * Try a checkin on a port bound to Terminal server
	 */
	print("Bound checkin test -- Terminal");
	result = bootstrap_check_in(bootstrap_port, "TerminalService", &port);
	if (result != BOOTSTRAP_SUCCESS)
		error("Checkin of TerminalService failed: %d", result);
	else {
		result = port_type(task_self(), port, &ptype);
		if (result != KERN_SUCCESS)
			error("port type failed: %d", result);
		print("Checkin returned port type 0x%x", ptype);
	}

	/*
	 * Try a checkin on a port bound to Sleep server
	 */
	print("Bound checkin test -- Sleep");
	result = bootstrap_check_in(bootstrap_port, "SleepService", &port);
	if (result != BOOTSTRAP_SUCCESS)
		print("Checkin of SleepService failed (as expected): %d",
			result);
	else {
		result = port_type(task_self(), port, &ptype);
		if (result != KERN_SUCCESS)
			error("port type failed: %d", result);
		error("Checkin returned port type 0x%x(didn't fail!)", ptype);
	}

	/*
	 * Try a lookup_array
	 */
	print("Lookup array test");
	
	strncpy(&name_array[0], "NetMsgService", sizeof(name_array[0]));
	LAST_ELEMENT(name_array[0]) = '\0';
	strncpy(&name_array[1], "EnvironService", sizeof(name_array[1]));
	LAST_ELEMENT(name_array[1]) = '\0';
	strncpy(&name_array[2], "Service", sizeof(name_array[2]));
	LAST_ELEMENT(name_array[2]) = '\0';
	strncpy(&name_array[3], "WindowService", sizeof(name_array[3]));
	LAST_ELEMENT(name_array[3]) = '\0';
	
	result = bootstrap_look_up_array(bootstrap_port, name_array, 4, &ports,
	 &port_cnt, &all_known);
	if (result != BOOTSTRAP_SUCCESS)
		error("Lookup array failed: %d", result);
	else {
		print("Port count = %d, all known = %d", port_cnt, all_known);
		for (i = 0; i < 4; i++)
			if (ports[i] != mach_ports[i])
				error("port mismatch on port %d", i);
	}
	
	/*
	 * Get an unprivileged port
	 */
	print("Unprivileged port test");
	result = port_allocate(task_self(), &myport);
	result = bootstrap_get_unpriv_port(bootstrap_port, &unpriv_port);
	if (result != BOOTSTRAP_SUCCESS)
		error("Couldn't get unpriv port: %d", result);
	else {
		/*
		 * Try doing an unpriv operation
		 */
		result = bootstrap_look_up(unpriv_port, "FreeService2", &port);
		if (result != BOOTSTRAP_SUCCESS)
			error("lookup failed: %d", result);
		/*
		 * Try doing a privileged operation
		 */
		result = bootstrap_register(unpriv_port, "ANewService", myport);
		if (result != BOOTSTRAP_NOT_PRIVILEGED)
			error("Unexpected register port response: %d", result);

		/*
		 * Try creating a subset port.
		 */
		result = bootstrap_subset(unpriv_port, task_self(),
			&subset_port);
		if (result != BOOTSTRAP_SUCCESS)
			error("Couldn't get subset port from unpriv %d",
				result);
	}

	/*
	 * Get a subset port.
	 */
	print("Subset port test");
	result = bootstrap_subset(bootstrap_port, task_self(), &subset_port);
	if (result != BOOTSTRAP_SUCCESS)
		error("Couldn't get subset port: %d", result);
	else {

		/*
		 * Register a port.
		 */
		result = port_allocate(task_self(), &sub_reg_port);
		if (result != KERN_SUCCESS)
			error("port_allocate of sub_reg_port failed %d",
				result);
		result = bootstrap_register(subset_port, "SubsetReg",
					    sub_reg_port);
		if (result != BOOTSTRAP_SUCCESS)
			error("register of SubsetReg failed on subset port %d",
				result);
		/*
		 * Check that port registered only in subset.
		 */
		result = bootstrap_status(bootstrap_port, "SubsetReg",
					&active);
		if (result != BOOTSTRAP_UNKNOWN_SERVICE)
			error("status of SubsetReg  ok on bootstrap! %d",
				result);
		result = bootstrap_status(subset_port, "SubsetReg",
					&active);
		if (result != BOOTSTRAP_SUCCESS)
			error("status of SubsetReg failed on subset port %d",
				result);
		if (!active)
			error("SubsetReg isn't active");


		/*
		 * Try an info request.
		 */
		print("Subset info request");
		result = bootstrap_info(subset_port, &service_names,
			&service_cnt,
			&server_names, &server_cnt, &service_actives,
			&service_active_cnt);
		if (result != BOOTSTRAP_SUCCESS)
			error("info failed: %d", result);
		else {
			for (i = 0; i < service_cnt; i++)
				print("Name: %s	Server: %s	Active: %s",
				    service_names[i],
				    server_names[i][0] == '\0'
				     ? "Unknown"
				     : server_names[i],
				    service_actives[i] ? "Yes" : "No");
		}
	}

	/*
	 * Try an info request
	 */
	print("Info test");
	result = bootstrap_info(bootstrap_port, &service_names, &service_cnt,
	  &server_names, &server_cnt, &service_actives, &service_active_cnt);
	if (result != BOOTSTRAP_SUCCESS)
		error("info failed: %d", result);
	else {
		for (i = 0; i < service_cnt; i++)
			print("Name: %s	Server: %s	Active: %s", service_names[i],
			 server_names[i][0] == '\0' ? "Unknown" : server_names[i],
			 service_actives[i] ? "Yes" : "No");
	}
		
	exit(0);
}



