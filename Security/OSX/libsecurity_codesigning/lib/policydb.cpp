/*
 * Copyright (c) 2011-2013 Apple Inc. All Rights Reserved.
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
#include <security_utilities/blob.h>
#include <security_utilities/logging.h>
#include <security_utilities/simpleprefs.h>
#include <security_utilities/logging.h>
#include "csdatabase.h"

#include <dispatch/dispatch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <notify.h>

namespace Security {
namespace CodeSigning {


using namespace SQLite;


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

CFStringRef typeNameFor(AuthorityType type)
{
	for (const StringMap *mp = mapType; mp->cstring; ++mp)
		if (type == mp->enumeration)
			return mp->cstring;
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("type %d"), type);
}


//
// Open the database
//
PolicyDatabase::PolicyDatabase(const char *path, int flags)
	: SQLite::Database(path ? path : dbPath(), flags),
	  mLastExplicitCheck(0)
{
	// sqlite3 doesn't do foreign key support by default, have to turn this on per connection
	SQLite::Statement foreign(*this, "PRAGMA foreign_keys = true");
	foreign.execute();
	
	// Try upgrade processing if we may be open for write.
	// Ignore any errors (we may have been downgraded to read-only)
	// and try again later.
	if (openFlags() & SQLITE_OPEN_READWRITE)
		try {
			upgradeDatabase();
			installExplicitSet(gkeAuthFile, gkeSigsFile);
		} catch(...) {
		}
}

PolicyDatabase::~PolicyDatabase()
{ /* virtual */ }


//
// Quick-check the cache for a match.
// Return true on a cache hit, false on failure to confirm a hit for any reason.
//
bool PolicyDatabase::checkCache(CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFMutableDictionaryRef result)
{
	// we currently don't use the cache for anything but execution rules
	if (type != kAuthorityExecute)
		return false;
	
	CFRef<SecStaticCodeRef> code;
	MacOSError::check(SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref()));
	if (SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, NULL) != errSecSuccess)
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

		if (allow && !overrideAssessment(flags))
		    MacOSError::check(SecStaticCodeCheckValidity(code, kSecCSDefaultFlags, NULL));

		cfadd(result, "{%O=%B}", kSecAssessmentAssessmentVerdict, allow);
		PolicyEngine::addAuthority(flags, result, label, auth, kCFBooleanTrue);
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
// Database migration
//
std::string PolicyDatabase::featureLevel(const char *name)
{
	SQLite::Statement feature(*this, "SELECT value FROM feature WHERE name=:name");
	feature.bind(":name") = name;
	if (feature.nextRow()) {
		if (const char *value = feature[0])
			return value;
		else
			return "default";	// old engineering versions may have NULL values; tolerate this
	}
	return "";		// new feature (no level)
}

void PolicyDatabase::addFeature(const char *name, const char *value, const char *remarks)
{
	SQLite::Statement feature(*this, "INSERT OR REPLACE INTO feature (name,value,remarks) VALUES(:name, :value, :remarks)");
	feature.bind(":name") = name;
	feature.bind(":value") = value;
	feature.bind(":remarks") = remarks;
	feature.execute();
}

void PolicyDatabase::simpleFeature(const char *feature, void (^perform)())
{
	if (!hasFeature(feature)) {
		SQLite::Transaction update(*this);
		perform();
		addFeature(feature, "upgraded", "upgraded");
		update.commit();
	}
}

void PolicyDatabase::simpleFeature(const char *feature, const char *sql)
{
	simpleFeature(feature, ^{
		SQLite::Statement perform(*this, sql);
		perform.execute();
	});
}


void PolicyDatabase::upgradeDatabase()
{
	simpleFeature("bookmarkhints",
		"CREATE TABLE bookmarkhints ("
			"  id INTEGER PRIMARY KEY AUTOINCREMENT, "
			"  bookmark BLOB,"
			"  authority INTEGER NOT NULL"
			"     REFERENCES authority(id) ON DELETE CASCADE"
			")");

	simpleFeature("codesignedpackages", ^{
		SQLite::Statement update(*this,
			"UPDATE authority"
			" SET requirement = 'anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists and "
				"(certificate leaf[field.1.2.840.113635.100.6.1.14] or certificate leaf[field.1.2.840.113635.100.6.1.13])'"
			" WHERE type = 2 and label = 'Developer ID' and flags & :flag");
		update.bind(":flag") = kAuthorityFlagDefault;
		update.execute();
	});
	
	simpleFeature("filter_unsigned",
		"ALTER TABLE authority ADD COLUMN filter_unsigned TEXT NULL"
		);
	
	simpleFeature("strict_apple_installer", ^{
		SQLite::Statement update(*this,
			"UPDATE authority"
			" SET requirement = 'anchor apple generic and certificate 1[subject.CN] = \"Apple Software Update Certification Authority\"'"
			" WHERE flags & :flag AND label = 'Apple Installer'");
		update.bind(":flag") = kAuthorityFlagDefault;
		update.execute();
		SQLite::Statement add(*this,
			"INSERT INTO authority (type, label, flags, requirement)"
			" VALUES (2, 'Mac App Store', :flags, 'anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.10] exists')");
		add.bind(":flags") = kAuthorityFlagDefault;
		add.execute();
	});
	
	simpleFeature("document rules", ^{
		SQLite::Statement addApple(*this,
			"INSERT INTO authority (type, allow, flags, label, requirement) VALUES (3, 1, 2, 'Apple System', 'anchor apple')");
		addApple.execute();
		SQLite::Statement addDevID(*this,
			"INSERT INTO authority (type, allow, flags, label, requirement)	VALUES (3, 1, 2, 'Developer ID', 'anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists')");
		addDevID.execute();
	});
    
    simpleFeature("root_only", ^{
        UnixError::check(::chmod(dbPath(), S_IRUSR | S_IWUSR));
    });
}


