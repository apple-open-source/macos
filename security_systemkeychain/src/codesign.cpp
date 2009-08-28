/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// cs_dump - codesign dump/display operation
//
#include "codesign.h"
#include <Security/SecKeychain.h>
#include <cmath>
#include <getopt.h>

using namespace UnixPlusPlus;


//
// Operational mode
//
enum Operation {
	doNothing,						// none given (print usage)
	doSign,							// sign code
	doVerify,						// verify code
	doDump,							// dump/display signature
	doHostingInfo,					// build and display hosting chain
	doProcInfo,						// process state information
	doProcAction					// process state manipulation
};
Operation operation = doNothing;


//
// Command-line arguments and options
//
int pagesize = pagesizeUnspecified;	// signing page size (-1 => not specified)
SecIdentityRef signer = NULL;		// signer identity
SecKeychainRef keychain = NULL;		// source keychain for signer identity
const char *internalReq = NULL;		// internal requirement (raw optarg)
const char *testReq = NULL;			// external requirement (raw optarg)
const char *detached = NULL;		// detached signature path (to explicit file)
const char *detachedDb = NULL;		// reference to detached signature database
const char *entitlements = NULL;	// path to entitlement configuration input
const char *resourceRules = NULL;	// explicit resource rules template
const char *uniqueIdentifier = NULL; // unique ident hash
const char *identifierPrefix = NULL; // prefix for un-dotted default identifiers
const char *modifiedFiles = NULL;	// file to receive list of modified files
const char *extractCerts = NULL;	// location for extracting signing chain certificates
SecCSFlags verifyOptions = kSecCSDefaultFlags; // option flags to static verifications
CFDateRef signingTime;				// explicit signing time option
size_t signatureSize = 0;			// override CMS blob estimate
uint32_t cdFlags = 0;				// CodeDirectory flags requested
const char *procAction = NULL;		// action-on-process(es) requested
bool noMachO = false;				// force non-MachO operation
bool dryrun = false;				// do not actually change anything
bool preserveMetadata = false;		// keep metadata from previous signature (if any)


//
// Local functions
//
static void usage();
static OSStatus keychain_open(const char *name, SecKeychainRef &keychain);


//
// Command-line options
//
enum {
	optCheckExpiration = 1,
	optContinue,
	optDetachedDatabase,
	optDryRun,
	optExtractCerts,
	optEntitlements,
	optFileList,
	optIdentifierPrefix,
	optIgnoreResources,
	optKeychain,
	optNoMachO,
	optPreserveMetadata,
	optProcInfo,
	optProcAction,
	optRemoveSignature,
	optResourceRules,
	optSigningTime,
	optSignatureSize,
};

const struct option options[] = {
	{ "dump",		no_argument,			NULL, 'd' },
	{ "display",	no_argument,			NULL, 'd' },
	{ "detached",	required_argument,		NULL, 'D' },
	{ "force",		no_argument,			NULL, 'f' },
	{ "help",		no_argument,			NULL, '?' },
	{ "hosting",	no_argument,			NULL, 'h' },
	{ "identifier",	required_argument,		NULL, 'i' },
	{ "options",	required_argument,		NULL, 'o' },
	{ "pagesize",	required_argument,		NULL, 'P' },
	{ "requirements", required_argument,	NULL, 'r' },
	{ "test-requirement", required_argument,NULL, 'R' },
	{ "sign",		required_argument,		NULL, 's' },
	{ "verbose",	optional_argument,		NULL, 'v' },
	{ "verify",		no_argument,			NULL, 'v' },

	{ "check-expiration", no_argument,		NULL, optCheckExpiration },
	{ "continue",	no_argument,			NULL, optContinue },
	{ "detached-database", optional_argument, NULL, optDetachedDatabase },
	{ "dryrun",		no_argument,			NULL, optDryRun },
	{ "entitlements", required_argument,	NULL, optEntitlements },
	{ "expired",	no_argument,			NULL, optCheckExpiration },
	{ "extract-certificates", optional_argument, NULL, optExtractCerts },
	{ "file-list",	required_argument,		NULL, optFileList },
	{ "ignore-resources", no_argument,		NULL, optIgnoreResources },
	{ "keychain",	required_argument,		NULL, optKeychain },
	{ "no-macho",	no_argument,			NULL, optNoMachO },
	{ "prefix",		required_argument,		NULL, optIdentifierPrefix },
	{ "preserve-metadata", no_argument,		NULL, optPreserveMetadata },
	{ "procaction",	required_argument,		NULL, optProcAction },
	{ "procinfo",	no_argument,			NULL, optProcInfo },
	{ "remove-signature", no_argument,		NULL, optRemoveSignature },
	{ "resource-rules", required_argument,	NULL, optResourceRules },
	{ "signature-size", required_argument,	NULL, optSignatureSize },
	{ "signing-time", required_argument,	NULL, optSigningTime },
	{ }
};


