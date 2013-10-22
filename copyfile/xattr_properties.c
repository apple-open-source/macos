/*
 * Copyright (c) 2013 Apple, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <xattr_properties.h>

/*
 * Some default propeteries for EAs we know about internally.
 */
struct defaultList {
	const char *eaName;
	const char *propList;
	int flags;	// See below
};

#define propFlagsPrefix	0x0001	// The name is a prefix, so only look at that part

static const struct defaultList
defaultPropertyTable[] = {
	{ "com.apple.quarantine", "PC", 0 },	// not public
	{ "com.apple.TextEncoding", "PC", 0 },	// Content-dependent, public
	{ "com.apple.metadata:", "P", propFlagsPrefix },	// Don't export, keep for copy & safe save
	{ "com.apple.security.", "N", propFlagsPrefix },
	{ XATTR_RESOURCEFORK_NAME, "PC", 0 },	// Don't keep for safe save
	{ XATTR_FINDERINFO_NAME, "PC", 0 },	// Same as ResourceFork
	{ 0, 0, 0 },
};

/*
 * The property lists on an EA are set by having a suffix character,
 * and then a list of characters.  In general, we're choosing upper-case
 * to indicate the property is set, and lower-case to indicate it's to be
 * cleared.
 */
struct propertyListMapping {
	char enable;	// Character to enable
	char disable;	// Character to disable -- usually lower-case of enable
	CopyOperationProperties_t	value;
};
static const struct propertyListMapping
PropertyListMapTable[] = {
	{ 'C', 'c', kCopyOperationPropertyContentDependent },
	{ 'P', 'p', kCopyOperationPropertyNoExport },
	{ 'N', 'n', kCopyOperationPropertyNeverPreserve },
	{ 0, 0, 0 },
};
	
/*
 * Given a converted property list (that is, converted to the
 * CopyOperationProperties_t type), and an intent, determine if
 * it should be preserved or not.
 *
 * I've chosen to use a block instead of a simple mask on the belief
 * that the question may be moderately complex.  If it ends up not being
 * so, then this can simply be turned into a mask of which bits to check
 * as being exclusionary.
 */
static const struct divineIntent {
	CopyOperationIntent_t intent;
	int (^checker)(CopyOperationProperties_t);
} intentTable[] = {
	{ CopyOperationIntentCopy, ^(CopyOperationProperties_t props) {
			if (props & kCopyOperationPropertyNeverPreserve)
				return 0;
			return 1;
		} },
	{ CopyOperationIntentSave, ^(CopyOperationProperties_t props) {
			if (props & (kCopyOperationPropertyContentDependent | kCopyOperationPropertyNeverPreserve))
				return 0;
			return 1;
		} },
	{ CopyOperationIntentShare, ^(CopyOperationProperties_t props) {
			if ((props & (kCopyOperationPropertyNoExport | kCopyOperationPropertyNeverPreserve)) != 0)
				return 0;
			return 1;
		} },
	{ 0, 0 },
};


/*
 * If an EA name is in the default list, find it, and return the property
 * list string for it.
 */
static const char *
nameInDefaultList(const char *eaname)
{
	const struct defaultList *retval;

	for (retval = defaultPropertyTable; retval->eaName; retval++) {
		if ((retval->flags & propFlagsPrefix) != 0 &&
		    strncmp(retval->eaName, eaname, strlen(retval->eaName)) == 0)
			return retval->propList;
		if (strcmp(retval->eaName, eaname) == 0)
			return retval->propList;
	}
	return NULL;
}

/*
 * Given an EA name, see if it has a property list in it, and
 * return a pointer to it.  All this is doing is looking for
 * the delimiter, and returning the string after that.  Returns
 * NULL if the delimiter isn't found.  Note that an empty string
 * is a valid property list, as far as we're concerned.
 */
static const char *
findPropertyList(const char *eaname)
{
	const char *ptr = strrchr(eaname, '#');
	if (ptr)
		return ptr+1;
	return NULL;
}

/*
 * Convert a property list string (e.g., "pCd") into a
 * CopyOperationProperties_t type.
 */
