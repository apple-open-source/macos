/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// testacls - ACL-related test cases.
// 
#include "testclient.h"
#include "testutils.h"
#include <Security/osxsigner.h>

using namespace CodeSigning;


//
// Authorization test.
// This tests the authorization API support.
// @@@ Incomplete and not satisfactory.
//
void authorizations()
{
	printf("* authorization test\n");
	ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	
	// make a simple authorization query
	AuthorizationBlob auth;
	AuthorizationItem testingItem = { "debug.testing", 0, NULL, NULL };
	AuthorizationItem testingMoreItem = { "debug.testing.more", 0, NULL, NULL };
	AuthorizationItem denyItem = { "debug.deny", 0, NULL, NULL };
	AuthorizationItemSet request = { 1, &testingItem };
	ss.authCreate(&request, NULL/*environment*/,
		kAuthorizationFlagInteractionAllowed |
		kAuthorizationFlagExtendRights |
		kAuthorizationFlagPartialRights,
		auth);
	detail("Initial authorization obtained");
	
	// ask for rights from this authorization
	{
		AuthorizationItem moreItems[3] = { testingItem, denyItem, testingMoreItem };
		AuthorizationItemSet moreRequests = { 3, moreItems };
		AuthorizationItemSet *rightsVector;
		ss.authCopyRights(auth, &moreRequests, NULL/*environment*/,
			kAuthorizationFlagInteractionAllowed |
			kAuthorizationFlagExtendRights |
			kAuthorizationFlagPartialRights,
			&rightsVector);
		if (rightsVector->count != 2)
			error("COPYRIGHTS RETURNED %d RIGHTS (EXPECTED 2)", int(rightsVector->count));
		// the output rights could be in either order -- be flexible
		set<string> rights;
		rights.insert(rightsVector->items[0].name);
		rights.insert(rightsVector->items[1].name);
		assert(rights.find("debug.testing") != rights.end() &&
			rights.find("debug.testing.more") != rights.end());
		free(rightsVector);
		detail("CopyRights okay");
	}
		
	// ask for the impossible
	try {
		AuthorizationBlob badAuth;
		AuthorizationItem badItem = { "debug.deny", 0, NULL, NULL };
		AuthorizationItemSet badRequest = { 1, &badItem };
		ss.authCreate(&badRequest, NULL/*environment*/,
			kAuthorizationFlagInteractionAllowed |
			kAuthorizationFlagExtendRights,
			auth);
		error("AUTHORIZED debug.deny OPERATION");
	} catch (CssmCommonError &err) {
		detail(err, "debug.deny authorization denied properly");
	}
	
	// externalize
	AuthorizationExternalForm extForm;
	ss.authExternalize(auth, extForm);
	
	// re-internalize
	AuthorizationBlob auth2;
	ss.authInternalize(extForm, auth2);
	
	// make sure it still works
	{
		AuthorizationItem moreItems[2] = { testingItem, denyItem };
		AuthorizationItemSet moreRequests = { 2, moreItems };
		AuthorizationItemSet *rightsVector;
		ss.authCopyRights(auth2, &moreRequests, NULL/*environment*/,
			kAuthorizationFlagInteractionAllowed |
			kAuthorizationFlagExtendRights |
			kAuthorizationFlagPartialRights,
			&rightsVector);
		if (rightsVector->count != 1)
			error("COPYRIGHTS RETURNED %d RIGHTS (EXPECTED 1)", int(rightsVector->count));
		assert(!strcmp(rightsVector->items[0].name, "debug.testing"));
		free(rightsVector);
		detail("Re-internalized authorization checks out okay");

		// try it with no rights output (it's optional)
		ss.authCopyRights(auth2, &moreRequests, NULL/*environment*/,
			kAuthorizationFlagPartialRights, NULL);
		detail("authCopyRights partial success OK (with no output)");
		
		// but this will fail if we want ALL rights...
		try {
			ss.authCopyRights(auth2, &moreRequests, NULL/*environment*/,
			kAuthorizationFlagDefaults, NULL);
			error("authCopyRights succeeded with (only) partial success");
		} catch (CssmError &err) {
			detail("authCopyRight failed for (only) partial success");
		}
	}
}
