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
 * bootstrap -- fundamental service initiator and port server
 * Mike DeMoney, NeXT, Inc.
 * Copyright, 1990.  All rights reserved.
 *
 * lists.c -- implementation of list handling routines
 */

#import <mach/boolean.h>
#import <mach/mach_error.h>

#import <stdlib.h>
#import <string.h>

#import "bootstrap_internal.h"
#import "lists.h"
#import "error_log.h"

/*
 * Exports
 */
bootstrap_info_t bootstraps;		/* head of list of all bootstrap ports */
server_t servers;		/* head of list of all servers */
service_t services;		/* head of list of all services */
unsigned nservices;		/* number of services in list */

#ifndef ASSERT
#define ASSERT(p)
#endif

/*
 * Private macros
 */
#define	NEW(type, num)	((type *)ckmalloc(sizeof(type) * num))
#define	STREQ(a, b)		(strcmp(a, b) == 0)
#define	NELEM(x)			(sizeof(x)/sizeof((x)[0]))
#define	LAST_ELEMENT(x)		((x)[NELEM(x)-1])

void
init_lists(void)
{
	bootstraps.next = bootstraps.prev = &bootstraps;
	servers.next = servers.prev = &servers;
	services.next = services.prev = &services;
	nservices = 0;
}

server_t *
new_server(
	servertype_t	servertype,
	const char	*cmd,
	int		priority)
{
	server_t *serverp;

	debug("adding new server \"%s\" with priority %d\n", cmd, priority);	
	serverp = NEW(server_t, 1);
	if (serverp != NULL) {
		/* Doubly linked list */
		servers.prev->next = serverp;
		serverp->prev = servers.prev;
		serverp->next = &servers;
		servers.prev = serverp;

		serverp->port = MACH_PORT_NULL;
		serverp->servertype = servertype;
		serverp->priority = priority;
		strncpy(serverp->cmd, cmd, sizeof serverp->cmd);
		LAST_ELEMENT(serverp->cmd) = '\0';
	}
	return serverp;
}
			
service_t *
new_service(
	bootstrap_info_t	*bootstrap,
	const char	*name,
	mach_port_t		service_port,
	boolean_t	isActive,
	servicetype_t	servicetype,
	server_t	*serverp)
{
	extern mach_port_t notify_port;
        service_t *servicep;
	mach_port_t pport;
        kern_return_t result;
        
	servicep = NEW(service_t, 1);
	if (servicep != NULL) {
		/* Doubly linked list */
		services.prev->next = servicep;
		servicep->prev = services.prev;
		servicep->next = &services;
		services.prev = servicep;
		
		nservices += 1;
		
		strncpy(servicep->name, name, sizeof servicep->name);
		LAST_ELEMENT(servicep->name) = '\0';
		servicep->bootstrap = bootstrap;
		servicep->server = serverp;
		servicep->port = service_port;
                result = mach_port_request_notification(mach_task_self(),
                                                        service_port,
                                                        MACH_NOTIFY_DEAD_NAME,
                                                        0,
                                                        notify_port,
                                                        MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                                        &pport);
                if (result == KERN_SUCCESS) {
                    debug("added notification for %s\n", servicep->name);
                } else {
                    error("couldn't add notification for %s: %s\n", servicep->name, mach_error_string(result));
                }
                servicep->isActive = isActive;
		servicep->servicetype = servicetype;
	}
	return servicep;
}

bootstrap_info_t *
new_bootstrap(
	bootstrap_info_t	*parent,
	mach_port_t	bootstrap_port,
	mach_port_t	requestor_port)
{
	extern mach_port_t notify_port;
	bootstrap_info_t *bootstrap;
	mach_port_t pport;
	kern_return_t result;

	bootstrap = NEW(bootstrap_info_t, 1);
	if (bootstrap != NULL) {
		/* Doubly linked list */
		bootstraps.prev->next = bootstrap;
		bootstrap->prev = bootstraps.prev;
		bootstrap->next = &bootstraps;
		bootstraps.prev = bootstrap;
		
		bootstrap->bootstrap_port = bootstrap_port;
		bootstrap->requestor_port = requestor_port;
		bootstrap->parent = parent;
                result = mach_port_request_notification(mach_task_self(),
                                                        requestor_port,
                                                        MACH_NOTIFY_DEAD_NAME,
                                                        0,
                                                        notify_port,
                                                        MACH_MSG_TYPE_MAKE_SEND_ONCE, &pport); 
                if (result == KERN_SUCCESS) {
                    info("added notification for sub-bootstrap");
                } else {
                    error("couldn't add notification for sub-bootstrap: %s\n", mach_error_string(result));
                }

	}
	return bootstrap;
}

bootstrap_info_t *
lookup_bootstrap_by_port(mach_port_t port)
{
	bootstrap_info_t *bootstrap;

	for (  bootstrap = FIRST(bootstraps)
	     ; !IS_END(bootstrap, bootstraps)
	     ; bootstrap = NEXT(bootstrap))
	{
		if (bootstrap->bootstrap_port == port)
			return bootstrap;
	}

	return &bootstraps;
}

