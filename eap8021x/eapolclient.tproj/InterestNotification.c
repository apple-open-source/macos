/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
 * InterestNotification.c
 * - register for IOKit interest notification on a particular BSD name
 */

/* 
 * Modification History
 *
 * March 10, 2009	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "InterestNotification.h"

struct InterestNotification {
    IONotificationPortRef 		notify;
    io_object_t				if_change;
    InterestNotificationCallbackRef	callback;
    const void *			arg;
};

static void
InterestNotificationHandleMessage(void * refcon, io_service_t service, 
				  natural_t message_type, 
				  void * messageArgument)
{
    InterestNotificationRef	interest_p = (InterestNotificationRef)refcon;

    if (message_type == kIOMessageServicePropertyChange) {
	InterestNotificationCallbackRef	callback = interest_p->callback;
	const void *			arg = interest_p->arg;

	(*callback)(interest_p, arg);
    }
    return;
}

static void
InterestNotificationInit(InterestNotificationRef interest_p)
{
    bzero(interest_p, sizeof(*interest_p));
    return;
}

void
InterestNotificationRelease(InterestNotificationRef interest_p)
{
    if (interest_p->if_change != MACH_PORT_NULL) {
	IOObjectRelease(interest_p->if_change);
    }
    if (interest_p->notify != MACH_PORT_NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      IONotificationPortGetRunLoopSource(interest_p->notify),
			      kCFRunLoopDefaultMode);
	IONotificationPortDestroy(interest_p->notify);
    }
    InterestNotificationInit(interest_p);
    free(interest_p);
    return;
}

static boolean_t
InterestNotificationStart(InterestNotificationRef interest_p,
			  const char * if_name,
			  InterestNotificationCallbackRef callback,
			  const void * arg)
{
    io_object_t			if_change = MACH_PORT_NULL;
    kern_return_t		kr;
    io_iterator_t		list = MACH_PORT_NULL;
    CFDictionaryRef		matching;
    IONotificationPortRef 	notify = NULL;
    io_object_t			obj = MACH_PORT_NULL;
    boolean_t			ok = FALSE;

    matching = IOBSDNameMatching(kIOMasterPortDefault, 0, if_name);
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
				      matching, &list);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr, "InterestNotificationStart: No such interface %s\n",
		if_name);
	goto done;
    }
    notify = IONotificationPortCreate(kIOMasterPortDefault);
    if (notify == NULL) {
	fprintf(stderr,	"InterestNotificationStart: IONotificationPortCreate"
		" failed\n");
	goto done;
    }
    obj = IOIteratorNext(list);
    if (obj == MACH_PORT_NULL) {
	fprintf(stderr,
		"InterestNotificationStart: IOIteratorNext no object\n");
	goto done;
    }
    kr = IOServiceAddInterestNotification(notify,
					  obj,
					  kIOGeneralInterest,
					  &InterestNotificationHandleMessage,
					  (void *)interest_p,
					  &if_change);
    if (kr != KERN_SUCCESS) {
	fprintf(stderr,
		"InterestNotificationStart: IOServiceAddInterestNotification"
		" failed, 0x%x\n", kr);
	goto done;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       IONotificationPortGetRunLoopSource(notify),
		       kCFRunLoopDefaultMode);
    interest_p->notify = notify;
    interest_p->if_change = if_change;
    interest_p->callback = callback;
    interest_p->arg = arg;
    ok = TRUE;

 done:
    if (list != MACH_PORT_NULL) {
	IOObjectRelease(list);
    }
    if (obj != MACH_PORT_NULL) {
	IOObjectRelease(obj);
    }
    if (ok == FALSE) {
	if (notify != NULL) {
	    IONotificationPortDestroy(notify);
	}
	if (if_change != MACH_PORT_NULL) {
	    IOObjectRelease(if_change);
	}
    }
    return (ok);
}

InterestNotificationRef
InterestNotificationCreate(const char * if_name, 
			   InterestNotificationCallbackRef callback,
			   const void * arg)
{
    InterestNotificationRef	interest_p;

    if (callback == NULL) {
	return (NULL);
    }
    interest_p = (InterestNotificationRef)malloc(sizeof(*interest_p));
    InterestNotificationInit(interest_p);
    if (InterestNotificationStart(interest_p, if_name, callback, arg) 
	== FALSE) {
	free(interest_p);
	return (NULL);
    }
    return (interest_p);
}

#ifdef TEST_INTERESTNOTIFICATION

static void
change_callback(InterestNotificationRef interest_p,
		const void * arg)
{
    printf("Change, all done\n");
    InterestNotificationRelease(interest_p);
    return;
}

int
main(int argc, char * argv[])
{
    InterestNotificationRef	interest_p;

    if (argc < 2) {
	fprintf(stderr, "usage: ioregwatch <ifname>\n");
	exit(1);
    }
    interest_p = InterestNotificationCreate(argv[1],
					    change_callback, NULL);
    if (interest_p == NULL) {
	fprintf(stderr, "Create failed\n");
	exit(2);
    }
    CFRunLoopRun();
    exit(0);
    return (0);
}

#endif /* TEST_INTERESTNOTIFICATION */
