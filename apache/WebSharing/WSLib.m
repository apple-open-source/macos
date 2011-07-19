//
//  WSLib.m
//  Web Sharing Utility Library
//
//  Created by Al Begley on 4/4/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#import "WSLib.h"
#include <launch.h>
#include <unistd.h>

#define APACHE_PLIST_PATH @"/System/Library/LaunchDaemons/org.apache.httpd.plist"
#define APACHE_SERVICE_LABEL @"org.apache.httpd"
#define WEBSHARING_DEFINE_STRING @"-D WEBSHARING_ON"
#define MACOSXSERVER_DEFINE_STRING @"-D MACOSXSERVER"
#define WEBSHARING_DEFINE_ARG @"WEBSHARING_ON"
#define HTTPD_PIDFILE @"/var/run/httpd.pid"

static int pid_from_file(NSString *file) {
	int pid = 0;
	NSScanner *scanner = nil;
	NSString *contents = nil;
	NSData *data = [NSData dataWithContentsOfFile:file];
	
	if (data != nil) {
		contents = [[[NSString allocWithZone:NULL] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
	}
	if (contents == nil) {
		return -1;
	}
	scanner = [NSScanner scannerWithString:contents];
	[scanner setCharactersToBeSkipped:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	[scanner scanInt:&pid];
	return pid;
}

static int wait_for_pid_from_file(NSString* pidFile) {
	int i, returnValue;
	for (i = 0; i < 6 && (returnValue = pid_from_file(pidFile)) < 0; i++) {
		usleep(200000);	// .2 sec. * 6 iterations = give up after 3 sec.
	}
	return returnValue;
}

static BOOL	WSGetLaunchDaemonServiceState(NSString *inLabel)
{
	launch_data_t msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	launch_data_t jobLabel = launch_data_new_string([inLabel UTF8String]);
	launch_data_dict_insert(msg, jobLabel, LAUNCH_KEY_GETJOB);
	launch_data_t response = launch_msg(msg);
	launch_data_free(msg);
	if (launch_data_get_type(response) == LAUNCH_DATA_DICTIONARY) {
		launch_data_free(response);
		return YES;
	}
	else
		launch_data_free(response);
	return NO;
}	

static BOOL isApacheRunning()
{
	// Must operate without root privileges, so just check httpd.pid contains pid
	pid_t pid = pid_from_file(HTTPD_PIDFILE);
	return (pid > 0);
}
static BOOL WSSetLaunchDaemonServiceState(NSString* serviceFilePath, BOOL state)
{	
	NSTask* task = [[NSTask alloc] init];
	[task setLaunchPath:@"/bin/launchctl"];
	[task setArguments:[NSArray arrayWithObjects:(state ? @"load" : @"unload"), @"-w", serviceFilePath, nil]];
	[task launch];
	[task waitUntilExit];
	return [task terminationStatus] == 0;
}

static NSArray* WSGetLaunchDaemonServiceConfigArgs(NSString *serviceFilePath)
{
	return [[NSDictionary dictionaryWithContentsOfFile: serviceFilePath] objectForKey:@LAUNCH_JOBKEY_PROGRAMARGUMENTS];
}

static BOOL SetLaunchDaemonServiceConfigArgs(NSString *serviceFilePath, NSArray *args)
{
	NSMutableDictionary* serviceDictionary = [NSDictionary dictionaryWithContentsOfFile: serviceFilePath];
	if (!serviceDictionary || !args || !serviceFilePath)
		return NO;
	[serviceDictionary setObject:args forKey:@LAUNCH_JOBKEY_PROGRAMARGUMENTS];
	return [serviceDictionary writeToFile: serviceFilePath atomically:YES];;
}

int WSGetWebSharingState() {
	// Must run without root privieges
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	BOOL apacheIsRunning = isApacheRunning();
	if (!apacheIsRunning) {
		[pool release];
		return WS_STATE_STOPPED;
	}
	NSArray* args = WSGetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH);
	NSString* argString = [args componentsJoinedByString:@" "];
	NSRange range = [argString rangeOfString:WEBSHARING_DEFINE_STRING];
	BOOL webSharingIsDefined = !(range.location == NSNotFound);

	[pool release];
	if (webSharingIsDefined)
		return WS_STATE_RUNNING;
	else
		return WS_STATE_STOPPED;
}
int WSSetWebSharingState(int newState) {
	int returnValue = newState;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	BOOL jobIsLoaded = WSGetLaunchDaemonServiceState(APACHE_SERVICE_LABEL);
	NSMutableArray* args = [[WSGetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH) mutableCopy] autorelease];
	NSString* argString = [args componentsJoinedByString:@" "];
	NSRange range = [argString rangeOfString:WEBSHARING_DEFINE_STRING];
	BOOL webSharingIsDefined = !(range.location == NSNotFound);
	range = [argString rangeOfString:MACOSXSERVER_DEFINE_STRING];
	BOOL macOSXSereverIsDefined = !(range.location == NSNotFound);

	if (newState == WS_STATE_RUNNING) {
		if (jobIsLoaded) {
			if (!webSharingIsDefined) {
				WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, NO);
				[args addObject:@"-D"];
				[args addObject:WEBSHARING_DEFINE_ARG];
				if (!SetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH, args))
					returnValue = WS_STATE_ERROR;
				else {
					WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, YES);
					if (wait_for_pid_from_file(HTTPD_PIDFILE) <= 0)
						returnValue = WS_STATE_ERROR;
				}
			}
		}
		else {
			if (webSharingIsDefined) {
				WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, YES);
				if (wait_for_pid_from_file(HTTPD_PIDFILE) <= 0)
					returnValue = WS_STATE_ERROR;
			}
			else {
				[args addObject:@"-D"];
				[args addObject:WEBSHARING_DEFINE_ARG];
				if (!SetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH, args))
					returnValue = WS_STATE_ERROR;
				else {
					WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, YES);
					if (wait_for_pid_from_file(HTTPD_PIDFILE) <= 0)
						returnValue = WS_STATE_ERROR;
				}
			}
		}
	}
	if (newState == WS_STATE_STOPPED) {
		if (jobIsLoaded) {
			if (webSharingIsDefined) {
				WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, NO);
				NSUInteger defineIndex = [args indexOfObject:WEBSHARING_DEFINE_ARG];
				[args removeObjectAtIndex:defineIndex];
				[args removeObjectAtIndex:defineIndex - 1];
				if (!SetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH, args))
					returnValue = WS_STATE_ERROR;
				else if (macOSXSereverIsDefined)
					WSSetLaunchDaemonServiceState(APACHE_PLIST_PATH, YES);
			}
		}
		else {
			if (webSharingIsDefined) {
				NSUInteger defineIndex = [args indexOfObject:WEBSHARING_DEFINE_ARG];
				[args removeObjectAtIndex:defineIndex];
				[args removeObjectAtIndex:defineIndex - 1];
				if (!SetLaunchDaemonServiceConfigArgs(APACHE_PLIST_PATH, args))
					returnValue = WS_STATE_ERROR;
			}
		}
	}
	return returnValue;
	[pool release];
}
