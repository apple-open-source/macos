/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include <mach/mach.h>
#include <mach/notify.h>
#include <mach/mach_error.h>
#include <pthread.h>

Boolean
_SCSerialize(CFPropertyListRef		obj,
	     CFDataRef			*xml,
	     void			**dataRef,
	     CFIndex			*dataLen)
{
	CFDataRef	myXml;

	if (!obj) {
		/* if no object to serialize */
		return FALSE;
	}

	if (!xml && !(dataRef && dataLen)) {
		/* if not keeping track of allocated space */
		return FALSE;
	}

	myXml = CFPropertyListCreateXMLData(NULL, obj);
	if (!myXml) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFPropertyListCreateXMLData() failed"));
		if (xml)	*xml     = NULL;
		if (dataRef)	*dataRef = NULL;
		if (dataLen)	*dataLen = 0;
		return FALSE;
	}

	if (xml) {
		*xml = myXml;
		if (dataRef) {
			*dataRef = (void *)CFDataGetBytePtr(myXml);
		}
		if (dataLen) {
			*dataLen = CFDataGetLength(myXml);
		}
	} else {
		kern_return_t	status;

		*dataLen = CFDataGetLength(myXml);
		status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
		if (status != KERN_SUCCESS) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("vm_allocate(): %s"),
			      mach_error_string(status));
			CFRelease(myXml);
			*dataRef = NULL;
			*dataLen = 0;
			return FALSE;
		}

		bcopy((char *)CFDataGetBytePtr(myXml), *dataRef, *dataLen);
		CFRelease(myXml);
	}

	return TRUE;
}


Boolean
_SCUnserialize(CFPropertyListRef	*obj,
	       void			*dataRef,
	       CFIndex			dataLen)
{
	kern_return_t		status;
	CFDataRef		xml;
	CFStringRef		xmlError;

	if (!obj) {
		return FALSE;
	}

	xml = CFDataCreate(NULL, (void *)dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	*obj = CFPropertyListCreateFromXMLData(NULL,
					       xml,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xml);

	if (!obj) {
		if (xmlError) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("CFPropertyListCreateFromXMLData() failed: %@"),
			      xmlError);
			CFRelease(xmlError);
		}
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


void
__showMachPortStatus()
{
#ifdef	DEBUG
	/* print status of in-use mach ports */
	if (_sc_debug) {
		kern_return_t		status;
		mach_port_name_array_t	ports;
		mach_port_type_array_t	types;
		int			pi, pn, tn;
		CFMutableStringRef	str;

		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("----------"));

		/* report on ALL mach ports associated with this task */
		status = mach_port_names(mach_task_self(), &ports, &pn, &types, &tn);
		if (status == MACH_MSG_SUCCESS) {
			str = CFStringCreateMutable(NULL, 0);
			for (pi=0; pi < pn; pi++) {
				char	rights[16], *rp = &rights[0];

				if (types[pi] != MACH_PORT_TYPE_NONE) {
					*rp++ = ' ';
					*rp++ = '(';
					if (types[pi] & MACH_PORT_TYPE_SEND)
						*rp++ = 'S';
					if (types[pi] & MACH_PORT_TYPE_RECEIVE)
						*rp++ = 'R';
					if (types[pi] & MACH_PORT_TYPE_SEND_ONCE)
						*rp++ = 'O';
					if (types[pi] & MACH_PORT_TYPE_PORT_SET)
						*rp++ = 'P';
					if (types[pi] & MACH_PORT_TYPE_DEAD_NAME)
						*rp++ = 'D';
					*rp++ = ')';
				}
				*rp = '\0';
				CFStringAppendFormat(str, NULL, CFSTR(" %d%s"), ports[pi], rights);
			}
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("Task ports (n=%d):%@"), pn, str);
			CFRelease(str);
		} else {
			/* log (but ignore) errors */
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("mach_port_names(): %s"), mach_error_string(status));
		}
	}
#endif	/* DEBUG */
	return;
}


void
__showMachPortReferences(mach_port_t port)
{
#ifdef	DEBUG
	kern_return_t		status;
	mach_port_urefs_t	refs_send	= 0;
	mach_port_urefs_t	refs_recv	= 0;
	mach_port_urefs_t	refs_once	= 0;
	mach_port_urefs_t	refs_pset	= 0;
	mach_port_urefs_t	refs_dead	= 0;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("user references for mach port %d"), port);

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND,      &refs_send);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_SEND): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE,   &refs_recv);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_RECEIVE): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND_ONCE, &refs_once);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_SEND_ONCE): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_PORT_SET,  &refs_pset);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_PORT_SET): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, &refs_dead);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_DEAD_NAME): %s"), mach_error_string(status));
		return;
	}

	SCLog(_sc_verbose, LOG_DEBUG,
	       CFSTR("  send = %d, receive = %d, send once = %d, port set = %d, dead name = %d"),
	       refs_send,
	       refs_recv,
	       refs_once,
	       refs_pset,
	       refs_dead);

#endif	/* DEBUG */
	return;
}