bootstrap_info_t *
lookup_bootstrap_req_by_port(mach_port_t port)
{
	bootstrap_info_t *bootstrap;

	for (  bootstrap = FIRST(bootstraps)
	     ; !IS_END(bootstrap, bootstraps)
	     ; bootstrap = NEXT(bootstrap))
	{
		if (bootstrap->requestor_port == port)
			return bootstrap;
	}

	return NULL;
}

service_t *
lookup_service_by_name(bootstrap_info_t *bootstrap, name_t name)
{
	service_t *servicep;

	while (bootstrap) {
		for (  servicep = FIRST(services)
		     ; !IS_END(servicep, services)
		     ; servicep = NEXT(servicep))
		{
			if (!STREQ(name, servicep->name))
				continue;
			if (bootstrap && servicep->bootstrap != bootstrap)
				continue;
			return servicep;
		}
		bootstrap = bootstrap->parent;
	}

	return NULL;
}

void
unlink_service(service_t *servicep)
{
	ASSERT(servicep->prev->next == servicep);
	ASSERT(servicep->next->prev == servicep);
	servicep->prev->next = servicep->next;
	servicep->next->prev = servicep->prev;
	servicep->prev = servicep->next = servicep;	// idempotent
}

void
delete_service(service_t *servicep)
{
	unlink_service(servicep);
	free(servicep);
	nservices -= 1;
}

void
destroy_services(bootstrap_info_t *bootstrap)
{
	service_t *servicep;
	service_t *next;
	
	for (  servicep = FIRST(services)
	     ; !IS_END(servicep, services)
	     ; servicep = next)
	{
		next = NEXT(servicep);
	  	if (bootstrap != servicep->bootstrap)
			continue;
		unlink_service(servicep);
		switch (servicep->servicetype) {
		case REGISTERED:
			log("Service %s deleted - bootstrap deleted", servicep->name);
			msg_destroy_port(servicep->port);
			delete_service(servicep);
			break;
		case DECLARED:	// don't alter status of (now unavailable) server
			error("Declared service %s now unavailable", servicep->name);
			delete_service(servicep);
			break;
		case SELF:
			error("Self service %s now unavailable", servicep->name);
			break;
		default:
			error("unknown service type %d\n", servicep->servicetype);
			break;
		}
	}
}

service_t *
lookup_service_by_port(mach_port_t port)
{
	service_t *servicep;
	
        for (  servicep = FIRST(services)
	     ; !IS_END(servicep, services)
	     ; servicep = NEXT(servicep))
	{
	  	if (port == servicep->port)
			return servicep;
	}
        return NULL;
}

server_t *
lookup_server_by_task_port(mach_port_t port)
{
	server_t *serverp;
	
	for (  serverp = FIRST(servers)
	     ; !IS_END(serverp, servers)
	     ; serverp = NEXT(serverp))
	{
	  	if (port == serverp->task_port)
			return serverp;
	}
	return NULL;
}

void
delete_bootstrap(bootstrap_info_t *bootstrap)
{
	bootstrap_info_t *child_bootstrap;

	ASSERT(bootstrap->prev->next == bootstrap);
	ASSERT(bootstrap->next->prev == bootstrap);

	destroy_services(bootstrap);
	for (  child_bootstrap = FIRST(bootstraps)
	     ; !IS_END(child_bootstrap, bootstraps)
	     ; child_bootstrap = NEXT(child_bootstrap))
	{
		if (child_bootstrap->parent == bootstrap)
			delete_bootstrap(child_bootstrap);
	}

	debug("deleting bootstrap %d, requestor %d",
		bootstrap->bootstrap_port,
		bootstrap->requestor_port);
	bootstrap->prev->next = bootstrap->next;
	bootstrap->next->prev = bootstrap->prev;
	mach_port_destroy(mach_task_self(), bootstrap->bootstrap_port);
	mach_port_deallocate(mach_task_self(), bootstrap->requestor_port);
	free(bootstrap);
}

server_t *
lookup_server_by_port(mach_port_t port)
{
	server_t *serverp;
	
	for (  serverp = FIRST(servers)
	     ; !IS_END(serverp, servers)
	     ; serverp = NEXT(serverp))
	{
	  	if (port == serverp->port)
			return serverp;
	}
	return NULL;
}

server_t *
find_init_server(void)
{
	server_t *serverp;
	
	for (  serverp = FIRST(servers)
	     ; !IS_END(serverp, servers)
	     ; serverp = NEXT(serverp))
	{
	 	if (serverp->servertype == ETCINIT)
			return serverp;
	}
	return NULL;
}

void *
ckmalloc(unsigned nbytes)
{
	void *cp;
	
	if ((cp = malloc(nbytes)) == NULL)
		fatal("Out of memory");
	return cp;
}


