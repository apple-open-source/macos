/*
 * Copyright(c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1(the
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

#include <SystemConfiguration/SCD.h>
#include <SystemConfiguration/SCP.h>
#include "SCPPrivate.h"
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCPPath.h>

static CFArrayRef
normalizePath(CFStringRef path)
{
	CFArrayRef		tmpElements;
	CFMutableArrayRef	elements;
	CFIndex			nElements;
	CFIndex			i;

	if (!CFStringHasPrefix(path, CFSTR("/"))) {
		/* if no root separator */
		return NULL;
	}

	tmpElements = CFStringCreateArrayBySeparatingStrings(NULL, path, CFSTR("/"));
	elements    = CFArrayCreateMutableCopy(NULL, 0, tmpElements);
	CFRelease(tmpElements);

	/* remove empty path components */
	nElements = CFArrayGetCount(elements);
	for (i=nElements; i>0; i--) {
		CFStringRef	pathElement;

		pathElement = CFArrayGetValueAtIndex(elements, i-1);
		if (CFStringGetLength(pathElement) == 0) {
			CFArrayRemoveValueAtIndex(elements, i-1);
			nElements--;
		}
	}

	if (nElements < 1) {
		CFRelease(elements);
		return NULL;
	}

	return elements;
}


static SCPStatus
getPath(SCPSessionRef session, CFStringRef path, CFMutableDictionaryRef *entity)
{
	CFArrayRef		elements;
	CFIndex			i;
	CFIndex			nElements;
	SCPStatus		status;
	CFMutableDictionaryRef	value		= NULL;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	elements = normalizePath(path);
	if (elements == NULL) {
		return SCP_NOKEY;
	}

	/* get preferences key */
	status = SCPGet(session,
			CFArrayGetValueAtIndex(elements, 0),
			(CFPropertyListRef *)&value);
	if (status != SCP_OK) {
		goto done;
	}

	if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
		status = SCP_NOKEY;
		goto done;
	}

	nElements = CFArrayGetCount(elements);
	for (i=1; i<nElements; i++) {
		CFStringRef	element;

		element = CFArrayGetValueAtIndex(elements, i);
		value   = (CFMutableDictionaryRef)CFDictionaryGetValue(value, element);
		if (value == NULL) {
			/* if (parent) path component does not exist */
			status = SCP_NOKEY;
			goto done;
		}

		if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
			status = SCP_NOKEY;
			goto done;
		}

	}

	*entity = value;
	status = SCP_OK;

    done :

	CFRelease(elements);
	return status;
}


SCPStatus
SCPPathCreateUniqueChild(SCPSessionRef	session,
			 CFStringRef	prefix,
			 CFStringRef	*newPath)
{
	SCPStatus		status;
	CFMutableDictionaryRef	value;
	boolean_t		newValue	= FALSE;
	CFIndex			i;
	CFStringRef		path;
	CFMutableDictionaryRef	newDict;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	status = getPath(session, prefix, &value);
	switch (status) {
		case SCP_OK :
			break;
		case SCP_NOKEY :
			value = CFDictionaryCreateMutable(NULL,
							  0,
							  &kCFTypeDictionaryKeyCallBacks,
							  &kCFTypeDictionaryValueCallBacks);
			newValue = TRUE;
			break;
		default :
			return status;
	}

	if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
		/* if specified path is not a dictionary */
		status = SCP_NOKEY;
		goto done;
	}

	if (CFDictionaryContainsKey(value, kSCResvLink)) {
		/* the path is a link... */
		status = SCP_FAILED;
		goto done;
	}

	i = 0;
	while (TRUE) {
		CFStringRef	pathComponent;
		Boolean		found;

		pathComponent = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), i);
		found = CFDictionaryContainsKey(value, pathComponent);
		CFRelease(pathComponent);

		if (!found) {
			/* if we've identified the next unique key */
			path = CFStringCreateWithFormat(NULL,
							NULL,
							CFSTR("%@/%i"),
							prefix,
							i);
			break;
		}
		i++;
	}

	/* save the new dictionary */
	newDict = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	status = SCPPathSetValue(session, path, newDict);
	CFRelease(newDict);
	if (status != SCP_OK) {
		CFRelease(path);
		goto done;
	}

	*newPath = path;

    done :

	if (newValue)	CFRelease(value);
	return status;
}


SCPStatus
SCPPathGetValue(SCPSessionRef	session,
		CFStringRef	path,
		CFDictionaryRef	*value)
{
	SCPStatus		status;
	CFMutableDictionaryRef	entity;
	CFStringRef		entityLink;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	status = getPath(session, path, &entity);
	if (status != SCP_OK) {
		return status;
	}

/* XXXX Add code here to chase multiple links XXXXX */

	if ((CFGetTypeID(entity) == CFDictionaryGetTypeID()) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (void **)&entityLink))) {
		    /* if this is a dictionary AND it is a link */
		    status = getPath(session, entityLink, &entity);
		    if (status != SCP_OK) {
			    /* if it was a bad link */
			    return status;
		    }
	}

	*value = entity;
	return status;
}


