
#import "IrDAExtra.h"
#import <sys/stat.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <CoreFoundation/CFLogUtilities.h> // CFLogTest

//#include <HIServices/CoreDockDocklingServer.h>
#include <ApplicationServices/ApplicationServicesPriv.h>
#include <IOKit/IOMessage.h>

enum{
	kIrDA1_Idle = 0,
	kIrDA2_Discovering,
	kIrDA3_Connected,
	kIrDA4_BrokenBeam,
	kIrDA5_Invalid,
	kIrDA6_Off,
	kNumPictures
};

@implementation IrDAExtra
// driverCallback
static void driverCallback(void *refcon, io_service_t service, uint32_t messageType, void* messageArgument)
{
	switch (messageType){
		case kIOMessageServiceIsTerminated:
			NSLog(@"driverCallback: messageType = kIOMessageServiceIsTerminated");
			{
				IrDAExtra	*temp = refcon;
				
				[temp stopNotification];
				CoreMenuExtraRemoveMenuExtra (0, temp->mBundleID);
			}
			break;
		case kIrDACallBack_Status:
			{
			    // irda state is passed back to us in high-byte of messageArgument (ppc), or low byte if intel
			    // http://lists.apple.com/archives/darwin-development/2003/Oct/msg00063.html
			    // could probably clean up below ..
#if defined(__ppc__)
			    UInt8 status = ((uintptr_t)messageArgument) >> 24;
#endif
#if (defined(__i386__) || defined(__x86_64__))
			    UInt8 status = ((uintptr_t)messageArgument) & 0xff;
#endif    
			    IrDAExtra	*temp = refcon;
	
			    [temp updateState:status];	
			}
			break;
		case kIrDACallBack_Unplug:
			NSLog(@"driverCallback: messageType = kIrDACallBack_Unplug");
			{
				IrDAExtra	*temp = refcon;
				
				[temp stopNotification];
				CoreMenuExtraRemoveMenuExtra (0, temp->mBundleID);
			}
			break;
		default:
			NSLog(@"driverCallback: messageType = %d", messageType);
			break;
	};
	
}

- (void) setIrDAImage
{
	[self setImage:[mImages objectAtIndex:mCurrentImage]];
}

- (void)updateState:(UInt8)newState
{
	BOOL			makeSound	= NO;
	
	mIrDAState = newState;
	switch (mIrDAState){
		case kIrDAStatusIdle:				mCurrentImage = kIrDA1_Idle;												break;
		case kIrDAStatusDiscoverActive:		mCurrentImage = kIrDA2_Discovering;											break;
		case kIrDAStatusConnected:			mCurrentImage = kIrDA3_Connected;		makeSound = YES;					break;
		case kIrDAStatusBrokenConnection:	mCurrentImage = kIrDA4_BrokenBeam;		makeSound = YES;					break;
		case kIrDAStatusOff:				mCurrentImage = kIrDA6_Off;													break;
		case kIrDAStatusInvalid:			mCurrentImage = kIrDA5_Invalid;												break;
		default:							mCurrentImage = kIrDA5_Invalid;			mIrDAState = kIrDAStatusInvalid;	break;
	};
	[self setIrDAImage];
	if (makeSound && mSoundState){
		NSBeep();
	}
}

- (void) setmName:(UInt8 *)name{
	[mName release];
	mName = [NSString stringWithUTF8String:(const char *)name];
	[mName retain];
}

/* close  the driver */
- (void) closeIrDA
{
	IOServiceClose(mConObj);
}

/* open up the driver and leave it open so we can talk to it */
- (void) openIrDA
{
	IOServiceOpen(mDriverObject, mach_task_self(), 123, &mConObj);
}

