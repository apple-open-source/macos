/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <mach/mach.h>
#include <mach/notify.h>
#include <mach/mach_error.h>
#include <pthread.h>

#define	N_QUICK	32


char *
_SC_cfstring_to_cstring(CFStringRef cfstr, char *buf, int bufLen, CFStringEncoding encoding)
{
	CFIndex	last;
	CFIndex	len	= CFStringGetLength(cfstr);

	/* how much buffer space will we really need? */
	(void)CFStringGetBytes(cfstr,
			       CFRangeMake(0, len),
			       encoding,
			       0,
			       FALSE,
			       NULL,
			       0,
			       &len);

	if (!buf) {
		bufLen = len + 1;
		buf = CFAllocatorAllocate(NULL, bufLen, 0);
		if (!buf) {
			return NULL;
		}
	}

	if (len >= bufLen) {
		len = bufLen - 1;
	}

	(void)CFStringGetBytes(cfstr,
			       CFRangeMake(0, len),
			       encoding,
			       0,
			       FALSE,
			       buf,
			       bufLen,
			       &last);
	buf[last] = '\0';

	return buf;
}


Boolean
_SCSerialize(CFPropertyListRef obj, CFDataRef *xml, void **dataRef, CFIndex *dataLen)
{
	CFDataRef	myXml;

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
_SCUnserialize(CFPropertyListRef *obj, CFDataRef xml, void *dataRef, CFIndex dataLen)
{
	CFStringRef	xmlError;

	if (!xml) {
		kern_return_t	status;

		xml = CFDataCreateWithBytesNoCopy(NULL, (void *)dataRef, dataLen, kCFAllocatorNull);
		*obj = CFPropertyListCreateFromXMLData(NULL,
						       xml,
						       kCFPropertyListImmutable,
						       &xmlError);
		CFRelease(xml);

		status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
	} else {
		*obj = CFPropertyListCreateFromXMLData(NULL,
						       xml,
						       kCFPropertyListImmutable,
						       &xmlError);
	}

	if (*obj == NULL) {
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


Boolean
_SCSerializeString(CFStringRef str, CFDataRef *data, void **dataRef, CFIndex *dataLen)
{
	CFDataRef	myData;

	if (!isA_CFString(str)) {
		/* if not a CFString */
		return FALSE;
	}

	if (!data && !(dataRef && dataLen)) {
		/* if not keeping track of allocated space */
		return FALSE;
	}

	myData = CFStringCreateExternalRepresentation(NULL, str, kCFStringEncodingUTF8, 0);
	if (!myData) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFStringCreateExternalRepresentation() failed"));
		if (data)	*data    = NULL;
		if (dataRef)	*dataRef = NULL;
		if (dataLen)	*dataLen = 0;
		return FALSE;
	}

	if (data) {
		*data = myData;
		if (dataRef) {
			*dataRef = (void *)CFDataGetBytePtr(myData);
		}
		if (dataLen) {
			*dataLen = CFDataGetLength(myData);
		}
	} else {
		kern_return_t	status;

		*dataLen = CFDataGetLength(myData);
		status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
		if (status != KERN_SUCCESS) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("vm_allocate(): %s"),
			      mach_error_string(status));
			CFRelease(myData);
			*dataRef = NULL;
			*dataLen = 0;
			return FALSE;
		}

		bcopy((char *)CFDataGetBytePtr(myData), *dataRef, *dataLen);
		CFRelease(myData);
	}

	return TRUE;
}


Boolean
_SCUnserializeString(CFStringRef *str, CFDataRef utf8, void *dataRef, CFIndex dataLen)
{
	if (!utf8) {
		kern_return_t	status;

		utf8 = CFDataCreateWithBytesNoCopy(NULL, dataRef, dataLen, kCFAllocatorNull);
		*str = CFStringCreateFromExternalRepresentation(NULL, utf8, kCFStringEncodingUTF8);
		CFRelease(utf8);

		status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
	} else {
		*str = CFStringCreateFromExternalRepresentation(NULL, utf8, kCFStringEncodingUTF8);
	}

	if (*str == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFStringCreateFromExternalRepresentation() failed"));
		return FALSE;
	}

	return TRUE;
}


Boolean
_SCSerializeData(CFDataRef data, void **dataRef, CFIndex *dataLen)
{
	kern_return_t	status;

	if (!isA_CFData(data)) {
		/* if not a CFData */
		return FALSE;
	}

	*dataLen = CFDataGetLength(data);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("vm_allocate(): %s"),
		      mach_error_string(status));
		*dataRef = NULL;
		*dataLen = 0;
		return FALSE;
	}

	bcopy((char *)CFDataGetBytePtr(data), *dataRef, *dataLen);

	return TRUE;
}


Boolean
_SCUnserializeData(CFDataRef *data, void *dataRef, CFIndex dataLen)
{
	kern_return_t		status;

	*data = CFDataCreate(NULL, dataRef, dataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

	return TRUE;
}


CFDictionaryRef
_SCSerializeMultiple(CFDictionaryRef dict)
{
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			nElements;
	CFDictionaryRef		newDict		= NULL;
	const void *		pLists_q[N_QUICK];
	const void **		pLists		= pLists_q;
	const void *		values_q[N_QUICK];
	const void **		values		= values_q;

	nElements = CFDictionaryGetCount(dict);
	if (nElements > 0) {
		CFIndex	i;

		if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			pLists = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		}
		bzero(pLists, nElements * sizeof(CFTypeRef));

		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i = 0; i < nElements; i++) {
			if (!_SCSerialize((CFPropertyListRef)values[i], (CFDataRef *)&pLists[i], NULL, NULL)) {
				goto done;
			}
		}
	}

	newDict = CFDictionaryCreate(NULL,
				     keys,
				     pLists,
				     nElements,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    done :

	if (nElements > 0) {
		CFIndex	i;

		for (i = 0; i < nElements; i++) {
			if (pLists[i])	CFRelease(pLists[i]);
		}

		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
			CFAllocatorDeallocate(NULL, pLists);
		}
	}

	return newDict;
}


CFDictionaryRef
_SCUnserializeMultiple(CFDictionaryRef dict)
{
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			nElements;
	CFDictionaryRef		newDict		= NULL;
	const void *		pLists_q[N_QUICK];
	const void **		pLists		= pLists_q;
	const void *		values_q[N_QUICK];
	const void **		values		= values_q;

	nElements = CFDictionaryGetCount(dict);
	if (nElements > 0) {
		CFIndex	i;

		if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			pLists = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		}
		bzero(pLists, nElements * sizeof(CFTypeRef));

		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i = 0; i < nElements; i++) {
			if (!_SCUnserialize((CFPropertyListRef *)&pLists[i], values[i], NULL, NULL)) {
				goto done;
			}
		}
	}

	newDict = CFDictionaryCreate(NULL,
				     keys,
				     pLists,
				     nElements,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    done :

	if (nElements > 0) {
		CFIndex	i;

		for (i = 0; i < nElements; i++) {
			if (pLists[i])	CFRelease(pLists[i]);
		}

		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
			CFAllocatorDeallocate(NULL, pLists);
		}
	}

	return newDict;
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
			for (pi = 0; pi < pn; pi++) {
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
