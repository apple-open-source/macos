#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <SystemUIPlugin/NSMenuExtra.h>

#include "UtilityRoutines.h"

@interface IrDAExtra : NSMenuExtra {
    NSMutableArray 			*mImages;
    unsigned 				mCurrentImage;
	io_connect_t			mConObj;
	io_service_t				mDriverObject;
	UInt8					mIrDAState;
	NSString				*mName;
	BOOL					mSoundState;
	NSBundle				*mBundle;
	CFStringRef				mBundleID;
	IONotificationPortRef	mNotifyPort;
	io_object_t		mNotification;

}
- (NSMenu*) menu;

- (id)initWithBundle:(NSBundle*)bundle;
- (void)updateState:(UInt8)newState;
- (void)menuActionSoundOn:(id)sender;
- (void)menuActionSoundOff:(id)sender;
- (void)menuActionPowerOn:(id)sender;
- (void)menuActionPowerOff:(id)sender;
- (void)menuActionNetworkPrefs:(id)sender;
- (void) stopNotification;
- (void) _testSleep:(NSTimeInterval) sleepTime;
- (void) _testrun:(int)iteration;
- (void) runSelfTest:(unsigned int)inTestToRun duration:(NSTimeInterval)inDuration;

@end