- (void) PollState
{
    kern_return_t			kr;
    IrDAStatus				stats;
    size_t	outputsize = sizeof(stats);

	[self openIrDA];
	kr = doCommand(mConObj, kIrDAUserCmd_GetStatus, nil, 0, &stats, &outputsize);
	if (kr == kIOReturnSuccess) {
		mIrDAState = stats.connectionState;
	}
	switch (mIrDAState){
		case kIrDAStatusIdle:				mCurrentImage = kIrDA1_Idle;												break;
		case kIrDAStatusDiscoverActive:		mCurrentImage = kIrDA2_Discovering;											break;
		case kIrDAStatusConnected:			mCurrentImage = kIrDA3_Connected;		[self setmName:stats.nickName];		break;
		case kIrDAStatusBrokenConnection:	mCurrentImage = kIrDA4_BrokenBeam;											break;
		case kIrDAStatusOff:				mCurrentImage = kIrDA6_Off;													break;
		case kIrDAStatusInvalid:			mCurrentImage = kIrDA5_Invalid;												break;
		default:							mCurrentImage = kIrDA5_Invalid;			mIrDAState = kIrDAStatusInvalid;	break;
	};
	[self closeIrDA];
	[self setIrDAImage];
}

- (void)menuActionSoundOn:(id)sender
{
	CFPreferencesSetAppValue(CFSTR("UseSoundForIrDA"), CFSTR("YES"), mBundleID);
	CFPreferencesAppSynchronize(mBundleID);
	mSoundState = YES;
}
- (void)menuActionSoundOff:(id)sender
{
	CFPreferencesSetAppValue(CFSTR("UseSoundForIrDA"), CFSTR("NO"), mBundleID);
	CFPreferencesAppSynchronize(mBundleID);
	mSoundState = NO;
}
- (void)menuActionNetworkPrefs:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:@"/System/Library/PreferencePanes/Network.prefPane"];
}
- (void)menuActionPowerOn:(id)sender
{
    kern_return_t	kr;
    size_t outputsize = 0;

	[self openIrDA];
	kr = doCommand(mConObj, kIrDAUserCmd_Enable, nil, 0, nil, &outputsize);
	if (kr == kIOReturnSuccess) {
		CFPreferencesSetAppValue(CFSTR("UseIrDAHardware"), CFSTR("YES"), mBundleID);
		CFPreferencesAppSynchronize(mBundleID);
	}
	[self closeIrDA];
}
- (void)menuActionPowerOff:(id)sender
{
    kern_return_t			kr;
    size_t outputsize = 0;

	[self openIrDA];
	kr = doCommand(mConObj, kIrDAUserCmd_Disable, nil, 0, nil, &outputsize);
	if (kr == kIOReturnSuccess) {
		CFPreferencesSetAppValue(CFSTR("UseIrDAHardware"), CFSTR("NO"), mBundleID);
		CFPreferencesAppSynchronize(mBundleID);
	}
	[self closeIrDA];
}
- (void)CheckPrefs
{
	Boolean		temp;
	Boolean		valid;
	/* Check Sound state */
	mSoundState = CFPreferencesGetAppBooleanValue(CFSTR("UseSoundForIrDA"), mBundleID, &valid);
	/* Check Power state */
	temp = CFPreferencesGetAppBooleanValue(CFSTR("UseIrDAHardware"), mBundleID, &valid);
	if (temp){
		[self menuActionPowerOn:self];
	}
}

