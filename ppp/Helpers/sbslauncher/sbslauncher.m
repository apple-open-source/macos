/*
 * Copyright (c) 2009-2011 Apple Computer, Inc. All rights reserved.
 */


#define URL_START "prefs:root=General&path=ManagedConfigurationList/ProfileError&profileID="

#include "sbslauncher.h"
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>


static CFPropertyListRef UnSerialize(const void *data, u_int32_t dataLen);
int launch_profile_janitor(int argc, const char * argv[]);

int main (int argc, const char * argv[]) {
	
	int result = -1;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	if (strcmp(argv[1], SBSLAUNCHER_TYPE_PROFILE_JANITOR) == 0)
		result = launch_profile_janitor(argc, argv);
	return result;
}


int launch_profile_janitor(int argc, const char * argv[]) {
	
	CFURLRef url = NULL;
	Boolean success = FALSE;
	char *buf = NULL;
	int bufsize	= 0;
	
	if (argc <= 2)
		goto done;
	
	bufsize = strlen(URL_START) + strlen(argv[2]) + 1; // includes NULL termination
	
	buf = malloc(bufsize);
	if (buf == NULL)
		goto done;
		
	strlcpy(buf, URL_START, bufsize);
	strlcat(buf, argv[2], bufsize);
	
	url = CFURLCreateWithBytes(NULL, (UInt8 *)buf, bufsize - 1, kCFStringEncodingUTF8, NULL);
	if (url == NULL)
		goto done;

	success = SBSOpenSensitiveURL(url); 
	if (!success)
		goto done;

done:

	if (success) {
		syslog(LOG_NOTICE, "Opened URL'%s'", buf);
	}
	else {
		if (url) 
			syslog(LOG_NOTICE, "Failed to open URL'%s'", buf);
		else
			syslog(LOG_NOTICE, "Failed to create URL for '%s'", (argc > 2) ? argv[2] : "no profile identifier");
	}
		
	if (url)
		CFRelease(url);
	if (buf)
		free(buf);
	return (success ? 0 : -1);
}