//
// codesign [options] bundle-path
//
int main(int argc, char *argv[])
{
	try {
		const char *signerName = NULL;
		int arg, argslot;
		while (argslot = -1,
				(arg = getopt_long(argc, argv, "dD:fhi:o:P:r:R:s:v", options, &argslot)) != -1)
			switch (arg) {
			case 'd':
				operation = doDump;
				break;
			case 'D':
				detached = optarg;
				break;
			case 'f':
				force = true;
				break;
			case 'h':
				operation = doHostingInfo;
				break;
			case 'i':
				uniqueIdentifier = optarg;
				break;
			case 'o':
				cdFlags = parseCdFlags(optarg);
				break;
			case 'P':
				{
					if (pagesize = atol(optarg)) {
						int pslog;
						if (frexp(pagesize, &pslog) != 0.5)
							fail("page size must be a power of two");
					}
					break;
				}
			case 'r':
				internalReq = optarg;
				break;
			case 'R':
				testReq = optarg;
				break;
			case 's':
				signerName = optarg;
				operation = doSign;
				break;
			case 'v':
				if (argslot < 0)								// -v
					verbose++;
				else if (options[argslot].has_arg == no_argument)
					operation = doVerify;						// --verify
				else if (optarg)
					verbose = atoi(optarg);						// --verbose=level
				else						
					verbose++;									// --verbose
				break;
			
			case optCheckExpiration:
				verifyOptions |= kSecCSConsiderExpiration;
				break;
			case optContinue:
				continueOnError = true;
				break;
			case optDetachedDatabase:
				if (optarg)
					detachedDb = optarg;
				else
					detachedDb = "system";
				break;
			case optDryRun:
				dryrun = true;
				break;
			case optEntitlements:
				entitlements = optarg;
				break;
			case optExtractCerts:
				if (optarg)
					extractCerts = optarg;
				else
					extractCerts = "./codesign";
				break;
			case optFileList:
				modifiedFiles = optarg;
				break;
			case optIdentifierPrefix:
				identifierPrefix = optarg;
				break;
			case optIgnoreResources:
				verifyOptions |= kSecCSDoNotValidateResources;
				break;
			case optKeychain:
				MacOSError::check(keychain_open(optarg, keychain));
				break;
			case optNoMachO:
				noMachO = true;
				break;
			case optPreserveMetadata:
				preserveMetadata = true;
				break;
			case optProcAction:
				operation = doProcAction;
				procAction = optarg;
				break;
			case optProcInfo:
				operation = doProcInfo;
				break;
			case optRemoveSignature:
				signerName = NULL;
				operation = doSign;		// well, un-sign
				break;
			case optResourceRules:
				resourceRules = optarg;
				break;
			case optSignatureSize:
				signatureSize = atol(optarg);
				break;
			case optSigningTime:
				signingTime = parseDate(optarg);
				break;
				
			case '?':
				usage();
			}

		if (signerName)
			signer = findIdentity(keychain, signerName);
	} catch (...) {
		diagnose(NULL, exitFailure);
	}
	
	// -v does double duty as -v(erbose) and -v(erify)
	if (operation == doNothing && verbose) {
		operation = doVerify;
		verbose--;
	}
	if (optind == argc || operation == doNothing)
		usage();
	
	// masticate the more interesting arguments
	try {
		switch (operation) {
		case doSign:
			prepareToSign();
			break;
		case doVerify:
			prepareToVerify();
			break;
		}
	} catch (...) {
		diagnose(NULL, exitFailure);
	}
	
	// operate on paths given after options
	for ( ; optind < argc; optind++) {
		const char *target = argv[optind];
		try {
			switch (operation) {
			case doSign:
				sign(target);
				break;
			case doVerify:
				verify(target);
				break;
			case doDump:
				dump(target);
				break;
			case doHostingInfo:
				hostinginfo(target);
				break;
			case doProcInfo:
				procinfo(target);
				break;
			case doProcAction:
				procaction(target);
				break;
			}
		} catch (...) {
			diagnose(target);
			if (!exitcode)
				exitcode = exitFailure;
			if (!continueOnError)
				exit(exitFailure);
		}
	}

	exit(exitcode);
}

void usage()
{
	fprintf(stderr, "Usage: codesign -s identity [-fv*] [-o flags] [-r reqs] [-i ident] path ... # sign\n"
		"       codesign -v [-v*] [-R testreq] path|pid ... # verify\n"
		"       codesign -d [options] path ... # display contents\n"
		"       codesign -h pid ... # display hosting paths\n"
	);
	exit(exitUsage);
}

OSStatus
keychain_open(const char *name, SecKeychainRef &keychain)
{
	OSStatus result;

//	check_obsolete_keychain(name);
	if (name && name[0] != '/')
	{
		CFArrayRef dynamic = NULL;
		result = SecKeychainCopyDomainSearchList(
			kSecPreferencesDomainDynamic, &dynamic);
		if (result)
			return result;
		else
		{
			uint32_t i;
			uint32_t count = dynamic ? CFArrayGetCount(dynamic) : 0;

			for (i = 0; i < count; ++i)
			{
				char pathName[PATH_MAX];
				UInt32 ioPathLength = sizeof(pathName);
				bzero(pathName, ioPathLength);
				keychain = (SecKeychainRef)CFArrayGetValueAtIndex(dynamic, i);
				result = SecKeychainGetPath(keychain, &ioPathLength, pathName);
				if (result)
					return result;

				if (!strncmp(pathName, name, ioPathLength))
				{
					CFRetain(keychain);
					CFRelease(dynamic);
					return noErr;
				}
			}
			CFRelease(dynamic);
		}
	}

	return SecKeychainOpen(name, &keychain);
}


