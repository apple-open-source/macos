/*
 * Copyright (c) 2009-2011 Apple Computer, Inc. All rights reserved.
 */


#define URL_START "prefs:root=General&path=ManagedConfigurationList/ProfileError&profileID="

#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <CFNetwork/CFSocketStreamPriv.h>


static CFPropertyListRef UnSerialize(const void *data, u_int32_t dataLen);
static void probe_callback(CFReadStreamRef readStream, CFStreamEventType type, void * context);
static void timer_callback( CFRunLoopTimerRef timerref, void *context);
int launch_profile_janitor(int argc, const char * argv[]);
int launch_http_probe(int argc, const char * argv[]);

int main (int argc, const char * argv[]) {
    int result = -1;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	

done:
	return result;
}

