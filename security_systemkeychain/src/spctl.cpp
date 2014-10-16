/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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

//
// spctl - command-line access to system policy control (SecAssessment)
//
#include "spctl.h"
#include "cs_utils.h"
#include <security_utilities/unix++.h>
#include <getopt.h>

using namespace UnixPlusPlus;


//
// Operational mode
//
enum Operation {
	doNothing,						// none given (print usage)
	doAssess,						// assessment operation
	doStatus,						// master query status
	doMasterEnable,					// master honor assessment rejects
	doMasterDisable,				// master bypass assessment rejects
	doDevIDStatus,					// query devid status
	doDevIDEnable,					// devid honor assessment rejects
	doDevIDDisable,					// devid bypass assessment rejects
	doAdd,							// add authority rule
	doRemove,						// remove rule(s)
	doRuleEnable,					// (re)enable rule(s)
	doRuleDisable,					// disable rule(s)
	doList,							// list authority rules
	doPurge,						// purge object cache
};
Operation operation = doNothing;


//
// Specification type
//
enum Specification {
	specPath,						// path to file(s)
	specRequirement,				// code requirement(s)
	specAnchor,						// path to anchor certificate(s)
	specHash,						// CodeDirectory hash(es)
	specRule,						// (removal by) rule number
};
Specification specification = specPath;


//
// Command-line arguments and options
//
const char *assessmentType;
SecAssessmentFlags assessmentFlags;
SecAssessmentFlags outcomeFlags;
const char *featureCheck;
const char *label;
const char *priority;
const char *remarks;
bool rawOutput;
CFMutableDictionaryRef context = makeCFMutableDictionary();
// additional variables declared in cs_utils


//
// Feature set
//
static const char *features[] = {
	NULL							// sentinel
};


//
// Local functions
//
static void usage();
static void checkFeatures(const char *arg);

static void assess(const char *target);
static void addAuthority(const char *target);
static void removeAuthority(const char *target);
static void enableAuthority(const char *target);
static void disableAuthority(const char *target);
static void listAuthority(const char *target);
static void status(Operation op);

static CFTypeRef typeKey(const char *type);
static string hashArgument(const char *s);
static string fileHash(const char *path);


//
// Command-line options
//
enum {
	optNone = 0,	// null (no, absent) option
	optAdd,
	optAnchor,
	optContext,
	optContinue,
	optDirect,
	optEnforce,
	optRuleEnable,
	optRuleDisable,
	optMasterEnable,
	optMasterDisable,
	optDevIDStatus,
	optDevIDEnable,
	optDevIDDisable,
	optFeatures,
	optHash,
	optIgnoreCache,
	optLabel,
	optNoCache,
	optPath,
	optPriority,
	optPurge,
	optRawOutput,
	optRemarks,
	optRemove,
	optRequirement,
	optRule,
	optStatus,
};

const struct option options[] = {
	{ "add",		no_argument,			NULL, optAdd },
	{ "anchor",		no_argument,			NULL, optAnchor },
	{ "assess",		no_argument,			NULL, 'a' },
	{ "context",	required_argument,		NULL, optContext },
	{ "continue",	no_argument,			NULL, optContinue },
	{ "direct",		no_argument,			NULL, 'D' },
	{ "status",		optional_argument,		NULL, optStatus },
	{ "enable",		no_argument,			NULL, optRuleEnable },
	{ "enforce-assessment",	no_argument,	NULL, optEnforce },
	{ "disable",	no_argument,			NULL, optRuleDisable },
	{ "master-enable",	no_argument,		NULL, optMasterEnable },
	{ "master-disable", no_argument,		NULL, optMasterDisable },
	{ "test-devid-status",	no_argument,	NULL, optDevIDStatus },
	{ "test-devid-enable",	no_argument,	NULL, optDevIDEnable },
	{ "test-devid-disable", no_argument,	NULL, optDevIDDisable },
	{ "features",	optional_argument,		NULL, optFeatures },
	{ "hash",		no_argument,			NULL, optHash },
	{ "ignore-cache", no_argument,			NULL, optIgnoreCache },
	{ "label",		required_argument,		NULL, optLabel },
	{ "list",		no_argument,			NULL, 'l' },
	{ "no-cache",	no_argument,			NULL, optNoCache },
	{ "path",		no_argument,			NULL, optPath },
	{ "priority",	required_argument,		NULL, optPriority },
	{ "purge",		no_argument,			NULL, optPurge },
	{ "raw",		no_argument,			NULL, optRawOutput },
	{ "remarks",	required_argument,		NULL, optRemarks },
	{ "remove",		no_argument,			NULL, optRemove },
	{ "requirement", no_argument,			NULL, optRequirement },
	{ "rule",		no_argument,			NULL, optRule },
	{ "type",		required_argument,		NULL, 't' },
	{ "verbose",	optional_argument,		NULL, 'v' },
	{ }
};


