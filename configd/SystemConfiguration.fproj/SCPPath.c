/*
 * Copyright(c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCPreferencesInternal.h"

#define	MAXLINKS	8

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


static Boolean
getPath(SCPreferencesRef session, CFStringRef path, CFDictionaryRef *entity)
{
	CFStringRef		element;
	CFArrayRef		elements;
	CFIndex			i;
	CFStringRef		link;
	CFIndex			nElements;
	CFIndex			nLinks		= 0;
	Boolean			ok		= FALSE;
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;
	CFDictionaryRef		value		= NULL;

	elements = normalizePath(path);
	if (elements == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

    restart :

	nElements = CFArrayGetCount(elements);
	for (i=0; i<nElements; i++) {
		element = CFArrayGetValueAtIndex(elements, i);
		if (i == 0) {
			sessionPrivate->accessed = TRUE;
			value = CFDictionaryGetValue(sessionPrivate->prefs,
						     CFArrayGetValueAtIndex(elements, 0));
		} else {
			value = CFDictionaryGetValue(value, element);
		}
		if (value == NULL) {
			/* if path component does not exist */
			_SCErrorSet(kSCStatusNoKey);
			goto done;
		}

		if (!isA_CFDictionary(value)) {
			/* if path component not a dictionary */
			_SCErrorSet(kSCStatusNoKey);
			goto done;
		}

		if ((i < nElements-1) &&
		    CFDictionaryGetValueIfPresent(value, kSCResvLink, (const void **)&link)) {
			/*
			 * if not the last path component and this
			 * element is a link
			 */
			CFArrayRef		linkElements;
			CFMutableArrayRef	newElements;

			if (++nLinks > MAXLINKS) {
				/* if we are chasing our tail */
				_SCErrorSet(kSCStatusMaxLink);
				goto done;
			}

			linkElements = normalizePath(link);
			if (linkElements == NULL) {
				/* if the link is bad */
				_SCErrorSet(kSCStatusNoKey);
				goto done;
			}

			newElements = CFArrayCreateMutableCopy(NULL, 0, linkElements);
			CFArrayAppendArray(newElements,
					   elements,
					   CFRangeMake(i+1, nElements-i-1));
			CFRelease(elements);
			elements = newElements;

			goto restart;
		}
	}

	*entity = value;
	ok = TRUE;

    done :

	CFRelease(elements);
	return ok;
}