//
// Install Gatekeeper override (GKE) data.
// The arguments are paths to the authority and signature files.
//
void PolicyDatabase::installExplicitSet(const char *authfile, const char *sigfile)
{
	// only try this every gkeCheckInterval seconds
	time_t now = time(NULL);
	if (mLastExplicitCheck + gkeCheckInterval > now)
		return;
	mLastExplicitCheck = now;

	try {
		if (CFRef<CFDataRef> authData = cfLoadFile(authfile)) {
			CFDictionary auth(CFRef<CFDictionaryRef>(makeCFDictionaryFrom(authData)), errSecCSDbCorrupt);
			CFDictionaryRef content = auth.get<CFDictionaryRef>(CFSTR("authority"));
			std::string authUUID = cfString(auth.get<CFStringRef>(CFSTR("uuid")));
			if (authUUID.empty()) {
				secinfo("gkupgrade", "no uuid in auth file; ignoring gke.auth");
				return;
			}
			std::string dbUUID;
			SQLite::Statement uuidQuery(*this, "SELECT value FROM feature WHERE name='gke'");
			if (uuidQuery.nextRow())
				dbUUID = (const char *)uuidQuery[0];
			if (dbUUID == authUUID) {
				secinfo("gkupgrade", "gke.auth already present, ignoring");
				return;
			}
			Syslog::notice("loading GKE %s (replacing %s)", authUUID.c_str(), dbUUID.empty() ? "nothing" : dbUUID.c_str());

			// first, load code signatures. This is pretty much idempotent
			if (sigfile)
				if (FILE *sigs = fopen(sigfile, "r")) {
					unsigned count = 0;
				    SignatureDatabaseWriter db;
					while (const BlobCore *blob = BlobCore::readBlob(sigs)) {
						db.storeCode(blob, "<remote>");
						count++;
					}
					secinfo("gkupgrade", "%d detached signature(s) loaded from override data", count);
					fclose(sigs);
				}
			
			// start transaction (atomic from here on out)
			SQLite::Transaction loadAuth(*this, SQLite::Transaction::exclusive, "GKE_Upgrade");
			
			// purge prior authority data
			SQLite::Statement purge(*this, "DELETE FROM authority WHERE flags & :flag");
			purge.bind(":flag") = kAuthorityFlagWhitelist;
			purge();
			
			// load new data
			CFIndex count = CFDictionaryGetCount(content);
			CFStringRef keys[count];
			CFDictionaryRef values[count];
			CFDictionaryGetKeysAndValues(content, (const void **)keys, (const void **)values);
			
			SQLite::Statement insert(*this, "INSERT INTO authority (type, allow, requirement, label, filter_unsigned, flags, remarks)"
				" VALUES (:type, 1, :requirement, 'GKE', :filter, :flags, :path)");
			for (CFIndex n = 0; n < count; n++) {
				CFDictionary info(values[n], errSecCSDbCorrupt);
				uint32_t flags = kAuthorityFlagWhitelist;
				if (CFNumberRef versionRef = info.get<CFNumberRef>("version")) {
					int version = cfNumber<int>(versionRef);
					if (version >= 2) {
						flags |= kAuthorityFlagWhitelistV2;
						if (version >= 3) {
							flags |= kAuthorityFlagWhitelistSHA256;
						}
					}
				}
				insert.reset();
				insert.bind(":type") = cfString(info.get<CFStringRef>(CFSTR("type")));
				insert.bind(":path") = cfString(info.get<CFStringRef>(CFSTR("path")));
				insert.bind(":requirement") = "cdhash H\"" + cfString(info.get<CFStringRef>(CFSTR("cdhash"))) + "\"";
				insert.bind(":filter") = cfString(info.get<CFStringRef>(CFSTR("screen")));
				insert.bind(":flags").integer(flags);
				insert();
			}
			
			// we just changed the authority configuration at priority zero
			this->purgeObjects(0);
			
			// update version and commit
			addFeature("gke", authUUID.c_str(), "gke loaded");
			loadAuth.commit();
            /* now that we have moved to a bundle for gke files, delete any old style files we find
               This is really just a best effort cleanup, so we don't care about errors. */
            if (access(gkeAuthFile_old, F_OK) == 0)
            {
                if (unlink(gkeAuthFile_old) == 0)
                {
                    Syslog::notice("Deleted old style gke file (%s)", gkeAuthFile_old);
                }
            }
            if (access(gkeSigsFile_old, F_OK) == 0)
            {
                if (unlink(gkeSigsFile_old) == 0)
                {
                    Syslog::notice("Deleted old style gke file (%s)", gkeSigsFile_old);
                }
            }
		}
	} catch (...) {
		secinfo("gkupgrade", "exception during GKE upgrade");
	}
}