static CopyOperationProperties_t
stringToProperties(const char *proplist)
{
	CopyOperationProperties_t retval = 0;
	const char *ptr;

	// A switch would be more efficient, but less generic.
	for (ptr = proplist; *ptr; ptr++) {
		const struct propertyListMapping *mapPtr;
		for (mapPtr = PropertyListMapTable; mapPtr->enable; mapPtr++) {
			if (*ptr == mapPtr->enable) {
				retval |= mapPtr->value;
			} else if (*ptr == mapPtr->disable) {
				retval &= ~mapPtr->value;
			}
		}
	}
	return retval;
}

/*
 * Given an EA name (e.g., "com.apple.lfs.hfs.test"), and a
 * CopyOperationProperties_t value (it's currently an integral value, so
 * just a bitmask), cycle through the list of known properties, and return
 * a string with the EA name, and the property list appended.  E.g., we
 * might return "com.apple.lfs.hfs.test#pD".
 *
 * The tricky part of this funciton is that it will not append any letters
 * if the value is only the default properites.  In that case, it will copy
 * the EA name, and return that.
 *
 * It returns NULL if there was an error.  The two errors right now are
 * no memory (strdup failed), in which case it will set errno to ENOMEM; and
 * the resulting EA name is longer than XATTR_MAXNAMELEN, in which case it
 * sets errno to ENAMETOOLONG.
 *
 * (Note that it also uses ENAMETOOLONG if the buffer it's trying to set
 * gets too large.  I honestly can't see how that would happen, but it's there
 * for sanity checking.  That would require having more than 64 bits to use.)
 */
char *
_xattrNameWithProperties(const char *orig, CopyOperationProperties_t propList)
{
	char *retval = NULL;
	char suffix[66] = { 0 }; // 66:  uint64_t for property types, plus '#', plus NUL
	char *cur = suffix;
	const struct propertyListMapping *mapPtr;

	*cur++ = '#';
	for (mapPtr = PropertyListMapTable; mapPtr->enable; mapPtr++) {
		if ((propList & mapPtr->value) != 0) {
			*cur++ = mapPtr->enable;
		}
		if (cur >= (suffix + sizeof(suffix))) {
			errno = ENAMETOOLONG;
			return NULL;
		}

	}
	
		
	if (cur == suffix + 1) {
		// No changes made
		retval = strdup(orig);
		if (retval == NULL)
			errno = ENOMEM;
	} else {
		const char *defaultEntry = NULL;
		if ((defaultEntry = nameInDefaultList(orig)) != NULL &&
		    strcmp(defaultEntry, suffix + 1) == 0) {
			// Just use the name passed in
			retval = strdup(orig);
		} else {
			asprintf(&retval, "%s%s", orig, suffix);
		}
		if (retval == NULL) {
			errno = ENOMEM;
		} else {
			if (strlen(retval) > XATTR_MAXNAMELEN) {
				free(retval);
				retval = NULL;
				errno = ENAMETOOLONG;
			}
		}
	}
	return retval;
}

CopyOperationProperties_t
_xattrPropertiesFromName(const char *eaname)
{
	CopyOperationProperties_t retval = 0;
	const char *propList;

	propList = findPropertyList(eaname);
	if (propList == NULL) {
		propList = findPropertyList(eaname);
	}
	if (propList != NULL) {
		retval = stringToProperties(propList);
	}
	
	return retval;
}

/*
 * Indicate whether an EA should be preserved, when using the
 * given intent.
 * 
 * This returns 0 if it should not be preserved, and 1 if it should.
 * 
 * It simply looks through the tables we have above, and compares the
 * CopyOperationProperties_t for the EA with the intent.  If the
 * EA doesn't have any properties, and it's not on the default list, the
 * default is to preserve it.
 */

int
_PreserveEA(const char *eaname, CopyOperationIntent_t intent)
{
	const struct divineIntent *ip;
	CopyOperationProperties_t props;
	const char *propList;

	if ((propList = findPropertyList(eaname)) == NULL &&
	    (propList = nameInDefaultList(eaname)) == NULL)
		props = 0;
	else
		props = stringToProperties(propList);

	for (ip = intentTable; ip->intent; ip++) {
		if (ip->intent == intent) {
			return ip->checker(props);
		}
	}
	
	if ((props & kCopyOperationPropertyNeverPreserve) != 0)
		return 0;	// Special case, don't try to preserve this one

	return 1;	// Default to preserving everything
}
