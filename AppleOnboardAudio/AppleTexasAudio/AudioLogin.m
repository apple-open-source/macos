//
//  AudioLogin.m
//  AudioLogin
//
//  Created by jfu on Mon Nov 12 2001.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <unistd.h>
#import <AppKit/AppKit.h>

#import "AudioLogin.h"
#import "CMDebug.h"

@implementation AudioLogin

-(id)init
{
	if ( [super init] )
	{
		kern_return_t			kr;
		IONotificationPortRef	hotplug_port;

		kr = IOMasterPort( bootstrap_port, &_masterPort );
		CMRequire_int( kr == KERN_SUCCESS, kr, fail );
		
		hotplug_port = IONotificationPortCreate( _masterPort );
		CMRequire_int( kr == KERN_SUCCESS, kr, fail );
		
		//--- Add this notification port to the run source
		CFRunLoopAddSource( CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource( hotplug_port ), kCFRunLoopDefaultMode );

		//--- Check to see if this is a Mac that we need to run on ---
                if ([self shouldLaunch]) {
                    [self launch];
                }

		return self;
	}

fail:
	return nil;
}

-(void)dealloc
{
	[super dealloc];
}

-(void)willLogout
{
	//--- Nothing to do ---
}

-(BOOL)launched
{
	//--- If the file /tmp/.al exists, then the AudioLoginPlugin app has launched ---
	FILE *f = fopen( "/tmp/.al", "r" );
	
	if ( f == NULL )
		return NO;
		
	fclose( f );
	return YES;
}

-(void)launch
{
	FILE 			*f = NULL;
	NSBundle		*bundle = nil;
	NSString		*resourcePath = nil;
	NSString 		*toolPath = nil;
	NSMutableArray		*args = nil;
	NSTask			*task = nil;
	
	//--- If already launched, do nothing ---
	if ( [self launched] )
		return;

	//--- Create the /tmp/.al to mark the AudioLoginPlugin as launched ---
	f = fopen( "/tmp/.al", "w" );
	fprintf( f, "al\n" );
	fclose( f );
	
	//--- Now launch the tool ---
	bundle 		= [NSBundle bundleWithIdentifier:@"com.apple.AudioLogin"];
	resourcePath 	= [bundle resourcePath];
	toolPath 	= [resourcePath stringByAppendingPathComponent:@"AudioLoginTool"];
	
	CMRequire( bundle != nil, exit );
	CMRequire( [[NSFileManager defaultManager] fileExistsAtPath:toolPath], exit );

	//--- Launch ---
	args = [NSMutableArray array];
	task = [NSTask launchedTaskWithLaunchPath:toolPath arguments:args];
	CMRequire( task != nil, exit );

exit:
	return;
}

#define kNumRootCandidates	3
-(BOOL)shouldLaunch
{
    kern_return_t		kr		= 0;
    mach_port_t			masterPort	= 0;
    io_iterator_t		iterator	= 0;
    io_object_t			rootObject	= 0;
    io_name_t			rootName;
    BOOL			found		= FALSE;
    char *			rootCandidates[kNumRootCandidates]	= {"PowerMac4,2", "PowerBook2,2", "PowerBook2,1"};
    long			i;

    kr = IOMasterPort (bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS)
        goto Exit;

    kr = IORegistryCreateIterator (masterPort, kIOServicePlane, kIORegistryIterateRecursively, &iterator);
    if (kr != kIOReturnSuccess || NULL == iterator)
        goto Exit;

    rootObject = IOIteratorNext (iterator);
    if (NULL == rootObject)
        goto Exit;

    kr = IORegistryEntryGetName (rootObject, rootName);
    if (kr != KERN_SUCCESS)
        goto Exit;

    IOObjectRelease (rootObject);

    i = 0;
    while (FALSE == found && i < kNumRootCandidates) {
        if (0 == strcmp (rootName, rootCandidates[i++])) {
            found = TRUE;
        }
    }

Exit:
    return found;
}

@end