//
// main command-line driver
//
int main(int argc, char *argv[])
{
	try {
		int arg, argslot;
		while (argslot = -1,
				(arg = getopt_long(argc, argv, "aDlt:v", options, &argslot)) != -1)
			switch (arg) {
			case 'a':
				operation = doAssess;
				break;
			case 'D':
				assessmentFlags |= kSecAssessmentFlagDirect;
				outcomeFlags |= kSecAssessmentFlagDirect;
				break;
			case 'l':
				operation = doList;
				break;
			case 't':
				assessmentType = optarg;
				break;
			case 'v':
				verbose++;
				break;
			
			case optAdd:
				operation = doAdd;
				break;
			case optAnchor:
				specification = specAnchor;
				break;
			case optContext:
				if (const char *eq = strchr(optarg, '=')) {	// key=value
					CFDictionaryAddValue(context, CFTempString(string(optarg, eq - optarg)), CFTempString(eq+1));
				} else {	// key, assume =true
					CFDictionaryAddValue(context, CFTempString(optarg), kCFBooleanTrue);
				}
				break;
			case optContinue:
				continueOnError = true;
				break;
			case optEnforce:
				assessmentFlags |= kSecAssessmentFlagEnforce;
				outcomeFlags |= kSecAssessmentFlagEnforce;
				break;
			case optRuleDisable:
				operation = doRuleDisable;
				break;
			case optRuleEnable:
				operation = doRuleEnable;
				break;
			case optMasterDisable:
				operation = doMasterDisable;
				break;
			case optMasterEnable:
				operation = doMasterEnable;
				break;
			case optDevIDStatus:
				operation = doDevIDStatus;
				break;
			case optDevIDDisable:
				operation = doDevIDDisable;
				break;
			case optDevIDEnable:
				operation = doDevIDEnable;
				break;
			case optFeatures:
				featureCheck = optarg;
				break;
			case optHash:
				specification = specHash;
				break;
			case optIgnoreCache:
				assessmentFlags |= kSecAssessmentFlagIgnoreCache;
				break;
			case optLabel:
				label = optarg;
				break;
			case optNoCache:
				assessmentFlags |= kSecAssessmentFlagNoCache;
				break;
			case optPath:
				specification = specPath;
				break;
			case optPriority:
				priority = optarg;
				break;
			case optPurge:
				operation = doPurge;
				break;
			case optRawOutput:
				rawOutput = true;
				break;
			case optRemarks:
				remarks = optarg;
				break;
			case optRemove:
				operation = doRemove;
				break;
			case optRequirement:
				specification = specRequirement;
				break;
			case optRule:
				specification = specRule;
				break;
			case optStatus:
				operation = doStatus;
				break;
								
			case '?':
				usage();
			}
		
		if (featureCheck) {
			checkFeatures(featureCheck);
			if (operation == doNothing)
				exit(0);
		}
	
		// dispatch operations with no arguments
		switch (operation) {
		case doNothing:
			usage();
		case doStatus:
		case doMasterEnable:
		case doMasterDisable:
		case doDevIDStatus:
		case doDevIDEnable:
		case doDevIDDisable:
			if (optind != argc)
				usage();
			status(operation);
			exit(0);
		case doRemove:		// optional arguments
			if (optind == argc) {
				removeAuthority(NULL);
				exit(0);
			}
			break;
		case doRuleEnable:
			if (optind == argc) {
				enableAuthority(NULL);
				exit(0);
			}
			break;
		case doRuleDisable:
			if (optind == argc) {
				disableAuthority(NULL);
				exit(0);
			}
			break;
		case doList:
			if (optind == argc) {
				listAuthority(NULL);
				exit(0);
			}
			break;
		default:
			if (optind == argc)
				usage();
			break;
		}
		
		// operate on paths given after options
		for ( ; optind < argc; optind++) {
			const char *target = argv[optind];
			try {
				switch (operation) {
				case doAssess:
					assess(target);
					break;
				case doAdd:
					addAuthority(target);
					break;
				case doRemove:
					removeAuthority(target);
					break;
				case doRuleEnable:
					enableAuthority(target);
					break;
				case doRuleDisable:
					disableAuthority(target);
					break;
				case doList:
					listAuthority(target);
					break;
				default:
					assert(false);
				}
			} catch (...) {
				diagnose(target);
				if (!exitcode)
					exitcode = exitFailure;
				if (!continueOnError)
					exit(exitFailure);
			}
		}

	} catch (...) {
		diagnose(NULL, exitFailure);
	}

	exit(exitcode);
}