SCPStatus
SCPPathGetLink(SCPSessionRef		session,
	       CFStringRef		path,
	       CFStringRef		*link)
{
	SCPStatus		status;
	CFMutableDictionaryRef	entity;
	CFStringRef		entityLink;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	status = getPath(session, path, &entity);
	if (status != SCP_OK) {
		return status;
	}

	if ((CFGetTypeID(entity) == CFDictionaryGetTypeID()) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (void **)&entityLink))) {
		    /* if this is a dictionary AND it is a link */
		*link = entityLink;
		return status;
	}

	return SCP_NOKEY;
}


SCPStatus
SCPPathSetValue(SCPSessionRef session, CFStringRef path, CFDictionaryRef value)
{
	CFMutableDictionaryRef	element;
	CFArrayRef		elements	= NULL;
	CFIndex			i;
	CFIndex			nElements;
	boolean_t		newRoot		= FALSE;
	CFMutableDictionaryRef	root		= NULL;
	SCPStatus		status		= SCP_NOKEY;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	elements = normalizePath(path);
	if (elements == NULL) {
		return SCP_NOKEY;
	}

	/* get preferences key */
	status = SCPGet(session,
			CFArrayGetValueAtIndex(elements, 0),
			(CFPropertyListRef *)&root);
	if (status != SCP_OK) {
		root = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
		newRoot = TRUE;
	}

	nElements = CFArrayGetCount(elements);
	if (nElements == 1) {
		/* if we are only updating the data associated with the preference key */
		if (newRoot) {
			CFRelease(root);
			newRoot = FALSE;
		}
		root = (CFMutableDictionaryRef)value;
	}

	element = root;
	for (i=1; i<nElements-1; i++) {
		CFStringRef		pathComponent;
		CFMutableDictionaryRef	tmpElement;

		pathComponent = CFArrayGetValueAtIndex(elements, i);
		tmpElement  = (void *)CFDictionaryGetValue(element, pathComponent);
		if (tmpElement == NULL) {
			/* if (parent) path component does not exist */
			tmpElement = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(element, pathComponent, tmpElement);
			CFRelease(tmpElement);
		}
		element = tmpElement;
	}

	if (nElements > 1) {
		CFDictionarySetValue(element,
				     CFArrayGetValueAtIndex(elements, nElements-1),
				     value);
	}
	status = SCPSet(session, CFArrayGetValueAtIndex(elements, 0), root);

	if (newRoot)	CFRelease(root);
	CFRelease(elements);
	return status;
}


SCPStatus
SCPPathSetLink(SCPSessionRef session, CFStringRef path, CFStringRef link)
{
	CFMutableDictionaryRef	dict;
	SCPStatus		status;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(dict, kSCResvLink, link);
	status = SCPPathSetValue(session, path, dict);
	CFRelease(dict);

	return status;
}


SCPStatus
SCPPathRemove(SCPSessionRef session, CFStringRef path)
{
	CFMutableDictionaryRef	element;
	CFArrayRef		elements	= NULL;
	CFIndex			i;
	CFIndex			nElements;
	CFMutableDictionaryRef	root		= NULL;
	SCPStatus		status		= SCP_NOKEY;

	if (session == NULL) {
		return SCP_NOSESSION;	/* you can't do anything with a closed session */
	}

	elements = normalizePath(path);
	if (elements == NULL) {
		return SCP_NOKEY;
	}

	/* get preferences key */
	status = SCPGet(session,
			CFArrayGetValueAtIndex(elements, 0),
			(CFPropertyListRef *)&root);
	if (status != SCP_OK) {
		goto done;
	}

	nElements = CFArrayGetCount(elements);
	if (nElements == 1) {
		/* if we are removing the data associated with the preference key */
		status = SCPRemove(session, CFArrayGetValueAtIndex(elements, 0));
		goto done;
	}

	element = root;
	for (i=1; i<nElements-1; i++) {
		CFStringRef		pathComponent;
		CFMutableDictionaryRef	tmpElement;

		pathComponent = CFArrayGetValueAtIndex(elements, i);
		tmpElement    = (void *)CFDictionaryGetValue(element, pathComponent);
		if (tmpElement == NULL) {
			status = SCP_NOKEY;
			goto done;
		}
		element = tmpElement;
	}

	CFDictionaryRemoveValue(element,
				CFArrayGetValueAtIndex(elements, nElements-1));
	status = SCPSet(session, CFArrayGetValueAtIndex(elements, 0), root);

    done :

	CFRelease(elements);
	return status;
}
