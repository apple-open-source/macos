/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  AZNTest.cpp
 *  SecurityServer
 *
 *  Created by michael on Fri Oct 20 2000.
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 */

#include <Security/Authorization.h>

#include <Security/AuthorizationEngine.h>

using namespace Authorization;

static const AuthorizationItem gItems[] =
{
	{"login", 0, NULL, NULL},
	{"reboot", 0, NULL, NULL},
	{"shutdown", 0, NULL, NULL},
	{"mount", 0, NULL, NULL},
	{"login.reboot", 0, NULL, NULL},
	{"login.shutdown", 0, NULL, NULL},
	{"unmount", 0, NULL, NULL}
};

static const AuthorizationRights gRights =
{
	7,
	const_cast<AuthorizationItem *>(gItems)
};

void
printRights(const RightSet &rightSet)
{
	for(RightSet::const_iterator it = rightSet.begin(); it != rightSet.end(); ++it)
	{
		printf("right: \"%s\"\n", it->rightName());
	}
}

int
main(int argc, char **argv)
{
	Engine engine("/tmp/config.plist");

	const RightSet inputRights(&gRights);
	MutableRightSet outputRights;
	printf("InputRights:\n");
	printRights(inputRights);
	printf("Authorizing:\n");
	OSStatus result = engine.authorize(inputRights, NULL,
		kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights | kAuthorizationFlagPartialRights,
		NULL, NULL, &outputRights);
	printf("Result: %ld\n", result);
	printf("OutputRights:\n");
	printRights(outputRights);
	return 0;
}