//
// Check the override-enable master flag
//
#define SP_ENABLE_KEY CFSTR("enabled")
#define SP_ENABLED CFSTR("yes")
#define SP_DISABLED CFSTR("no")

bool overrideAssessment(SecAssessmentFlags flags /* = 0 */)
{
	static bool enabled = true;
	static dispatch_once_t once;
	static int token = -1;
	static int have_token = 0;
	static dispatch_queue_t queue;
	int check;

	if (flags & kSecAssessmentFlagEnforce)	// explicitly disregard disables (force on)
		return false;

	if (have_token && notify_check(token, &check) == NOTIFY_STATUS_OK && !check)
		return !enabled;

	dispatch_once(&once, ^{
		if (notify_register_check(kNotifySecAssessmentMasterSwitch, &token) == NOTIFY_STATUS_OK)
			have_token = 1;
		queue = dispatch_queue_create("com.apple.SecAssessment.assessment", NULL);
             });

	dispatch_sync(queue, ^{
		/* upgrade configuration from emir, ignore all error since we might not be able to write to */
		if (::access(visibleSecurityFlagFile, F_OK) == 0) {
			try {
				setAssessment(true);
				::unlink(visibleSecurityFlagFile);
			} catch (...) {
			}
			enabled = true;
			return;
		}

		try {
			Dictionary * prefsDict = Dictionary::CreateDictionary(prefsFile);
			if (prefsDict == NULL)
				return;
			
			CFStringRef value = prefsDict->getStringValue(SP_ENABLE_KEY);
			if (value && CFStringCompare(value, SP_DISABLED, 0) == 0)
				enabled = false;
			else
				enabled = true;
			delete prefsDict;
		} catch(...) {
		}
	});

	return !enabled;
}

void setAssessment(bool masterSwitch)
{
	MutableDictionary *prefsDict = MutableDictionary::CreateMutableDictionary(prefsFile);
	if (prefsDict == NULL)
		prefsDict = new MutableDictionary::MutableDictionary();
	prefsDict->setValue(SP_ENABLE_KEY, masterSwitch ? SP_ENABLED : SP_DISABLED);
	prefsDict->writePlistToFile(prefsFile);
	delete prefsDict;

	/* make sure permissions is right */
	::chmod(prefsFile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	notify_post(kNotifySecAssessmentMasterSwitch);

	/* reset the automatic rearm timer */
	resetRearmTimer("masterswitch");
}


//
// Reset or query the automatic rearm timer
//
void resetRearmTimer(const char *event)
{
	CFRef<CFDateRef> now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFTemp<CFDictionaryRef> info("{event=%s, timestamp=%O}", event, now.get());
	CFRef<CFDataRef> infoData = makeCFData(info.get());
	UnixPlusPlus::AutoFileDesc fd(rearmTimerFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	fd.write(CFDataGetBytePtr(infoData), CFDataGetLength(infoData));
}

bool queryRearmTimer(CFTimeInterval &delta)
{
	if (CFRef<CFDataRef> infoData = cfLoadFile(rearmTimerFile)) {
		if (CFRef<CFDictionaryRef> info = makeCFDictionaryFrom(infoData)) {
			CFDateRef timestamp = (CFDateRef)CFDictionaryGetValue(info, CFSTR("timestamp"));
			if (timestamp && CFGetTypeID(timestamp) == CFDateGetTypeID()) {
				delta = CFAbsoluteTimeGetCurrent() - CFDateGetAbsoluteTime(timestamp);
				return true;
			}
		}
		MacOSError::throwMe(errSecCSDbCorrupt);
	}
	return false;
}


} // end namespace CodeSigning
} // end namespace Security
