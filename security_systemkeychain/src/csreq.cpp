/*
 * Copyright (c) 2006-2007 Apple Computer, Inc. All Rights Reserved.
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
// csreq - code requirement munging tool for Code Signing
//
#include "cs_utils.h"
#include <Security/CodeSigning.h>
#include <Security/SecRequirementPriv.h>

using namespace UnixPlusPlus;


//
// Command-line arguments and options
//
const char *requirement = NULL;		// requirement input
const char *output = NULL;			// output file

static enum {
	outputCheck,
	outputText,
	outputBinary
} outputType = outputCheck;

static SecCSFlags reqType = kSecCSDefaultFlags;


//
// Local functions
//
static void usage();
static SecCSFlags type(const char *t);


//
// Command-line options
//
enum {
	optType
};

const struct option options[] = {
	{ "binary",			required_argument,	NULL, 'b' },
	{ "check",			no_argument,		NULL, 'c' },
	{ "requirements",	required_argument,	NULL, 'r' },
	{ "text",			no_argument,		NULL, 't' },
	{ "type",			required_argument,	NULL, optType },
	{ "verbose",		optional_argument,	NULL, 'v' },
	{ }
};


//
// codesign [options] bundle-path
//
int main(int argc, char *argv[])
{
	try {
		extern int optind;
		extern char *optarg;
		int arg, argslot;
		while (argslot = -1,
				(arg = getopt_long(argc, argv, "b:ctr:v", options, &argslot)) != -1)
			switch (arg) {
			case 'b':
				outputType = outputBinary;
				output = optarg;
				break;
			case 't':
				outputType = outputText;
				break;
			case 'r':
				requirement = optarg;
				break;
			case 'v':
				if (argslot < 0)								// -v
					verbose++;
				else if (optarg)
					verbose = atoi(optarg);						// --verbose=level
				else						
					verbose++;									// --verbose
				break;
			
			case optType:
				reqType = type(optarg);
				break;
				
			case '?':
				usage();
			}
		
		if (requirement == NULL)
			usage();
		
		CFRef<CFTypeRef> req = readRequirement(requirement, reqType);
		assert(req);
		
		switch (outputType) {
		case outputCheck:
			note(1, "valid");
			break;
		case outputText:
			{
				CFRef<CFStringRef> text;
				MacOSError::check(SecRequirementsCopyString(req, kSecCSDefaultFlags, &text.aref()));
				string result = cfString(text);
				if (result.empty())		// empty requirement set
					result = "/* no requirements in set */\n";
				else if (result[result.length()-1] != '\n')
					result += '\n';
				printf("%s", result.c_str());
				break;
			}
		case outputBinary:
			{
				CFRef<CFDataRef> data;
				if (CFGetTypeID(req) == SecRequirementGetTypeID())
					MacOSError::check(SecRequirementCopyData(SecRequirementRef(req.get()), kSecCSDefaultFlags, &data.aref()));
				else if (CFGetTypeID(req) == CFDataGetTypeID())
					data = CFDataRef(req.get());
				if (data)
					AutoFileDesc(output, O_WRONLY | O_TRUNC | O_CREAT).writeAll(CFDataGetBytePtr(data), CFDataGetLength(data));
				break;
			}
			break;
		}
		
		exit(exitSuccess);
	} catch (...) {
		diagnose(NULL, exitFailure);
	}
}

static void usage()
{
	fprintf(stderr,
		"Usage: csreq [-v] -r requirement           # check\n"
		"       csreq [-v] -r requirement -t        # text output\n"
		"       csreq [-v] -r requirement -b output # binary output\n"
	);
	exit(exitUsage);
}

static SecCSFlags type(const char *t)
{
	if (!strncmp("requirement", t, strlen(t)))
		return kSecCSParseRequirement;
	else if (!strncmp("internal", t, strlen(t)))
		return kSecCSParseRequirementSet;
	else if (!strncmp("group", t, strlen(t)))
		return kSecCSParseRequirementSet;
	else if (!strncmp("auto", t, strlen(t)))
		return kSecCSDefaultFlags;
	else {
		fprintf(stderr, "%s: invalid type\n", t);
		usage();
	}
}