static Boolean
setPath(SCPreferencesRef session, CFStringRef path, CFDictionaryRef entity)
{
	CFStringRef		element;
	CFArrayRef		elements;
	CFIndex			i;
	CFStringRef		link;
	CFIndex			nElements;
	CFIndex			nLinks		= 0;
	CFDictionaryRef		newEntity	= NULL;
	CFDictionaryRef		node		= NULL;
	CFMutableArrayRef	nodes;
	Boolean			ok		= FALSE;
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;

	elements = normalizePath(path);
	if (elements == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

    restart :

	nElements = CFArrayGetCount(elements);
	nodes     = CFArrayCreateMutable(NULL, nElements-1, &kCFTypeArrayCallBacks);
	for (i=0; i<nElements-1; i++) {
		element = CFArrayGetValueAtIndex(elements, i);
		if (i == 0) {
			sessionPrivate->accessed = TRUE;
			node = CFDictionaryGetValue(sessionPrivate->prefs, element);
		} else {
			node = CFDictionaryGetValue(node, element);

		}

		if (node) {
			/* if path component exists */
			CFArrayAppendValue(nodes, node);
		} else {
			/* if path component does not exist */
			node = CFDictionaryCreate(NULL,
						  NULL,
						  NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
			CFArrayAppendValue(nodes, node);
			CFRelease(node);
		}

		if (!isA_CFDictionary(node)) {
			_SCErrorSet(kSCStatusNoKey);
			goto done;
		}

		if ((i < nElements-1) &&
		    CFDictionaryGetValueIfPresent(node, kSCResvLink, (const void **)&link)) {
			/*
			 * if not the last path component and this
			 * element is a link
			 */
			CFArrayRef		linkElements;
			CFMutableArrayRef	newElements;

			if (++nLinks > MAXLINKS) {
				/* if we are chasing our tail */
				_SCErrorSet(kSCStatusMaxLink);
				goto done;
			}

			linkElements = normalizePath(link);
			if (linkElements == NULL) {
				/* if the link is bad */
				_SCErrorSet(kSCStatusNoKey);
				goto done;
			}

			newElements = CFArrayCreateMutableCopy(NULL, 0, linkElements);
			CFArrayAppendArray(newElements,
					   elements,
					   CFRangeMake(i+1, nElements-i-1));
			CFRelease(elements);
			elements = newElements;

			CFRelease(nodes);
			goto restart;
		}
	}

	if (entity) {
		newEntity = CFRetain(entity);
	}
	for (i=nElements-1; i>=0; i--) {
		element = CFArrayGetValueAtIndex(elements, i);
		if (i == 0) {
			if (newEntity) {
				CFDictionarySetValue(sessionPrivate->prefs, element, newEntity);
			} else {
				CFDictionaryRemoveValue(sessionPrivate->prefs, element);
			}
			sessionPrivate->changed  = TRUE;
			ok = TRUE;
		} else {
			CFMutableDictionaryRef	newNode;

			node    = CFArrayGetValueAtIndex(nodes, i-1);
			newNode = CFDictionaryCreateMutableCopy(NULL, 0, node);
			if (newEntity) {
				CFDictionarySetValue(newNode, element, newEntity);
				CFRelease(newEntity);
			} else {
				CFDictionaryRemoveValue(newNode, element);
			}
			newEntity = newNode;
		}
	}
	if (newEntity) {
		CFRelease(newEntity);
	}

    done :

	CFRelease(nodes);
	CFRelease(elements);
	return ok;
}


CFStringRef
SCPreferencesPathCreateUniqueChild(SCPreferencesRef	session,
				   CFStringRef		prefix)
{
	CFStringRef             child;
	CFStringRef		newPath		= NULL;
	CFMutableDictionaryRef	newDict		= NULL;
	CFUUIDRef               uuid;
	CFDictionaryRef		entity;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathCreateUniqueChild:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  prefix = %@"), prefix);

	if (getPath(session, prefix, &entity)) {
		// if prefix path exists
		if (CFDictionaryContainsKey(entity, kSCResvLink)) {
			/* the path is a link... */
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	} else if (SCError() != kSCStatusNoKey) {
		// if any error except for a missing prefix path component
		return NULL;
	}

	uuid    = CFUUIDCreate(NULL);
	child   = CFUUIDCreateString(NULL, uuid);
	newPath = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%@"), prefix, child);
	CFRelease(child);
	CFRelease(uuid);

	/* save the new dictionary */
	newDict = CFDictionaryCreateMutable(NULL,
					    0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	if (setPath(session, newPath, newDict)) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  child  = %@"), newPath);
	} else {
		CFRelease(newPath);
		newPath = NULL;
	}
	CFRelease(newDict);

	return newPath;
}


CFDictionaryRef
SCPreferencesPathGetValue(SCPreferencesRef	session,
			  CFStringRef		path)
{
	CFDictionaryRef	entity;
	CFStringRef	entityLink;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathGetValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path  = %@"), path);

	if (!getPath(session, path, &entity)) {
		return NULL;
	}

	if (isA_CFDictionary(entity) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (const void **)&entityLink))) {
		/* if this is a dictionary AND it is a link */
		if (!getPath(session, entityLink, &entity)) {
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
	CFDictionaryRef	entity;
	CFStringRef	entityLink;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathGetLink:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);

	if (!getPath(session, path, &entity)) {
		return NULL;
	}

	if (isA_CFDictionary(entity) &&
	    (CFDictionaryGetValueIfPresent(entity, kSCResvLink, (const void **)&entityLink))) {
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
	Boolean			ok;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathSetValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path  = %@"), path);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  value = %@"), value);

	if (!value) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	ok = setPath(session, path, value);
	return ok;
}


Boolean
SCPreferencesPathSetLink(SCPreferencesRef	session,
			 CFStringRef		path,
			 CFStringRef		link)
{
	CFMutableDictionaryRef	dict;
	CFDictionaryRef		entity;
	Boolean			ok;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathSetLink:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  link = %@"), link);

	if (!link) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (!getPath(session, link, &entity)) {
		// if bad link
		return FALSE;
	}

	dict = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(dict, kSCResvLink, link);
	ok = setPath(session, path, dict);
	CFRelease(dict);

	return ok;
}


Boolean
SCPreferencesPathRemoveValue(SCPreferencesRef	session,
			     CFStringRef	path)
{
	CFArrayRef		elements	= NULL;
	Boolean			ok		= FALSE;
	CFDictionaryRef		value;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesPathRemoveValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  path = %@"), path);

	if (!getPath(session, path, &value)) {
		// if no such path
		return FALSE;
	}

	elements = normalizePath(path);
	if (elements == NULL) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

	ok = setPath(session, path, NULL);

	CFRelease(elements);
	return ok;
}
