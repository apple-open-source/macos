/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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
// trampolineServer.cpp - tool-side trampoline support functions
//
#include <cstdlib>
#include <unistd.h>
#include <Security/Authorization.h>
#include <Security/SecBase.h>

//
// In a tool launched via AuthorizationCopyPrivilegedReference, retrieve a copy
// of the AuthorizationRef that started it all.
//
OSStatus AuthorizationCopyPrivilegedReference(AuthorizationRef *authorization,
	AuthorizationFlags flags)
{
	// flags are currently reserved
	if (flags != 0)
		return errAuthorizationInvalidFlags;

	// retrieve hex form of external form from environment
	const char *mboxFdText = getenv("__AUTHORIZATION");
	if (!mboxFdText)
		return errAuthorizationInvalidRef;

    // retrieve mailbox file and read external form
    AuthorizationExternalForm extForm;
    int fd;
    if (sscanf(mboxFdText, "auth %d", &fd) != 1)
        return errAuthorizationInvalidRef;
    if (lseek(fd, 0, SEEK_SET) ||
            read(fd, &extForm, sizeof(extForm)) != sizeof(extForm)) {
        close(fd);
        return errAuthorizationInvalidRef;
    }

	// internalize the authorization
	AuthorizationRef auth;
	if (OSStatus error = AuthorizationCreateFromExternalForm(&extForm, &auth))
		return error;

	// well, here you go
	*authorization = auth;
	return errSecSuccess;
}