/* This routine will need to generate the menu based on the current state of the system. */
- (NSMenu*) menu
{
    NSMenu		*menu;
    NSMenuItem 	*item;
    
	[self PollState];					// Update the state just in case
	
	menu = [[[NSMenu alloc] initWithTitle:@"MenuTitle"] autorelease];
	
	/* Only one Menu Item if we are invalid */
	if (mIrDAState == kIrDAStatusInvalid){				
		item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"IrDA: Invalid" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
		[item setTarget:self];
		[menu addItem: item];
		return menu;
	};

	/* Only one Menu Item if we are turned off */
	if (mIrDAState == kIrDAStatusOff){				
		item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"Turn IrDA On" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
		[item setAction:@selector(menuActionPowerOn:)];
		[item setTarget:self];
		[menu addItem: item];
		mIrDAState = kIrDAStatusInvalid;
		return menu;
	};

	/* First Menu Item, "Status" Should be disabled*/
	switch (mIrDAState){
		case kIrDAStatusIdle:
			item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"IrDA: Idle" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
			break;
		case kIrDAStatusDiscoverActive:
			item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"IrDA: Discovering" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
			break;
		case kIrDAStatusConnected:
			{
				NSString *part1 = [mBundle localizedStringForKey: @"IrDA: Connected (" value: @"" table: @"menu"];
				NSString *part2 = [part1 stringByAppendingString:mName];
				NSString *part3 = [part2 stringByAppendingString:[mBundle localizedStringForKey: @")" value: @"" table: @"menu"]];

				item = [[[NSMenuItem alloc] initWithTitle:part3 action: NULL keyEquivalent:@""] autorelease];
			}
			break;
		case kIrDAStatusBrokenConnection:
			item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"IrDA: Broken Beam" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
			break;
		default:
			item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"IrDA: Invalid" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
			break;
	};
	[menu addItem: item];

	/* Second Menu Item, Power On/Off */
	item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"Turn IrDA Off" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
	[item setAction:@selector(menuActionPowerOff:)];
	[item setTarget:self];
	[menu addItem: item];

	/* Fifth Menu Item, Seperator */
	[menu addItem:[NSMenuItem separatorItem]];

	/* Sixth Menu Item, Sound On/Off */
	item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"Use Sound Effects" value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
	if (mSoundState){
		[item setState:NSOnState];
		[item setAction:@selector(menuActionSoundOff:)];
	}
	else{
		[item setState:NSOffState];
		[item setAction:@selector(menuActionSoundOn:)];
	}
	[item setTarget:self];
	[menu addItem: item];

	/* Seventh Menu Item, Seperator */
	[menu addItem:[NSMenuItem separatorItem]];

	/* Eigth Menu Item, Open Internet Connect / Network Prefs*/
	item = [[[NSMenuItem alloc] initWithTitle:[mBundle localizedStringForKey: @"Open Network Preferences..." value: @"" table: @"menu"] action: NULL keyEquivalent:@""] autorelease];
	[item setAction:@selector(menuActionNetworkPrefs:)];
	[item setTarget:self];
	[menu addItem: item];
	
	mIrDAState = kIrDAStatusInvalid;
    return menu;
}
- (void)InitImages
{
	NSImage *image;
	
	mImages = [[NSMutableArray alloc] initWithCapacity: kNumPictures];
	
	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA1IdleCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];
		
	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA2DiscoveringCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];

	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA3ConnectedCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];
		
	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA4BrokenBeamCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];
		
	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA5InvalidCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];
		
	image = [[[NSImage alloc] initWithContentsOfFile:[mBundle pathForResource:@"IRDA6OffCropped.pdf" ofType:nil]] autorelease];
	[image setTemplate:YES];
	[mImages addObject:image];
		
	mCurrentImage = kIrDA5_Invalid;
	[self setIrDAImage];
}

- (BOOL)convertedForNewUI 
{ 
	return YES; 
}

- (BOOL) searchForDriver
{
    mach_port_t		masterPort;
    kern_return_t	kr;
	
    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr == KERN_SUCCESS) {
		mDriverObject = getInterfaceWithName(masterPort, "AppleIrDA");
		if (mDriverObject) {
			return YES;
		}
    }
	return NO;
}

/* This has been migrated from NSPrefs to CFPreferences */
- (void) DefaultPrefs{
    CFTypeRef	ref;
    
    mBundleID = (CFStringRef)[mBundle bundleIdentifier];
    CFRetain(mBundleID);

    /* UseIrDAHardware */
    ref = CFPreferencesCopyAppValue(CFSTR("UseIrDAHardware"), mBundleID);
    if (ref == nil){
	CFPreferencesSetAppValue(CFSTR("UseIrDAHardware"), CFSTR("NO"), mBundleID);
	CFPreferencesAppSynchronize(mBundleID);
    }
    else{
	CFRelease(ref);
	ref = nil;
    }
    /* UseSoundForIrDA */
    ref = CFPreferencesCopyAppValue(CFSTR("UseSoundForIrDA"), mBundleID);
    if (ref == nil){
	CFPreferencesSetAppValue(CFSTR("UseSoundForIrDA"), CFSTR("YES"), mBundleID);
	CFPreferencesAppSynchronize(mBundleID);
    }
    else{
	CFRelease(ref);
	ref = nil;
    }
}

