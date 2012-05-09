/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
#include "cs.h"
#include "policydb.h"
#include "policyengine.h"
#include <Security/CodeSigning.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>

namespace Security {
namespace CodeSigning {


using namespace SQLite;


//
// The one and only PolicyDatabase object.
// It auto-adapts to readonly vs. writable use.
//
ModuleNexus<PolicyDatabase> PolicyDatabase;


//
// Determine the database path
//
static const char *dbPath()
{
	if (const char *s = getenv("SYSPOLICYDATABASE"))
		return s;
	return defaultDatabase;
}


//
// Help mapping API-ish CFString keys to more convenient internal enumerations
//
typedef struct {
	const CFStringRef &cstring;
	uint enumeration;
} StringMap;

static uint mapEnum(CFDictionaryRef context, CFStringRef attr, const StringMap *map, uint value = 0)
{
	if (context)
		if (CFTypeRef value = CFDictionaryGetValue(context, attr))
			for (const StringMap *mp = map; mp->cstring; ++mp)
				if (CFEqual(mp->cstring, value))
					return mp->enumeration;
	return value;
}

static const StringMap mapType[] = {
	{ kSecAssessmentOperationTypeExecute, kAuthorityExecute },
	{ kSecAssessmentOperationTypeInstall, kAuthorityInstall },
	{ kSecAssessmentOperationTypeOpenDocument, kAuthorityOpenDoc },
	{ NULL }
};

AuthorityType typeFor(CFDictionaryRef context, AuthorityType type /* = kAuthorityInvalid */)
{
	return mapEnum(context, kSecAssessmentContextKeyOperation, mapType, type);
}


//
// Open the database (creating it if necessary and possible).
// Note that this isn't creating the schema; we do that on first write.
//
PolicyDatabase::PolicyDatabase(const char *path, int flags)
	: SQLite::Database(path ? path : dbPath(), flags)
{
}

PolicyDatabase::~PolicyDatabase()
{ /* virtual */ }


//
// Quick-check the cache for a match.
// Return true on a cache hit, false on failure to confirm a hit for any reason.
//
bool PolicyDatabase::checkCache(CFURLRef path, AuthorityType type, CFMutableDictionaryRef result)
{
	// we currently don't use the cache for anything but execution rules
	if (type != kAuthorityExecute)
		return false;
	
	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
	if (SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, NULL) != noErr)
		return false;	// quick pass - any error is a cache miss
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
	CFDataRef cdHash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
	
	// check the cache table for a fast match
	SQLite::Statement cached(*this, "SELECT object.allow, authority.label, authority FROM object, authority"
		" WHERE object.authority = authority.id AND object.type = :type AND object.hash = :hash AND authority.disabled = 0"
		" AND JULIANDAY('now') < object.expires;");
	cached.bind(":type").integer(type);
	cached.bind(":hash") = cdHash;
	if (cached.nextRow()) {
		bool allow = int(cached[0]);
		const char *label = cached[1];
		SQLite::int64 auth = cached[2];
		SYSPOLICY_ASSESS_CACHE_HIT();

		// If its allowed, lets do a full validation unless if
		// we are overriding the assessement, since that force
		// the verdict to 'pass' at the end

		if (allow && !overrideAssessment())
		    MacOSError::check(SecStaticCodeCheckValidity(code, kSecCSDefaultFlags, NULL));

		cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
		PolicyEngine::addAuthority(result, label, auth, kCFBooleanTrue);
		return true;
	}
	return false;
}


//
// Purge the object cache of all expired entries.
// These are meant to run within the caller's transaction.
//
void PolicyDatabase::purgeAuthority()
{
	SQLite::Statement cleaner(*this,
		"DELETE FROM authority WHERE expires <= JULIANDAY('now');");
	cleaner.execute();
}

void PolicyDatabase::purgeObjects()
{
	SQLite::Statement cleaner(*this,
		"DELETE FROM object WHERE expires <= JULIANDAY('now');");
	cleaner.execute();
}

void PolicyDatabase::purgeObjects(double priority)
{
	SQLite::Statement cleaner(*this,
		"DELETE FROM object WHERE expires <= JULIANDAY('now') OR (SELECT priority FROM authority WHERE id = object.authority) <= :priority;");
	cleaner.bind(":priority") = priority;
	cleaner.execute();
}


//
// Check the override-enable master flag
//
bool overrideAssessment()
{
	if (::access(visibleSecurityFlagFile, F_OK) == 0) {
		return false;
	} else if (errno == ENOENT) {
		return true;
	} else
		UnixError::throwMe();
}


} // end namespace CodeSigning
} // end namespace Security