void usage()
{
	fprintf(stderr, "Usage: spctl --assess [--type type] [-v] path ... # assessment\n"
		"       spctl --add [--type type] [--path|--requirement|--anchor|--hash] spec ... # add rule(s)\n"
		"       spctl [--enable|--disable|--remove] [--type type] [--path|--requirement|--anchor|--hash|--rule] spec # change rule(s)\n"
		"       spctl --status | --master-enable | --master-disable # system master switch\n"
	);
	exit(exitUsage);
}


//
// Perform an assessment operation.
// This does not change anything (except possibly, indirectly, the object cache).
//
void assess(const char *target)
{
	SecAssessmentFlags flags = assessmentFlags;
	if (verbose > 1)
		flags |= kSecAssessmentFlagRequestOrigin;
	if (assessmentType)
		CFDictionaryAddValue(context, kSecAssessmentContextKeyOperation, typeKey(assessmentType));
	
	CheckedRef<SecAssessmentRef> ass;
	ass.check(SecAssessmentCreate(CFTempURL(target), flags, context, ass));
	CheckedRef<CFDictionaryRef> outcome;
	outcome.check(SecAssessmentCopyResult(ass, outcomeFlags, outcome));

	CFDictionary result(outcome.get(), 0);
	bool success = result.get<CFBooleanRef>(kSecAssessmentAssessmentVerdict) == kCFBooleanTrue;
	CFDictionary authority(result.get<CFDictionaryRef>(kSecAssessmentAssessmentAuthority), 0);
	CFStringRef source = NULL;
	if (authority)
		source = authority.get<CFStringRef>(kSecAssessmentAssessmentSource);
	
	// if the result is a whitelisted weak signature, bend the polite (but not raw) output to our will
	if (!rawOutput && authority) {
		if (CFBooleanRef weak = authority.get<CFBooleanRef>(kSecAssessmentAssessmentWeakSignature))
			if (CFEqual(weak, kCFBooleanTrue)) {
					// succeeded only because of weak-signature whitelist. Report it as failed
					success = false;
					if (source && CFEqual(source, CFSTR("allowed cdhash")))
						source = CFSTR("matched cdhash");
				}
	}
	
	if (success) {
		note(1, "%s: accepted", target);
	} else {
		note(0, "%s: rejected", target);
		if (!exitcode)
			exitcode = exitNoverify;
	}
	
	if (rawOutput) {
		if (CFRef<CFDataRef> xml = makeCFData(outcome.get()))
			fwrite(CFDataGetBytePtr(xml), CFDataGetLength(xml), 1, stdout);
	} else if (verbose) {
		if (authority) {
			if (source)
				note(1, "source=%s", cfString(source).c_str());
			if (CFBooleanRef cached = authority.get<CFBooleanRef>(kSecAssessmentAssessmentFromCache)) {
				if (cached == kCFBooleanFalse)
					note(2, "cache=no");
				else if (CFNumberRef row = authority.get<CFNumberRef>(kSecAssessmentAssessmentAuthorityRow))
					note(2, "cache=yes,row %d", cfNumber<int>(row));
				else
					note(2, "cache=yes");
			}
			if (CFStringRef override = authority.get<CFStringRef>(kSecAssessmentAssessmentAuthorityOverride))
				note(0, "override=%s", cfString(override).c_str());
		} else
			note(1, "authority=none");
	}
	if (CFStringRef originator = result.get<CFStringRef>(kSecAssessmentAssessmentOriginator))
		note(2, "origin=%s", cfString(originator).c_str());
}