- (void) startNotification
{
    kern_return_t	kr;
    
    mNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    
    if (mNotifyPort) {
	mNotification = IO_OBJECT_NULL;
	
	kr = IOServiceAddInterestNotification(mNotifyPort, mDriverObject, kIOGeneralInterest, &driverCallback, (void *) self, &mNotification);
	if (kr== KERN_SUCCESS){
	    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(mNotifyPort), kCFRunLoopDefaultMode);
	}
    }
}

- (void) stopNotification
{
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(mNotifyPort), kCFRunLoopDefaultMode);
    if (mNotification) {
	IOObjectRelease(mNotification);
	mNotification = IO_OBJECT_NULL;
    }
    if (mNotifyPort) {
	IONotificationPortDestroy(mNotifyPort);
	mNotifyPort = NULL;
    }
}

/* Get everything ready to go */
- (id)initWithBundle:(NSBundle*)bundle
{
    self = [super initWithBundle:bundle];
    
    //NSLog(@"initWithBundle:");
    if (self != nil){
		if ([self searchForDriver]){
			mBundle = bundle;
			[mBundle retain];																		// Don't throw the bundle away
			mName = @"";
			[mName retain];																			// Keep me 
			[self DefaultPrefs];																	// set default Prefs
			[self InitImages];																		// Load all of the images
			[self CheckPrefs];																		// See what current prefs are
			[self startNotification];																// Let the driver know we care
			[self PollState];
		}
		else{
			[super dealloc];	// I am not sure I need this
			return (nil);
		}
    }
    return self;
}

/* Get rid of everything allocated in init */
- (void) dealloc
{
	[self stopNotification];
	[mBundle release];
	[mImages release];
	[mName release]; 
	CFRelease(mBundleID);
	IOObjectRelease(mDriverObject);
	[super dealloc];
}

- (void) _testSleep:(NSTimeInterval) sleepTime
{
    if (sleepTime>0)
	[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:sleepTime]];
}

#define MaxWaitTime  15	// max time in seconds for a connection, 3 is typical.  Off takes about 1.

- (void) _testrun:(int)iteration
{
    int i;
    static int worked, failed;
    CFLogTest(0, CFSTR( "Iteration:%d %@ running IrDA on/off self test" ), iteration, [self className]);
    [self menuActionPowerOn:self];
    for (i = 0 ; i < MaxWaitTime; i++) {
	if (mIrDAState == kIrDAStatusConnected) {
	    worked++;
	    [self PollState];	    // to get nicname of peer
	    CFLogTest(0, CFSTR( "Iteration:%d %@ connected after %d seconds with '%@'.  Worked %d, failed %d, %4.1f%%." ),
		      iteration, [self className], i, mName, worked, failed, 100.*worked / (double)(worked + failed));
	    break;
	}
	[self _testSleep:1];
    }
    if (mIrDAState != kIrDAStatusConnected) {
	failed++;
	CFLogTest(0, CFSTR( "Iteration:%d %@ did not connect.  Worked %d, failed %d, %4.1f%%." ),
		  iteration, [self className], worked, failed, 100.*worked / (double)(worked + failed));
    }

    [self menuActionPowerOff:self];
    for (i = 0 ; i < MaxWaitTime ; i++) {
	if (mIrDAState == kIrDAStatusOff) {
	    break;
	}
	[self _testSleep:1];
    }
    [self _testSleep:2];	// some settle time before reconnect
}

// Only one test implemented, so inTestToRun is ignored.  And inDuration is also ignored, as the SystemUIServer
// will loop calling all of it's menu extras until the total duration time has elapsed.  We need to return after
// a single test to give equal time to the other menu extras.

- (void) runSelfTest:(unsigned int)inTestToRun duration:(NSTimeInterval)inDuration
{
    static int iteration = 0;
    iteration++;
    
    [self _testrun:iteration];	    // run an on/off sequence
    
    // given we can't tell which is the last iteration, we're not
    // bothering to restore state after the last test run.
}



@end
