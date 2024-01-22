
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTDeviceInformationNameUpdateListener
- (void)deviceNameUpdated;
@end

@protocol OTDeviceInformationAdapter

- (void)setOverriddenMachineID:(NSString*)machineID;
- (NSString* _Nullable)getOverriddenMachineID;
- (BOOL)isMachineIDOverridden;
- (void)clearOverride;

/* Returns a string like "iPhone3,5" */
- (NSString*)modelID;

/* Returns the user-entered name for this device */
- (NSString* _Nullable)deviceName;

/* Returns a string describing the current os version */
- (NSString*)osVersion;

/* Returns the serial number as a string. */
- (NSString* _Nullable)serialNumber;

/* register for deviceName updates */
- (void)registerForDeviceNameUpdates:(id<OTDeviceInformationNameUpdateListener>)listener;

/* Returns whether the current device is a homepod */
- (BOOL)isHomePod;

/* Returns whether the current device is a watch */
- (BOOL)isWatch;

/* Returns whether the current device is an AppleTV */
- (BOOL)isAppleTV;

@end

@interface OTDeviceInformationActualAdapter : NSObject <OTDeviceInformationAdapter>

@end

NS_ASSUME_NONNULL_END
