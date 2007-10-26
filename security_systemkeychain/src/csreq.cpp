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
#include <security_codesigning/reqdumper.h>

using namespace CodeSigning;
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

static enum RequirementType {
	typeSingle,						// single Requirement
	typeInternal,					// (internal) Requirements set
	typeAuto						// auto-sense either
} reqType = typeAuto;


//
// Local functions
//
static void usage();
static RequirementType type(const char *t);


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
		
		const BlobCore *req;
		switch (reqType) {
		case typeSingle:
			req = readRequirement<Requirement>(requirement);
			break;
		case typeInternal:
			req = readRequirement<Requirements>(requirement);
			break;
		case typeAuto:
			req = readRequirement<BlobCore>(requirement);
			break;
		}
		assert(req);
		
		switch (outputType) {
		case outputCheck:
			note(1, "valid");
			break;
		case outputText:
			puts(CodeSigning::Dumper::dump(req, verbose > 1).c_str());
			break;
		case outputBinary:
			AutoFileDesc(output, O_WRONLY | O_TRUNC | O_CREAT).writeAll(*req);
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

static RequirementType type(const char *t)
{
	if (!strncmp("requirement", t, strlen(t)))
		return typeSingle;
	else if (!strncmp("internal", t, strlen(t)))
		return typeInternal;
	else if (!strncmp("group", t, strlen(t)))
		return typeInternal;
	else if (!strncmp("auto", t, strlen(t)))
		return typeAuto;
	else {
		fprintf(stderr, "%s: invalid type\n", t);
		usage();
	}
}
