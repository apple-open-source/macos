
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <CoreFoundation/CFBundle.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>
#include "server.h"
#include "controller.h"
#include "mylog.h"

char * eapolclient_path = NULL;

/*
 * configd plugin-specific routines:
 */
void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    Boolean		ok;
    char		path[MAXPATHLEN];
    CFURLRef		url;

    /* get a path to eapolclient */
    url = CFBundleCopyResourceURL(bundle, CFSTR("eapolclient"), NULL, NULL);
    if (url == NULL) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: failed to get URL for eapolclient");
	return;
    }
    ok = CFURLGetFileSystemRepresentation(url, TRUE, path, sizeof(path));
    CFRelease(url);
    if (ok == FALSE) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: failed to get path for eapolclient");
	return;
    }
    eapolclient_path = strdup(path);
#if 0
    S_bundle = (CFBundleRef)CFRetain(bundle);
    S_verbose = bundleVerbose;
#endif 0
    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
}

void
prime()
{
    if (eapolclient_path != NULL) {
	ControllerInitialize();
	
	if (server_active()) {
	    return;
	}
	server_init();
    }
    return;
}