//
// Apply a change to the system-wide authority configuration.
// These are all privileged operations, of course.
//
static CFDictionaryRef updateOperation(const char *target, CFMutableDictionaryRef context,
	CFStringRef operation)
{
	SecCSFlags flags = assessmentFlags;
	CFDictionaryAddValue(context, kSecAssessmentContextKeyUpdate, operation);
	if (assessmentType)
		CFDictionaryAddValue(context, kSecAssessmentContextKeyOperation, typeKey(assessmentType));

	CFRef<CFTypeRef> subject;
	if (target)
		switch (specification) {
		case specPath:
			{
				subject = makeCFURL(target);
				// For add operations, add a bookmark so we get icons
				if (operation == kSecAssessmentUpdateOperationAdd) {
					CFRef<CFDataRef> bookmark = CFURLCreateBookmarkData(NULL, subject.as<CFURLRef>(), 0, NULL, NULL, NULL);
					if (bookmark)
						CFDictionaryAddValue(context, kSecAssessmentRuleKeyBookmark, bookmark);
				}
				break;
			}
		case specRequirement:
			MacOSError::check(SecRequirementCreateWithString(CFTempString(target),
				kSecCSDefaultFlags, (SecRequirementRef *)&subject.aref()));
			break;
		case specAnchor:
			{
				string reqString;
				if (target[0] == '/') {	// assume path to anchor cert on disk
					reqString = "anchor " + fileHash(target);
				} else {
					reqString = "anchor " + hashArgument(target);
				}
				MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString),
					kSecCSDefaultFlags, (SecRequirementRef *)&subject.aref()));
				break;
			}
		case specHash:
			{
				string reqString = "cdhash " + hashArgument(target);
				MacOSError::check(SecRequirementCreateWithString(CFTempString(reqString),
					kSecCSDefaultFlags, (SecRequirementRef *)&subject.aref()));
				break;
			}
		case specRule:
			{
				if (operation == kSecAssessmentUpdateOperationAdd)
					fail("cannot insert by rule number");
				char *end;
				uint64_t rule = strtol(target, &end, 0);
				if (*end)
					fail("%s: invalid rule number", target);
				subject.take(CFTempNumber(rule));
				break;
			}
		}

	if (label)
		CFDictionaryAddValue(context, kSecAssessmentUpdateKeyLabel, CFTempString(label));
	if (priority) {
		char *end;
		double pri = strtod(priority, &end);
		if (*end)	// empty or bad conversion
			fail("%s: invalid rule priority", priority);
		CFDictionaryAddValue(context, kSecAssessmentUpdateKeyPriority, CFTempNumber(pri));
	}
	if (remarks)
		CFDictionaryAddValue(context, kSecAssessmentUpdateKeyRemarks, CFTempString(remarks));
	
	ErrorCheck check;
	CFRef<CFDictionaryRef> outcome = SecAssessmentCopyUpdate(subject.get(), flags, context, check);
	check(outcome);
	
	if (rawOutput)
		if (CFRef<CFDataRef> xml = makeCFData(outcome.get()))
			fwrite(CFDataGetBytePtr(xml), CFDataGetLength(xml), 1, stdout);
	
	return outcome.yield();
}

void addAuthority(const char *target)
{
	CFDictionary result(updateOperation(target, context, kSecAssessmentUpdateOperationAdd), noErr);
	if (verbose && !rawOutput)
		printf("Created rule %lld\n", cfNumber<long long>(result.get<CFNumberRef>(kSecAssessmentUpdateKeyRow)));
}


void removeAuthority(const char *target)
{
	CFDictionary result(updateOperation(target, context, kSecAssessmentUpdateOperationRemove), noErr);
	if (verbose && !rawOutput)
		printf("Removed %lld rule(s)\n", cfNumber<long long>(result.get<CFNumberRef>(kSecAssessmentUpdateKeyCount)));
}

void enableAuthority(const char *target)
{
	CFDictionary result(updateOperation(target, context, kSecAssessmentUpdateOperationEnable), noErr);
	if (verbose && !rawOutput)
		printf("Enabled %lld rule(s)\n", cfNumber<long long>(result.get<CFNumberRef>(kSecAssessmentUpdateKeyCount)));
}

void disableAuthority(const char *target)
{
	CFDictionary result(updateOperation(target, context, kSecAssessmentUpdateOperationDisable), noErr);
	if (verbose && !rawOutput)
		printf("Disabled %lld rule(s)\n", cfNumber<long long>(result.get<CFNumberRef>(kSecAssessmentUpdateKeyCount)));
}

