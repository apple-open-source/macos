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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 16, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

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


static int
getPath(SCPreferencesRef session, CFStringRef path, CFMutableDictionaryRef *entity)
{
	CFArrayRef		elements;
	CFIndex			i;
	CFIndex			nElements;
	int			status		= kSCStatusFailed;
	CFMutableDictionaryRef	value		= NULL;

	elements = normalizePath(path);
	if (elements == NULL) {
		return kSCStatusNoKey;
	}

	/* get preferences key */
	value = (CFMutableDictionaryRef)SCPreferencesGetValue(session,
							      CFArrayGetValueAtIndex(elements, 0));
	if (!value) {
		status = kSCStatusNoKey;
		goto done;
	}

	if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
		status = kSCStatusNoKey;
		goto done;
	}

	nElements = CFArrayGetCount(elements);
	for (i=1; i<nElements; i++) {
		CFStringRef	element;

		element = CFArrayGetValueAtIndex(elements, i);
		value   = (CFMutableDictionaryRef)CFDictionaryGetValue(value, element);
		if (value == NULL) {
			/* if (parent) path component does not exist */
			status = kSCStatusNoKey;
			goto done;
		}

		if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
			status = kSCStatusNoKey;
			goto done;
		}

	}

	*entity = value;
	status = kSCStatusOK;

    done :

	CFRelease(elements);
	return status;
}


CFStringRef
SCPreferencesPathCreateUniqueChild(SCPreferencesRef	session,
				   CFStringRef		prefix)
{
	int			status;
	CFMutableDictionaryRef	value;
	CFStringRef		newPath		= NULL;
	Boolean			newValue	= FALSE;
	CFIndex			i;
	CFMutableDictionaryRef	newDict		= NULL;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathCreateUniqueChild:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  prefix = %@"), prefix);

	status = getPath(session, prefix, &value);
	switch (status) {
		case kSCStatusOK :
			break;
		case kSCStatusNoKey :
			value = CFDictionaryCreateMutable(NULL,
							  0,
							  &kCFTypeDictionaryKeyCallBacks,
							  &kCFTypeDictionaryValueCallBacks);
			newValue = TRUE;
			break;
		default :
			return NULL;
	}

	if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
		/* if specified path is not a dictionary */
		status = kSCStatusNoKey;
		goto error;
	}

	if (CFDictionaryContainsKey(value, kSCResvLink)) {
		/* the path is a link... */
		status = kSCStatusFailed;
		goto error;
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
			newPath = CFStringCreateWithFormat(NULL,
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
	if (!SCPreferencesPathSetValue(session, newPath, newDict)) {
		goto error;
	}
	CFRelease(newDict);

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  child  = %@"), newPath);
	if (newValue)	CFRelease(value);
	return newPath;

    error :

	if (newDict)	CFRelease(newDict);
	if (newValue)	CFRelease(value);
	if (newPath)	CFRelease(newPath);
	return NULL;
}


CFDictionaryRef
SCPreferencesPathGetValue(SCPreferencesRef	session,
			  CFStringRef		path)
{
	int			status;
	CFMutableDictionaryRef	entity;
	CFStringRef		entityLink;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathGetValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path  = %@"), path);

	status = getPath(session, path, &entity);
	if (status != kSCStatusOK) {
		return NULL;
	}

/* XXXX Add code here to chase multiple links XXXXX */

	if ((CFGetTypeID(entity) == CFDictionaryGetTypeID()) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (void **)&entityLink))) {
		    /* if this is a dictionary AND it is a link */
		    status = getPath(session, entityLink, &entity);
		    if (status != kSCStatusOK) {
			    /* if it was a bad link */
			    return NULL;
		    }
	}

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  value = %@"), entity);
	return entity;
}


CFStringRef
SCPreferencesPathGetLink(SCPreferencesRef	session,
			 CFStringRef		path)
{
	int			status;
	CFMutableDictionaryRef	entity;
	CFStringRef		entityLink;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathGetLink:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);

	status = getPath(session, path, &entity);
	if (status != kSCStatusOK) {
		return NULL;
	}

	if ((CFGetTypeID(entity) == CFDictionaryGetTypeID()) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (void **)&entityLink))) {
		    /* if this is a dictionary AND it is a link */
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  link = %@"), entityLink);
		return entityLink;
	}

	return NULL;
}


Boolean
SCPreferencesPathSetValue(SCPreferencesRef	session,
			  CFStringRef		path,
			  CFDictionaryRef	value)
{
	CFMutableDictionaryRef	element;
	CFArrayRef		elements	= NULL;
	CFIndex			i;
	CFIndex			nElements;
	Boolean			newRoot		= FALSE;
	Boolean			ok;
	CFMutableDictionaryRef	root		= NULL;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathSetValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path  = %@"), path);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  value = %@"), value);

	elements = normalizePath(path);
	if (elements == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

	/* get preferences key */
	root = (CFMutableDictionaryRef)SCPreferencesGetValue(session,
							     CFArrayGetValueAtIndex(elements, 0));
	if (!root) {
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
	ok = SCPreferencesSetValue(session, CFArrayGetValueAtIndex(elements, 0), root);
	if (newRoot)	CFRelease(root);
	CFRelease(elements);
	return ok;
}


Boolean
SCPreferencesPathSetLink(SCPreferencesRef	session,
			 CFStringRef		path,
			 CFStringRef		link)
{
	CFMutableDictionaryRef	dict;
	Boolean			ok;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathSetLink:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  link = %@"), link);

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(dict, kSCResvLink, link);
	ok = SCPreferencesPathSetValue(session, path, dict);
	CFRelease(dict);

	return ok;
}


Boolean
SCPreferencesPathRemoveValue(SCPreferencesRef	session,
			     CFStringRef	path)
{
	CFMutableDictionaryRef	element;
	CFArrayRef		elements	= NULL;
	CFIndex			i;
	CFIndex			nElements;
	Boolean			ok		= FALSE;
	CFMutableDictionaryRef	root		= NULL;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathRemoveValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);

	elements = normalizePath(path);
	if (elements == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

	/* get preferences key */
	root = (CFMutableDictionaryRef)SCPreferencesGetValue(session,
							     CFArrayGetValueAtIndex(elements, 0));
	if (!root) {
		goto done;
	}

	nElements = CFArrayGetCount(elements);
	if (nElements == 1) {
		/* if we are removing the data associated with the preference key */
		ok = SCPreferencesRemoveValue(session, CFArrayGetValueAtIndex(elements, 0));
		goto done;
	}

	element = root;
	for (i=1; i<nElements-1; i++) {
		CFStringRef		pathComponent;
		CFMutableDictionaryRef	tmpElement;

		pathComponent = CFArrayGetValueAtIndex(elements, i);
		tmpElement    = (void *)CFDictionaryGetValue(element, pathComponent);
		if (tmpElement == NULL) {
			goto done;
		}
		element = tmpElement;
	}

	CFDictionaryRemoveValue(element,
				CFArrayGetValueAtIndex(elements, nElements-1));
	ok = SCPreferencesSetValue(session, CFArrayGetValueAtIndex(elements, 0), root);

    done :

	CFRelease(elements);
	return ok;
}
