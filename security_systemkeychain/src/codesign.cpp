/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
#include "cs_utils.h"
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
const char *sdkRoot = NULL;			// alternate root for looking up sub-components
const char *featureCheck = NULL;	// feature support check
SecCSFlags staticVerifyOptions = kSecCSCheckAllArchitectures; // option flags to static verifications
SecCSFlags dynamicVerifyOptions = kSecCSDefaultFlags; // option flags to static verifications
uint32_t digestAlgorithm = 0;		// digest algorithm to be used when signing
CFDateRef signingTime;				// explicit signing time option
size_t signatureSize = 0;			// override CMS blob estimate
uint32_t cdFlags = 0;				// CodeDirectory flags requested
const char *procAction = NULL;		// action-on-process(es) requested
Architecture architecture;			// specific binary architecture to process (from a universal file)
const char *bundleVersion;			// specific version string requested (from a versioned bundle)
bool noMachO = false;				// force non-MachO operation
bool dryrun = false;				// do not actually change anything
bool allArchitectures = false;		// process all architectures in a universal (aka fat) code file
int preserveMetadata = 0;			// what metadata to keep from previous signature


//
// Feature set
//
static const char *features[] = {
	"hash-identities",				// supports -s hash-of-certificate
	"identity-preferences",		// supports -s identity-preference-name
	"deep-verify",					// supports --deep-verify
	NULL							// sentinel
};


//
// Local functions
//
static void usage();
static OSStatus keychain_open(const char *name, SecKeychainRef &keychain);
static bool chooseArchitecture(const char *arg);
static uint32_t parseMetadataFlags(const char *arg);
static void checkFeatures(const char *arg);


//
// Command-line options
//
enum {
	optNone = 0,	// null (no, absent) option
	optAllArchitectures,
	optBundleVersion,
	optCheckExpiration,
	optContinue,
	optDeepVerify,
	optDetachedDatabase,
	optDigestAlgorithm,
	optDryRun,
	optExtractCerts,
	optEntitlements,
	optFeatures,
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
	optSDKRoot,
	optSigningTime,
	optSignatureSize,
};

const struct option options[] = {
	{ "architecture", required_argument,	NULL, 'a' },
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

	{ "all-architectures", no_argument,		NULL, optAllArchitectures },
	{ "bundle-version", required_argument,	NULL, optBundleVersion },
	{ "check-expiration", no_argument,		NULL, optCheckExpiration },
	{ "continue",	no_argument,			NULL, optContinue },
	{ "deep-verify", no_argument,			NULL, optDeepVerify },
	{ "detached-database", optional_argument, NULL, optDetachedDatabase },
	{ "digest-algorithm", required_argument, NULL, optDigestAlgorithm },
	{ "dryrun",		no_argument,			NULL, optDryRun },
	{ "entitlements", required_argument,	NULL, optEntitlements },
	{ "expired",	no_argument,			NULL, optCheckExpiration },
	{ "extract-certificates", optional_argument, NULL, optExtractCerts },
	{ "features",	optional_argument,		NULL, optFeatures },
	{ "file-list",	required_argument,		NULL, optFileList },
	{ "ignore-resources", no_argument,		NULL, optIgnoreResources },
	{ "keychain",	required_argument,		NULL, optKeychain },
	{ "no-macho",	no_argument,			NULL, optNoMachO },
	{ "prefix",		required_argument,		NULL, optIdentifierPrefix },
	{ "preserve-metadata", optional_argument, NULL, optPreserveMetadata },
	{ "procaction",	required_argument,		NULL, optProcAction },
	{ "procinfo",	no_argument,			NULL, optProcInfo },
	{ "remove-signature", no_argument,		NULL, optRemoveSignature },
	{ "resource-rules", required_argument,	NULL, optResourceRules },
	{ "sdkroot", required_argument,			NULL, optSDKRoot },
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
				(arg = getopt_long(argc, argv, "a:dD:fhi:o:P:r:R:s:v", options, &argslot)) != -1)
			switch (arg) {
			case 'a':
				if (chooseArchitecture(optarg))
					staticVerifyOptions &= ~kSecCSCheckAllArchitectures;
				else
					usage();
				break;
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
			
			case optAllArchitectures:
				allArchitectures = true;
				staticVerifyOptions |= kSecCSCheckAllArchitectures;
				break;
			case optBundleVersion:
				bundleVersion = optarg;
				break;
			case optCheckExpiration:
				staticVerifyOptions |= kSecCSConsiderExpiration;
				dynamicVerifyOptions |= kSecCSConsiderExpiration;
				break;
			case optContinue:
				continueOnError = true;
				break;
			case optDeepVerify:
				staticVerifyOptions |= kSecCSCheckNestedCode;
				break;
			case optDetachedDatabase:
				if (optarg)
					detachedDb = optarg;
				else
					detachedDb = "system";
				break;
			case optDigestAlgorithm:
				digestAlgorithm = findHashType(optarg)->code;
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
			case optFeatures:
				if (optarg)
					featureCheck = optarg;
				else {
					for (const char **p = features; *p; p++)
						printf("%s\n", *p);
					exit(0);
				}
				break;
			case optFileList:
				modifiedFiles = optarg;
				break;
			case optIdentifierPrefix:
				identifierPrefix = optarg;
				break;
			case optIgnoreResources:
				staticVerifyOptions |= kSecCSDoNotValidateResources;
				break;
			case optKeychain:
				MacOSError::check(keychain_open(optarg, keychain));
				break;
			case optNoMachO:
				noMachO = true;
				break;
			case optPreserveMetadata:
				preserveMetadata = parseMetadataFlags(optarg);
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
			case optSDKRoot:
				sdkRoot = optarg;
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
	if (featureCheck) {
		checkFeatures(featureCheck);
		if (operation == doNothing)
			exit(0);
	}
	if (operation == doNothing || optind == argc)
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


bool chooseArchitecture(const char *arg)
{
	int arch, subarch;
	switch (sscanf(arg, "%d,%d", &arch, &subarch)) {
	case 0:		// not a number
		if (!(architecture = Architecture(arg)))
			fail("%s: unknown architecture name", arg);
		break;
	case 1:
		architecture = Architecture(arch);
		break;
	case 2:
		architecture = Architecture(arch, subarch);
		break;
	}
}


static uint32_t parseMetadataFlags(const char *arg)
{
	static const SecCodeDirectoryFlagTable metadataFlags[] = {
		{ "identifier",	kPreserveIdentifier,		true },
		{ "requirements", kPreserveRequirements,	true },
		{ "entitlements", kPreserveEntitlements,	true },
		{ "resource-rules", kPreserveResourceRules,	true },
		{ NULL }
	};
	if (arg == NULL) {	// --preserve-metadata compatibility default
		uint32_t flags = kPreserveRequirements | kPreserveEntitlements | kPreserveResourceRules;
		if (!getenv("RC_XBS") || getenv("RC_BUILDIT"))	// if we're NOT in real B&I...
			flags |= kPreserveIdentifier;				// ... then preserve identifier too
		return flags;
	} else {
		return parseOptionTable(arg, metadataFlags);
	}
}


//
// Exit unless each of the comma-separated feature names is supported
// by this version of codesign(1).
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