void listAuthority(const char *target)
{
	CFDictionary result(updateOperation(target, context, kSecAssessmentUpdateOperationFind), noErr);
	if (rawOutput)
		return;
	CFArrayRef rules = result.get<CFArrayRef>(kSecAssessmentUpdateKeyFound);
	CFIndex count = CFArrayGetCount(rules);
	for (CFIndex n = 0; n < count; n++) {
		CFDictionary rule(CFArrayGetValueAtIndex(rules, n), noErr);
		string typeString = "?";
		if (CFStringRef type = rule.get<CFStringRef>(kSecAssessmentRuleKeyType)) {
			typeString = cfString(type);
			string::size_type colon = typeString.find(':');
			if (colon != string::npos)
				typeString = typeString.substr(colon+1);
		}
		string label = "UNLABELED";
		if (CFStringRef lab = rule.get<CFStringRef>(kSecAssessmentRuleKeyLabel))
			label = cfString(lab);
		printf("%lld[%s] P%g %s %s",
			cfNumber<long long>(rule.get<CFNumberRef>(kSecAssessmentRuleKeyID)),
			label.c_str(),
			cfNumber<double>(rule.get<CFNumberRef>(kSecAssessmentRuleKeyPriority)),
			(rule.get<CFBooleanRef>(kSecAssessmentRuleKeyAllow) == kCFBooleanTrue) ? "allow" : "deny",
			typeString.c_str()
		);
		if (CFStringRef remarks = rule.get<CFStringRef>(kSecAssessmentRuleKeyRemarks))
			printf(" [%s]", cfString(remarks).c_str());
		printf("\n");
		printf("\t%s\n",
			cfString(rule.get<CFStringRef>(kSecAssessmentRuleKeyRequirement)).c_str()
		);
	}
}


//
// Manipulate the master status.
// This reports on, or changes, the master enable status.
// It does not actually affect the authority database, though
// it may tell the system to bypass it altogether.
//
void status(Operation op)
{
	ErrorCheck check;
	CFBooleanRef state;
	switch (op) {
	case doStatus:
		check(SecAssessmentControl(CFSTR("ui-status"), &state, check));
		if (state == kCFBooleanTrue) {
			printf("assessments enabled\n");
			if (verbose > 0) {
				check(SecAssessmentControl(CFSTR("ui-get-devid"), &state, check));
				if (state == kCFBooleanTrue)
					printf("developer id enabled\n");
				else
					printf("developer id disabled\n");
			}
			exit(0);
		} else {
			printf("assessments disabled\n");
			exit(1);
		}
	case doDevIDStatus:
		check(SecAssessmentControl(CFSTR("ui-get-devid"), &state, check));
		if (state == kCFBooleanTrue) {
			printf("devid enabled\n");
			exit(0);
		} else {
			printf("devid disabled\n");
			exit(1);
		}
	case doMasterEnable:
		check(SecAssessmentControl(CFSTR("ui-enable"), NULL, check));
		exit(0);
	case doMasterDisable:
		check(SecAssessmentControl(CFSTR("ui-disable"), NULL, check));
		exit(0);
	case doDevIDEnable:
		check(SecAssessmentControl(CFSTR("ui-enable-devid"), NULL, check));
		exit(0);
	case doDevIDDisable:
		check(SecAssessmentControl(CFSTR("ui-disable-devid"), NULL, check));
		exit(0);
	default:
		assert(false);
	}
}


//
// Support helper functions
//
static CFTypeRef typeKey(const char *type)
{
	if (!strncmp(type, "execute", strlen(type)))
		return kSecAssessmentOperationTypeExecute;
	else if (!strncmp(type, "install", strlen(type)))
		return kSecAssessmentOperationTypeInstall;
	else if (!strncmp(type, "open", strlen(type)))
		return kSecAssessmentOperationTypeOpenDocument;
	else
		fail("%s: unrecognized assessment type", type);
}

static string hashArgument(const char *s)
{
	for (const char *p = s; *p; p++)
		if (!isxdigit(*p))
			fail("%s: invalid hash specification", s);
	return string("H\"") + s + "\"";
}

static string fileHash(const char *path)
{
	CFRef<CFDataRef> certData = cfLoadFile(path);
	SHA1 hash;
	hash.update(CFDataGetBytePtr(certData), CFDataGetLength(certData));
	SHA1::Digest digest;
	hash.finish(digest);
	string s;
	for (const SHA1::Byte *p = digest; p < digest + sizeof(digest); p++) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%02.2x", *p);
		s += buf;
	}
	return hashArgument(s.c_str());
}


//
// Exit unless each of the comma-separated feature names is supported
// by this version of spctl(8).
//
void checkFeatures(const char *arg)
{
	while (true) {
		const char *comma = strchr(arg, ',');
		string feature = comma ? string(arg, comma-arg) : arg;
		if (feature.empty())
			fail("Invalid feature name");
		const char **p;
		for (p = features; *p && feature != *p; p++) ;
		if (!*p)
			fail("%s: not supported in this version", feature.c_str());
		if (comma) {
			arg = comma + 1;
			if (!*arg)	// tolerate trailing comma
				break;
		} else {
			break;
		}
	}
}
