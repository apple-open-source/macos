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
	doStatus,						// enable/disable/query status
	doEnable,
	doDisable,
};
Operation operation = doNothing;


//
// Command-line arguments and options
//
const char *assessmentType;
SecAssessmentFlags assessmentFlags;
bool rawOutput;
const char *featureCheck;


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
static void status(Operation op);

static CFTypeRef typeKey(const char *type);


//
// Command-line options
//
enum {
	optNone = 0,	// null (no, absent) option
	optContinue,
	optDirect,
	optNoCache,
	optIgnoreCache,
	optStatus,
	optEnable,
	optDisable,
	optRawOutput,
	optFeatures,
};

const struct option options[] = {
	{ "assess",		no_argument,			NULL, 'a' },
	{ "continue",	no_argument,			NULL, optContinue },
	{ "direct",		no_argument,			NULL, 'D' },
	{ "status",		optional_argument,		NULL, optStatus },
	{ "enable",		no_argument,			NULL, optEnable },
	{ "disable",	no_argument,			NULL, optDisable },
	{ "features",	optional_argument,		NULL, optFeatures },
	{ "ignore-cache", no_argument,			NULL, optIgnoreCache },
	{ "no--cache",	no_argument,			NULL, optNoCache },
	{ "type",		required_argument,		NULL, 't' },
	{ "raw",		no_argument,			NULL, optRawOutput },
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
				(arg = getopt_long(argc, argv, "aDt:v", options, &argslot)) != -1)
			switch (arg) {
			case 'a':
				operation = doAssess;
				break;
			case 'D':
				assessmentFlags |= kSecAssessmentFlagDirect;
				break;
			case 't':
				assessmentType = optarg;
				break;
			case 'v':
				verbose++;
				break;
			
			case optNoCache:
				assessmentFlags |= kSecAssessmentFlagNoCache;
				break;
			case optContinue:
				continueOnError = true;
				break;
			case optRawOutput:
				rawOutput = true;
				break;
			case optFeatures:
				featureCheck = optarg;
				break;
				
			case optStatus:
				operation = doStatus;
				break;
			case optEnable:
				operation = doEnable;
				break;
			case optDisable:
				operation = doDisable;
				break;
				
			case '?':
				usage();
			}
		
		if (featureCheck) {
			checkFeatures(featureCheck);
			if (operation == doNothing)
				exit(0);
		}
	
		// operations that take no arguments
		switch (operation) {
		case doNothing:
			usage();
		case doStatus:
		case doEnable:
		case doDisable:
			if (optind != argc)
				usage();
			status(operation);
			exit(0);
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
	fprintf(stderr, "Usage: spctl -a [-t type] [-v] path ... # assessment\n"
		"       spctl --status | --enable | --disable # system master switch\n"
	);
	exit(exitUsage);
}


void assess(const char *target)
{
	CFMutableDictionaryRef context = makeCFMutableDictionary();
	SecAssessmentFlags flags = assessmentFlags;
	if (verbose > 1)
		flags |= kSecAssessmentFlagRequestOrigin;
	if (assessmentType)
		CFDictionaryAddValue(context, kSecAssessmentContextKeyOperation, typeKey(assessmentType));
	
	CheckedRef<SecAssessmentRef> ass;
	ass.check(SecAssessmentCreate(CFTempURL(target), flags, context, ass));
	CheckedRef<CFDictionaryRef> outcome;
	outcome.check(SecAssessmentCopyResult(ass, kSecAssessmentDefaultFlags, outcome));

	CFDictionary result(outcome.get(), 0);
	bool success = result.get<CFBooleanRef>(kSecAssessmentAssessmentVerdict) == kCFBooleanTrue;
	
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
		CFDictionary authority(result.get<CFDictionaryRef>(kSecAssessmentAssessmentAuthority), 0);
		if (authority) {
			if (CFStringRef source = authority.get<CFStringRef>(kSecAssessmentAssessmentSource))
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


void status(Operation op)
{
	ErrorCheck check;
	switch (op) {
	case doStatus:
		{
			CFBooleanRef state;
			check(SecAssessmentControl(CFSTR("ui-status"), &state, check));
			if (state == kCFBooleanTrue) {
				printf("assessments enabled\n");
				exit(0);
			} else {
				printf("assessments disabled\n");
				exit(1);
			}
		}
	case doEnable:
		check(SecAssessmentControl(CFSTR("ui-enable"), NULL, check));
		exit(0);
	case doDisable:
		check(SecAssessmentControl(CFSTR("ui-disable"), NULL, check));
		exit(0);
	default:
		assert(false);
	}
}

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
